/** @file

  NV Display Controller Driver

  SPDX-FileCopyrightText: Copyright (c) 2021-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/DisplayDeviceTreeHelperLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/PrintLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/ClockNodeProtocol.h>
#include <Protocol/EmbeddedGpio.h>

#include <libfdt.h>

#define DISPLAY_SOR_COUNT      8
#define DISPLAY_FE_SW_SYS_CAP  0x00030000
#define DISPLAY_FE_SW_SYS_CAP_SOR_EXISTS_GET(x, i)     (BOOLEAN)(BitFieldRead32 ((x), 8 + (i), 8 + (i)) != 0)
#define DISPLAY_FE_CMGR_CLK_SOR(i)                     (0x00002300 + (i) * SIZE_2KB)
#define DISPLAY_FE_CMGR_CLK_SOR_MODE_BYPASS_SET(x, v)  BitFieldWrite32 ((x), 16, 17, (v))
#define DISPLAY_FE_CMGR_CLK_SOR_MODE_BYPASS_DP_SAFE  2

#define DISPLAY_CONTROLLER_SIGNATURE  SIGNATURE_32('N','V','D','C')

typedef struct {
  UINT32                     Signature;
  EFI_HANDLE                 DriverHandle;
  EFI_HANDLE                 ControllerHandle;
  NON_DISCOVERABLE_DEVICE    EdkiiNonDiscoverableDevice;
  BOOLEAN                    ResetsDeasserted;
  BOOLEAN                    ClocksEnabled;
  BOOLEAN                    OutputGpiosConfigured;
  EFI_EVENT                  OnFdtInstalledEvent;
  EFI_EVENT                  OnReadyToBootEvent;
} NVIDIA_DISPLAY_CONTROLLER_CONTEXT;

#define DISPLAY_CONTROLLER_CONTEXT_FROM_EDKII_DEVICE(a)  CR(\
    a,                                                      \
    NVIDIA_DISPLAY_CONTROLLER_CONTEXT,                      \
    EdkiiNonDiscoverableDevice,                             \
    DISPLAY_CONTROLLER_SIGNATURE                            \
    )

/* Discover driver */

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,tegra234-display", &gNVIDIANonDiscoverableT234DisplayDeviceGuid },
  { NULL,                      NULL                                         }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                      = L"NV Display Controller Driver",
  .AutoDeassertPg                  = TRUE,
  .SkipEdkiiNondiscoverableInstall = TRUE,
  .DirectEnumerationSupport        = TRUE,
};

/**
   Perform NvDisplay engine resets

   @param[in] ControllerHandle         Handle to the controller
   @param[in] Assert                   Assert/Deassert the reset signal

   @retval EFI_SUCCESS                 display engines successfully reset
   @retval others                      display engine reset failure
 */
STATIC
EFI_STATUS
ResetRequiredDisplayEngines (
  IN       EFI_HANDLE  ControllerHandle,
  IN CONST BOOLEAN     Assert
  )
{
  STATIC CONST CHAR8 *CONST  DisplayResets[] = {
    "nvdisplay_reset",
    "dpaux0_reset",
  };

  EFI_STATUS   Status = EFI_SUCCESS;
  UINTN        Index;
  CONST CHAR8  *ResetName;

  /* Reset all required display engines */
  for (Index = 0; Index < ARRAY_SIZE (DisplayResets); Index++) {
    ResetName = DisplayResets[Index];

    Status = DeviceDiscoveryConfigReset (ControllerHandle, ResetName, Assert);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to %a reset %a: %r \r\n",
        __FUNCTION__,
        Assert ? "assert" : "deassert",
        ResetName,
        Status
        ));
      break;
    }
  }

  return Status;
}

/**
   Modeled after dispTegraSocEnableRequiredClks_v04_02 and
   dispTegraSocInitMaxFreqForDispHubClks_v04_02 in
   <gpu/drv/drivers/resman/src/physical/gpu/disp/arch/v04/disp_0402.c>.

   @param[in] ControllerHandle          Handle to the controller
   @param[in] Enable                    Enable/disable the clocks

   @return EFI_SUCCESS      Clocks successfully enabled/disabled
   @return !=EFI_SUCCESS    An error occurred
 */
STATIC
EFI_STATUS
EnableRequiredDisplayClocks (
  IN       EFI_HANDLE  ControllerHandle,
  IN CONST BOOLEAN     Enable
  )
{
  STATIC CONST CHAR8 *CONST  Clocks[] = {
    "nvdisplay_disp_clk",
    "dpaux0_clk",
    "nvdisplayhub_clk",
    "dsi_core_clk",
    "maud_clk",
    "aza_2xbit_clk",
    "aza_bit_clk",
  };
  STATIC CONST CHAR8 *CONST  ClockParents[][2] = {
    { "nvdisplay_disp_clk", "disppll_clk"        },
    { "nvdisplayhub_clk",   "sppll0_clkoutb_clk" },
  };

  EFI_STATUS                  Status;
  UINTN                       Index;
  CONST CHAR8                 *ClockName;
  CONST CHAR8                 *ParentClockName;
  NVIDIA_CLOCK_NODE_PROTOCOL  *ClockNodeProtocol;

  if (Enable) {
    /* Set required clock parents */
    for (Index = 0; Index < ARRAY_SIZE (ClockParents); ++Index) {
      ClockName       = ClockParents[Index][0];
      ParentClockName = ClockParents[Index][1];

      Status = DeviceDiscoverySetClockParent (ControllerHandle, ClockName, ParentClockName);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to set parent of clock '%a' to '%a': %r\r\n",
          __FUNCTION__,
          ClockName,
          ParentClockName,
          Status
          ));
        return Status;
      }
    }

    /* Enable all required clocks */
    for (Index = 0; Index < ARRAY_SIZE (Clocks); ++Index) {
      ClockName = Clocks[Index];

      Status = DeviceDiscoveryEnableClock (ControllerHandle, ClockName, TRUE);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to enable clock '%a': %r\r\n",
          __FUNCTION__,
          ClockName,
          Status
          ));
        return Status;
      }
    }
  } else {
    Status = gBS->HandleProtocol (
                    ControllerHandle,
                    &gNVIDIAClockNodeProtocolGuid,
                    (VOID **)&ClockNodeProtocol
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to lookup clock node protocol: %r\r\n",
        __FUNCTION__,
        Status
        ));
      return Status;
    }

    Status = ClockNodeProtocol->DisableAll (ClockNodeProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to disable clocks: %r\r\n",
        __FUNCTION__,
        Status
        ));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
   Retrieves GPIO pin number from a subnode of the specified node.

   @param [in]  DeviceTreeBase  Base of the Device Tree to read.
   @param [in]  NodeOffset      Offset of the specified node.
   @param [in]  SubnodeName     Name of the subnode to look for.
   @param [out] Pin             Where to store the pin number.

   @retval TRUE     Pin number successfully retrieved.
   @retval FALSE    An error occurred.
*/
STATIC
BOOLEAN
GetSubnodeGpioPin (
  IN  VOID          *CONST  DeviceTreeBase,
  IN  CONST INT32           NodeOffset,
  IN  CONST CHAR8   *CONST  SubnodeName,
  OUT UINT32        *CONST  Pin
  )
{
  INT32                 SubnodeOffset;
  CONST CHAR8   *CONST  GpiosPropName = "gpios";
  CONST VOID            *GpiosProp;
  INT32                 PropSize;

  SubnodeOffset = fdt_subnode_offset (
                    DeviceTreeBase,
                    NodeOffset,
                    SubnodeName
                    );
  if (SubnodeOffset < 0) {
    if (SubnodeOffset != -FDT_ERR_NOTFOUND) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: could not locate subnode '%a': %a\r\n",
        __FUNCTION__,
        SubnodeName,
        fdt_strerror (SubnodeOffset)
        ));
    }

    return FALSE;
  }

  GpiosProp = fdt_getprop (
                DeviceTreeBase,
                SubnodeOffset,
                GpiosPropName,
                &PropSize
                );
  if (GpiosProp == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: could not locate property '%a': %a\r\n",
      __FUNCTION__,
      GpiosPropName,
      fdt_strerror (PropSize)
      ));
    return FALSE;
  }

  if (PropSize < sizeof (*Pin)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: invalid size of property '%a': %d\r\n",
      __FUNCTION__,
      GpiosPropName,
      (INTN)PropSize
      ));
    return FALSE;
  }

  *Pin = SwapBytes32 (*(CONST UINT32 *)GpiosProp);
  return TRUE;
}

/**
 configure any GPIOs needed for HDMI/DP output
 */
STATIC
EFI_STATUS
ConfigureOutputGpios (
  IN       EFI_HANDLE  ControllerHandle,
  IN CONST BOOLEAN     Enable,
  IN CONST BOOLEAN     UseDpOutput
  )
{
  EFI_STATUS                                Status;
  NVIDIA_DEVICE_TREE_NODE_PROTOCOL          *DeviceTreeNode;
  VOID                                      *DeviceTreeBase;
  EMBEDDED_GPIO                             *EmbeddedGpio;
  CONST CHAR8                       *CONST  GpioCompatible = "ti,tca9539";
  INT32                                     GpioOffset;
  UINT32                                    GpioPhandle;
  UINT32                                    EnVddHdmiPin;
  UINT32                                    Dp0AuxUart6SelPin;
  UINT32                                    HdmiDp0MuxSelPin;
  UINT32                                    Dp0AuxI2c8SelPin;
  EMBEDDED_GPIO_PIN                         GpioPin;
  EMBEDDED_GPIO_MODE                        GpioMode;

  Status = gBS->HandleProtocol (
                  ControllerHandle,
                  &gNVIDIADeviceTreeNodeProtocolGuid,
                  (VOID **)&DeviceTreeNode
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: could not retrieve DT node protocol: %r\r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  DeviceTreeBase = DeviceTreeNode->DeviceTreeBase;

  Status = gBS->LocateProtocol (
                  &gNVIDIAI2cExpanderGpioProtocolGuid,
                  NULL,
                  (VOID **)&EmbeddedGpio
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: could not locate I2C expander GPIO protocol: %r\r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  GpioOffset = -1;
  while (1) {
    GpioOffset = fdt_node_offset_by_compatible (DeviceTreeBase, GpioOffset, GpioCompatible);
    if (GpioOffset == -FDT_ERR_NOTFOUND) {
      DEBUG ((
        DEBUG_INFO,
        "%a: could not find compatible GPIO node in DT: not on SLT board?\r\n",
        __FUNCTION__
        ));
      /* Return success to avoid breaking boot on non-SLT boards. */
      return EFI_SUCCESS;
    } else if (GpioOffset < 0) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to lookup node by compatible '%a': %a\r\n",
        __FUNCTION__,
        GpioCompatible,
        fdt_strerror (GpioOffset)
        ));
      return EFI_NOT_FOUND;
    }

    if (  GetSubnodeGpioPin (DeviceTreeBase, GpioOffset, "en_vdd_hdmi_cvm", &EnVddHdmiPin)
       && GetSubnodeGpioPin (DeviceTreeBase, GpioOffset, "dp0_aux_uart6_sel", &Dp0AuxUart6SelPin)
       && GetSubnodeGpioPin (DeviceTreeBase, GpioOffset, "hdmi_dp0_mux_sel", &HdmiDp0MuxSelPin)
       && GetSubnodeGpioPin (DeviceTreeBase, GpioOffset, "dp0_aux_i2c8_sel", &Dp0AuxI2c8SelPin))
    {
      break;
    }
  }

  GpioPhandle = fdt_get_phandle (DeviceTreeBase, GpioOffset);
  if ((0 == GpioPhandle) || (-1 == GpioPhandle)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to find phandle of node at offset %d\r\n",
      __FUNCTION__,
      (INTN)GpioOffset
      ));
    return EFI_NOT_FOUND;
  }

  GpioPin  = GPIO (GpioPhandle, EnVddHdmiPin);
  GpioMode = Enable ? GPIO_MODE_OUTPUT_1 : GPIO_MODE_OUTPUT_0;
  Status   = EmbeddedGpio->Set (EmbeddedGpio, GpioPin, GpioMode);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: could not set pin 0x%x to mode 0x%x: %r\r\n",
      __FUNCTION__,
      GpioPin,
      GpioMode,
      Status
      ));
    return Status;
  }

  if (Enable) {
    GpioPin  = GPIO (GpioPhandle, Dp0AuxUart6SelPin);
    GpioMode = GPIO_MODE_OUTPUT_0;
    Status   = EmbeddedGpio->Set (EmbeddedGpio, GpioPin, GpioMode);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: could not set pin 0x%x to mode 0x%x: %r\r\n",
        __FUNCTION__,
        GpioPin,
        GpioMode,
        Status
        ));
      return Status;
    }

    GpioPin  = GPIO (GpioPhandle, HdmiDp0MuxSelPin);
    GpioMode = UseDpOutput ? GPIO_MODE_OUTPUT_1 : GPIO_MODE_OUTPUT_0;
    Status   = EmbeddedGpio->Set (EmbeddedGpio, GpioPin, GpioMode);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: could not set pin 0x%x to mode 0x%x: %r\r\n",
        __FUNCTION__,
        GpioPin,
        GpioMode,
        Status
        ));
      return Status;
    }

    GpioPin  = GPIO (GpioPhandle, Dp0AuxI2c8SelPin);
    GpioMode = GPIO_MODE_OUTPUT_0;
    Status   = EmbeddedGpio->Set (EmbeddedGpio, GpioPin, GpioMode);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: could not set pin 0x%x to mode 0x%x: %r\r\n",
        __FUNCTION__,
        GpioPin,
        GpioMode,
        Status
        ));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
   Creates an ACPI address space descriptor suitable for use as a
   framebuffer.

   @param[out] Desc  Resulting descriptor

   @retval EFI_SUCCESS Descriptor successfully created
   @retval others      Failed to create the descriptor
*/
STATIC
EFI_STATUS
CreateFramebufferResource (
  OUT EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *CONST  Desc
  )
{
  VOID                          *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO  *PlatformResourceInfo;
  EFI_PHYSICAL_ADDRESS          Address;
  UINTN                         Size;

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob == NULL) || (GET_GUID_HOB_DATA_SIZE (Hob) != sizeof (*PlatformResourceInfo))) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to retrieve platform resource information\r\n",
      __FUNCTION__
      ));
    return EFI_NOT_FOUND;
  }

  PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
  Address              = PlatformResourceInfo->FrameBufferInfo.Base;
  Size                 = PlatformResourceInfo->FrameBufferInfo.Size;

  if ((Address == 0) || (Size == 0)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: no framebuffer region present\r\n",
      __FUNCTION__
      ));
    return EFI_NOT_FOUND;
  }

  ZeroMem (Desc, sizeof (*Desc));
  Desc->Desc                 = ACPI_ADDRESS_SPACE_DESCRIPTOR;
  Desc->Len                  = sizeof (*Desc) - 3;
  Desc->AddrRangeMin         = Address;
  Desc->AddrLen              = Size;
  Desc->AddrRangeMax         = Address + Size - 1;
  Desc->ResType              = ACPI_ADDRESS_SPACE_TYPE_MEM;
  Desc->AddrSpaceGranularity = Address + Size > SIZE_4GB ? 64 : 32;

  return EFI_SUCCESS;
}

/**
   Copies resource descriptors from @a SourceResources to @a
   DestinationResources, optionally inserting @a *NewResource at index
   @a NewResourceIndex in the process.

   If @a DestinationResources is NULL, no copying is performed.

   If @a DestinationResourcesSize is not NULL, it will hold the
   minimum required size of @a DestinationResources (in bytes) upon
   return.

   Note that @a DestinationResources is assumed to have enough space
   available.

   @param[out]     DestinationResources      Where to put the result
   @param[out]     DestinationResourcesSize  Size of the result
   @param[in]      SourceResources           Resources to copy
   @param[in,out]  NewResource               Resource to insert on call; inserted resource on return
   @param[in]      NewResourceIndex          Index to insert the resource at

   @retval EFI_SUCCESS           Operation completed successfully
   @retval EFI_INVALID_PARAMETER SourceResources is NULL
   @retval EFI_INVALID_PARAMETER DestinationResources and DestinationResourcesSize are both NULL
   @retval EFI_INVALID_PARAMETER *NewResource is not NULL, but NewResourceIndex is out of bounds
*/
STATIC
EFI_STATUS
CopyAndInsertResource (
  OUT       EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *CONST     DestinationResources OPTIONAL,
  OUT       UINTN                              *CONST     DestinationResourcesSize OPTIONAL,
  IN     CONST EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *CONST  SourceResources,
  IN OUT CONST EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR **CONST  NewResource,
  IN     CONST UINTN                                      NewResourceIndex
  )
{
  UINTN                                    DestIndex, DestSize;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR        *DestDesc;
  CONST EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *SrcDesc, *NewDesc;

  if (  (  (DestinationResources == NULL)
        && (DestinationResourcesSize == NULL))
     || (SourceResources == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  NewDesc   = NewResource != NULL ? *NewResource : NULL;
  SrcDesc   = SourceResources;
  DestDesc  = DestinationResources;
  DestIndex = DestSize = 0;
  while (1) {
    if ((NewDesc != NULL) && (DestIndex == NewResourceIndex)) {
      if (DestDesc != NULL) {
        CopyMem (DestDesc, NewDesc, NewDesc->Len + 3);
        NewDesc  = DestDesc;
        DestDesc = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)((UINT8 *)DestDesc + DestDesc->Len + 3);
      }

      DestIndex++;
      DestSize += NewDesc->Len + 3;
    } else if (SrcDesc->Desc != ACPI_END_TAG_DESCRIPTOR) {
      if (DestDesc != NULL) {
        CopyMem (DestDesc, SrcDesc, SrcDesc->Len + 3);
        DestDesc = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)((UINT8 *)DestDesc + DestDesc->Len + 3);
      }

      SrcDesc = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)((UINT8 *)SrcDesc + SrcDesc->Len + 3);
      DestIndex++;
      DestSize += SrcDesc->Len + 3;
    } else {
      if (DestDesc != NULL) {
        CopyMem (DestDesc, SrcDesc, sizeof (EFI_ACPI_END_TAG_DESCRIPTOR));
      }

      DestSize += sizeof (EFI_ACPI_END_TAG_DESCRIPTOR);
      break;
    }
  }

  if (DestinationResourcesSize != NULL) {
    *DestinationResourcesSize = DestSize;
  }

  if (NewResource != NULL) {
    *NewResource = NewDesc;
  }

  // DestIndex is now equal to the total number of resources in
  // DestinationResources. If we wanted to insert NewDesc,
  // NewResourceIndex must be smaller than number of resources in
  // DestinationResources, otherwise NewResourceIndex is out of
  // bounds.
  if ((NewDesc != NULL) && !(NewResourceIndex < DestIndex)) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

/**
   Switch all SOR clocks to a safe source to prevent a lingering bad
   display HW state.

   @param[in] Context  Controller context to use.

   @retval EFI_SUCCESS    Operation successful.
   @retval !=EFI_SUCCESS  Error(s) occurred
*/
STATIC
EFI_STATUS
DisplayBypassSorClocks (
  IN NVIDIA_DISPLAY_CONTROLLER_CONTEXT *CONST  Context
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  DisplayBase;
  UINTN                 DisplaySize;
  UINTN                 SorIndex;
  UINT32                FeSwSysCap, FeCmgrClkSor;
  CONST UINTN           DisplayRegion = 0;

  Status = DeviceDiscoveryGetMmioRegion (
             Context->ControllerHandle,
             DisplayRegion,
             &DisplayBase,
             &DisplaySize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to retrieve display region: %r\r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  FeSwSysCap = MmioRead32 (DisplayBase + DISPLAY_FE_SW_SYS_CAP);
  for (SorIndex = 0; SorIndex < DISPLAY_SOR_COUNT; ++SorIndex) {
    if (DISPLAY_FE_SW_SYS_CAP_SOR_EXISTS_GET (FeSwSysCap, SorIndex)) {
      FeCmgrClkSor = MmioRead32 (DisplayBase + DISPLAY_FE_CMGR_CLK_SOR (SorIndex));
      FeCmgrClkSor = DISPLAY_FE_CMGR_CLK_SOR_MODE_BYPASS_SET (
                       FeCmgrClkSor,
                       DISPLAY_FE_CMGR_CLK_SOR_MODE_BYPASS_DP_SAFE
                       );
      MmioWrite32 (DisplayBase + DISPLAY_FE_CMGR_CLK_SOR (SorIndex), FeCmgrClkSor);
    }
  }

  return EFI_SUCCESS;
}

/**
  Performs teardown of the display hardware during ExitBootServices.

  @param[in] Context  Context of the display controller.

  @retval EFI_SUCCESS    Operation successful.
  @retval !=EFI_SUCCESS  Error(s) occurred
*/
STATIC
EFI_STATUS
DisplayStopOnExitBootServices (
  IN NVIDIA_DISPLAY_CONTROLLER_CONTEXT *CONST  Context
  )
{
  EFI_STATUS     Status = EFI_SUCCESS;
  EFI_STATUS     Status1;
  EFI_HANDLE     ControllerHandle;
  CONST BOOLEAN  UseDpOutput = FALSE;

  if (Context != NULL) {
    ControllerHandle = Context->ControllerHandle;

    if (Context->OnFdtInstalledEvent != NULL) {
      Status1 = gBS->CloseEvent (Context->OnFdtInstalledEvent);
      if (EFI_ERROR (Status1)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to close OnFdtInstalled event: %r\r\n",
          __FUNCTION__,
          Status1
          ));
      }

      if (!EFI_ERROR (Status)) {
        Status = Status1;
      }
    }

    if (Context->OnReadyToBootEvent != NULL) {
      Status1 = gBS->CloseEvent (Context->OnReadyToBootEvent);
      if (EFI_ERROR (Status1)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to close OnReadyToBoot event: %r\r\n",
          __FUNCTION__,
          Status1
          ));
      }

      if (!EFI_ERROR (Status)) {
        Status = Status1;
      }
    }

    if (Context->OutputGpiosConfigured) {
      Status1 = ConfigureOutputGpios (ControllerHandle, FALSE, UseDpOutput);
      if (!EFI_ERROR (Status)) {
        Status = Status1;
      }

      Context->OutputGpiosConfigured = FALSE;
    }

    if (Context->ClocksEnabled) {
      Status1 = EnableRequiredDisplayClocks (ControllerHandle, FALSE);
      if (!EFI_ERROR (Status)) {
        Status = Status1;
      }

      Context->ClocksEnabled = FALSE;
    }

    if (Context->ResetsDeasserted) {
      Status1 = ResetRequiredDisplayEngines (ControllerHandle, TRUE);
      if (!EFI_ERROR (Status)) {
        Status = Status1;
      }

      Context->ResetsDeasserted = FALSE;
    }
  }

  return Status;
}

/**
  Performs teardown of the display hardware.

  Cannot be called during ExitBootServices since it also frees the
  display context.

  @param[in] Context  Context of the display controller.

  @retval EFI_SUCCESS    Operation successful.
  @retval !=EFI_SUCCESS  Error(s) occurred
*/
STATIC
EFI_STATUS
DisplayStop (
  IN NVIDIA_DISPLAY_CONTROLLER_CONTEXT *CONST  Context
  )
{
  EFI_STATUS  Status;

  Status = DisplayStopOnExitBootServices (Context);

  if (Context != NULL) {
    FreePool (Context);
  }

  return Status;
}

/**
   Locates a child handle with the GOP protocol installed.

   @param[in]  Context  Context whose child handle shall be located.
   @param[out] Handle   The located child handle.

   @retval EFI_SUCCESS    Child handle found successfully.
   @retval !=EFI_SUCCESS  Error occurred.
*/
STATIC
EFI_STATUS
DisplayLocateChildGopHandle (
  IN  NVIDIA_DISPLAY_CONTROLLER_CONTEXT *CONST  Context,
  OUT EFI_HANDLE                        *CONST  Handle
  )
{
  EFI_STATUS                Status;
  EFI_HANDLE                ParentHandle, *Handles = NULL;
  UINTN                     HandleIndex, HandleCount;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiGraphicsOutputProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to enumerate graphics output device handles: %r\r\n",
      __FUNCTION__,
      Status
      ));
    goto Exit;
  }

  for (HandleIndex = 0; HandleIndex < HandleCount; ++HandleIndex) {
    Status = gBS->OpenProtocol (
                    Handles[HandleIndex],
                    &gEfiDevicePathProtocolGuid,
                    (VOID **)&DevicePath,
                    Context->DriverHandle,
                    Context->ControllerHandle,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      if (Status != EFI_UNSUPPORTED) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to retrieve device path from handle %p: %r\r\n",
          __FUNCTION__,
          Handles[HandleIndex],
          Status
          ));
      }

      continue;
    }

    Status = gBS->LocateDevicePath (
                    &gEdkiiNonDiscoverableDeviceProtocolGuid,
                    &DevicePath,
                    &ParentHandle
                    );
    if (EFI_ERROR (Status)) {
      if (Status != EFI_NOT_FOUND) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to locate parent handle: %r\r\n",
          __FUNCTION__,
          Status
          ));
      }

      continue;
    }

    if (ParentHandle == Context->ControllerHandle) {
      /* This handle is our child handle. */
      break;
    }
  }

  if (HandleIndex < HandleCount) {
    *Handle = Handles[HandleIndex];
    Status  = EFI_SUCCESS;
  } else {
    Status = EFI_NOT_FOUND;
  }

Exit:
  if (Handles != NULL) {
    FreePool (Handles);
  }

  return Status;
}

/**
   Locates an instance of the GOP protocol installed on a child
   handle.

   @param[in]  Context   Context whose child GOP shall be located.
   @param[out] Protocol  The located GOP instance.

   @retval EFI_SUCCESS    Child handle found successfully.
   @retval !=EFI_SUCCESS  Error occurred.
*/
STATIC
EFI_STATUS
DisplayLocateChildGop (
  IN  NVIDIA_DISPLAY_CONTROLLER_CONTEXT *CONST  Context,
  OUT EFI_GRAPHICS_OUTPUT_PROTOCOL     **CONST  Protocol
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  GopHandle;

  Status = DisplayLocateChildGopHandle (Context, &GopHandle);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->OpenProtocol (
                  GopHandle,
                  &gEfiGraphicsOutputProtocolGuid,
                  (VOID **)Protocol,
                  Context->DriverHandle,
                  Context->ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to retrieve graphics output protocol from handle %p: %r\r\n",
      __FUNCTION__,
      GopHandle,
      Status
      ));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
   Checks if the given GOP instance has an active mode with a frame
   buffer available.

   @param[in] Gop  The GOP protocol instance to check.

   @return TRUE   A mode is active and a frame buffer is available.
   @return FALSE  No mode is active.
   @return FALSE  A mode is active, but no frame buffer is available.
*/
STATIC
BOOLEAN
CheckGopModeActiveWithFrameBuffer (
  IN CONST EFI_GRAPHICS_OUTPUT_PROTOCOL *CONST  Gop
  )
{
  CONST EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *CONST  Mode = Gop->Mode;

  return (  (Mode != NULL)
         && (Mode->Mode < Mode->MaxMode)
         && (Mode->Info != NULL)
         && (Mode->SizeOfInfo >= sizeof (*Mode->Info))
         && (Mode->Info->PixelFormat != PixelBltOnly)
         && (Mode->FrameBufferBase != 0)
         && (Mode->FrameBufferSize != 0));
}

/**
   Event handler for whenever a new Device Tree is installed on the
   system.

   @param[in] Event    Event used for the notification.
   @param[in] Context  Controller context to use.
*/
STATIC
VOID
EFIAPI
DisplayOnFdtInstalled (
  IN EFI_EVENT                                 Event,
  IN NVIDIA_DISPLAY_CONTROLLER_CONTEXT *CONST  Context
  )
{
  EFI_STATUS                    Status;
  VOID                          *Fdt;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop;

  Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &Fdt);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to retrieve FDT: %r\r\n",
      __FUNCTION__,
      Status
      ));
    return;
  }

  Status = DisplayLocateChildGop (Context, &Gop);
  if (EFI_ERROR (Status)) {
    return;
  }

  if (CheckGopModeActiveWithFrameBuffer (Gop)) {
    UpdateDeviceTreeSimpleFramebufferInfo (
      Fdt,
      Gop->Mode->Info,
      (UINT64)Gop->Mode->FrameBufferBase,
      (UINT64)Gop->Mode->FrameBufferSize
      );
  }
}

/**
   Event handler for when the read-to-boot event is signalled.

   @param[in] Event    Event used for the notification.
   @param[in] Context  Controller context to use.
*/
STATIC
VOID
EFIAPI
DisplayOnReadyToBoot (
  IN EFI_EVENT                                 Event,
  IN NVIDIA_DISPLAY_CONTROLLER_CONTEXT *CONST  Context
  )
{
  EFI_STATUS                    Status;
  VOID                          *Fdt;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop;

  Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &Fdt);
  if (EFI_ERROR (Status)) {
    return;
  }

  Status = DisplayLocateChildGop (Context, &Gop);
  if (EFI_ERROR (Status)) {
    return;
  }

  if (CheckGopModeActiveWithFrameBuffer (Gop)) {
    UpdateDeviceTreeSimpleFramebufferInfo (
      Fdt,
      Gop->Mode->Info,
      (UINT64)Gop->Mode->FrameBufferBase,
      (UINT64)Gop->Mode->FrameBufferSize
      );
  }
}

/**
  Performs the necessary initialization of the display hardware.

  @param[in] DriverHandle             Handle of the driver.
  @param[in] ControllerHandle         Handle of the controller.

  @retval EFI_SUCCESS              Operation successful.
  @retval others                   Error occurred
*/
STATIC
EFI_STATUS
DisplayStart (
  OUT NVIDIA_DISPLAY_CONTROLLER_CONTEXT **CONST  Context,
  IN  EFI_HANDLE                                 DriverHandle,
  IN  EFI_HANDLE                                 ControllerHandle
  )
{
  EFI_STATUS                         Status;
  UINTN                              ResourcesSize;
  NON_DISCOVERABLE_DEVICE            *NvNonDiscoverableDevice;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  FramebufferDescriptor, *FramebufferResource = NULL;
  NVIDIA_DISPLAY_CONTROLLER_CONTEXT  *Result     = NULL;
  CONST BOOLEAN                      UseDpOutput = FALSE;

  CONST UINTN  FramebufferResourceIndex = (UINTN)(PcdGet8 (PcdFramebufferBarIndex) + 1) - 1;

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gNVIDIANonDiscoverableDeviceProtocolGuid,
                  (VOID **)&NvNonDiscoverableDevice,
                  DriverHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to open NVIDIA non-discoverable device protocol: %r\r\n",
      __FUNCTION__,
      Status
      ));
    goto Exit;
  }

  if (FramebufferResourceIndex != MAX_UINTN) {
    Status = CreateFramebufferResource (&FramebufferDescriptor);
    if (EFI_ERROR (Status)) {
      goto Exit;
    }

    FramebufferResource = &FramebufferDescriptor;
  }

  Status = CopyAndInsertResource (
             NULL,
             &ResourcesSize,
             NvNonDiscoverableDevice->Resources,
             (CONST EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR **)&FramebufferResource,
             FramebufferResourceIndex
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: could not determine size of resources: %r\r\n",
      __FUNCTION__,
      Status
      ));
    goto Exit;
  }

  Result = (NVIDIA_DISPLAY_CONTROLLER_CONTEXT *)AllocateZeroPool (sizeof (*Result) + ResourcesSize);
  if (Result == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: could not allocate %u bytes for display controller context\r\n",
      __FUNCTION__,
      sizeof (*Result) + ResourcesSize
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Result->Signature        = DISPLAY_CONTROLLER_SIGNATURE;
  Result->DriverHandle     = DriverHandle;
  Result->ControllerHandle = ControllerHandle;

  CopyMem (
    &Result->EdkiiNonDiscoverableDevice,
    NvNonDiscoverableDevice,
    sizeof (Result->EdkiiNonDiscoverableDevice)
    );

  Result->EdkiiNonDiscoverableDevice.Resources =
    (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)(Result + 1);

  Status = CopyAndInsertResource (
             Result->EdkiiNonDiscoverableDevice.Resources,
             NULL,
             NvNonDiscoverableDevice->Resources,
             (CONST EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR **)&FramebufferResource,
             FramebufferResourceIndex
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: could not insert resource: %r\r\n",
      __FUNCTION__,
      Status
      ));
    goto Exit;
  }

  Status = ResetRequiredDisplayEngines (ControllerHandle, FALSE);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Result->ResetsDeasserted = TRUE;

  Status = EnableRequiredDisplayClocks (ControllerHandle, TRUE);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Result->ClocksEnabled = TRUE;

  Status = ConfigureOutputGpios (ControllerHandle, TRUE, UseDpOutput);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Result->OutputGpiosConfigured = TRUE;

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  (EFI_EVENT_NOTIFY)DisplayOnFdtInstalled,
                  Result,
                  &gFdtTableGuid,
                  &Result->OnFdtInstalledEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to create OnFdtInstalled event: %r\r\n",
      __FUNCTION__,
      Status
      ));
    Result->OnFdtInstalledEvent = NULL;
    goto Exit;
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  (EFI_EVENT_NOTIFY)DisplayOnReadyToBoot,
                  Result,
                  &gEfiEventReadyToBootGuid,
                  &Result->OnReadyToBootEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to create OnReadyToBoot event: %r\r\n",
      __FUNCTION__,
      Status
      ));
    Result->OnReadyToBootEvent = NULL;
    goto Exit;
  }

  *Context = Result;

Exit:
  if (EFI_ERROR (Status)) {
    DisplayStop (Result);
  }

  return Status;
}

/**
  Callback that will be invoked at various phases of the driver initialization

  This function allows for modification of system behavior at various points in
  the driver binding process.

  @param[in] Phase                    Current phase of the driver initialization
  @param[in] DriverHandle             Handle of the driver.
  @param[in] ControllerHandle         Handle of the controller.
  @param[in] DeviceTreeNode           Pointer to the device tree node protocol is available.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_SUCCESS              Driver does not handle this phase
  @retval others                   Error occurred

**/
EFI_STATUS
DeviceDiscoveryNotify (
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES          Phase,
  IN  EFI_HANDLE                              DriverHandle,
  IN  EFI_HANDLE                              ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS                         Status;
  TEGRA_PLATFORM_TYPE                Platform;
  NON_DISCOVERABLE_DEVICE            *EdkiiNonDiscoverableDevice;
  NVIDIA_DISPLAY_CONTROLLER_CONTEXT  *Context;
  EFI_GRAPHICS_OUTPUT_PROTOCOL       *Gop;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingSupported:
      Platform = TegraGetPlatform ();
      if (Platform != TEGRA_PLATFORM_SILICON) {
        return EFI_UNSUPPORTED;
      }

      return EFI_SUCCESS;

    case DeviceDiscoveryDriverBindingStart:
      Status = DisplayStart (&Context, DriverHandle, ControllerHandle);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      Status = gBS->InstallMultipleProtocolInterfaces (
                      &ControllerHandle,
                      &gEdkiiNonDiscoverableDeviceProtocolGuid,
                      &Context->EdkiiNonDiscoverableDevice,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to install non-discoverable device protocol: %r\r\n",
          __FUNCTION__,
          Status
          ));
        DisplayStop (Context);
      }

      return Status;

    case DeviceDiscoveryDriverBindingStop:
    case DeviceDiscoveryOnExit:
      Status = gBS->OpenProtocol (
                      ControllerHandle,
                      &gEdkiiNonDiscoverableDeviceProtocolGuid,
                      (VOID **)&EdkiiNonDiscoverableDevice,
                      DriverHandle,
                      ControllerHandle,
                      EFI_OPEN_PROTOCOL_GET_PROTOCOL
                      );
      if (Status == EFI_UNSUPPORTED) {
        return EFI_SUCCESS;
      } else if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to open non-discoverable device protocol: %r\r\n",
          __FUNCTION__,
          Status
          ));
        return Status;
      }

      Context = DISPLAY_CONTROLLER_CONTEXT_FROM_EDKII_DEVICE (EdkiiNonDiscoverableDevice);

      if (Phase == DeviceDiscoveryDriverBindingStop) {
        Status = gBS->UninstallMultipleProtocolInterfaces (
                        ControllerHandle,
                        &gEdkiiNonDiscoverableDeviceProtocolGuid,
                        EdkiiNonDiscoverableDevice,
                        NULL
                        );
        if (EFI_ERROR (Status)) {
          DEBUG ((
            DEBUG_ERROR,
            "%a: failed to uninstall non-discoverable device protocol: %r\r\n",
            __FUNCTION__,
            Status
            ));
          return Status;
        }

        return DisplayStop (Context);
      } else {
        Status = DisplayLocateChildGop (Context, &Gop);
        if (!EFI_ERROR (Status) && CheckGopModeActiveWithFrameBuffer (Gop)) {
          /* We have an active GOP child, leave the display running. */
          return EFI_ABORTED;
        }

        /* The display is inactive, reset to known good state. */
        Status = DisplayBypassSorClocks (Context);
        ASSERT_EFI_ERROR (Status);

        return DisplayStopOnExitBootServices (Context);
      }

    default:
      return EFI_SUCCESS;
  }
}
