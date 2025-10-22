/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2023-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <PiDxe.h>

#include <Library/DebugLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/DisplayDeviceTreeHelperLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/PrintLib.h>

#include <libfdt.h>

///
/// Maximum number of maximum clock rates supported by
/// DisplayDeviceTreeUpdateMaxClockRates.
///
#define MAX_CLK_RATE_MAX_COUNT  4

/**
   Updates Device Tree framebuffer region node at given offset with
   the framebuffer region address and size.

   @param[in] DeviceTree  Device Tree to update.
   @param[in] NodeOffset  Offset of the node to update.
   @param[in] Base        Base address of the framebuffer region.
   @param[in] Size        Size of the framebuffer region.

   @retval TRUE   Update successful.
   @retval FALSE  Update failed.
*/
STATIC
BOOLEAN
UpdateDeviceTreeFrameBufferRegionNode (
  IN VOID *CONST   DeviceTree,
  IN CONST INT32   NodeOffset,
  IN CONST UINT64  Base,
  IN CONST UINT64  Size
  )
{
  INT32        Result;
  CONST VOID   *Prop;
  CONST CHAR8  *Name, *NameEnd;
  CHAR8        NameBuffer[64];
  UINT64       RegProp[2];
  UINT32       IommuAddrsProp[5];

  CONST UINT32  BaseLo = (UINT32)Base;
  CONST UINT32  BaseHi = (UINT32)(Base >> 32);

  Name = fdt_get_name (DeviceTree, NodeOffset, &Result);
  if (Name == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to get name: %r\r\n",
      __FUNCTION__,
      fdt_strerror (Result)
      ));
    return FALSE;
  }

  NameEnd = AsciiStrStr (Name, "@");
  if (NameEnd == NULL) {
    NameEnd = Name + Result;
  }

  if (BaseHi != 0) {
    Result = (INT32)AsciiSPrint (
                      NameBuffer,
                      sizeof (NameBuffer),
                      "%.*a@%x,%x",
                      (UINTN)(NameEnd - Name),
                      Name,
                      BaseHi,
                      BaseLo
                      );
  } else {
    Result = (INT32)AsciiSPrint (
                      NameBuffer,
                      sizeof (NameBuffer),
                      "%.*a@%x",
                      (UINTN)(NameEnd - Name),
                      Name,
                      BaseLo
                      );
  }

  /* AsciiSPrint returns the number of written ASCII charaters not
     including the Null-terminator. Adding +1 ensures we would have
     had room for another character, therefore the result was not
     truncated. */
  if (!(Result + 1 < ARRAY_SIZE (NameBuffer))) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: name '%a' is too long\r\n",
      __FUNCTION__,
      Name
      ));
    return FALSE;
  }

  Result = fdt_set_name (DeviceTree, NodeOffset, NameBuffer);
  if (Result != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to set name: %r\r\n",
      __FUNCTION__,
      fdt_strerror (Result)
      ));
    return FALSE;
  }

  RegProp[0] = SwapBytes64 (Base);
  RegProp[1] = SwapBytes64 (Size);

  Result = fdt_setprop_inplace (
             DeviceTree,
             NodeOffset,
             "reg",
             RegProp,
             sizeof (RegProp)
             );
  if (Result != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to set 'reg' property: %a\r\n",
      __FUNCTION__,
      fdt_strerror (Result)
      ));
    return FALSE;
  }

  Prop = fdt_getprop (DeviceTree, NodeOffset, "iommu-addresses", &Result);
  if (Prop != NULL) {
    if (Result != sizeof (IommuAddrsProp)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: 'iommu-addresses' property size mismatch: expected %lu, got %lu\r\n",
        __FUNCTION__,
        (UINT64)sizeof (IommuAddrsProp),
        (UINT64)Result
        ));
      return FALSE;
    }

    /* Copy device phandle */
    IommuAddrsProp[0] = *(CONST UINT32 *)Prop;

    /* Set up IOMMU identity mapping */
    CopyMem (&IommuAddrsProp[1], RegProp, sizeof (RegProp));

    Result = fdt_setprop_inplace (
               DeviceTree,
               NodeOffset,
               "iommu-addresses",
               IommuAddrsProp,
               sizeof (IommuAddrsProp)
               );
    if (Result != 0) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to set 'iommu-addresses' property: %a\r\n",
        __FUNCTION__,
        fdt_strerror (Result)
        ));
      return FALSE;
    }
  }

  Result = fdt_setprop_string (
             DeviceTree,
             NodeOffset,
             "status",
             "okay"
             );
  if (Result != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to set 'status' property: %a\r\n",
      __FUNCTION__,
      fdt_strerror (Result)
      ));
    return FALSE;
  }

  return TRUE;
}

/**
   Updates Device Tree simple-framebuffer node at given offset with
   details about the given graphics output mode and framebuffer
   region.

   @param[in] DeviceTree       Device Tree to update.
   @param[in] NodeOffset       Offset of the node to update.
   @param[in] ModeInfo         Pointer to the mode information to use.
   @param[in] FrameBufferBase  Base address of the framebuffer region.
   @param[in] FrameBufferSize  Size of the framebuffer region.

   @retval TRUE   Update successful.
   @retval FALSE  Update failed.
*/
STATIC
BOOLEAN
UpdateDeviceTreeSimpleFramebufferNode (
  IN VOID *CONST                                        DeviceTree,
  IN CONST INT32                                        NodeOffset,
  IN CONST EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *CONST  ModeInfo,
  IN CONST UINT64                                       FrameBufferBase,
  IN CONST UINT64                                       FrameBufferSize
  )
{
  STATIC CONST CHAR8              FbRgbxFormat[]   = "x8b8g8r8";
  STATIC CONST CHAR8              FbBgrxFormat[]   = "x8r8g8b8";
  STATIC CONST EFI_PIXEL_BITMASK  PixelBitMaskRgbx = { 0xFFU <<  0, 0xFFU << 8, 0xFFU << 16, 0xFFU << 24 };
  STATIC CONST EFI_PIXEL_BITMASK  PixelBitMaskBgrx = { 0xFFU << 16, 0xFFU << 8, 0xFFU <<  0, 0xFFU << 24 };

  INT32        Result;
  CONST CHAR8  *FbFormat;
  UINT32       PixelSize;
  UINT32       Stride;
  UINT64       FrameBufferSizeMin;
  CONST VOID   *Prop;
  UINT32       MemoryRegionPhandle;
  INT32        MemoryRegionOffset;

  switch (ModeInfo->PixelFormat) {
    case PixelRedGreenBlueReserved8BitPerColor:
      FbFormat  = FbRgbxFormat;
      PixelSize = 4;
      break;

    case PixelBlueGreenRedReserved8BitPerColor:
      FbFormat  = FbBgrxFormat;
      PixelSize = 4;
      break;

    case PixelBitMask:
    case PixelBltOnly:
      /* UEFI spec says PixelInformation is only valid if PixelFormat
         is PixelBitMask, but attempt to recover the real pixel format
         for PixelBltOnly too. */
      if (0 == CompareMem (
                 &ModeInfo->PixelInformation,
                 &PixelBitMaskRgbx,
                 sizeof (ModeInfo->PixelInformation)
                 ))
      {
        FbFormat  = FbRgbxFormat;
        PixelSize = 4;
        break;
      }

      if (0 == CompareMem (
                 &ModeInfo->PixelInformation,
                 &PixelBitMaskBgrx,
                 sizeof (ModeInfo->PixelInformation)
                 ))
      {
        FbFormat  = FbBgrxFormat;
        PixelSize = 4;
        break;
      }

    /* fallthrough */

    default:
      DEBUG ((
        DEBUG_ERROR,
        "%a: unsupported pixel format: %lu\r\n",
        __FUNCTION__,
        (UINT64)ModeInfo->PixelFormat
        ));
      return FALSE;
  }

  Stride             = ModeInfo->PixelsPerScanLine * PixelSize;
  FrameBufferSizeMin = (UINT64)ModeInfo->VerticalResolution * (UINT64)Stride;
  if (FrameBufferSize < FrameBufferSizeMin) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: framebuffer too small: got %lu bytes, but need at least %lu bytes\r\n",
      __FUNCTION__,
      FrameBufferSize,
      FrameBufferSizeMin
      ));
    return FALSE;
  }

  Prop = fdt_getprop (DeviceTree, NodeOffset, "memory-region", &Result);
  if (Prop == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to get 'memory-region': %a\r\n",
      __FUNCTION__,
      fdt_strerror (Result)
      ));
    return FALSE;
  } else if (Result != sizeof (UINT32)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: unexpected size of 'memory-region': expected %u, got %d\r\n",
      __FUNCTION__,
      (UINTN)sizeof (UINT32),
      (INTN)Result
      ));
    return FALSE;
  }

  MemoryRegionPhandle = SwapBytes32 (*(CONST UINT32 *)Prop);

  Result = fdt_setprop_inplace_u32 (
             DeviceTree,
             NodeOffset,
             "width",
             ModeInfo->HorizontalResolution
             );
  if (Result != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to set 'width' property: %a\r\n",
      __FUNCTION__,
      fdt_strerror (Result)
      ));
    return FALSE;
  }

  Result = fdt_setprop_inplace_u32 (
             DeviceTree,
             NodeOffset,
             "height",
             ModeInfo->VerticalResolution
             );
  if (Result != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to set 'height' property: %a\r\n",
      __FUNCTION__,
      fdt_strerror (Result)
      ));
    return FALSE;
  }

  Result = fdt_setprop_inplace_u32 (
             DeviceTree,
             NodeOffset,
             "stride",
             Stride
             );
  if (Result != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to set 'stride' property: %a\r\n",
      __FUNCTION__,
      fdt_strerror (Result)
      ));
    return FALSE;
  }

  Result = fdt_setprop_string (
             DeviceTree,
             NodeOffset,
             "format",
             FbFormat
             );
  if (Result != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to set 'format' property: %a\r\n",
      __FUNCTION__,
      fdt_strerror (Result)
      ));
    return FALSE;
  }

  Result = fdt_setprop_string (
             DeviceTree,
             NodeOffset,
             "status",
             "okay"
             );
  if (Result != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to set 'status' property: %a\r\n",
      __FUNCTION__,
      fdt_strerror (Result)
      ));
    return FALSE;
  }

  MemoryRegionOffset = fdt_node_offset_by_phandle (DeviceTree, MemoryRegionPhandle);
  if (MemoryRegionOffset < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: cannot find memory region node by phandle 0x%x: %a\r\n",
      __FUNCTION__,
      (UINTN)MemoryRegionPhandle,
      fdt_strerror (MemoryRegionOffset)
      ));
    return FALSE;
  }

  return UpdateDeviceTreeFrameBufferRegionNode (
           DeviceTree,
           MemoryRegionOffset,
           FrameBufferBase,
           FrameBufferSizeMin
           );
}

/**
   Updates Device Tree simple-framebuffer node(s) with details about
   the given graphics output mode and framebuffer region.

   @param[in] DeviceTree       Device Tree to update.
   @param[in] ModeInfo         Pointer to the mode information to use.
   @param[in] FrameBufferBase  Base address of the framebuffer region.
   @param[in] FrameBufferSize  Size of the framebuffer region.

   @retval TRUE   Update successful.
   @retval FALSE  Update failed.
*/
BOOLEAN
UpdateDeviceTreeSimpleFramebufferInfo (
  IN VOID *CONST                                        DeviceTree,
  IN CONST EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *CONST  ModeInfo,
  IN CONST UINT64                                       FrameBufferBase,
  IN CONST UINT64                                       FrameBufferSize
  )
{
  INT32  Result, NodeOffset;
  UINTN  NodeCount;

  Result = fdt_path_offset (DeviceTree, "/chosen");
  if (Result < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: cannot find node '/chosen': %a\r\n",
      __FUNCTION__,
      fdt_strerror (Result)
      ));
    return FALSE;
  }

  NodeCount = 0;
  fdt_for_each_subnode (NodeOffset, DeviceTree, Result) {
    Result = fdt_node_check_compatible (DeviceTree, NodeOffset, "simple-framebuffer");
    if (Result != 0) {
      continue;
    }

    if (!UpdateDeviceTreeSimpleFramebufferNode (
           DeviceTree,
           NodeOffset,
           ModeInfo,
           FrameBufferBase,
           FrameBufferSize
           ))
    {
      return FALSE;
    }

    ++NodeCount;
  }

  if ((NodeOffset < 0) && (NodeOffset != -FDT_ERR_NOTFOUND)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: failed to enumerate children of '/chosen': %a\r\n",
      __FUNCTION__,
      fdt_strerror (NodeOffset)
      ));
    return FALSE;
  }

  if (NodeCount == 0) {
    DEBUG ((
      DEBUG_WARN,
      "%a: no compatible framebuffer nodes found\r\n",
      __FUNCTION__
      ));
  }

  return NodeCount > 0;
}

/**
  Update node property data if the property itself exists.

  @param[in] NodeOffset    Node offset.
  @param[in] Property      Property name.
  @param[in] PropertyData  Data of the property.
  @param[in] PropertySize  Size of the property data.

  @retval EFI_SUCCESS            Property data successfully updated.
  @retval EFI_SUCCESS            The property does not exist.
  @retval EFI_INVALID_PARAMETER  Property is NULL.
  @retval EFI_INVALID_PARAMETER  PropertySize is positive, but PropertyData is NULL.
  @retval EFI_DEVICE_ERROR       Other errors.
*/
STATIC
EFI_STATUS
SetNodePropertyIfExists (
  IN CONST INT32         NodeOffset,
  IN CONST CHAR8 *CONST  Property,
  IN CONST VOID  *CONST  PropertyData,
  IN CONST UINT32        PropertySize
  )
{
  EFI_STATUS  Status;

  Status = DeviceTreeGetNodeProperty (NodeOffset, Property, NULL, NULL);
  if (!EFI_ERROR (Status)) {
    Status = DeviceTreeSetNodeProperty (NodeOffset, Property, PropertyData, PropertySize);
  } else if (Status == EFI_NOT_FOUND) {
    Status = EFI_SUCCESS;
  }

  return Status;
}

/**
  Update Device Tree display node with maximum dispclk/hubclk rates.

  This function does not allocate any memory, hence it is safe to call
  during ExitBootServices.

  @param[in,opt] DeviceTree           Base of the Device Tree to update.
  @param[in]     DisplayNodePath      Path to the display node.
  @param[in]     MaxDispClkRateKhz    Maximum dispclk rates in kHz.
  @param[in]     MaxDispClkRateCount  Number of maximum dispclk rates.
  @param[in]     MaxHubClkRateKhz     Maximum hubclk rates in kHz.
  @param[in]     MaxHubClkRateCount   Number of maximum hubclk rates.

  @retval EFI_SUCCESS            Node successfully updated.
  @retval EFI_INVALID_PARAMETER  DisplayNodePath is NULL.
  @retval EFI_INVALID_PARAMETER  MaxDispClkRateCount is non-zero, but
                                 MaxDispClkRateKhz is NULL.
  @retval EFI_INVALID_PARAMETER  MaxHubClkRateCount is non-zero, but
                                 MaxHubClkRateKhz is NULL.
  @retval EFI_OUT_OF_RESOURCES   MaxDispClkRateCount is too large.
  @retval EFI_OUT_OF_RESOURCES   MaxHubClkRateCount is too large.
  @retval EFI_NOT_FOUND          Node specified by DevicePath not found.
  @retval EFI_DEVICE_ERROR       Other errors.
*/
EFI_STATUS
EFIAPI
DisplayDeviceTreeUpdateMaxClockRates (
  IN VOID         *CONST  DeviceTree  OPTIONAL,
  IN CONST CHAR8  *CONST  DisplayNodePath,
  IN CONST UINT32 *CONST  MaxDispClkRateKhz,
  IN CONST UINTN          MaxDispClkRateCount,
  IN CONST UINT32 *CONST  MaxHubClkRateKhz,
  IN CONST UINTN          MaxHubClkRateCount
  )
{
  EFI_STATUS  Status;
  INT32       NodeOffset;
  VOID        *OldDeviceTree;
  UINTN       OldDeviceTreeSize;
  UINTN       Index, Count;
  UINT32      PropertyData[MAX_CLK_RATE_MAX_COUNT];

  NV_ASSERT_RETURN (
    DisplayNodePath != NULL,
    return EFI_INVALID_PARAMETER,
    "%a: DisplayNodePath is NULL",
    __FUNCTION__
    );
  NV_ASSERT_RETURN (
    (MaxDispClkRateKhz != NULL) || (MaxDispClkRateCount == 0),
    return EFI_INVALID_PARAMETER,
    "%a: MaxDispClkRateCount is non-zero, but MaxDispClkRateKhz is NULL",
    __FUNCTION__
    );
  NV_ASSERT_RETURN (
    (MaxHubClkRateKhz != NULL) || (MaxHubClkRateCount == 0),
    return EFI_INVALID_PARAMETER,
    "%a: MaxHubClkRateCount is non-zero, but MaxHubClkRateKhz is NULL",
    __FUNCTION__
    );
  NV_ASSERT_RETURN (
    MaxDispClkRateCount <= MAX_CLK_RATE_MAX_COUNT,
    return EFI_OUT_OF_RESOURCES,
    "%a: MaxDispClkRateCount is too large: %lu\r\n",
    __FUNCTION__,
    (UINT64)MaxDispClkRateCount
    );
  NV_ASSERT_RETURN (
    MaxHubClkRateCount <= MAX_CLK_RATE_MAX_COUNT,
    return EFI_OUT_OF_RESOURCES,
    "%a: MaxHubClkRateCount is too large: %lu\r\n",
    __FUNCTION__,
    (UINT64)MaxHubClkRateCount
    );

  if (DeviceTree != NULL) {
    Status = GetDeviceTreePointer (&OldDeviceTree, &OldDeviceTreeSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_WARN,
        "%a: failed to retrieve Device Tree pointer: %r\r\n",
        __FUNCTION__,
        Status
        ));
      OldDeviceTree     = NULL;
      OldDeviceTreeSize = 0;
    }

    SetDeviceTreePointer (NULL, 0);
    SetDeviceTreePointer (DeviceTree, fdt_totalsize (DeviceTree));
  }

  Status = DeviceTreeGetNodeByPath (DisplayNodePath, &NodeOffset);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Count = MaxDispClkRateCount;
  for (Index = 0; Index < Count; ++Index) {
    PropertyData[Index] = cpu_to_fdt32 (MaxDispClkRateKhz[Index]);
  }

  Status = SetNodePropertyIfExists (
             NodeOffset,
             "nvidia,max-disp-clk-rate-khz",
             PropertyData,
             Count * sizeof (*PropertyData)
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Count = MaxHubClkRateCount;
  for (Index = 0; Index < Count; ++Index) {
    PropertyData[Index] = cpu_to_fdt32 (MaxHubClkRateKhz[Index]);
  }

  Status = SetNodePropertyIfExists (
             NodeOffset,
             "nvidia,max-hub-clk-rate-khz",
             PropertyData,
             Count * sizeof (*PropertyData)
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = EFI_SUCCESS;

Exit:
  if (DeviceTree != NULL) {
    SetDeviceTreePointer (NULL, 0);
    SetDeviceTreePointer (OldDeviceTree, OldDeviceTreeSize);
  }

  return Status;
}

/**
  Update Device Tree display node with allocated ISO bandwidth and
  memory clock floor.

  This function does not allocate any memory, hence it is safe to call
  during ExitBootServices.

  @param[in,opt] DisplayNodePath            Path to the display node.
  @param[in]     IsoBandwidthKbytesPerSec   Requested ISO bandwidth.
  @param[in]     MemclockFloorKbytesPerSec  Requested memory clock floor.

  @retval EFI_SUCCESS            Node successfully updated.
  @retval EFI_INVALID_PARAMETER  DisplayNodePath is NULL.
  @retval EFI_NOT_FOUND          Node specified by DevicePath not found.
  @retval EFI_DEVICE_ERROR       Other errors.
*/
EFI_STATUS
EFIAPI
DisplayDeviceTreeUpdateIsoBandwidth (
  IN VOID        *CONST  DeviceTree  OPTIONAL,
  IN CONST CHAR8 *CONST  DisplayNodePath,
  IN CONST UINT32        IsoBandwidthKbytesPerSec,
  IN CONST UINT32        MemclockFloorKbytesPerSec
  )
{
  EFI_STATUS  Status;
  INT32       NodeOffset;
  VOID        *OldDeviceTree;
  UINTN       OldDeviceTreeSize;
  UINT32      PropertyData;

  NV_ASSERT_RETURN (
    DisplayNodePath != NULL,
    return EFI_INVALID_PARAMETER,
    "%a: DisplayNodePath is NULL",
    __FUNCTION__
    );

  if (DeviceTree != NULL) {
    Status = GetDeviceTreePointer (&OldDeviceTree, &OldDeviceTreeSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_WARN,
        "%a: failed to retrieve Device Tree pointer: %r\r\n",
        __FUNCTION__,
        Status
        ));
      OldDeviceTree     = NULL;
      OldDeviceTreeSize = 0;
    }

    SetDeviceTreePointer (NULL, 0);
    SetDeviceTreePointer (DeviceTree, fdt_totalsize (DeviceTree));
  }

  Status = DeviceTreeGetNodeByPath (DisplayNodePath, &NodeOffset);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  PropertyData = cpu_to_fdt32 (IsoBandwidthKbytesPerSec);

  Status = SetNodePropertyIfExists (
             NodeOffset,
             "nvidia,iso-bandwidth-kbps",
             &PropertyData,
             sizeof (PropertyData)
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  PropertyData = cpu_to_fdt32 (MemclockFloorKbytesPerSec);

  Status = SetNodePropertyIfExists (
             NodeOffset,
             "nvidia,dram-floor-kbps",
             &PropertyData,
             sizeof (PropertyData)
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = EFI_SUCCESS;

Exit:
  if (DeviceTree != NULL) {
    SetDeviceTreePointer (NULL, 0);
    SetDeviceTreePointer (OldDeviceTree, OldDeviceTreeSize);
  }

  return Status;
}
