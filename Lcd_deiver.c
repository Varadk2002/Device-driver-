#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kernel.h>

#define I2C_BUS_AVAILABLE   2              // I2C Bus available in BBB
#define SLAVE_DEVICE_NAME   "HD44780"      // Device and Driver Name
#define LCD_SLAVE_ADDR   (0x4E >> 1)       // lcd Slave Address
// 0x4E >> 1 = 01001110 >> 1 = 00100111 = 0x27

#define LCD_CLEAR		0x01
#define LCD_FN_SET_8BIT	0x30
#define LCD_FN_SET_4BIT	0x20
#define LCD_FN_SET_4BIT_2LINES	0x28
#define LCD_DISP_CTRL	0x08
#define LCD_DISP_ON		0x0C
#define LCD_ENTRY_MODE	0x06
#define LCD_LINE1		0x80
#define LCD_LINE2		0xC0

#define LCD_RS	0
#define LCD_RW	1
#define LCD_EN	2
#define LCD_BL	3

#define LCD_CMD		0x80
#define LCD_DATA	1

#define BV(n)       (1 << (n))
#define __NOP()     asm("nop")
typedef unsigned char uint8_t;

static struct i2c_adapter *desd_i2c_adapter    = NULL;  // I2C Adapter Structure
static struct i2c_client  *desd_i2c_client_lcd = NULL;  // I2C Cient Structure (In our case it is lcd)

int LcdWriteByte(uint8_t val) {
	return i2c_master_send(desd_i2c_client_lcd, &val, 1);
}

void LcdWrite(uint8_t rs, uint8_t val) {
	uint8_t high = val & 0xF0, low = (val << 4) & 0xF0;
	uint8_t bvrs = (rs == LCD_CMD) ? 0 : BV(LCD_RS);
	LcdWriteByte(high | bvrs | BV(LCD_EN) | BV(LCD_BL));
	mdelay(1);
	LcdWriteByte(high | bvrs | BV(LCD_BL));

	LcdWriteByte(low | bvrs | BV(LCD_EN) | BV(LCD_BL));
	mdelay(1);
	LcdWriteByte(low | bvrs | BV(LCD_BL));
}

// As per 4-bit initialization sequence mentioned HD44780 datasheet fig 24 (page 46)
int LcdInit(void) {
	int ret;
    // wait for min 15 ms (for 5V)
	mdelay(20);

	// attention sequence
	ret = LcdWriteByte(LCD_FN_SET_8BIT | BV(LCD_EN));
	// check if lcd is ready
    if(ret != 0)
        return -1;
	__NOP();
	LcdWriteByte(LCD_FN_SET_8BIT);
	mdelay(5);

	LcdWriteByte(LCD_FN_SET_8BIT | BV(LCD_EN));
	__NOP();
	LcdWriteByte(LCD_FN_SET_8BIT);
	mdelay(1);

	LcdWriteByte(LCD_FN_SET_8BIT | BV(LCD_EN));
	__NOP();
	LcdWriteByte(LCD_FN_SET_8BIT);
	mdelay(3);

	LcdWriteByte(LCD_FN_SET_4BIT | BV(LCD_EN));
	__NOP();
	LcdWriteByte(LCD_FN_SET_4BIT);
	mdelay(3);

	// lcd initialization
	LcdWriteByte(LCD_FN_SET_4BIT_2LINES);
	mdelay(1);
	LcdWrite(LCD_CMD, LCD_DISP_CTRL);
	mdelay(1);
	LcdWrite(LCD_CMD, LCD_CLEAR);
	mdelay(1);
	LcdWrite(LCD_CMD, LCD_ENTRY_MODE);
	mdelay(1);
	LcdWrite(LCD_CMD, LCD_DISP_ON);
	mdelay(1);
	return ret;
}

// call this functiond from device write operation.
void LcdPuts(uint8_t line, char str[]) {
	int i;
	LcdWrite(LCD_CMD, line); // line address
	mdelay(1);
	for(i=0; str[i]!='\0'; i++)
		LcdWrite(LCD_DATA, str[i]);
}

// lcd_open()
// lcd_close()
// lcd_read() -- do nothing
// lcd_write()
    // 1. copy data from user buf to kernel buf
    // 2. send data to lcd device from kernel buf -- LcdPuts()

static int desd_lcd_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    int ret;
    pr_info("lcd Probed!!!\n");
	ret = LcdInit();
    if(ret != 0) {
        pr_info("LCD not ready/available.\n");
        return ret;
    }
    pr_info("LCD is initialized.\n");
    // allocate device number
    // create device class and device file
    // init cdev (with fops) and add it in kernel
    return ret;
}

static int desd_lcd_remove(struct i2c_client *client) {
    pr_info("lcd Removed!!!\n");
    // delete cdev from kernel
    // destroy device file and device class
    // release device number
    return 0;
}

static const struct i2c_device_id desd_lcd_id[] = {
        { SLAVE_DEVICE_NAME, 0 },
        { }
};
MODULE_DEVICE_TABLE(i2c, desd_lcd_id);

static struct i2c_driver desd_lcd_driver = {
        .driver = {
            .name   = SLAVE_DEVICE_NAME,
            .owner  = THIS_MODULE,
        },
        .probe          = desd_lcd_probe,
        .remove         = desd_lcd_remove,
        .id_table       = desd_lcd_id,
};

static struct i2c_board_info lcd_i2c_board_info = {
    I2C_BOARD_INFO(SLAVE_DEVICE_NAME, LCD_SLAVE_ADDR)
};

static int __init desd_driver_init(void) {
    int ret = -1;
    desd_i2c_adapter = i2c_get_adapter(I2C_BUS_AVAILABLE);
    if(desd_i2c_adapter != NULL) {
        //desd_i2c_client_lcd = i2c_new_device(desd_i2c_adapter, &lcd_i2c_board_info);
        desd_i2c_client_lcd = i2c_new_client_device(desd_i2c_adapter, &lcd_i2c_board_info);
        if(desd_i2c_client_lcd != NULL) {
            i2c_add_driver(&desd_lcd_driver);
            pr_info("Driver Added!!!\n");
            ret = 0;
        }
		else
            pr_info("lcd client not found!!!\n");
        
        i2c_put_adapter(desd_i2c_adapter);
    }
	else
        pr_info("I2C Bus Adapter Not Available!!!\n");
    return ret;
}

static void __exit desd_driver_exit(void) {
	if(desd_i2c_client_lcd != NULL) {
        i2c_unregister_device(desd_i2c_client_lcd);
		i2c_del_driver(&desd_lcd_driver);
	}
    pr_info("Driver Removed!!!\n");
}

module_init(desd_driver_init);
module_exit(desd_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Varad Kalekar ");
MODULE_DESCRIPTION("Simple I2C driver(lcd)");
