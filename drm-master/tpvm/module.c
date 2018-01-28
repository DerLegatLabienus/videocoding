#include "module.h"
#include <asm/uaccess.h>    /* for put_user */
#include <asm/page.h>

#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>     /* kmalloc() */
#include <linux/errno.h>    /* error codes */
#include <linux/types.h>    /* size_t */
#include <linux/mm.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/rcupdate.h>
#include <linux/device.h>
#include <linux/kthread.h>

#include "rc4.h"
#include "../rsa/rsa_common.h"
#include "../rsa/rsa.h"
#include "main.h"
#include "protocol.h"
#include "queue.h"

// constants
static const int VERBOSE = 0;
static const int BLK_SIZE = 4096;
static const int BLKS = 2;

// general module variables
static int Major;        // Major number assigned to our device driver
static int dev_open = 0; // Is device open?  Used to prevent multiple access to device

struct task_struct* decoder_thread  = 0;
struct task_struct* listener_thread = 0;

extern struct queue_root* root;

// video metadata (height, width, frames). will be read from the server
Metadata metadata;

// tpvm_key will hold the video key. the buffer will be filled in rsa.c
unsigned char tpvm_key[KEY_LEN]; // KEY_LEN from rsa_common.h

//will be read from userspace. Process ID and eventfd's File descriptor are enough to uniquely identify an eventfd object.

//Resolved references. these will be set in module_efd.c
struct eventfd_ctx * ctx_k2v = NULL, // kernel signals viewer
                   * ctx_v2k = NULL, // viewer signals kernel
                   * ctx_k2l = NULL, // kernel signals listener
                   * ctx_l2k = NULL; // listener signals kernel

static int frame_size = 0;

// mem_buffer is used both for the frames buffer and to the blocks buffer
void* mem_buffer = 0;
frame* frames[2];
unsigned char* blks[2];

rc4_state my_rc4_state;

static int keyj_dev_open(struct inode *inode, struct file *file);
static int keyj_dev_release(struct inode *inode, struct file *file);
static int keyj_dev_mmap(struct file *filp, struct vm_area_struct *vma);
long keyj_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

static struct file_operations fops = {
    .owner          = THIS_MODULE,
    .open           = keyj_dev_open,
    .release        = keyj_dev_release,
	.unlocked_ioctl = keyj_ioctl,
    .mmap           = keyj_dev_mmap,
};

int init_module(void)
{
    int res = rsaLoad();
    if (res < 0) {
        printk(KERN_ALERT "RSA loading failed with %d\n", res);
        return res;
    }
    res = register_chrdev(0, DEVICE_NAME, &fops);
    if (res < 0) {
        printk(KERN_ALERT "Registering char device failed with %d\n", res);
        return res;
    }
    Major = res;

    res = setup_proc_fs();
    if (res < 0) {
        printk(KERN_ERR "setup procfs failed with %d\n", res);
        return res;
    }

    printk(KERN_INFO "I was assigned major number %d. To talk to\n", Major);
    printk(KERN_INFO "the driver, create a dev file with\n");
    printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);

    return SUCCESS;
}

void cleanup_module(void)
{
    rsaUnload();
    remove_proc_entries();
    unregister_chrdev(Major, DEVICE_NAME);
}

int decoder_thread_fn(void* arg)
{
    frame* temp_frame;
    int frm_buf_id = 0;
    int frm = 0;
    uint64_t is_viewer_ready = NA;

    // verify ctxs are valid
    if (!ctx_k2v) {
        printk(KERN_ERR "decoder_thd: ctx_k2v is NULL, aborting");
        return FAILURE;
    }
    if (!ctx_v2k) {
        printk(KERN_ERR "decoder_thd: ctx_v2k is NULL, aborting");
        return FAILURE;
    }

    // verify frames buffer
    if (!frames[0] || !frames[1]) {
        printk(KERN_ERR "decoder_thd: frames buffer is not initialized");
        return FAILURE;
    }

    h264_open(NULL);

    while (1) {
        if (frm > metadata.frames) {
            printk(KERN_INFO "decoder_thd: reached max frame %d, break\n", frm);
            break;
        }

        // TODO: make the decoding function write directly to a supplied buffer
        temp_frame = h264_decode_frame(VERBOSE, &my_rc4_state); 
        if (!temp_frame) {
            printk(KERN_INFO "temp_frame = 0, signal VIDEO_DONE\n");
            eventfd_signal(ctx_k2v, VIDEO_DONE);
            return SUCCESS;
        }
        memcpy(frames[frm_buf_id], temp_frame, frame_size);
        eventfd_signal(ctx_k2v, NEW_DATA_AVAILABLE);

        // now wait until viewer signals it's ready for another frame
        if (eventfd_ctx_read(ctx_v2k, 0, &is_viewer_ready) < 0) {
            printk(KERN_ERR "decoder_thd: reading from ctx_v2k failed, abort\n");
            return FAILURE;
        }
        if (is_viewer_ready != READY_FOR_NEW_DATA) {
            printk(KERN_INFO "decoder_thd: unexpected type (expected %d), break\n", READY_FOR_NEW_DATA);
            break;
        }
        frm_buf_id = !frm_buf_id;
        frm++;
    }
    return SUCCESS;
}

int listener_thread_fn(void* arg)
{
    int blk_id = 0;
    int msg_type;
    uint64_t is_new_block_available = NA;
    uint32_t blk_size = 0;
    struct queue_node* node = NULL;

    // verify ctxs are valid
    if (!ctx_l2k) {
        printk(KERN_ERR "listener_thd: ctx_l2k is NULL, aborting\n");
        return FAILURE;
    }
    if (!ctx_k2l) {
        printk(KERN_ERR "listener_thd: ctx_k2l is NULL, aborting\n");
        return FAILURE;
    }

    printk(KERN_INFO "listener_thd: running\n");
    while (1) {
        // wait until listener signals it has a new block
        if (eventfd_ctx_read(ctx_l2k, 0, &is_new_block_available) < 0) {
            printk(KERN_ERR "reading from ctx_l2k failed\n");
            return FAILURE;
        }
        msg_type = GET_TYPE(is_new_block_available);
        blk_size = GET_VAL(is_new_block_available);
        if (msg_type != NEW_DATA_AVAILABLE) {
            if (msg_type == VIDEO_DONE) {
                printk(KERN_INFO "listener_thd: got VIDEO_DONE, breaking\n");
                node = create_empty_node();
                queue_put(node, root);

                break;
            } else {
                printk(KERN_INFO "listener_thd: unexpected type received (%d), abort\n", msg_type);
                return FAILURE;
            }
        }
        node = kmalloc(sizeof(struct queue_node), GFP_KERNEL);
        init_queue_node(node);
        memcpy(node->data, blks[blk_id], blk_size);
        node->data_size = blk_size;
        queue_put(node, root);
        blk_id = !blk_id;
        if (!ctx_k2l) {
            printk(KERN_ERR "listener_thd: ctx_k2l is NULL, abort\n");
            return FAILURE;
        }
        eventfd_signal(ctx_k2l, READY_FOR_NEW_DATA);
    }
    printk(KERN_INFO "listener_thd: exiting\n");
    return 0;
}

int setup_decoder(void);
int setup_listener(void);

// we use ioctl to instruct the module to start either the listener or the viewer threads
long keyj_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int err = 0;

    printk("aviv 1");
    if (_IOC_TYPE(cmd) != KEYJ_IOC_MAGIC) return -ENOTTY;
    if (_IOC_NR(cmd) > KEYJ_IOC_MAXNR) return -ENOTTY;

    /*
     * the direction is a bitmask, and VERIFY_WRITE catches R/W
     * transfers. 'Type' is user-oriented, while
     * access_ok is kernel-oriented, so the concept of "read" and
     * "write" is reversed
     */
    printk("aviv 2");
    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    if (err) return -EFAULT;

    printk("aviv 3");
    switch(cmd) {
        case KEYJ_IOCRENDER:
		
    printk("aviv 4");
            return setup_decoder();

        case KEYJ_IOCLISTENER:
    printk("aviv 5");
            return setup_listener();

        default:  /* redundant, as cmd was checked against MAXNR */
    printk("aviv 6");
            return -ENOTTY;
    }
}

static int keyj_dev_open(struct inode *inode, struct file *file)
{
    // the device can be opened by at most two processes - the listener and the player
    printk(KERN_INFO "Device is opening...\n" );
    if (dev_open > 1)
        return -EBUSY;

    dev_open++;

    printk(KERN_INFO "Device opened successfully, dev_open = %d\n", dev_open);
    return SUCCESS;
}


int setup_listener(void)
{
    int i, Ltotal, Ctotal;

    // calculate the frame size using the image width/height
    printk(KERN_INFO "in setup_listener, metadata = %d %d %d", metadata.width, metadata.height, metadata.frames);
    Ltotal = metadata.height * metadata.width; 
    Ctotal = Ltotal>>2;
    frame_size = sizeof(frame) + Ltotal + 2*Ctotal;

    // init the frames buffer
    mem_buffer = kmalloc(BLKS * BLK_SIZE + 2 * frame_size, GFP_KERNEL);
    if (!mem_buffer) {
        printk(KERN_ERR "memory  buffer allocation failed\n");
        remove_proc_entries();
        return FAILURE;
    }
    for (i = 0; i != BLKS; ++i) {
        blks[i] = (unsigned char*)(mem_buffer + i * BLK_SIZE);
    }

    root = alloc_queue_root();

    printk(KERN_INFO "create listener thread\n");
    listener_thread = kthread_create(listener_thread_fn, NULL, "listener_thread");
    if (listener_thread) {
        wake_up_process(listener_thread);
    }

    return SUCCESS;

}

int setup_decoder(void)
{
    rc4_init(&my_rc4_state, tpvm_key, KEY_LEN); // always successes
    printk(KERN_INFO "in setup_decoder, tpvm_key = %s", tpvm_key);

    frames[0] = (frame*)(mem_buffer + BLKS * BLK_SIZE);
    frames[1] = (frame*)(mem_buffer + BLKS * BLK_SIZE + frame_size);

    printk(KERN_INFO "create decoder thread\n");
    decoder_thread = kthread_create(decoder_thread_fn, NULL, "decoder_thread");
    if (decoder_thread) {
        wake_up_process(decoder_thread);
    }

    return SUCCESS;
}

/* 
 * Called when a process closes the device file.
 */
static int keyj_dev_release(struct inode *inode, struct file *file)
{
    //h264_close();
    dev_open--;
    printk(KERN_INFO "in dev_release, dev_open = %d\n", dev_open);
    if (ctx_k2l) {
        printk(KERN_INFO "send abort to listener\n");
        eventfd_signal(ctx_k2l, ABORT);
    }
    if (!dev_open) {
        printk(KERN_INFO "cleanups\n");
        if (mem_buffer)  {  
            printk(KERN_INFO "free mem_buffer\n");
            kfree(mem_buffer); mem_buffer = 0; 
        }
        if (ctx_v2k) { 
            printk(KERN_INFO "eventfd_ctx_put(ctk_v2k)\n");
            eventfd_ctx_put(ctx_v2k); ctx_v2k = 0;
        }
        if (ctx_l2k) { 
            printk(KERN_INFO "eventfd_ctx_put(ctk_l2k)\n");
            eventfd_ctx_put(ctx_l2k); ctx_l2k = 0; 
        }
        if (ctx_k2v) { 
            printk(KERN_INFO "eventfd_ctx_put(ctk_k2v)\n");
            eventfd_ctx_put(ctx_k2v); ctx_k2v = 0; 
        }
        if (ctx_k2l) { 
            printk(KERN_INFO "eventfd_ctx_put(ctk_k2l)\n");
            eventfd_ctx_put(ctx_k2l); ctx_k2l = 0; 
        }
    }

    printk(KERN_INFO "Device released successfully\n");
    return 0;
}

static int keyj_dev_mmap(struct file *filp, struct vm_area_struct *vma)
{
    if (remap_pfn_range(vma, vma->vm_start, 
                __pa(mem_buffer) >> PAGE_SHIFT,
                vma->vm_end - vma->vm_start,
                vma->vm_page_prot))
        return -EAGAIN;

    return 0;
}

MODULE_AUTHOR("Asaf David");
MODULE_LICENSE("Dual BSD/GPL");

