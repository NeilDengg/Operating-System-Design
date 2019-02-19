#define LINUX

#include <linux/module.h>
#include <linux/kernel.h>
#include "mp1_given.h"

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>


#include <linux/slab.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>




MODULE_LICENSE("GPL");
MODULE_AUTHOR("059");
MODULE_DESCRIPTION("CS-423 MP1");

LIST_HEAD(head_proc_list);

/**
Linked list struct to store registered pid and corresponding cpt time
*/
typedef struct {
  struct list_head list;
  unsigned long cpu_time;
  unsigned int pid;
}proc_list;

// int flag = 1;
#define DEBUG 1
#define FILENAME "status"
#define DIRECTORY "mp1"
#define TIME 5000

/**
static variables and struct
*/
static struct proc_dir_entry *proc_dir;
static struct timer_list mp1_timer;
static struct work_struct mp1_work;


struct workqueue_struct *mp1_q;
static spinlock_t mp1_lock;


/**
Called when user request cpu time about registed pid
Go through the linked list, read cpt time of each registered pid, save in the buffer for caller
Return: number of bytes read
*/
static ssize_t mp1_read (struct file *file, char __user *buffer, size_t count, loff_t *data)
{
  printk(KERN_ALERT "read function is called!!! %d", *data);
  // if(*data>0)
  //   return 0;

  unsigned long copied = 0;
  char * buf;
  struct proc_list *tmp;
  int offset = 0;

  buf = (char *) kmalloc(count,GFP_KERNEL); 
  // enter critical section

  spin_lock(&mp1_lock);
  
  list_for_each_entry(tmp, &head_proc_list, list) {
    char temp[256];
    sprintf(temp, "%lu: %lu ms\n", tmp->pid, jiffies_to_msecs(tmp->cpu_time));
    strcpy(buf + offset, temp);
    offset = strlen(buf);
  }

  spin_unlock(&mp1_lock);
  
  // critical section end
  
  copied = strlen(buf)+1;
  
  copy_to_user(buffer, buf, copied);
  
  kfree(buf);
  
  printk(KERN_ALERT "READ COUNT COPIED %d\t%d\n", count, copied);
  
  *data += copied;
  
  return copied;
}

/**
Called when user register pid to the linked list
Initiate a new block and add it to the linked list
*/
static ssize_t mp1_write (struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
  int copied = 0;
  struct proc_list* tmp;
  char * buf;

  // Initialize tmplist
  tmp  = kmalloc(sizeof(struct proc_list), GFP_KERNEL);
  INIT_LIST_HEAD(&tmp->list);



  buf = (char *) kmalloc(count+1,GFP_KERNEL); 
  copy_from_user(buf, buffer, count);
  buf[count] = '\0';
  sscanf(buf,"%u",&tmp->pid);
  printk("buf is %s",buf);
  //   if(kstrtol(buf, 10, &(tmp->pid)))
  // {
  //   int i = 0;
  //   while( buf[i] != '\0' )
  //   {
  //     printk("ERROR STR TO LONG: %x\n", *(buf+i));
  //     i++;
  //   }
  // }
  printk("PID AT BEGINNING %d \n", tmp->pid);
  printk("PID: %d\n", tmp->pid);
  tmp->cpu_time = 1337;
  // critical section begin
  spin_lock(&mp1_lock);
  list_add(&tmp->list, &head_proc_list);
  spin_unlock(&mp1_lock);
  // critical section end
  printk("%d \n", ((struct mp1_list*)head_proc_list.next)->cpu_time);

  kfree(buf);
  return count;
}

static const struct file_operations mp1_file = {
  .owner = THIS_MODULE, 
  .read = mp1_read,
  .write = mp1_write,
};

/**
Top half interrupt:
setup up a mp1_timer for 5 seconds and put the bottom half on the workqueue
*/
void mp1_timer_callback( unsigned long data )
{
    // setup_timer( &mp1_timer, mp1_timer_callback, 0 );
    queue_work(mp1_q, &mp1_work);
    // mod_timer( &mp1_timer, jiffies + msecs_to_jiffies(5000) );
}

/**
Bottom half interrupt:
go through the linked list of registered pid and update cpu time for each
remove the registerd pid if the job is completed
*/
static void bottom_fn(void *ptr)
{
  struct mp1_list *tmp;
  struct mp1_list *temp_entry;
  // critical section begin
  spin_lock(&mp1_lock);
  list_for_each_entry_safe(tmp,temp_entry, &head_proc_list, list) 
  {
    unsigned long cpu_value;
    if(get_cpu_use(tmp->pid, &cpu_value)==-1)
    // {
    //   //success
    //   tmp->cpu_time = cpu_value;
    //   printk("RECORDED CPU TIME FOR PID %d: %d\n", tmp->pid, jiffies_to_msecs(tmp->cpu_time));
    // }
    // else  // job finished, remove from linked list 
    {
      list_del(&(tmp->list));
      printk("ERROR: CAN'T GET CPU USE FOR PID: %d, So this process is now being deleted\n", tmp->pid);
    }
  }
  spin_unlock(&mp1_lock);
  mod_timer( &mp1_timer, jiffies + msecs_to_jiffies(5000) );
  // critical section end
}

/**
mp1_init - Called when module is loaded
create proc directory and file
set up mp1_timer
create workqueue and work struct
*/
int __init mp1_init(void)
{
  #ifdef DEBUG
  printk(KERN_ALERT "MP1 MODULE LOADING\n");
  #endif
  proc_dir =  proc_mkdir("mp1",NULL);
  proc_create("status",0666, proc_dir, &mp1_file);  
  spin_lock_init(&mp1_lock);
  setup_timer( &mp1_timer, mp1_timer_callback, 0 ); 
  mod_timer( &mp1_timer, jiffies + msecs_to_jiffies(5000) ); 
  
  mp1_q = create_workqueue("mp_queue");
  INIT_WORK(&mp1_work, &bottom_fn);
  printk(KERN_ALERT "MP1 MODULE LOADED\n");

  return 0;   
}

/**
mp1_exit - Called when module is unloaded
remove proc directory and file
clean the linked list and free all memory used
destroy workqueue and mp1_timer
*/
void __exit mp1_exit(void)
{
  #ifdef DEBUG
  printk(KERN_ALERT "MP1 MODULE UNLOADING\n");
  #endif
  struct list_head* n = head_proc_list.next; 
  struct mp1_list* ptr;
  remove_proc_entry("status", proc_dir);
  remove_proc_entry("mp1", NULL);

  while (n != &head_proc_list) {
    ptr = (struct mp1_list*) n;
    n = n->next;
    kfree(ptr);
  }
  del_timer( &mp1_timer );
  destroy_workqueue(mp1_q);
  printk(KERN_ALERT "MP1 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp1_init);
module_exit(mp1_exit);