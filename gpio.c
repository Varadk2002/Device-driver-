// gpio device driver
//  1. module initialization -- gpio ready
//      - device number allocation
//      - class and device creation
//      - cdev init and add
//      - led state variable -- off
//      - check if led gpio is valid
//      - acquire/request led gpio
//      - change led gpio direction as output
//  2. module de-initialization
//      - led gpio release
//      - cdev del
//      - device and class destroy
//      - release device number
//  3. open() and close()
//      - do nothing
//  4. write()
//      - user space test application expected to send "1" or "0"
//      - get buf data from user space.
//      - if user buf is "1", then set gpio value to 1 (also modify led_state var)
//      - if user buf is "0", then set gpio value to 0 (also modify led_state var)
//      - if user buf is not "0" or "1", then ignore it.
//  5. read()
//      - if led_state is 0, write "0" to user buf
//      - if led_state is 1, write "1" to user buf

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/gpio.h>

// led connections - GPIO_48 i.e. GPIO1.16 i.e. BBB P9.15
#define DESD_LED        48
static int led_state = 0;

// function declarations
static int desd_led_open(struct inode *pinode, struct file *pfile);
static int desd_led_close(struct inode *pinode, struct file *pfile);
static ssize_t desd_led_read(struct file *pfile, char __user *ubuf, size_t bufsize, loff_t *poffset);
static ssize_t desd_led_write(struct file *pfile, const char __user *ubuf, size_t bufsize, loff_t *poffset);

// global variables
static dev_t devno;
static struct class *pclass;
static struct file_operations desd_led_ops = {
    .owner = THIS_MODULE,
    .open = desd_led_open,
    .release = desd_led_close,
    .read = desd_led_read,
    .write = desd_led_write
};
static struct cdev desd_led_cdev;

// module initialization function
static int __init desd_led_init(void) {
    int ret;
    struct device *pdevice;
    pr_info("%s: desd_led_init() called.\n", THIS_MODULE->name);
    // allocate device number
    ret = alloc_chrdev_region(&devno, 0, 1, "desd_led");
    if(ret < 0) {
        pr_err("%s: alloc_chrdev_region() failed.\n", THIS_MODULE->name);
        goto alloc_chrdev_region_failed;
    }
    pr_info("%s: device number = %d/%d.\n", THIS_MODULE->name, MAJOR(devno), MINOR(devno));
    // create device class
    //pclass = class_create(THIS_MODULE, "desd_led_class"); // for kernel 5.x
    pclass = class_create("desd_led_class"); // for kernel 6.x
    if(IS_ERR(pclass)) {
        pr_err("%s: class_create() failed.\n", THIS_MODULE->name);
        ret = -1;
        goto class_create_failed;
    }
    pr_info("%s: device class is created.\n", THIS_MODULE->name);
    // create device file
    pdevice = device_create(pclass, NULL, devno, NULL, "desd_led");
    if(IS_ERR(pdevice)) {
        pr_err("%s: device_create() failed.\n", THIS_MODULE->name);
        ret = -1;
        goto device_create_failed;
    }
    pr_info("%s: device file is created.\n", THIS_MODULE->name);
    // init cdev struct and add into kernel db
    cdev_init(&desd_led_cdev, &desd_led_ops);
    ret = cdev_add(&desd_led_cdev, devno, 1);
    if(ret < 0) {
        pr_err("%s: cdev_add() failed.\n", THIS_MODULE->name);
        goto cdev_add_failed;
    }
    pr_info("%s: device cdev is added in kernel.\n", THIS_MODULE->name);
    // gpio config
    ret = gpio_is_valid(DESD_LED);
    if(!ret) {
        pr_err("%s: gpio_is_valid() returned false - gpio invalid.\n", THIS_MODULE->name);
        goto gpio_is_valid_failed;        
    }
    pr_info("%s: gpio_is_valid() for gpio %d.\n", THIS_MODULE->name, DESD_LED);

    ret = gpio_request(DESD_LED, "desd_led");
    if(ret != 0) {
        pr_err("%s: gpio_request() failed.\n", THIS_MODULE->name);
        goto gpio_request_failed;
    }
    pr_info("%s: gpio_request() success for gpio %d.\n", THIS_MODULE->name, DESD_LED);

    ret = gpio_direction_output(DESD_LED, led_state);
    if(ret != 0) {
        pr_err("%s: gpio_direction_output() failed.\n", THIS_MODULE->name);
        goto gpio_direction_output_failed;
    }
    pr_info("%s: gpio_direction_output() set gpio %d as output.\n", THIS_MODULE->name, DESD_LED);
    
    return 0; // module initialized successfully.

gpio_direction_output_failed:
    gpio_free(DESD_LED);
gpio_request_failed:
gpio_is_valid_failed:
    cdev_del(&desd_led_cdev);
cdev_add_failed:
    device_destroy(pclass, devno);
device_create_failed:
    class_destroy(pclass);
class_create_failed:
    unregister_chrdev_region(devno, 1);
alloc_chrdev_region_failed:
    return ret;
}


// module de-initialization function
static void __exit desd_led_exit(void) {
    pr_info("%s: desd_led_exit() called.\n", THIS_MODULE->name);
    // release gpio pin
    gpio_free(DESD_LED);
    pr_info("%s: led gpio %d is released.\n", THIS_MODULE->name, DESD_LED);
    // remove device cdev from the kernel db.
    cdev_del(&desd_led_cdev);
    pr_info("%s: device cdev is removed from kernel.\n", THIS_MODULE->name);
    // destroy device file
    device_destroy(pclass, devno);
    pr_info("%s: device file is destroyed.\n", THIS_MODULE->name);
    // destroy device class
    class_destroy(pclass);
    pr_info("%s: device class is destroyed.\n", THIS_MODULE->name);
    // unallocate device number
    unregister_chrdev_region(devno, 1);
    pr_info("%s: device number released.\n", THIS_MODULE->name);
}

// pchar file operations
static int desd_led_open(struct inode *pinode, struct file *pfile) {
    pr_info("%s: desd_led_open() called.\n", THIS_MODULE->name);
    return 0;
}

static int desd_led_close(struct inode *pinode, struct file *pfile) {
    pr_info("%s: desd_led_close() called.\n", THIS_MODULE->name);
    return 0;
}

static ssize_t desd_led_read(struct file *pfile, char __user *ubuf, size_t bufsize, loff_t *poffset) {
    int nbytes;
    pr_info("%s: desd_led_read() called.\n", THIS_MODULE->name);
    // send led state to user space
    if(led_state == 1) {
        char kbuf[2] = "1";
        nbytes = 2 - copy_to_user(ubuf, kbuf, 2);
    }
    else if(led_state == 0) {
        char kbuf[2] = "0";
        nbytes = 2 - copy_to_user(ubuf, kbuf, 2);
    }
    // returns num of bytes successfully read.
    return nbytes;  
}

static ssize_t desd_led_write(struct file *pfile, const char __user *ubuf, size_t bufsize, loff_t *poffset) {
    int nbytes;
    char kbuf[2] = "";
    pr_info("%s: desd_led_write() called.\n", THIS_MODULE->name);
    // copy user buf into kernel buf
    nbytes = 1 - copy_from_user(kbuf, ubuf, 1);
    // if user buf is 1 -- led_state=1 and on led
    if(kbuf[0] == '1') {
        led_state = 1;
        gpio_set_value(DESD_LED, led_state);
        pr_info("%s: desd_led_write() -- Led ON.\n", THIS_MODULE->name);    
    }
    // if user buf is 0 -- led_state=0 and off led
    else if(kbuf[0] == '0') {
        led_state = 0;
        gpio_set_value(DESD_LED, led_state);
        pr_info("%s: desd_led_write() -- Led OFF.\n", THIS_MODULE->name);    
    } 
    // otherwise ignore
    // returns num of bytes successfully written.
    return bufsize;
}

module_init(desd_led_init);
module_exit(desd_led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Varad Kalekar ");
MODULE_DESCRIPTION("Hello Char Device Driver");
