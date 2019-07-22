// SPDX-License-Identifier: GPL-2.0
#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

#include "mvpp2.h"
#include "mvpp2_main.h"
#include "mvpp2_prs.h"
#include "mvpp2_cls.h"

MALLOC_DEFINE(M_PEPEVTWO, "pepevtwo", "pepevtwo driver memeory");

static int
pepevtwo_probe(device_t dev)
{
	ACPI_HANDLE h;

	if ((h = acpi_get_handle(dev)) == NULL ||
			acpi_MatchHid(h, "MRVL0110") == ACPI_MATCHHID_NOMATCH)
		return (ENXIO);

	device_set_desc(dev, "Marvell PPv2.2 (UNOFFICIAL WIP DRIVER)");

	return (BUS_PROBE_DEFAULT);
}

static int pepevtwo_detach(device_t dev);

static int
pepevtwo_attach(device_t dev)
{
	struct mvpp2 *priv = device_get_softc(dev);
	int rid = 0;

	priv->base_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (priv->base_res == NULL) {
		device_printf(dev, "Failed to map base\n");
		return (ENXIO);
	}

	rid = 1;
	priv->iface_base_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (priv->iface_base_res == NULL) {
		device_printf(dev, "Failed to map iface_base\n");
		pepevtwo_detach(dev);
		return (ENXIO);
	}

	priv->hw_version = MVPP22;
	priv->max_port_rxqs = 32;
	priv->tclk = 333333333; // for MACCHIATObin

	mvpp2_setup_bm_pool();

	priv->nthreads = num_online_cpus();
	if (priv->nthreads > MVPP2_MAX_THREADS) {
		device_printf(dev, "Where did you get an Armada SoC with %d cores????\n", priv->nthreads);
		pepevtwo_detach(dev);
		return (ENXIO);
	}

	priv->max_port_rxqs = 32;

	// err = dma_set_mask(&pdev->dev, MVPP2_DESC_DMA_MASK);
	// err = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));

	// mvpp2_init //
	mvpp2_axi_init(priv);

	/* Disable HW PHY polling */
	u32 val = bus_read_4(priv->iface_base_res, MVPP22_SMI_MISC_CFG_REG);
	val &= ~MVPP22_SMI_POLLING_EN;
	bus_write_4(priv->iface_base_res, MVPP22_SMI_MISC_CFG_REG, val);

	/* Allocate and initialize aggregated TXQs */
	priv->aggr_txqs = mallocarray(MVPP2_MAX_THREADS, sizeof(*priv->aggr_txqs), M_PEPEVTWO, M_WAITOK | M_ZERO);
	if (priv->aggr_txqs == NULL) {
		device_printf(dev, "Could not alloc aggr_txqs\n");
		pepevtwo_detach(dev);
		return (ENOMEM);
	}
	for (int i = 0; i < MVPP2_MAX_THREADS; i++) {
		priv->aggr_txqs[i].id = i;
		priv->aggr_txqs[i].size = MVPP2_AGGR_TXQ_SIZE;
		// mvpp2_aggr_txq_init //
		struct mvpp2_tx_queue *aggr_txq = &priv->aggr_txqs[i];
		/* TODO: Allocate memory for TX descriptors */

		aggr_txq->last_desc = MVPP2_AGGR_TXQ_SIZE - 1;

		/* Aggr TXQ no reset WA */
		aggr_txq->next_desc_to_proc = mvpp2_read(priv, MVPP2_AGGR_TXQ_INDEX_REG(i));

		// TODO: more DMA
	}

	/* Fifo Init */
	mvpp22_rx_fifo_init(priv);
	mvpp22_tx_fifo_init(priv);

	/* Allow cache snoop when transmiting packets */
	mvpp2_write(priv, MVPP2_TX_SNOOP_REG, 0x1);

	/* Buffer Manager initialization */
	// mvpp2_bm_init //
	for (int i = 0; i < MVPP2_BM_POOLS_NUM; i++) {
		/* Mask BM all interrupts */
		mvpp2_write(priv, MVPP2_BM_INTR_MASK_REG(i), 0);
		/* Clear BM cause register */
		mvpp2_write(priv, MVPP2_BM_INTR_CAUSE_REG(i), 0);
	}

	/* Allocate and initialize BM pools */
	priv->bm_pools = mallocarray(MVPP2_BM_POOLS_NUM, sizeof(*priv->bm_pools), M_PEPEVTWO, M_WAITOK | M_ZERO);
	if (priv->bm_pools == NULL) {
		device_printf(dev, "Could not alloc bm_pools\n");
		pepevtwo_detach(dev);
		return (ENOMEM);
	}

	// mvpp2_bm_pools_init //
	for (int i = 0; i < MVPP2_BM_POOLS_NUM; i++) {
		struct mvpp2_bm_pool *bm_pool = &priv->bm_pools[i];
		bm_pool->id = i;
		bm_pool->size_bytes = 2 * sizeof(u64) * MVPP2_BM_POOL_SIZE_MAX;
		// TODO: DMA
	}

	/* Parser default initialization */
	mvpp2_prs_default_init(M_PEPEVTWO, priv);

	/* Classifier default initialization */
	mvpp2_cls_init(priv);

	// TODO: ports

	return (0);
}

static int
pepevtwo_detach(device_t dev)
{
	struct mvpp2 *priv = device_get_softc(dev);

	if (priv->prs_double_vlans != NULL) {
		free(priv->prs_double_vlans, M_PEPEVTWO);
	}

	if (priv->prs_shadow != NULL) {
		free(priv->prs_shadow, M_PEPEVTWO);
	}

	if (priv->bm_pools != NULL) {
		free(priv->bm_pools, M_PEPEVTWO);
	}

	if (priv->aggr_txqs != NULL) {
		free(priv->aggr_txqs, M_PEPEVTWO);
	}

	if (priv->base_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, rman_get_rid(priv->base_res), priv->base_res);
	}

	if (priv->iface_base_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, rman_get_rid(priv->iface_base_res), priv->iface_base_res);
	}

	return (0);
}

static device_method_t pepevtwo_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, pepevtwo_probe),
	DEVMETHOD(device_attach, pepevtwo_attach),
	DEVMETHOD(device_detach, pepevtwo_detach),
	DEVMETHOD_END
};

static driver_t pepevtwo_driver = {
	"pepevtwo", pepevtwo_methods, sizeof(struct mvpp2),
};

devclass_t pepevtwodevclass;
DRIVER_MODULE(pepevtwo, acpi, pepevtwo_driver, pepevtwodevclass, 0, 0);
MODULE_DEPEND(pepevtwo, ether, 1, 1, 1);
MODULE_DEPEND(pepevtwo, linuxkpi, 1, 1, 1);
