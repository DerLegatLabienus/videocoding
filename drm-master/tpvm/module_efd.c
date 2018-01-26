#include "module.h"
#include <linux/sched.h>
#include <linux/fdtable.h>

// given the viewer pid and its eventfd descriptor, this function should resolve its 
// ctx_k2v so the kernel can notify the viewer over eventfd
int setup_efd_ctx(int pid, int efd1, int efd2, struct eventfd_ctx** ctx1, struct eventfd_ctx** ctx2)
{
    struct task_struct* userspace_task = 0;
    struct file* efd1_file, *efd2_file;
    if (pid <= 0) {
        printk(KERN_ERR "invalid pid\n");
        return -1;
    }
    if (efd1 <= 0) {
        printk(KERN_ERR "invalid efd1\n");
        return -1;
    }
    if (efd2 <= 0) {
        printk(KERN_ERR "invalid efd2\n");
        return -1;
    }

    // points to userspace program's task struct
    userspace_task = pid_task(find_vpid(pid), PIDTYPE_PID);

    rcu_read_lock();
    // points to eventfd's file struct
    efd1_file = fcheck_files(userspace_task->files, efd1);
    efd2_file = fcheck_files(userspace_task->files, efd2);
    rcu_read_unlock();

    if (!efd1_file || !efd2_file) {
        printk(KERN_ALERT "fcheck_files failed\n");
        return -1;
    }

    *ctx1 = eventfd_ctx_fileget(efd1_file);
    *ctx2 = eventfd_ctx_fileget(efd2_file);
    if (!*ctx1 || !*ctx2) {
        printk(KERN_ALERT "eventfd_ctx_fileget() Jhol, Bye.\n");
        return -1;
    }
    return 0;
}

