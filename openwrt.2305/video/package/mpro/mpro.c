// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 Hans de Goede <hdegoede@redhat.com>
 */

#include <linux/module.h>
#include <linux/pm.h>
#include <linux/usb.h>
#include <linux/usb/input.h>
#include <linux/input.h>
#include <linux/backlight.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_state_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_damage_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_edid.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_file.h>
#include <drm/drm_format_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_gem_shmem_helper.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_managed.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_simple_kms_helper.h>

#define DRIVER_NAME		"mpro"
#define DRIVER_DESC		"VoCore Screen"
#define DRIVER_DATE		"2024"
#define DRIVER_MAJOR		0
#define DRIVER_MINOR		1

#define MPRO_BPP		16
#define MPRO_MAX_DELAY		100
#define MPRO_INPUT_TRS_SIZE	14

#define MODEL_DEFAULT		"MPRO\n"
#define MODEL_5IN		"MPRO-5\n"
#define MODEL_5IN_OLED		"MPRO-5H\n"
#define MODEL_4IN3		"MPRO-4IN3\n"
#define MODEL_4IN		"MPRO-4\n"
#define MODEL_6IN8		"MPRO-6IN8\n"
#define MODEL_3IN4		"MPRO-3IN4\n"

struct mpro_device {
	struct drm_device dev;
	struct device *dmadev;
	struct usb_interface *interface;
	struct backlight_device *bl_dev;
	struct drm_simple_display_pipe pipe;
	struct drm_connector conn;

	unsigned int screen;
	unsigned int version;
	unsigned char id[8];
	char *model;

	unsigned int width;
	unsigned int height;
	unsigned int margin;
	unsigned int width_mm;
	unsigned int height_mm;

	unsigned char cmd[64];
	unsigned char *draw_buf;

	struct input_dev *input;
	char name[64];
	char phys[64];
	struct urb *irq;
	unsigned char *trans_buf;
	dma_addr_t dma;
};

union mpro_axis {
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

struct mpro_point {
	union mpro_axis xh;
	unsigned char xl;
	union mpro_axis yh;
	unsigned char yl;

	unsigned char weight;
	unsigned char misc;
};

struct mpro_touch {
	unsigned char unused[2];
	unsigned char count;
	struct mpro_point p[2];
};

#define to_mpro(__dev) container_of(__dev, struct mpro_device, dev)

static const char cmd_get_screen[5] = {
	0x51, 0x02, 0x04, 0x1f, 0xfc
};

static const char cmd_get_version[5] = {
	0x51, 0x02, 0x04, 0x1f, 0xf8
};

static const char cmd_get_id[5] = {
	0x51, 0x02, 0x08, 0x1f, 0xf0
};

//static char cmd_draw[6] = {
//	0x00, 0x2c, 0x00, 0x00, 0x00, 0x00
//};

static char cmd_draw_part[12] = {
	0x00, 0x2c, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static char cmd_quit_sleep[6] = {
	0x00, 0x29, 0x00, 0x00, 0x00, 0x00
};

static char cmd_set_brightness[8] = {
	0x00, 0x51, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00
};

static inline struct usb_device *mpro_to_usb_device(struct mpro_device *mpro)
{
	return interface_to_usbdev(to_usb_interface(mpro->dev.dev));
}

static int mpro_data_alloc(struct mpro_device *mpro)
{
	int block_size;

	block_size = mpro->height * mpro->width * MPRO_BPP / 8 + mpro->margin;

	mpro->draw_buf = drmm_kmalloc(&mpro->dev, block_size, GFP_KERNEL);
	if (!mpro->draw_buf)
		return -ENOMEM;

	return 0;
}

static int mpro_send_command(struct mpro_device *mpro, void* cmd, unsigned int len)
{
	struct usb_device *udev = mpro_to_usb_device(mpro);

	if (len == 0)
		return 0;

	if (len > 64)
		return -EINVAL;

	/* Make sure use kmalloc memory space. */
	memcpy(mpro->cmd, cmd, len);

	/* 0x40 USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE */
	return usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
				0xb0, 0x40, 0, 0, mpro->cmd, len,
				MPRO_MAX_DELAY);
}

static int mpro_update_frame(struct mpro_device *mpro, unsigned int len)
{
	struct usb_device *udev = mpro_to_usb_device(mpro);
	int ret;

	ret = mpro_send_command(mpro, cmd_draw_part, sizeof(cmd_draw_part));
	if (ret < 0)
		return ret;

	return usb_bulk_msg(udev, usb_sndbulkpipe(udev, 0x02), mpro->draw_buf,
			    len, NULL, MPRO_MAX_DELAY);
}

static int mpro_buf_copy(void *dst, const struct dma_buf_map *map, struct drm_framebuffer *fb, struct drm_rect *clip)
{
	int ret;

	ret = drm_gem_fb_begin_cpu_access(fb, DMA_FROM_DEVICE);
	if (ret)
		return ret;

	drm_fb_xrgb8888_to_rgb565(dst, map->vaddr, fb, clip, false);

	drm_gem_fb_end_cpu_access(fb, DMA_FROM_DEVICE);

	return 0;
}

static void mpro_fb_mark_dirty(struct drm_framebuffer *fb, const struct dma_buf_map *map, struct drm_rect *rect)
{
	struct mpro_device *mpro = to_mpro(fb->dev);
	int idx, len, width, ret;

	if (!drm_dev_enter(fb->dev, &idx))
		return;

	ret = mpro_buf_copy(mpro->draw_buf, map, fb, rect);
	if (ret)
		goto err_msg;

	len = (rect->x2 - rect->x1) * (rect->y2 - rect->y1) * MPRO_BPP / 8;
	width = rect->x2 - rect->x1;

	cmd_draw_part[6] = (char)(rect->x1 >> 0);
	cmd_draw_part[7] = (char)(rect->x1 >> 8);
	cmd_draw_part[8] = (char)(rect->y1 >> 0);
	cmd_draw_part[9] = (char)(rect->y1 >> 8);
	cmd_draw_part[10] = (char)(width >> 0);
	cmd_draw_part[11] = (char)(width >> 8);

	cmd_draw_part[2] = (char)(len >> 0);
	cmd_draw_part[3] = (char)(len >> 8);
	cmd_draw_part[4] = (char)(len >> 16);

	ret = mpro_update_frame(mpro, len);
err_msg:
	if (ret)
		dev_err_once(fb->dev->dev, "Failed to update display %d\n", ret);

	drm_dev_exit(idx);
}

/* ------------------------------------------------------------------ */
/* mpro connector						      */

/*
 * We use fake EDID info so that userspace know that it is dealing with
 * an Acer projector, rather then listing this as an "unknown" monitor.
 * Note this assumes this driver is only ever used with the Acer C120, if we
 * add support for other devices the vendor and model should be parameterized.
 */
static struct edid mpro_edid = {
	.header		= { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 },
	.mfg_id		= { 0x59, 0xe3 },	/* "VOC" */
	.prod_code	= { 0x04, 0x10 },	/* 1004 */
	.serial		= 0xdeadbeef,
	.mfg_week	= 16,
	.mfg_year	= 24,
	.version	= 1,			/* EDID 1.3 */
	.revision	= 3,			/* EDID 1.3 */
	.input		= 0x80,			/* Digital input */
	.features	= 0x02,			/* Pref timing in DTD 1 */
	.standard_timings = { { 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 },
			      { 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 } },
	.detailed_timings = { {
		.pixel_clock = 1800,
		/* hactive = 848, hblank = 256 */
		.data.pixel_data.hactive_lo = 0x50,
		.data.pixel_data.hblank_lo = 0x00,
		.data.pixel_data.hactive_hblank_hi = 0x31,
		/* vactive = 480, vblank = 28 */
		.data.pixel_data.vactive_lo = 0xe0,
		.data.pixel_data.vblank_lo = 0x1c,
		.data.pixel_data.vactive_vblank_hi = 0x10,
		/* hsync offset 40 pw 128, vsync offset 1 pw 4 */
		.data.pixel_data.hsync_offset_lo = 0x28,
		.data.pixel_data.hsync_pulse_width_lo = 0x80,
		.data.pixel_data.vsync_offset_pulse_width_lo = 0x14,
		.data.pixel_data.hsync_vsync_offset_pulse_width_hi = 0x00,
		/* Digital separate syncs, hsync+, vsync+ */
		.data.pixel_data.misc = 0x1e,
	}, {
		.pixel_clock = 0,
		.data.other_data.type = 0xfd, /* Monitor ranges */
		.data.other_data.data.range.min_vfreq = 27,
		.data.other_data.data.range.max_vfreq = 33,
		.data.other_data.data.range.min_hfreq_khz = 29,
		.data.other_data.data.range.max_hfreq_khz = 32,
		.data.other_data.data.range.pixel_clock_mhz = 2,
		.data.other_data.data.range.flags = 0,
		.data.other_data.data.range.formula.cvt = {
			0xa0, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20 },
	}, {
		.pixel_clock = 0,
		.data.other_data.type = 0xfc, /* Model string */
		.data.other_data.data.str.str = {
			'M', 'P', 'R', 'O', '\n', ' ', ' ', ' ', ' ', ' ',
			' ', ' ', ' ' },
	}, {
		.pixel_clock = 0,
		.data.other_data.type = 0xff, /* Serial Number */
		.data.other_data.data.str.str = {
			'\n', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
			' ', ' ',  ' ' },
	} },
	.checksum = 0x00,
};

static int mpro_conn_get_modes(struct drm_connector *connector)
{
	drm_connector_update_edid_property(connector, &mpro_edid);
	return drm_add_edid_modes(connector, &mpro_edid);
}

static const struct drm_connector_helper_funcs mpro_conn_helper_funcs = {
	.get_modes = mpro_conn_get_modes,
};

static int mpro_backlight_update_status(struct backlight_device *bd)
{
	struct mpro_device *mpro = bl_get_data(bd);
	int brightness = backlight_get_brightness(bd);
	int ret;

	cmd_set_brightness[6] = brightness & 0xff;
	ret = mpro_send_command(mpro, cmd_set_brightness, sizeof(cmd_set_brightness));
	if (ret < 0)
		return ret;

	return 0;
}

static const struct backlight_ops mpro_bl_ops = {
	.update_status = mpro_backlight_update_status,
};

static int mpro_conn_late_register(struct drm_connector *connector)
{
	struct mpro_device *mpro = to_mpro(connector->dev);
	struct backlight_device *bl;

	bl = devm_backlight_device_register(connector->kdev, "mpro_backlight",
					    connector->kdev, mpro,
					    &mpro_bl_ops, NULL);
	if (IS_ERR(bl)) {
		drm_err(connector->dev, "Unable to register backlight device\n");
		return -EIO;
	}

	bl->props.brightness = 255;
	bl->props.max_brightness = 255;
	mpro->bl_dev = bl;

	return 0;
}

static const struct drm_connector_funcs mpro_conn_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
	.late_register = mpro_conn_late_register,
};

static int mpro_conn_init(struct mpro_device *mpro)
{
	drm_connector_helper_add(&mpro->conn, &mpro_conn_helper_funcs);
	return drm_connector_init(&mpro->dev, &mpro->conn,
				  &mpro_conn_funcs, DRM_MODE_CONNECTOR_USB);
}

static void mpro_pipe_enable(struct drm_simple_display_pipe *pipe,
				 struct drm_crtc_state *crtc_state,
				 struct drm_plane_state *plane_state)
{
	struct mpro_device *mpro = to_mpro(pipe->crtc.dev);
	struct drm_shadow_plane_state *shadow_plane_state = to_drm_shadow_plane_state(plane_state);
	struct drm_framebuffer *fb = plane_state->fb;
	struct drm_rect rect = {
		.x1 = 0,
		.x2 = fb->width,
		.y1 = 0,
		.y2 = fb->height,
	};

	mpro_send_command(mpro, cmd_quit_sleep, sizeof(cmd_quit_sleep));
	mpro_fb_mark_dirty(fb, &shadow_plane_state->data[0], &rect);
}

static void mpro_pipe_disable(struct drm_simple_display_pipe *pipe)
{
	//struct mpro_device *mpro = to_mpro(pipe->crtc.dev);

}

static void mpro_pipe_update(struct drm_simple_display_pipe *pipe,
				 struct drm_plane_state *old_state)
{
	struct drm_plane_state *state = pipe->plane.state;
	struct drm_shadow_plane_state *shadow_plane_state = to_drm_shadow_plane_state(state);
	struct drm_framebuffer *fb = state->fb;
	struct drm_rect rect;

	if (!pipe->crtc.state->active)
		return;

	if (drm_atomic_helper_damage_merged(old_state, state, &rect))
		mpro_fb_mark_dirty(fb, &shadow_plane_state->data[0], &rect);
}

static const struct drm_simple_display_pipe_funcs mpro_pipe_funcs = {
	.enable	    = mpro_pipe_enable,
	.disable    = mpro_pipe_disable,
	.update	    = mpro_pipe_update,
	DRM_GEM_SIMPLE_DISPLAY_PIPE_SHADOW_PLANE_FUNCS,
};

static const uint32_t mpro_pipe_formats[] = {
	DRM_FORMAT_XRGB8888,
};

static const uint64_t mpro_pipe_modifiers[] = {
	DRM_FORMAT_MOD_LINEAR,
	DRM_FORMAT_MOD_INVALID
};

DEFINE_DRM_GEM_FOPS(mpro_fops);

static const struct drm_driver mpro_drm_driver = {
	.driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_ATOMIC,

	.name		 = DRIVER_NAME,
	.desc		 = DRIVER_DESC,
	.date		 = DRIVER_DATE,
	.major		 = DRIVER_MAJOR,
	.minor		 = DRIVER_MINOR,

	.fops		 = &mpro_fops,
	DRM_GEM_SHMEM_DRIVER_OPS,
};

static const struct drm_mode_config_funcs mpro_mode_config_funcs = {
	.fb_create = drm_gem_fb_create_with_dirty,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static int mpro_get_screen(struct mpro_device *mpro)
{
	struct usb_device *udev = mpro_to_usb_device(mpro);
	void *cmd = mpro->cmd;
	int ret;

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
			      0xb5, 0x40, 0, 0,
			      (void *)cmd_get_screen, 5,
			      MPRO_MAX_DELAY);
	if (ret < 5)
		goto err;

	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			      0xb6, 0xc0, 0, 0,
			      cmd, 1,
			      MPRO_MAX_DELAY);
	if (ret < 1)
		goto err;

	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			      0xb7, 0xc0, 0, 0,
			      cmd, 5,
			      MPRO_MAX_DELAY);
	if (ret < 5)
		goto err;

	mpro->screen = ((unsigned int *)(cmd + 1))[0];
	return 0;
err:
	return -EIO;
}

static int mpro_get_version(struct mpro_device *mpro)
{
	struct usb_device *udev = mpro_to_usb_device(mpro);
	void *cmd = mpro->cmd;
	int ret;

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
			      0xb5, 0x40, 0, 0,
			      (void *)cmd_get_version, 5,
			      MPRO_MAX_DELAY);
	if (ret < 5)
		goto err;

	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			      0xb6, 0xc0, 0, 0,
			      cmd, 1,
			      MPRO_MAX_DELAY);
	if (ret < 1)
		goto err;

	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			      0xb7, 0xc0, 0, 0,
			      cmd, 5,
			      MPRO_MAX_DELAY);
	if (ret < 5)
		goto err;

	mpro->version = ((unsigned int *)(cmd + 1))[0];
	return 0;
err:
	return -EIO;
}

static int mpro_get_id(struct mpro_device *mpro)
{
	struct usb_device *udev = mpro_to_usb_device(mpro);
	void *cmd = mpro->cmd;
	int ret;

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
			      0xb5, 0x40, 0, 0,
			      (void *)cmd_get_id, 5,
			      MPRO_MAX_DELAY);
	if (ret < 5)
		goto err;

	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			      0xb6, 0xc0, 0, 0,
			      cmd, 1,
			      MPRO_MAX_DELAY);
	if (ret < 1)
		goto err;

	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			      0xb7, 0xc0, 0, 0,
			      cmd, 9,
			      MPRO_MAX_DELAY);
	if (ret < 5)
		goto err;

	memcpy(mpro->id, cmd + 1, 8);

	return 0;
err:
	return -EIO;
}

static void mpro_mode_config_setup(struct mpro_device *mpro)
{
	struct drm_device *dev = &mpro->dev;
	int width, height, margin, width_mm, height_mm;
	char *model = MODEL_DEFAULT;

	width  = 480;
	height = 800;
	margin = 0;
	width_mm = 0;
	height_mm = 0;

	switch (mpro->screen) {
	case 0x00000005:
		model = MODEL_5IN;
		height = 854;
		width_mm = 62;
		height_mm = 110;
		if (mpro->version != 0x00000003)
			margin = 320;
		break;

	case 0x00001005:
		model = MODEL_5IN_OLED;
		width = 720;
		height = 1280;
		width_mm = 62;
		height_mm = 110;
		break;

	case 0x00000304:
		model = MODEL_4IN3;
		width_mm = 56;
		height_mm = 94;
		break;

	case 0x00000004:
	case 0x00000b04:
	case 0x00000104:
		model = MODEL_4IN;
		width_mm = 53;
		height_mm = 86;
		break;

	case 0x00000007:
		model = MODEL_6IN8;
		width = 800;
		height = 480;
		width_mm = 89;
		height_mm = 148;
		break;

	case 0x00000403:
		model = MODEL_3IN4;
		width = 800;
		height = 800;
		width_mm = 88;
		height_mm = 88;
		break;
	}

	mpro->width = width;
	mpro->height = height;
	mpro->margin = margin;
	mpro->model = model;
	mpro->width_mm = width_mm;
	mpro->height_mm = height_mm;

	dev->mode_config.funcs = &mpro_mode_config_funcs;
	dev->mode_config.min_width = width;
	dev->mode_config.max_width = width;
	dev->mode_config.min_height = height;
	dev->mode_config.max_height = height;
}

static int mpro_edid_block_checksum(u8 *raw_edid)
{
	int i;
	u8 csum = 0, crc = 0;

	for (i = 0; i < EDID_LENGTH - 1; i++)
		csum += raw_edid[i];

	crc = 0x100 - csum;

	return crc;
}

static void mpro_edid_setup(struct mpro_device *mpro)
{
	unsigned int width, height, width_mm, height_mm;
	char buf[16];

	width = mpro->width;
	height = mpro->height;
	width_mm = mpro->width_mm;
	height_mm = mpro->height_mm;

	mpro_edid.detailed_timings[0].data.pixel_data.hactive_lo = width % 256;
	mpro_edid.detailed_timings[0].data.pixel_data.hactive_hblank_hi &= 0x0f;
	mpro_edid.detailed_timings[0].data.pixel_data.hactive_hblank_hi |= \
						((u8)(width / 256) << 4);

	mpro_edid.detailed_timings[0].data.pixel_data.vactive_lo = height % 256;
	mpro_edid.detailed_timings[0].data.pixel_data.vactive_vblank_hi &= 0x0f;
	mpro_edid.detailed_timings[0].data.pixel_data.vactive_vblank_hi |= \
						((u8)(height / 256) << 4);

	mpro_edid.detailed_timings[0].data.pixel_data.width_mm_lo = \
							width_mm % 256;
	mpro_edid.detailed_timings[0].data.pixel_data.height_mm_lo = \
							height_mm % 256;
	mpro_edid.detailed_timings[0].data.pixel_data.width_height_mm_hi = \
					((u8)(width_mm / 256) << 4) | \
					((u8)(height_mm / 256) & 0xf);

	memcpy(&mpro_edid.detailed_timings[2].data.other_data.data.str.str,
		mpro->model, strlen(mpro->model));

	snprintf(buf, 16, "%02X%02X%02X%02X\n",
		mpro->id[4], mpro->id[5], mpro->id[6], mpro->id[7]);

	memcpy(&mpro_edid.detailed_timings[3].data.other_data.data.str.str,
		buf, strlen(buf));

	mpro_edid.checksum = mpro_edid_block_checksum((u8*)&mpro_edid);
}

static int mpro_touch_open(struct input_dev *input)
{
	struct mpro_device *mpro = input_get_drvdata(input);
	int ret;

	ret = usb_submit_urb(mpro->irq, GFP_KERNEL);
	if (ret)
		return -EIO;

	return 0;
}

static void mpro_touch_close(struct input_dev *input)
{
	struct mpro_device *mpro = input_get_drvdata(input);
	usb_kill_urb(mpro->irq);
}

static void mpro_touch_irq(struct urb *urb)
{
	struct mpro_device *mpro = urb->context;
	struct device *dev = &mpro->interface->dev;
	struct mpro_touch *t = (struct mpro_touch *)mpro->trans_buf;
	int ret, x, y, touch;

	/* urb->status == ENOENT: closed by usb_kill_urb */
	if (urb->status)
		return;

	x = (((int)t->p[0].xh.x.h) << 8) + t->p[0].xl;
	y = (((int)t->p[0].yh.y.h) << 8) + t->p[0].yl;
	touch = (t->p[0].xh.x.f != 1) ? 1 : 0;

	input_report_key(mpro->input, BTN_TOUCH, touch);
	input_report_abs(mpro->input, ABS_X, x);
	input_report_abs(mpro->input, ABS_Y, y);
	input_sync(mpro->input);

	ret = usb_submit_urb(urb, GFP_ATOMIC);
	if (ret)
		dev_err(dev, "usb_submit_urb failed at irq: %d\n", ret);
}

static int mpro_touch_init(struct usb_interface *interface,
				struct mpro_device *mpro)
{
	struct usb_device *udev = mpro_to_usb_device(mpro);
	int ret;

	mpro->irq = usb_alloc_urb(0, GFP_KERNEL);
	if (!mpro->irq)
		return -ENOMEM;

	mpro->trans_buf = usb_alloc_coherent(udev, MPRO_INPUT_TRS_SIZE,
						GFP_KERNEL, &mpro->dma);
	if (!mpro->trans_buf) {
		ret = -ENOMEM;
		goto error_free_urb;
	}

	usb_fill_int_urb(mpro->irq, udev, usb_rcvintpipe(udev, 1),
			mpro->trans_buf, MPRO_INPUT_TRS_SIZE,
			mpro_touch_irq, mpro, 0);
	mpro->irq->dev = udev;
	mpro->irq->transfer_dma = mpro->dma;
	mpro->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	usb_make_path(udev, mpro->phys, sizeof(mpro->phys));
	strlcat(mpro->phys, "/input0", sizeof(mpro->phys));
	snprintf(mpro->name, sizeof(mpro->name), "VoCore Touch");

	mpro->input = input_allocate_device();
	if (!mpro->input) {
		ret = -ENOMEM;
		goto error_free_buffer;
	}
	usb_to_input_id(udev, &mpro->input->id);
	mpro->input->name = mpro->name;
	mpro->input->phys = mpro->phys;
	mpro->input->dev.parent = &interface->dev;

	mpro->input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	mpro->input->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	input_set_abs_params(mpro->input, ABS_X, 0, mpro->width, 0, 0);
	input_set_abs_params(mpro->input, ABS_Y, 0, mpro->height, 0, 0);

	mpro->input->open = mpro_touch_open;
	mpro->input->close = mpro_touch_close;
	input_set_drvdata(mpro->input, mpro);

	ret = input_register_device(mpro->input);
	if (ret)
		goto error_free_input;

	return 0;

error_free_input:
	input_free_device(mpro->input);
error_free_buffer:
	usb_free_coherent(udev, MPRO_INPUT_TRS_SIZE, mpro->trans_buf, mpro->dma);
error_free_urb:
	usb_free_urb(mpro->irq);

	return ret;
}

static int mpro_usb_probe(struct usb_interface *interface,
			  const struct usb_device_id *id)
{
	struct mpro_device *mpro;
	struct drm_device *dev;
	int ret;

	mpro = devm_drm_dev_alloc(&interface->dev, &mpro_drm_driver,
				      struct mpro_device, dev);
	if (IS_ERR(mpro))
		return PTR_ERR(mpro);
	dev = &mpro->dev;
	mpro->interface = interface;

	mpro->dmadev = usb_intf_get_dma_device(to_usb_interface(dev->dev));
	if (!mpro->dmadev)
		drm_warn(dev, "buffer sharing not supported"); /* not an error */

	ret = mpro_get_screen(mpro);
	if (ret) {
		drm_err(dev, "cant't get screen info.\n");
		goto err_put_device;
	}

	ret = mpro_get_version(mpro);
	if (ret) {
		drm_err(dev, "cant't get screen version.\n");
		goto err_put_device;
	}

	ret = mpro_get_id(mpro);
	if (ret) {
		drm_err(dev, "cant't get screen id.\n");
		goto err_put_device;
	}

	ret = drmm_mode_config_init(dev);
	if (ret)
		goto err_put_device;

	mpro_mode_config_setup(mpro);
	mpro_edid_setup(mpro);

	ret = mpro_data_alloc(mpro);
	if (ret)
		goto err_put_device;

	ret = mpro_conn_init(mpro);
	if (ret)
		goto err_put_device;

	ret = drm_simple_display_pipe_init(&mpro->dev,
					   &mpro->pipe,
					   &mpro_pipe_funcs,
					   mpro_pipe_formats,
					   ARRAY_SIZE(mpro_pipe_formats),
					   mpro_pipe_modifiers,
					   &mpro->conn);
	if (ret)
		goto err_put_device;

	drm_plane_enable_fb_damage_clips(&mpro->pipe.plane);

	drm_mode_config_reset(dev);

	usb_set_intfdata(interface, dev);
	ret = drm_dev_register(dev, 0);
	if (ret)
		goto err_put_device;

	drm_fbdev_generic_setup(dev, 0);

	ret = mpro_touch_init(interface, mpro);

	return ret;

err_put_device:
	put_device(mpro->dmadev);
	return ret;
}

static void mpro_usb_disconnect(struct usb_interface *interface)
{
	struct drm_device *dev = usb_get_intfdata(interface);
	struct mpro_device *mpro = to_mpro(dev);
	struct usb_device *udev = mpro_to_usb_device(mpro);

	input_unregister_device(mpro->input);
	usb_free_urb(mpro->irq);
	usb_free_coherent(udev, MPRO_INPUT_TRS_SIZE, mpro->trans_buf,
			  mpro->dma);

	put_device(mpro->dmadev);
	mpro->dmadev = NULL;
	drm_dev_unplug(dev);
	drm_atomic_helper_shutdown(dev);
}

static const struct usb_device_id id_table[] = {
	{ USB_DEVICE(0xc872, 0x1004) },
	{},
};
MODULE_DEVICE_TABLE(usb, id_table);

static struct usb_driver mpro_usb_driver = {
	.name = "mpro",
	.probe = mpro_usb_probe,
	.disconnect = mpro_usb_disconnect,
	.id_table = id_table,
};

module_usb_driver(mpro_usb_driver);
MODULE_AUTHOR("ieiao <ieiao@outlook.com>");
MODULE_LICENSE("GPL");
