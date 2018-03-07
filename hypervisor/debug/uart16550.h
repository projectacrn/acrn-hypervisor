/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UART16550_H
#define UART16550_H

/* Register / bit definitions for 16c550 uart */
#define UART16550_RBR           0x00
/*receive buffer register            | base+00h, dlab=0b r*/
#define UART16550_THR           0x00
/*transmit holding register          | base+00h, dlab=0b w*/
#define UART16550_DLL           0x00
/*divisor least significant byte     | base+00h, dlab=1b rw*/
#define UART16550_IER           0x01
/*interrupt enable register          | base+01h, dlab=0b rw*/
#define UART16550_DLM           0x01
/*divisor most significant byte      | base+01h, dlab=1b rw*/
#define UART16550_IIR           0x02
/*interrupt identification register  | base+02h, dlab=0b r*/
#define UART16550_FCR           0x02
/*fifo control register              | base+02h, dlab=0b w*/
#define UART16550_LCR           0x03
/*line control register              | base+03h, dlab=xb rw*/
#define UART16550_MCR           0x04
/*modem control register, only uart0 | base+04h, dlab=xb rw*/
#define UART16550_LSR           0x05
/*line status register               | base+05h, dlab=xb r*/
#define UART16550_MSR           0x06
/*modem status register, only uart0  | base+06h, dlab=xb r*/
#define UART16550_SCR           0x07
/*scratch pad register               | base+07h, dlab=xb rw*/
#define UART16550_MDR1          0x08
#define UARTML7213_BRCSR        0x0e
/*baud rate reference clock select register dlab xb*/
#define UARTML7213_SRST         0x0f /*Soft Reset Register dlab xb*/

/* value definitions for IIR */
#define IIR_FIFO_MASK		0xc0 /* set if FIFOs are enabled */
#define IIR_RXTOUT		0x0c
#define IIR_RLS			0x06
#define IIR_RXRDY		0x04
#define IIR_TXRDY		0x02
#define IIR_NOPEND		0x01
#define IIR_MLSC		0x00

#define IER_EDSSI		(0x0008)
/*enable/disable modem status interrupt*/
#define IER_ELSI		(0x0004)
/*enable/disable receive data error interrupt*/
#define IER_ETBEI		(0x0002)
/*enable/disable transmit data write request interrupt*/
#define IER_ERBFI		(0x0001)
/*enable/disable receive data read request interrupt*/

/* definition for LCR */
#define LCR_DLAB	(1 << 7) /*DLAB THR/RBR&IER or DLL&DLM= Bit 7*/
#define LCR_SB		(1 << 6) /*break control on/off= Bit 6*/
#define LCR_SP		(1 << 5) /*Specifies the operation of parity bit*/
#define LCR_EPS		(1 << 4) /*Specifies the logic of a parity bit*/
#define LCR_PEN		(1 << 3) /*Specifies whether to add a parity bit*/
#define LCR_STB		(1 << 2) /*stop bit length*/
#define LCR_WL8		(0x03) /*number of bits of serial data*/
#define LCR_WL7		(0x02) /*number of bits of serial data*/
#define LCR_WL6		(0x01) /*number of bits of serial data*/
#define LCR_WL5		(0x00) /*number of bits of serial data*/
#define LCR_PARITY_ODD		(LCR_PEN)
#define LCR_PARITY_NONE		0x0
#define LCR_PARITY_EVEN		(LCR_PEN | LCR_EPS)
#define LCR_NB_STOP_BITS_1	0x0
#define LCR_NB_STOP_BITS_2	(LCR_STB)

/* bit definitions for LSR */
/* at least one error in data within fifo */
#define LSR_ERR		(1 << 7)
/* Transmit data Present */
#define LSR_TEMT	(1 << 6)
/* Transmit data write request present */
#define LSR_THRE	(1 << 5)
/* Break interrupt data Present */
#define LSR_BI		(1 << 4)
/* Framing Error Occurred */
#define LSR_FE		(1 << 3)
/* Parity Error Occurred */
#define LSR_PE		(1 << 2)
/* Overrun error */
#define LSR_OE		(1 << 1)
/* Readable received data is present */
#define LSR_DR		(1 << 0)

/* definition for MCR */
#define MCR_RTS		(1 << 1) /* Request to Send */
#define MCR_DTR		(1 << 0) /* Data Terminal Ready */

/* definition for FCR */
#define FCR_RX_MASK	0xc0
#define FCR_DMA		(1 << 3)
#define FCR_TFR		(1 << 2) /* Reset Transmit Fifo */
#define FCR_RFR		(1 << 1) /* Reset Receive Fifo */
#define FCR_FIFOE	(1 << 0) /* Fifo Enable */

#define UART_IER_DISABLE_ALL	0x00000000

#endif /* !UART16550_H */
