#include "module.h"
#include "protocol.h"
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/uidgid.h>

#define INFO_BUFFER_LEN 20

const char* metadata_proc_name      = "tpvm_metadata";
const char* info_viewer_proc_name   = "tpvm_viewer_info";
const char* info_listener_proc_name = "tpvm_listener_info";

extern struct proc_dir_entry;

struct proc_dir_entry *metadata_proc      = NULL, 
                      *info_viewer_proc   = NULL, 
                      *info_listener_proc = NULL;

extern struct eventfd_ctx * ctx_k2v, // kernel signals viewer
                          * ctx_v2k, // viewer signals kernel
                          * ctx_k2l, // kernel signals listener
                          * ctx_l2k; // listener signals kernel

int info_proc_write_inner(struct file *file, const char *buffer, unsigned long count, void *data, const char *name, struct eventfd_ctx** ctx1, struct eventfd_ctx** ctx2) {

    static char info_buffer[INFO_BUFFER_LEN];
    int pid, efd1, efd2;

    if (count > INFO_BUFFER_LEN) count = INFO_BUFFER_LEN;
    if (copy_from_user(info_buffer, buffer, INFO_BUFFER_LEN)) {
        printk(KERN_ERR "info_proc_write_inner::copy_from_user failed");
        return FAILURE;
    }
    info_buffer[INFO_BUFFER_LEN-1] = '\0'; // TODO: needed?

    sscanf(info_buffer, "%d %d %d", &pid, &efd1, &efd2);

    if (setup_efd_ctx(pid, efd1, efd2, ctx1, ctx2) < 0) {
        printk(KERN_ERR "setup_efd_ctx failed for %s\n", name);
        return FAILURE;
    }
    return count;
}

int info_viewer_proc_write(struct file *file, const char *buffer, unsigned long count, void *data) {
    return info_proc_write_inner(file, buffer, count, data, "info_viewer_proc_write", &ctx_k2v, &ctx_v2k);
}

int info_listener_proc_write(struct file *file, const char *buffer, unsigned long count, void *data) {
    return info_proc_write_inner(file, buffer, count, data, "info_listener_proc_write", &ctx_k2l, &ctx_l2k);
}

extern Metadata metadata;

int metadata_proc_write(struct file *file, const char *buffer, unsigned long count, void *data) { 

    if (count > sizeof(metadata)) count = sizeof(metadata);
    if (copy_from_user(&metadata, buffer, sizeof(metadata))) {
        printk(KERN_ERR "metadta_proc_write::copy_from_user failed");
        return FAILURE;
    }

    printk(KERN_INFO "read metadta, %d %d %d", metadata.width, metadata.height, metadata.frames);

    return sizeof(metadata);
}
int setup_proc_fs(void)
{

    printk(KERN_INFO "setup_proc_fs started\n");
    struct file_operations ops =  { .write = metadata_proc_write};
    metadata_proc = proc_create(metadata_proc_name, S_IFREG | S_IRUGO , NULL, &ops);
    if (!metadata_proc) {
        remove_proc_entry(metadata_proc_name, NULL);
        printk(KERN_ALERT "Error: Could not initialize /proc/%s\n", metadata_proc_name);
        return -ENOMEM;
    }

    struct file_operations ops2 =  { .write = info_viewer_proc_write };
    info_viewer_proc = proc_create(info_viewer_proc_name, 0, NULL, &ops2);
    if (!info_viewer_proc) {
        printk(KERN_ALERT "Error: Could not initialize /proc/%s\n", info_viewer_proc_name);
        remove_proc_entries();
        return -ENOMEM;
    }
    struct file_operations ops3 =  { .write = info_listener_proc_write };

    info_listener_proc = proc_create(info_listener_proc_name, 0, NULL, &ops3);
    if (!info_viewer_proc) {
        printk(KERN_ALERT "Error: Could not initialize /proc/%s\n", info_listener_proc_name);
        remove_proc_entries();
        return -ENOMEM;
    }

    kgid_t gid2 = KGIDT_INIT(0);
    kuid_t uid2 = KUIDT_INIT(0);
    proc_set_user(metadata_proc,uid2,gid2);
    proc_set_size(metadata_proc,37);

    printk(KERN_INFO "setup_proc_fs didn't failed\n");
	/* metadata_proc->write_proc      = metadata_proc_write;
    info_viewer_proc->write_proc   = info_viewer_proc_write;
    info_listener_proc->write_proc = info_listener_proc_write;

    metadata_proc->mode = info_viewer_proc->mode = info_listener_proc->mode = S_IFREG | S_IRUGO;                                                       
    metadata_proc->uid  = info_viewer_proc->uid  = info_listener_proc->uid  = 0;                                                                       
    metadata_proc->gid  = info_viewer_proc->gid  = info_listener_proc->gid  = 0;                                                                       
    metadata_proc->size = info_viewer_proc->size = info_listener_proc->size = 37;*/
    return 0;
}

void remove_proc_entries(void) 
{
    if (metadata_proc)      remove_proc_entry(metadata_proc_name,      NULL);
    if (info_viewer_proc)   remove_proc_entry(info_viewer_proc_name,   NULL);
    if (info_listener_proc) remove_proc_entry(info_listener_proc_name, NULL);
}
