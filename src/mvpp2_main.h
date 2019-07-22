// SPDX-License-Identifier: GPL-2.0
#pragma once

enum mvpp2_bm_pool_log_num {
	MVPP2_BM_SHORT,
	MVPP2_BM_LONG,
	MVPP2_BM_JUMBO,
	MVPP2_BM_POOLS_NUM
};

static struct {
	int pkt_size;
	int buf_num;
} mvpp2_pools[MVPP2_BM_POOLS_NUM];

void mvpp2_setup_bm_pool(void);
void mvpp22_rx_fifo_init(struct mvpp2 *priv);
void mvpp22_tx_fifo_init(struct mvpp2 *priv);
void mvpp2_axi_init(struct mvpp2 *priv);
