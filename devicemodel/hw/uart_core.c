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
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/errno.h>

#include "types.h"
#include "mevent.h"
#include "uart_core.h"
#include "ns16550.h"
#include "dm.h"
#include "dm_string.h"

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

#define	DEFAULT_FIFOSZ	(256)
#define	SOCK_FIFOSZ	(32 * 1024)

static int uart_debug;
#define DPRINTF(params) do { if (uart_debug) printf params; } while (0)
#define WPRINTF(params) (printf params)

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

enum uart_be_type {
	UART_BE_INVALID = 0,
	UART_BE_STDIO,
	UART_BE_TTY,
	UART_BE_SOCK
};

struct fifo {
	uint8_t	*buf;
	int	rindex;		/* index to read from */
	int	windex;		/* index to write to */
	int	num;		/* number of characters in the fifo */
	int	size;		/* size of the fifo */
};

struct uart_backend {
	/*
	 * UART_BE_STDIO: fd = STDIN_FILENO
	 * UART_BE_TTY: fd = open(tty)
	 * UART_BE_SOCK: fd = file descriptor of listen socket
	 */
	int			fd;
	struct mevent		*evp;

	/*
	 * UART_BE_STDIO: fd2 = STDOUT_FILENO
	 * UART_BE_TTY: fd2 = fd = open(tty)
	 * UART_BE_SOCK: fd2 = file descriptor of connected socket
	 */
	int			fd2;
	struct mevent		*evp2;

	enum uart_be_type	be_type;
	bool			opened;
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
	struct uart_backend be;

	bool	thre_int_pending;	/* THRE interrupt pending */

	void	*arg;
	int	rxfifo_size;
	uart_intr_func_t intr_assert;
	uart_intr_func_t intr_deassert;
};

static void uart_drain(int fd, enum ev_type ev, void *arg);
static void uart_deinit(struct uart_vdev *uart);
static int uart_backend_read(struct uart_backend *be);
static int uart_backend_write(struct uart_backend *be, unsigned char wb);
static int uart_reset_backend(struct uart_backend *be);
static int uart_enable_backend(struct uart_backend *be, bool enable);

static void
uart_reset_stdio(void)
{
	tcsetattr(STDIN_FILENO, TCSANOW, &tio_stdio_orig);
	stdio_in_use = false;
}

static void
rxfifo_reset(struct uart_vdev *uart, int size)
{
	struct fifo *fifo;

	if (size > uart->rxfifo_size)
		size = uart->rxfifo_size;

	fifo = &uart->rxfifo;
	fifo->rindex = 0;
	fifo->windex = 0;
	fifo->num = 0;
	fifo->size = size;

	uart_reset_backend(&uart->be);
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

	fifo = &uart->rxfifo;

	if (fifo->num < fifo->size) {
		fifo->buf[fifo->windex] = ch;
		fifo->windex = (fifo->windex + 1) % fifo->size;
		fifo->num++;
		if (!rxfifo_available(uart))
			uart_enable_backend(&uart->be, false);
		return 0;
	} else
		return -1;
}

static int
rxfifo_getchar(struct uart_vdev *uart)
{
	struct fifo *fifo;
	int c, wasfull;

	wasfull = 0;
	fifo = &uart->rxfifo;
	if (fifo->num > 0) {
		if (!rxfifo_available(uart))
			wasfull = 1;
		c = fifo->buf[fifo->rindex];
		fifo->rindex = (fifo->rindex + 1) % fifo->size;
		fifo->num--;
		if (wasfull)
			uart_enable_backend(&uart->be, true);
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
uart_mevent_teardown(void *param)
{
	struct uart_vdev *uart = param;
	struct uart_backend *be;

	if (!uart)
		return;

	be = &uart->be;

	if (!be->opened)
		return;

	switch (be->be_type) {
	case UART_BE_STDIO:
		uart_reset_stdio();
		break;
	case UART_BE_TTY:
		if (be->fd > 0)
			close(be->fd);
		break;
	case UART_BE_SOCK:
		if (be->fd2 > 0)
			close(be->fd2);
		if (be->fd > 0)
			close(be->fd);
		break;
	default:
		break; /* nothing to do */
	}

	be->evp = NULL;
	be->evp2 = NULL;
	be->fd2 = -1;
	be->fd = -1;
	be->opened = false;
	be->be_type = UART_BE_INVALID;

	uart_deinit(uart);
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
uart_reset(struct uart_vdev *uart)
{
	uint16_t divisor;

	divisor = DEFAULT_RCLK / DEFAULT_BAUD / 16;
	uart->dll = divisor;
	uart->dlh = divisor >> 16;
	uart->msr = modem_status(uart->mcr);

	rxfifo_reset(uart, 1);	/* no fifo until enabled by software */

	/* set the right reset state here */
	uart->ier = 0;
	uart->thre_int_pending = true;
	uart_toggle_intr(uart);
}

static void
uart_drain(int fd, enum ev_type ev, void *arg)
{
	struct uart_vdev *uart;
	int ch;

	uart = arg;

	/*
	 * This routine is called in the context of the mevent thread
	 * to take out the uart lock to protect against concurrent
	 * access from a vCPU i/o exit
	 */
	pthread_mutex_lock(&uart->mtx);

	if ((uart->mcr & MCR_LOOPBACK) != 0) {
		(void) uart_backend_read(&uart->be);
	} else {
		/* only read tty when rxfifo available to make sure no data lost */
		while (rxfifo_available(uart) && (ch = uart_backend_read(&uart->be)) != -1)
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
		/* THRE INT is cleared after writing data into THR register */
		uart->thre_int_pending = false;
		uart_toggle_intr(uart);
		if (uart->mcr & MCR_LOOPBACK) {
			if (rxfifo_putchar(uart, value) != 0)
				uart->lsr |= LSR_OE;
		} else {
			uart_backend_write(&uart->be, value);
		} /* else drop on floor */

		/* We view the transmission is completed immediately */
		uart->thre_int_pending = true;
		break;
	case REG_IER:
		if (((uart->ier & IER_ETXRDY) == 0) &&
				((value & IER_ETXRDY) != 0))
			uart->thre_int_pending = true;
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
			fifosz = (value & FCR_ENABLE) ? uart->rxfifo_size : 1;
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
				rxfifo_reset(uart, uart->rxfifo_size);

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
		 * Reading the IIR register clears the THRE INT.
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

static struct uart_vdev *
uart_init(uart_intr_func_t intr_assert, uart_intr_func_t intr_deassert,
	void *arg, int rxfifo_size)
{
	struct uart_vdev *uart;

	uart = calloc(1, sizeof(struct uart_vdev) + rxfifo_size);
	if (uart) {
		uart->arg = arg;
		uart->rxfifo_size = rxfifo_size;
		uart->intr_assert = intr_assert;
		uart->intr_deassert = intr_deassert;
		uart->rxfifo.buf = (uint8_t *)(uart + 1);

		pthread_mutex_init(&uart->mtx, NULL);

		uart_reset(uart);
	}

	return uart;
}

static void
uart_deinit(struct uart_vdev *uart)
{
	if (uart)
		free(uart);
}

static void
uart_sock_accept(int fd __attribute__((unused)),
		 enum ev_type t __attribute__((unused)),
		 void *arg)
{
	struct uart_vdev *uart = (struct uart_vdev *)arg;
	int s, flags;

	s = accept(uart->be.fd, NULL, NULL);
	if (s < 0) {
		DPRINTF(("uart: accept error %d\n", s));
		return;
	}

	if (uart->be.opened) {
		DPRINTF(("uart: already connected\n"));
		close(s);
		return;
	}

	flags = fcntl(s, F_GETFL);
	fcntl(s, F_SETFL, flags | O_NONBLOCK);

	uart->be.opened = true;
	uart->be.fd2 = s;
	uart->be.evp2 = mevent_add(s, EVF_READ, uart_drain, uart,
		uart_mevent_teardown, uart);
	if (!uart->be.evp2)
		WPRINTF(("uart: mevent_add evp2 failed\n"));
	DPRINTF(("uart: %s\r\n", __func__));
}

static int
uart_backend_read(struct uart_backend *be)
{
	unsigned char rb;
	int rc = -1;

	if (!be || !be->opened)
		return -1;

	switch (be->be_type) {
	case UART_BE_STDIO:
	case UART_BE_TTY:
		/* fd is used to read */
		rc = read(be->fd, &rb, 1);
		break;
	case UART_BE_SOCK:
		rc = recv(be->fd2, &rb, 1, 0);
		if (rc <= 0 && errno != EAGAIN) {
			if (be->evp2) {
				mevent_delete(be->evp2);
				be->evp2 = NULL;
			}
			if (be->fd2 > 0) {
				close(be->fd2);
				be->fd2 = -1;
			}
			be->opened = false;
			WPRINTF(("%s connection closed, rc = %d, errno = %d\n",
				__func__, rc, errno));
		}
		break;
	default:
		WPRINTF(("not supported backend %d!\n", be->be_type));
	}

	if (rc <= 0)
		return -1;

	return rb;
}

static int
uart_backend_write(struct uart_backend *be, unsigned char wb)
{
	int rc = -1;

	if (!be || !be->opened)
		return -1;

	switch (be->be_type) {
	case UART_BE_STDIO:
	case UART_BE_TTY:
		/* fd2 is used to write */
		rc = write(be->fd2, &wb, 1);
		break;
	case UART_BE_SOCK:
		rc = send(be->fd2, &wb, 1, 0);
		if (rc != 1)
			WPRINTF(("%s: send error, rc = %d, errno = %d\r\n",
				__func__, rc, errno));
		break;
	default:
		WPRINTF(("not supported backend %d!\n", be->be_type));
	}

	return rc;
}

static int
uart_reset_backend(struct uart_backend *be)
{
	char flushbuf[32];
	ssize_t nread;
	int error;
	int fd;
	struct mevent *evp;

	if (!be || !be->opened)
		return -1;

	switch (be->be_type) {
	case UART_BE_STDIO:
	case UART_BE_TTY:
		fd = be->fd;
		evp = be->evp;
		break;
	case UART_BE_SOCK:
		fd = be->fd2;
		evp = be->evp2;
		break;
	default:
		WPRINTF(("not supported backend %d!\n", be->be_type));
		return -1;
	}

	/* Flush any unread input from the backend device. */
	while (1) {
		nread = read(fd, flushbuf, sizeof(flushbuf));
		if (nread != sizeof(flushbuf))
			break;
	}

	if (evp) {
		error = mevent_enable(evp);
		if (error) {
			WPRINTF(("mevent_enable error\n"));
			return -1;
		}
	}

	return 0;
}

static int
uart_enable_backend(struct uart_backend *be, bool enable)
{
	int error;
	struct mevent *evp;

	if (!be || !be->opened)
		return -1;

	switch (be->be_type) {
	case UART_BE_STDIO:
	case UART_BE_TTY:
		evp = be->evp;
		break;
	case UART_BE_SOCK:
		evp = be->evp2;
		break;
	default:
		WPRINTF(("not supported backend %d!\n", be->be_type));
		return -1;
	}

	if (evp) {
		if (enable)
			error = mevent_enable(evp);
		else
			error = mevent_disable(evp);
		if (error) {
			WPRINTF(("mevent %s error\n", enable ? "enable" : "disable"));
			return -1;
		}
	}

	return 0;
}

static int
uart_open_backend(struct uart_backend *be, const char *path,
		  enum uart_be_type be_type)
{
	int fd, rc = -1;

	switch (be_type) {
	case UART_BE_STDIO:
		if (stdio_in_use) {
			WPRINTF(("uart: stdio is used by other device\n"));
			break;
		}
		be->fd = STDIN_FILENO;
		be->fd2 = STDOUT_FILENO;
		stdio_in_use = true;
		rc = 0;
		break;
	case UART_BE_TTY:
		fd = open(path, O_RDWR | O_NONBLOCK);
		if (fd < 0)
			WPRINTF(("uart: open failed: %s\n", path));
		else if (!isatty(fd)) {
			WPRINTF(("uart: not a tty: %s\n", path));
			close(fd);
			fd = -1;
		} else {
			be->fd = fd;
			be->fd2 = fd;
			rc = 0;
		}
		break;
	case UART_BE_SOCK:
		fd = socket(AF_INET, SOCK_STREAM | O_NONBLOCK, 0);
		if (fd < 0)
			WPRINTF(("uart: open socket failed\n"));
		else {
			be->fd = fd;
			rc = 0;
		}
		break;
	default:
		WPRINTF(("not supported backend %d!\n", be_type));
	}

	return rc;
}

static int
uart_config_backend(struct uart_vdev *uart, struct uart_backend *be, long port)
{
	int fd, flags;
	struct termios tio, saved_tio;
	int opt = true;
	struct sockaddr_in addr;

	if (!be || be->fd == -1)
		return -1;

	fd = be->fd;
	switch (be->be_type) {
	case UART_BE_TTY:
	case UART_BE_STDIO:
		tcgetattr(fd, &tio);
		saved_tio = tio;
		cfmakeraw(&tio);
		tio.c_cflag |= CLOCAL;
		tcflush(fd, TCIOFLUSH);
		tcsetattr(fd, TCSANOW, &tio);

		if (be->be_type == UART_BE_STDIO) {
			flags = fcntl(fd, F_GETFL);
			fcntl(fd, F_SETFL, flags | O_NONBLOCK);
			tio_stdio_orig = saved_tio;
			atexit(uart_reset_stdio);
		}
		be->opened = true;
		/*
		 * When acrn-dm is started by acrnd as a background process,
		 * STDIO is redirected to journal log file. In this case epoll
		 * cannot be used on a regular file.
		 */
		if (isatty(fd)) {
			be->evp = mevent_add(fd, EVF_READ, uart_drain, uart,
				uart_mevent_teardown, uart);
			if (!be->evp) {
				WPRINTF(("uart: mevent_add failed\n"));
				return -1;
			}
		}
		break;
	case UART_BE_SOCK:
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,
			sizeof(opt)) < 0) {
			WPRINTF(("uart: setsockopt failed, errno = %d\n",
				errno));
			return -1;
		}
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(port);
		if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			WPRINTF(("uart: bind failed, errno = %d\n",
				errno));
			return -1;
		}
		if (listen(fd, 1) < 0) {
			WPRINTF(("uart: listen failed, errno = %d\n",
				errno));
			return -1;
		}
		be->opened = false;
		be->evp = mevent_add(fd, EVF_READ, uart_sock_accept, uart, NULL, NULL);
		if (!be->evp) {
			WPRINTF(("uart: mevent_add failed\n"));
			return -1;
		}
		break;
	default:
		break; /* nothing to do */
	}

	return 0;
}

struct uart_vdev *
uart_set_backend(uart_intr_func_t intr_assert, uart_intr_func_t intr_deassert,
	void *arg, const char *opts)
{
	int retval = -1;
	struct uart_vdev *uart;
	struct uart_backend *be = NULL;
	const char *path = NULL;
	enum uart_be_type be_type = UART_BE_INVALID;
	char *vopts, *p;
	long port = 0;
	int rxfifo_size = DEFAULT_FIFOSZ;

	if (opts == NULL) {
		uart = uart_init(intr_assert, intr_deassert, arg,
			rxfifo_size);
		return uart;
	}

	if (strncmp(opts, "tcp", 3) == 0) {
		be_type = UART_BE_SOCK;
		rxfifo_size = SOCK_FIFOSZ;
		vopts = strdup(opts);
		if (!vopts)
			goto opts_fail;

		p = vopts;
		if (!strsep(&p, ":") || dm_strtol(p, &p, 10, &port)) {
			free(vopts);
			goto opts_fail;
		}

		free(vopts);
		vopts = NULL;
	} else if (strcmp("stdio", opts) == 0) {
		be_type = UART_BE_STDIO;
	} else {
		be_type = UART_BE_TTY;
		path = opts;
	}

	uart = uart_init(intr_assert, intr_deassert, arg, rxfifo_size);
	if (!uart)
		goto init_fail;

	be = &uart->be;
	retval = uart_open_backend(be, path, be_type);
	if (retval < 0) {
		WPRINTF(("uart: open_backend failed\n"));
		goto open_fail;
	}

	be->be_type = be_type;
	if (uart_config_backend(uart, be, port) < 0) {
		WPRINTF(("uart: config_backend failed\n"));
		goto config_fail;
	}

	return uart;

config_fail:
	/* for all kinds of be, be->evp2 is not initialized */
	if (be->be_type == UART_BE_SOCK) {
		/* there is no teardown callback for socket listen fd */
		if (be->evp) {
			mevent_delete(be->evp);
			be->evp = NULL;
		}
		if (be->fd > 0) {
			close(be->fd);
			be->fd = -1;
		}
	} else if (be->evp)
		mevent_delete(be->evp);
	else
		uart_mevent_teardown(uart);
	return NULL;

open_fail:
	uart_deinit(uart);
init_fail:
opts_fail:
	return NULL;
}

void
uart_release_backend(struct uart_vdev *uart, const char *opts)
{
	struct uart_backend *be;

	if (uart == NULL)
		return;

	be = &uart->be;

	/*
	 * By current design, for the invalid PTY parameters, the virtual uarts
	 * are still expose to UOS but all data be dropped by backend service.
	 * The uart backend is not setup for this case, so don't try to release
	 * the uart backend in here.
	 * TODO: need re-visit the whole policy for such scenario in future.
	 */
	if (opts == NULL || be->be_type == UART_BE_INVALID) {
		uart_deinit(uart);
		return;
	}

	if (be->be_type == UART_BE_SOCK) {
		/* there is no teardown callback for socket listen fd */
		if (be->evp) {
			mevent_delete(be->evp);
			be->evp = NULL;
		}
		if (be->fd > 0) {
			close(be->fd);
			be->fd = -1;
		}
		if (be->evp2)
			mevent_delete(be->evp2);
		else
			uart_mevent_teardown(uart);
	} else {
		if (be->evp)
			mevent_delete(be->evp);
		else
			uart_mevent_teardown(uart);
	}
}
