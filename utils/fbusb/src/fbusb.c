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

#define FBUSB_BPP		16

#define FBUSB_PALETTE_SIZE	16
#define FBUSB_SCREEN_BUFFER	58
#define FBUSB_MAX_DELAY		100
#define FBUSB_PAUSE_INFINIT -1

static const struct fb_fix_screeninfo fbusb_fix = {
	.id = "fbusb",
	.type = FB_TYPE_PACKED_PIXELS,
	.visual = FB_VISUAL_TRUECOLOR,
	.accel = FB_ACCEL_NONE,
};

static const struct fb_var_screeninfo fbusb_var = {
	.height = -1,
	.width = -1,
	.activate = FB_ACTIVATE_NOW,
	.vmode = FB_VMODE_NONINTERLACED,
};

struct fbusb_par {
	struct fb_info *info;
	char cmd[6];		/* store write frame command */
	u32 palette[FBUSB_PALETTE_SIZE];
	int screen_size;	/* real used size, not aligned page */
};

struct fbusb_info {
	struct fb_info *info;
	struct task_struct *task;
	struct usb_interface *interface;
	struct usb_device *udev;
	u16 command;
	u32 frame_count;
	long pause;

	/* touch screen parameters */
	struct input_dev *input;
	char name[64];
	char phys[64];
	struct urb *irq;
	unsigned char *data;
	dma_addr_t dma;
	int data_size;

	/* screen info */
	u32 screen_info;
	u32 version_info;
	int width;
	int height;
	int margin;

	/* local command to control the screen */
	char cmd[64];
	int cmd_len;
};

static struct fbusb_info *cur_uinfo;

static int fbusb_reboot_callback(struct notifier_block *self,
				 unsigned long val, void *data)
{
	if (val == SYS_RESTART) {
		if (cur_uinfo) {
			cur_uinfo->pause = FBUSB_PAUSE_INFINIT;
			msleep(100);
		}
	}

	return NOTIFY_DONE;
}

static struct notifier_block fbusb_reboot_notifier = {
	.notifier_call = fbusb_reboot_callback,
};

static struct fb_ops fbusb_ops = {
	.owner = THIS_MODULE,
	.fb_read = fb_sys_read,
	.fb_write = fb_sys_write,
//	.fb_setcolreg = cfb_setcolreg,
	.fb_fillrect = sys_fillrect,
	.fb_copyarea = sys_copyarea,
	.fb_imageblit = sys_imageblit,
};

static int fbusb_update_frame(struct fbusb_info *uinfo)
{
	struct fb_info *info = uinfo->info;
	struct fbusb_par *par = info->par;
	struct usb_device *udev = uinfo->udev;
	int ret;

	/* 0x40 USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE */
	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0xb0, 0x40,
			      0, 0, par->cmd, sizeof(par->cmd),
			      FBUSB_MAX_DELAY);
	if (ret < 0)
		return ret;

	ret =
	    usb_bulk_msg(udev, usb_sndbulkpipe(udev, 0x02), info->screen_buffer,
			 par->screen_size, NULL, FBUSB_MAX_DELAY);
	if (ret < 0)
		return ret;

	return 0;
}

static int fbusb_send_command(struct fbusb_info *uinfo)
{
	int ret;

	if (uinfo->cmd_len == 0)
		return 0;

	ret = usb_control_msg(uinfo->udev, usb_sndctrlpipe(uinfo->udev, 0),
			      0xb0, 0x40, 0, 0, uinfo->cmd, uinfo->cmd_len,
			      FBUSB_MAX_DELAY);
	uinfo->cmd_len = 0;

	return ret;
}

static int fbusb_refresh_thread(void *data)
{
	struct fbusb_info *uinfo = data;
	struct fbusb_par *par = uinfo->info->par;

	memset(uinfo->info->screen_buffer, 0, par->screen_size);

	// send sleep out to screen, quit sleep mode.
	uinfo->cmd[0] = 0x00;
	uinfo->cmd[1] = 0x29;
	uinfo->cmd[2] = 0x00;
	uinfo->cmd[3] = 0x00;
	uinfo->cmd_len = 4;
	fbusb_send_command(uinfo);

	while (!kthread_should_stop()) {
		// pause == 0 means not pause.
		// pause < 0, it will stop send any frame.
		// pause > 0, sleep milliseconds and send frame.
		if (uinfo->pause >= 0) {
			msleep(uinfo->pause);
                        if (fbusb_update_frame(uinfo) < 0)
                                uinfo->pause = FBUSB_PAUSE_INFINIT;
                        if (fbusb_send_command(uinfo) < 0)
                                uinfo->pause = FBUSB_PAUSE_INFINIT;
		} else {
			ssleep(1);
		}
		uinfo->frame_count++;
	}

	return 0;
}

union axis {
	struct hx {
		unsigned char h:4;
		unsigned char u:2;
		unsigned char f:2;
	} x;

	struct hy {
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
	int ret, x, y, touch;

	/* urb->status == ENOENT: closed by usb_kill_urb */
	if (urb->status)
		return;

	x = (((int)t->p[0].xh.x.h) << 8) + t->p[0].xl;
	y = (((int)t->p[0].yh.y.h) << 8) + t->p[0].yl;
	touch = (t->p[0].xh.x.f != 1) ? 1 : 0;

	input_report_key(uinfo->input, BTN_TOUCH, touch);
	input_report_abs(uinfo->input, ABS_X, x);
	input_report_abs(uinfo->input, ABS_Y, y);
	input_sync(uinfo->input);
	dev_dbg(dev, "touch x=%d, y=%d, key=%d.\n", x, y, touch);

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

	return 0;
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

static ssize_t fbusb_command_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct fbusb_info *uinfo = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", uinfo->cmd_len);
}

static ssize_t fbusb_command_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct fbusb_info *uinfo = dev_get_drvdata(dev);
	int used = sizeof(uinfo->cmd);

	if (count < used)
		used = count;

	memcpy(uinfo->cmd, buf, used);
	uinfo->cmd_len = used;
	return used;
}

static DEVICE_ATTR(command, 0660, fbusb_command_show, fbusb_command_store);

static ssize_t fbusb_pause_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fbusb_info *uinfo = dev_get_drvdata(dev);
	return sprintf(buf, "%ld\n", uinfo->pause);
}

static ssize_t fbusb_pause_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct fbusb_info *uinfo = dev_get_drvdata(dev);
	int result = kstrtol(buf, 10, &uinfo->pause);
	return result == 0 ? count : result;
}

static DEVICE_ATTR(pause, 0660, fbusb_pause_show, fbusb_pause_store);

static int fbusb_get_info(struct fbusb_info *uinfo)
{
	int ret;

	uinfo->cmd[0] = 0x51;
	uinfo->cmd[1] = 0x02;
	uinfo->cmd[2] = 0x04;
	uinfo->cmd[3] = 0x1f;
	uinfo->cmd[4] = 0xfc;

	uinfo->cmd_len = 5;

	ret = usb_control_msg(uinfo->udev, usb_sndctrlpipe(uinfo->udev, 0),
			      0xb5, 0x40, 0, 0, uinfo->cmd, uinfo->cmd_len,
			      FBUSB_MAX_DELAY);
	if (ret < uinfo->cmd_len)
		return -1;

	uinfo->cmd_len = 1;
	ret = usb_control_msg(uinfo->udev, usb_rcvctrlpipe(uinfo->udev, 0),
			      0xb6, 0xc0, 0, 0, uinfo->cmd, uinfo->cmd_len,
			      FBUSB_MAX_DELAY);
	if (ret < uinfo->cmd_len)
		return -1;

	uinfo->cmd_len = 5;
	ret = usb_control_msg(uinfo->udev, usb_rcvctrlpipe(uinfo->udev, 0),
			      0xb7, 0xc0, 0, 0, uinfo->cmd, uinfo->cmd_len,
			      FBUSB_MAX_DELAY);
	if (ret < uinfo->cmd_len)
		return -1;

	uinfo->cmd_len = 0;
	return 1;
}

static int fbusb_get_version(struct fbusb_info *uinfo)
{
	int ret;

	uinfo->cmd[0] = 0x51;
	uinfo->cmd[1] = 0x02;
	uinfo->cmd[2] = 0x04;
	uinfo->cmd[3] = 0x1f;
	uinfo->cmd[4] = 0xf8;

	uinfo->cmd_len = 5;

	ret = usb_control_msg(uinfo->udev, usb_sndctrlpipe(uinfo->udev, 0),
			      0xb5, 0x40, 0, 0, uinfo->cmd, uinfo->cmd_len,
			      FBUSB_MAX_DELAY);
	if (ret < uinfo->cmd_len)
		return -1;

	uinfo->cmd_len = 1;
	ret = usb_control_msg(uinfo->udev, usb_rcvctrlpipe(uinfo->udev, 0),
			      0xb6, 0xc0, 0, 0, uinfo->cmd, uinfo->cmd_len,
			      FBUSB_MAX_DELAY);
	if (ret < uinfo->cmd_len)
		return -1;

	uinfo->cmd_len = 5;
	ret = usb_control_msg(uinfo->udev, usb_rcvctrlpipe(uinfo->udev, 0),
			      0xb7, 0xc0, 0, 0, uinfo->cmd, uinfo->cmd_len,
			      FBUSB_MAX_DELAY);
	if (ret < uinfo->cmd_len)
		return -1;

	uinfo->cmd_len = 0;
	return 1;
}

static int fbusb_probe(struct usb_interface *interface,
		       const struct usb_device_id *id)
{
	struct fbusb_info *uinfo = NULL;
	struct fb_info *info = NULL;
	struct fbusb_par *par = NULL;
	u8 *vmem = NULL;
	int vmem_size, ret;

	char *model_string = NULL;

	uinfo = kzalloc(sizeof(struct fbusb_info), GFP_KERNEL);
	if (uinfo == NULL) {
		ret = -ENOMEM;
		goto error_fb_release;
	}
	uinfo->interface = interface;
	usb_set_intfdata(interface, uinfo);

	info = framebuffer_alloc(sizeof(struct fbusb_par), &interface->dev);
	if (!info) {
		dev_info(&interface->dev, "framebuffer alloc error.\n");
		ret = -ENOMEM;
		goto error_fb_release;
	}
	uinfo->info = info;
	uinfo->command = 100;	/* max command default */
	uinfo->udev = usb_get_dev(interface_to_usbdev(interface));

	par = info->par;
	par->info = info;
	if (fbusb_get_info(uinfo) < 0)
		dev_err(&interface->dev, "can't get screen info!\n");
	uinfo->screen_info = ((int *)(uinfo->cmd + 1))[0];
	if (fbusb_get_version(uinfo) < 0)
		dev_err(&interface->dev, "can't get screen version!\n");
	uinfo->version_info = ((int *)(uinfo->cmd + 1))[0];

	dev_info(&interface->dev, "screen info code:     %08x\n",
		 uinfo->screen_info);
	dev_info(&interface->dev, "screen version code:  %08x\n",
		 uinfo->version_info);

	uinfo->width = 480;
	uinfo->height = 800;
	uinfo->margin = 0;

	if (uinfo->screen_info == 0x00000005) {
		uinfo->height = 854;
		uinfo->margin = 320;
		if (uinfo->version_info == 0xffffffff)
			model_string = "5inch, 480x854x16/24, SLM5.0-81FPC-A";
		else if (uinfo->version_info == 0x00000000)
			model_string = "5inch, 480x854x16/24, D500FPC9373-C";
	} else if (uinfo->screen_info == 0x00000304) {
		model_string = "4.3inch, 480x800x16/24, D430FPC9316-A";
	} else if (uinfo->screen_info == 0x00000004) {
		model_string = "4inch, 480x800x16, TOSHIBA-2122";
	} else if (uinfo->screen_info == 0x00000104) {
		if (uinfo->version_info == 0xffffffff)
			model_string = "4inch, 480x800x16/24, DJN-1922";
		else if (uinfo->version_info == 0x00000000)
			model_string = "4inch, 480x800x16/24, D397FPC9367-B";
		else if (uinfo->version_info == 0x00000001)
			model_string = "4inch, 480x800x16/24, SLM4.0-33FPC-A";
	} else if (uinfo->screen_info == 0x00000007) {
		uinfo->width = 800;
		uinfo->height = 480;
		model_string = "6.8inch, 800x480x16/24, D680FPC930G-A\n";
	} else {
		model_string = "4inch/5inch/6inch, 800x480x16/24, UNKNOWN\n";
	}
	dev_info(&interface->dev, "screen model: %s\n", model_string);

	par->screen_size = uinfo->height * uinfo->width * FBUSB_BPP / 8
	    + uinfo->margin;

	/* setup write command for frame */
	par->cmd[1] = 0x2c;
	par->cmd[2] = par->screen_size >> 0;
	par->cmd[3] = par->screen_size >> 8;
	par->cmd[4] = par->screen_size >> 16;
	par->cmd[5] = 0x00;

	/* alloc and align buffer to page */
	vmem_size = PAGE_ALIGN(par->screen_size);
	vmem = kmalloc(vmem_size, GFP_KERNEL);
	if (!vmem) {
		ret = -ENOMEM;
		goto error_fb_release;
	}

	info->fbops = &fbusb_ops;
	info->flags = FBINFO_VIRTFB;
	info->screen_buffer = vmem;
	info->pseudo_palette = par->palette;

	info->fix = fbusb_fix;
	info->fix.smem_start = virt_to_phys(vmem);
	info->fix.smem_len = vmem_size;
	info->fix.line_length = uinfo->width * FBUSB_BPP / 8;

	info->var = fbusb_var;
	info->var.xres = uinfo->width;
	info->var.yres = uinfo->height;
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

	uinfo->task = kthread_run(fbusb_refresh_thread, uinfo, "fbusb");
	if (IS_ERR(uinfo->task)) {
		dev_err(&interface->dev, "create thread failed.\n");
		goto error_fb_release;
	}

	device_create_file(&interface->dev, &dev_attr_pause);
	device_create_file(&interface->dev, &dev_attr_frame_count);
	device_create_file(&interface->dev, &dev_attr_command);

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
	usb_fill_int_urb(uinfo->irq, uinfo->udev,
			 usb_rcvintpipe(uinfo->udev, 1), uinfo->data,
			 uinfo->data_size, fbusb_touch_irq, uinfo, 0);
	uinfo->irq->dev = uinfo->udev;
	uinfo->irq->transfer_dma = uinfo->dma;
	uinfo->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	/* register input device for touch */
	usb_make_path(uinfo->udev, uinfo->phys, sizeof(uinfo->phys));
	strlcat(uinfo->phys, "/input0", sizeof(uinfo->phys));
	snprintf(uinfo->name, sizeof(uinfo->name), "VoCore Touch");

	uinfo->input = input_allocate_device();
	usb_to_input_id(uinfo->udev, &uinfo->input->id);
	uinfo->input->name = uinfo->name;
	uinfo->input->phys = uinfo->phys;
	uinfo->input->dev.parent = &interface->dev;

	uinfo->input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	uinfo->input->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	input_set_abs_params(uinfo->input, ABS_X, 0, info->var.xres, 0, 0);
	input_set_abs_params(uinfo->input, ABS_Y, 0, info->var.yres, 0, 0);

	uinfo->input->open = fbusb_touch_open;
	uinfo->input->close = fbusb_touch_close;
	input_set_drvdata(uinfo->input, uinfo);

	ret = input_register_device(uinfo->input);
	if (ret) {
		dev_err(&interface->dev, "unable to regist touch device.\n");
		goto error_free_urb;
	}

	dev_info(&interface->dev, "fb%d: mode=%dx%dx%d.\n", info->node,
		 info->var.xres, info->var.yres, info->var.bits_per_pixel);

	return 0;

 error_free_urb:
	usb_free_urb(uinfo->irq);
	usb_free_coherent(uinfo->udev, uinfo->data_size, uinfo->data,
			  uinfo->dma);

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

	if (uinfo->task)
		kthread_stop(uinfo->task);

	input_unregister_device(uinfo->input);
	usb_free_urb(uinfo->irq);
	usb_free_coherent(uinfo->udev, uinfo->data_size, uinfo->data,
			  uinfo->dma);

	unregister_framebuffer(info);
	kfree(info->screen_buffer);
	framebuffer_release(info);
	kfree(uinfo);

	device_remove_file(&interface->dev, &dev_attr_pause);
	device_remove_file(&interface->dev, &dev_attr_frame_count);
	device_remove_file(&interface->dev, &dev_attr_command);

	unregister_reboot_notifier(&fbusb_reboot_notifier);

	dev_info(&interface->dev, "device now disconnected.\n");
}

static const struct usb_device_id fbusb_ids[] = {
	{USB_DEVICE(0xc872, 0x1004)},
	{}
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
MODULE_DESCRIPTION("VoCore USB2.0 screen framebuffer driver");
MODULE_LICENSE("GPL");
