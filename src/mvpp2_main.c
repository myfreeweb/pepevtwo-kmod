// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for Marvell PPv2 network controller for Armada 375 SoC.
 *
 * Copyright (C) 2014 Marvell
 *
 * Marcin Wojtas <mw@semihalf.com>
 */

#include "mvpp2.h"
#include "mvpp2_main.h"

void mvpp2_write(struct mvpp2 *priv, u32 offset, u32 data)
{
	bus_write_4(priv->base_res, offset, data);
}

u32 mvpp2_read(struct mvpp2 *priv, u32 offset)
{
	return bus_read_4(priv->base_res, offset);
}

static u32 mvpp2_read_relaxed(struct mvpp2 *priv, u32 offset)
{
	// XXX: not relaxed :D
	return bus_read_4(priv->base_res, offset);
}

static inline u32 mvpp2_cpu_to_thread(struct mvpp2 *priv, int cpu)
{
	// FreeBSD: we do not support the threads > max case
	return cpu;
}

/* These accessors should be used to access:
 *
 * - per-thread registers, where each thread has its own copy of the
 *   register.
 *
 *   MVPP2_BM_VIRT_ALLOC_REG
 *   MVPP2_BM_ADDR_HIGH_ALLOC
 *   MVPP22_BM_ADDR_HIGH_RLS_REG
 *   MVPP2_BM_VIRT_RLS_REG
 *   MVPP2_ISR_RX_TX_CAUSE_REG
 *   MVPP2_ISR_RX_TX_MASK_REG
 *   MVPP2_TXQ_NUM_REG
 *   MVPP2_AGGR_TXQ_UPDATE_REG
 *   MVPP2_TXQ_RSVD_REQ_REG
 *   MVPP2_TXQ_RSVD_RSLT_REG
 *   MVPP2_TXQ_SENT_REG
 *   MVPP2_RXQ_NUM_REG
 *
 * - global registers that must be accessed through a specific thread
 *   window, because they are related to an access to a per-thread
 *   register
 *
 *   MVPP2_BM_PHY_ALLOC_REG    (related to MVPP2_BM_VIRT_ALLOC_REG)
 *   MVPP2_BM_PHY_RLS_REG      (related to MVPP2_BM_VIRT_RLS_REG)
 *   MVPP2_RXQ_THRESH_REG      (related to MVPP2_RXQ_NUM_REG)
 *   MVPP2_RXQ_DESC_ADDR_REG   (related to MVPP2_RXQ_NUM_REG)
 *   MVPP2_RXQ_DESC_SIZE_REG   (related to MVPP2_RXQ_NUM_REG)
 *   MVPP2_RXQ_INDEX_REG       (related to MVPP2_RXQ_NUM_REG)
 *   MVPP2_TXQ_PENDING_REG     (related to MVPP2_TXQ_NUM_REG)
 *   MVPP2_TXQ_DESC_ADDR_REG   (related to MVPP2_TXQ_NUM_REG)
 *   MVPP2_TXQ_DESC_SIZE_REG   (related to MVPP2_TXQ_NUM_REG)
 *   MVPP2_TXQ_INDEX_REG       (related to MVPP2_TXQ_NUM_REG)
 *   MVPP2_TXQ_PENDING_REG     (related to MVPP2_TXQ_NUM_REG)
 *   MVPP2_TXQ_PREF_BUF_REG    (related to MVPP2_TXQ_NUM_REG)
 *   MVPP2_TXQ_PREF_BUF_REG    (related to MVPP2_TXQ_NUM_REG)
 */
static void mvpp2_thread_write(struct mvpp2 *priv, unsigned int thread,
			       u32 offset, u32 data)
{
	bus_write_4(priv->base_res, thread * MVPP22_ADDR_SPACE_SZ + offset, data);
}

static u32 mvpp2_thread_read(struct mvpp2 *priv, unsigned int thread,
			     u32 offset)
{
	return bus_read_4(priv->base_res, thread * MVPP22_ADDR_SPACE_SZ + offset);
}

static void mvpp2_thread_write_relaxed(struct mvpp2 *priv, unsigned int thread,
				       u32 offset, u32 data)
{
	// XXX: not relaxed :D
	bus_write_4(priv->base_res, thread * MVPP22_ADDR_SPACE_SZ + offset, data);
}

static u32 mvpp2_thread_read_relaxed(struct mvpp2 *priv, unsigned int thread,
				     u32 offset)
{
	// XXX: not relaxed :D
	return bus_read_4(priv->base_res, thread * MVPP22_ADDR_SPACE_SZ + offset);
}

void mvpp2_setup_bm_pool(void)
{
	/* Short pool */
	mvpp2_pools[MVPP2_BM_SHORT].buf_num  = MVPP2_BM_SHORT_BUF_NUM;
	mvpp2_pools[MVPP2_BM_SHORT].pkt_size = MVPP2_BM_SHORT_PKT_SIZE;

	/* Long pool */
	mvpp2_pools[MVPP2_BM_LONG].buf_num  = MVPP2_BM_LONG_BUF_NUM;
	mvpp2_pools[MVPP2_BM_LONG].pkt_size = MVPP2_BM_LONG_PKT_SIZE;

	/* Jumbo pool */
	mvpp2_pools[MVPP2_BM_JUMBO].buf_num  = MVPP2_BM_JUMBO_BUF_NUM;
	mvpp2_pools[MVPP2_BM_JUMBO].pkt_size = MVPP2_BM_JUMBO_PKT_SIZE;
}

/* Initialize Rx FIFO's */
void mvpp22_rx_fifo_init(struct mvpp2 *priv)
{
	int port;

	/* The FIFO size parameters are set depending on the maximum speed a
	 * given port can handle:
	 * - Port 0: 10Gbps
	 * - Port 1: 2.5Gbps
	 * - Ports 2 and 3: 1Gbps
	 */

	mvpp2_write(priv, MVPP2_RX_DATA_FIFO_SIZE_REG(0),
		    MVPP2_RX_FIFO_PORT_DATA_SIZE_32KB);
	mvpp2_write(priv, MVPP2_RX_ATTR_FIFO_SIZE_REG(0),
		    MVPP2_RX_FIFO_PORT_ATTR_SIZE_32KB);

	mvpp2_write(priv, MVPP2_RX_DATA_FIFO_SIZE_REG(1),
		    MVPP2_RX_FIFO_PORT_DATA_SIZE_8KB);
	mvpp2_write(priv, MVPP2_RX_ATTR_FIFO_SIZE_REG(1),
		    MVPP2_RX_FIFO_PORT_ATTR_SIZE_8KB);

	for (port = 2; port < MVPP2_MAX_PORTS; port++) {
		mvpp2_write(priv, MVPP2_RX_DATA_FIFO_SIZE_REG(port),
			    MVPP2_RX_FIFO_PORT_DATA_SIZE_4KB);
		mvpp2_write(priv, MVPP2_RX_ATTR_FIFO_SIZE_REG(port),
			    MVPP2_RX_FIFO_PORT_ATTR_SIZE_4KB);
	}

	mvpp2_write(priv, MVPP2_RX_MIN_PKT_SIZE_REG,
		    MVPP2_RX_FIFO_PORT_MIN_PKT);
	mvpp2_write(priv, MVPP2_RX_FIFO_INIT_REG, 0x1);
}

/* Initialize Tx FIFO's: the total FIFO size is 19kB on PPv2.2 and 10G
 * interfaces must have a Tx FIFO size of 10kB. As only port 0 can do 10G,
 * configure its Tx FIFO size to 10kB and the others ports Tx FIFO size to 3kB.
 */
void mvpp22_tx_fifo_init(struct mvpp2 *priv)
{
	int port, size, thrs;

	for (port = 0; port < MVPP2_MAX_PORTS; port++) {
		if (port == 0) {
			size = MVPP22_TX_FIFO_DATA_SIZE_10KB;
			thrs = MVPP2_TX_FIFO_THRESHOLD_10KB;
		} else {
			size = MVPP22_TX_FIFO_DATA_SIZE_3KB;
			thrs = MVPP2_TX_FIFO_THRESHOLD_3KB;
		}
		mvpp2_write(priv, MVPP22_TX_FIFO_SIZE_REG(port), size);
		mvpp2_write(priv, MVPP22_TX_FIFO_THRESH_REG(port), thrs);
	}
}

void mvpp2_axi_init(struct mvpp2 *priv)
{
	u32 val, rdval, wrval;

	mvpp2_write(priv, MVPP22_BM_ADDR_HIGH_RLS_REG, 0x0);

	/* AXI Bridge Configuration */

	rdval = MVPP22_AXI_CODE_CACHE_RD_CACHE
		<< MVPP22_AXI_ATTR_CACHE_OFFS;
	rdval |= MVPP22_AXI_CODE_DOMAIN_OUTER_DOM
		<< MVPP22_AXI_ATTR_DOMAIN_OFFS;

	wrval = MVPP22_AXI_CODE_CACHE_WR_CACHE
		<< MVPP22_AXI_ATTR_CACHE_OFFS;
	wrval |= MVPP22_AXI_CODE_DOMAIN_OUTER_DOM
		<< MVPP22_AXI_ATTR_DOMAIN_OFFS;

	/* BM */
	mvpp2_write(priv, MVPP22_AXI_BM_WR_ATTR_REG, wrval);
	mvpp2_write(priv, MVPP22_AXI_BM_RD_ATTR_REG, rdval);

	/* Descriptors */
	mvpp2_write(priv, MVPP22_AXI_AGGRQ_DESCR_RD_ATTR_REG, rdval);
	mvpp2_write(priv, MVPP22_AXI_TXQ_DESCR_WR_ATTR_REG, wrval);
	mvpp2_write(priv, MVPP22_AXI_TXQ_DESCR_RD_ATTR_REG, rdval);
	mvpp2_write(priv, MVPP22_AXI_RXQ_DESCR_WR_ATTR_REG, wrval);

	/* Buffer Data */
	mvpp2_write(priv, MVPP22_AXI_TX_DATA_RD_ATTR_REG, rdval);
	mvpp2_write(priv, MVPP22_AXI_RX_DATA_WR_ATTR_REG, wrval);

	val = MVPP22_AXI_CODE_CACHE_NON_CACHE
		<< MVPP22_AXI_CODE_CACHE_OFFS;
	val |= MVPP22_AXI_CODE_DOMAIN_SYSTEM
		<< MVPP22_AXI_CODE_DOMAIN_OFFS;
	mvpp2_write(priv, MVPP22_AXI_RD_NORMAL_CODE_REG, val);
	mvpp2_write(priv, MVPP22_AXI_WR_NORMAL_CODE_REG, val);

	val = MVPP22_AXI_CODE_CACHE_RD_CACHE
		<< MVPP22_AXI_CODE_CACHE_OFFS;
	val |= MVPP22_AXI_CODE_DOMAIN_OUTER_DOM
		<< MVPP22_AXI_CODE_DOMAIN_OFFS;

	mvpp2_write(priv, MVPP22_AXI_RD_SNOOP_CODE_REG, val);

	val = MVPP22_AXI_CODE_CACHE_WR_CACHE
		<< MVPP22_AXI_CODE_CACHE_OFFS;
	val |= MVPP22_AXI_CODE_DOMAIN_OUTER_DOM
		<< MVPP22_AXI_CODE_DOMAIN_OFFS;

	mvpp2_write(priv, MVPP22_AXI_WR_SNOOP_CODE_REG, val);
}
