/** @file
*
*  Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __TH500_DEFINES_H__
#define __TH500_DEFINES_H__

#define TH500_BL_CARVEOUT_OFFSET  0x910

// SOCKETS
#define TH500_PRIMARY_SOCKET  0
#define TH500_MAX_SOCKETS     4
#define TH500_SOCKET_SHFT     44

// SBSA ACS
#define ARMARCH_TMR_HYPVIRT_PPI  28

// UART0
#define TH500_UART0_INTR  0xCA

// ETHERNET
#define TH500_ETHERNET_BASE_ADDR  0x03B40000
#define TH500_ETHERNET_CAR_SIZE   0x00000100
#define TH500_ETHERNET_INTR       0xFA

// LIC SW IO0 for EINJ
#define TH500_SW_IO0_BASE  0x03F00000

// LIC SW IO1 for GPU _RST
#define TH500_SW_IO1_BASE_SOCKET_0  0x000003F10000ULL
#define TH500_SW_IO1_BASE_SOCKET_1  0x100003F10000ULL
#define TH500_SW_IO1_BASE_SOCKET_2  0x200003F10000ULL
#define TH500_SW_IO1_BASE_SOCKET_3  0x300003F10000ULL
#define TH500_SW_IO1_SIZE           0x1000

// LIC SW IO2 for GENERIC EVENT DEVICE (GED)
#define TH500_SW_IO2_BASE  0x03F20000
#define TH500_SW_IO2_SIZE  0x1000
#define TH500_SW_IO2_INTR  0xE2

// LIC SW IO3 for PCIe DPC
#define TH500_SW_IO3_BASE_SOCKET_0  0x000003F30000ULL
#define TH500_SW_IO3_BASE_SOCKET_1  0x100003F30000ULL
#define TH500_SW_IO3_BASE_SOCKET_2  0x200003F30000ULL
#define TH500_SW_IO3_BASE_SOCKET_3  0x300003F30000ULL
#define TH500_SW_IO3_SIZE           0x1000
#define TH500_SW_IO3_INTR_SOCKET_0  227
#define TH500_SW_IO3_INTR_SOCKET_1  547
#define TH500_SW_IO3_INTR_SOCKET_2  867
#define TH500_SW_IO3_INTR_SOCKET_3  4291

// LIC SW IO4 for PCIE _OST()
#define TH500_SW_IO4_BASE_SOCKET_0  0x000003F40000ULL
#define TH500_SW_IO4_BASE_SOCKET_1  0x100003F40000ULL
#define TH500_SW_IO4_BASE_SOCKET_2  0x200003F40000ULL
#define TH500_SW_IO4_BASE_SOCKET_3  0x300003F40000ULL
#define TH500_SW_IO4_SIZE           0x1000
#define TH500_SW_IO4_INTR_SOCKET_0  228
#define TH500_SW_IO4_INTR_SOCKET_1  548
#define TH500_SW_IO4_INTR_SOCKET_2  868
#define TH500_SW_IO4_INTR_SOCKET_3  4292

// LIC SW IO6 for ERST
#define TH500_SW_IO6_BASE  0x03F60000

// PCIE
#define TH500_PCIE_ADDRESS_BITS  49

// GIC
#define TH500_GIC_DISTRIBUTOR_BASE             0x22000000
#define TH500_GIC_REDISTRIBUTOR_BASE_SOCKET_0  0x000022080000ULL
#define TH500_GIC_REDISTRIBUTOR_BASE_SOCKET_1  0x100022080000ULL
#define TH500_GIC_REDISTRIBUTOR_BASE_SOCKET_2  0x200022080000ULL
#define TH500_GIC_REDISTRIBUTOR_BASE_SOCKET_3  0x300022080000ULL
#define TH500_GIC_REDISTRIBUTOR_INSTANCES      84

// GIC ITS
#define TH500_GIC_ITS_BASE_SOCKET_0  0x000022040000
#define TH500_GIC_ITS_BASE_SOCKET_1  0x100022040000
#define TH500_GIC_ITS_BASE_SOCKET_2  0x200022040000
#define TH500_GIC_ITS_BASE_SOCKET_3  0x300022040000

// WDT
#define TH500_WDT_CTRL_BASE   0x0C6AB000
#define TH500_WDT_RFRSH_BASE  0x0C6AA000

// SCRATCH
#define TH500_SCRATCH_BASE_SOCKET_0  0x00000C390000ULL
#define TH500_SCRATCH_BASE_SOCKET_1  0x10000C390000ULL
#define TH500_SCRATCH_BASE_SOCKET_2  0x20000C390000ULL
#define TH500_SCRATCH_BASE_SOCKET_3  0x30000C390000ULL
#define TH500_SCRATCH_SIZE           SIZE_64KB

// Platform CPU Floorsweeping scratch offsets from TH500_SCRATCH_BASE_SOCKET_X
#define TH500_CPU_FLOORSWEEPING_DISABLE_OFFSET_0  0x78
#define TH500_CPU_FLOORSWEEPING_DISABLE_OFFSET_1  0x7C
#define TH500_CPU_FLOORSWEEPING_DISABLE_OFFSET_2  0x80

// Platform CPU Floorsweeping scratch masks
#define TH500_CPU_FLOORSWEEPING_DISABLE_MASK_0  0x00000000
#define TH500_CPU_FLOORSWEEPING_DISABLE_MASK_1  0x00000000
#define TH500_CPU_FLOORSWEEPING_DISABLE_MASK_2  0xFFF00000

// Platform SatMC Core
#define TH500_CPU_FLOORSWEEPING_SATMC_CORE_BIT_LO   25
#define TH500_CPU_FLOORSWEEPING_SATMC_CORE_BIT_HI   31
#define TH500_CPU_FLOORSWEEPING_SATMC_CORE_INVALID  0x7F

// PCI-e floorsweeping scratch offset from TH500_SCRATCH_BASE_SOCKET_X
#define TH500_PCIE_FLOORSWEEPING_DISABLE_OFFSET  0x74

// Platform PCI-e floorsweeping scratch masks
#define TH500_PCIE_SIM_FLOORSWEEPING_INFO      0x1F3
#define TH500_PCIE_FPGA_FLOORSWEEPING_INFO     0x0FF
#define TH500_PCIE_FLOORSWEEPING_DISABLE_MASK  0xFFFFFC00

#define PCIE_ID_TO_SOCKET(PcieId)     ((PcieId) >> 4)
#define PCIE_ID_TO_INTERFACE(PcieId)  ((PcieId) & 0xfUL)

// NVLW floorsweeping
#define NVLM_DISABLE_OFFSET  0x74
#define NVLM_DISABLE_MASK    0xFFFFF3FF
#define NVLM_DISABLE_SHIFT   10

// C2C floorsweeping
#define C2C_DISABLE_OFFSET  0x74
#define C2C_DISABLE_MASK    0xFFFFEFFF
#define C2C_DISABLE_SHIFT   12

// Memory channel floorsweeping
#define HALF_CHIP_DISABLE_OFFSET  0x74
#define HALF_CHIP_DISABLE_MASK    0xFFFF9FFF
#define HALF_CHIP_DISABLE_SHIFT   13

// CCPLEX CSN floorsweeping
#define CCPLEX_CSN_DISABLE_OFFSET_0  0x84
#define CCPLEX_CSN_DISABLE_OFFSET_1  0x88
#define CCPLEX_CSN_DISABLE_MASK_0    0x00000000
#define CCPLEX_CSN_DISABLE_MASK_1    0xFFFFFE00

// CCPLEX SOC bridge floorsweeping
#define CCPLEX_SOC_BR_DISABLE_OFFSET  0x88
#define CCPLEX_SOC_BR_DISABLE_MASK    0xFFFFC3FF
#define CCPLEX_SOC_BR_DISABLE_SHIFT   10

// CCPLEX MCF bridge floorsweeping
#define CCPLEX_MCF_BR_DISABLE_OFFSET  0x88
#define CCPLEX_MCF_BR_DISABLE_MASK    0xFFC03FFF
#define CCPLEX_MCF_BR_DISABLE_SHIFT   14

// SCF Cache floorsweeping scratch offsets from TH500_SCRATCH_BASE_SOCKET_X
#define TH500_SCF_CACHE_FLOORSWEEPING_DISABLE_OFFSET_0  0x8C
#define TH500_SCF_CACHE_FLOORSWEEPING_DISABLE_OFFSET_1  0x90
#define TH500_SCF_CACHE_FLOORSWEEPING_DISABLE_OFFSET_2  0x94

// Platform SCF Cache floorsweeping scratch masks
#define TH500_SCF_CACHE_FLOORSWEEPING_DISABLE_MASK_0  0x00000000
#define TH500_SCF_CACHE_FLOORSWEEPING_DISABLE_MASK_1  0x00000000
#define TH500_SCF_CACHE_FLOORSWEEPING_DISABLE_MASK_2  0xFFF00000

#define SCF_CACHE_SLICE_SIZE  (SIZE_1MB + SIZE_512KB)
#define SCF_CACHE_SLICE_SETS  2048

// MCF Channel floorsweeping
#define MCF_CHANNEL_DISABLE_OFFSET  0x98
#define MCF_CHANNEL_DISABLE_MASK    0x00000000

// UPHY floorsweeping
#define UPHY_LANE_OWNERSHIP_OFFSET  0x9C
#define UPHY_LANE_OWNERSHIP_MASK    0xFFFC0000

// UPHY Lane Bifurcation
#define UPHY_LANE_BIFURCATION_X16        1
#define UPHY_LANE_BIFURCATION_2X8        2
#define UPHY_BIFURCATION_PER_SOCKET      4
#define UPHY_LANE_BIFURCATION_DELAY_OFF  3000000
#define UPHY_LANE_BIFURCATION_DELAY_ON   10000000

// CBB FABRIC
#define TH500_CBB_FABRIC_BASE_SOCKET_0  0x000013a00000ULL
#define TH500_CBB_FABRIC_BASE_SOCKET_1  0x100013a00000ULL
#define TH500_CBB_FABRIC_BASE_SOCKET_2  0x200013a00000ULL
#define TH500_CBB_FABRIC_BASE_SOCKET_3  0x300013a00000ULL
#define TH500_CBB_FABRIC_SIZE           0x10000
#define TH500_CBB_FABRIC_32BIT_LOW      0x0
#define TH500_CBB_FABRIC_32BIT_HIGH     0x4
#define TH500_CBB_FABRIC_32BIT_SIZE     0x8
#define TH500_CBB_FABRIC_64BIT_LOW      0x10
#define TH500_CBB_FABRIC_64BIT_HIGH     0x14
#define TH500_CBB_FABRIC_64BIT_SIZE     0x18

#define TH500_VDM_SIZE   0x10000000
#define TH500_ECAM_SIZE  0x10000000

// MSS
#define TH500_MSS_BASE_SOCKET_0     0x000004040000ULL
#define TH500_MSS_BASE_SOCKET_1     0x100004040000ULL
#define TH500_MSS_BASE_SOCKET_2     0x200004040000ULL
#define TH500_MSS_BASE_SOCKET_3     0x300004040000ULL
#define TH500_MSS_SIZE              0x20000
#define TH500_MSS_C2C_MODE          0xC910
#define TH500_MSS_C2C_MODE_ONE_GPU  0x0
#define TH500_MSS_C2C_MODE_TWO_GPU  0x1

// SOCKET AMAP
#define TH500_AMAP_START_SOCKET_0  0x000000000000ULL
#define TH500_AMAP_START_SOCKET_1  0x100000000000ULL
#define TH500_AMAP_START_SOCKET_2  0x200000000000ULL
#define TH500_AMAP_START_SOCKET_3  0x300000000000ULL
#define TH500_AMAP_START_MASK      0x300000000000ULL
#define TH500_AMAP_START_SHFT      44
#define TH500_AMAP_GET_SOCKET(addr)      (((UINT64)addr & TH500_AMAP_START_MASK) >> TH500_AMAP_START_SHFT)
#define TH500_AMAP_GET_ADD(addr, socid)  ((UINT64)addr | ((UINT64)socid << TH500_AMAP_START_SHFT))

#define PLATFORM_MAX_SOCKETS             (PcdGet32 (PcdTegraMaxSockets))
#define TH500_HV_EGM_PXM_DOMAIN_START    4
#define TH500_GPU_PXM_DOMAIN_START       4
#define TH500_GPU_HBM_PXM_DOMAIN_START   (TH500_GPU_PXM_DOMAIN_START + PLATFORM_MAX_SOCKETS)
#define TH500_GPU_MAX_NR_MEM_PARTITIONS  8
#define TH500_TOTAL_PROXIMITY_DOMAINS    (TH500_GPU_HBM_PXM_DOMAIN_START + TH500_GPU_MAX_NR_MEM_PARTITIONS * PLATFORM_MAX_SOCKETS)
#define TH500_GPU_HBM_PXM_DOMAIN_START_FOR_GPU_ID(gpuId)  (TH500_GPU_HBM_PXM_DOMAIN_START + ((gpuId) * TH500_GPU_MAX_NR_MEM_PARTITIONS))
#define TH500_GPU_MAX_PXM_DOMAINS  ((PcdGet32 (PcdTegraMaxSockets)) * TH500_GPU_MAX_NR_MEM_PARTITIONS)

// BPMP
#define BPMP_TX_MAILBOX_SOCKET_0  0x00004007C000ULL
#define BPMP_TX_MAILBOX_SOCKET_1  0x10004007C000ULL
#define BPMP_TX_MAILBOX_SOCKET_2  0x20004007C000ULL
#define BPMP_TX_MAILBOX_SOCKET_3  0x30004007C000ULL
#define BPMP_RX_MAILBOX_SOCKET_0  0x00004007D000ULL
#define BPMP_RX_MAILBOX_SOCKET_1  0x10004007D000ULL
#define BPMP_RX_MAILBOX_SOCKET_2  0x20004007D000ULL
#define BPMP_RX_MAILBOX_SOCKET_3  0x30004007D000ULL
#define BPMP_CHANNEL_SIZE         0x100
#define BPMP_DOORBELL_SOCKET_0    0x000003C90300ULL
#define BPMP_DOORBELL_SOCKET_1    0x100003C90300ULL
#define BPMP_DOORBELL_SOCKET_2    0x200003C90300ULL
#define BPMP_DOORBELL_SOCKET_3    0x300003C90300ULL
#define BPMP_DOORBELL_SIZE        0x100

// Thermal Zones
#define ZONE_TEMP                  1
#define TH500_THERMAL_ZONE_CPU0    0
#define TH500_THERMAL_ZONE_CPU1    1
#define TH500_THERMAL_ZONE_CPU2    2
#define TH500_THERMAL_ZONE_CPU3    3
#define TH500_THERMAL_ZONE_SOC0    4
#define TH500_THERMAL_ZONE_SOC1    5
#define TH500_THERMAL_ZONE_SOC2    6
#define TH500_THERMAL_ZONE_SOC3    7
#define TH500_THERMAL_ZONE_SOC4    8
#define TH500_THERMAL_ZONE_TJ_MAX  9
#define TH500_THERMAL_ZONE_TJ_MIN  10
#define TH500_THERMAL_ZONE_TJ_AVG  11
#define TEMP_POLL_TIME_100MS       1                  // 100ms
#define TH500_THERMAL_ZONE_PSV     1000               // indicates tenths of deg C
#define TH500_THERMAL_ZONE_CRT     1045               // indicates tenths of deg C
#define TH500_THERMAL_ZONE_TC1     2
#define TH500_THERMAL_ZONE_TC2     8
#define TH500_THERMAL_ZONE_TSP     1                  // 100ms
#define TH500_THERMAL_ZONE_TFP     10                 // 10ms

// Power Meter
#define TH500_MODULE_PWR                      0
#define TH500_TH500_PWR                       1
#define TH500_CPU_PWR                         2
#define TH500_SOC_PWR                         3
#define TH500_MAX_PWR_METER                   4
#define TH500_MODULE_PWR_IDX_VALID_FLAG       0x00000010
#define TH500_TH500_PWR_IDX_VALID_FLAG        0x00000020
#define TH500_CPU_PWR_IDX_VALID_FLAG          0x00000040
#define TH500_SOC_PWR_IDX_VALID_FLAG          0x00000080
#define TH500_MODULE_PWR_1SEC_IDX_VALID_FLAG  0x00000100
#define TH500_TH500_PWR_1SEC_IDX_VALID_FLAG   0x00000200
#define TH500_CPU_PWR_1SEC_IDX_VALID_FLAG     0x00000400
#define TH500_SOC_PWR_1SEC_IDX_VALID_FLAG     0x00000800
#define TH500_BPMP_IPC_CALL_INTERVAL_50MS     500000
#define TH500_TEL_LAYOUT_VALID_FLAGS0_IDX     90
#define TH500_TEL_LAYOUT_VALID_FLAGS1_IDX     91
#define TH500_TEL_LAYOUT_VALID_FLAGS2_IDX     92
#define TH500_TEL_LAYOUT_DRAM_RATE_IDX        58

// SMMU CMDQV

#define TH500_SMMU0_BASE_SOCKET_0         0x000011000000ULL
#define TH500_SMMU0_BASE_SOCKET_1         0x100011000000ULL
#define TH500_SMMU0_BASE_SOCKET_2         0x200011000000ULL
#define TH500_SMMU0_BASE_SOCKET_3         0x300011000000ULL
#define TH500_SMMU0_CMDQV_BASE_SOCKET_0   0x000011200000ULL
#define TH500_SMMU0_CMDQV_LIMIT_SOCKET_0  0x000011A2FFFFULL
#define TH500_SMMU0_CMDQV_BASE_SOCKET_1   0x100011200000ULL
#define TH500_SMMU0_CMDQV_LIMIT_SOCKET_1  0x100011A2FFFFULL
#define TH500_SMMU0_CMDQV_BASE_SOCKET_2   0x200011200000ULL
#define TH500_SMMU0_CMDQV_LIMIT_SOCKET_2  0x200011A2FFFFULL
#define TH500_SMMU0_CMDQV_BASE_SOCKET_3   0x300011200000ULL
#define TH500_SMMU0_CMDQV_LIMIT_SOCKET_3  0x300011A2FFFFULL
#define TH500_SMMU0_CMDQV_SIZE            0x00830000
#define TH500_SMMU0_CMDQV_INTR_SOCKET_0   283
#define TH500_SMMU0_CMDQV_INTR_SOCKET_1   603
#define TH500_SMMU0_CMDQV_INTR_SOCKET_2   923
#define TH500_SMMU0_CMDQV_INTR_SOCKET_3   4347

#define TH500_SMMU1_BASE_SOCKET_0         0x000012000000ULL
#define TH500_SMMU1_BASE_SOCKET_1         0x100012000000ULL
#define TH500_SMMU1_BASE_SOCKET_2         0x200012000000ULL
#define TH500_SMMU1_BASE_SOCKET_3         0x300012000000ULL
#define TH500_SMMU1_CMDQV_BASE_SOCKET_0   0x000012200000ULL
#define TH500_SMMU1_CMDQV_LIMIT_SOCKET_0  0x000012A2FFFFULL
#define TH500_SMMU1_CMDQV_BASE_SOCKET_1   0x100012200000ULL
#define TH500_SMMU1_CMDQV_LIMIT_SOCKET_1  0x100012A2FFFFULL
#define TH500_SMMU1_CMDQV_BASE_SOCKET_2   0x200012200000ULL
#define TH500_SMMU1_CMDQV_LIMIT_SOCKET_2  0x200012A2FFFFULL
#define TH500_SMMU1_CMDQV_BASE_SOCKET_3   0x300012200000ULL
#define TH500_SMMU1_CMDQV_LIMIT_SOCKET_3  0x300012A2FFFFULL
#define TH500_SMMU1_CMDQV_SIZE            0x00830000
#define TH500_SMMU1_CMDQV_INTR_SOCKET_0   292
#define TH500_SMMU1_CMDQV_INTR_SOCKET_1   612
#define TH500_SMMU1_CMDQV_INTR_SOCKET_2   932
#define TH500_SMMU1_CMDQV_INTR_SOCKET_3   4356

#define TH500_SMMU2_BASE_SOCKET_0         0x000015000000ULL
#define TH500_SMMU2_BASE_SOCKET_1         0x100015000000ULL
#define TH500_SMMU2_BASE_SOCKET_2         0x200015000000ULL
#define TH500_SMMU2_BASE_SOCKET_3         0x300015000000ULL
#define TH500_SMMU2_CMDQV_BASE_SOCKET_0   0x000015200000ULL
#define TH500_SMMU2_CMDQV_LIMIT_SOCKET_0  0x000015A2FFFFULL
#define TH500_SMMU2_CMDQV_BASE_SOCKET_1   0x100015200000ULL
#define TH500_SMMU2_CMDQV_LIMIT_SOCKET_1  0x100015A2FFFFULL
#define TH500_SMMU2_CMDQV_BASE_SOCKET_2   0x200015200000ULL
#define TH500_SMMU2_CMDQV_LIMIT_SOCKET_2  0x200015A2FFFFULL
#define TH500_SMMU2_CMDQV_BASE_SOCKET_3   0x300015200000ULL
#define TH500_SMMU2_CMDQV_LIMIT_SOCKET_3  0x300015A2FFFFULL
#define TH500_SMMU2_CMDQV_SIZE            0x00830000
#define TH500_SMMU2_CMDQV_INTR_SOCKET_0   301
#define TH500_SMMU2_CMDQV_INTR_SOCKET_1   621
#define TH500_SMMU2_CMDQV_INTR_SOCKET_2   941
#define TH500_SMMU2_CMDQV_INTR_SOCKET_3   4365

#define TH500_GSMMU0_BASE_SOCKET_0         0x000016000000ULL
#define TH500_GSMMU0_BASE_SOCKET_1         0x100016000000ULL
#define TH500_GSMMU0_BASE_SOCKET_2         0x200016000000ULL
#define TH500_GSMMU0_BASE_SOCKET_3         0x300016000000ULL
#define TH500_GSMMU0_CMDQV_BASE_SOCKET_0   0x000016200000ULL
#define TH500_GSMMU0_CMDQV_LIMIT_SOCKET_0  0x000016A2FFFFULL
#define TH500_GSMMU0_CMDQV_BASE_SOCKET_1   0x100016200000ULL
#define TH500_GSMMU0_CMDQV_LIMIT_SOCKET_1  0x100016A2FFFFULL
#define TH500_GSMMU0_CMDQV_BASE_SOCKET_2   0x200016200000ULL
#define TH500_GSMMU0_CMDQV_LIMIT_SOCKET_2  0x200016A2FFFFULL
#define TH500_GSMMU0_CMDQV_BASE_SOCKET_3   0x300016200000ULL
#define TH500_GSMMU0_CMDQV_LIMIT_SOCKET_3  0x300016A2FFFFULL
#define TH500_GSMMU0_CMDQV_SIZE            0x00830000
#define TH500_GSMMU0_CMDQV_INTR_SOCKET_0   310
#define TH500_GSMMU0_CMDQV_INTR_SOCKET_1   630
#define TH500_GSMMU0_CMDQV_INTR_SOCKET_2   950
#define TH500_GSMMU0_CMDQV_INTR_SOCKET_3   4374

#define TH500_GSMMU1_BASE_SOCKET_0         0x000005000000ULL
#define TH500_GSMMU1_BASE_SOCKET_1         0x100005000000ULL
#define TH500_GSMMU1_BASE_SOCKET_2         0x200005000000ULL
#define TH500_GSMMU1_BASE_SOCKET_3         0x300005000000ULL
#define TH500_GSMMU1_CMDQV_BASE_SOCKET_0   0x000005200000ULL
#define TH500_GSMMU1_CMDQV_LIMIT_SOCKET_0  0x000005A2FFFFULL
#define TH500_GSMMU1_CMDQV_BASE_SOCKET_1   0x100005200000ULL
#define TH500_GSMMU1_CMDQV_LIMIT_SOCKET_1  0x100005A2FFFFULL
#define TH500_GSMMU1_CMDQV_BASE_SOCKET_2   0x200005200000ULL
#define TH500_GSMMU1_CMDQV_LIMIT_SOCKET_2  0x200005A2FFFFULL
#define TH500_GSMMU1_CMDQV_BASE_SOCKET_3   0x300005200000ULL
#define TH500_GSMMU1_CMDQV_LIMIT_SOCKET_3  0x300005A2FFFFULL
#define TH500_GSMMU1_CMDQV_SIZE            0x00830000
#define TH500_GSMMU1_CMDQV_INTR_SOCKET_0   319
#define TH500_GSMMU1_CMDQV_INTR_SOCKET_1   639
#define TH500_GSMMU1_CMDQV_INTR_SOCKET_2   959
#define TH500_GSMMU1_CMDQV_INTR_SOCKET_3   4383

// MCF SMMU
#define TH500_MCF_SMMU_SOCKET_0                 0x000004010000ULL
#define TH500_MCF_SMMU_SOCKET_1                 0x100004010000ULL
#define TH500_MCF_SMMU_SOCKET_2                 0x200004010000ULL
#define TH500_MCF_SMMU_SOCKET_3                 0x300004010000ULL
#define TH500_MCF_SMMU_CLKEN_OVERRIDE_0_OFFSET  0x0
#define TH500_MCF_SMMU_BYPASS_0_OFFSET          0x4

// Maximum time ACPI loops before abort */
#define TH500_ACPI_MAX_LOOP_TIMEOUT  30

// Maximum time ACPI loops before abort during GPU _RST */
#define TH500_ACPI_GPU_RST_MAX_LOOP_TIMEOUT  10

#endif //__TH500_DEFINES_H__
