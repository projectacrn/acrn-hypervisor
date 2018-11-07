/*-
 * Copyright (c) 2012 NetApp, Inc.
 * Copyright (c) 2013 Neel Natu <neel@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <sys/errno.h>

#include "types.h"
#include "mevent.h"
#include "uart_core.h"
#include "ns16550.h"
#include "dm.h"

#define	COM1_BASE	0x3F8
#define COM1_IRQ	4
#define	COM2_BASE	0x2F8
#define COM2_IRQ	3

#define	DEFAULT_RCLK	1843200
#define	DEFAULT_BAUD	9600

#define	FCR_RX_MASK	0xC0

#define	MCR_OUT1	0x04
#define	MCR_OUT2	0x08

#define	MSR_DELTA_MASK	0x0f

#ifndef REG_SCR
#define REG_SCR		com_scr
#endif

#define	FIFOSZ	256

static struct termios tio_stdio_orig;

static struct {
	int	baseaddr;
	int	irq;
	bool	inuse;
} uart_lres[] = {
	{ COM1_BASE, COM1_IRQ, false},
	{ COM2_BASE, COM2_IRQ, false},
};

#define	UART_NLDEVS	(ARRAY_SIZE(uart_lres))

struct fifo {
	uint8_t	buf[FIFOSZ];
	int	rindex;		/* index to read from */
	int	windex;		/* index to write to */
	int	num;		/* number of characters in the fifo */
	int	size;		/* size of the fifo */
};

struct ttyfd {
	bool	opened;
	int	fd_in;		/* tty device file descriptor */
	int	fd_out;		/* stdin=0 stdout=1 should be different, when use stdio*/
	struct termios tio_orig, tio_new;    /* I/O Terminals */
};

struct uart_vdev {
	pthread_mutex_t mtx;	/* protects all elements */
	uint8_t	data;		/* Data register (R/W) */
	uint8_t ier;		/* Interrupt enable register (R/W) */
	uint8_t lcr;		/* Line control register (R/W) */
	uint8_t mcr;		/* Modem control register (R/W) */
	uint8_t lsr;		/* Line status register (R/W) */
	uint8_t msr;		/* Modem status register (R/W) */
	uint8_t fcr;		/* FIFO control register (W) */
	uint8_t scr;		/* Scratch register (R/W) */

	uint8_t dll;		/* Baudrate divisor latch LSB */
	uint8_t dlh;		/* Baudrate divisor latch MSB */

	struct fifo rxfifo;
	struct mevent *mev;

	struct ttyfd tty;
	bool	thre_int_pending;	/* THRE interrupt pending */

	void	*arg;
	uart_intr_func_t intr_assert;
	uart_intr_func_t intr_deassert;
};

static void uart_drain(int fd, enum ev_type ev, void *arg);

static void
ttyclose(void)
{
	tcsetattr(STDIN_FILENO, TCSANOW, &tio_stdio_orig);
}

static void
ttyopen(struct ttyfd *tf)
{
	tcgetattr(tf->fd_in, &tf->tio_orig);

	tf->tio_new = tf->tio_orig;
	cfmakeraw(&tf->tio_new);
	tf->tio_new.c_cflag |= CLOCAL;
	tcsetattr(tf->fd_in, TCSANOW, &tf->tio_new);

	if (tf->fd_in == STDIN_FILENO) {
		tio_stdio_orig = tf->tio_orig;
		atexit(ttyclose);
	}
}

static int
ttyread(struct ttyfd *tf)
{
	unsigned char rb;

	if (read(tf->fd_in, &rb, 1) > 0)
		return rb;

	return -1;
}

static int
ttywrite(struct ttyfd *tf, unsigned char wb)
{
	if (write(tf->fd_out, &wb, 1) > 0)
		return 1;

	return -1;
}

static void
rxfifo_reset(struct uart_vdev *uart, int size)
{
	char flushbuf[32];
	struct fifo *fifo;
	ssize_t nread;
	int error;

	fifo = &uart->rxfifo;
	bzero(fifo, sizeof(struct fifo));
	fifo->size = size;

	if (uart->tty.opened) {
		/*
		 * Flush any unread input from the tty buffer.
		 */
		while (1) {
			nread = read(uart->tty.fd_in, flushbuf, sizeof(flushbuf));
			if (nread != sizeof(flushbuf))
				break;
		}

		/*
		 * Enable mevent to trigger when new characters are available
		 * on the tty fd.
		 */
		if (isatty(uart->tty.fd_in)) {
			error = mevent_enable(uart->mev);
			assert(error == 0);
		}
	}
}

static int
rxfifo_available(struct uart_vdev *uart)
{
	struct fifo *fifo;

	fifo = &uart->rxfifo;
	return (fifo->num < fifo->size);
}

static int
rxfifo_putchar(struct uart_vdev *uart, uint8_t ch)
{
	struct fifo *fifo;
	int error;

	fifo = &uart->rxfifo;

	if (fifo->num < fifo->size) {
		fifo->buf[fifo->windex] = ch;
		fifo->windex = (fifo->windex + 1) % fifo->size;
		fifo->num++;
		if (!rxfifo_available(uart)) {
			if (uart->tty.opened) {
				/*
				 * Disable mevent callback if the FIFO is full.
				 */
				if (isatty(uart->tty.fd_in)) {
					error = mevent_disable(uart->mev);
					assert(error == 0);
				}
			}
		}
		return 0;
	} else
		return -1;
}

static int
rxfifo_getchar(struct uart_vdev *uart)
{
	struct fifo *fifo;
	int c, error, wasfull;

	wasfull = 0;
	fifo = &uart->rxfifo;
	if (fifo->num > 0) {
		if (!rxfifo_available(uart))
			wasfull = 1;
		c = fifo->buf[fifo->rindex];
		fifo->rindex = (fifo->rindex + 1) % fifo->size;
		fifo->num--;
		if (wasfull) {
			if (uart->tty.opened && isatty(uart->tty.fd_in)) {
				error = mevent_enable(uart->mev);
				assert(error == 0);
			}
		}
		return c;
	} else
		return -1;
}

static int
rxfifo_numchars(struct uart_vdev *uart)
{
	struct fifo *fifo = &uart->rxfifo;

	return fifo->num;
}

static void
uart_opentty(struct uart_vdev *uart)
{
	ttyopen(&uart->tty);
	if (isatty(uart->tty.fd_in)) {
		uart->mev = mevent_add(uart->tty.fd_in, EVF_READ,
			uart_drain, uart);
		assert(uart->mev != NULL);
	}
}

static void
uart_closetty(struct uart_vdev *uart)
{
	if (isatty(uart->tty.fd_in)) {
		if (uart->tty.fd_in != STDIN_FILENO)
			mevent_delete_close(uart->mev);
		else
			mevent_delete(uart->mev);

		uart->mev = 0;
	}

	ttyclose();
}

static uint8_t
modem_status(uint8_t mcr)
{
	uint8_t msr;

	if (mcr & MCR_LOOPBACK) {
		/*
		 * In the loopback mode certain bits from the MCR are
		 * reflected back into MSR.
		 */
		msr = 0;
		if (mcr & MCR_RTS)
			msr |= MSR_CTS;
		if (mcr & MCR_DTR)
			msr |= MSR_DSR;
		if (mcr & MCR_OUT1)
			msr |= MSR_RI;
		if (mcr & MCR_OUT2)
			msr |= MSR_DCD;
	} else {
		/*
		 * Always assert DCD and DSR so tty open doesn't block
		 * even if CLOCAL is turned off.
		 */
		msr = MSR_DCD | MSR_DSR;
	}
	assert((msr & MSR_DELTA_MASK) == 0);

	return msr;
}

/*
 * The IIR returns a prioritized interrupt reason:
 * - receive data available
 * - transmit holding register empty
 * - modem status change
 *
 * Return an interrupt reason if one is available.
 */
static int
uart_intr_reason(struct uart_vdev *uart)
{
	if ((uart->lsr & LSR_OE) != 0 && (uart->ier & IER_ERLS) != 0)
		return IIR_RLS;
	else if (rxfifo_numchars(uart) > 0 && (uart->ier & IER_ERXRDY) != 0)
		return IIR_RXTOUT;
	else if (uart->thre_int_pending && (uart->ier & IER_ETXRDY) != 0)
		return IIR_TXRDY;
	else if ((uart->msr & MSR_DELTA_MASK) != 0 &&
		 (uart->ier & IER_EMSC) != 0)
		return IIR_MLSC;
	else
		return IIR_NOPEND;
}

static void
uart_reset(struct uart_vdev *uart)
{
	uint16_t divisor;

	divisor = DEFAULT_RCLK / DEFAULT_BAUD / 16;
	uart->dll = divisor;
	uart->dlh = divisor >> 16;
	uart->msr = modem_status(uart->mcr);

	rxfifo_reset(uart, 1);	/* no fifo until enabled by software */
}

/*
 * Toggle the COM port's intr pin depending on whether or not we have an
 * interrupt condition to report to the processor.
 */
static void
uart_toggle_intr(struct uart_vdev *uart)
{
	uint8_t intr_reason;

	intr_reason = uart_intr_reason(uart);

	if (intr_reason == IIR_NOPEND)
		(*uart->intr_deassert)(uart->arg);
	else
		(*uart->intr_assert)(uart->arg);
}

static void
uart_drain(int fd, enum ev_type ev, void *arg)
{
	struct uart_vdev *uart;
	int ch;

	uart = arg;

	assert(fd == uart->tty.fd_in);
	assert(ev == EVF_READ);

	/*
	 * This routine is called in the context of the mevent thread
	 * to take out the uart lock to protect against concurrent
	 * access from a vCPU i/o exit
	 */
	pthread_mutex_lock(&uart->mtx);

	if ((uart->mcr & MCR_LOOPBACK) != 0) {
		(void) ttyread(&uart->tty);
	} else {
		while ((ch = ttyread(&uart->tty)) != -1)
			rxfifo_putchar(uart, ch);

		uart_toggle_intr(uart);
	}

	pthread_mutex_unlock(&uart->mtx);
}

void
uart_write(struct uart_vdev *uart, int offset, uint8_t value)
{
	int fifosz;
	uint8_t msr;

	pthread_mutex_lock(&uart->mtx);

	/*
	 * Take care of the special case DLAB accesses first
	 */
	if ((uart->lcr & LCR_DLAB) != 0) {
		if (offset == REG_DLL) {
			uart->dll = value;
			goto done;
		}

		if (offset == REG_DLH) {
			uart->dlh = value;
			goto done;
		}
	}

	switch (offset) {
	case REG_DATA:
		if (uart->mcr & MCR_LOOPBACK) {
			if (rxfifo_putchar(uart, value) != 0)
				uart->lsr |= LSR_OE;
		} else if (uart->tty.opened) {
			ttywrite(&uart->tty, value);
		} /* else drop on floor */
		uart->thre_int_pending = true;
		break;
	case REG_IER:
		/*
		 * Apply mask so that bits 4-7 are 0
		 * Also enables bits 0-3 only if they're 1
		 */
		uart->ier = value & 0x0F;
		break;
	case REG_FCR:
		/*
		 * When moving from FIFO and 16450 mode and vice versa,
		 * the FIFO contents are reset.
		 */
		if ((uart->fcr & FCR_ENABLE) ^ (value & FCR_ENABLE)) {
			fifosz = (value & FCR_ENABLE) ? FIFOSZ : 1;
			rxfifo_reset(uart, fifosz);
		}

		/*
		 * The FCR_ENABLE bit must be '1' for the programming
		 * of other FCR bits to be effective.
		 */
		if ((value & FCR_ENABLE) == 0) {
			uart->fcr = 0;
		} else {
			if ((value & FCR_RCV_RST) != 0)
				rxfifo_reset(uart, FIFOSZ);

			uart->fcr = value &
				(FCR_ENABLE | FCR_DMA | FCR_RX_MASK);
		}
		break;
	case REG_LCR:
		uart->lcr = value;
		break;
	case REG_MCR:
		/* Apply mask so that bits 5-7 are 0 */
		uart->mcr = value & 0x1F;
		msr = modem_status(uart->mcr);

		/*
		 * Detect if there has been any change between the
		 * previous and the new value of MSR. If there is
		 * then assert the appropriate MSR delta bit.
		 */
		if ((msr & MSR_CTS) ^ (uart->msr & MSR_CTS))
			uart->msr |= MSR_DCTS;
		if ((msr & MSR_DSR) ^ (uart->msr & MSR_DSR))
			uart->msr |= MSR_DDSR;
		if ((msr & MSR_DCD) ^ (uart->msr & MSR_DCD))
			uart->msr |= MSR_DDCD;
		if ((uart->msr & MSR_RI) != 0 && (msr & MSR_RI) == 0)
			uart->msr |= MSR_TERI;

		/*
		 * Update the value of MSR while retaining the delta
		 * bits.
		 */
		uart->msr &= MSR_DELTA_MASK;
		uart->msr |= msr;
		break;
	case REG_LSR:
		/*
		 * Line status register is not meant to be written to
		 * during normal operation.
		 */
		break;
	case REG_MSR:
		/*
		 * As far as I can tell MSR is a read-only register.
		 */
		break;
	case REG_SCR:
		uart->scr = value;
		break;
	default:
		break;
	}

done:
	uart_toggle_intr(uart);
	pthread_mutex_unlock(&uart->mtx);
}

uint8_t
uart_read(struct uart_vdev *uart, int offset)
{
	uint8_t iir, intr_reason, reg;

	pthread_mutex_lock(&uart->mtx);

	/*
	 * Take care of the special case DLAB accesses first
	 */
	if ((uart->lcr & LCR_DLAB) != 0) {
		if (offset == REG_DLL) {
			reg = uart->dll;
			goto done;
		}

		if (offset == REG_DLH) {
			reg = uart->dlh;
			goto done;
		}
	}

	switch (offset) {
	case REG_DATA:
		reg = rxfifo_getchar(uart);
		break;
	case REG_IER:
		reg = uart->ier;
		break;
	case REG_IIR:
		iir = (uart->fcr & FCR_ENABLE) ? IIR_FIFO_MASK : 0;

		intr_reason = uart_intr_reason(uart);

		/*
		 * Deal with side effects of reading the IIR register
		 */
		if (intr_reason == IIR_TXRDY)
			uart->thre_int_pending = false;

		iir |= intr_reason;

		reg = iir;
		break;
	case REG_LCR:
		reg = uart->lcr;
		break;
	case REG_MCR:
		reg = uart->mcr;
		break;
	case REG_LSR:
		/* Transmitter is always ready for more data */
		uart->lsr |= LSR_TEMT | LSR_THRE;

		/* Check for new receive data */
		if (rxfifo_numchars(uart) > 0)
			uart->lsr |= LSR_RXRDY;
		else
			uart->lsr &= ~LSR_RXRDY;

		reg = uart->lsr;

		/* The LSR_OE bit is cleared on LSR read */
		uart->lsr &= ~LSR_OE;
		break;
	case REG_MSR:
		/*
		 * MSR delta bits are cleared on read
		 */
		reg = uart->msr;
		uart->msr &= ~MSR_DELTA_MASK;
		break;
	case REG_SCR:
		reg = uart->scr;
		break;
	default:
		reg = 0xFF;
		break;
	}

done:
	uart_toggle_intr(uart);
	pthread_mutex_unlock(&uart->mtx);

	return reg;
}

int
uart_legacy_alloc(int which, int *baseaddr, int *irq)
{
	if (which < 0 || which >= UART_NLDEVS || uart_lres[which].inuse)
		return -1;

	uart_lres[which].inuse = true;
	*baseaddr = uart_lres[which].baseaddr;
	*irq = uart_lres[which].irq;

	return 0;
}

void
uart_legacy_dealloc(int which)
{
	uart_lres[which].inuse = false;
}

struct uart_vdev *
uart_init(uart_intr_func_t intr_assert, uart_intr_func_t intr_deassert,
	void *arg)
{
	struct uart_vdev *uart;

	uart = calloc(1, sizeof(struct uart_vdev));

	assert(uart != NULL);

	uart->arg = arg;
	uart->intr_assert = intr_assert;
	uart->intr_deassert = intr_deassert;

	pthread_mutex_init(&uart->mtx, NULL);

	uart_reset(uart);

	return uart;
}

void
uart_deinit(struct uart_vdev *uart)
{
	if (uart) {
		if (uart->tty.opened && uart->tty.fd_in == STDIN_FILENO) {
			ttyclose();
			stdio_in_use = false;
		}
		free(uart);
	}
}

static int
uart_tty_backend(struct uart_vdev *uart, const char *opts)
{
	int fd;
	int retval;

	retval = -1;

	fd = open(opts, O_RDWR | O_NONBLOCK);
	if (fd > 0 && isatty(fd)) {
		uart->tty.fd_in = fd;
		uart->tty.fd_out = fd;
		uart->tty.opened = true;
		retval = 0;
	}

	return retval;
}

int
uart_set_backend(struct uart_vdev *uart, const char *opts)
{
	int retval;

	retval = -1;

	if (opts == NULL)
		return -EINVAL;

	if (strcmp("stdio", opts) == 0) {
		if (!stdio_in_use) {
			uart->tty.fd_in = STDIN_FILENO;
			uart->tty.fd_out = STDOUT_FILENO;
			uart->tty.opened = true;
			stdio_in_use = true;
			retval = 0;
		}
	} else if (uart_tty_backend(uart, opts) == 0) {
		retval = 0;
	}

	if (retval)
		return -EINVAL;

	/* Make the backend file descriptor non-blocking */
	if (retval == 0)
		retval = fcntl(uart->tty.fd_in, F_SETFL, O_NONBLOCK);

	if (retval == 0)
		uart_opentty(uart);

	return retval;
}

void
uart_release_backend(struct uart_vdev *uart, const char *opts)
{
	if (opts == NULL)
		return;

	/*
	 * By current design, for the invalid PTY parameters, the virtual uarts
	 * are still expose to UOS but all data be dropped by backend service.
	 * The uart backend is not setup for this case, so don't try to release
	 * the uart backend in here.
	 * TODO: need re-visit the whole policy for such scenario in future.
	 */
	if (!uart->tty.opened)
		return;

	uart_closetty(uart);
	if (strcmp("stdio", opts) == 0) {
		stdio_in_use = false;
	} else
		close(uart->tty.fd_in);

	uart->tty.fd_in = -1;
	uart->tty.fd_out = -1;
	uart->tty.opened = false;
}
