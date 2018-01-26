#ifndef __MODULE_H__
#define __MODULE_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/eventfd.h>

#define DEVICE_NAME "tpvm"  /* Dev name as it appears in /proc/devices   */
#define BUF_LEN 80          /* Max length of the message from the device */

#define FRAME_HDR_LEN (6*sizeof(int))
#define DEFAULT_KEY "keykeykey"

#define KEYJ_IOC_MAGIC  'k'
#define KEYJ_IOCRENDER   _IOW(KEYJ_IOC_MAGIC,  0, int)
#define KEYJ_IOCLISTENER _IOW(KEYJ_IOC_MAGIC,  1, int)

#define KEYJ_IOC_MAXNR 1
#define SUCCESS  0
#define FAILURE -1

int init_module(void);
void cleanup_module(void);

// /proc handling
int setup_proc_fs(void);
void remove_proc_entries(void);

// efd handling
int setup_efd_ctx(int, int, int, struct eventfd_ctx**, struct eventfd_ctx**);

#endif
