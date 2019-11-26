#define FBUSB_TYPE	"ili9806"

static const unsigned char boot[] = {
	0xff, 3,  0xff, 0x98, 0x06,
	0xbc, 21, 0x01, 0x0e, 0x61, 0xfb, 0x10, 0x10, 0x0b, 0x0f, 0x2e, 0x73,
		  0xff, 0xff, 0x0e, 0x0e, 0x00, 0x03, 0x66, 0x63, 0x01, 0x00,
		  0x00,
	0xbd, 8,  0x01, 0x23, 0x45, 0x67, 0x01, 0x23, 0x45, 0x67,
	0xbe, 9,  0x00, 0x21, 0xab, 0x60, 0x22, 0x22, 0x22, 0x22, 0x22,
	0xed, 3,  0x7f, 0xf0, 0x00,
	0xfc, 1,  0x0a,
	0xf3, 1,  0x74,
	0xb4, 3,  0x00, 0x00, 0x00,
	0xf7, 1,  0x8a,
	0xb1, 3,  0x00, 0x12, 0x13,
	0xf2, 4,  0x80, 0x5b, 0x40, 0x28,
	0xc0, 3,  0x37, 0x0b, 0x0a,
	0xc1, 4,  0x17, 0x7d, 0x7a, 0x20,
	0xc7, 1,  0x6f,
	0xe0, 16, 0x00, 0x11, 0x1c, 0x0e, 0x0f, 0x0c, 0xc7, 0x06, 0x06, 0x0a,
		  0x10, 0x12, 0x0a, 0x10, 0x02, 0x00,
	0xe1, 16, 0x00, 0x12, 0x18, 0x0c, 0x0f, 0x0a, 0x77, 0x06, 0x07, 0x0a,
		  0x0e, 0x0b, 0x10, 0x1d, 0x17, 0x00,
	0x35, 1,  0x00,
	0x21, 0,
	0x3a, 1,  0x55,	// 0x77 24bit, 0x55 16bit
	0x36, 1,  0x08, // BGR order.
	0x11, 0,	// sleep out.
	0x51, 1,  0xff,	// backlight max(3.3V output).
	0x53, 1,  0x24,
	0x55, 1,  0x02,
	0x29, 0,
	0x00, 0,	// end mark.
};

inline void fbusb_load_logo(u8 *logo)
{
	u8 *buf = kmalloc(0x6000, GFP_KERNEL);
	int size, osize, i;

	if (buf == NULL)
		return;

	fbusb_data_uncompress(logo16, sizeof(logo16), buf, size);
	fbusb_data_uncompress(buf, size, logo, osize);
	kfree(buf);

	// swap byte order for this screen.
	for (i = 0; i < osize; i += 2) {
		u8 tmp = logo[i];
		logo[i] = logo[i + 1];
		logo[i + 1] = tmp;
	}
}

static int fbusb_boot_setup(struct fbusb_info *uinfo)
{
	struct usb_device *udev;
	const u8 *cur = boot;
	u8 *buf = NULL, *mem = NULL;
	int i, ret, size;

	udev = interface_to_usbdev(uinfo->interface);
	mem = kmalloc(FBUSB_SCREEN_BUFFER, GFP_KERNEL);
	if (mem == NULL) {
		ret = -ENOMEM;
		goto fbusb_boot_setup_error;
	}
	buf = mem + 6;

	/* check if firmwall already installed */
	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0), 0xb4, 0xc0,
			      0, 0, buf, 1, FBUSB_MAX_DELAY);
	if (ret < 0 || buf[0] == 1)
		goto fbusb_boot_setup_error;

	while(1) {
		if (cur[0] == 0x00)
			break;		// this is end mark.
		if (cur[1] > FBUSB_SCREEN_BUFFER / 2) {
			ret =  -EIO;	// max word is buffer/2.
			goto fbusb_boot_setup_error;
		}

		for (i = 0; i < cur[1]; i++) {
			buf[i * 2] = 0;
			buf[i * 2 + 1] = cur[i + 2];
		}

		size = cur[1] * 2;
		mem[0] = 0;
		mem[1] = cur[0];
		mem[2] = size;
		mem[3] = size >> 8;
		mem[4] = size >> 16;
		mem[5] = size >> 24;

		ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0xb0, 0x40,
				      0, 0, mem, size + 6, FBUSB_MAX_DELAY);
		if (ret < 0)
			goto fbusb_boot_setup_error;

		cur += cur[1] + 2;
	}

	// send ready command.
	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0xb3, 0x40,
			      0, 0, NULL, 0, FBUSB_MAX_DELAY);

fbusb_boot_setup_error:
	kfree(mem);
	return ret;
}

static int fbusb_update_backlight(struct fbusb_info *uinfo)
{
	static u16 backlight;
	struct fbusb_par *par = uinfo->info->par;
	u8 *cmd;
	int ret;

	if (backlight == uinfo->backlight)
		return 0;
	backlight = uinfo->backlight;

	cmd = kzalloc(8, GFP_KERNEL);
	cmd[1] = 0x51;
	cmd[2] = 0x02;
	cmd[7] = (backlight * 255 + 50) / 100;
	ret = usb_control_msg(par->udev, usb_sndctrlpipe(par->udev, 0), 0xb0, 0x40,
		0, 0, cmd, 8, FBUSB_MAX_DELAY);
	kfree(cmd);

	if (ret < 0)
		return ret;
	return 0;
}
