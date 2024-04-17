/** @file

  XUDC Controller Driver descriptor structures

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef XUSB_DEV_CONTROLLER_DESC_H_
#define XUSB_DEV_CONTROLLER_DESC_H_

/* Specifies a Language ID string descriptor index */
#define USB_LANGUAGE_ID  0
/*Specifies a Manufacturer ID string descriptor index */
#define USB_MANF_ID  1
/* Specifies a Product ID string descriptor index */
#define USB_PROD_ID  2
/* Specifies a Serial No string descriptor index */
#define USB_SERIAL_ID  3

#define MAX_SERIALNO_LEN  32

#define USB_DESC_FLAG_STATIC  (0x1)

#define USB_DESC_STATIC(x)      \
        { .desc = (VOID *)(x), .len = (UINT16)sizeof(x), .flags = USB_DESC_FLAG_STATIC}

/**
 * @brief USB Descriptor
 */
struct tegrabl_usb_descriptor {
  /* Description of USB descriptor */
  VOID      *desc;
  /* Size of the USB descriptor */
  UINT16    len;
  /* USB descriptor flags */
  UINT32    flags;
};

/**
 * @brief Complete USB config struct, passed into usb_setup()
 */
struct tegrabl_usbf_config {
  /* device desc for high speed.*/
  struct tegrabl_usb_descriptor    hs_device;
  /* device desc fpr super speed.*/
  struct tegrabl_usb_descriptor    ss_device;
  /* device qualifier desc data.*/
  struct tegrabl_usb_descriptor    device_qual;
  /* configuration desc data for super speed.*/
  struct tegrabl_usb_descriptor    ss_config;
  /* configuration desc data for non-super speed.*/
  struct tegrabl_usb_descriptor    hs_config;
  /* Other Speed configuration desc data.*/
  struct tegrabl_usb_descriptor    other_config;
  /* Language id desc data.*/
  struct tegrabl_usb_descriptor    langid;
  /* Manufacturer string desc data. */
  struct tegrabl_usb_descriptor    manufacturer;
  /* product string desc data.*/
  struct tegrabl_usb_descriptor    product;
  /* serialno string desc data. */
  struct tegrabl_usb_descriptor    serialno;
};

/**
 * USB Device Descriptor: 12 bytes as per the USB2.0 Specification
 * Stores the Device descriptor data must be word aligned
 */
static UINT8  s_ss_device_descr[] = {
  0x12,          /* bLength - Size of this descriptor in bytes */
  0x01,          /* bDescriptorType - Device Descriptor Type */
  0x00,          /* bcd USB (LSB) - USB Spec. Release number */
  0x03,          /* bcd USB (MSB) - USB Spec. Release number (3.0) */
  0x00,          /* bDeviceClass - Class is specified in the interface descriptor. */
  0x00,          /* bDeviceSubClass - SubClass is specified interface descriptor. */
  0x00,          /* bDeviceProtocol - Protocol is specified interface descriptor. */
  0x09,          /* bMaxPacketSize0 - Maximum packet size for EP0 */
  0x55,          /* idVendor(LSB) - Vendor ID assigned by USB forum */
  0x09,          /* idVendor(MSB) - Vendor ID assigned by USB forum */
  0x00,          /* idProduct(LSB) - Product ID assigned by Organization */
  0x70,          /* idProduct(MSB) - Product ID assigned by Organization */
  0x00,          /* bcd Device (LSB) - Device Release number in BCD */
  0x00,          /* bcd Device (MSB) - Device Release number in BCD */
  USB_MANF_ID,   /* Index of String descriptor describing Manufacturer */
  USB_PROD_ID,   /* Index of String descriptor describing Product */
  USB_SERIAL_ID, /* Index of String descriptor describing Serial number */
  0x01           /* bNumConfigurations - Number of possible configuration */
};

static UINT8  s_hs_device_descr[] = {
  0x12,          /* bLength - Size of this descriptor in bytes */
  0x01,          /* bDescriptorType - Device Descriptor Type */
  0x00,          /* bcd USB (LSB) - USB Spec. Release number */
  0x02,          /* bcd USB (MSB) - USB Spec. Release number (2.1) */
  0x00,          /* bDeviceClass - Class is specified in the interface descriptor. */
  0x00,          /* bDeviceSubClass - SubClass is specified interface descriptor. */
  0x00,          /* bDeviceProtocol - Protocol is specified interface descriptor. */
  0x40,          /* bMaxPacketSize0 - Maximum packet size for EP0 */
  0x55,          /* idVendor(LSB) - Vendor ID assigned by USB forum */
  0x09,          /* idVendor(MSB) - Vendor ID assigned by USB forum */
  0x00,          /* idProduct(LSB) - Product ID assigned by Organization */
  0x71,          /* idProduct(MSB) - Product ID assigned by Organization */
  0x00,          /* bcd Device (LSB) - Device Release number in BCD */
  0x00,          /* bcd Device (MSB) - Device Release number in BCD */
  USB_MANF_ID,   /* Index of String descriptor describing Manufacturer */
  USB_PROD_ID,   /* Index of String descriptor describing Product */
  USB_SERIAL_ID, /* Index of String descriptor describing Serial number */
  0x01           /* bNumConfigurations - Number of possible configuration */
};

/* Stores the Device Qualifier Desriptor data */
static const UINT8  s_usb_device_qualifier[] = {
  /* Device Qualifier descriptor */
  0x0a,       /* Size of the descriptor */
  0x06,       /* Device Qualifier Type */
  0x00,       /* USB specification version number: LSB */
  0x02,       /*  USB specification version number: MSB */
  0xFF,       /* Class Code */
  0xFF,       /* Subclass Code */
  0xFF,       /* Protocol Code */
  0x40,       /*Maximum packet size for other speed */
  0x01,       /*Number of Other-speed Configurations */
  0x00        /* Reserved for future use, must be zero */
};

/** super speed  config descriptor for fastboot */
static UINT8  s_usb_ss_config_descr_fastboot[] = {
  /* Configuration Descriptor 32 bytes  */
  0x09,         /* bLength - Size of this descriptor in bytes */
  0x02,         /* bDescriptorType - Configuration Descriptor Type */
  0x2c,         /* WTotalLength (LSB) - length of data for this configuration */
  0x00,         /* WTotalLength (MSB) - length of data for this configuration */
  0x01,         /* bNumInterface - Nos of Interface supported by configuration */
  0x01,         /* bConfigurationValue */
  0x00,         /* iConfiguration - Index of descriptor describing configuration */
  0xc0,         /* bmAttributes-bitmap "D4-D0:Res,D6:Self Powered,D5:Remote Wakeup*/
  0x10,         /* MaxPower in mA - Max Power Consumption of the USB device */

  /* Interface Descriptor */
  0x09,         /* bLength - Size of this descriptor in bytes */
  0x04,         /* bDescriptorType - Interface Descriptor Type */
  0x00,         /* binterface_number - Number of Interface */
  0x00,         /* bAlternateSetting - Value used to select alternate setting */
  0x02,         /* bNumEndpoints - Nos of Endpoints used by this Interface */
  0xFF,         /* bInterfaceClass - Class code "Vendor Specific Class." */
  0x42,         /* bInterfaceSubClass - Subclass code "Vendor specific". */
  0x03,         /* bInterfaceProtocol - Protocol code "Vendor specific". */
  0x00,         /* iInterface - Index of String descriptor describing Interface */

  /* Endpoint Descriptor IN EP1 */
  0x07,         /* bLength - Size of this descriptor in bytes */
  0x05,         /* bDescriptorType - ENDPOINT Descriptor Type */
  0x81,         /* bEndpointAddress - The address of EP on the USB device  */
  0x02,         /* bmAttributes - Bit 1-0: Transfer Type 10: Bulk,  */
  0x00,         /* wMaxPacketSize(LSB) - Maximum Packet Size for EP */
  0x04,         /* wMaxPacketSize(MSB) - Maximum Packet Size for EP */
  0x00,         /* bInterval - interval for polling EP, for Int and Isochronous  */

  /* Endpoint IN Companion */
  0x6,          /* bLength - Size of this descriptor in bytes */
  0x30,         /* bDescriptorType - ENDPOINT companion Descriptor */
  0x1,          /* MaxBurst */
  0x0,          /* Attributes */
  0x0,          /* Interval */
  0x0,          /* Interval */

  /** Endpoint Descriptor OUT EP1 */
  0x07,         /* bLength - Size of this descriptor in bytes */
  0x05,         /* bDescriptorType - ENDPOINT Descriptor Type */
  0x01,         /* bEndpointAddress - The address of EP on the USB device  */
  0x02,         /* bmAttributes - Bit 1-0: Transfer Type 10: Bulk,  */
  0x00,         /* wMaxPacketSize(LSB) - Maximum Packet Size for EP */
  0x04,         /* wMaxPacketSize(MSB) - Maximum Packet Size for EP */
  0x00,         /* bInterval - interval for polling EP, for Int Isochronous  */

  /* Endpoint OUT Companion */
  0x6,          /* bLength - Size of this descriptor in bytes */
  0x30,         /* bDescriptorType - ENDPOINT companion Descriptor */
  0xF,          /* MaxBurst */
  0x0,          /* Attributes */
  0x0,          /* Interval */
  0x0,          /* Interval */
};

/** Hish speed config descriptor for fastboot protocol */
static UINT8  s_usb_hs_config_descr_fastboot[] = {
  /* Configuration Descriptor 32 bytes  */
  0x09,         /* bLength - Size of this descriptor in bytes */
  0x02,         /* bDescriptorType - Configuration Descriptor Type */
  0x20,         /* WTotalLength (LSB) - length of data for this configuration */
  0x00,         /* WTotalLength (MSB) - length of data for this configuration */
  0x01,         /* bNumInterface - Nos of Interface supported by configuration */
  0x01,         /* bConfigurationValue */
  0x00,         /* iConfiguration - Index of descriptor describing configuration */
  0xc0,         /* bmAttributes-bitmap "D4-D0:Res,D6:Self Powered,D5:Remote Wakeup*/
  0x10,         /* MaxPower in mA - Max Power Consumption of the USB device */

  /* Interface Descriptor */
  0x09,         /* bLength - Size of this descriptor in bytes */
  0x04,         /* bDescriptorType - Interface Descriptor Type */
  0x00,         /* binterface_number - Number of Interface */
  0x00,         /* bAlternateSetting - Value used to select alternate setting */
  0x02,         /* bNumEndpoints - Nos of Endpoints used by this Interface */
  0xFF,         /* bInterfaceClass - Class code "Vendor Specific Class." */
  0x42,         /* bInterfaceSubClass - Subclass code "Vendor specific". */
  0x03,         /* bInterfaceProtocol - Protocol code "Vendor specific". */
  0x00,         /* iInterface - Index of String descriptor describing Interface */

  /* Endpoint Descriptor IN EP1 */
  0x07,         /* bLength - Size of this descriptor in bytes */
  0x05,         /* bDescriptorType - ENDPOINT Descriptor Type */
  0x81,         /* bEndpointAddress - The address of EP on the USB device  */
  0x02,         /* bmAttributes - Bit 1-0: Transfer Type 10: Bulk,  */
  0x00,         /* wMaxPacketSize(LSB) - Maximum Packet Size for EP */
  0x02,         /* wMaxPacketSize(MSB) - Maximum Packet Size for EP */
  0x00,         /* bInterval - interval for polling EP, for Int and Isochronous  */

  /** Endpoint Descriptor OUT EP1 */
  0x07,         /* bLength - Size of this descriptor in bytes */
  0x05,         /* bDescriptorType - ENDPOINT Descriptor Type */
  0x01,         /* bEndpointAddress - The address of EP on the USB device  */
  0x02,         /* bmAttributes - Bit 1-0: Transfer Type 10: Bulk,  */
  0x00,         /* wMaxPacketSize(LSB) - Maximum Packet Size for EP */
  0x02,         /* wMaxPacketSize(MSB) - Maximum Packet Size for EP */
  0x00          /* bInterval - interval for polling EP, for Int Isochronous  */
};

/*
 * USB Device other speed Configuration Descriptors:
 * 32 bytes as per the USB2.0 Specification. This contains
 * Configuration descriptor, Interface descriptor and endpoint descriptors.
 */
static UINT8  s_other_speed_config_desc[] = {
  /* Configuration Descriptor 32 bytes  */
  0x09,         /* bLength - Size of this descriptor in bytes */
  0x07,         /* bDescriptorType - Configuration Descriptor Type */
  0x20,         /* (LSB)Total length of data for this configuration */
  0x00,         /* (MSB)Total length of data for this configuration */
  0x01,         /* bNumInterface - Nos of Interface supported */
  0x01,         /* bConfigurationValue */
  0x00,         /* iConfiguration - Index for this configuration */
  0xc0,         /* bmAttributes - */
  0x10,         /* MaxPower in mA - */

  /* Interface Descriptor */
  0x09,         /* bLength - Size of this descriptor in bytes */
  0x04,         /* bDescriptorType - Interface Descriptor Type */
  0x00,         /* bInterfaceNumber - Number of Interface */
  0x00,         /* bAlternateSetting - Value used to select alternate setting */
  0x02,         /* bNumEndpoints - Nos of Endpoints used by this Interface */
  0xFF,         /* bInterfaceClass - Class code "Vendor Specific Class." */
  0xFF,         /* bInterfaceSubClass - Subclass code "Vendor specific". */
  0xFF,         /* bInterfaceProtocol - Protocol code "Vendor specific". */
  0x00,         /* iInterface - Index of String descriptor describing Interface */

  /* Endpoint Descriptor IN EP2 */
  0x07,         /* bLength - Size of this descriptor in bytes */
  0x05,         /* bDescriptorType - ENDPOINT Descriptor Type */
  0x81,         /* bEndpointAddress - The address of EP on the USB device */
  0x02,         /* bmAttributes - Bit 1-0: Transfer Type 10: Bulk, */
  0x40,         /* wMaxPacketSize(LSB) - Maximum Packet Size for this EP */
  0x00,         /* wMaxPacketSize(MSB) - Maximum Packet Size for this EP */
  0x00,         /* bIntervel - */

  /** Endpoint Descriptor OUT EP1 */
  0x07,         /* bLength - Size of this descriptor in bytes */
  0x05,         /* bDescriptorType - ENDPOINT Descriptor Type */
  0x01,         /* bEndpointAddress - The address of EP on the USB device */
  0x02,         /* bmAttributes - Bit 1-0: Transfer Type 10: Bulk */
  0x40,         /* wMaxPacketSize(LSB) - Maximum Packet Size for this EP */
  0x00,         /* wMaxPacketSize(MSB) - Maximum Packet Size for this EP */
  0x00          /* bIntervel - */
};

/* Stores the Language ID Descriptor data */
static UINT8  s_usb_language_id[] = {
  /* Language Id string descriptor */
  4,                /* Length of descriptor */
  0x03,             /* STRING descriptor type. */
  0x09, 0x04        /* LANGID Code 0: American English 0x409 */
};

/* Stores the Manufactures ID sting descriptor data */
static const UINT8  s_usb_manufacturer_id[] = {
  0x1A, /* Length of descriptor */
  0x03, /* STRING descriptor type. */
  'N', 0,
  'V', 0,
  'I', 0,
  'D', 0,
  'I', 0,
  'A', 0,
  ' ', 0,
  'C', 0,
  'o', 0,
  'r', 0,
  'p', 0,
  '.', 0
};

/* Stores the Product ID string descriptor data */
static UINT8  s_usb_product_id_fastboot[] = {
  0x12, /* Length of descriptor */
  0x03, /* STRING descriptor type. */
  'F', 0x00,
  'a', 0x00,
  's', 0x00,
  't', 0x00,
  'b', 0x00,
  'o', 0x00,
  'o', 0x00,
  't', 0x00
};

/* Stores the Serial Number String descriptor data */
static UINT8  s_usb_serial_number[MAX_SERIALNO_LEN * 2 + 2] = {
  [0]  = 0xc,        /* Length of descriptor */
  [1]  = 0x03,       /* STRING descriptor type. */
  [2]  = '0', [3]  = 0x00,
  [4]  = '0', [5]  = 0x00,
  [6]  = '0', [7]  = 0x00,
  [8]  = '0', [9]  = 0x00,
  [10] = '0', [11] = 0x00
};

static struct tegrabl_usbf_config  config_fastboot = {
  .hs_device    = USB_DESC_STATIC (s_hs_device_descr),
  .ss_device    = USB_DESC_STATIC (s_ss_device_descr),
  .device_qual  = USB_DESC_STATIC (s_usb_device_qualifier),
  .ss_config    = USB_DESC_STATIC (s_usb_ss_config_descr_fastboot),
  .hs_config    = USB_DESC_STATIC (s_usb_hs_config_descr_fastboot),
  .other_config = USB_DESC_STATIC (s_other_speed_config_desc),
  .langid       = USB_DESC_STATIC (s_usb_language_id),
  .manufacturer = USB_DESC_STATIC (s_usb_manufacturer_id),
  .product      = USB_DESC_STATIC (s_usb_product_id_fastboot),
  .serialno     = USB_DESC_STATIC (s_usb_serial_number),
};

/**
 * USB BOS Descriptor:
 * Stores the Device descriptor data must be word aligned
 */
static UINT8  s_bos_descriptor[USB_BOS_DESCRIPTOR_SIZE] = {
  0x5,        /* bLength - Size of this descriptor in bytes */
  0xF,        /* bDescriptorType - BOS Descriptor Type */
  0x16,       /* wTotalLength LSB */
  0x0,        /* wTotalLength MSB */
  0x2,        /* NumDeviceCaps */

  0x7,        /* bLength - Size of USB2.0 extension Device Capability Descriptor */
  0x10,       /* bDescriptorType - Device Capability Type */
  0x2,        /* bDevCapabilityType - USB 2.0 Extention */
  0x2,        /* bmAttributes -Bit 1 Capable of generating Latency Tolerace Msgs */
  0x0,        /* Reserved */
  0x0,        /* Reserved */
  0x0,        /* Reserved */

  0xA,        /* bLength - Size of Super Speed Device Capability Descriptor */
  0x10,       /* bDescriptorType - Device Capability Type */
  0x3,        /* bDevCapabilityType - SUPER SPEED USB */
  0x0,        /* bmAttributes - Bit 1 Capable of generating Latency Tolerace Msgs */
  0xC,        /* wSpeedsSupported LSB - Device Supports High and Super speed's */
  0x0,        /* wSpeedsSupported MSB */
  0x2,        /* bFunctionalitySupport - All features available above FS. */
  0xA,        /* bU1DevExitLat - Less than 10us */
  0xFF,       /* wU2DevExitLat LSB */
  0x7,        /* wU2DevExitLat MSB - Less than 2047us */
};

/* Stores the Device status descriptor data */
static UINT8  s_usb_dev_status[USB_DEV_STATUS_LENGTH] = {
  USB_DEVICE_SELF_POWERED,
  0,
};

#endif /* XUSB_DEV_CONTROLLER_DESC_H_ */
