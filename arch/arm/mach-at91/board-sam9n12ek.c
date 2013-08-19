/*
 *  Board-specific setup code for the AT91SAM9N12 Evaluation Kit
 *
 *  Copyright (C) 2011 Atmel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/fb.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/leds.h>
#include <linux/clk.h>
#include <linux/atmel-mci.h>
#include <linux/delay.h>

#include <mach/hardware.h>
#include <video/atmel_lcdfb.h>

#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/atmel_lcdc.h>
#include <mach/atmel_hlcdc.h>
#include <mach/at91sam9_smc.h>
#include <mach/at91_shdwc.h>

#include "sam9_smc.h"
#include "generic.h"

static void __init ek_map_io(void)
{
	/* Initialize processor: 16.000 MHz crystal */
	at91sam9n12_initialize(16000000);

	/* DGBU on ttyS0. (Rx & Tx only) */
	at91_register_uart(0, 0, 0);

	/* eric REVISIT: check the connection on SAM9N12-EK */
	/* USART0 on ttyS1. (Rx, Tx, RTS, CTS) */
	/* USART1 on ttyS2. (Rx, Tx, RTS, CTS) */
	at91_register_uart(AT91SAM9N12_ID_USART0, 1, ATMEL_UART_CTS | ATMEL_UART_RTS);
	at91_register_uart(AT91SAM9N12_ID_USART1, 2, ATMEL_UART_CTS | ATMEL_UART_RTS);

	/* set serial console to ttyS0 (ie, DBGU) */
	at91_set_serial_console(0);
}

static void __init ek_init_irq(void)
{
	at91sam9n12_init_interrupts(NULL);
}

/*
 * USB FS Host port
 */
static struct at91_usbh_data __initdata ek_usbh_fs_data = {
	.ports		= 1,
	.vbus_pin	= { AT91_PIN_PB7 },
};

/*
 * USB FS Device port
 */
static struct at91_udc_data __initdata ek_udc_data = {
	.vbus_pin	= AT91_PIN_PB16,
};

/*
 * SPI devices.
 */
static struct spi_board_info ek_spi_devices[] = {
	{	/* DataFlash chip */
		.modalias	= "mtd_dataflash",
		.chip_select	= 0,
		.max_speed_hz	= 15 * 1000 * 1000,
		.bus_num	= 0,
	},
};

/*
 * MCI (SD/MMC)
 */
static struct mci_platform_data __initdata mci_data = {
	.slot[0] = {
		.bus_width	= 4,
		.detect_pin	= AT91_PIN_PA7,
	},
};

/*
 * NAND flash
 */
static struct mtd_partition __initdata ek_nand_partition[] = {
	{
		.name	= "Partition 1",
		.offset	= 0,
		.size	= SZ_64M,
	},
	{
		.name	= "Partition 2",
		.offset	= MTDPART_OFS_NXTBLK,
		.size	= MTDPART_SIZ_FULL,
	},
};

static struct mtd_partition * __init nand_partitions(int size, int *num_partitions)
{
	*num_partitions = ARRAY_SIZE(ek_nand_partition);
	return ek_nand_partition;
}

/* det_pin is not connected */
static struct atmel_nand_data __initdata ek_nand_data = {
	.ale		= 21,
	.cle		= 22,
	.rdy_pin	= AT91_PIN_PD5,
	.enable_pin	= AT91_PIN_PD4,
	.partition_info	= nand_partitions,
	.bus_on_d0 = 1,
#if defined(CONFIG_MTD_NAND_ATMEL_BUSWIDTH_16)
	.bus_width_16	= 1,
#else
	.bus_width_16	= 0,
#endif
};

static struct sam9_smc_config __initdata ek_nand_smc_config = {
	.ncs_read_setup		= 0,
	.nrd_setup		= 2,
	.ncs_write_setup	= 0,
	.nwe_setup		= 1,

	.ncs_read_pulse		= 6,
	.nrd_pulse		= 4,
	.ncs_write_pulse	= 5,
	.nwe_pulse		= 3,

	.read_cycle		= 7,
	.write_cycle		= 5,

	.mode			= AT91_SMC_READMODE | AT91_SMC_WRITEMODE | AT91_SMC_EXNWMODE_DISABLE,
	.tdf_cycles		= 1,
};

static void __init ek_add_device_nand(void)
{
	/* setup bus-width (8 or 16) */
	if (ek_nand_data.bus_width_16)
		ek_nand_smc_config.mode |= AT91_SMC_DBW_16;
	else
		ek_nand_smc_config.mode |= AT91_SMC_DBW_8;

	/* configure chip-select 3 (NAND) */
	sam9_smc_configure(3, &ek_nand_smc_config);

	at91_add_device_nand(&ek_nand_data);
}

/*
 * LCD Controller
 */
#if defined(CONFIG_FB_ATMEL) || defined(CONFIG_FB_ATMEL_MODULE)
static struct fb_videomode at91_tft_vga_modes[] = {
	{
		.name           = "LG",
		.refresh	= 60,
		.xres		= 480,		.yres		= 272,
		.pixclock	= KHZ2PICOS(9000),

		.left_margin	= 1,		.right_margin	= 1,
		.upper_margin	= 40,		.lower_margin	= 1,
		.hsync_len	= 45,		.vsync_len	= 1,

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
#define AT91SAM9N12_DEFAULT_LCDCFG5	(LCDC_LCDCFG5_MODE_OUTPUT_24BPP)

/* Driver datas */
static struct atmel_lcdfb_info __initdata ek_lcdc_data = {
	.lcdcon_is_backlight		= true,
	.alpha_enabled			= false,
	.default_bpp			= 16,
	/* Reserve enough memory for 32bpp */
	.smem_len			= 800 * 480 * 4,
	/* In AT91SAM9N12 default_lcdcon2 is used for LCDCFG5 */
	.default_lcdcon2		= AT91SAM9N12_DEFAULT_LCDCFG5,
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
 * GPIO Buttons
 */
#if defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE)
static struct gpio_keys_button ek_buttons[] = {
	{	/* BP1, "leftclic" */
		.code		= BTN_LEFT,
		.gpio		= AT91_PIN_PB6,
		.active_low	= 1,
		.desc		= "left_click",
		.wakeup		= 1,
	},
	{	/* BP2, "rightclic" */
		.code		= BTN_RIGHT,
		.gpio		= AT91_PIN_PB7,
		.active_low	= 1,
		.desc		= "right_click",
		.wakeup		= 1,
	},
		/* BP3, "joystick" */
	{
		.code		= KEY_LEFT,
		.gpio		= AT91_PIN_PB14,
		.active_low	= 1,
		.desc		= "Joystick Left",
	},
	{
		.code		= KEY_RIGHT,
		.gpio		= AT91_PIN_PB15,
		.active_low	= 1,
		.desc		= "Joystick Right",
	},
	{
		.code		= KEY_UP,
		.gpio		= AT91_PIN_PB16,
		.active_low	= 1,
		.desc		= "Joystick Up",
	},
	{
		.code		= KEY_DOWN,
		.gpio		= AT91_PIN_PB17,
		.active_low	= 1,
		.desc		= "Joystick Down",
	},
	{
		.code		= KEY_ENTER,
		.gpio		= AT91_PIN_PB18,
		.active_low	= 1,
		.desc		= "Joystick Press",
	},
};

static struct gpio_keys_platform_data ek_button_data = {
	.buttons	= ek_buttons,
	.nbuttons	= ARRAY_SIZE(ek_buttons),
};

static struct platform_device ek_button_device = {
	.name		= "gpio-keys",
	.id		= -1,
	.num_resources	= 0,
	.dev		= {
		.platform_data	= &ek_button_data,
	}
};

static void __init ek_add_device_buttons(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ek_buttons); i++) {
		at91_set_GPIO_periph(ek_buttons[i].gpio, 1);
		at91_set_deglitch(ek_buttons[i].gpio, 1);
	}

	platform_device_register(&ek_button_device);
}
#else
static void __init ek_add_device_buttons(void) {}
#endif

/*
 * LEDs ... these could all be PWM-driven, for variable brightness
 */
static struct gpio_led ek_leds[] = {
	{	/* "top" led, red, powerled */
		.name			= "d8",
		.gpio			= AT91_PIN_PD30,
		.default_trigger	= "heartbeat",
	},
	{	/* "left" led, green, userled2, pwm3 */
		.name			= "d6",
		.gpio			= AT91_PIN_PD0,
		.active_low		= 1,
		.default_trigger	= "nand-disk",
	},
#if !(defined(CONFIG_LEDS_ATMEL_PWM) || defined(CONFIG_LEDS_ATMEL_PWM_MODULE))
	{	/* "right" led, green, userled1, pwm1 */
		.name			= "d7",
		.gpio			= AT91_PIN_PD31,
		.active_low		= 1,
		.default_trigger	= "mmc0",
	},
#endif
};

/*
 * PWM Leds
 */
static struct gpio_led ek_pwm_led[] = {
#if defined(CONFIG_LEDS_ATMEL_PWM) || defined(CONFIG_LEDS_ATMEL_PWM_MODULE)
	{	/* "right" led, green, userled1, pwm1 */
		.name			= "d7",
		.gpio			= 1,	/* is PWM channel number */
		.active_low		= 1,
		.default_trigger	= "none",
	},
#endif
};

static void __init ek_board_init(void)
{
	/* Serial */
	at91_add_device_serial();
	/* USB FS Host */
	at91_add_device_usbh_ohci(&ek_usbh_fs_data);
	/* USB FS Device */
	at91_add_device_udc(&ek_udc_data);
	/* SPI */
	at91_add_device_spi(ek_spi_devices, ARRAY_SIZE(ek_spi_devices));
	/* MMC */
	at91_add_device_mci(0, &mci_data);
	/* NAND */
	ek_add_device_nand();
	/* I2C */
	at91_add_device_i2c(0, NULL, 0);
	/* LCD Controller */
	at91_add_device_lcdc(&ek_lcdc_data);
	/* Touch Screen */
	at91_add_device_tsadcc(&ek_tsadcc_data);
	/* Push Buttons */
	ek_add_device_buttons();
	/* LEDs */
	at91_gpio_leds(ek_leds, ARRAY_SIZE(ek_leds));
	at91_pwm_leds(ek_pwm_led, ARRAY_SIZE(ek_pwm_led));
}

MACHINE_START(AT91SAM9N12EK, "Atmel AT91SAM9N12-EK")
	/* Maintainer: Atmel */
	.boot_params	= AT91_SDRAM_BASE + 0x100,
	.timer		= &at91sam926x_timer,
	.map_io		= ek_map_io,
	.init_irq	= ek_init_irq,
	.init_machine	= ek_board_init,
MACHINE_END
