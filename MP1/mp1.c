#define LINUX

#include <linux/module.h>
#include <linux/kernel.h>
#include "mp1_given.h"

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/jiffies.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("059");
MODULE_DESCRIPTION("CS-423 MP1");

#define DEBUG 1
#define DIRECTORY "mp1"
#define FILE "status"
#define INTERVAL 5000

// define struct proc_list
typedef struct {
    struct list_head list;
    unsigned int pid;
    unsigned long cpu_time;
} proc_list;

static struct proc_dir_entry *proc_dir, *proc_entry;
LIST_HEAD(mp1_proc_list);
static struct timer_list mp1_timer;
static struct workqueue_struct *mp1_workqueue;
static spinlock_t mp1_lock;
static struct work_struct *mp1_work;

static const struct file_operations mp1_fops = {
    .owner   = THIS_MODULE,
    .read    = mp1_read,
    .write   = mp1_write
};

void mp1_timer_callback(unsigned long data)
{
    queue_work(mp1_workqueue, mp1_work);
}

static void mp1_work_function(struct work_struct *work)
{
    unsigned long flags;
    proc_list *pos, *n;
    // enter critical section
    spin_lock_irqsave(&mp1_lock, flags);
    list_for_each_entry_safe(pos, n, &mp1_proc_list, list) {
        // unregister the process from mp1_proc_list if the process does not exist
        if (get_cpu_use(pos->pid, &pos->cpu_time) == -1) {
            list_del(&pos->list);
            kfree(pos);
        }
    }
    spin_unlock_irqrestore(&mp1_lock, flags);

    mod_timer(&mp1_timer, jiffies + msecs_to_jiffies(TIMEINTERVAL));
}

// mp1_init - Called when module is loaded
int __init mp1_init(void)
{
   #ifdef DEBUG
   printk(KERN_ALERT "MP1 MODULE LOADING\n");
   #endif
   // Insert your code here ...
   
   // create directory: proc/mp1
   proc_dir = proc_mkdir(DIRECTORY, NULL);

   // create file: proc/mp1/status
   proc_entry = proc_create(FILENAME, 0666, proc_dir, &mp1_fops);
   
   // create workqueue and allocate memory for work
   mp1_workqueue = create_workqueue("mp1_workqueue");
   mp1_work = (struct work_struct *)kmalloc(sizeof(struct work_struct), GFP_KERNEL);
    
   // initialize lize workqueue
   INIT_WORK(mp1_work, mp1_work_function);

   // set and start timer
   setup_timer(&mp1_timer, mp1_timer_callback, 0);
   mod_timer(&mp1_timer, jiffies + msecs_to_jiffies(TIMEINTERVAL));

   // initialize lock
   spin_lock_init(&mp1_lock);

   printk(KERN_ALERT "MP1 MODULE LOADED\n");
   return 0;   
}

// mp1_exit - Called when module is unloaded
void __exit mp1_exit(void)
{
   #ifdef DEBUG
   printk(KERN_ALERT "MP1 MODULE UNLOADING\n");
   #endif
   // Insert your code here ...
   
   // remove file 
   remove_proc_entry(FILE, proc_dir);
   
   // remove directory
   remove_proc_entry(DIRECTORY, NULL);
   
   // to avoid list broken, use safe function to free mp1_proc_list
   list_for_each_entry_safe(pos, n, &mp1_proc_list, list) {
        list_del(&pos->list);
        kfree(pos);
    }

	// free workqueue
	flush_workqueue(mp1_workqueue);
	destroy_workqueue(mp1_workqueue);

    // free work
    kfree(mp1_work);

    // free timer
    del_timer_sync(&mp1_timer);

   printk(KERN_ALERT "MP1 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp1_init);
module_exit(mp1_exit);
