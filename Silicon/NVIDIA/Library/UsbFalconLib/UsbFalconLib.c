/** @file

  Falcon Register Access

  Copyright (c) 2019-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/UsbFalconLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <string.h>
#include <Library/DmaLib.h>


/* Base Address of Xhci Controller's Configuration registers. These config
 * registers are used to access the Falcon Registers and for FW Loading.
 * This address should be set first by calling FalconSetHostCfgAddr() before
 * accessing any other functions in the Falcon Library
 */
STATIC UINTN XusbHostCfgAddr = 0;
STATIC UINTN XusbHostBase2Addr = 0;
STATIC UINTN XusbAoAddr = 0;

VOID
FalconSetHostCfgAddr (
  IN UINTN Address
  )
{
  XusbHostCfgAddr = Address;
}

VOID
FalconSetHostBase2Addr (
  IN UINTN Address
  )
{
  XusbHostBase2Addr = Address;
}

VOID
FalconSetAoAddr (
  IN UINTN Address
  )
{
  XusbAoAddr = Address;
}

VOID *
FalconMapReg (
  IN  UINTN Address
  )
{
  UINTN PageIndex = Address / 0x200;
  UINTN PageOffset = Address % 0x200;
  VOID *Register;

  if (XusbHostBase2Addr != 0)
  {
    MmioWrite32(XusbHostBase2Addr + XUSB_BAR2_ARU_C11_CSBRANGE /* BAR2 CSBRANGE */, PageIndex);
    Register = (VOID *) (XusbHostBase2Addr + XUSB_BAR2_CSB_BASE_ADDR + PageOffset);
    return Register;
  }

  /* write page index into XUSB PCI CFG register CSBRANGE */
  MmioWrite32(XusbHostCfgAddr + 0x41c /* CSBRANGE */, PageIndex);

  /* calculate falcon register address within 512-byte aperture in XUSB PCI CFG space between offsets 0x800 and 0xa00 */
  Register = (VOID *) (XusbHostCfgAddr + 0x800 + PageOffset);

  return Register;

}

UINT32
FalconRead32 (
  IN  UINTN Address
  )
{
  UINT32 *Register32;
  if (XusbHostCfgAddr == 0) {
    DEBUG ((EFI_D_ERROR, "%a:Invalid Xhci Config Address\n", __FUNCTION__));
    return 0;
  }
  Register32 = (UINT32 *) FalconMapReg (Address);
  UINT32 Value = MmioRead32 ((UINTN) Register32);

  DEBUG ((EFI_D_VERBOSE, "%a: %x --> %x\r\n", __FUNCTION__, Address, Value));

  return Value;

}

UINT32
FalconWrite32 (
  IN  UINTN Address,
  IN  UINT32 Value
  )
{
  UINT32 *Register32;
  if (XusbHostCfgAddr == 0) {
    DEBUG ((EFI_D_ERROR, "%a:Invalid Xhci Config Address\n", __FUNCTION__));
    return 0;
  }
  Register32 = (UINT32 *) FalconMapReg (Address);

  DEBUG ((EFI_D_VERBOSE, "%a: %x <-- %x\r\n", __FUNCTION__, Address, Value));

  MmioWrite32((UINTN) Register32, Value);

  return Value;

}

UINT32
Fpci2Read32 (
  IN  UINTN Address
  )
{
  UINT32 *Register32;
  if (XusbHostBase2Addr == 0) {
    DEBUG ((EFI_D_ERROR, "%a:Invalid Xhci Config Address\n", __FUNCTION__));
    return 0;
  }
  Register32 = (UINT32 *) (XusbHostBase2Addr + Address);
  UINT32 Value = MmioRead32 ((UINTN) Register32);

  DEBUG ((EFI_D_VERBOSE, "%a: %x --> %x\r\n", __FUNCTION__, Address, Value));

  return Value;

}

UINT32
Fpci2Write32 (
  IN  UINTN Address,
  IN  UINT32 Value
  )
{
  UINT32 *Register32;
  if (XusbHostBase2Addr == 0) {
    DEBUG ((EFI_D_ERROR, "%a:Invalid Xhci Config Address\n", __FUNCTION__));
    return 0;
  }

  Register32 = (UINT32 *)(XusbHostBase2Addr + Address);
  DEBUG ((EFI_D_VERBOSE, "%a: %x <-- %x\r\n", __FUNCTION__, Address, Value));

  MmioWrite32((UINTN) Register32, Value);

  return Value;

}

static VOID
FalconDumpDMEM (
  VOID
  )
{
  UINT32 Value;
  UINTN i;

  Value = 0x02000000;
  FalconWrite32 (0x1c0, Value);
  for (i = 0; i < 16; i++)
  {
    Value = FalconRead32 (0x1c4);
    DEBUG ((EFI_D_VERBOSE, "%a: [%d] 0x1c4 = %x\r\n",__FUNCTION__, i, Value));
  }

}

EFI_STATUS
FalconFirmwareIfrLoad (
  IN  UINT8 *Firmware,
  IN  UINT32 FirmwareSize
  )
{
  EFI_STATUS Status;
  UINT32     Value;
  UINT32     Pages;
  UINT32     RegVal, i;
  UINTN      BufferSize;
  UINT8      *FirmwareBuffer;
  UINT64     FirmwareBufferBusAddress;
  VOID       *FirmwareBufferMapping;

  Status = EFI_SUCCESS;
  Value = 0;

  if (XusbAoAddr == 0)
  {
    DEBUG ((EFI_D_ERROR, "%a: XUSB AO Address is not init\n",__FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  Value = FalconRead32 (XUSB_CSB_MEMPOOL_IDIRECT_PC);
  if (Value != 0) {
    DEBUG ((EFI_D_ERROR, "%a: XUSB FW is loaded before, Failed: %r\n",__FUNCTION__, Status));
    return Status;
  }

  Pages = EFI_SIZE_TO_PAGES (FirmwareSize);
  Status  = DmaAllocateAlignedBuffer (EfiRuntimeServicesData, Pages, 256, (void **)&FirmwareBuffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: DmaAllocateAlignedBuffer Failed: %r\n",__FUNCTION__, Status));
    return Status;
  }

  BufferSize = EFI_PAGES_TO_SIZE (Pages);
  Status = DmaMap (MapOperationBusMasterCommonBuffer, FirmwareBuffer, &BufferSize,
             &FirmwareBufferBusAddress, &FirmwareBufferMapping);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: DmaMap Failed: %r\n",__FUNCTION__, Status));
    return Status;
  }

  DEBUG ((EFI_D_ERROR, "%a: Firmware %p FirmwareSize %x (unaligned)\r\n",__FUNCTION__, Firmware, FirmwareSize));
  memset (FirmwareBuffer, 0xdf, BufferSize);
  memcpy (FirmwareBuffer, Firmware, FirmwareSize);
  for (i = 0; i < FirmwareSize; i++)
  {
    if (FirmwareBuffer[i] != Firmware[i])
    {
      DEBUG ((EFI_D_ERROR, "%a: FirmwareBuffer[%d] != Firmware[%d]\r\n",__FUNCTION__, i, i));
      return Status;
    }
  }

  MemoryFence ();
  Firmware = FirmwareBuffer;
  DEBUG ((EFI_D_ERROR, "%a: Firmware %p FirmwareSize %x (aligned)\r\n",__FUNCTION__, Firmware, FirmwareSize));

#define XUSB_BAR2_ARU_IFRDMA_CFG0               0x1bc
#define XUSB_BAR2_ARU_IFRDMA_CFG1               0x1c0
#define XUSB_BAR2_ARU_IFRDMA_STREAMID_FIELD     0x1c4

  /* set IFRDMA address */
  MmioWrite32(XusbAoAddr + XUSB_BAR2_ARU_IFRDMA_CFG0, (UINT32)(FirmwareBufferBusAddress & 0xffffffff));
  MmioWrite32(XusbAoAddr + XUSB_BAR2_ARU_IFRDMA_CFG1, (UINT32)((FirmwareBufferBusAddress >> 32) & 0xff));

  /* set streamid */
  RegVal = MmioRead32(XusbAoAddr + XUSB_BAR2_ARU_IFRDMA_STREAMID_FIELD);
  RegVal &= ~((UINT32) 0xff);
  RegVal |= 0xE;
  MmioWrite32(XusbAoAddr + XUSB_BAR2_ARU_IFRDMA_STREAMID_FIELD, RegVal);

  return Status;
}

EFI_STATUS
FalconFirmwareLoad (
  IN  UINT8 *Firmware,
  IN  UINT32 FirmwareSize,
  IN  BOOLEAN LoadIfrRom
  )
{
  struct tegra_xhci_fw_cfgtbl *FirmwareCfg;
  UINTN FirmwareAddress;
  UINTN SIZE;
  UINTN SRC_ADDR;
  UINTN BOOTPATH;
  UINTN ACTION;
  UINTN DEST_INDEX;
  UINTN SRC_OFFSET;
  UINTN SRC_COUNT;
  UINTN NBLOCKS;
  UINTN TAG_LO;
  UINTN TAG_HI;
  UINTN VEC;
  UINT32 Value;
  UINTN i;
  EFI_STATUS Status = EFI_SUCCESS;
  UINT32 Pages;
  UINTN BufferSize;
  UINT8 *FirmwareBuffer;
  UINTN FirmwareBufferBusAddress;
  VOID *FirmwareBufferMapping;

  DEBUG ((EFI_D_VERBOSE, "%a\r\n",__FUNCTION__));

  if (LoadIfrRom == TRUE)
  {
    Status = FalconFirmwareIfrLoad(Firmware, FirmwareSize);
    return Status;
  }

  /* check if firmware already running */
  Value = FalconRead32 (XUSB_CSB_MEMPOOL_ILOAD_BASE_LO_0);
  if (Value != 0)
  {
    Value = FalconRead32 (FALCON_CPUCTL_0);
    DEBUG ((EFI_D_VERBOSE, "%s: firmware already running cpu state %x\r\n", __FUNCTION__, Value));
    return Status;
  }

  Pages = EFI_SIZE_TO_PAGES (FirmwareSize);
  Status  = DmaAllocateAlignedBuffer (EfiRuntimeServicesData, Pages, 256, (void **)&FirmwareBuffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: DmaAllocateAlignedBuffer Failed: %r\n",__FUNCTION__, Status));
    return Status;
  }

  BufferSize = EFI_PAGES_TO_SIZE (Pages);
  Status = DmaMap (MapOperationBusMasterCommonBuffer, FirmwareBuffer, &BufferSize,
             &FirmwareBufferBusAddress, &FirmwareBufferMapping);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: DmaMap Failed: %r\n",__FUNCTION__, Status));
    return Status;
  }

  DEBUG ((EFI_D_VERBOSE, "%a: Firmware %p FirmwareSize %x (unaligned)\r\n",__FUNCTION__, Firmware, FirmwareSize));
  memset (FirmwareBuffer, 0xdf, BufferSize);
  memcpy (FirmwareBuffer, Firmware, FirmwareSize);
  for (i = 0; i < FirmwareSize; i++)
  {
    if (FirmwareBuffer[i] != Firmware[i])
    {
        DEBUG ((EFI_D_VERBOSE, "%a: FirmwareBuffer[%d] != Firmware[%d]\r\n",__FUNCTION__, i, i));
        return Status;
    }
  }
  MemoryFence ();
  Firmware = FirmwareBuffer;
  DEBUG ((EFI_D_VERBOSE, "%a: Firmware %p FirmwareSize %x (aligned)\r\n",__FUNCTION__, Firmware, FirmwareSize));

  /* Configure FW */
  FirmwareCfg = (struct tegra_xhci_fw_cfgtbl *) Firmware;
  DEBUG ((EFI_D_VERBOSE, "%a: %x %x\r\n",__FUNCTION__, Firmware[0], Firmware[1]));
  DEBUG ((EFI_D_VERBOSE, "%a: %x %x\r\n",__FUNCTION__, Firmware[2], Firmware[3]));
  DEBUG ((EFI_D_VERBOSE, "%a: FirmwareCfg %p ss_portmap %x\r\n",__FUNCTION__, FirmwareCfg, FirmwareCfg->ss_portmap));
  FirmwareCfg->ss_portmap = 0xff;
  DEBUG ((EFI_D_VERBOSE, "%a: FirmwareCfg %p ss_portmap %x\r\n",__FUNCTION__, FirmwareCfg, FirmwareCfg->ss_portmap));
  DEBUG ((EFI_D_VERBOSE, "%a: FirmwareCfg %p num_hsic_port %x\r\n",__FUNCTION__, FirmwareCfg, FirmwareCfg->num_hsic_port));
  FirmwareCfg->num_hsic_port = 0;
  DEBUG ((EFI_D_VERBOSE, "%a: FirmwareCfg %p num_hsic_port %x\r\n",__FUNCTION__, FirmwareCfg, FirmwareCfg->num_hsic_port));
  DEBUG ((EFI_D_VERBOSE, "%a: FirmwareCfg %p boot_codetag %x\r\n",__FUNCTION__, FirmwareCfg, FirmwareCfg->boot_codetag));
  DEBUG ((EFI_D_VERBOSE, "%a: FirmwareCfg %p boot_codesize %x\r\n",__FUNCTION__, FirmwareCfg, FirmwareCfg->boot_codesize));
  DEBUG ((EFI_D_VERBOSE, "%a: FirmwareCfg %p fwimg_len %x\r\n",__FUNCTION__, FirmwareCfg, FirmwareCfg->fwimg_len));

  /* program system memory address where FW code starts */
  FirmwareAddress = FirmwareBufferBusAddress + sizeof(*FirmwareCfg);
  SIZE = FirmwareSize / 256;
  DEBUG ((EFI_D_VERBOSE, "%a: SIZE %x\r\n",__FUNCTION__, SIZE));
  Value = 0;
  Value |= ((SIZE & 0xfff)) << 8;
  FalconWrite32 (XUSB_CSB_MEMPOOL_ILOAD_ATTR_0, Value);
  SRC_ADDR = (FirmwareAddress >> 0) & 0xffffffff;
  DEBUG ((EFI_D_VERBOSE, "%a: SRC_ADDR %x\r\n",__FUNCTION__, SRC_ADDR));
  Value = 0;
  Value |= ((SRC_ADDR & /*0xff*/0xffffffff)) << 0;
  FalconWrite32 (XUSB_CSB_MEMPOOL_ILOAD_BASE_LO_0, Value);
  SRC_ADDR = (FirmwareAddress >> 32) & 0xffffffff;
  DEBUG ((EFI_D_VERBOSE, "%a: SRC_ADDR %x\r\n",__FUNCTION__, SRC_ADDR));
  Value = 0;
  Value |= ((SRC_ADDR & /*0xff*/0xffffffff)) << 0;
  FalconWrite32 (XUSB_CSB_MEMPOOL_ILOAD_BASE_HI_0, Value);

  /* set BOOTPATH to 1 in APMAP */
  BOOTPATH = 1;
  DEBUG ((EFI_D_VERBOSE, "%a: BOOTPATH %x\r\n",__FUNCTION__, BOOTPATH));
  Value = 0;
  Value = FalconRead32 (XUSB_CSB_MEMPOOL_APMAP_0);
  Value |= ((BOOTPATH & 0x1)) << 31;
  FalconWrite32 (XUSB_CSB_MEMPOOL_APMAP_0, Value);

  /* invalidate L2IMEM entries */
  ACTION = 0x40 /* L2IMEM_INVALIDATE_ALL */;
  DEBUG ((EFI_D_VERBOSE, "%a: ACTION %x\r\n",__FUNCTION__, ACTION));
  DEST_INDEX = 0;
  DEBUG ((EFI_D_VERBOSE, "%a: DEST_INDEX %x\r\n",__FUNCTION__, DEST_INDEX));
  Value = 0;
  Value |= (ACTION & 0xff) << 24;
  Value |= (DEST_INDEX & 0x3ff) << 8;
  FalconWrite32 (XUSB_CSB_MEMPOOL_L2IMEMOP_TRIG_0, Value);

  /* fetch complete bootstrap into L2IMEM */
  SRC_OFFSET = (FirmwareCfg->boot_codetag + (IMEM_BLOCK_SIZE - 1)) / IMEM_BLOCK_SIZE;
  DEBUG ((EFI_D_VERBOSE, "%a: SRC_OFFSET %x\r\n",__FUNCTION__, SRC_OFFSET));
  SRC_COUNT = (FirmwareCfg->boot_codesize + (IMEM_BLOCK_SIZE - 1)) / IMEM_BLOCK_SIZE;
  DEBUG ((EFI_D_VERBOSE, "%a: SRC_COUNT %x\r\n",__FUNCTION__, SRC_COUNT));
  Value = 0;
  Value |= ((SRC_OFFSET & 0xfff)) << 8;
  Value |= ((SRC_COUNT & 0xff)) << 24;
  FalconWrite32 (XUSB_CSB_MEMPOOL_L2IMEMOP_SIZE_0, Value);
  ACTION = 0x11 /* L2IMEM_LOAD_LOCKED_RESULT */;
  DEBUG ((EFI_D_VERBOSE, "%a: ACTION %x\r\n",__FUNCTION__, ACTION));
  DEST_INDEX = 0;
  DEBUG ((EFI_D_VERBOSE, "%a: DEST_INDEX %x\r\n",__FUNCTION__, DEST_INDEX));
  Value = 0;
  Value |= (ACTION & 0xff) << 24;
  Value |= (DEST_INDEX & 0x3ff) << 8;
  FalconWrite32 (XUSB_CSB_MEMPOOL_L2IMEMOP_TRIG_0, Value);

  /* reserve required IMEM blocks by writing to IMEMFILLCTL register */
  NBLOCKS = SRC_COUNT;
  DEBUG ((EFI_D_VERBOSE, "%a: NBLOCKS %x\r\n",__FUNCTION__, NBLOCKS));
  Value = 0;
  Value |= ((NBLOCKS & 0xff)) << 0;
  FalconWrite32 (FALCON_IMFILLCTL_0, Value);

  /* enable auto-fill mode for bootstrap code range */
  TAG_LO = (SRC_OFFSET & 0xffff);
  DEBUG ((EFI_D_VERBOSE, "%a: TAG_LO %x\r\n",__FUNCTION__, TAG_LO));
  TAG_HI = ((SRC_OFFSET + SRC_COUNT) & 0xffff);
  DEBUG ((EFI_D_VERBOSE, "%a: TAG_HI %x\r\n",__FUNCTION__, TAG_HI));
  Value = 0;
  Value |= ((TAG_HI & 0xffff)) << 16;
  Value |= ((TAG_LO & 0xffff)) << 0;
  FalconWrite32 (FALCON_IMFILLRNG1_0, Value);

  /* reset DMACTL */
  Value = 0;
  FalconWrite32 (FALCON_DMACTL_0, Value);

  /* wait for RESULT_VLD to get set */
  for (i = 0; i < 100; i++)
  {
    Value = FalconRead32 (XUSB_CSB_MEMPOOL_L2IMEMOP_RESULT_0);
    DEBUG ((EFI_D_VERBOSE, "%a: XUSB_CSB_MEMPOOL_L2IMEMOP_RESULT_0 = %x\r\n",__FUNCTION__, Value));
    if (Value & L2IMEMOP_RESULT_VLD)
      break;
    gBS->Stall(100);
  }

  /* program BOOTVEC with Falcon boot code location in IMEM */
  VEC = FirmwareCfg->boot_codetag;
  DEBUG ((EFI_D_VERBOSE, "%a: VEC %x\r\n",__FUNCTION__, VEC));
  Value = 0;
  Value |= ((VEC & 0xffffffff)) << 0;
  FalconWrite32 (FALCON_BOOTVEC_0, Value);

  /* dump DMEM */
  FalconDumpDMEM ();

  /* start Falcon by writing STARTCPU field in CPUCTL register */
  Value = 0;
  Value |= 1 << 1 /* STARTCPU */;
  FalconWrite32 (FALCON_CPUCTL_0, Value);

  for (i = 0; i < 10; i++)
  {
    Value = FalconRead32 (FALCON_CPUCTL_0);
    DEBUG ((EFI_D_VERBOSE, "%a: FALCON_CPUCTL_0 = %x\r\n",__FUNCTION__, Value));
    if (Value & 0x20 /* stopped */)
    {
      break;
    }
  }
  DEBUG ((EFI_D_VERBOSE, "%a: FALCON_CPUCTL_0 = %x\r\n",__FUNCTION__, Value));

  /* dump DMEM */
  FalconDumpDMEM ();

  /* */
  return Status;

}

