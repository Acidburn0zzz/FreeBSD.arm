/*	$OpenBSD: udl.h,v 1.21 2013/04/15 09:23:02 mglocker Exp $ */
/*	$FreeBSD: head/sys/dev/usb/video/udl.h 281644 2015-04-17 07:07:06Z hselasky $	*/

/*
 * Copyright (c) 2009 Marcus Glocker <mglocker@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _UDL_H_
#define	_UDL_H_

#include <sys/types.h>
#include <sys/queue.h>

/*
 * BULK command transfer structure.
 */
#define	UDL_CMD_MAX_FRAMES	64	/* units */
#define	UDL_CMD_MAX_DATA_SIZE	512	/* bytes */
#define	UDL_CMD_MAX_HEAD_SIZE	16	/* bytes */
#define	UDL_CMD_MAX_PIXEL_COUNT	((UDL_CMD_MAX_DATA_SIZE - UDL_CMD_MAX_HEAD_SIZE) / 2)
#define	UDL_CMD_MAX_BUFFERS	(3 * UDL_CMD_MAX_FRAMES)
#define	UDL_FONT_HEIGHT		16	/* pixels */
#define	UDL_MAX_MODES		25	/* units */

MALLOC_DECLARE(M_USB_DL);

struct udl_buffer {
	TAILQ_ENTRY(udl_buffer) entry;
	uint32_t size;
};

TAILQ_HEAD(udl_buffer_head, udl_buffer);

struct udl_cmd_buf {
	TAILQ_ENTRY(udl_cmd_buf) entry;
	uint32_t off;
	uint8_t	buf[UDL_CMD_MAX_DATA_SIZE] __aligned(4);
};

TAILQ_HEAD(udl_cmd_head, udl_cmd_buf);

enum {
	UDL_BULK_WRITE_0,
	UDL_BULK_WRITE_1,
	UDL_N_TRANSFER,
};

/*
 * Our per device structure.
 */
struct udl_softc {
	struct mtx sc_mtx;
	struct cv sc_cv;
	struct callout sc_callout;
	struct usb_xfer *sc_xfer[UDL_N_TRANSFER];
	struct usb_device *sc_udev;
	device_t sc_fbdev;
	struct fb_info sc_fb_info;
	uint8_t	sc_edid[128];
	struct edid_info sc_edid_info;
	struct udl_cmd_head sc_xfer_head[2];
	struct udl_cmd_head sc_cmd_buf_free;
	struct udl_cmd_head sc_cmd_buf_pending;
	struct udl_cmd_buf sc_cmd_buf_temp[UDL_CMD_MAX_BUFFERS];
	uint32_t sc_sync_off;
	uint32_t sc_fb_size;
	uint8_t *sc_fb_addr;
	uint8_t *sc_fb_copy;
	int	sc_def_chip;		/* default chip version */
	int	sc_chip;
#define	DLALL	0x0000
#define	DL125	0x0000			/* max 1280x1024, 1440x900 */
#define	DL120	0x0001			/* max 1280x1024, 1440x1050 */
#define	DL160	0x0002			/* max 1600x1200, 1680x1050 */
#define	DL165	0x0003			/* max 1600x1200, 1920x1080 */
#define	DL195	0x0004			/* max 1920x1200, 2048x1152 */
#define	DLMAX	0x0004
#define	DLUNK	0x00ff			/* unknown */
	int	sc_def_mode;		/* default mode */
	int	sc_cur_mode;
	uint8_t	sc_power_save;		/* set if power save is enabled */
	uint8_t	sc_gone;
};

#define	UDL_LOCK(sc)	mtx_lock(&(sc)->sc_mtx)
#define	UDL_UNLOCK(sc)	mtx_unlock(&(sc)->sc_mtx)

/*
 * Chip commands.
 */
#define	UDL_CTRL_CMD_READ_EDID		0x02
#define	UDL_CTRL_CMD_WRITE_1		0x03
#define	UDL_CTRL_CMD_READ_1		0x04
#define	UDL_CTRL_CMD_POLL		0x06
#define	UDL_CTRL_CMD_SET_KEY		0x12

#define	UDL_BULK_SOC			0xaf	/* start of command token */

#define	UDL_BULK_CMD_REG_WRITE_1	0x20	/* write 1 byte to register */
#define	UDL_BULK_CMD_EOC		0xa0	/* end of command stack */
#define	UDL_BULK_CMD_DECOMP		0xe0	/* send decompression table */

#define	UDL_BULK_CMD_FB_BASE		0x60
#define	UDL_BULK_CMD_FB_WORD		0x08
#define	UDL_BULK_CMD_FB_COMP		0x10
#define	UDL_BULK_CMD_FB_WRITE		(UDL_BULK_CMD_FB_BASE | 0x00)
#define	UDL_BULK_CMD_FB_COPY		(UDL_BULK_CMD_FB_BASE | 0x02)

/*
 * Chip registers.
 */
#define	UDL_REG_ADDR_START16		0x20
#define	UDL_REG_ADDR_STRIDE16		0x23
#define	UDL_REG_ADDR_START8		0x26
#define	UDL_REG_ADDR_STRIDE8		0x29

#define	UDL_REG_SCREEN			0x1f
#define	UDL_REG_SCREEN_ON		0x00
#define	UDL_REG_SCREEN_OFF		0x01
#define	UDL_REG_SYNC			0xff

#define	UDL_MODE_SIZE 29

/*
 * Register values for screen resolution initialization.
 */
static const uint8_t udl_reg_vals_640x480_60[UDL_MODE_SIZE] = {	/* 25.17 Mhz 59.9 Hz
								 * VESA std */
	0x00, 0x99, 0x30, 0x26, 0x94, 0x60, 0xa9, 0xce, 0x60, 0x07, 0xb3, 0x0f,
	0x79, 0xff, 0xff, 0x02, 0x80, 0x83, 0xbc, 0xff, 0xfc, 0xff, 0xff, 0x01,
	0xe0, 0x01, 0x02, 0xab, 0x13
};
static const uint8_t udl_reg_vals_640x480_67[UDL_MODE_SIZE] = {	/* 30.25 MHz 66.6 Hz MAC
								 * std */
	0x00, 0x1d, 0x33, 0x07, 0xb3, 0x60, 0xa9, 0xce, 0x60, 0xb6, 0xa8, 0xff,
	0xff, 0xbf, 0x70, 0x02, 0x80, 0x83, 0xbc, 0xff, 0xff, 0xff, 0xf9, 0x01,
	0xe0, 0x01, 0x02, 0xa2, 0x17
};
static const uint8_t udl_reg_vals_640x480_72[UDL_MODE_SIZE] = {	/* 31.50 Mhz 72.8 Hz
								 * VESA std */
	0x00, 0x2b, 0xeb, 0x35, 0xd3, 0x0a, 0x95, 0xe6, 0x0e, 0x0f, 0xb5, 0x15,
	0x2a, 0xff, 0xff, 0x02, 0x80, 0xcc, 0x1d, 0xff, 0xf9, 0xff, 0xff, 0x01,
	0xe0, 0x01, 0x02, 0x9c, 0x18
};
static const uint8_t udl_reg_vals_640x480_75[UDL_MODE_SIZE] = {	/* 31.50 Mhz 75.7 Hz
								 * VESA std */
	0x00, 0xeb, 0xf7, 0xd3, 0x0f, 0x4f, 0x93, 0xfa, 0x47, 0xb5, 0x58, 0xff,
	0xff, 0xbf, 0x70, 0x02, 0x80, 0xf4, 0x8f, 0xff, 0xff, 0xff, 0xf9, 0x01,
	0xe0, 0x01, 0x02, 0x9c, 0x18
};
static const uint8_t udl_reg_vals_800x480_61[UDL_MODE_SIZE] = {	/* 33.00 MHz 61.9 Hz */
	0x00, 0x20, 0x3c, 0x7a, 0xc9, 0xf2, 0x6c, 0x48, 0xf9, 0x70, 0x53, 0xff,
	0xff, 0x21, 0x27, 0x03, 0x20, 0x91, 0xf3, 0xff, 0xff, 0xff, 0xf9, 0x01,
	0xe0, 0x01, 0x02, 0xc8, 0x19
};
static const uint8_t udl_reg_vals_800x600_56[UDL_MODE_SIZE] = {	/* 36.00 MHz 56.2 Hz
								 * VESA std */
	0x00, 0x65, 0x35, 0x48, 0xf4, 0xf2, 0x6c, 0x19, 0x18, 0xc9, 0x4b, 0xff,
	0xff, 0x70, 0x35, 0x03, 0x20, 0x32, 0x31, 0xff, 0xff, 0xff, 0xfc, 0x02,
	0x58, 0x01, 0x02, 0x20, 0x1c
};
static const uint8_t udl_reg_vals_800x600_60[UDL_MODE_SIZE] = {	/* 40.00 MHz 60.3 Hz
								 * VESA std */
	0x00, 0x20, 0x3c, 0x7a, 0xc9, 0x93, 0x60, 0xc8, 0xc7, 0x70, 0x53, 0xff,
	0xff, 0x21, 0x27, 0x03, 0x20, 0x91, 0x8f, 0xff, 0xff, 0xff, 0xf2, 0x02,
	0x58, 0x01, 0x02, 0x40, 0x1f
};
static const uint8_t udl_reg_vals_800x600_72[UDL_MODE_SIZE] = {	/* 50.00 MHz 72.1 Hz
								 * VESA std */
	0x00, 0xeb, 0xf7, 0xd1, 0x90, 0x4d, 0x82, 0x23, 0x1f, 0x39, 0xcf, 0xff,
	0xff, 0x43, 0x21, 0x03, 0x20, 0x62, 0xc5, 0xff, 0xff, 0xff, 0xca, 0x02,
	0x58, 0x01, 0x02, 0x10, 0x27
};
static const uint8_t udl_reg_vals_800x600_74[UDL_MODE_SIZE] = {	/* 50.00 MHz 74.4 Hz */
	0x00, 0xb3, 0x76, 0x39, 0xcf, 0x60, 0xa9, 0xc7, 0xf4, 0x70, 0x53, 0xff,
	0xff, 0x35, 0x33, 0x03, 0x20, 0x8f, 0xe9, 0xff, 0xff, 0xff, 0xf9, 0x02,
	0x58, 0x01, 0x02, 0x10, 0x27
};
static const uint8_t udl_reg_vals_800x600_75[UDL_MODE_SIZE] = {	/* 49.50 MHz 75.0 Hz
								 * VESA std */
	0x00, 0xb3, 0x76, 0x39, 0xcf, 0xf2, 0x6c, 0x19, 0x18, 0x70, 0x53, 0xff,
	0xff, 0x35, 0x33, 0x03, 0x20, 0x32, 0x31, 0xff, 0xff, 0xff, 0xf9, 0x02,
	0x58, 0x01, 0x02, 0xac, 0x26
};
static const uint8_t udl_reg_vals_1024x768_60[UDL_MODE_SIZE] = {	/* 65.00 MHz 60.0 Hz
									 * VESA std */
	0x00, 0x36, 0x18, 0xd5, 0x10, 0x60, 0xa9, 0x7b, 0x33, 0xa1, 0x2b, 0x27,
	0x32, 0xff, 0xff, 0x04, 0x00, 0xd9, 0x9a, 0xff, 0xca, 0xff, 0xff, 0x03,
	0x00, 0x04, 0x03, 0xc8, 0x32
};
static const uint8_t udl_reg_vals_1024x768_70[UDL_MODE_SIZE] = {	/* 75.00 MHz 70.0 Hz
									 * VESA std */
	0x00, 0xb4, 0xed, 0x4c, 0x5e, 0x60, 0xa9, 0x7b, 0x33, 0x10, 0x4d, 0xff,
	0xff, 0x27, 0x32, 0x04, 0x00, 0xd9, 0x9a, 0xff, 0xff, 0xff, 0xca, 0x03,
	0x00, 0x04, 0x02, 0x98, 0x3a
};
static const uint8_t udl_reg_vals_1024x768_75[UDL_MODE_SIZE] = {	/* 78.75 MHz 75.0 Hz
									 * VESA std */
	0x00, 0xec, 0xb4, 0xa0, 0x4c, 0x36, 0x0a, 0x07, 0xb3, 0x5e, 0xd5, 0xff,
	0xff, 0x0f, 0x79, 0x04, 0x00, 0x0f, 0x66, 0xff, 0xff, 0xff, 0xf9, 0x03,
	0x00, 0x04, 0x02, 0x86, 0x3d
};
static const uint8_t udl_reg_vals_1280x800_60[UDL_MODE_SIZE] = {	/* 83.46 MHz 59.9 MHz */
	0x00, 0xb2, 0x19, 0x34, 0xdf, 0x93, 0x60, 0x30, 0xfb, 0x9f, 0xca, 0xff,
	0xff, 0x27, 0x32, 0x05, 0x00, 0x61, 0xf6, 0xff, 0xff, 0xff, 0xf9, 0x03,
	0x20, 0x04, 0x02, 0x34, 0x41
};
static const uint8_t udl_reg_vals_1280x960_60[UDL_MODE_SIZE] = {	/* 108.00 MHz 60.0 Hz
									 * VESA std */
	0x00, 0xa6, 0x03, 0x5c, 0x7e, 0x0a, 0x95, 0x48, 0xf4, 0x61, 0xbd, 0xff,
	0xff, 0x94, 0x43, 0x05, 0x00, 0x91, 0xe8, 0xff, 0xff, 0xff, 0xf9, 0x03,
	0xc0, 0x04, 0x02, 0x60, 0x54
};
static const uint8_t udl_reg_vals_1280x1024_60[UDL_MODE_SIZE] = {	/* 108.00 MHz 60.0 Hz
									 * VESA std */
	0x00, 0x98, 0xf8, 0x0d, 0x57, 0x2a, 0x55, 0x4d, 0x54, 0xca, 0x0d, 0xff,
	0xff, 0x94, 0x43, 0x05, 0x00, 0x9a, 0xa8, 0xff, 0xff, 0xff, 0xf9, 0x04,
	0x00, 0x04, 0x02, 0x60, 0x54
};
static const uint8_t udl_reg_vals_1280x1024_75[UDL_MODE_SIZE] = {	/* 135.00 MHz 75.0 Hz
									 * VESA std */
	0x00, 0xce, 0x12, 0x3f, 0x9f, 0x2a, 0x55, 0x4d, 0x54, 0xca, 0x0d, 0xff,
	0xff, 0x32, 0x60, 0x05, 0x00, 0x9a, 0xa8, 0xff, 0xff, 0xff, 0xf9, 0x04,
	0x00, 0x04, 0x02, 0x78, 0x69
};
static const uint8_t udl_reg_vals_1366x768_60[UDL_MODE_SIZE] = {	/* 90 MHz 60.0 Hz */
	0x01, 0x19, 0x1e, 0x1f, 0xb0, 0x93, 0x60, 0x40, 0x7b, 0x36, 0xe8, 0x27,
	0x32, 0xff, 0xff, 0x05, 0x56, 0x03, 0xd9, 0xff, 0xff, 0xfc, 0xa7, 0x03,
	0x00, 0x04, 0x02, 0x9a, 0x42
};
static const uint8_t udl_reg_vals_1440x900_60[UDL_MODE_SIZE] = {	/* 106.47 MHz 59.9 Hz */
	0x00, 0x24, 0xce, 0xe7, 0x72, 0x36, 0x0a, 0x86, 0xca, 0x1c, 0x10, 0xff,
	0xff, 0x60, 0x3a, 0x05, 0xa0, 0x0d, 0x94, 0xff, 0xff, 0xff, 0xf9, 0x03,
	0x84, 0x04, 0x02, 0x2e, 0x53
};
static const uint8_t udl_reg_vals_1440x900_59[UDL_MODE_SIZE] = {	/* 106.50 MHz 59.8 Hz */
	0x00, 0x24, 0xce, 0xe7, 0x72, 0xd8, 0x2a, 0x1b, 0x28, 0x1c, 0x10, 0xff,
	0xff, 0x60, 0x3a, 0x05, 0xa0, 0x36, 0x50, 0xff, 0xff, 0xff, 0xf9, 0x03,
	0x84, 0x04, 0x02, 0x34, 0x53
};
static const uint8_t udl_reg_vals_1440x900_75[UDL_MODE_SIZE] = {	/* 136.49 MHz 75.0 Hz */
	0x00, 0x73, 0xa6, 0x14, 0xea, 0x0a, 0x95, 0xca, 0x10, 0x7f, 0x46, 0xff,
	0xff, 0x60, 0x3a, 0x05, 0xa0, 0x94, 0x20, 0xff, 0xff, 0xff, 0xf9, 0x03,
	0x84, 0x04, 0x02, 0xa2, 0x6a
};
static const uint8_t udl_reg_vals_1680x1050_60[UDL_MODE_SIZE] = {	/* 147.14 MHz 60.0 Hz */
	0x00, 0x53, 0x43, 0xa6, 0x71, 0xc1, 0x52, 0xd9, 0x29, 0x69, 0x9f, 0xff,
	0xff, 0xd7, 0xee, 0x06, 0x90, 0xb2, 0x53, 0xff, 0xff, 0xff, 0xf9, 0x04,
	0x1a, 0x04, 0x02, 0xf4, 0x72
};
static const uint8_t udl_reg_vals_1600x1200_60[UDL_MODE_SIZE] = {	/* 162.00 MHz 60.0 Hz
									 * VESA std */
	0x00, 0xcf, 0xa4, 0x3c, 0x4e, 0x55, 0x73, 0x71, 0x2b, 0x71, 0x52, 0xff,
	0xff, 0xee, 0xca, 0x06, 0x40, 0xe2, 0x57, 0xff, 0xff, 0xff, 0xf9, 0x04,
	0xb0, 0x04, 0x02, 0x90, 0x7e
};
static const uint8_t udl_reg_vals_1920x1080_60[UDL_MODE_SIZE] = {	/* 138.50 MHz 59.9 Hz */
	0x00, 0x73, 0xa6, 0x28, 0xb3, 0x54, 0xaa, 0x41, 0x5d, 0x0d, 0x9f, 0x32,
	0x60, 0xff, 0xff, 0x07, 0x80, 0x0a, 0xea, 0xff, 0xf9, 0xff, 0xff, 0x04,
	0x38, 0x04, 0x02, 0xe0, 0x7c
};

struct udl_mode {
	uint16_t hdisplay;
	uint16_t vdisplay;
	uint8_t	hz;
	uint16_t chip;
	uint32_t clock;
	const uint8_t *mode;
};

static const struct udl_mode udl_modes[UDL_MAX_MODES] = {
	{640, 480, 60, DLALL, 2520, udl_reg_vals_640x480_60},
	{640, 480, 67, DLALL, 3025, udl_reg_vals_640x480_67},
	{640, 480, 72, DLALL, 3150, udl_reg_vals_640x480_72},
	{640, 480, 75, DLALL, 3150, udl_reg_vals_640x480_75},
	{800, 480, 59, DLALL, 5000, udl_reg_vals_800x480_61},
	{800, 480, 61, DLALL, 3300, udl_reg_vals_800x480_61},
	{800, 600, 56, DLALL, 3600, udl_reg_vals_800x600_56},
	{800, 600, 60, DLALL, 4000, udl_reg_vals_800x600_60},
	{800, 600, 72, DLALL, 5000, udl_reg_vals_800x600_72},
	{800, 600, 74, DLALL, 5000, udl_reg_vals_800x600_74},
	{800, 600, 75, DLALL, 4950, udl_reg_vals_800x600_75},
	{1024, 768, 60, DLALL, 6500, udl_reg_vals_1024x768_60},
	{1024, 768, 70, DLALL, 7500, udl_reg_vals_1024x768_70},
	{1024, 768, 75, DLALL, 7850, udl_reg_vals_1024x768_75},
	{1280, 800, 60, DLALL, 8346, udl_reg_vals_1280x800_60},
	{1280, 960, 60, DLALL, 10800, udl_reg_vals_1280x960_60},
	{1280, 1024, 60, DLALL, 10800, udl_reg_vals_1280x1024_60},
	{1280, 1024, 75, DLALL, 13500, udl_reg_vals_1280x1024_75},
	{1366, 768, 60, DLALL, 9000, udl_reg_vals_1366x768_60},
	{1440, 900, 59, DL125, 10650, udl_reg_vals_1440x900_59},
	{1440, 900, 60, DL125, 10647, udl_reg_vals_1440x900_60},
	{1440, 900, 75, DL125, 13649, udl_reg_vals_1440x900_75},
	{1680, 1050, 60, DL160, 14714, udl_reg_vals_1680x1050_60},
	{1600, 1200, 60, DL160, 16200, udl_reg_vals_1600x1200_60},
	{1920, 1080, 60, DL165, 13850, udl_reg_vals_1920x1080_60}
};

/*
 * Encryption.
 */
static const uint8_t udl_null_key_1[] = {
	0x57, 0xcd, 0xdc, 0xa7, 0x1c, 0x88, 0x5e, 0x15, 0x60, 0xfe, 0xc6, 0x97,
	0x16, 0x3d, 0x47, 0xf2
};

#endif					/* _UDL_H_ */
