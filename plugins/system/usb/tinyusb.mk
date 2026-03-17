
# C source files
TINYUSB_SRC_C += \
	tinyusb/src/tusb.c \
	tinyusb/src/common/tusb_fifo.c \
	tinyusb/src/device/usbd.c \
	tinyusb/src/device/usbd_control.c \
	tinyusb/src/class/cdc/cdc_device.c \
	tinyusb/src/portable/raspberrypi/rp2040/dcd_rp2040.c \
	tinyusb/src/portable/raspberrypi/rp2040/rp2040_usb.c

TINYUSB_INCLUDE_DIRS += \
	tinyusb/src \
	tinyusb/src/common \
	tinyusb/src/device \
	tinyusb/src/class/cdc \
	tinyusb/src/portable/raspberrypi/rp2040