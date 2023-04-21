/** @file
*  Nvidia Configuration Dxe
*
*  Copyright (c) 2020-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*  Copyright (c) 2017, Linaro Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __NVIDIA_CONFIG_HII_H__
#define __NVIDIA_CONFIG_HII_H__

#include <Guid/NVIDIAPublicVariableGuid.h>
#include <Guid/NVIDIATokenSpaceGuid.h>
#include <NVIDIAConfiguration.h>

#define NVIDIA_CONFIG_FORMSET_GUID  { 0x685c0b6e, 0x11af, 0x47cf, { 0xa9, 0xef, 0x95, 0xac, 0x18, 0x68, 0x73, 0xc3 } }

#define NVIDIA_CONFIG_FORM_ID                      0x0001
#define PCIE_CONFIGURATION_FORM_ID                 0x0002
#define BOOT_CONFIGURATION_FORM_ID                 0x0003
#define L4T_CONFIGURATION_FORM_ID                  0x0004
#define TH500_CONFIGURATION_FORM_ID                0x0005
#define TH500_SOCKET0_CONFIGURATION_FORM_ID        0x0006
#define TH500_SOCKET1_CONFIGURATION_FORM_ID        0x0007
#define TH500_SOCKET2_CONFIGURATION_FORM_ID        0x0008
#define TH500_SOCKET3_CONFIGURATION_FORM_ID        0x0009
#define TH500_SOCKET0_PCIE0_CONFIGURATION_FORM_ID  0x000A
#define TH500_SOCKET0_PCIE1_CONFIGURATION_FORM_ID  0x000B
#define TH500_SOCKET0_PCIE2_CONFIGURATION_FORM_ID  0x000C
#define TH500_SOCKET0_PCIE3_CONFIGURATION_FORM_ID  0x000D
#define TH500_SOCKET0_PCIE4_CONFIGURATION_FORM_ID  0x000E
#define TH500_SOCKET0_PCIE5_CONFIGURATION_FORM_ID  0x000F
#define TH500_SOCKET0_PCIE6_CONFIGURATION_FORM_ID  0x0010
#define TH500_SOCKET0_PCIE7_CONFIGURATION_FORM_ID  0x0011
#define TH500_SOCKET0_PCIE8_CONFIGURATION_FORM_ID  0x0012
#define TH500_SOCKET0_PCIE9_CONFIGURATION_FORM_ID  0x0013
#define TH500_SOCKET1_PCIE0_CONFIGURATION_FORM_ID  0x0014
#define TH500_SOCKET1_PCIE1_CONFIGURATION_FORM_ID  0x0015
#define TH500_SOCKET1_PCIE2_CONFIGURATION_FORM_ID  0x0016
#define TH500_SOCKET1_PCIE3_CONFIGURATION_FORM_ID  0x0017
#define TH500_SOCKET1_PCIE4_CONFIGURATION_FORM_ID  0x0018
#define TH500_SOCKET1_PCIE5_CONFIGURATION_FORM_ID  0x0019
#define TH500_SOCKET1_PCIE6_CONFIGURATION_FORM_ID  0x001A
#define TH500_SOCKET1_PCIE7_CONFIGURATION_FORM_ID  0x001B
#define TH500_SOCKET1_PCIE8_CONFIGURATION_FORM_ID  0x001C
#define TH500_SOCKET1_PCIE9_CONFIGURATION_FORM_ID  0x001D
#define TH500_SOCKET2_PCIE0_CONFIGURATION_FORM_ID  0x001E
#define TH500_SOCKET2_PCIE1_CONFIGURATION_FORM_ID  0x001F
#define TH500_SOCKET2_PCIE2_CONFIGURATION_FORM_ID  0x0020
#define TH500_SOCKET2_PCIE3_CONFIGURATION_FORM_ID  0x0021
#define TH500_SOCKET2_PCIE4_CONFIGURATION_FORM_ID  0x0022
#define TH500_SOCKET2_PCIE5_CONFIGURATION_FORM_ID  0x0023
#define TH500_SOCKET2_PCIE6_CONFIGURATION_FORM_ID  0x0024
#define TH500_SOCKET2_PCIE7_CONFIGURATION_FORM_ID  0x0025
#define TH500_SOCKET2_PCIE8_CONFIGURATION_FORM_ID  0x0026
#define TH500_SOCKET2_PCIE9_CONFIGURATION_FORM_ID  0x0027
#define TH500_SOCKET3_PCIE0_CONFIGURATION_FORM_ID  0x0028
#define TH500_SOCKET3_PCIE1_CONFIGURATION_FORM_ID  0x0029
#define TH500_SOCKET3_PCIE2_CONFIGURATION_FORM_ID  0x002A
#define TH500_SOCKET3_PCIE3_CONFIGURATION_FORM_ID  0x002B
#define TH500_SOCKET3_PCIE4_CONFIGURATION_FORM_ID  0x002C
#define TH500_SOCKET3_PCIE5_CONFIGURATION_FORM_ID  0x002D
#define TH500_SOCKET3_PCIE6_CONFIGURATION_FORM_ID  0x002E
#define TH500_SOCKET3_PCIE7_CONFIGURATION_FORM_ID  0x002F
#define TH500_SOCKET3_PCIE8_CONFIGURATION_FORM_ID  0x0030
#define TH500_SOCKET3_PCIE9_CONFIGURATION_FORM_ID  0x0031
#define DEBUG_CONFIGURATION_FORM_ID                0x0032
#define NVIDIA_PRODUCT_INFO_FORM_ID                0x0033

#define KEY_ENABLE_PCIE_CONFIG         0x0100
#define KEY_ENABLE_PCIE_IN_OS_CONFIG   0x0101
#define KEY_ENABLE_QUICK_BOOT          0x0102
#define KEY_NEW_DEVICE_HIERARCHY       0x0103
#define KEY_SERIAL_PORT_CONFIG         0x0104
#define KEY_KERNEL_CMDLINE             0x0105
#define KEY_L4T_BOOTMODE_DEFAULT       0x0106
#define KEY_RESET_VARIABLES            0x0107
#define KEY_OS_CHAIN_STATUS_A          0x0108
#define KEY_OS_CHAIN_STATUS_B          0x0109
#define KEY_L4T_CONFIG                 0x010A
#define KEY_BOOT_CONFIG                0x010B
#define KEY_IPMI_NETWORK_BOOT_MODE     0x010C
#define KEY_ENABLE_ACPI_TIMER          0x010D
#define KEY_DGPU_DT_EFIFB_SUPPORT      0x010E
#define KEY_ENABLE_HOST_INTERFACE      0x010F
#define KEY_PRODUCT_INFO               0x0110
#define KEY_PRODUCT_CHASSIS_ASSET_TAG  0x0111
#define KEY_ENABLE_UEFI_SHELL          0x0112

#define NVIDIA_CONFIG_HII_CONTROL_ID  0x1000

#define PCIE_IN_OS_DISABLE  0x0
#define PCIE_IN_OS_ENABLE   0x1

#define QUICK_BOOT_DISABLE  0x0
#define QUICK_BOOT_ENABLE   0x1

#define IPMI_NETWORK_BOOT_MODE_IPV4  0x0
#define IPMI_NETWORK_BOOT_MODE_IPV6  0x1

#define ACPI_TIMER_DISABLE  0x0
#define ACPI_TIMER_ENABLE   0x1

#define HOST_INTERFACE_DISABLE  0x0
#define HOST_INTERFACE_ENABLE   0x1

#define NEW_DEVICE_HIERARCHY_BOTTOM  0x0
#define NEW_DEVICE_HIERARCHY_TOP     0x1

#define DGPU_DT_EFIFB_DISABLE  0x0
#define DGPU_DT_EFIFB_ENABLE   0x1

#define MAX_SOCKETS  4
#define MAX_PCIE     10
#define MAX_UPHY     6

#define EXPOSE_PCIE_FLOORSWEEPING_VARIABLE          0x0001
#define EXPOSE_NVML_FLOORSWEEPING_VARIABLE          0x0002
#define EXPOSE_C2C_FLOORSWEEPING_VARIABLE           0x0004
#define EXPOSE_HALF_CHIP_DISABLED_VARIABLE          0x0008
#define EXPOSE_MCF_CHANNEL_DISABLED_VARIABLE        0x0010
#define EXPOSE_UPHY_LANE_OWNERSHIP_VARAIBLE         0x0020
#define EXPOSE_CCPLEX_CORE_DISABLED_VARIABLE        0x0040
#define EXPOSE_CCPLEX_MCF_BRIDGE_DISABLED_VARIABLE  0x0080
#define EXPOSE_CCPLEX_SOC_BRIDGE_DISABLED_VARIABLE  0x0100
#define EXPOSE_CCPLEX_CSN_DISABLED_VARIABLE         0x0200
#define EXPOSE_SCF_CACHE_DISABLED_VARIABLE          0x0400

typedef struct {
  UINT32     L4TSupported;
  BOOLEAN    QuickBootSupported;
  BOOLEAN    DebugMenuSupported;
  BOOLEAN    RedfishSupported;
  UINT32     RootfsRedundancyLevel;
  BOOLEAN    TH500Config;
  BOOLEAN    SocketEnabled[MAX_SOCKETS];
  UINT8      PhysicalPcieWidth0[MAX_PCIE];
  UINT8      PhysicalPcieWidth1[MAX_PCIE];
  UINT8      PhysicalPcieWidth2[MAX_PCIE];
  UINT8      PhysicalPcieWidth3[MAX_PCIE];
  // MB1 DATA
  BOOLEAN    EgmEnabled;
  UINT32     EgmHvSizeMb;
  UINT32     UefiDebugLevel;
  BOOLEAN    SpreadSpectrumEnable;
  BOOLEAN    AtsPageGranule4k;
  UINT8      PerfVersion;
  UINT8      UphySetting0[MAX_UPHY];
  UINT8      UphySetting1[MAX_UPHY];
  UINT8      UphySetting2[MAX_UPHY];
  UINT8      UphySetting3[MAX_UPHY];
  UINT32     MaxSpeed0[MAX_PCIE];
  UINT32     MaxSpeed1[MAX_PCIE];
  UINT32     MaxSpeed2[MAX_PCIE];
  UINT32     MaxSpeed3[MAX_PCIE];
  UINT32     MaxWidth0[MAX_PCIE];
  UINT32     MaxWidth1[MAX_PCIE];
  UINT32     MaxWidth2[MAX_PCIE];
  UINT32     MaxWidth3[MAX_PCIE];
  UINT8      SlotType0[MAX_PCIE];
  UINT8      SlotType1[MAX_PCIE];
  UINT8      SlotType2[MAX_PCIE];
  UINT8      SlotType3[MAX_PCIE];
  BOOLEAN    EnableAspmL1_0[MAX_PCIE];
  BOOLEAN    EnableAspmL1_1[MAX_PCIE];
  BOOLEAN    EnableAspmL1_2[MAX_PCIE];
  BOOLEAN    EnableAspmL1_3[MAX_PCIE];
  BOOLEAN    EnableAspmL1_1_0[MAX_PCIE];
  BOOLEAN    EnableAspmL1_1_1[MAX_PCIE];
  BOOLEAN    EnableAspmL1_1_2[MAX_PCIE];
  BOOLEAN    EnableAspmL1_1_3[MAX_PCIE];
  BOOLEAN    EnableAspmL1_2_0[MAX_PCIE];
  BOOLEAN    EnableAspmL1_2_1[MAX_PCIE];
  BOOLEAN    EnableAspmL1_2_2[MAX_PCIE];
  BOOLEAN    EnableAspmL1_2_3[MAX_PCIE];
  BOOLEAN    EnablePciPmL1_2_0[MAX_PCIE];
  BOOLEAN    EnablePciPmL1_2_1[MAX_PCIE];
  BOOLEAN    EnablePciPmL1_2_2[MAX_PCIE];
  BOOLEAN    EnablePciPmL1_2_3[MAX_PCIE];
  BOOLEAN    SupportsClkReq0[MAX_PCIE];
  BOOLEAN    SupportsClkReq1[MAX_PCIE];
  BOOLEAN    SupportsClkReq2[MAX_PCIE];
  BOOLEAN    SupportsClkReq3[MAX_PCIE];
  BOOLEAN    DisableDLFE0[MAX_PCIE];
  BOOLEAN    DisableDLFE1[MAX_PCIE];
  BOOLEAN    DisableDLFE2[MAX_PCIE];
  BOOLEAN    DisableDLFE3[MAX_PCIE];
  BOOLEAN    EnableECRC_0[MAX_PCIE];
  BOOLEAN    EnableECRC_1[MAX_PCIE];
  BOOLEAN    EnableECRC_2[MAX_PCIE];
  BOOLEAN    EnableECRC_3[MAX_PCIE];
  BOOLEAN    DisableOptionRom0[MAX_PCIE];
  BOOLEAN    DisableOptionRom1[MAX_PCIE];
  BOOLEAN    DisableOptionRom2[MAX_PCIE];
  BOOLEAN    DisableOptionRom3[MAX_PCIE];
  BOOLEAN    DisableDPCAtRP_0[MAX_PCIE];
  BOOLEAN    DisableDPCAtRP_1[MAX_PCIE];
  BOOLEAN    DisableDPCAtRP_2[MAX_PCIE];
  BOOLEAN    DisableDPCAtRP_3[MAX_PCIE];
} NVIDIA_CONFIG_HII_CONTROL;

#define ADD_GOTO_SOCKET_FORM(socket)                                       \
  suppressif ideqval NVIDIA_CONFIG_HII_CONTROL.SocketEnabled[socket] == 0; \
  goto TH500_SOCKET##socket##_CONFIGURATION_FORM_ID,                       \
      prompt = STRING_TOKEN(STR_SOCKET##socket##_CONFIG_FORM_TITLE),       \
      help = STRING_TOKEN(STR_SOCKET##socket##_CONFIG_FORM_HELP);          \
  endif;

#define ADD_GOTO_PCIE_FORM(socket, pcie) \
  goto TH500_SOCKET##socket##_PCIE##pcie##_CONFIGURATION_FORM_ID, \
    prompt = STRING_TOKEN (STR_PCIE##pcie##_CONFIG_FORM_TITLE), \
    help   = STRING_TOKEN (STR_NULL);

#define ADD_SOCKET_FORM(socket) \
  form formid = TH500_SOCKET##socket##_CONFIGURATION_FORM_ID, \
    title = STRING_TOKEN(STR_SOCKET##socket##_CONFIG_FORM_TITLE); \
    subtitle text = STRING_TOKEN(STR_NULL); \
    suppressif ideqval NVIDIA_CONFIG_HII_CONTROL.SocketEnabled[socket] == 0; \
      oneof varid = NVIDIA_CONFIG_HII_CONTROL.UphySetting##socket[0],\
        prompt = STRING_TOKEN(STR_UPHY0_SOCKET##socket##_PROMPT),\
        help   = STRING_TOKEN(STR_UPHY0_HELP),\
        flags  = RESET_REQUIRED,\
        option text = STRING_TOKEN(STR_DISABLED), value = 0, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_C0_X16),  value = 1, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_C0_X8_C1_X8),  value = 2, flags = 0;\
      endoneof;\
      ADD_GOTO_PCIE_FORM(socket,0) \
      ADD_GOTO_PCIE_FORM(socket,1) \
      oneof varid = NVIDIA_CONFIG_HII_CONTROL.UphySetting##socket[1],\
        prompt = STRING_TOKEN(STR_UPHY1_SOCKET##socket##_PROMPT),\
        help   = STRING_TOKEN(STR_UPHY1_HELP),\
        flags  = RESET_REQUIRED,\
        option text = STRING_TOKEN(STR_DISABLED), value = 0, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_C2_X16),  value = 1, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_C2_X8_C3_X8),  value = 2, flags = 0;\
      endoneof;\
      ADD_GOTO_PCIE_FORM(socket,2) \
      ADD_GOTO_PCIE_FORM(socket,3) \
      oneof varid = NVIDIA_CONFIG_HII_CONTROL.UphySetting##socket[2],\
        prompt = STRING_TOKEN(STR_UPHY2_SOCKET##socket##_PROMPT),\
        help   = STRING_TOKEN(STR_UPHY2_HELP),\
        flags  = RESET_REQUIRED,\
        option text = STRING_TOKEN(STR_DISABLED), value = 0, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_C4_X16),  value = 1, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_C4_X8_C5_X8),  value = 2, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_C5_X4_NVLINK_X12),  value = 3, flags = 0;\
      endoneof;\
      ADD_GOTO_PCIE_FORM(socket,4) \
      ADD_GOTO_PCIE_FORM(socket,5) \
      oneof varid = NVIDIA_CONFIG_HII_CONTROL.UphySetting##socket[3],\
        prompt = STRING_TOKEN(STR_UPHY3_SOCKET##socket##_PROMPT),\
        help   = STRING_TOKEN(STR_UPHY3_HELP),\
        flags  = RESET_REQUIRED,\
        option text = STRING_TOKEN(STR_DISABLED), value = 0, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_C6_X16),  value = 1, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_C6_X8_C7_X8),  value = 2, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_C7_X4_NVLINK_X12),  value = 3, flags = 0;\
      endoneof;\
      ADD_GOTO_PCIE_FORM(socket,6) \
      ADD_GOTO_PCIE_FORM(socket,7) \
      oneof varid = NVIDIA_CONFIG_HII_CONTROL.UphySetting##socket[4],\
        prompt = STRING_TOKEN(STR_UPHY4_SOCKET##socket##_PROMPT),\
        help   = STRING_TOKEN(STR_UPHY4_HELP),\
        flags  = RESET_REQUIRED,\
        option text = STRING_TOKEN(STR_DISABLED), value = 0, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_C8_X2),  value = 1, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_C8_X1_USB),  value = 2, flags = 0;\
      endoneof;\
      ADD_GOTO_PCIE_FORM(socket,8) \
      oneof varid = NVIDIA_CONFIG_HII_CONTROL.UphySetting##socket[5],\
        prompt = STRING_TOKEN(STR_UPHY5_SOCKET##socket##_PROMPT),\
        help   = STRING_TOKEN(STR_UPHY5_HELP),\
        flags  = RESET_REQUIRED,\
        option text = STRING_TOKEN(STR_DISABLED), value = 0, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_C9_X2),  value = 1, flags = 0;\
      endoneof;\
      ADD_GOTO_PCIE_FORM(socket,9) \
    endif; \
  endform;

#define ADD_PCIE_FORM(socket, pcie) \
  form formid = TH500_SOCKET##socket##_PCIE##pcie##_CONFIGURATION_FORM_ID, \
    title = STRING_TOKEN(STR_PCIE##pcie##_CONFIG_FORM_TITLE); \
    subtitle text = STRING_TOKEN(STR_NULL); \
    suppressif ideqval NVIDIA_CONFIG_HII_CONTROL.SocketEnabled[socket] == 0; \
      oneof varid = NVIDIA_CONFIG_HII_CONTROL.MaxSpeed##socket[pcie],\
        prompt = STRING_TOKEN(STR_PCIE_MAX_SPEED_SOCKET##socket##_PCIE##pcie##_TITLE),\
        help   = STRING_TOKEN(STR_PCIE_MAX_SPEED_HELP),\
        flags  = RESET_REQUIRED,\
        option text = STRING_TOKEN(STR_PCIE_GEN5), value = 5, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_GEN4), value = 4, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_GEN3), value = 3, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_GEN2), value = 2, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_GEN1), value = 1, flags = 0;\
      endoneof;\
      oneof varid = NVIDIA_CONFIG_HII_CONTROL.MaxWidth##socket[pcie],\
        prompt = STRING_TOKEN(STR_PCIE_MAX_WIDTH_SOCKET##socket##_PCIE##pcie##_TITLE),\
        help   = STRING_TOKEN(STR_PCIE_MAX_WIDTH_HELP),\
        flags  = RESET_REQUIRED,\
        option text = STRING_TOKEN(STR_PCIE_X16), value = 16, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_X8), value = 8, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_X4), value = 4, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_X2), value = 2, flags = 0;\
        option text = STRING_TOKEN(STR_PCIE_X1), value = 1, flags = 0;\
      endoneof;\
      checkbox varid = NVIDIA_CONFIG_HII_CONTROL.EnableAspmL1_##socket[pcie], \
        prompt = STRING_TOKEN(STR_PCIE_ENABLE_ASPM_L1_SOCKET##socket##_PCIE##pcie##_TITLE), \
        help   = STRING_TOKEN(STR_NULL), \
        flags  = RESET_REQUIRED, \
        default = FALSE, \
      endcheckbox; \
      grayoutif ideqval NVIDIA_CONFIG_HII_CONTROL.EnableAspmL1_##socket[pcie] == 0; \
        checkbox varid = NVIDIA_CONFIG_HII_CONTROL.EnableAspmL1_1_##socket[pcie], \
          prompt = STRING_TOKEN(STR_PCIE_ENABLE_ASPM_L1_1_SOCKET##socket##_PCIE##pcie##_TITLE), \
          help   = STRING_TOKEN(STR_NULL), \
          flags  = RESET_REQUIRED, \
          default = FALSE, \
        endcheckbox; \
        checkbox varid = NVIDIA_CONFIG_HII_CONTROL.EnableAspmL1_2_##socket[pcie], \
          prompt = STRING_TOKEN(STR_PCIE_ENABLE_ASPM_L1_2_SOCKET##socket##_PCIE##pcie##_TITLE), \
          help   = STRING_TOKEN(STR_NULL), \
          flags  = RESET_REQUIRED, \
          default = FALSE, \
        endcheckbox; \
      endif; \
      checkbox varid = NVIDIA_CONFIG_HII_CONTROL.EnablePciPmL1_2_##socket[pcie], \
        prompt = STRING_TOKEN(STR_PCIE_ENABLE_PCIPM_L1_2_SOCKET##socket##_PCIE##pcie##_TITLE), \
        help   = STRING_TOKEN(STR_NULL), \
        flags  = RESET_REQUIRED, \
        default = FALSE, \
      endcheckbox; \
      checkbox varid = NVIDIA_CONFIG_HII_CONTROL.SupportsClkReq##socket[pcie], \
        prompt = STRING_TOKEN(STR_PCIE_SUPPORTS_CLK_REQ_SOCKET##socket##_PCIE##pcie##_TITLE), \
        help   = STRING_TOKEN(STR_PCIE_SUPPORTS_CLK_REQ_HELP), \
        flags  = RESET_REQUIRED, \
        default = FALSE, \
      endcheckbox; \
      checkbox varid = NVIDIA_CONFIG_HII_CONTROL.DisableDLFE##socket[pcie], \
        prompt = STRING_TOKEN(STR_PCIE_DISABLE_DLFE_SOCKET##socket##_PCIE##pcie##_TITLE), \
        help   = STRING_TOKEN(STR_PCIE_DISABLE_DLFE_HELP), \
        flags  = RESET_REQUIRED, \
        default = FALSE, \
      endcheckbox; \
      checkbox varid = NVIDIA_CONFIG_HII_CONTROL.EnableECRC_##socket[pcie], \
        prompt = STRING_TOKEN(STR_PCIE_ENABLE_ECRC_SOCKET##socket##_PCIE##pcie##_TITLE), \
        help   = STRING_TOKEN(STR_PCIE_ENABLE_ECRC_HELP), \
        flags  = RESET_REQUIRED, \
        default = FALSE, \
      endcheckbox; \
    checkbox varid = NVIDIA_CONFIG_HII_CONTROL.DisableOptionRom##socket[pcie], \
        prompt = STRING_TOKEN(STR_PCIE_DISABLE_OPT_ROM_SOCKET##socket##_PCIE##pcie##_TITLE), \
        help   = STRING_TOKEN(STR_PCIE_DISABLE_OPT_ROM_HELP), \
        flags  = RESET_REQUIRED, \
        default = FALSE, \
    endcheckbox; \
        checkbox varid = NVIDIA_CONFIG_HII_CONTROL.DisableDPCAtRP_##socket[pcie], \
          prompt = STRING_TOKEN(STR_PCIE_DISABLE_DPC_AT_RP_SOCKET##socket##_PCIE##pcie##_TITLE), \
            help   = STRING_TOKEN(STR_PCIE_DISABLE_DPC_AT_RP_HELP), \
          flags  = RESET_REQUIRED, \
          default = FALSE, \
          endcheckbox; \
    endif; \
  endform;

#define PCIE_SEG(socket, pcie)  (((socket) << 4) | ((pcie) & 0xF))

#endif // __NVIDIA_CONFIG_HII_H__
