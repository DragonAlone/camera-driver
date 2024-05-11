#include "asm-generic/errno-base.h"
#include "linux/kernel.h"
#include "linux/media-bus-format.h"
#include "linux/v4l2-mediabus.h"
#include "media/v4l2-async.h"
#include "media/v4l2-common.h"
#include "ov2640_reg.h"
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <asm-generic/gpio.h>
#include <media/v4l2-subdev.h>
#include <linux/delay.h>
#include <stdarg.h>
#include "ov2640_reg.h"

#define PIDH 0x0A
#define PIDL 0x0B

#define I2C_DRIVER_NAME "ov2640-camera"

MODULE_LICENSE("GPL");

struct camera_device
{
    int width;
    int height;
    u32 code;
    int rst_gpio;
    struct v4l2_subdev subdev;
};

static void camera_register_write(const struct i2c_client* client, const struct regval_list* vals)
{
    int err;
    while((vals->reg_num != 0xff) || (vals->value != 0xff))
    {
        err = i2c_smbus_write_byte_data(client,vals->reg_num,vals->value);
        if(err){
            printk("fail to i2c_smbus_write_byte_data,reg:%#x,val:%#x",vals->reg_num,vals->value);
            return;
        }
        vals++;
    }
    return;
}

static int camera_mbus_fmt(struct v4l2_subdev* sd,unsigned index,u32* code)
{
    u32 ov2640_code[] = {
        MEDIA_BUS_FMT_YUYV8_2X8,
        MEDIA_BUS_FMT_UYVY8_2X8,
    };
    if(index >= ARRAY_SIZE(ov2640_code)){
        return -EINVAL;
    }

    *code = ov2640_code[index];

    return 0;
}

const struct ov2640_win_size* camera_size_match(int width,int height)
{
    int i = 0;
    int vga = 3;
    for(i=0;i < ARRAY_SIZE(ov2640_supported_win_sizes);i++){
        if(ov2640_supported_win_sizes[i].width >= width &&
           ov2640_supported_win_sizes[i].height >= height){
            return &ov2640_supported_win_sizes[i];
        }
    }
    return &ov2640_supported_win_sizes[vga];
}

static int camera_s_mbus_fmt(struct v4l2_subdev* sd,struct v4l2_mbus_framefmt* fmt)
{
    const struct regval_list* fmt_reg;
    struct i2c_client* client = v4l2_get_subdevdata(sd);
    const struct ov2640_win_size* win = camera_size_match(fmt->width,fmt->height);
    struct camera_device *ovdev = container_of(sd,struct camera_device, subdev);

    switch(fmt->code){
        case MEDIA_BUS_FMT_YUYV8_2X8:           fmt_reg = ov2640_yuyv_regs;break;
        case MEDIA_BUS_FMT_UYVY8_2X8:           fmt_reg = ov2640_uyvy_regs;break;
        default:                                fmt_reg = ov2640_yuyv_regs;fmt->code = MEDIA_BUS_FMT_YUYV8_2X8;break;
    }
    camera_register_write(client,ov2640_init_regs);
    camera_register_write(client,ov2640_size_change_preamble_regs);
    camera_register_write(client,win->regs);

    camera_register_write(client,ov2640_format_change_preamble_regs);
    camera_register_write(client,fmt_reg);
    camera_register_write(client,ov2640_light_mode_sunny_regs);

    fmt->width = win->width;
    fmt->height = win->height;

    ovdev->code = fmt->code;
    ovdev->width = win->width;
    ovdev->height = win->height;
    return 0;
}

static int camera_s_power(struct v4l2_subdev *sd, int on)
{
    if (on){
        printk("ov2640 power on\n");
    }else{
        printk("ov2640 power off\n");
    }
    
    return 0;
}

static int camera_g_mbus_fmt(struct v4l2_subdev* sd,struct v4l2_mbus_framefmt* mf)
{
    struct camera_device *ovdev = container_of(sd,struct camera_device, subdev);

    mf->code = ovdev->code;
    mf->height = ovdev->height;
    mf->width = ovdev->width;

    return 0;
}

static const struct v4l2_subdev_video_ops ov2640_video_ops = {
    .enum_mbus_fmt = camera_mbus_fmt,
    .s_mbus_fmt = camera_s_mbus_fmt,
    .g_mbus_fmt = camera_g_mbus_fmt,
};

static const struct v4l2_subdev_core_ops ov2640_core_ops = {
    .s_power = camera_s_power,
};

static const struct v4l2_subdev_ops ov2640_ops = {
    .core = &ov2640_core_ops,
    .video = &ov2640_video_ops,
};

static int i2c_camera_probe(struct i2c_client *client,
                 const struct i2c_device_id *id)
{
    printk("i2c ov2640 probe\n");
    int err;
    struct camera_device *ovdev;
    struct device_node *np = client->dev.of_node;

    ovdev = devm_kzalloc(&client->dev,sizeof(*ovdev), GFP_KERNEL);
    if(!ovdev){
        printk("fail to devm_kmalloc\n");
        return -ENOMEM;
    }

    ovdev->rst_gpio = of_get_named_gpio(np,"rst-gpios", 0);
    if(!gpio_is_valid(ovdev->rst_gpio)){
        printk("Invalid gpio:%d\n",ovdev->rst_gpio);
        return -ENODEV;
    }
    err = devm_gpio_request_one(&client->dev,ovdev->rst_gpio,
                                 GPIOF_OUT_INIT_HIGH,"ov2640_reset");
    if (err){
        printk("Fail to devm_gpio_request_one\n");
        return err;
    }
    printk("reset gpio:%d\n",ovdev->rst_gpio);

    v4l2_i2c_subdev_init(&ovdev->subdev, client,&ov2640_ops);

    err = v4l2_async_register_subdev(&ovdev->subdev);
    if(err){
        printk("fail to v4l2_async_register_subdev");
        return err;
    }

    return 0;
}

static int i2c_camera_remove(struct i2c_client *client)
{
    struct v4l2_subdev* subdev = i2c_get_clientdata(client);
    v4l2_async_unregister_subdev(subdev);
    return 0;
}

static const struct i2c_device_id camera_id[] = {
    {I2C_DRIVER_NAME, 0},
    {}
};
MODULE_DEVICE_TABLE(i2c, camera_id);

#ifdef CONFIG_OF
static const struct of_device_id camera_of_match[] = {
    {.compatible = "ovti,ov2640"},
    {},
};
MODULE_DEVICE_TABLE(of, camera_of_match);
#endif

static struct i2c_driver camera_driver = {
    .id_table = camera_id,
    .probe = i2c_camera_probe,
    .remove = i2c_camera_remove,
    .driver = {
        .name = I2C_DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(camera_of_match),
    },
};

module_i2c_driver(camera_driver);
