#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/uaccess.h>

#define I2C_SLAVE_NAME "max30100"

// Register definitions
#define REG_INT_STATUS 0x00
#define REG_MODE_CONFIG 0x06
#define REG_SPO2_CONFIG 0x07
#define REG_LED_CONFIG 0x09
#define REG_FIFO_DATA 0x05

// Global pointer for the I2C client.
static struct i2c_client *max30100_client = NULL;

// Character device globals
static dev_t devno;
static struct class *max30100_class;
static struct cdev max30100_cdev;

// Forward declarations
static int max30100_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int max30100_remove(struct i2c_client *client);
static int max30100_open(struct inode *inode, struct file *pfile);
static int max30100_close(struct inode *inode, struct file *pfile);
static ssize_t max30100_read(struct file *file, char __user *buf, size_t count, loff_t *offset);
static int max30100_init_chip(void);

// Helper function to write a byte to a register
static int max30100_write_reg(u8 reg, u8 val)
{
    if (!max30100_client)
        return -ENODEV;
    return i2c_smbus_write_byte_data(max30100_client, reg, val);
}

// Helper function to read a byte to a register
static int max30100_read_reg(u8 reg)
{
    if (!max30100_client)
        return -ENODEV;
    return i2c_smbus_read_byte_data(max30100_client, reg);
}

// Initializes the MAX30100 chip with the required settings
static int max30100_init_chip(void)
{
    int status;
    int retries = 10;

    while (retries--)
    {
        status = max30100_read_reg(REG_INT_STATUS);
        if (status < 0)
        {
            pr_err("%s: max30100_read_reg() failed\n", THIS_MODULE->name);
            return status;
        }
        if (status & 0x01)
        {
            pr_info("%s status = 0x%x\n", THIS_MODULE->name, status);
            break;
        }
        msleep(100);
    }

    if (!(status & 0x01))
    {
        pr_err("MAX30100: PWR_RDY not set\n");
        return -ENODEV;
    }

    pr_info("MAX30100: chip ready, initializing...\n");
    msleep(10);

    max30100_write_reg(REG_MODE_CONFIG, 0x03); // SPO2 mode
    pr_info("Step 1 successful: SPO2 mode set.\n");
    msleep(10);

    max30100_write_reg(REG_SPO2_CONFIG, 0x5C); // 100Hz sample rate, 1600us pulse width
    pr_info("Step 2 successful: SPO2 config set.\n");
    msleep(10);

    max30100_write_reg(REG_LED_CONFIG, 0x59); // IR=24mA, Red=24mA
    pr_info("Step 3 successful: LED current set.\n");
    msleep(10);

    pr_info("MAX30100: Initialization done\n");
    return 0;
}

// File Operations
static struct file_operations max30100_fops = {
    .owner = THIS_MODULE,
    .read = max30100_read,
    .open = max30100_open,
    .release = max30100_close,
};

static int max30100_open(struct inode *inode, struct file *file)
{
    pr_info("max30100 device opened\n");
    return 0;
}

static int max30100_close(struct inode *inode, struct file *pfile)
{
    pr_info("max30100 device closed\n");
    return 0;
}

// Reads raw IR and Red data from the FIFO and sends it to user space
static ssize_t max30100_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    u8 data[4];
    int ret;
    u16 ir, red;
    char output[128];
    int len;

    // Use a combined I2C transfer to write the register address and then read the data
    struct i2c_msg msgs[2] = {
        {
            .addr = max30100_client->addr,
            .flags = 0, // Write
            .len = 1,
            .buf = (u8[]){REG_FIFO_DATA},
        },
        {
            .addr = max30100_client->addr,
            .flags = I2C_M_RD, // Read
            .len = 4,
            .buf = data,
        },
    };

    ret = i2c_transfer(max30100_client->adapter, msgs, 2);
    if (ret != 2)
    {
        pr_err("MAX30100: I2C transfer failed: %d\n", ret);
        return -EIO;
    }

    // Combine the received bytes into 16-bit values
    ir = ((u16)data[0] << 8) | data[1];
    red = ((u16)data[2] << 8) | data[3];

    // Format the raw IR and RED data into a string for user space
    len = snprintf(output, sizeof(output), "%u %u\n", ir, red);

    // Copy the raw data string to the user buffer
    if (copy_to_user(buf, output, len))
        return -EFAULT;

    return len;
}

// I2C Driver Core
static int max30100_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret;
    struct device *max30100_device;

    // Set the global client pointer for helper functions
    max30100_client = client;

    // Initialize the chip
    ret = max30100_init_chip();
    if (ret < 0)
    {
        pr_err("MAX30100 is not ready or failed to initialize.\n");
        return ret;
    }

    // Create character device
    ret = alloc_chrdev_region(&devno, 0, 1, "max30100_dev");
    if (ret < 0)
        return ret;

    max30100_class = class_create(THIS_MODULE, "max30100_class");
    if (IS_ERR(max30100_class))
    {
        unregister_chrdev_region(devno, 1);
        return PTR_ERR(max30100_class);
    }

    max30100_device = device_create(max30100_class, &client->dev, devno, NULL, "max30100");
    if (IS_ERR(max30100_device))
    {
        class_destroy(max30100_class);
        unregister_chrdev_region(devno, 1);
        return PTR_ERR(max30100_device);
    }

    // Initialize and add the cdev
    cdev_init(&max30100_cdev, &max30100_fops);
    ret = cdev_add(&max30100_cdev, devno, 1);
    if (ret < 0)
    {
        device_destroy(max30100_class, devno);
        class_destroy(max30100_class);
        unregister_chrdev_region(devno, 1);
        return ret;
    }

    pr_info("MAX30100 driver loaded and device created.\n");
    return 0;
}

static int max30100_remove(struct i2c_client *client)
{
    cdev_del(&max30100_cdev);
    device_destroy(max30100_class, devno);
    class_destroy(max30100_class);
    unregister_chrdev_region(devno, 1);
    pr_info("MAX30100 driver removed.\n");
    return 0;
}

// Matching Tables
static const struct of_device_id max30100_of_match[] = {
    {.compatible = "MSDhoni,max30100"}, // matching with device tree
    {}};
MODULE_DEVICE_TABLE(of, max30100_of_match);

static const struct i2c_device_id max30100_id[] = {
    {I2C_SLAVE_NAME, 0},
    {}};
MODULE_DEVICE_TABLE(i2c, max30100_id);

// I2C Driver Structure
static struct i2c_driver max30100_driver = {
    .driver = {
        .name = I2C_SLAVE_NAME,
        .of_match_table = of_match_ptr(max30100_of_match),
    },
    .probe = max30100_probe,
    .remove = max30100_remove,
    .id_table = max30100_id,
};

module_i2c_driver(max30100_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Varad Kalekar,Satyam Patil,Sourabh Divate,Srushti Nakate");
MODULE_DESCRIPTION("I2C Device Driver for MAX30100 with Device Tree support");