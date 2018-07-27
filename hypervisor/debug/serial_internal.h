/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SERIAL_INTER_H
#define SERIAL_INTER_H

struct shared_buf;

/* Maximum serial devices supported by the platform. */
#define SERIAL_MAX_DEVS                 1U

/* Maximum length of unique id of each UART port enabled in platform. */
#define SERIAL_ID_MAX_LENGTH            8

/* SERIAL error values */
#define SERIAL_SUCCESS                  0
#define SERIAL_EOF                     -1
#define SERIAL_ERROR                   -2
#define SERIAL_DEV_NOT_FOUND           -3
#define INVALID_COM_PORT               -4
#define SERIAL_NO_char_AVAIL           -5

#define SERIAL_INVALID_HANDLE          0xFFFFFFFF

/* Pending interrupt defines */
#define SD_NO_INTERRUPT                   0
#define SD_RX_INTERRUPT                   1

/* RX error defines */
#define SD_RX_NO_ERROR                    0U
#define SD_RX_OVERRUN_ERROR               1U
#define SD_RX_PARITY_ERROR                2U
#define SD_RX_FRAME_ERROR                 3U

/* Defines for encoding/decoding the unique UART handle of each port. */

#define SERIAL_MAGIC_NUM               0x005500AAU
#define SERIAL_VALIDATE_HANDLE(handle) \
	((handle & 0xFFFF00FF) == (SERIAL_MAGIC_NUM))
#define SERIAL_ENCODE_INDEX(index)	((SERIAL_MAGIC_NUM) | (index << 8))
#define SERIAL_DECODE_INDEX(handle)	((handle & 0x0000FF00U) >> 8)

#define         NO_SUSPEND                       0
#define         SUSPEND                          0xFFFFFFFFU

/* Enumeration values to set UART Configuration */
typedef enum _baudenum_ {
	/* Baud Rate Options */
	BAUD_110 = 110,		/* not supported on OMAP5 */
	BAUD_300 = 300,
	BAUD_600 = 600,
	BAUD_1200 = 1200,
	BAUD_2400 = 2400,
	BAUD_4800 = 4800,
	BAUD_9600 = 9600,
	BAUD_14400 = 14400,
	BAUD_19200 = 19200,
	BAUD_28800 = 28800,
	BAUD_38400 = 38400,
	BAUD_57600 = 57600,
	BAUD_115200 = 115200,
	BAUD_230400 = 230400,
	BAUD_460800 = 460800,
	BAUD_921600 = 921600,
	BAUD_1843000 = 1843000,
	BAUD_36884000 = 36884000
} BAUD_ENUM;

typedef enum _flowenum_ {
	/* Flow Control Bits */
	FLOW_NONE = 0,
	FLOW_HARD = 1,
	FLOW_X = 2
} FLOW_ENUM;

typedef enum _parityenum_ {
	/* Parity Bits */
	PARITY_NONE = 0,
	PARITY_ODD = 1,
	PARITY_EVEN = 2,
	PARITY_MARK = 3,
	PARITY_SPACE = 4
} PARITY_ENUM;

typedef enum _stopenum_ {
	/* Stop Bits */
	STOP_1 = 1,
	STOP_2 = 2
} STOP_ENUM;

typedef enum _dataenum_ {
	/* Data bits */
	DATA_7 = 7,
	DATA_8 = 8
} DATA_ENUM;

/* Control Block definition about error in Rx data */
struct rx_error {
	uint32_t parity_errors;
	uint32_t frame_errors;
	uint32_t overrun_errors;
	uint32_t general_errors;
};

/* Control Block definition for configuration specific
 * parameters of UART
 */
struct uart_config {
	uint32_t data_bits;
	uint32_t stop_bits;
	uint32_t parity_bits;
	uint32_t baud_rate;
	uint32_t flow_control;

	/* Read mode of UART port in interrupt mode. It can be NO_SUSPEND or
	 * SUSPEND or (1-4,294,967,293). SUSPEND means unlimited blocking,
	 * NO_SUSPEND means non-blocking and some integer value means timeout
	 * blocking support. By default, it is set to SUSPEND.
	 */
	uint32_t read_mode;

};

/* Control Block definition for target specific driver
 * of UART
 */
struct tgt_uart {
	char uart_id[SERIAL_ID_MAX_LENGTH];
	uint64_t base_address;
	uint32_t clock_frequency;
	uint32_t buffer_size;
	uint32_t open_count;

	/* Target specific function pointers. */
	int (*init)(struct tgt_uart *tgt_uart);
	int (*open)(struct tgt_uart *tgt_uart, struct uart_config *config);
	void (*close)(struct tgt_uart *tgt_uart);
	void (*read)(struct tgt_uart *tgt_uart,
				void *buffer, uint32_t *bytes_read);
	void (*write)(struct tgt_uart *tgt_uart,
				const void *buffer, uint32_t *bytes_written);
	bool (*tx_is_busy)(struct tgt_uart *tgt_uart);
	bool (*rx_data_is_avail)(struct tgt_uart *tgt_uart, uint32_t *lsr_reg);
	uint32_t (*get_rx_err)(uint32_t rx_data);
};

/* Control Block definition of light-weight serial driver */
struct uart {
	/* Pointer to target specific Control Block of UART */
	struct tgt_uart *tgt_uart;

	/* Configuration of UART */
	struct uart_config config;

	/* Errors in data received from UART port */
	struct rx_error rx_error;

	/* Pointer to receive circular buffer */
	struct shared_buf *rx_sio_queue;

	/* Lock to provide mutual exclusion for transmitting data to UART port*/
	spinlock_t tx_lock;

	/* Lock to provide mutual exclusion for accessing shared buffer */
	spinlock_t buffer_lock;

	/* Flag to indicate whether UART port is opened or not */
	uint8_t open_flag;

};

/* Null terminated array of target specific UART control blocks */
extern struct tgt_uart Tgt_Uarts[SERIAL_MAX_DEVS];

uint32_t serial_open(const char *uart_id);
int serial_getc(uint32_t uart_handle);
int serial_gets(uint32_t uart_handle, char *buffer, uint32_t length_arg);
int serial_puts(uint32_t uart_handle, const char *s_arg, uint32_t length_arg);
uint32_t serial_get_rx_data(uint32_t uart_handle);

#endif /* !SERIAL_INTER_H */
