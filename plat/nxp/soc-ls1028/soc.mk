#
# Copyright 2018-2021 NXP
#
# SPDX-License-Identifier: BSD-3-Clause
#

# SoC-specific build parameters
SOC			:=	ls1028
PLAT_PATH		:=	plat/nxp
PLAT_COMMON_PATH	:=	plat/nxp/common
PLAT_DRIVERS_PATH	:=	drivers/nxp
PLAT_SOC_PATH		:=	${PLAT_PATH}/soc-${SOC}
BOARD_PATH		:=	${PLAT_SOC_PATH}/${BOARD}

# Get SoC-specific definitions
include ${PLAT_SOC_PATH}/soc.def
include ${PLAT_COMMON_PATH}/soc_common_def.mk

ifeq (${TRUSTED_BOARD_BOOT},1)
$(eval $(call SET_FLAG,SMMU_NEEDED,BL2))
$(eval $(call SET_FLAG,SFP_NEEDED,BL2))
$(eval $(call SET_FLAG,SNVS_NEEDED,BL2))
SECURE_BOOT := yes
endif
$(eval $(call SET_FLAG,CRYPTO_NEEDED,BL_COMM))

$(eval $(call SET_FLAG,DCFG_NEEDED,BL_COMM))
$(eval $(call SET_FLAG,TIMER_NEEDED,BL_COMM))
$(eval $(call SET_FLAG,INTERCONNECT_NEEDED,BL_COMM))
$(eval $(call SET_FLAG,GIC_NEEDED,BL31))
$(eval $(call SET_FLAG,CONSOLE_NEEDED,BL_COMM))
$(eval $(call SET_FLAG,PMU_NEEDED,BL_COMM))
$(eval $(call SET_FLAG,DDR_DRIVER_NEEDED,BL2))
$(eval $(call SET_FLAG,TZASC_NEEDED,BL2))
$(eval $(call SET_FLAG,I2C_NEEDED,BL2))
$(eval $(call SET_FLAG,IMG_LOADR_NEEDED,BL2))

# Selecting PSCI & SIP_SVC support
$(eval $(call SET_FLAG,PSCI_NEEDED,BL31))
$(eval $(call SET_FLAG,SIPSVC_NEEDED,BL31))

# Selecting boot source
ifeq (${BOOT_MODE}, flexspi_nor)
$(eval $(call SET_FLAG,XSPI_NEEDED,BL2))
$(eval $(call add_define,FLEXSPI_NOR_BOOT))
else
ifeq (${BOOT_MODE}, sd)
$(eval $(call SET_FLAG,SD_MMC_NEEDED,BL2))
$(eval $(call add_define,SD_BOOT))
else
ifeq (${BOOT_MODE}, emmc)
$(eval $(call SET_FLAG,SD_MMC_NEEDED,BL2))
$(eval $(call add_define,EMMC_BOOT))
else
$(error Un-supported Boot Mode = ${BOOT_MODE})
endif
endif
endif

PLAT_INCLUDES		+=	-I${PLAT_COMMON_PATH}/include/default\
				-I${BOARD_PATH}\
				-I${PLAT_COMMON_PATH}/include/default/ch_${CHASSIS}\
				-I${PLAT_SOC_PATH}/include\
				-I${PLAT_COMMON_PATH}/soc_errata

ifeq (${SECURE_BOOT},yes)
include ${PLAT_COMMON_PATH}/tbbr/tbbr.mk
endif

ifeq ($(WARM_BOOT),yes)
include ${PLAT_COMMON_PATH}/warm_reset/warm_reset.mk
endif

ifeq (${NXP_NV_SW_MAINT_LAST_EXEC_DATA}, yes)
include ${PLAT_COMMON_PATH}/nv_storage/nv_storage.mk
endif

ifeq (${PSCI_NEEDED}, yes)
include ${PLAT_COMMON_PATH}/psci/psci.mk
endif

ifeq (${SIPSVC_NEEDED}, yes)
include ${PLAT_COMMON_PATH}/sip_svc/sipsvc.mk
endif

ifeq (${DDR_FIP_IO_NEEDED}, yes)
include ${PLAT_COMMON_PATH}/fip_handler/ddr_fip/ddr_fip_io.mk
endif

# For fuse-fip & fuse-programming
ifeq (${FUSE_PROG}, 1)
include ${PLAT_COMMON_PATH}/fip_handler/fuse_fip/fuse.mk
endif

ifeq (${IMG_LOADR_NEEDED},yes)
include $(PLAT_COMMON_PATH)/img_loadr/img_loadr.mk
endif

# Add source files for the above selected drivers.
include ${PLAT_DRIVERS_PATH}/drivers.mk

# Add SoC specific files
include ${PLAT_COMMON_PATH}/soc_errata/errata.mk

PLAT_INCLUDES		+=	${NV_STORAGE_INCLUDES}\
				${WARM_RST_INCLUDES}

BL31_SOURCES		+=	${PLAT_SOC_PATH}/$(ARCH)/${SOC}.S\
				${WARM_RST_BL31_SOURCES}\
				${PSCI_SOURCES}\
				${SIPSVC_SOURCES}\
				${PLAT_COMMON_PATH}/$(ARCH)/bl31_data.S

PLAT_BL_COMMON_SOURCES	+=	${PLAT_COMMON_PATH}/$(ARCH)/ls_helpers.S\
				${PLAT_SOC_PATH}/aarch64/${SOC}_helpers.S\
				${NV_STORAGE_SOURCES}\
				${WARM_RST_BL_COMM_SOURCES}\
				${PLAT_SOC_PATH}/soc.c

ifeq (${TEST_BL31}, 1)
BL31_SOURCES	+=	${PLAT_SOC_PATH}/$(ARCH)/bootmain64.S \
			${PLAT_SOC_PATH}/$(ARCH)/nonboot64.S
endif

BL2_SOURCES		+=	${DDR_CNTLR_SOURCES}\
				${TBBR_SOURCES}\
				${FUSE_SOURCES}

# Add TFA setup files
include ${PLAT_PATH}/common/setup/common.mk
