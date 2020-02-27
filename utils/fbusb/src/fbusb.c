/*
* fbusb.c
*
* Copyright (c) 2018 Qin Wei (me@vonger.cn)
*
*	This driver is designed to compatible with VoCore screen. It allows
*	quick access the framebuffer.
*
*	This program is free software; you can redistribute it and/or
*	modify it under the terms of the GNU General Public License as
*	published by the Free Software Foundation, version 2.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/fb.h>
#include <linux/mm.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/input.h>
#include <linux/usb/input.h>

#define FBUSB_WIDTH		480
#define FBUSB_HEIGHT		800
#define FBUSB_BPP		16

#define FBUSB_ID_NOT_PREPARED	0x8613
#define FBUSB_ID_PREPARED	0x1004

#define FBUSB_PALETTE_SIZE	16
#define FBUSB_SCREEN_BUFFER	58
#define FBUSB_MAX_DELAY		100

static const struct fb_fix_screeninfo fbusb_fix = {
	.id		= "fbusb",
	.type		= FB_TYPE_PACKED_PIXELS,
	.visual		= FB_VISUAL_TRUECOLOR,
	.accel		= FB_ACCEL_NONE,
};

static const struct fb_var_screeninfo fbusb_var = {
	.height		= -1,
	.width		= -1,
	.activate	= FB_ACTIVATE_NOW,
	.vmode		= FB_VMODE_NONINTERLACED,
};

struct fbusb_par {
	struct fb_info *info;
	char cmd[6];
	u32 palette[FBUSB_PALETTE_SIZE];
	int screen_size;	/* real used size, not aligned page */
};

struct fbusb_info {
	struct fb_info *info;
	struct task_struct *task;
	struct usb_interface *interface;
	struct usb_device *udev;
	u16 id_product;
	u16 backlight;
	u32 frame_count;
	u8  pause;

	/* touch screen parameters */
	struct input_dev *input;
	char phys[64];
	struct urb *irq;
	unsigned char *data;
	dma_addr_t dma;
	int data_size;
	int touch_swapxy, touch_mirror;
};

static struct fbusb_info *cur_uinfo;

#include "fbusb_mpucore.h"
#include "fbusb_common.c"

#define FBUSB_NT35510
/* such screen as djn1522 is based on this */
#ifdef FBUSB_NT35510
#include "fbusb_nt35510.c"
#endif

/* old screen vocore2 is based on this */
#ifdef FBUSB_ILI9806
#include "fbusb_ili9806.c"
#endif

static int fbusb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
				  u_int transp, struct fb_info *info)
{
	u32 *pal = info->pseudo_palette;
	u32 cr = red >> (16 - info->var.red.length);
	u32 cg = green >> (16 - info->var.green.length);
	u32 cb = blue >> (16 - info->var.blue.length);
	u32 value;

	if (regno >= FBUSB_PALETTE_SIZE)
		return -EINVAL;

	value = (cr << info->var.red.offset) |
		(cg << info->var.green.offset) |
		(cb << info->var.blue.offset);
	if (info->var.transp.length > 0) {
		u32 mask = (1 << info->var.transp.length) - 1;
		mask <<= info->var.transp.offset;
		value |= mask;
	}
	pal[regno] = value;
	return 0;
}

static int fbusb_reboot_callback(struct notifier_block *self,
				  unsigned long val, void *data)
{
	if (val == SYS_RESTART) {
		if (cur_uinfo) {
			cur_uinfo->pause = 1;
			msleep(100);
		}
	}

	return NOTIFY_DONE;
}

static struct notifier_block fbusb_reboot_notifier = {
	.notifier_call = fbusb_reboot_callback,
};

static struct fb_ops fbusb_ops = {
	.owner		= THIS_MODULE,
	.fb_read	= fbusb_read,
	.fb_write	= fbusb_write,
	.fb_setcolreg	= fbusb_setcolreg,
	.fb_fillrect	= fbusb_fillrect,
	.fb_copyarea	= fbusb_copyarea,
	.fb_imageblit	= fbusb_imageblit,
};

static int fbusb_update_frame(struct fbusb_info *uinfo)
{
	struct fb_info *info = uinfo->info;
	struct fbusb_par *par = info->par;
	struct usb_device *udev = uinfo->udev;
	int ret;

	/* 0x40 USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE */
	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0xb0, 0x40,
			0, 0, par->cmd, sizeof(par->cmd), FBUSB_MAX_DELAY);
	if (ret < 0)
		return ret;
	ret = usb_bulk_msg(udev, usb_sndbulkpipe(udev, 0x02), info->screen_buffer,
			 par->screen_size, NULL, FBUSB_MAX_DELAY);
	if (ret < 0)
		return ret;
	return 0;
}

int fbusb_install_firmware(struct fbusb_info *uinfo)
{
	struct usb_device *udev;
	u8 *buf, *ram;
	size_t size;
	int ret = 0, i;

	udev = interface_to_usbdev(uinfo->interface);
	buf = kzalloc(0x4000, GFP_KERNEL);
	ram = kzalloc(0x4000, GFP_KERNEL);
	if (buf == NULL || ram == NULL) {
		ret = -ENOMEM;
		goto fbusb_install_firmware_error;
	}

	buf[0] = 0x01;
	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0xa0, 0x40,
				  0xe600, 0x00, buf, 1, FBUSB_MAX_DELAY);
	if (ret < 0)
		goto fbusb_install_firmware_error;

	/* upload binary to 8051 RAM */
	fbusb_data_uncompress(mpucore, sizeof(mpucore), buf, size);
	for (i = 0; i < 4; i++) {
		/* maximum data size for control transfer = 4 kiB */
		ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0xa0, 0x40,
			i * 4096, 0, buf + i * 4096, 4096, FBUSB_MAX_DELAY);
		if (ret != 4096)
			goto fbusb_install_firmware_error;
	}
	/* load uploaded data back */
	for (i = 0; i < 4; i++) {
		ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0), 0xa0, 0xc0,
			i * 4096, 0, ram + i * 4096, 4096, FBUSB_MAX_DELAY);
		if (ret != 4096)
			goto fbusb_install_firmware_error;
	}
	/* verify uploaded code */
	for (i = 0; i < 0x4000; i++) {
		if (ram[i] == buf[i])
			continue;
		ret = -EIO;
		goto fbusb_install_firmware_error;
	}

	/* run 8051 */
	buf[0] = 0x00;
	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0xa0, 0x40,
				  0xe600, 0x00, buf, 1, FBUSB_MAX_DELAY);

fbusb_install_firmware_error:
	kfree(ram);
	kfree(buf);
	return ret;
}

static int fbusb_refresh_thread(void *data)
{
	struct fbusb_info *uinfo = data;
	struct fbusb_par *par = uinfo->info->par;

	/* this delay make splash could show. :) */
	ssleep(2);
	memset(uinfo->info->screen_buffer, 0, par->screen_size);

	while (!kthread_should_stop()) {
		if (!uinfo->pause) {
			if (fbusb_update_frame(uinfo) < 0)
				uinfo->pause = 1;
			if (fbusb_update_backlight(uinfo) < 0)
				uinfo->pause = 1;
		} else {
			ssleep(1);
		}

		uinfo->frame_count++;
	}

	return 0;
}

union axis{
    struct hx{
	unsigned char h:4;
	unsigned char u:2;
	unsigned char f:2;
    } x;

    struct hy{
	unsigned char h:4;
	unsigned char id:4;
    } y;

    char c;
};

struct point {
    union axis xh;
    unsigned char xl;
    union axis yh;
    unsigned char yl;

    unsigned char weight;
    unsigned char misc;
};

struct touch {
    unsigned char unused[2];
    unsigned char count;
    struct point p[2];
};

static void fbusb_touch_irq(struct urb *urb)
{
	struct fbusb_info *uinfo = urb->context;
	struct device *dev = &uinfo->interface->dev;
	struct touch *t = (struct touch *)uinfo->data;
	int ret, x, y, k;

	/* urb->status == ENOENT: closed by usb_kill_urb */
	if (urb->status)
		return;

	x = (((int)t->p[0].xh.x.h) << 8) + t->p[0].xl;
	y = (((int)t->p[0].yh.y.h) << 8) + t->p[0].yl;
	k = (t->p[0].xh.x.f != 1) ? 1 : 0;

	input_report_key(uinfo->input, BTN_TOUCH, k);
	if (uinfo->touch_swapxy) {
		input_report_abs(uinfo->input, ABS_X, y);
		input_report_abs(uinfo->input, ABS_Y, x);
	} else {
		input_report_abs(uinfo->input, ABS_X, x);
		input_report_abs(uinfo->input, ABS_Y, y);
	}
	input_sync(uinfo->input);
	dev_dbg(dev, "touchscreen x=%d, y=%d, key=%d.\n", x, y, k);

	ret = usb_submit_urb(urb, GFP_ATOMIC);
	if (ret)
		dev_err(dev, "usb_submit_urb failed at irq: %d\n", ret);
}

static int fbusb_touch_open(struct input_dev *input)
{
	struct fbusb_info *uinfo = input_get_drvdata(input);
	int ret;

	ret = usb_submit_urb(uinfo->irq, GFP_KERNEL);
	if (ret) {
		dev_err(&uinfo->interface->dev,
			"usb_submit_urb failed at open: %d.\n", ret);
		return -EIO;
	}

	return ret;
}

static void fbusb_touch_close(struct input_dev *input)
{
	struct fbusb_info *uinfo = input_get_drvdata(input);
	usb_kill_urb(uinfo->irq);
}

static ssize_t fbusb_frame_count_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct fbusb_info *uinfo = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", uinfo->frame_count);
}

static DEVICE_ATTR(frame_count, S_IRUGO, fbusb_frame_count_show, NULL);

static ssize_t fbusb_backlight_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct fbusb_info *uinfo = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", uinfo->backlight);
}

static ssize_t fbusb_backlight_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fbusb_info *uinfo = dev_get_drvdata(dev);
	u16 backlight = simple_strtoul(buf, NULL, 10);
	if (backlight > 100) {
		uinfo->backlight = 100;
	} else {
		uinfo->backlight = backlight;
	}
	return count;
}

static DEVICE_ATTR(backlight, 0660, fbusb_backlight_show, fbusb_backlight_store);

static ssize_t fbusb_pause_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct fbusb_info *uinfo = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", uinfo->pause);
}

static ssize_t fbusb_pause_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fbusb_info *uinfo = dev_get_drvdata(dev);
	if (buf[0] == '0' || buf[0] == '1')
		uinfo->pause = buf[0] - '0';
	return count;
}

static DEVICE_ATTR(pause, 0660, fbusb_pause_show, fbusb_pause_store);

static ssize_t fbusb_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", FBUSB_TYPE);
}

static ssize_t fbusb_type_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return 0;	// not imply yet.
}

static DEVICE_ATTR(type, 0660, fbusb_type_show, fbusb_type_store);

static ssize_t fbusb_touch_swapxy_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct fbusb_info *uinfo = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", uinfo->touch_swapxy);
}

static ssize_t fbusb_touch_swapxy_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fbusb_info *uinfo = dev_get_drvdata(dev);
	if (buf[0] == '0' || buf[0] == '1')
		uinfo->touch_swapxy = buf[0] - '0';
	return count;
}

static DEVICE_ATTR(touch_swapxy, 0660, fbusb_touch_swapxy_show, fbusb_touch_swapxy_store);

static int fbusb_probe(struct usb_interface *interface,
			   const struct usb_device_id *id)
{
	struct fbusb_info *uinfo = NULL;
	struct fb_info *info = NULL;
	struct fbusb_par *par = NULL;
	u8 *vmem = NULL;
	int vmem_size, ret;

	uinfo = kzalloc(sizeof(struct fbusb_info), GFP_KERNEL);
	if (uinfo == NULL) {
		ret = -ENOMEM;
		goto error_fb_release;
	}
	uinfo->id_product = id->idProduct;
	uinfo->interface = interface;
	usb_set_intfdata(interface, uinfo);

	if (uinfo->id_product == FBUSB_ID_NOT_PREPARED) {
		ret = fbusb_install_firmware(uinfo);
		if (ret < 0) {
			dev_err(&interface->dev, "can not attach device, %d.\n", ret);
			goto error_fb_release;
		}
		dev_info(&interface->dev, "looking for new device.\n");
		return 0;
	} else if (uinfo->id_product == FBUSB_ID_PREPARED) {
		ret = fbusb_boot_setup(uinfo);
		if (ret < 0) {
			dev_err(&interface->dev, "can not setup device, %d\n", ret);
			goto error_fb_release;
		}
		dev_info(&interface->dev, "device detected.\n");
	} else {
		ret = -EINVAL;
		dev_info(&interface->dev, "device is not supported.\n");
		goto error_fb_release;
	}

	info = framebuffer_alloc(sizeof(struct fbusb_par), &interface->dev);
	if (!info) {
		dev_info(&interface->dev, "framebuffer alloc error.\n");
		ret = -ENOMEM;
		goto error_fb_release;
	}
	uinfo->info = info;
	uinfo->backlight = 100;		/* max backlight default */
	uinfo->udev = usb_get_dev(interface_to_usbdev(interface));

	par = info->par;
	par->info = info;
	par->screen_size = FBUSB_WIDTH * FBUSB_HEIGHT * FBUSB_BPP / 8;

	/* setup write command for frame, 768000 bytes. */
	par->cmd[1] = 0x2c;
	par->cmd[3] = 0xb8;
	par->cmd[4] = 0x0b;

	/* alloc and align buffer to page */
	vmem_size = PAGE_ALIGN(par->screen_size);
	vmem = kmalloc(vmem_size, GFP_KERNEL);
	if (!vmem) {
		ret = -ENOMEM;
		goto error_fb_release;
	}

	info->fbops = &fbusb_ops;
	info->flags = FBINFO_DEFAULT | FBINFO_VIRTFB;
	info->screen_buffer = vmem;
	info->pseudo_palette = par->palette;

	info->fix = fbusb_fix;
	info->fix.smem_start = virt_to_phys(vmem);
	info->fix.smem_len = vmem_size;
	info->fix.line_length = FBUSB_WIDTH * FBUSB_BPP / 8;

	info->var = fbusb_var;
	info->var.xres = FBUSB_WIDTH;
	info->var.yres = FBUSB_HEIGHT;
	info->var.xres_virtual = info->var.xres;
	info->var.yres_virtual = info->var.yres;
	info->var.bits_per_pixel = FBUSB_BPP;
	info->var.red.offset = 11;
	info->var.red.length = 5;
	info->var.green.offset = 5;
	info->var.green.length = 6;
	info->var.blue.offset = 0;
	info->var.blue.length = 5;
	info->var.transp.offset = 0;
	info->var.transp.length = 0;

	ret = register_framebuffer(info);
	if (ret < 0) {
		dev_err(&interface->dev, "unable to register, %d\n", ret);
		goto error_fb_release;
	}

	/* start a thread to upload framebuffer to screen */
	fbusb_load_logo(vmem);
	fbusb_update_frame(uinfo);

	uinfo->task = kthread_run(fbusb_refresh_thread, uinfo, "fbusb");
	if (IS_ERR(uinfo->task)) {
		dev_err(&interface->dev, "create thread failed.\n");
		goto error_fb_release;
	}

	device_create_file(&interface->dev, &dev_attr_pause);
	device_create_file(&interface->dev, &dev_attr_frame_count);
	device_create_file(&interface->dev, &dev_attr_backlight);
	device_create_file(&interface->dev, &dev_attr_type);
	device_create_file(&interface->dev, &dev_attr_touch_swapxy);

	register_reboot_notifier(&fbusb_reboot_notifier);
	cur_uinfo = uinfo;

	/* setup usb interrupt device */
	uinfo->irq = usb_alloc_urb(0, GFP_KERNEL);
	if (!uinfo->irq) {
		dev_err(&interface->dev, "unable to alloc usb irq.\n");
		goto error_fb_release;
	}
	uinfo->data_size = 14;	/* touch infomation data length */
	uinfo->data = usb_alloc_coherent(uinfo->udev, uinfo->data_size,
					 GFP_KERNEL, &uinfo->dma);
	usb_fill_int_urb(uinfo->irq, uinfo->udev, usb_rcvintpipe(uinfo->udev, 1),
		uinfo->data, uinfo->data_size, fbusb_touch_irq, uinfo, 0);
	uinfo->irq->dev = uinfo->udev;
	uinfo->irq->transfer_dma = uinfo->dma;
	uinfo->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	/* register input device for touch */
	usb_make_path(uinfo->udev, uinfo->phys, sizeof(uinfo->phys));
	strlcat(uinfo->phys, "/input0", sizeof(uinfo->phys));

	uinfo->input = input_allocate_device();
	usb_to_input_id(uinfo->udev, &uinfo->input->id);
	uinfo->input->name = "VoCore Touchscreen";
	uinfo->input->phys = uinfo->phys;
	uinfo->input->dev.parent = &interface->dev;

	input_set_capability(uinfo->input, EV_KEY, BTN_TOUCH);
	input_set_abs_params(uinfo->input, ABS_X, 0, info->var.xres, 0, 0);
	input_set_abs_params(uinfo->input, ABS_Y, 0, info->var.yres, 0, 0);

	uinfo->input->open = fbusb_touch_open;
	uinfo->input->close = fbusb_touch_close;
	input_set_drvdata(uinfo->input, uinfo);

	ret = input_register_device(uinfo->input);
	if (ret) {
		dev_err(&interface->dev, "unable to regist touchscreen device.\n");
		goto error_free_urb;
	}

	dev_info(&interface->dev, "fb%d: mode=%dx%dx%d.\n", info->node,
		 info->var.xres, info->var.yres, info->var.bits_per_pixel);

	return 0;

error_free_urb:
	usb_free_urb(uinfo->irq);
	usb_free_coherent(uinfo->udev, uinfo->data_size, uinfo->data, uinfo->dma);

error_fb_release:
	framebuffer_release(info);
	kfree(vmem);
	kfree(uinfo);
	return ret;
}

static void fbusb_disconnect(struct usb_interface *interface)
{
	struct fbusb_info *uinfo = usb_get_intfdata(interface);
	struct fb_info *info = uinfo->info;

	if (uinfo->id_product == FBUSB_ID_NOT_PREPARED) {
		kfree(uinfo);
		return;
	}

	if (uinfo->task)
		kthread_stop(uinfo->task);

	input_unregister_device(uinfo->input);
	usb_free_urb(uinfo->irq);
	usb_free_coherent(uinfo->udev, uinfo->data_size, uinfo->data, uinfo->dma);

	unregister_framebuffer(info);
	kfree(info->screen_buffer);
	framebuffer_release(info);
	kfree(uinfo);

	device_remove_file(&interface->dev, &dev_attr_pause);
	device_remove_file(&interface->dev, &dev_attr_frame_count);
	device_remove_file(&interface->dev, &dev_attr_backlight);
	device_remove_file(&interface->dev, &dev_attr_type);
	device_remove_file(&interface->dev, &dev_attr_touch_swapxy);

	unregister_reboot_notifier(&fbusb_reboot_notifier);

	dev_info(&interface->dev, "device now disconnected.\n");
}

static const struct usb_device_id fbusb_ids[] = {
	{ USB_DEVICE(0x04b4, FBUSB_ID_PREPARED) },
	{ USB_DEVICE(0x04b4, FBUSB_ID_NOT_PREPARED) },
	{ }
};
MODULE_DEVICE_TABLE(usb, fbusb_ids);

static struct usb_driver fbusb_driver = {
	.name = "fbusb",
	.probe = fbusb_probe,
	.disconnect = fbusb_disconnect,
	.id_table = fbusb_ids,
};

module_usb_driver(fbusb_driver);

MODULE_AUTHOR("Qin Wei (me@vonger.cn)");
MODULE_DESCRIPTION("VoCore USB framebuffer driver");
MODULE_LICENSE("GPL");

