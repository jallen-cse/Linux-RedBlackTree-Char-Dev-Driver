#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>

#include <linux/fs.h>           //file system
#include <linux/cdev.h>         //character device file
#include <asm/uaccess.h>      //user access
#include <linux/device.h>
#include <linux/kdev_t.h>

#include <linux/slab.h>         //kmalloc and kfree

#include <linux/string.h>

#include <linux/rbtree.h>

#define DRIVER_NAME "rb438_drv"

struct rb_packet {
    int key;
    int insert;
    char data[4];
};

struct rb_object {
    struct rb_node node;
    int key;
    char data[4];
};

//https://www.kernel.org/doc/html/latest/core-api/rbtree.html
struct rb_object* search(struct rb_root* root, int key)
{
    struct rb_object* rb_obj;
    struct rb_node* iter_node = root->rb_node;

    while(iter_node)
    {
        rb_obj = rb_entry(iter_node, struct rb_object, node);
        
        if (rb_obj->key > key)
        {
            iter_node = iter_node->rb_left;         //go left
        }
        else if (rb_obj->key < key)
        {
            iter_node = iter_node->rb_right;        //go right
        }
        else                                        
        {
            return rb_obj;                          //found node
        }
    }

    return NULL;                            //not found in tree
}

//https://www.kernel.org/doc/html/latest/core-api/rbtree.html
int insert(struct rb_root *root, struct rb_object* data)
{
    struct rb_node **new = &(root->rb_node), *parent = NULL;
    struct rb_object *this;

    /* Figure out where to put new node */
    while (*new)
    {
        this = rb_entry(*new, struct rb_object, node);

        parent = *new;

        if (data->key > this->key)
        {
            new = &((*new)->rb_right);
        }
        else if (data->key < this->key)
        {
            new = &((*new)->rb_left);
        }
        else
        {
            //if key is already in tree, just copy the data over
            memcpy(this->data, data->data, 4);
            return 1;
        }
    }

    //node and balance tree
    rb_link_node(&data->node, parent, new);
    rb_insert_color(&data->node, root);

    return 0;
}

struct device_container {
    struct cdev cdev;
    int id;
    struct rb_root root;
    char dev_buffer[12];
    int head_pos;
} *dev_container1, *dev_container2;

//prototypes; defined later
static ssize_t read_dev(struct file*, char __user*, size_t, loff_t*);
static ssize_t write_dev(struct file*, const char __user*, size_t, loff_t*);
static int open_dev(struct inode*, struct file*);
static int release_dev(struct inode*, struct file*);
static long ioctl_dev(struct file*, unsigned int, unsigned long);

//file operations struct used by both devices
struct file_operations file_ops = {
    .owner = THIS_MODULE,
    .read = read_dev,
    .write = write_dev,
    .open = open_dev,
    .release = release_dev,
    .unlocked_ioctl = ioctl_dev
};

//stores identifiers
static dev_t dev_t1, dev_t2;

//character device names
static char* rb_dev1_name = "rb438_dev1";
static char* rb_dev2_name = "rb438_dev2";

//class under which the devices are created
struct class *dev_class;

//TEMP
char message[] = "test message\n";

//change class dev_class permissions
static int uevent_handler(struct device *dev, struct kobj_uevent_env *env)
{
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

//on sudo insmod rb438_drv
static int rb438_drv_init(void)
{
    //dynamically allocated registration; check errors
    if (alloc_chrdev_region(&dev_t1, 0, 1, DRIVER_NAME) < 0 || alloc_chrdev_region(&dev_t2, 1, 2, DRIVER_NAME) < 0)
    {   printk(KERN_ALERT "Unable to allocate device major/minor numbers\n"); return -1;   }

    //allocate kernel space memory to both device container structs
    dev_container1 = kmalloc(sizeof(struct device_container), GFP_KERNEL);
    dev_container2 = kmalloc(sizeof(struct device_container), GFP_KERNEL);

    //initialize character devices with file operations defined earlier
    cdev_init(&dev_container1->cdev, &file_ops);
    cdev_init(&dev_container2->cdev, &file_ops);

    //easy identification for debugging and logging
    dev_container1->id = 1;
    dev_container2->id = 2;

    //init head positions of each device, rightmost by default
    dev_container1->head_pos = 0;
    dev_container2->head_pos = 0;

    //create roots of both rbtrees
    dev_container1->root = RB_ROOT;
    dev_container2->root = RB_ROOT;

    //connect major/minor numbers to character devices; check errors
    if (cdev_add(&dev_container1->cdev, dev_t1, 1) < 0 || cdev_add(&dev_container2->cdev, dev_t2, 1))
    {   printk(KERN_ALERT "Unable to add devices to system\n"); goto fail1;   }

    //create a device class to be used by both character devices
    if ((dev_class = class_create(THIS_MODULE, "rb438_drv_class")) == 0)
    {   printk(KERN_ALERT "Cannot create class\n"); goto fail1;   }

    //assign uevent callback; callback changes class permissions
    dev_class->dev_uevent = uevent_handler;

    //create character devices to be shown in file system
    if (device_create(dev_class, NULL, dev_t1, NULL, rb_dev1_name) == 0 || device_create(dev_class, NULL, dev_t2, NULL, rb_dev2_name) == 0)
    {   printk(KERN_ALERT "Cannot create device\n"); goto fail2;   }

    //everything was successful, we are ready to go
    printk("Loaded rb438_drv.  major: %d\n", MAJOR(dev_t1));
    return 0;

    //in cases of failure in device creation steps above
    fail2:
    class_destroy(dev_class);
    fail1:
    unregister_chrdev_region(dev_t1, 1);
    unregister_chrdev_region(dev_t2, 1);
    return -1;
}

//on sudo rmmod rb438_drv
static void rb438_drv_exit(void)
{    
    //to deallocate all memory and destroy trees
    struct rb_node *next1, *iter1 = rb_first(&dev_container1->root);
    struct rb_node *next2, *iter2 = rb_first(&dev_container1->root);

    while (iter1)
    {
        //store next node in tree
        next1 = rb_next(iter1);

        //remove node from tree
        rb_erase(iter1, &dev_container1->root);

        //free mem from containing object
        kfree(rb_entry(iter1, struct rb_object, node));

        //iterate to next node
        iter1 = next1;
    }

    //same as above loop, but for second tree
    while (iter2)
    {
        next2 = rb_next(iter2);
        rb_erase(iter2, &dev_container2->root);
        kfree(rb_entry(iter2, struct rb_object, node));
        iter2 = next2;
    }
    
    device_destroy(dev_class, dev_t1);
    device_destroy(dev_class, dev_t2);
    class_destroy(dev_class);    
    cdev_del(&dev_container1->cdev);
    cdev_del(&dev_container2->cdev);
    unregister_chrdev_region(dev_t1, 1);
    unregister_chrdev_region(dev_t2, 1);

    kfree(dev_container1);
    kfree(dev_container2);
    
    printk("Unloaded rb438_drv\n");
}

static ssize_t read_dev(struct file* file, char __user* user_buffer, size_t len, loff_t* _)
{
    /*to retrieve either the leftmost or rightmost object of the RB tree and then the node is
    removed. If the RB tree is empty, -1 is returned and errno is set to EINVAL (i.e., the read file
    operation returns-EINVAL).*/

    //which device was opened by the user
    struct device_container* dc = (struct device_container*)file->private_data;

    //used to serialize data sent to user
    struct rb_packet packet;

    //object containing leftmost or rightmost node
    struct rb_object* first_or_last;

    //first_or_last->key = 11;
    //memcpy(packet.data, "abcd", 4);

    //if device is rb438_dev1, read leftmost; else read rightmost
    if (dc->head_pos == 1)
    {
        //read leftmost
        struct rb_node *leftmost = rb_first(&dc->root);
        if(leftmost == NULL) { return -EINVAL; }

        first_or_last = container_of(leftmost, struct rb_object, node);
    }
    else
    {
        //read rightmost
        struct rb_node *rightmost = rb_last(&dc->root);
        if(rightmost == NULL) { return -EINVAL; }

        first_or_last = container_of(rightmost, struct rb_object, node);
    }

    rb_erase(&first_or_last->node, &dc->root);

    //prepare packet for serialization
    packet.key = first_or_last->key;
    memcpy(packet.data, first_or_last->data, 4);

    //free memory when removed from tree
    kfree(first_or_last);

    //send packet to user
    if(copy_to_user(user_buffer, &packet, 12)){}

    return 0;
}

static ssize_t write_dev(struct file* file, const char __user* user_buffer, size_t len, loff_t* _)
{
    /*write: if the input object of rb_object_t has a non-null char in data[0], a node is created and added
    to the RB tree. If an object with the same key already exists in the tree, it should be replaced with
    the new one. If data[0] is null, any existing object with the same input key is deleted from the tree.        */

    struct device_container* dc = (struct device_container*)file->private_data;

    //used for pointing to objects that are being manipulated in the below block
    struct rb_object *rb_obj = NULL;

    //used to deserialize data received from user, points to device buffer
    struct rb_packet* packet;

    //ensure device buffer is clear
    memset(dc->dev_buffer, 0, 12);
    
    //copy from user space, check for error
    if (copy_from_user(dc->dev_buffer, user_buffer, len) != 0){}
    //{   printk(KERN_ALERT "too much written to rb438_dev%d\n", dc->id);   }

    //derserialize packet into struct
    packet = (struct rb_packet*)dc->dev_buffer;
    //printk(KERN_INFO "got %d, %d, %s\n", packet->key, packet->insert, packet->data);

    //this is determined by the user given the input script
    if (packet->insert)         //if we want to insert/replace a node
    {
        //allocate mem for new node
        rb_obj = (struct rb_object*)kmalloc(sizeof(struct rb_object), GFP_KERNEL);

        //set key and data of tree node obj
        rb_obj->key = packet->key;
        memcpy(rb_obj->data, packet->data, 4);

        //insert; if key already exists, value is just copied over in insert function, so free new rb_obj
        if(insert(&dc->root, rb_obj))
        {
            kfree(rb_obj);
            //printk(KERN_ALERT "Node already in tree; value has been updated\n");
        }

        //
        // BLOCK HERE WAS MOVED TO BOTTOM FOR STORAGE
        //

    }
    else                        //if we want to remove a node
    {
        //find node in tree
        rb_obj = search(&dc->root, packet->key);
        
        //if node was found in tree
        if (rb_obj != NULL)
        {
            rb_erase(&rb_obj->node, &dc->root);     //erase from free where we found it
            kfree(rb_obj);                          //free allocated mem
        }
    }

    //printk(KERN_INFO "Successfully inserted node with key %d to tree.\n", packet->key);

    return 0;
}

static long ioctl_dev(struct file* file, unsigned int cmd, unsigned long arg)
{
    struct device_container *dc = (struct device_container*)file->private_data;

    if(cmd > 1)     //error
    {
        return -1;
    }
    
    //change head position
    dc->head_pos = cmd;

    return 0;
}

static int open_dev(struct inode* inode, struct file* file)
{
    //load file struct with device_container corresponding to the opened character device
    file->private_data = container_of(inode->i_cdev, struct device_container, cdev);

    printk("Opened rb438_dev%d\n", container_of(inode->i_cdev, struct device_container, cdev)->id);
    return 0;
}

static int release_dev(struct inode* inode, struct file* file)
{
    struct device_container* dc = file->private_data;
    
    printk("Closed rb438_dev%d\n", dc->id);
    return 0;
}

module_init(rb438_drv_init);
module_exit(rb438_drv_exit);

MODULE_LICENSE("GPL");  //avoid annoying error