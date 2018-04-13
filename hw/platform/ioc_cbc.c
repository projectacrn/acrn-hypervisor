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
 *
 */
/*
 * Carrier Board Commnucation protocol stack.
 */

#include <stdio.h>

#include "ioc.h"

/*
 * Debug printf
 */
static int ioc_cbc_debug;
#define	DPRINTF(fmt, args...) \
	do { if (ioc_cbc_debug) printf(fmt, ##args); } while (0)
#define	WPRINTF(fmt, args...) printf(fmt, ##args)

/*
 * Buffer bytes of reading from the virtual UART, because the bytes maybe can
 * not generate one complete CBC frame.
 */
int
cbc_copy_to_ring(const uint8_t *buf, size_t size, struct cbc_ring *ring)
{
	int i, pos;

	/* TODO: memcpy or other implementation instead copy byte one by one */
	for (i = 0; i < size; i++) {
		pos = (ring->tail + 1) & (CBC_RING_BUFFER_SIZE - 1);
		if (pos != ring->head) {
			ring->buf[ring->tail] =  buf[i];

			ring->tail = pos;
		} else {
			WPRINTF("ioc cbc ring buffer is full!!\r\n");
			return -1;
		}
	}
	return 0;
}

/*
 * Drop the bytes from the ring buffer.
 */
static inline void
cbc_ring_skips(struct cbc_ring *ring, size_t bytes)
{
	ring->head = (ring->head + bytes) & (CBC_RING_BUFFER_SIZE - 1);

}

/*
 * Caculate checksum
 */
static inline uint16_t
cbc_cal_chksum(const uint8_t *data, size_t size)
{
	int i;
	uint16_t value = 0;

	for (i = 0; i < size; i++)
		value += 0x100 - *(data + i);
	return value;
}

/*
 * Checksum verification
 */
static inline int
cbc_verify_chksum(struct cbc_ring *ring, size_t size, uint8_t c)
{
	int i, pos;
	uint16_t value = 0;

	for (i = 0; i < size; i++) {
		pos = (ring->head + i) & (CBC_RING_BUFFER_SIZE - 1);

		value +=  0x100 - *(ring->buf + pos);

	}
	return ((value & 0xFF) == c ? 0 : -1);
}

/*
 * Make the buf alignment with unit and pack out 0xFF if need.
 */
static size_t
cbc_fill_padding(uint8_t *buf, size_t size, int unit)
{
	size_t i, left;
	size_t paddings = size;

	left = size % unit;
	if (left != 0) {
		paddings = size + unit - left;
		for (i = size - CBC_CHKSUM_SIZE; i < paddings; i++)
			buf[i] = 0xFF;
	}
	return paddings;
}

/*
 * Unpack CBC link layer data.
 * The function trys to generate a CBC link frame then build a cbc_request and
 * put the cbc_request to the CBC rx queue.
 */
void
cbc_unpack_link(struct ioc_dev *ioc)
{
	/*
	 * To build one complete frame from the ring buffer,
	 * The remains means the real length of current frame.
	 * The avalids means the bytes in the ring buffer.
	 * If the avalids can not meet minimum frame length or remains,
	 * need to wait more bytes before CBC link unpack.
	 */
	static int remains, rx_seq_counter;
	uint8_t seq, len, ext, checksum;
	int avalids, frame_len, els_pos, chksum_pos;
	struct cbc_ring *ring = &ioc->ring;

	for (;;) {

		/* Get the avalid bytes in the ring buffer */
		avalids = ring->tail - ring->head;
		avalids = avalids >= 0 ? avalids :
			(CBC_RING_BUFFER_SIZE + avalids);

		/*
		 * The avalid bytes from ring buffer must be a minimum frame
		 * or the part of one complete frame remain bytes
		 */
		if (avalids < CBC_MIN_FRAME_SIZE || avalids < remains)
			break;

		/* Reset the remains flag if the avalid bytes can be parsed */
		remains = 0;

		/*
		 * If start of frame value is incorrect, drop the byte
		 * of ring buffer head
		 */
		if (ring->buf[ring->head] != CBC_SOF_VALUE) {
			cbc_ring_skips(ring, 1);
			continue;
		}

		/*
		 * Parse the extension, frame length and sequence
		 */
		els_pos = (ring->head + CBC_ELS_POS) &
			(CBC_RING_BUFFER_SIZE - 1);

		ext = (ring->buf[els_pos] >> CBC_EXT_OFFSET) & CBC_EXT_MASK;
		len = (ring->buf[els_pos] >> CBC_LEN_OFFSET) & CBC_LEN_MASK;
		seq = (ring->buf[els_pos] >> CBC_SEQ_OFFSET) & CBC_SEQ_MASK;

		/* FIXME: Extension defined in CBC protocol, but not use yet */
		(void)ext;

		/*
		 * CBC service block aligns with four types
		 * length is zero means four bytes, so length needs to add one
		 * and real len contains all CBC protocol headers length.
		 */
		len = (len + 1) * 4;
		frame_len = len + CBC_LINK_HDR_SIZE + CBC_ADDR_HDR_SIZE;

		/* Safty check */
		if (frame_len > CBC_MAX_FRAME_SIZE) {
			cbc_ring_skips(ring, 1);
			continue;
		}

		/* Need more bytes to build one complete CBC frame */
		if (avalids < frame_len) {
			remains = frame_len;
			continue;
		}

		/* Checksum verification */
		chksum_pos = (ring->head + frame_len - 1) &
			(CBC_RING_BUFFER_SIZE - 1);

		checksum = ring->buf[chksum_pos];
		if (cbc_verify_chksum(ring, frame_len - 1, checksum) != 0) {
			cbc_ring_skips(ring, 1);
			continue;
		}

		/*
		 * Rx sequence check
		 * TODO: just warning now, need to drop the frame or not?
		 */
		rx_seq_counter = (rx_seq_counter + 1) & CBC_SEQ_MASK;
		if (rx_seq_counter != seq) {
			WPRINTF("%s", "ioc rx sequence check falied\r\n");
			rx_seq_counter = seq;
		}

		/* Build a cbc_request and put it on the queue */
		ioc_build_request(ioc, frame_len, len);

		/* Drop the bytes from the ring buffer */
		cbc_ring_skips(ring, frame_len);
	}
}

/*
 * Pack CBC link header includes SOF value, extension bits, frame length bits,
 * tx sequence bits, link alignment paddings and checksum byte.
 */
static void
cbc_pack_link(struct cbc_pkt *pkt)
{
	static size_t tx_seq_counter;
	size_t len;
	uint16_t checksum = 0;

	/* Safty check */
	if (pkt->req->srv_len > CBC_MAX_SERVICE_SIZE) {
		DPRINTF("ioc pack req with wrong service length:%d\r\n",
					pkt->req->srv_len);
		return;
	}

	/*
	 * Compute one CBC frame length
	 * and aligh with CBC default granularity
	 */
	len = pkt->req->srv_len + CBC_ADDR_HDR_SIZE + CBC_LINK_HDR_SIZE;
	len = cbc_fill_padding(pkt->req->buf, len, CBC_GRANULARITY);

	/* Fill start of frame */
	pkt->req->buf[CBC_SOF_POS] = CBC_SOF_VALUE;

	/* Fill extension bits ,frame length bits and tx sequence bits */
	pkt->req->buf[CBC_ELS_POS] = (CBC_EXT_VALUE & CBC_EXT_MASK)
		<< CBC_EXT_OFFSET;

	pkt->req->buf[CBC_ELS_POS] |= (((pkt->req->srv_len - 1)/CBC_LEN_UNIT)
			& CBC_LEN_MASK) << CBC_LEN_OFFSET;

	pkt->req->buf[CBC_ELS_POS] |= (tx_seq_counter & CBC_SEQ_MASK)
		<< CBC_SEQ_OFFSET;

	/* Fill checksum that is last byte */
	checksum = cbc_cal_chksum(pkt->req->buf, len - 1);
	pkt->req->buf[len - 1] = checksum & 0xFF;

	/* Set the CBC link frame length */
	pkt->req->link_len = len;

	/* Increase tx sequence */
	tx_seq_counter = (tx_seq_counter + 1) & CBC_SEQ_MASK;
}

/*
 * Pack CBC address layer header includes channel mux and priority.
 */
static void
cbc_pack_address(struct cbc_pkt *pkt)
{
	uint8_t prio, mux;

	mux = pkt->req->id;
	switch (mux) {
	case IOC_NATIVE_PMT:
	case IOC_NATIVE_LFCC:
	case IOC_NATIVE_SIGNAL:
	case IOC_NATIVE_DLT:
		prio = CBC_PRIO_HIGH;
		break;
	case IOC_NATIVE_DIAG:
		prio = CBC_PRIO_LOW;
		break;
	default:
		prio = CBC_PRIO_MEDIUM;
		break;
	}
	pkt->req->buf[CBC_ADDR_POS] = ((mux & CBC_MUX_MASK) << CBC_MUX_OFFSET);
	pkt->req->buf[CBC_ADDR_POS] |=
		((prio & CBC_PRIO_MASK)<<CBC_PRIO_OFFSET);

}

/*
 * Send CBC packet buf to IOC xmit channel.
 * TODO: Due to rx/tx threads share the function, it is better to seperate.
 */
static void
cbc_send_pkt(struct cbc_pkt *pkt)
{
	int rc;
	uint8_t *data;
	size_t len;
	enum ioc_ch_id id;

	/*
	 * link_len is 0 means the packet from CBC cdev that there is no data
	 * of link layer, the packet is transmitted to virtual UART.
	 * Opposite, the packet is transmitted to CBC cdev.
	 */
	if (pkt->req->link_len == 0) {
		cbc_pack_address(pkt);
		cbc_pack_link(pkt);
		id = IOC_VIRTUAL_UART;
		data = pkt->req->buf;
		len = pkt->req->link_len;
	} else {
		id = pkt->req->id;
		data = pkt->req->buf + CBC_SRV_POS;
		len = pkt->req->srv_len;
	}
	rc = ioc_ch_xmit(id, data, len);
	if (rc < 0)
		DPRINTF("ioc xmit failed on channel id=%d\n\r", id);
}

/*
 * Update wakeup reason value and notify UOS immediately.
 * Some events can change the wakeup reason include periodic wakeup reason
 * from IOC firmware, IOC bootup reason, heartbeat state changing and VMM
 * callback.
 */
static void
cbc_update_wakeup_reason(struct cbc_pkt *pkt, uint32_t reason)
{
}

/*
 * CBC service lifecycle process.
 * FIXME: called in rx and tx threads, seperating two functions is better.
 */
static void
cbc_process_lifecycle(struct cbc_pkt *pkt)
{
}

/*
 * CBC service signal data process.
 * FIXME: called in rx and tx threads, seperating two functions is better.
 */
static void
cbc_process_signal(struct cbc_pkt *pkt)
{
}

/*
 * Rx handler mainly processes rx direction data flow
 * the rx direction is that virtual UART -> native CBC cdevs
 */
void
cbc_rx_handler(struct cbc_pkt *pkt)
{
	uint8_t mux, prio;

	/*
	 * FIXME: need to check CBC request type in the rx handler
	 * currently simply check is enough, expand the check in the further
	 */
	if (pkt->req->rtype != CBC_REQ_T_PROT)
		return;
	/*
	 * TODO: use this prio to enable dynamic cbc priority configuration
	 * feature in the future, currently ignore it.
	 */
	prio = (pkt->req->buf[CBC_ADDR_POS] >> CBC_PRIO_OFFSET) & CBC_PRIO_MASK;
	(void) prio;

	mux = (pkt->req->buf[CBC_ADDR_POS] >> CBC_MUX_OFFSET) & CBC_MUX_MASK;
	pkt->req->id = mux;
	switch (mux) {
	case IOC_NATIVE_LFCC:
		cbc_process_lifecycle(pkt);
		break;
	case IOC_NATIVE_SIGNAL:
		cbc_process_signal(pkt);
		break;

	/* Forward directly */
	case IOC_NATIVE_RAW0 ... IOC_NATIVE_RAW11:
		cbc_send_pkt(pkt);
		break;
	default:
		DPRINTF("ioc unpack wrong channel=%d\r\n", mux);
		break;
	}
}

/*
 * Tx handler mainly processes tx direction data flow,
 * the tx direction is that native CBC cdevs -> virtual UART.
 */
void
cbc_tx_handler(struct cbc_pkt *pkt)
{
	if (pkt->req->rtype == CBC_REQ_T_PROT) {
		switch (pkt->req->id) {
		case IOC_NATIVE_LFCC:
			cbc_process_lifecycle(pkt);
			break;
		case IOC_NATIVE_SIGNAL:
			cbc_process_signal(pkt);
			break;
		case IOC_NATIVE_RAW0 ... IOC_NATIVE_RAW11:
			cbc_send_pkt(pkt);
			break;
		default:
			DPRINTF("ioc cbc tx handler got invalid channel=%d\r\n",
					pkt->req->id);
			break;
		}
	} else if (pkt->req->rtype == CBC_REQ_T_SOC) {
		/*
		 * Update wakeup reasons with SoC new state
		 * the new state update by heartbeat state change
		 * (active/inactive) in rx thread
		 */
		pkt->soc_active = pkt->req->buf[0];
		cbc_update_wakeup_reason(pkt, pkt->reason);
	} else {
		/* TODO: others request types process */
		DPRINTF("ioc invalid cbc_request type in tx:%d\r\n",
				pkt->req->rtype);
	}
}
