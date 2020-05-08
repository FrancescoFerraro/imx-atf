/*
 * Copyright 2019 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <bl31/interrupt_mgmt.h>
#include <common/runtime_svc.h>
#include <lib/mmio.h>
#include <lib/spinlock.h>
#include <plat/common/platform.h>

#include <dram.h>
#include <gpc.h>

#define IMX_SIP_DDR_DVFS_GET_FREQ_COUNT		0x10
#define IMX_SIP_DDR_DVFS_GET_FREQ_INFO		0x11

#define TIMING_CFG_PTR(ptr, old_base, new_base)	\
	((struct dram_cfg_param *)(((uint64_t)(ptr) & ~(uint64_t)(old_base)) + (uint64_t)(new_base)))

struct dram_info dram_info;

/* lock used for DDR DVFS */
spinlock_t dfs_lock;

#if defined(PLAT_imx8mq)
/* ocram used to dram timing */
static uint8_t dram_timing_saved[13 * 1024] __aligned(8);
#endif

static volatile uint32_t wfe_done;
static volatile bool wait_ddrc_hwffc_done = true;
static unsigned int dev_fsp = 0x1;

static uint32_t fsp_init_reg[3][4] = {
	{ DDRC_INIT3(0), DDRC_INIT4(0), DDRC_INIT6(0), DDRC_INIT7(0) },
	{ DDRC_FREQ1_INIT3(0), DDRC_FREQ1_INIT4(0), DDRC_FREQ1_INIT6(0), DDRC_FREQ1_INIT7(0) },
	{ DDRC_FREQ2_INIT3(0), DDRC_FREQ2_INIT4(0), DDRC_FREQ2_INIT6(0), DDRC_FREQ2_INIT7(0) },
};


#if defined (PLAT_imx8mq)
/* copy the dram timing info from DRAM to OCRAM */
void imx8mq_dram_timing_copy(struct dram_timing_info *from)
{
	struct dram_timing_info *info = (struct dram_timing_info *)dram_timing_saved;

	/* copy the whole 13KB content used for dram timing info */
	memcpy(dram_timing_saved, from, sizeof(dram_timing_saved));

	/* correct the header after copied into ocram */
	info->ddrc_cfg = TIMING_CFG_PTR(info->ddrc_cfg, from, dram_timing_saved);
	info->ddrphy_cfg = TIMING_CFG_PTR(info->ddrphy_cfg, from, dram_timing_saved);
	info->ddrphy_trained_csr = TIMING_CFG_PTR(info->ddrphy_trained_csr, from, dram_timing_saved);
	info->ddrphy_pie = TIMING_CFG_PTR(info->ddrphy_pie, from, dram_timing_saved);
}
#endif

static void get_mr_values(uint32_t (*mr_value)[8])
{
	uint32_t init_val;
	int i, fsp_index;

	for (fsp_index = 0; fsp_index < 3; fsp_index++) {
		for(i = 0; i < 4; i++) {
			init_val = mmio_read_32(fsp_init_reg[fsp_index][i]);
			mr_value[fsp_index][2*i] = init_val >> 16;
			mr_value[fsp_index][2*i + 1] = init_val & 0xFFFF;
		}
	}
}

static void save_rank_setting(void)
{
	uint32_t i, offset;
	uint32_t pstate_num = dram_info.num_fsp;

	for(i = 0; i < pstate_num; i++) {
		offset = i ? (i + 1) * 0x1000 : 0;
		if (dram_info.dram_type == DDRC_LPDDR4) {
			dram_info.rank_setting[i][0] = mmio_read_32(DDRC_DRAMTMG2(0) + offset);
		} else {
			dram_info.rank_setting[i][0] = mmio_read_32(DDRC_DRAMTMG2(0) + offset);
			dram_info.rank_setting[i][1] = mmio_read_32(DDRC_DRAMTMG9(0) + offset);
		}
#if !defined(PLAT_imx8mq)
		dram_info.rank_setting[i][2] = mmio_read_32(DDRC_RANKCTL(0) + offset);
#endif
	}
#if defined(PLAT_imx8mq)
	dram_info.rank_setting[0][2] = mmio_read_32(DDRC_RANKCTL(0));
#endif
}
/* Restore the ddrc configs */
void dram_umctl2_init(struct dram_timing_info *timing)
{
	struct dram_cfg_param *ddrc_cfg = timing->ddrc_cfg;
	int i;

	for (i =0;  i < timing->ddrc_cfg_num; i++) {
		mmio_write_32(ddrc_cfg->reg, ddrc_cfg->val);
		ddrc_cfg++;
	}

	/* set the default fsp to P0 */
	mmio_write_32(DDRC_MSTR2(0), 0x0);
}

/* Restore the dram PHY config */
void dram_phy_init(struct dram_timing_info *timing)
{
	struct dram_cfg_param *cfg = timing->ddrphy_cfg;
	int i;

	/* Restore the PHY init config */
	cfg = timing->ddrphy_cfg;
	for (i = 0; i < timing->ddrphy_cfg_num; i++) {
		dwc_ddrphy_apb_wr(cfg->reg, cfg->val);
		cfg++;
	}

	/* Restore the DDR PHY CSRs */
	cfg = timing->ddrphy_trained_csr;
	for (i = 0; i < timing->ddrphy_trained_csr_num; i++) {
		dwc_ddrphy_apb_wr(cfg->reg, cfg->val);
		cfg++;
	}

	/* Load the PIE image */
	cfg = timing->ddrphy_pie;
	for (i = 0; i < timing->ddrphy_pie_num; i++) {
		dwc_ddrphy_apb_wr(cfg->reg, cfg->val);
		cfg++;
	}
}

/* EL3 SGI-8 IPI handler for DDR Dynamic frequency scaling */
static uint64_t waiting_dvfs(uint32_t id, uint32_t flags,
				void *handle, void *cookie)
{
	uint64_t mpidr = read_mpidr_el1();
	unsigned int cpu_id = MPIDR_AFFLVL0_VAL(mpidr);
	uint32_t irq;

	irq = plat_ic_acknowledge_interrupt();
	if (irq < 1022U)
		plat_ic_end_of_interrupt(irq);

	/* set the WFE done status */
	spin_lock(&dfs_lock);
	wfe_done |= (1 << cpu_id * 8);
	dsb();
	spin_unlock(&dfs_lock);

	while (1) {
		/* ddr frequency change done */
		if (!wait_ddrc_hwffc_done)
			break;

		wfe();
	}

	return 0;
}

void dram_info_init(unsigned long dram_timing_base)
{
	uint32_t ddrc_mstr, current_fsp;
	uint32_t flags = 0;
	uint32_t rc;
	int i;

	/* Get the dram type & rank */
	ddrc_mstr = mmio_read_32(DDRC_MSTR(0));

	dram_info.dram_type = ddrc_mstr & DDR_TYPE_MASK;
	dram_info.num_rank = ((ddrc_mstr >> 24) & ACTIVE_RANK_MASK) == 0x3 ?
		DDRC_ACTIVE_TWO_RANK : DDRC_ACTIVE_ONE_RANK;

	/* Get current fsp info */
	current_fsp = mmio_read_32(DDRC_DFIMISC(0)) & 0xf;
	dram_info.boot_fsp = current_fsp;
	dram_info.current_fsp = current_fsp;

#if defined(PLAT_imx8mq)
	imx8mq_dram_timing_copy((struct dram_timing_info *)dram_timing_base);

	dram_timing_base = (unsigned long) dram_timing_saved;
#endif

	get_mr_values(dram_info.mr_table);

	dram_info.timing_info = (struct dram_timing_info *)dram_timing_base;

	/* get the num of supported fsp */
	for (i = 0; i < 4; ++i)
		if (!dram_info.timing_info->fsp_table[i])
			break;
	dram_info.num_fsp = i;

	/* save the DRAMTMG2/9 for rank to rank workaround */
	save_rank_setting();

	/* check if has bypass mode support */
	if (dram_info.timing_info->fsp_table[i-1] < 666)
		dram_info.bypass_mode = true;
	else
		dram_info.bypass_mode = false;

	/* Register the EL3 handler for DDR DVFS */
	set_interrupt_rm_flag(flags, NON_SECURE);
	rc = register_interrupt_type_handler(INTR_TYPE_EL3, waiting_dvfs, flags);
	if (rc)
		panic();

	if (dram_info.dram_type == DDRC_LPDDR4 && current_fsp != 0x0) {
		/* flush the L1/L2 cache */
		dcsw_op_all(DCCSW);
		lpddr4_swffc(&dram_info, dev_fsp, 0x0);
		dev_fsp = (~dev_fsp) & 0x1;
	} else if (current_fsp != 0x0) {
		/* flush the L1/L2 cache */
		dcsw_op_all(DCCSW);
		ddr4_swffc(&dram_info, 0x0);
	}
}


/*
 * For each freq return the following info:
 *
 * r1: data rate
 * r2: 1 + dram_core parent
 * r3: 1 + dram_alt parent index
 * r4: 1 + dram_apb parent index
 *
 * The parent indices can be used by an OS who manages source clocks to enabled
 * them ahead of the switch.
 *
 * A parent value of "0" means "don't care".
 *
 * Current implementation of freq switch is hardcoded in
 * plat/imx/common/imx8m/clock.c but in theory this can be enhanced to support
 * a wide variety of rates.
 */
int dram_dvfs_get_freq_info(void *handle, u_register_t index)
{
	switch (index) {
	case 0: SMC_RET4(handle, dram_info.timing_info->fsp_table[0],
				1, 0, 5);
	case 1:
		if (!dram_info.bypass_mode)
			SMC_RET4(handle, dram_info.timing_info->fsp_table[1],
					1, 0, 0);
		SMC_RET4(handle, dram_info.timing_info->fsp_table[1],
				2, 2, 4);
	case 2:
		if (!dram_info.bypass_mode)
			SMC_RET4(handle, dram_info.timing_info->fsp_table[2],
					1, 0, 0);
		SMC_RET4(handle, dram_info.timing_info->fsp_table[2],
				2, 3, 3);
	case 3: SMC_RET4(handle, dram_info.timing_info->fsp_table[3],
				1, 0, 0);
	default:
		SMC_RET1(handle, -3);
	}
}

int dram_dvfs_handler(uint32_t smc_fid, void *handle,
	u_register_t x1, u_register_t x2, u_register_t x3)
{
	uint64_t mpidr = read_mpidr_el1();
	unsigned int cpu_id = MPIDR_AFFLVL0_VAL(mpidr);
	unsigned int fsp_index = x1;
	uint32_t online_cores = x2;

	if (IMX_SIP_DDR_DVFS_GET_FREQ_COUNT == x1) {
		SMC_RET1(handle, dram_info.num_fsp);
	} else if (IMX_SIP_DDR_DVFS_GET_FREQ_INFO == x1) {
		return dram_dvfs_get_freq_info(handle, x2);
	} else if (x1 < 4) {
		wait_ddrc_hwffc_done = true;
		dsb();

		/* trigger the SGI IPI to info other cores */
		for (int i = 0; i < PLATFORM_CORE_COUNT; i++)
			if (cpu_id != i && (online_cores & (0x1 << (i * 8))))
				plat_ic_raise_el3_sgi(0x8, i);
#if defined(PLAT_imx8mq)
		for (int i = 0; i < 4; i++) {
			if (i != cpu_id && online_cores & (1 << (i * 8)))
				imx_gpc_core_wake(1 << i);
		}
#endif

		/* make sure all the core in WFE */
		online_cores &= ~(0x1 << (cpu_id * 8));
		while (1)
			if (online_cores == wfe_done)
				break;

		/* flush the L1/L2 cache */
		dcsw_op_all(DCCSW);

		if (dram_info.dram_type == DDRC_LPDDR4) {
			lpddr4_swffc(&dram_info, dev_fsp, fsp_index);
			dev_fsp = (~dev_fsp) & 0x1;
		} else {
			ddr4_swffc(&dram_info, fsp_index);
		}

		dram_info.current_fsp = fsp_index;
		wait_ddrc_hwffc_done = false;
		wfe_done = 0;
		dsb();
		sev();
		isb();
	}

	SMC_RET1(handle, 0);
}
