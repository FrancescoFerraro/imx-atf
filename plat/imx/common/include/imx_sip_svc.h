/*
 * Copyright (c) 2015-2019, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __IMX_SIP_SVC_H__
#define __IMX_SIP_SVC_H__

/* SMC function IDs for SiP Service queries */
#define IMX_SIP_CPUFREQ			0xC2000001
#define IMX_SIP_SET_CPUFREQ		0x00

#define IMX_SIP_SRTC			0xC2000002
#define IMX_SIP_SRTC_SET_TIME		0x00
#define IMX_SIP_SRTC_START_WDOG		0x01
#define IMX_SIP_SRTC_STOP_WDOG		0x02
#define IMX_SIP_SRTC_SET_WDOG_ACT	0x03
#define IMX_SIP_SRTC_PING_WDOG		0x04
#define IMX_SIP_SRTC_SET_TIMEOUT_WDOG	0x05
#define IMX_SIP_SRTC_GET_WDOG_STAT	0x06
#define IMX_SIP_SRTC_SET_PRETIME_WDOG	0x07

#define IMX_SIP_BUILDINFO			0xC2000003
#define IMX_SIP_BUILDINFO_GET_COMMITHASH	0x00

#define IMX_SIP_DDR_DVFS		0xc2000004

#define IMX_SIP_SRC			0xC2000005
#define IMX_SIP_SRC_SET_SECONDARY_BOOT	0x10
#define IMX_SIP_SRC_IS_SECONDARY_BOOT	0x11

#define IMX_SIP_GET_SOC_INFO		0xC2000006

#define IMX_SIP_WAKEUP_SRC		0xC2000009
#define IMX_SIP_WAKEUP_SRC_SCU		0x1
#define IMX_SIP_WAKEUP_SRC_IRQSTEER	0x2

#define IMX_SIP_OTP_READ		0xC200000A
#define IMX_SIP_OTP_WRITE		0xC200000B

#define IMX_SIP_MISC_SET_TEMP		0xC200000C

#define IMX_SIP_AARCH32			0xC20000FD

int imx_kernel_entry_handler(uint32_t smc_fid, u_register_t x1,
			     u_register_t x2, u_register_t x3,
			     u_register_t x4);
#if defined(PLAT_imx8mq)
int imx_soc_info_handler(uint32_t smc_fid, u_register_t x1,
			 u_register_t x2, u_register_t x3);
#endif
#if defined(PLAT_imx8mm) || defined(PLAT_imx8mn) || defined(PLAT_imx8mp)
int dram_dvfs_handler(uint32_t smc_fid, void *handle,
	u_register_t x1, u_register_t x2, u_register_t x3);
#endif

#if defined(PLAT_imx8mm) || defined(PLAT_imx8mq)
int imx_src_handler(uint32_t smc_fid, u_register_t x1,
		    u_register_t x2, u_register_t x3, void *handle);
#endif

#if (defined(PLAT_imx8qm) || defined(PLAT_imx8qx) || defined(PLAT_imx8dx) || defined(PLAT_imx8dxl))
int imx_cpufreq_handler(uint32_t smc_fid, u_register_t x1,
			u_register_t x2, u_register_t x3);
int imx_srtc_handler(uint32_t smc_fid, void *handle, u_register_t x1,
		     u_register_t x2, u_register_t x3, u_register_t x4);
int imx_wakeup_src_handler(uint32_t smc_fid, u_register_t x1,
			   u_register_t x2, u_register_t x3);
int imx_otp_handler(uint32_t smc_fid, void *handle,
		    u_register_t x1, u_register_t x2);
int imx_misc_set_temp_handler(uint32_t smc_fid, u_register_t x1,
			      u_register_t x2, u_register_t x3,
			      u_register_t x4);
int imx_get_cpu_rev(uint32_t *cpu_id, uint32_t *cpu_rev);
#endif
uint64_t imx_buildinfo_handler(uint32_t smc_fid, u_register_t x1,
			       u_register_t x2, u_register_t x3,
			       u_register_t x4);

#endif /* __IMX_SIP_SVC_H__ */
