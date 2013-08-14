/*
 *  Chip-specific setup code for the AT91SAM9N12 SoC
 *
 *  Copyright (C) 2011 Atmel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/pm.h>

#include <asm/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <mach/at91sam9n12.h>
#include <mach/at91_pmc.h>
#include <mach/at91_rstc.h>
#include <mach/at91_shdwc.h>
#include <mach/cpu.h>

#include "generic.h"
#include "clock.h"

static struct map_desc at91sam9n12_io_desc[] __initdata = {
	{
		.virtual	= AT91_VA_BASE_SYS,
		.pfn		= __phys_to_pfn(AT91_BASE_SYS),
		.length		= SZ_16K,
		.type		= MT_DEVICE,
	}, {
		.virtual	= AT91_IO_VIRT_BASE - AT91SAM9N12_SRAM_SIZE,
		.pfn		= __phys_to_pfn(AT91SAM9N12_SRAM_BASE),
		.length		= AT91SAM9N12_SRAM_SIZE,
		.type		= MT_DEVICE,
	}
};

/* --------------------------------------------------------------------
 *  Clocks
 * -------------------------------------------------------------------- */

/*
 * The peripheral clocks.
 */
static struct clk pioAB_clk = {
	.name		= "pioAB_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_PIOAB,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk pioCD_clk = {
	.name		= "pioCD_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_PIOCD,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk usart0_clk = {
	.name		= "usart0_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_USART0,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk usart1_clk = {
	.name		= "usart1_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_USART1,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk usart2_clk = {
	.name		= "usart2_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_USART2,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk usart3_clk = {
	.name		= "usart3_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_USART3,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk twi0_clk = {
	.name		= "twi0_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_TWI0,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk twi1_clk = {
	.name		= "twi1_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_TWI1,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk mmc_clk = {
	.name		= "mci_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_MCI,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk spi0_clk = {
	.name		= "spi0_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_SPI0,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk spi1_clk = {
	.name		= "spi1_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_SPI1,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk uart0_clk = {
	.name		= "uart0_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_UART0,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk uart1_clk = {
	.name		= "uart1_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_UART1,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk tcb0_clk = {
	.name		= "tcb0_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_TCB,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk pwm_clk = {
	.name		= "pwm_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_PWM,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk adc_clk = {
	.name		= "adc_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_ADC,
	.type	= CLK_TYPE_PERIPHERAL,
};
static struct clk dma_clk = {
	.name		= "dma_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_DMA,
	.type	= CLK_TYPE_PERIPHERAL,
};
static struct clk uhpfs_clk = {
	.name		= "uhpfs_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_UHPFS,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk udc_clk = {
	.name		= "udc_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_UDPFS,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk lcdc_clk = {
	.name		= "lcdc_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_LCDC,
	.type		= CLK_TYPE_PERIPHERAL,
};
static struct clk ssc_clk = {
	.name		= "ssc_clk",
	.pmc_mask	= 1 << AT91SAM9N12_ID_SSC,
	.type		= CLK_TYPE_PERIPHERAL,
};
/* One additional fake clock for ohci */
static struct clk ohci_clk = {
	.name		= "ohci_clk",
	.pmc_mask	= 0,
	.type		= CLK_TYPE_PERIPHERAL,
	.parent		= &uhpfs_clk,
};

/* One additional fake clock for second TC block */
static struct clk tcb1_clk = {
	.name		= "tcb1_clk",
	.pmc_mask	= 0,
	.type		= CLK_TYPE_PERIPHERAL,
	.parent		= &tcb0_clk,
};

static struct clk *periph_clocks[] __initdata = {
	&pioAB_clk,
	&pioCD_clk,
	&usart0_clk,
	&usart1_clk,
	&usart2_clk,
	&usart3_clk,
	&twi0_clk,
	&twi1_clk,
	&mmc_clk,
	&spi0_clk,
	&spi1_clk,
	&uart0_clk,
	&uart1_clk,
	&tcb0_clk,
	&pwm_clk,
	&adc_clk,
	&dma_clk,
	&lcdc_clk,
	&uhpfs_clk,
	&udc_clk,
	&ssc_clk,
	&ohci_clk,
	&tcb1_clk,
};

/*
 * The two programmable clocks.
 * You must configure pin multiplexing to bring these signals out.
 */
static struct clk pck0 = {
	.name		= "pck0",
	.pmc_mask	= AT91_PMC_PCK0,
	.type		= CLK_TYPE_PROGRAMMABLE,
	.id		= 0,
};
static struct clk pck1 = {
	.name		= "pck1",
	.pmc_mask	= AT91_PMC_PCK1,
	.type		= CLK_TYPE_PROGRAMMABLE,
	.id		= 1,
};

static void __init at91sam9n12_register_clocks(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(periph_clocks); i++)
		clk_register(periph_clocks[i]);

	clk_register(&pck0);
	clk_register(&pck1);
}

/* --------------------------------------------------------------------
 *  GPIO
 * -------------------------------------------------------------------- */

static struct at91_gpio_bank at91sam9n12_gpio[] = {
	{
		.id		= AT91SAM9N12_ID_PIOAB,
		.offset		= AT91_PIOA,
		.clock		= &pioAB_clk,
	}, {
		.id		= AT91SAM9N12_ID_PIOAB,
		.offset		= AT91_PIOB,
		.clock		= &pioAB_clk,
	}, {
		.id		= AT91SAM9N12_ID_PIOCD,
		.offset		= AT91_PIOC,
		.clock		= &pioCD_clk,
	}, {
		.id		= AT91SAM9N12_ID_PIOCD,
		.offset		= AT91_PIOD,
		.clock		= &pioCD_clk,
	}
};

static void at91sam9n12_reset(void)
{
	at91_sys_write(AT91_RSTC_CR, AT91_RSTC_KEY | AT91_RSTC_PROCRST | AT91_RSTC_PERRST);
}

static void at91sam9n12_poweroff(void)
{
	at91_sys_write(AT91_SHDW_CR, AT91_SHDW_KEY | AT91_SHDW_SHDW);
}

/* --------------------------------------------------------------------
 *  AT91SAM9N12 processor initialization
 * -------------------------------------------------------------------- */

void __init at91sam9n12_initialize(unsigned long main_clock)
{
	/* Map peripherals */
	iotable_init(at91sam9n12_io_desc, ARRAY_SIZE(at91sam9n12_io_desc));

	at91_arch_reset = at91sam9n12_reset;
	pm_power_off = at91sam9n12_poweroff;
	at91_extern_irq = (1 << AT91SAM9N12_ID_IRQ0);

	/* Init clock subsystem */
	at91_clock_init(main_clock);

	/* Register the processor-specific clocks */
	at91sam9n12_register_clocks();

	/* Register GPIO subsystem */
	at91_gpio_init(at91sam9n12_gpio, 4);
}

/* --------------------------------------------------------------------
 *  Interrupt initialization
 * -------------------------------------------------------------------- */

/*
 * The default interrupt priority levels (0 = lowest, 7 = highest).
 */
static unsigned int at91sam9n12_default_irq_priority[NR_AIC_IRQS] __initdata = {
	7,	/* Advanced Interrupt Controller (FIQ) */
	7,	/* System Peripherals */
	1,	/* Parallel IO Controller A and B */
	1,	/* Parallel IO Controller C and D */
	0,	/* Reserved */
	5,	/* USART 0 */
	5,	/* USART 1 */
	5,	/* USART 2 */
	5,	/* USART 3 */
	6,	/* Two-Wire Interface 0 */
	6,	/* Two-Wire Interface 1 */
	0,	/* Reserved */
	0,	/* Multimedia Card Interface 0 */
	5,	/* Serial Peripheral Interface 0 */
	5,	/* Serial Peripheral Interface 1 */
	5,	/* UART 0 */
	5,	/* UART 1 */
	0,	/* Timer Counter 0, 1, 2, 3, 4 and 5 */
	0,	/* Pulse Width Modulation Controller */
	0,	/* ADC COntroller */
	0,	/* DMA Controller */
	0,	/* Reserved */
	2,	/* USB Host Full Speed port */
	2,	/* USB Device Full Speed port */
	0,	/* Reserved */
	3,	/* LDC Controller or Image Sensor Interface */
	0,	/* Reserved */
	0,	/* Reserved */
	4,	/* Synchronous Serial Interface */
	0,	/* Reserved */
	1,	/* TRNG */
	0,	/* Advanced Interrupt Controller (IRQ0) */
};

void __init at91sam9n12_init_interrupts(unsigned int priority[NR_AIC_IRQS])
{
	if (!priority)
		priority = at91sam9n12_default_irq_priority;

	/* Initialize the AIC interrupt controller */
	at91_aic_init(priority);

	/* Enable GPIO interrupts */
	at91_gpio_irq_setup();
}
