#
# Copyright 2018-2021 NXP
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Board-specific build parameters
BOOT_MODE	?=	flexspi_nor
BOARD		:=	ls1028aqds
POVDD_ENABLE	:=	no
WARM_BOOT	:=	no

# DDR build parameters
NUM_OF_DDRC		:=	1
DDRC_NUM_DIMM		:=	1
DDR_ECC_EN		:=	yes

# On-board flash
FLASH_TYPE	:=	MT35XU02G
XSPI_FLASH_SZ	:=	0x10000000

BL2_SOURCES	+=	${BOARD_PATH}/ddr_init.c \
			${BOARD_PATH}/platform.c

# Add platform board build info
include plat/nxp/common/plat_common_def.mk

# Add SoC build info
include plat/nxp/soc-ls1028/soc.mk
