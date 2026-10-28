// SPDX-License-Identifier: GPL-2.0
/*
 * HID driver for Xiaomi Mi Dual Mode Wireless Mouse Silent Edition
 *
 * Copyright (c) 2021 Ilya Skriblovsky
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/hid.h>

#include "hid-ids.h"

/* Fixed Mi Silent Mouse report descriptor */
/* Button's Usage Maximum changed from 3 to 5 to make side buttons work */
#define MI_SILENT_MOUSE_ORIG_RDESC_LENGTH   87
static __u8 mi_silent_mouse_rdesc_fixed[] = {
	0x05, 0x01,         /*  Usage Page (Desktop),               */
	0x09, 0x02,         /*  Usage (Mouse),                      */
	0xA1, 0x01,         /*  Collection (Application),           */
	0x85, 0x03,         /*      Report ID (3),                  */
	0x09, 0x01,         /*      Usage (Pointer),                */
	0xA1, 0x00,         /*      Collection (Physical),          */
	0x05, 0x09,         /*          Usage Page (Button),        */
	0x19, 0x01,         /*          Usage Minimum (01h),        */
	0x29, 0x05, /* X */ /*          Usage Maximum (05h),        */
	0x15, 0x00,         /*          Logical Minimum (0),        */
	0x25, 0x01,         /*          Logical Maximum (1),        */
	0x75, 0x01,         /*          Report Size (1),            */
	0x95, 0x05,         /*          Report Count (5),           */
	0x81, 0x02,         /*          Input (Variable),           */
	0x75, 0x03,         /*          Report Size (3),            */
	0x95, 0x01,         /*          Report Count (1),           */
	0x81, 0x01,         /*          Input (Constant),           */
	0x05, 0x01,         /*          Usage Page (Desktop),       */
	0x09, 0x30,         /*          Usage (X),                  */
	0x09, 0x31,         /*          Usage (Y),                  */
	0x15, 0x81,         /*          Logical Minimum (-127),     */
	0x25, 0x7F,         /*          Logical Maximum (127),      */
	0x75, 0x08,         /*          Report Size (8),            */
	0x95, 0x02,         /*          Report Count (2),           */
	0x81, 0x06,         /*          Input (Variable, Relative), */
	0x09, 0x38,         /*          Usage (Wheel),              */
	0x15, 0x81,         /*          Logical Minimum (-127),     */
	0x25, 0x7F,         /*          Logical Maximum (127),      */
	0x75, 0x08,         /*          Report Size (8),            */
	0x95, 0x01,         /*          Report Count (1),           */
	0x81, 0x06,         /*          Input (Variable, Relative), */
	0xC0,               /*      End Collection,                 */
	0xC0,               /*  End Collection,                     */
	0x06, 0x01, 0xFF,   /*  Usage Page (FF01h),                 */
	0x09, 0x01,         /*  Usage (01h),                        */
	0xA1, 0x01,         /*  Collection (Application),           */
	0x85, 0x05,         /*      Report ID (5),                  */
	0x09, 0x05,         /*      Usage (05h),                    */
	0x15, 0x00,         /*      Logical Minimum (0),            */
	0x26, 0xFF, 0x00,   /*      Logical Maximum (255),          */
	0x75, 0x08,         /*      Report Size (8),                */
	0x95, 0x04,         /*      Report Count (4),               */
	0xB1, 0x02,         /*      Feature (Variable),             */
	0xC0                /*  End Collection                      */
};

static __u8 *xiaomi_report_fixup(struct hid_device *hdev, __u8 *rdesc,
				 unsigned int *rsize)
{
	switch (hdev->product) {
	case USB_DEVICE_ID_MI_SILENT_MOUSE:
		if (*rsize == MI_SILENT_MOUSE_ORIG_RDESC_LENGTH) {
			hid_info(hdev, "fixing up Mi Silent Mouse report descriptor\n");
			rdesc = mi_silent_mouse_rdesc_fixed;
			*rsize = sizeof(mi_silent_mouse_rdesc_fixed);
		}
		break;
	}
	return rdesc;
}

/*
 * The Focus Pen sends its tip pressure in report 5, which the userspace stylus
 * service reads over hidraw and forwards to TouchFeature. Keep report 5 off the
 * input devices so it doesn't turn pressure samples into phantom keypresses.
 *
 * Report 2 carries the pen keyboard events:
 *   - Lower/Writing button: KEY_PAGEUP
 *   - Upper/Screenshot button: KEY_PAGEDOWN
 *   - Gestures (Pro): KEY_F19 (pinch), KEY_KP9 (swipe up), KEY_KP3 (swipe down),
 *     KEY_KPENTER (double tap)
 *
 * Keep Report 2 mapped so the pen keyboard is exposed to Android input.
 */
static int xiaomi_input_mapping(struct hid_device *hdev, struct hid_input *hi,
				struct hid_field *field, struct hid_usage *usage,
				unsigned long **bit, int *max)
{
	if (hdev->vendor != BT_VENDOR_ID_XIAOMI_PEN)
		return 0;

	switch (hdev->product) {
	case BT_DEVICE_ID_XIAOMI_FOCUS_PEN_PRO:
	case BT_DEVICE_ID_XIAOMI_FOCUS_PEN:
	case BT_DEVICE_ID_XIAOMI_FOCUS_PEN_2:
	case BT_DEVICE_ID_XIAOMI_FOCUS_PEN_3:
	case BT_DEVICE_ID_XIAOMI_FOCUS_PEN_4:
		break;
	default:
		return 0;
	}

	switch (field->report->id) {
	case 5:
	case 6:
		return -1;
	}

	/* Drop phantom keypad keys emitted during drawing/disconnect */
	if ((usage->hid & HID_USAGE_PAGE) == HID_UP_KEYBOARD) {
		switch (usage->hid & HID_USAGE) {
		case 0x57: /* Keypad + (KEY_KPPLUS) */
		case 0x67: /* Keypad = (KEY_KPEQUAL) */
		case 0x2e: /* Keyboard = (KEY_EQUAL) */
			return -1;
		}
	}

	return 0;
}

static const struct hid_device_id xiaomi_devices[] = {
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_XIAOMI, USB_DEVICE_ID_MI_SILENT_MOUSE) },
	{ HID_BLUETOOTH_DEVICE(BT_VENDOR_ID_XIAOMI_PEN, BT_DEVICE_ID_XIAOMI_FOCUS_PEN_PRO) },
	{ HID_BLUETOOTH_DEVICE(BT_VENDOR_ID_XIAOMI_PEN, BT_DEVICE_ID_XIAOMI_FOCUS_PEN) },
	{ HID_BLUETOOTH_DEVICE(BT_VENDOR_ID_XIAOMI_PEN, BT_DEVICE_ID_XIAOMI_FOCUS_PEN_2) },
	{ HID_BLUETOOTH_DEVICE(BT_VENDOR_ID_XIAOMI_PEN, BT_DEVICE_ID_XIAOMI_FOCUS_PEN_3) },
	{ HID_BLUETOOTH_DEVICE(BT_VENDOR_ID_XIAOMI_PEN, BT_DEVICE_ID_XIAOMI_FOCUS_PEN_4) },
	{ }
};
MODULE_DEVICE_TABLE(hid, xiaomi_devices);

static struct hid_driver xiaomi_driver = {
	.name = "xiaomi",
	.id_table = xiaomi_devices,
	.report_fixup = xiaomi_report_fixup,
	.input_mapping = xiaomi_input_mapping,
};
module_hid_driver(xiaomi_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ilya Skriblovsky <IlyaSkriblovsky@gmail.com>");
MODULE_DESCRIPTION("Fixing side buttons of Xiaomi Mi Silent Mouse");
