/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef UART16550_H
#define UART16550_H

/* Register / bit definitions for 16c550 uart */
/*receive buffer register            | base+00h, dlab=0b r*/
#define UART16550_RBR           0x00U
/*transmit holding register          | base+00h, dlab=0b w*/
#define UART16550_THR           0x00U
/*divisor least significant byte     | base+00h, dlab=1b rw*/
#define UART16550_DLL           0x00U
/*interrupt enable register          | base+01h, dlab=0b rw*/
#define UART16550_IER           0x01U
/*divisor most significant byte      | base+01h, dlab=1b rw*/
#define UART16550_DLM           0x01U
/*interrupt identification register  | base+02h, dlab=0b r*/
#define UART16550_IIR           0x02U
/*fifo control register              | base+02h, dlab=0b w*/
#define UART16550_FCR           0x02U
/*line control register              | base+03h, dlab=xb rw*/
#define UART16550_LCR           0x03U
/*modem control register, only uart0 | base+04h, dlab=xb rw*/
#define UART16550_MCR           0x04U
/*line status register               | base+05h, dlab=xb r*/
#define UART16550_LSR           0x05U
/*modem status register, only uart0  | base+06h, dlab=xb r*/
#define UART16550_MSR           0x06U
/*scratch pad register               | base+07h, dlab=xb rw*/
#define UART16550_SCR           0x07U

/* value definitions for IIR */
#define IIR_FIFO_MASK		0xc0U /* set if FIFOs are enabled */
#define IIR_RXTOUT		0x0cU
#define IER_EMSC		0x08U
#define IIR_RLS			0x06U
#define IIR_RXRDY		0x04U
#define IIR_TXRDY		0x02U
#define IIR_NOPEND		0x01U
#define IIR_MLSC		0x00U

#define IER_EDSSI		(0x0008U)
/*enable/disable modem status interrupt*/
#define IER_ELSI		(0x0004U)
/*enable/disable receive data error interrupt*/
#define IER_ETBEI		(0x0002U)
/*enable/disable transmit data write request interrupt*/
#define IER_ERBFI		(0x0001U)
/*enable/disable receive data read request interrupt*/

/* definition for LCR */
#define LCR_DLAB	(1U << 7U) /*DLAB THR/RBR&IER or DLL&DLM= Bit 7*/
#define LCR_SB		(1U << 6U) /*break control on/off= Bit 6*/
#define LCR_SP		(1U << 5U) /*Specifies the operation of parity bit*/
#define LCR_EPS		(1U << 4U) /*Specifies the logic of a parity bit*/
#define LCR_PEN		(1U << 3U) /*Specifies whether to add a parity bit*/
#define LCR_STB		(1U << 2U) /*stop bit length*/
#define LCR_WL8		(0x03U) /*number of bits of serial data*/
#define LCR_WL7		(0x02U) /*number of bits of serial data*/
#define LCR_WL6		(0x01U) /*number of bits of serial data*/
#define LCR_WL5		(0x00U) /*number of bits of serial data*/
#define LCR_PARITY_ODD		(LCR_PEN)
#define LCR_PARITY_NONE		0x0U
#define LCR_PARITY_EVEN		(LCR_PEN | LCR_EPS)
#define LCR_NB_STOP_BITS_1	0x0U
#define LCR_NB_STOP_BITS_2	(LCR_STB)

/* bit definitions for LSR */
/* at least one error in data within fifo */
#define LSR_ERR		(1U << 7U)
/* Transmit data Present */
#define LSR_TEMT	(1U << 6U)
/* Transmit data write request present */
#define LSR_THRE	(1U << 5U)
/* Break interrupt data Present */
#define LSR_BI		(1U << 4U)
/* Framing Error Occurred */
#define LSR_FE		(1U << 3U)
/* Parity Error Occurred */
#define LSR_PE		(1U << 2U)
/* Overrun error */
#define LSR_OE		(1U << 1U)
/* Readable received data is present */
#define LSR_DR		(1U << 0U)

/* definition for MCR */
#define MCR_PRESCALE	(1U << 7U) /* only available on 16650 up */
#define MCR_LOOPBACK	(1U << 4U)
#define MCR_IE		(1U << 3U)
#define MCR_IENABLE	MCR_IE
#define MCR_DRS		(1U << 2U)
#define MCR_RTS		(1U << 1U) /* Request to Send */
#define MCR_DTR		(1U << 0U) /* Data Terminal Ready */

/* defifor MSR */
#define MSR_DCD		(1U << 7U)
#define MSR_RI		(1U << 6U)
#define MSR_DSR		(1U << 5U)
#define MSR_CTS		(1U << 4U)
#define MSR_DDCD	(1U << 3U)
#define MSR_TERI	(1U << 2U)
#define MSR_DDSR	(1U << 1U)
#define MSR_DCTS	(1U << 0U)

#define MCR_OUT2	(1U << 3U)
#define MCR_OUT1	(1U << 2U)

#define MSR_DELTA_MASK	0x0FU

/* definition for FCR */
#define FCR_RX_MASK	0xc0U
#define FCR_DMA		(1U << 3U)
#define FCR_TFR		(1U << 2U) /* Reset Transmit Fifo */
#define FCR_RFR		(1U << 1U) /* Reset Receive Fifo */
#define FCR_FIFOE	(1U << 0U) /* Fifo Enable */

#define UART_IER_DISABLE_ALL	0x00000000U

#define BAUD_9600      9600U
#define BAUD_115200    115200U
#define BAUD_460800    460800U

/* UART oscillator clock */
#define UART_CLOCK_RATE	1843200U	/* 1.8432 MHz */

void uart16550_init(bool early_boot);
char uart16550_getc(void);
size_t uart16550_puts(const char *buf, uint32_t len);
void uart16550_set_property(bool enabled, bool port_mapped, uint64_t base_addr);
bool is_pci_dbg_uart(union pci_bdf bdf_value);

#endif /* !UART16550_H */
