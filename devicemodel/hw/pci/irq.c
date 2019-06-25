/*-
 * Copyright (c) 2014 Hudson River Trading LLC
 * Written by: John H. Baldwin <jhb@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "types.h"
#include "acpi.h"
#include "vmmapi.h"
#include "pci_core.h"
#include "lpc.h"

/*
 * Implement an 8 pin PCI interrupt router compatible with the router
 * present on Intel's ICH10 chip.
 */

/* Fields in each PIRQ register. */
#define	PIRQ_DIS	0x80
#define	PIRQ_IRQ	0x0f

/* Only IRQs 3-7, 9-12, and 14-15 are permitted. */
#define	PERMITTED_IRQS	0xdef8
#define	IRQ_PERMITTED(irq)	(((1U << (irq)) & PERMITTED_IRQS) != 0)

/* IRQ count to disable an IRQ. */
#define	IRQ_DISABLED	0xff

static struct pirq {
	uint8_t	reg;
	int	use_count;
	int	active_count;
	pthread_mutex_t lock;
} pirqs[8];

static u_char irq_counts[16];
static int pirq_cold = 1;

/*
 * Returns true if this pin is enabled with a valid IRQ.  Setting the
 * register to a reserved IRQ causes interrupts to not be asserted as
 * if the pin was disabled.
 */
static bool
pirq_valid_irq(int reg)
{
	if (reg & PIRQ_DIS)
		return false;
	return IRQ_PERMITTED(reg & PIRQ_IRQ);
}

uint8_t
pirq_read(int pin)
{
	if (pin <= 0 || pin > nitems(pirqs))
		return PIRQ_DIS;

	return pirqs[pin - 1].reg;
}

void
pirq_write(struct vmctx *ctx, int pin, uint8_t val)
{
	struct pirq *pirq;

	if (pin <= 0 || pin > nitems(pirqs))
		return;

	pirq = &pirqs[pin - 1];
	pthread_mutex_lock(&pirq->lock);
	if (pirq->reg != (val & (PIRQ_DIS | PIRQ_IRQ))) {
		if (pirq->active_count != 0 && pirq_valid_irq(pirq->reg))
			vm_set_gsi_irq(ctx, pirq->reg & PIRQ_IRQ, GSI_SET_LOW);
		pirq->reg = val & (PIRQ_DIS | PIRQ_IRQ);
		if (pirq->active_count != 0 && pirq_valid_irq(pirq->reg))
			vm_set_gsi_irq(ctx, pirq->reg & PIRQ_IRQ, GSI_SET_HIGH);
	}
	pthread_mutex_unlock(&pirq->lock);
}

void
pci_irq_reserve(int irq) {
	if ((irq >= 0 && irq < nitems(irq_counts)) && pirq_cold
		&& (irq_counts[irq] == 0 || irq_counts[irq] == IRQ_DISABLED))
		irq_counts[irq] = IRQ_DISABLED;
}

void
pci_irq_use(int irq)
{
	if ((irq >= 0 && irq < nitems(irq_counts)) && pirq_cold
		&& (irq_counts[irq] != IRQ_DISABLED))
		irq_counts[irq]++;
}

void
pci_irq_init(struct vmctx *ctx)
{
	int i;

	for (i = 0; i < nitems(pirqs); i++) {
		pirqs[i].reg = PIRQ_DIS;
		pirqs[i].use_count = 0;
		pirqs[i].active_count = 0;
		pthread_mutex_init(&pirqs[i].lock, NULL);
	}
	for (i = 0; i < nitems(irq_counts); i++) {
		if (IRQ_PERMITTED(i))
			irq_counts[i] = 0;
		else
			irq_counts[i] = IRQ_DISABLED;
	}
}

void pci_irq_deinit(struct vmctx *ctx)
{
	pirq_cold = 1;
}

void
pci_irq_assert(struct pci_vdev *dev)
{
	vm_set_gsi_irq(dev->vmctx, dev->lintr.ioapic_irq, GSI_SET_HIGH);
}

void
pci_irq_deassert(struct pci_vdev *dev)
{
	vm_set_gsi_irq(dev->vmctx, dev->lintr.ioapic_irq, GSI_SET_LOW);
}

int
pirq_alloc_pin(struct pci_vdev *dev)
{
	int best_count, best_irq, best_pin, irq, pin;

	pirq_cold = 0;

	/* Find the least-used PIRQ pin. */
	best_pin = 0;
	best_count = pirqs[0].use_count;
	for (pin = 1; pin < nitems(pirqs); pin++) {
		if (pirqs[pin].use_count < best_count) {
			best_pin = pin;
			best_count = pirqs[pin].use_count;
		}
	}
	pirqs[best_pin].use_count++;

	/* Second, route this pin to an IRQ. */
	if (pirqs[best_pin].reg == PIRQ_DIS) {
		best_irq = -1;
		best_count = 0;
		for (irq = 0; irq < nitems(irq_counts); irq++) {
			if (irq_counts[irq] == IRQ_DISABLED)
				continue;
			if (best_irq == -1 || irq_counts[irq] < best_count) {
				best_irq = irq;
				best_count = irq_counts[irq];
			}
		}
		if (best_irq < 0)
			return -1;

		irq_counts[best_irq]++;
		pirqs[best_pin].reg = best_irq;
	}

	return (best_pin + 1);
}

int
pirq_irq(int pin)
{
	if (pin <= 0 || pin > nitems(pirqs))
		return 0xFF;

	return (pirqs[pin - 1].reg & PIRQ_IRQ);
}

/* XXX: Generate $PIR table. */

static void
pirq_dsdt(void)
{
	char *irq_prs, *old;
	int irq, pin;

	irq_prs = NULL;
	for (irq = 0; irq < nitems(irq_counts); irq++) {
		if (!IRQ_PERMITTED(irq))
			continue;
		if (irq_prs == NULL) {
			if (asprintf(&irq_prs, "%d", irq) < 0) {
				/*error*/
				if (irq_prs != NULL)
					free(irq_prs);

				return;
			}
		} else {
			old = irq_prs;
			if (asprintf(&irq_prs, "%s,%d", old, irq) < 0) {
				/*error*/
				if (irq_prs != NULL)
					free(irq_prs);

				free(old);
				return;
			}
			free(old);
		}
	}

	/*
	 * A helper method to validate a link register's value.  This
	 * duplicates pirq_valid_irq().
	 */
	dsdt_line("");
	dsdt_line("Method (PIRV, 1, NotSerialized)");
	dsdt_line("{");
	dsdt_line("  If (And (Arg0, 0x%02X))", PIRQ_DIS);
	dsdt_line("  {");
	dsdt_line("    Return (0x00)");
	dsdt_line("  }");
	dsdt_line("  And (Arg0, 0x%02X, Local0)", PIRQ_IRQ);
	dsdt_line("  If (LLess (Local0, 0x03))");
	dsdt_line("  {");
	dsdt_line("    Return (0x00)");
	dsdt_line("  }");
	dsdt_line("  If (LEqual (Local0, 0x08))");
	dsdt_line("  {");
	dsdt_line("    Return (0x00)");
	dsdt_line("  }");
	dsdt_line("  If (LEqual (Local0, 0x0D))");
	dsdt_line("  {");
	dsdt_line("    Return (0x00)");
	dsdt_line("  }");
	dsdt_line("  Return (0x01)");
	dsdt_line("}");

	for (pin = 0; pin < nitems(pirqs); pin++) {
		dsdt_line("");
		dsdt_line("Device (LNK%c)", 'A' + pin);
		dsdt_line("{");
		dsdt_line("  Name (_HID, EisaId (\"PNP0C0F\"))");
		dsdt_line("  Name (_UID, 0x%02X)", pin + 1);
		dsdt_line("  Method (_STA, 0, NotSerialized)");
		dsdt_line("  {");
		dsdt_line("    If (PIRV (PIR%c))", 'A' + pin);
		dsdt_line("    {");
		dsdt_line("       Return (0x0B)");
		dsdt_line("    }");
		dsdt_line("    Else");
		dsdt_line("    {");
		dsdt_line("       Return (0x09)");
		dsdt_line("    }");
		dsdt_line("  }");
		dsdt_line("  Name (_PRS, ResourceTemplate ()");
		dsdt_line("  {");
		dsdt_line("    IRQ (Level, ActiveLow, Shared, )");
		dsdt_line("      {%s}", irq_prs);
		dsdt_line("  })");
		dsdt_line("  Name (CB%02X, ResourceTemplate ()", pin + 1);
		dsdt_line("  {");
		dsdt_line("    IRQ (Level, ActiveLow, Shared, )");
		dsdt_line("      {}");
		dsdt_line("  })");
		dsdt_line("  CreateWordField (CB%02X, 0x01, CIR%c)",
		    pin + 1, 'A' + pin);
		dsdt_line("  Method (_CRS, 0, NotSerialized)");
		dsdt_line("  {");
		dsdt_line("    And (PIR%c, 0x%02X, Local0)", 'A' + pin,
		    PIRQ_DIS | PIRQ_IRQ);
		dsdt_line("    If (PIRV (Local0))");
		dsdt_line("    {");
		dsdt_line("      ShiftLeft (0x01, Local0, CIR%c)", 'A' + pin);
		dsdt_line("    }");
		dsdt_line("    Else");
		dsdt_line("    {");
		dsdt_line("      Store (0x00, CIR%c)", 'A' + pin);
		dsdt_line("    }");
		dsdt_line("    Return (CB%02X)", pin + 1);
		dsdt_line("  }");
		dsdt_line("  Method (_DIS, 0, NotSerialized)");
		dsdt_line("  {");
		dsdt_line("    Store (0x80, PIR%c)", 'A' + pin);
		dsdt_line("  }");
		dsdt_line("  Method (_SRS, 1, NotSerialized)");
		dsdt_line("  {");
		dsdt_line("    CreateWordField (Arg0, 0x01, SIR%c)", 'A' + pin);
		dsdt_line("    FindSetRightBit (SIR%c, Local0)", 'A' + pin);
		dsdt_line("    Store (Decrement (Local0), PIR%c)", 'A' + pin);
		dsdt_line("  }");
		dsdt_line("}");
	}
	free(irq_prs);
}
LPC_DSDT(pirq_dsdt);
