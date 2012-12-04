/*
 *  Board-specific setup code for the AT91SAM9x5 Evaluation Kit family
 *
 *  Copyright (C) 2010 Atmel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

/*
 *  CPU module specific setup code for the AT91SAM9x5 family
 *
 *  Copyright (C) 2011 Atmel Corporation.
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

#undef cm_is_revA
#undef ek_is_revA

void __init cm_map_io(void)
{
	/* Initialize processor: 12.000 MHz crystal */
	at91sam9x5_initialize(12000000);

	/* DGBU on ttyS0. (Rx & Tx only) */
	at91_register_uart(0, 0, 0);

	/* set serial console to ttyS0 (ie, DBGU) */
	at91_set_serial_console(0);
}

void __init cm_init_irq(void)
{
	at91sam9x5_init_interrupts(NULL);
}

/*
 * SPI devices.
 */

/* det_pin is not connected */
static struct atmel_nand_data __initdata cm_nand_data = {
	.ale		= 21,
	.cle		= 22,
	.enable_pin	= AT91_PIN_PD4,
#if defined(CONFIG_MTD_NAND_AT91_BUSWIDTH_16)
	.bus_width_16	= 1,
#endif
};

static struct sam9_smc_config __initdata cm_nand_smc_config = {
	.ncs_read_setup		= 0,
	.nrd_setup		= 1,
	.ncs_write_setup	= 0,
	.nwe_setup		= 1,

	.ncs_read_pulse		= 6,
	.nrd_pulse		= 4,
	.ncs_write_pulse	= 5,
	.nwe_pulse		= 3,

	.read_cycle		= 6,
	.write_cycle		= 5,

	.mode			= AT91_SMC_READMODE | AT91_SMC_WRITEMODE | AT91_SMC_EXNWMODE_DISABLE,
	.tdf_cycles		= 1,
};

static void __init cm_add_device_nand(void)
{
	cm_nand_smc_config.mode |= AT91_SMC_DBW_8;

	cm_nand_data.bus_on_d0 = 0;
	cm_nand_data.rdy_pin = AT91_PIN_PD5;

	/* configure chip-select 3 (NAND) */
	sam9_smc_configure(3, &cm_nand_smc_config);

	at91_add_device_nand(&cm_nand_data);
}

/*
 * LEDs
 */
static struct gpio_led cm_leds[] = {
	{	/* "left" led, blue, userled1 */
		.name			= "d1",
		.gpio			= AT91_PIN_PB18,
		.default_trigger	= "heartbeat",
	},
	{	/* "right" led, red, userled2 */
		.name			= "d2",
		.gpio			= AT91_PIN_PD21,
		.active_low		= 1,
		.default_trigger	= "mmc0",
	},
};

/*
 * I2C Devices
 */
static struct i2c_board_info __initdata cm_i2c_devices[] = {
	{
		I2C_BOARD_INFO("24c512", 0x50)
	},
};

void __init cm_board_init(u32 *cm_config)
{
	*cm_config = 0;
	/* NAND */
	cm_add_device_nand();
	/* I2C */
	at91_add_device_i2c(0, cm_i2c_devices, ARRAY_SIZE(cm_i2c_devices));
	*cm_config |= CM_CONFIG_I2C0_ENABLE;
	/* LEDs */
	at91_gpio_leds(cm_leds, ARRAY_SIZE(cm_leds));

	printk(KERN_CRIT "AT91: CM rev B and higher\n");
}


static void __init ek_map_io(void)
{
	/* Initialize processor and DBGU */
	cm_map_io();

	/* USART0 on ttyS1. (Rx, Tx) */
	at91_register_uart(AT91SAM9X5_ID_USART0, 1, 0);
}

/*
 * USB Host port (OHCI)
 */
/* Port A is shared with gadget port & Port C is full-speed only */
static struct at91_usbh_data __initdata ek_usbh_fs_data = {
	.ports		= 3,

};

/*
 * USB HS Host port (EHCI)
 */
/* Port A is shared with gadget port */
static struct at91_usbh_data __initdata ek_usbh_hs_data = {
	.ports		= 2,
};


/*
 * USB HS Device port
 */
static struct usba_platform_data __initdata ek_usba_udc_data;


/*
 * MACB Ethernet devices
 */
static struct at91_eth_data __initdata ek_macb0_data = {
	.is_rmii	= 1,
};

static struct at91_eth_data __initdata ek_macb1_data = {
	.phy_irq_pin	= AT91_PIN_PC26,
	.is_rmii	= 1,
};


/*
 * MCI (SD/MMC)
 */
/* mci0 detect_pin is revision dependent */
static struct mci_platform_data __initdata mci0_data = {
	.slot[0] = {
		.bus_width	= 4,
		.wp_pin		= -1,
	},
};

static struct mci_platform_data __initdata mci1_data = {
	.slot[0] = {
		.bus_width	= 4,
		.detect_pin	= AT91_PIN_PD14,
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
	.vendor		= "ATMEL",
	.product	= "SAM9X5",
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
	.vendorDescr	= "ATMEL",
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
	.product_name		= "SAM9X5",
	.manufacturer_name	= "ATMEL",
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

/*
 * Touchscreen
 */
static struct at91_tsadcc_data ek_tsadcc_data = {
	.adc_clock		= 300000,
	.filtering_average	= 0x03,	/* averages 2^filtering_average ADC conversions */
	.pendet_debounce	= 0x08,
	.pendet_sensitivity	= 0x02,	/* 2 = set to default */
	.ts_sample_hold_time	= 0x0a,
};

/*
 * I2C Devices
 */
static struct i2c_board_info __initdata ek_i2c_devices[] = {
	{
		I2C_BOARD_INFO("wm8731", 0x1a)
	},
#if defined(CONFIG_KEYBOARD_QT1070)
	{
		I2C_BOARD_INFO("qt1070", 0x1b),
		.irq = AT91_PIN_PA7,
		.flags = I2C_CLIENT_WAKE,
	},
#endif
};

static void __init ek_board_configure_pins(void)
{
	/* Port A is shared with gadget port */
	/*ek_usbh_fs_data.vbus_pin[0] = AT91_PIN_PD18;*/
	/*ek_usbh_hs_data.vbus_pin[0] = AT91_PIN_PD18;*/
	ek_usbh_fs_data.vbus_pin[1] = AT91_PIN_PD19;
	ek_usbh_hs_data.vbus_pin[1] = AT91_PIN_PD19;
	/* Port C is full-speed only */
	ek_usbh_fs_data.vbus_pin[2] = AT91_PIN_PD20;

	ek_usba_udc_data.vbus_pin = AT91_PIN_PB16;

	ek_macb0_data.phy_irq_pin = AT91_PIN_PB8;

	mci0_data.slot[0].detect_pin = AT91_PIN_PD15;

#if defined(CONFIG_KEYBOARD_QT1070)
		if (!cpu_is_at91sam9g25())
			/* conflict with ISI */
			at91_set_gpio_input(ek_i2c_devices[1].irq, 1);
#endif
}

/*
 * BATTERY
 */
static struct platform_device battery = {
	.name = "dummy-battery",
	.id   = -1,
};

static void __init at91_init_battery(void)
{
	platform_device_register(&battery);
}

static void __init ek_board_init(void)
{
	u32 cm_config;

	cm_board_init(&cm_config);
	ek_board_configure_pins();
	/* Serial */
	at91_add_device_serial();
	/* USB HS Host */
	at91_add_device_usbh_ohci(&ek_usbh_fs_data);
	at91_add_device_usbh_ehci(&ek_usbh_hs_data);
	/* USB HS Device */
	at91_add_device_usba(&ek_usba_udc_data);
	/* Ethernet */
	at91_add_device_eth(0, &ek_macb0_data);
	at91_add_device_eth(1, &ek_macb1_data);
	/* MMC */
	at91_add_device_mci(0, &mci0_data);
	/* Conflict between SPI0 and MCI1 pins */
	if (!(cm_config & CM_CONFIG_SPI0_ENABLE))
		at91_add_device_mci(1, &mci1_data);
	/* I2C */
	if (cm_config & CM_CONFIG_I2C0_ENABLE)
		i2c_register_board_info(0,
				ek_i2c_devices, ARRAY_SIZE(ek_i2c_devices));
	else
		at91_add_device_i2c(0,
				ek_i2c_devices, ARRAY_SIZE(ek_i2c_devices));


	/* LCD Controller */
	at91_add_device_lcdc(&ek_lcdc_data);
	/* Touch Screen */
	at91_add_device_tsadcc(&ek_tsadcc_data);

	/* SSC (for WM8731) */
	at91_add_device_ssc(AT91SAM9X5_ID_SSC, ATMEL_SSC_TX | ATMEL_SSC_RX);

	at91_init_battery();

	printk(KERN_CRIT "AT91: EK rev B and higher\n");

	/*usb adb*/
	at91_usb_adb_init();
}

MACHINE_START(AT91SAM9X5EK, "CryoCon AT91SAM9X5-EK with sam9g35")
	/* Maintainer: Atmel */
/* XXX/ukl: can we drop .boot_params? */
	.boot_params	= AT91_SDRAM_BASE + 0x100,
	.timer		= &at91sam926x_timer,
	.map_io		= ek_map_io,
	.init_irq	= cm_init_irq,
	.init_machine	= ek_board_init,
MACHINE_END
