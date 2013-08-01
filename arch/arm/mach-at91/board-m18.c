/*
 *  Setup code for the Cryocon Model 18 with AT91SAM9G35
 *
 *  Copyright (C) 2012 Cryogenic Control Systems, Inc.
 *
 *  From AT91SAM9x5-EK setup code, copyright (C) 2010 Atmel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/gpio_keys.h>
#include <linux/i2c/at24.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/leds.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/flash.h>
#include <linux/spi/spi.h>
#include <linux/types.h>
#include <linux/usb/android_composite.h>
#include <linux/etherdevice.h>
#include <mach/cpu.h>

#include <video/atmel_lcdfb.h>

#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <asm/mach/map.h>

#include <mach/hardware.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/at91_shdwc.h>
#include <mach/at91sam9_smc.h>
#include <mach/atmel_hlcdc.h>

#include "sam9_smc.h"
#include "generic.h"
#include <mach/board-sam9x5.h>

void __init m18_map_io(void)
{
	/* Initialize processor: 12.000 MHz crystal */
	at91sam9x5_initialize(12000000);

	/* DGBU on ttyS0. (Rx & Tx only) */
	at91_register_uart(0, 0, 0);

	/* set serial console to ttyS0 (ie, DBGU) */
	at91_set_serial_console(0);

	/* USART1 on ttyS1. (Rx, Tx) */
	at91_register_uart(AT91SAM9X5_ID_USART1, 1, 0);
}

void __init m18_init_irq(void)
{
	at91sam9x5_init_interrupts(NULL);
}

/*
 * SPI devices.
 */

static struct spi_board_info m18_spi_devices[] = {
	{
		.bus_num	= 0,
		.chip_select	= 0,
		.modalias	= "spidev",
		.max_speed_hz	= 1000 * 1000,
		.mode		= SPI_MODE_1,
		.irq            = -1,
		.controller_data = AT91_PIN_PD7,
	},
	{
		.bus_num	= 1,
		.chip_select	= 0,
		.modalias	= "spidev",
		.max_speed_hz	= 3 * 1000 * 1000,
		.mode		= SPI_MODE_0,
		.irq            = -1,
	},
};


/*
 * LEDs
 */
static struct gpio_led m18_leds[] = {
	{
		.name			= "ledrx",
		.gpio			= AT91_PIN_PD1,
		.active_low		= 1,
		.default_trigger	= "heartbeat",
	},
};

/*
 * GPIO Buttons
 */
static struct gpio_keys_button m18_buttons[] = {
	{	/* KBUpn */
		.code		= KEY_UP,
		.gpio		= AT91_PIN_PD18,
		.active_low	= 1,
		.desc		= "up",
		.wakeup		= 1,
	},
	{	/* KBDown */
		.code		= KEY_DOWN,
		.gpio		= AT91_PIN_PD19,
		.active_low	= 1,
		.desc		= "down",
		.wakeup		= 1,
	},
	{	/* KBRightn */
		.code		= KEY_RIGHT,
		.gpio		= AT91_PIN_PD20,
		.active_low	= 1,
		.desc		= "right",
		.wakeup		= 1,
	},
	{	/* KBSelectn */
		.code		= KEY_SELECT,
		.gpio		= AT91_PIN_PD21,
		.active_low	= 1,
		.desc		= "select",
		.wakeup		= 1,
	},
};

static struct gpio_keys_platform_data m18_button_data = {
	.buttons	= m18_buttons,
	.nbuttons	= ARRAY_SIZE(m18_buttons),
};

static struct platform_device m18_button_device = {
	.name		= "gpio-keys",
	.id		= -1,
	.num_resources	= 0,
	.dev		= {
		.platform_data	= &m18_button_data,
	}
};

static void __init m18_add_device_buttons(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(m18_buttons); i++) {
		at91_set_pulldown(m18_buttons[i].gpio, 0);
		at91_set_gpio_input(m18_buttons[i].gpio, 1);
		at91_set_deglitch(m18_buttons[i].gpio, 1);
	}

	platform_device_register(&m18_button_device);
}

/*
 * I2C Devices
 */


/*
 * MACB Ethernet devices
 */
static struct at91_eth_data __initdata m18_macb0_data = {
	.is_rmii	= 1,
};

static void m18_get_mac_addr(struct memory_accessor *mem_acc, void *context)
{
	char *mac_addr = m18_macb0_data.mac_addr;
	off_t offset = (off_t)context;

	if (is_valid_ether_addr(mac_addr)) {
		return;
	}
	/* Read MAC addr from EEPROM */
	int bytes = mem_acc->read(mem_acc, mac_addr, offset, ETH_ALEN);
	if (bytes != ETH_ALEN) {
		pr_warning("Failed to read MAC addr from EEPROM: %d\n", bytes);
		return;
	}
	pr_info("Read MAC addr from EEPROM: %pM\n", mac_addr);
	at91_add_device_eth(0, &m18_macb0_data);
}

static struct at24_platform_data eeprom_info = {
	.byte_len	= SZ_512K / 8,
	.page_size	= 128,
	.flags		= AT24_FLAG_ADDR16,
	.setup      = m18_get_mac_addr,
	.context	= (void *)0,		// EEPROM address containing MAC address
};


static struct i2c_board_info __initdata m18_i2c_devices[] = {
#if defined(CONFIG_KEYBOARD_QT1070)
	{
		I2C_BOARD_INFO("qt1070", 0x1b),
		.irq = AT91_PIN_PA7,
		.flags = I2C_CLIENT_WAKE,
	},
#endif
	{
		I2C_BOARD_INFO("24c512", 0x50),
		.platform_data=&eeprom_info,
	},
	{
		I2C_BOARD_INFO("24c512", 0x51),
		.platform_data=&eeprom_info,
	},
	{
		I2C_BOARD_INFO("lm73",0x48)
	},
	{
		I2C_BOARD_INFO("lm73",0x49)
	},
	{
		I2C_BOARD_INFO("lm73",0x4a)
	},
};


/*
 * USB HS Device port
 */
static struct usba_platform_data __initdata m18_usba_udc_data;

/*
 * MCI (SD/MMC)
 */
static struct mci_platform_data __initdata mci0_data = {
	.slot[0] = {
		.bus_width	= 4,
		.wp_pin		= -1,
	},
};

/*
  *  Android ADB
  */
static char *usb_functions_ums[] = {
	"usb_mass_storage",
};

static char *usb_functions_ums_adb[] = {
	"usb_mass_storage",
	"adb",
};

static char *usb_functions_rndis[] = {
	"rndis",
};

static char *usb_functions_rndis_adb[] = {
	"rndis",
	"adb",
};

#ifdef CONFIG_USB_ANDROID_DIAG
static char *usb_functions_adb_diag[] = {
	"usb_mass_storage",
	"adb",
	"diag",
};
#endif

static char *usb_functions_all[] = {
#ifdef CONFIG_USB_ANDROID_RNDIS
	"rndis",
#endif
	"usb_mass_storage",
	"adb",
#ifdef CONFIG_USB_ANDROID_ACM
	"acm",
#endif
#ifdef CONFIG_USB_ANDROID_DIAG
	"diag",
#endif
};

static struct android_usb_product usb_products[] = {
	{
		.product_id	= 0x6129,
		.num_functions	= ARRAY_SIZE(usb_functions_ums),
		.functions	= usb_functions_ums,
	},
	{
		.product_id	= 0x6155,
		.num_functions	= ARRAY_SIZE(usb_functions_ums_adb),
		.functions	= usb_functions_ums_adb,
	},
	{
		.product_id	= 0x6156,
		.num_functions	= ARRAY_SIZE(usb_functions_rndis),
		.functions	= usb_functions_rndis,
	},
	{
		.product_id	= 0x6157,
		.num_functions	= ARRAY_SIZE(usb_functions_rndis_adb),
		.functions	= usb_functions_rndis_adb,
	},
#ifdef CONFIG_USB_ANDROID_DIAG
	{
		.product_id	= 0x6158,
		.num_functions	= ARRAY_SIZE(usb_functions_adb_diag),
		.functions	= usb_functions_adb_diag,
	},
#endif
};

static struct usb_mass_storage_platform_data mass_storage_pdata = {
	.nluns		= 1,
	.vendor		= "CRYOCON",
	.product	= "M18",
	.release	= 0x0100,
};

static struct platform_device usb_mass_storage_device = {
	.name	= "usb_mass_storage",
	.id	= -1,
	.dev	= {
		.platform_data = &mass_storage_pdata,
	},
};

#ifdef CONFIG_USB_ANDROID_RNDIS
static struct usb_ether_platform_data rndis_pdata = {
	/* ethaddr is filled by board_serialno_setup */
	.vendorID	= 0x03EB,
	.vendorDescr	= "CRYOCON",
};

static struct platform_device rndis_device = {
	.name	= "rndis",
	.id	= -1,
	.dev	= {
		.platform_data = &rndis_pdata,
	},
};
#endif

static struct android_usb_platform_data android_usb_pdata = {
	.vendor_id	= 0x03EB,
	.product_id	= 0x6129,
	.version	= 0x0100,
	.product_name		= "M18",
	.manufacturer_name	= "CRYOCON",
	.num_products = ARRAY_SIZE(usb_products),
	.products = usb_products,
	.num_functions = ARRAY_SIZE(usb_functions_all),
	.functions = usb_functions_all,
};

static struct platform_device android_usb_device = {
	.name	= "android_usb",
	.id		= -1,
	.dev		= {
		.platform_data = &android_usb_pdata,
	},
};

static void __init at91_usb_adb_init(void){
	platform_device_register(&android_usb_device);
	platform_device_register(&usb_mass_storage_device);
}

/*
 * LCD Controller
 */
#if defined(CONFIG_FB_ATMEL) || defined(CONFIG_FB_ATMEL_MODULE)
static struct fb_videomode at91_tft_vga_modes[] = {
	{
		.name           = "LG",
		.refresh	= 60,
		.xres		= 800,		.yres		= 480,
		.pixclock	= KHZ2PICOS(33260),

		.left_margin	= 88,		.right_margin	= 168,
		.upper_margin	= 8,		.lower_margin	= 37,
		.hsync_len	= 128,		.vsync_len	= 2,

		.sync		= 0,
		.vmode		= FB_VMODE_NONINTERLACED,
	},
};

static struct fb_monspecs at91fb_default_monspecs = {
	.manufacturer	= "LG",
	.monitor        = "LB043WQ1",

	.modedb		= at91_tft_vga_modes,
	.modedb_len	= ARRAY_SIZE(at91_tft_vga_modes),
	.hfmin		= 15000,
	.hfmax		= 17640,
	.vfmin		= 57,
	.vfmax		= 67,
};

/* Default output mode is TFT 24 bit */
#define AT91SAM9X5_DEFAULT_LCDCFG5	(LCDC_LCDCFG5_MODE_OUTPUT_24BPP)

/* Driver datas */
static struct atmel_lcdfb_info __initdata ek_lcdc_data = {
	.lcdcon_is_backlight		= true,
	.alpha_enabled			= false,
	.default_bpp			= 16,
	/* Reserve enough memory for 32bpp */
	.smem_len			= 800 * 480 * 4,
	/* In 9x5 default_lcdcon2 is used for LCDCFG5 */
	.default_lcdcon2		= AT91SAM9X5_DEFAULT_LCDCFG5,
	.default_monspecs		= &at91fb_default_monspecs,
	.guard_time			= 9,
	.lcd_wiring_mode		= ATMEL_LCDC_WIRING_RGB,
};

#else
static struct atmel_lcdfb_info __initdata ek_lcdc_data;
#endif

static void __init m18_board_init(void)
{
	at91_set_gpio_output(AT91_PIN_PB11, 1);	// Set PHY RST# (pulled down) to HIGH

	/* I2C */
	at91_add_device_i2c(0, m18_i2c_devices, ARRAY_SIZE(m18_i2c_devices));
	/* LCD Controller */
	at91_add_device_lcdc(&ek_lcdc_data);
	/* LEDs */
	at91_gpio_leds(m18_leds, ARRAY_SIZE(m18_leds));
	/* Pins */
	m18_usba_udc_data.vbus_pin = AT91_PIN_PB16;
	/* Serial */
	at91_add_device_serial();
	/* USB HS Device */
	at91_add_device_usba(&m18_usba_udc_data);
	/* MMC */
	at91_add_device_mci(0, &mci0_data);
	/* SPI */
	at91_set_gpio_output(AT91_PIN_PA1, 0);
	at91_set_gpio_output(AT91_PIN_PA14, 1);
#ifndef CONFIG_KEYBOARD_QT1070
	at91_set_gpio_output(AT91_PIN_PA7, 1);
#else
	gpio_request_one(m18_i2c_devices[0].irq, GPIOF_IN, "QT1070 IRQ");
	at91_set_gpio_input(m18_i2c_devices[0].irq, 1);
#warning Pin PA7 dedicated to QT1070 IRQ, SPI0 CS1 disabled!
#endif
	at91_set_gpio_output(AT91_PIN_PA0, 1);
	at91_set_gpio_output(AT91_PIN_PD17, 1);
	at91_set_gpio_output(AT91_PIN_PA25, 1);
	at91_set_gpio_input(AT91_PIN_PB18, 0);
	at91_add_device_spi(m18_spi_devices, ARRAY_SIZE(m18_spi_devices));
	/* Buttons */
	m18_add_device_buttons();

	printk(KERN_CRIT "AT91: Cryocon M18\n");

	/*usb adb*/
	at91_usb_adb_init();
}

MACHINE_START(AT91SAM9X5EK, "CryoCon M18 with sam9g35")
	/* Maintainer: Atmel */
/* XXX/ukl: can we drop .boot_params? */
	.boot_params	= AT91_SDRAM_BASE + 0x100,
	.timer		= &at91sam926x_timer,
	.map_io		= m18_map_io,
	.init_irq	= m18_init_irq,
	.init_machine	= m18_board_init,
MACHINE_END
