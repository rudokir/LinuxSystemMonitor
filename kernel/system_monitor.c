/*
 * System Monitor Kernel Module
 *
 * This module collects various system statistics and exposes them through /proc filesystem.
 * It uses a kernel thread for continuous monitoring and provides a control interface
 * for enabling/disabling monitoring.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/blk-mq.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/part_stat.h>

/* Constants */
#define PROC_NAME "system_monitor"
#define PROC_CONTROL "system_monitor_control"
#define HISTORY_SIZE 60
#define MAX_PROCESSES 50

/* Data Structures */

// Circular buffer for historical stats
static struct {
    u64 cpu_usage[HISTORY_SIZE];
    u64 mem_usage[HISTORY_SIZE];
    int head;
    spinlock_t lock;
} stats_history;

// Store per-process statistics
struct process_stats {
    pid_t pid;
    u64 cpu_time;
    unsigned long vm_size;
    char comm[TASK_COMM_LEN];
};

static struct proc_dir_entry *proc_entry;
static struct proc_dir_entry *control_entry;
static struct timer_list stats_timer;
static struct task_struct *monitor_thread;
static int monitoring = 1;
static struct process_stats top_processes[MAX_PROCESSES];

static void collect_process_stats(void) {
    struct task_struct *task;
    int i = 0;

    rcu_read_lock();
    for_each_process(task) {
        if (i >= MAX_PROCESSES) break;

        struct process_stats *stats = &top_processes[i];
        stats->pid = task->pid;
        stats->cpu_time = task->utime + task->stime;

        if (task->mm) {
            stats->vm_size = task->mm->total_vm << PAGE_SHIFT;
        } else {
            stats->vm_size = 0;
        }

        get_task_comm(stats->comm, task);
        i++;
    }
    rcu_read_unlock();
}

static void get_io_stats(struct seq_file *m) {
    struct task_struct *task;
    unsigned long read_bytes = 0, write_bytes = 0;

    rcu_read_lock();
    for_each_process(task) {
        if (task->ioac.read_bytes) {
            read_bytes += task->ioac.read_bytes;
        }
        if (task->ioac.write_bytes) {
            write_bytes += task->ioac.write_bytes;
        }
    }
    rcu_read_unlock();

    seq_printf(m, "io_stats:%lu,%lu\n", read_bytes, write_bytes);
}

static int monitor_function(void *data) {
    while (!kthread_should_stop()) {
        if (monitoring == 1) {
            collect_process_stats();

            spin_lock(&stats_history.lock);
            stats_history.cpu_usage[stats_history.head] = get_jiffies_64();
            stats_history.mem_usage[stats_history.head] = si_mem_available();
            stats_history.head = (stats_history.head + 1) % HISTORY_SIZE;
            spin_unlock(&stats_history.lock);
        }
        msleep(1000);
    }
    return 0;
}

static void timer_callback(struct timer_list *t) {
    mod_timer(&stats_timer, jiffies + msecs_to_jiffies(1000));
}

static ssize_t control_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos) {
    char cmd[32];
    size_t len = min(count, sizeof(cmd) - 1);

    if (copy_from_user(cmd, buffer, len)) {
        return -EFAULT;
    }

    cmd[len] = '\0';

    if (strncmp(cmd, "enable", 6) == 0) {
        monitoring = 1;
    } else if (strncmp(cmd, "disable", 7) == 0) {
        monitoring = 0;
    }

    return count;
}

static void show_history(struct seq_file *m) {
    int i;
    spin_lock(&stats_history.lock);
    seq_puts(m, "history:\n");
    for (i = 0; i < HISTORY_SIZE; i++) {
        int idx = (stats_history.head - i - 1 + HISTORY_SIZE) % HISTORY_SIZE;
        seq_printf(m, "%d,%llu,%llu\n", i, stats_history.cpu_usage[idx], stats_history.mem_usage[idx]);
    }
    spin_unlock(&stats_history.lock);
}

static void show_top_processes(struct seq_file *m) {
    int i;
    seq_puts(m, "\ntop_processes:\n");
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (top_processes[i].pid == 0) break;
        seq_printf(m, "%d,%s,%llu,%lu\n", top_processes[i].pid, top_processes[i].comm, top_processes[i].cpu_time, top_processes[i].vm_size);
    }
}

static void get_cpu_stats(struct seq_file *m) {
    int cpu;
    u64 user = 0, nice = 0, system = 0, idle = 0;

    for_each_possible_cpu(cpu) {
        struct kernel_cpustat *kcs = &kcpustat_cpu(cpu);
        user += kcs->cpustat[VTIME_USER];
        nice += kcs->cpustat[CPUTIME_NICE];
        system += kcs->cpustat[VTIME_SYS];
        idle += kcs->cpustat[VTIME_IDLE];
    }

    seq_printf(m, "cpu_stats:%llu,%llu,%llu,%llu\n", user, nice, system, idle);
}

static void get_memory_stats(struct seq_file *m) {
    struct sysinfo si;
    si_meminfo(&si);

    seq_printf(m, "memory_stats:%lu,%lu,%lu\n", si.totalram << (PAGE_SHIFT - 10), si.freeram << (PAGE_SHIFT - 10), (si.totalram - si.freeram) << (PAGE_SHIFT - 10));
}

static void get_process_count(struct seq_file *m) {
    struct task_struct *task;
    int count = 0;

    rcu_read_lock();
    for_each_process(task) {
        count++;
    }
    rcu_read_unlock();

    seq_printf(m, "process_count:%d\n", count);
}

static void get_network_stats(struct seq_file *m) {
    struct net_device *dev;
    unsigned long rx_bytes = 0, tx_bytes = 0, rx_packets = 0, tx_packets = 0;

    rcu_read_lock();
    for_each_netdev_rcu(&init_net, dev) {
        struct rtnl_link_stats64 temp;
        struct rtnl_link_stats64 *stats = dev_get_stats(dev, &temp);

        rx_bytes += stats->rx_bytes;
        tx_bytes += stats->tx_bytes;
        rx_packets += stats->rx_packets;
        tx_packets += stats->tx_packets;
    }
    rcu_read_unlock();

    seq_printf(m, "network_stats:%lu,%lu,%lu,%lu\n", rx_bytes, tx_bytes, rx_packets, tx_packets);
}

static int system_stats_show(struct seq_file *m, void *v) {
    get_cpu_stats(m);
    get_memory_stats(m);
    get_process_count(m);
    get_io_stats(m);
    get_network_stats(m);
    show_history(m);
    show_top_processes(m);
    return 0;
}

static int system_stats_open(struct inode *inode, struct file *file) {
    return single_open(file, system_stats_show, NULL);
}

static const struct proc_ops system_stats_fops = {
    .proc_open = system_stats_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
static const struct proc_ops control_fops = {
    .proc_write = control_write,
};

static int __init system_monitor_init(void) {
    spin_lock_init(&stats_history.lock);
    stats_history.head = 0;

    proc_entry = proc_create(PROC_NAME, 0444, NULL, &system_stats_fops);
    control_entry = proc_create(PROC_CONTROL, 0222, NULL, &control_fops);
    if (!proc_entry || !control_entry) {
        return -ENOMEM;
    }

    timer_setup(&stats_timer, timer_callback, 0);
    mod_timer(&stats_timer, jiffies + msecs_to_jiffies(1000));

    monitor_thread = kthread_run(monitor_function, NULL, "system_monitor");
    if (IS_ERR(monitor_thread)) {
        return PTR_ERR(monitor_thread);
    }

    printk(KERN_INFO "System Monitor Module loaded\n");
    return 0;
}

static void __exit system_monitor_exit(void) {
    del_timer_sync(&stats_timer);
    kthread_stop(monitor_thread);
    proc_remove(proc_entry);
    proc_remove(control_entry);
    printk(KERN_INFO "System Monitor Module unloaded\n");
}

module_init(system_monitor_init);
module_exit(system_monitor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("rudokir");
MODULE_DESCRIPTION("System Statistics Kernel Module");
