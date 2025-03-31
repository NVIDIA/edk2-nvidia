/** @file

  XUDC Driver

  SPDX-FileCopyrightText: Copyright (c) 2021-2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/HobLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/PowerGateNodeProtocol.h>
#include <Protocol/ArmScmiClock2Protocol.h>
#include <Protocol/UsbPadCtl.h>
#include <Protocol/XudcController.h>
#include "XusbDevControllerPriv.h"
#include <Library/DmaLib.h>
#include "XusbDevControllerDesc.h"
#include <string.h>

#define XUSB_INTERRUPT_POLL_PERIOD  10000
#define NUM_TRB_EVENT_RING          32U
#define SETUP_DATA_SIZE             8
#define NUM_TRB_TRANSFER_RING       16U
#define NUM_EP_CONTEXT              4
#define EVENT_RING_SIZE             (NUM_TRB_EVENT_RING * sizeof(EVENT_TRB_STRUCT))
#define TX_RING_EP0_SIZE            (NUM_TRB_TRANSFER_RING * sizeof(DATA_TRB_STRUCT))
#define TX_RING_EP1_OUT_SIZE        (NUM_TRB_TRANSFER_RING * sizeof(DATA_TRB_STRUCT))
#define TX_RING_EP1_IN_SIZE         (NUM_TRB_TRANSFER_RING * sizeof(DATA_TRB_STRUCT))
#define EP_CONTEXT_SIZE             (NUM_EP_CONTEXT*sizeof(EP_CONTEXT))
#define SETUP_DATA_BUFFER_SIZE      (0x200)

#define U64_TO_U32_LO(addr)  ((UINT32)(((UINT64)(addr)) & 0xFFFFFFFF))
#define U64_TO_U32_HI(addr)  ((UINT32)(((UINT64)(addr)) >> 32))
#define U64_FROM_U32(addrlo, addrhi) \
    (((UINT64)(addrlo)) | ((UINT64)(addrhi) << 32))

#define TEGRABL_UNUSED(var)  ((VOID )var)

#define TDATA_SIZE  6U

#ifndef MIN
#define MIN(X, Y)  (((X) < (Y)) ? (X) : (Y))
#endif

#define MAX_TFR_LENGTH  (64U * 1024U)

#define  XUSB_DEV_CFG_4_BASE_ADDR_SHIFT  15U
#define  XUSB_DEV_CFG_4_BASE_ADDR_MASK   0x1FFFFU

typedef UINT32  string_descriptor_index_t;
typedef UINT64  dma_addr_t;

typedef struct {
  EFI_PHYSICAL_ADDRESS              XudcBaseAddress;
  EFI_PHYSICAL_ADDRESS              FpciBaseAddress;
  EFI_HANDLE                        ControllerHandle;
  NVIDIA_XUDCCONTROLLER_PROTOCOL    XudcControllerProtocol;
  NVIDIA_USBPADCTL_PROTOCOL         *mUsbPadCtlProtocol;
  EFI_EVENT                         ExitBootServicesEvent;
  EFI_EVENT                         TimerEvent;
  EVENT_TRB_STRUCT                  *pEventRing;
  DATA_TRB_STRUCT                   *pTxRingEP0;
  DATA_TRB_STRUCT                   *pTxRingEP1Out;
  DATA_TRB_STRUCT                   *pTxRingEP1In;
  UINT8                             *pSetupBuffer;
  EP_CONTEXT                        *pEPContext;
  UINT8                             *usb_setup_data;
  UINT8                             *tdata;
  XUDC_RX_CALLBACK                  DataReceivedCallback;
  XUDC_TX_CALLBACK                  DataSentCallback;
  VOID                              *DataPacket;
  UINTN                             TotalRxLength;
  UINTN                             CurrentRxLength;
  UINT32                            PgState;
} XUDC_CONTROLLER_PRIVATE_DATA;

XUDC_CONTROLLER_PRIVATE_DATA  *mPrivate;

XUSB_DEVICE_CONTEXT                s_xusb_device_context;
static struct tegrabl_usbf_config  *g_usbconfig;

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "nvidia,tegra264-xudc", NULL                                  },
  { "nvidia,tegra23*-xudc", &gNVIDIANonDiscoverableXudcDeviceGuid },
  { NULL,                   NULL                                  }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName                      = L"NVIDIA Xudc controller driver",
  .SkipEdkiiNondiscoverableInstall = TRUE,
  .AutoEnableClocks                = TRUE,
  .AutoDeassertReset               = TRUE,
  .AutoDeassertPg                  = FALSE,
  .ThreadedDeviceStart             = FALSE,
};

static EFI_STATUS
XudcInitep (
  UINT8    ep_index,
  BOOLEAN  reinit
  );

static EFI_STATUS
XudcDisableEp (
  UINT8  ep_index
  );

static EFI_STATUS
XudcPollField (
  UINT32  reg_addr,
  UINT32  mask,
  UINT32  expected_value,
  UINT32  timeout
  );

static EFI_STATUS
XudcGetDesc (
  UINT8   *psetup_data,
  UINT16  *tx_length,
  UINT8   *ptr_setup_buffer
  );

static VOID
XudcEpGetStatus (
  UINT16  ep_index,
  UINT16  *tx_length,
  UINT8   *ptr_setup_buffer
  );

static VOID
XudcClockPadProgramming (
  void
  );

static EFI_STATUS
XudcEnumerate (
  VOID
  );

static EFI_STATUS
XudcIssueNormalTrb (
  UINT8   *buffer,
  UINT32  bytes,
  UINT32  direction
  );

VOID
EFIAPI
OnExitBootServices (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  XUDC_CONTROLLER_PRIVATE_DATA     *Private;
  EFI_STATUS                       Status;
  NVIDIA_POWER_GATE_NODE_PROTOCOL  *PgProtocol;
  UINT32                           Index;
  UINT32                           PgState;
  VOID                             *AcpiBase;
  VOID                             *Hob;
  TEGRA_PLATFORM_RESOURCE_INFO     *PlatformResourceInfo;

  Private = (XUDC_CONTROLLER_PRIVATE_DATA *)Context;

  // Skip in ACPI case.
  Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
  if (!EFI_ERROR (Status)) {
    return;
  }

  // Check PG state before stopping the USB device mode controller.
  PgProtocol = NULL;
  PgState    = CmdPgStateOn;
  Status     = gBS->HandleProtocol (Private->ControllerHandle, &gNVIDIAPowerGateNodeProtocolGuid, (VOID **)&PgProtocol);
  if (EFI_ERROR (Status)) {
    return;
  }

  for (Index = 0; Index < PgProtocol->NumberOfPowerGates; Index++) {
    Status = PgProtocol->GetState (PgProtocol, PgProtocol->PowerGateId[Index], &PgState);
    if (EFI_ERROR (Status)) {
      return;
    }

    if (PgState != CmdPgStateOn) {
      break;
    }
  }

  Hob = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    PlatformResourceInfo = (TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob);
  } else {
    DEBUG ((DEBUG_ERROR, "Failed to get PlatformResourceInfo\n"));
    return;
  }

  // Stop USB device mode controller.
  // For T23x with no PG, PgState should keep the value as ON.
  if ((Private->XudcBaseAddress != 0) &&
      (PgState == CmdPgStateOn))
  {
    MmioBitFieldWrite32 (
      Private->XudcBaseAddress + XUSB_DEV_XHCI_CTRL_0,
      XUSB_DEV_XHCI_CTRL_0_RUN_SHIFT,
      XUSB_DEV_XHCI_CTRL_0_RUN_SHIFT,
      0
      );
  }

  for (Index = 0; Index < PgProtocol->NumberOfPowerGates; Index++) {
    Status = PgProtocol->GetState (PgProtocol, PgProtocol->PowerGateId[Index], &PgState);
    if (EFI_ERROR (Status)) {
      return;
    }

    if (PgState == CmdPgStateOn) {
      Status = PgProtocol->Assert (PgProtocol, PgProtocol->PowerGateId[Index]);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Xudc Assert pg fail: %d\r\n", PgProtocol->PowerGateId[Index]));
        return;
      }
    }
  }
}

static EFI_STATUS
XudcQueueTrb (
  UINT8              ep_index,
  NORMAL_TRB_STRUCT  *p_trb,
  UINT32             ring_doorbell
  )
{
  LINK_TRB_STRUCT      *p_link_trb;
  DATA_TRB_STRUCT      *p_next_trb;
  XUSB_DEVICE_CONTEXT  *p_xusb_dev_context = &s_xusb_device_context;
  UINT32               reg_data;
  EFI_STATUS           e       =  EFI_SUCCESS;
  dma_addr_t           dma_buf = 0;

  TEGRABL_UNUSED (dma_buf);

  /* If Control EP */
  if (ep_index == EP0_IN) {
    memcpy (
      (VOID *)p_xusb_dev_context->cntrl_epenqueue_ptr,
      (VOID *)p_trb,
      sizeof (NORMAL_TRB_STRUCT)
      );

    p_next_trb = (DATA_TRB_STRUCT *)p_xusb_dev_context->cntrl_epenqueue_ptr;
    p_next_trb++;

    /* Handle Link TRB */
    if (p_next_trb->trb_type == LINK_TRB) {
      p_link_trb     = (LINK_TRB_STRUCT *)p_next_trb;
      p_link_trb->c  = (UINT8)p_xusb_dev_context->cntrl_pcs;
      p_link_trb->tc = 1;

      /* next trb after link is always index 0 */
      p_next_trb = mPrivate->pTxRingEP0;

      /* Toggle cycle bit */
      p_xusb_dev_context->cntrl_pcs ^= 1U;
    }

    p_xusb_dev_context->cntrl_epenqueue_ptr = (UINT64)p_next_trb;
  }
  /* Bulk Endpoint */
  else if (ep_index == EP1_OUT) {
    memcpy (
      (VOID *)p_xusb_dev_context->bulkout_epenqueue_ptr,
      (VOID *)p_trb,
      sizeof (NORMAL_TRB_STRUCT)
      );
    p_next_trb = (DATA_TRB_STRUCT *)p_xusb_dev_context->bulkout_epenqueue_ptr;
    p_next_trb++;

    /* Handle Link TRB */
    if (p_next_trb->trb_type == LINK_TRB) {
      p_link_trb                       = (LINK_TRB_STRUCT *)p_next_trb;
      p_link_trb->c                    = (UINT8)p_xusb_dev_context->bulkout_pcs;
      p_link_trb->tc                   = 1U;
      p_next_trb                       = mPrivate->pTxRingEP1Out;
      p_xusb_dev_context->bulkout_pcs ^= 1U;
    }

    p_xusb_dev_context->bulkout_epenqueue_ptr = (UINT64)p_next_trb;
  }
  /* Bulk Endpoint */
  else if (ep_index == EP1_IN) {
    memcpy (
      (VOID *)p_xusb_dev_context->bulkin_epenqueue_ptr,
      (VOID *)p_trb,
      sizeof (NORMAL_TRB_STRUCT)
      );
    p_next_trb = (DATA_TRB_STRUCT *)p_xusb_dev_context->bulkin_epenqueue_ptr;
    p_next_trb++;

    /* Handle Link TRB */
    if (p_next_trb->trb_type == LINK_TRB) {
      p_link_trb                      = (LINK_TRB_STRUCT *)p_next_trb;
      p_link_trb->c                   = (UINT8)p_xusb_dev_context->bulkin_pcs;
      p_link_trb->tc                  = 1;
      p_next_trb                      = mPrivate->pTxRingEP1In;
      p_xusb_dev_context->bulkin_pcs ^= 1U;
    }

    p_xusb_dev_context->bulkin_epenqueue_ptr = (UINT64)p_next_trb;
  } else {
    e = EFI_INVALID_PARAMETER;
  }

  /* Ring Doorbell */
  if (ring_doorbell != 0U) {
    reg_data = ((((UINT32)(ep_index)) & (((UINT32)(XUSB_DEV_XHCI_DB_0_TARGET_FIELD)) >>
                                         ((UINT32)(XUSB_DEV_XHCI_DB_0_TARGET_SHIFT)))) <<
                ((UINT32)(XUSB_DEV_XHCI_DB_0_TARGET_SHIFT)));
    if (ep_index ==  EP0_IN) {
      reg_data = NV_FLD_SET_DRF_NUM (
                   XUSB_DEV_XHCI,
                   DB,
                   STREAMID,
                   p_xusb_dev_context->cntrl_seq_num,
                   reg_data
                   );
    }

    MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_DB_0, reg_data);
  }

  return e;
}

static VOID
XudcCreateStatusTrb (
  STATUS_TRB_STRUCT  *p_status_trb,
  UINT32             dir
  )
{
  XUSB_DEVICE_CONTEXT  *p_xusb_dev_context = &s_xusb_device_context;

  /* Event gen on Completion. */
  p_status_trb->c        = (UINT8)p_xusb_dev_context->cntrl_pcs;
  p_status_trb->ioc      = 1;
  p_status_trb->trb_type = STATUS_STAGE_TRB;
  p_status_trb->dir      = (UINT8)dir;
}

static EFI_STATUS
XudcIssueStatusTrb (
  UINT32  direction
  )
{
  EFI_STATUS           e = EFI_SUCCESS;
  STATUS_TRB_STRUCT    strb;
  XUSB_DEVICE_CONTEXT  *p_xusb_dev_context = &s_xusb_device_context;

  if ((p_xusb_dev_context->cntrl_epenqueue_ptr !=
       p_xusb_dev_context->cntrl_epdequeue_ptr) && (direction == DIR_IN))
  {
    return e;
  }

  memset ((VOID *)&strb, 0, sizeof (STATUS_TRB_STRUCT));
  XudcCreateStatusTrb (&strb, direction);

  /* Note EP0_IN is bi-directional. */
  e = XudcQueueTrb (EP0_IN, (NORMAL_TRB_STRUCT *)&strb, 1);

  p_xusb_dev_context->wait_for_eventt = STATUS_STAGE_TRB;
  return e;
}

static EFI_STATUS
XudcSetConfiguration (
  UINT8  *psetup_data
  )
{
  UINT16               wvalue;
  UINT32               reg_data;
  XUSB_DEVICE_CONTEXT  *p_xusb_dev_context = &s_xusb_device_context;
  EFI_STATUS           e                   = EFI_SUCCESS;

  /* Last stage of enumeration. */
  wvalue = psetup_data[USB_SETUP_VALUE] + ((UINT16)psetup_data[USB_SETUP_VALUE+1] << 8);

  /* If we get a set config 0, then disable endpoints and remain in addressed
   * state.
   * If we had already set a config before this request, then do the same but
   * also enable bulk endpoints after and set run bit.
   */

  if ((p_xusb_dev_context->config_num != 0U) || (wvalue == 0U)) {
    e = XudcDisableEp (EP1_IN);
    if (e != EFI_SUCCESS) {
      return e;
    }

    e = XudcDisableEp (EP1_OUT);
    if (e != EFI_SUCCESS) {
      return e;
    }

    reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_CTRL_0);
    reg_data = NV_FLD_SET_DRF_DEF (
                 XUSB_DEV_XHCI,
                 CTRL,
                 RUN,
                 STOP,
                 reg_data
                 );
    reg_data = MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_CTRL_0, reg_data);
  }

  if (wvalue != 0U) {
    e = XudcInitep (EP1_OUT, FALSE);

    if (e != EFI_SUCCESS) {
      return e;
    }

    e = XudcInitep (EP1_IN, FALSE);
    if (e != EFI_SUCCESS) {
      return e;
    }

    /* Now set run */
    reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_CTRL_0);
    reg_data = NV_FLD_SET_DRF_DEF (
                 XUSB_DEV_XHCI,
                 CTRL,
                 RUN,
                 RUN,
                 reg_data
                 );
    reg_data = MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_CTRL_0, reg_data);
  }

  /* Also clear Run Change bit just in case to enable Doorbell register. */
  reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_ST_0);
  reg_data = NV_FLD_SET_DRF_DEF (
               XUSB_DEV_XHCI,
               ST,
               RC,
               CLEAR,
               reg_data
               );
  reg_data = MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_ST_0, reg_data);

  /* Send status */
  e = XudcIssueStatusTrb (DIR_IN);
  if (e != EFI_SUCCESS) {
    return e;
  }

  p_xusb_dev_context->config_num = wvalue;

  /* Change device state only for non-zero configuration number
   * Otherwise device remains in addressed state.
   */
  if (wvalue != 0U) {
    p_xusb_dev_context->device_state = CONFIGURED_STATUS_PENDING;
  } else {
    p_xusb_dev_context->device_state = ADDRESSED_STATUS_PENDING;
  }

  return EFI_SUCCESS;
}

static EFI_STATUS
XudcSetInterface (
  UINT8  *psetup_data
  )
{
  UINT16               wvalue;
  XUSB_DEVICE_CONTEXT  *p_xusb_dev_context = &s_xusb_device_context;
  EFI_STATUS           error               = EFI_SUCCESS;

  wvalue = psetup_data[USB_SETUP_VALUE] +
           ((UINT16)psetup_data[USB_SETUP_VALUE + 1] << 8);

  p_xusb_dev_context->interface_num = wvalue;

  /* Send status */
  error = XudcIssueStatusTrb (DIR_IN);

  return error;
}

static EFI_STATUS
XudcSetAddress (
  UINT8  *psetup_data
  )
{
  UINT8       dev_addr;
  UINT32      reg_data;
  EP_CONTEXT  *ep_info;
  EFI_STATUS  e = EFI_SUCCESS;

  XUSB_DEVICE_CONTEXT  *p_xusb_dev_context = &s_xusb_device_context;

  dev_addr = psetup_data[USB_SETUP_VALUE];
  ep_info  = &mPrivate->pEPContext[EP0_IN];
  reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_CTRL_0);
  reg_data = NV_FLD_SET_DRF_NUM (
               XUSB_DEV_XHCI,
               CTRL,
               DEVADR,
               dev_addr,
               reg_data
               );
  MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_CTRL_0, reg_data);

  ep_info->device_addr = dev_addr;

  /* Send status */
  e = XudcIssueStatusTrb (DIR_IN);
  if (e != EFI_SUCCESS) {
    return e;
  }

  p_xusb_dev_context->device_state = ADDRESSED_STATUS_PENDING;

  return e;
}

static EFI_STATUS
XudcStallEp (
  UINT8    ep_index,
  BOOLEAN  stall
  )
{
  UINT32      reg_data, expected_value, int_pending_mask;
  EFI_STATUS  e;

  /* EIndex received from Host is of the form:
   * Byte 1(Bit7 is dir): Byte 0
   * 8: In : EpNum
   * 0: Out: EpNum
   */

  DEBUG ((EFI_D_ERROR, "%a: Stalling Endpoint Number %u\r\n", __FUNCTION__, ep_index));
  reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_HALT_0);
  if (stall) {
    reg_data |= (1UL << ep_index);
  } else {
    reg_data &= ~(1UL << ep_index);
  }

  MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_HALT_0, reg_data);

  /* Poll for state change */
  expected_value   = 1UL << ep_index;
  int_pending_mask = 1UL << ep_index;

  /* Poll for interrupt pending bit. */
  e = XudcPollField (
        mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_STCHG_0,
        int_pending_mask,
        expected_value,
        1000
        );
  if (e != EFI_SUCCESS) {
    e = EFI_TIMEOUT;
    return e;
  }

  MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_STCHG_0, (1UL << ep_index));
  return EFI_SUCCESS;
}

static EFI_STATUS
XudcGetDesc (
  UINT8   *psetup_data,
  UINT16  *tx_length,
  UINT8   *ptr_setup_buffer
  )
{
  UINT8                desc_type = 0;
  UINT8                desc_index = 0;
  UINT16               wlength, desc_length;
  XUSB_DEVICE_CONTEXT  *p_xusb_dev_context = &s_xusb_device_context;
  EFI_STATUS           e                   = EFI_SUCCESS;
  UINT8                *desc               = NULL;

  desc_type = psetup_data[USB_SETUP_DESCRIPTOR];
  wlength   = *(UINT16 *)&(psetup_data[USB_SETUP_LENGTH]);

  switch (desc_type) {
    case USB_DT_DEVICE:
      if (p_xusb_dev_context->port_speed == XUSB_SUPER_SPEED) {
        desc        = (UINT8 *)g_usbconfig->ss_device.desc;
        desc_length = (UINT16)g_usbconfig->ss_device.len;
      } else {
        desc        = (UINT8 *)g_usbconfig->hs_device.desc;
        desc_length = (UINT16)g_usbconfig->hs_device.len;
      }

      *tx_length = MIN (wlength, desc_length);
      memcpy ((VOID *)mPrivate->pSetupBuffer, (VOID *)desc, *tx_length);
      break;

    case USB_DT_CONFIG:
      if (p_xusb_dev_context->port_speed == XUSB_SUPER_SPEED) {
        desc       = (UINT8 *)g_usbconfig->ss_config.desc;
        *tx_length = MIN (wlength, g_usbconfig->ss_config.len);
      } else {
        desc       = (UINT8 *)g_usbconfig->hs_config.desc;
        *tx_length = MIN (wlength, g_usbconfig->hs_config.len);
      }

      if (p_xusb_dev_context->port_speed == XUSB_FULL_SPEED) {
        /* Apply full speed packet size */
        /* EP1_IN */
        desc[22] = 64;
        desc[23] = 0;

        /* EP1_OUT */
        desc[29] = 64;
        desc[30] = 0;
      }

      memcpy ((VOID *)mPrivate->pSetupBuffer, (VOID *)desc, *tx_length);
      break;

    case USB_DT_STRING:
      desc_index = psetup_data[USB_SETUP_VALUE];

      switch (desc_index) {
        case USB_MANF_ID:
          DEBUG ((EFI_D_ERROR, "%a: Get desc. Manf ID\r\n", __FUNCTION__));
          *tx_length = MIN (wlength, (UINT16)sizeof (s_usb_manufacturer_id));
          memcpy ((VOID *)mPrivate->pSetupBuffer, (VOID *)&s_usb_manufacturer_id[0], *tx_length);
          break;

        case USB_PROD_ID:
          DEBUG ((EFI_D_ERROR, "%a: Get desc. Prod ID\r\n", __FUNCTION__));
          desc       = (UINT8 *)g_usbconfig->product.desc;
          *tx_length = MIN (wlength, g_usbconfig->product.len);
          memcpy ((VOID *)mPrivate->pSetupBuffer, (VOID *)desc, *tx_length);
          break;

        case USB_SERIAL_ID:
          DEBUG ((EFI_D_ERROR, "%a: Get desc. Serial ID\r\n", __FUNCTION__));
          desc       = (UINT8 *)g_usbconfig->serialno.desc;
          *tx_length = MIN (wlength, g_usbconfig->serialno.len);
          memcpy ((VOID *)mPrivate->pSetupBuffer, (VOID *)desc, *tx_length);
          break;

        case USB_LANGUAGE_ID:
          DEBUG ((EFI_D_ERROR, "%a: Get desc. Lang ID\r\n", __FUNCTION__));
          *tx_length = MIN (wlength, (UINT16)sizeof (s_usb_language_id));
          memcpy ((VOID *)mPrivate->pSetupBuffer, (VOID *)&s_usb_language_id[0], *tx_length);
          break;

        default:
          DEBUG ((EFI_D_ERROR, "%a: ERR_NOT_SUPPORTED desc_index %u\r\n", __FUNCTION__, desc_index));
          break;
      }

      break;

    case USB_DT_DEVICE_QUALIFIER:
      DEBUG ((EFI_D_ERROR, "%a: Get desc. Dev qualifier\r\n", __FUNCTION__));
      *tx_length = MIN (wlength, (UINT16)sizeof (s_usb_device_qualifier));
      memcpy ((VOID *)mPrivate->pSetupBuffer, (VOID *)&s_usb_device_qualifier[0], *tx_length);
      break;

    case USB_DT_OTHER_SPEED_CONFIG:
      if (p_xusb_dev_context->port_speed == XUSB_HIGH_SPEED) {
        /* Full speed packet size as other speed. */
        /* EP1_IN */
        s_other_speed_config_desc[22] = 64;
        s_other_speed_config_desc[23] = 0;
        /* EP1_OUT */
        s_other_speed_config_desc[29] = 64;
        s_other_speed_config_desc[30] = 0;
      } else {
        /* High speed packet size as other speed. */
        /* EP1_IN */
        s_other_speed_config_desc[22] = 0;
        s_other_speed_config_desc[23] = 2;
        /* EP1_OUT */
        s_other_speed_config_desc[29] = 0;
        s_other_speed_config_desc[30] = 2;
      }

      *tx_length = MIN (wlength, (UINT16)sizeof (s_other_speed_config_desc));
      memcpy ((VOID *)mPrivate->pSetupBuffer, (VOID *)&s_other_speed_config_desc[0], *tx_length);
      break;

    case USB_DT_BOS:
      DEBUG ((EFI_D_ERROR, "%a: Get BOS\r\n", __FUNCTION__));
      *tx_length = MIN (wlength, (UINT16)sizeof (s_bos_descriptor));
      memcpy ((VOID *)mPrivate->pSetupBuffer, (VOID *)&s_bos_descriptor[0], *tx_length);
      break;

    default:
      DEBUG ((EFI_D_ERROR, "%a: ERR_NOT_SUPPORTED desc_type %u\r\n", __FUNCTION__, desc_type));
      /* stall if any Un supported request comes */
      e = XudcStallEp (EP0_IN, TRUE);
      break;
  }

  return e;
}

static VOID
XudcEpGetStatus (
  UINT16  ep_index,
  UINT16  *tx_length,
  UINT8   *ptr_setup_buffer
  )
{
  UINT32  ep_status;
  UINT8   endpoint_status[2] = { 0, 0 };

  /* ep_index received from Host is of the form:
   * Byte 1(Bit7 is dir): Byte 0
   * 8: In : EpNum
   * 0: Out: EpNum
   */

  DEBUG ((EFI_D_ERROR, "%a: Ep num = %u\r\n", __FUNCTION__, ep_index));
  ep_status = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_HALT_0) >> ep_index;

  if (ep_status == 1U) {
    endpoint_status[0] = 1;
  } else {
    endpoint_status[0] = 0;
  }

  *tx_length = (UINT16)sizeof (endpoint_status);
  memcpy ((VOID *)ptr_setup_buffer, (VOID *)&endpoint_status[0], sizeof (endpoint_status));
}

static VOID
XudcCreateDataTrb (
  DATA_TRB_STRUCT  *p_data_trb,
  dma_addr_t       buffer,
  UINT32           bytes,
  UINT32           dir
  )
{
  XUSB_DEVICE_CONTEXT  *p_xusb_dev_context = &s_xusb_device_context;

  p_data_trb->databufptr_lo = U64_TO_U32_LO (buffer);
  p_data_trb->databufptr_hi = U64_TO_U32_HI (buffer);
  p_data_trb->trb_tx_len    = bytes;

  /* BL will always queue only 1 TRB at a time. */
  p_data_trb->tdsize = 0;
  p_data_trb->c      = (UINT8)p_xusb_dev_context->cntrl_pcs;
  p_data_trb->ent    = 0;

  /* Make sure to interrupt on short packet i.e generate event. */
  p_data_trb->isp = 1;

  /* and on Completion. */
  p_data_trb->ioc = 1;

  p_data_trb->trb_type = DATA_STAGE_TRB;
  p_data_trb->dir      = (UINT8)dir;
}

static EFI_STATUS
XudcIssueDataTrb (
  dma_addr_t  buffer,
  UINT32      bytes,
  UINT32      direction
  )
{
  DATA_TRB_STRUCT      dtrb;
  EFI_STATUS           e;
  XUSB_DEVICE_CONTEXT  *p_xusb_dev_context = &s_xusb_device_context;

  /* Need to check if empty other wise don't issue
   * Will result in Seq Num Error
   */
  if (p_xusb_dev_context->cntrl_epenqueue_ptr !=
      p_xusb_dev_context->cntrl_epdequeue_ptr)
  {
    return EFI_SUCCESS;
  }

  memset ((VOID *)&dtrb, 0, sizeof (DATA_TRB_STRUCT));
  XudcCreateDataTrb (&dtrb, buffer, bytes, direction);

  /* Note EP0_IN is bi-directional. */
  e = XudcQueueTrb (EP0_IN, (NORMAL_TRB_STRUCT *)&dtrb, 1);
  if (e != EFI_SUCCESS) {
    return e;
  }

  p_xusb_dev_context->wait_for_eventt = DATA_STAGE_TRB;
  return e;
}

static EFI_STATUS
XudcHandleSetupPkt (
  UINT8  *psetup_data
  )
{
  UINT32               reg_data, int_pending_mask, expected_value;
  UINT16               wlength, tx_length = 0;
  EFI_STATUS           e;
  UINT8                ep_index;
  XUSB_DEVICE_CONTEXT  *p_xusb_dev_context = &s_xusb_device_context;
  dma_addr_t           dma_buf;
  UINT32               mask;
  UINTN                BufferSize;
  VOID                 *Mapping;

  wlength = *(UINT16 *)&psetup_data[USB_SETUP_LENGTH];
  const char  *config, *type;

  TEGRABL_UNUSED (config);
  TEGRABL_UNUSED (type);

  reg_data  = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_HALT_0);
  mask      = (1UL << EP0_IN);
  reg_data &= ~mask;
  MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_HALT_0, reg_data);

  expected_value   = 0;
  int_pending_mask = 1;

  /* Poll for interrupt pending bit. */
  e = XudcPollField (
        (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_HALT_0),
        int_pending_mask,
        expected_value,
        1000
        );

  if (e != EFI_SUCCESS) {
    e = EFI_TIMEOUT;
    return e;
  }

  switch (psetup_data[USB_SETUP_REQUEST_TYPE]) {
    case HOST2DEV_DEVICE:
      config = "device descriptor";
      type   = "H2D";

      /* Process the Host -> device Device descriptor */
      switch (psetup_data[USB_SETUP_REQUEST]) {
        case SET_CONFIGURATION:
          DEBUG ((EFI_D_ERROR, "%a: SET_CONFIGURATION\r\n", __FUNCTION__));
          e = XudcSetConfiguration (psetup_data);
          if (e != EFI_SUCCESS) {
            goto fail;
          }

          break;

        case SET_ADDRESS:
          DEBUG ((EFI_D_ERROR, "%a: SET_ADDRESS\r\n", __FUNCTION__));
          e = XudcSetAddress (psetup_data);
          if (e != EFI_SUCCESS) {
            DEBUG ((EFI_D_ERROR, "%a: SET_ADDRESS error\r\n", __FUNCTION__));
            goto fail;
          }

          break;

        case SET_ISOCH_DELAY:
          DEBUG ((EFI_D_ERROR, "%a: SET_ISOCH_DELAY\r\n", __FUNCTION__));
          /* Read timing values and store them */
          /* Send status */
          e = XudcIssueStatusTrb (DIR_IN);
          if (e != EFI_SUCCESS) {
            DEBUG ((EFI_D_ERROR, "%a: SET_ISOCH_DELAY error\r\n", __FUNCTION__));
            goto fail;
          }

          break;

        case SET_SEL:
          DEBUG ((EFI_D_ERROR, "%a: SET_SEL\r\n", __FUNCTION__));
          BufferSize = TDATA_SIZE;

          e = DmaMap (
                MapOperationBusMasterCommonBuffer,
                (VOID *)mPrivate->tdata,
                &BufferSize,
                &dma_buf,
                &Mapping
                );
          if (e != EFI_SUCCESS) {
            goto fail;
          }

          /* Data stage for receiving 6 bytes */
          e = XudcIssueDataTrb (dma_buf, TDATA_SIZE, DIR_OUT);
          if (e != EFI_SUCCESS) {
            goto fail;
          }

          e = DmaUnmap (Mapping);
          if (e != EFI_SUCCESS) {
            goto fail;
          }

          /* Send status */
          e = XudcIssueStatusTrb (DIR_IN);
          if (e != EFI_SUCCESS) {
            goto fail;
          }

          break;

        case SET_FEATURE:
          DEBUG ((EFI_D_ERROR, "%a: SET_FEATURE: value=%u\r\n", __FUNCTION__, psetup_data[USB_SETUP_VALUE]));

          reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTPM_0);

          switch (psetup_data[USB_SETUP_VALUE]) {
            case U1_ENABLE:
              reg_data |=  (1UL << 28);
              break;

            case U2_ENABLE:
              reg_data |=  (1UL << 29);
              break;

            default:
              /* Do nothing */
              break;
          }

          MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTPM_0, reg_data);

          /* Send status */
          e = XudcIssueStatusTrb (DIR_IN);
          if ( e != EFI_SUCCESS) {
            goto fail;
          }

          break;

        case CLEAR_FEATURE:
          DEBUG ((EFI_D_ERROR, "%a: CLEAR_FEATURE: value=%u\r\n", __FUNCTION__, psetup_data[USB_SETUP_VALUE]));

          reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTPM_0);

          switch (psetup_data[USB_SETUP_VALUE]) {
            case U1_ENABLE:
              reg_data &=  ~(1UL << 28);
              break;

            case U2_ENABLE:
              reg_data &=  ~(1UL << 29);
              break;

            default:
              /* Do nothing */
              break;
          }

          MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTPM_0, reg_data);

          /* Send status */
          e = XudcIssueStatusTrb (DIR_IN);
          if ( e != EFI_SUCCESS) {
            goto fail;
          }

          break;

        default:
          DEBUG ((EFI_D_ERROR, "%a: HOST2DEV_DEV: unhandled req=%u\r\n", __FUNCTION__, psetup_data[USB_SETUP_REQUEST]));
          break;
      }

      break;

    case HOST2DEV_INTERFACE:
      config = "device I/F";
      type   = "H2D";

      /* Start the endpoint for zero packet acknowledgment
       * Store the interface number.
       */
      DEBUG ((EFI_D_ERROR, "%a: SET INTERFACE\r\n", __FUNCTION__));
      e = XudcSetInterface (psetup_data);
      if (e != EFI_SUCCESS) {
        goto fail;
      }

      break;

    case DEV2HOST_DEVICE:
      config = "device descriptor";
      type   = "D2H";

      switch (psetup_data[USB_SETUP_REQUEST]) {
        case GET_STATUS:
          DEBUG ((EFI_D_ERROR, "%a: Get status\r\n", __FUNCTION__));
          tx_length = MIN (wlength, (UINT16)sizeof (s_usb_dev_status));
          memcpy ((VOID *)mPrivate->pSetupBuffer, (VOID *)&s_usb_dev_status[0], tx_length);
          break;

        case GET_CONFIGURATION:
          DEBUG ((EFI_D_ERROR, "%a: Get Config\r\n", __FUNCTION__));
          tx_length = MIN (wlength, (UINT16)sizeof (p_xusb_dev_context->config_num));
          memcpy ((VOID *)mPrivate->pSetupBuffer, &p_xusb_dev_context->config_num, tx_length);
          break;

        case GET_DESCRIPTOR:
          DEBUG ((EFI_D_ERROR, "%a: Get Desc\r\n", __FUNCTION__));
          e =  XudcGetDesc (psetup_data, &tx_length, mPrivate->pSetupBuffer);
          if (e != EFI_SUCCESS) {
            goto fail;
          }

          break;

        default:
          DEBUG ((EFI_D_ERROR, "%a: NOT SUPPORTED D2H_D request:%u\r\n", __FUNCTION__, psetup_data[USB_SETUP_REQUEST]));

          /* Stall if any Un supported request comes */
          e = XudcStallEp (EP0_IN, TRUE);
          if (e != EFI_SUCCESS) {
            goto fail;
          }

          break;
      }

      break;

    case DEV2HOST_INTERFACE:
      config = "device I/F";
      type   = "D2H";

      switch (psetup_data[USB_SETUP_REQUEST]) {
        case GET_STATUS:
          DEBUG ((EFI_D_ERROR, "%a: Get Status\r\n", __FUNCTION__));

          /* Just sending 0s. */
          /* GetStatus() Request to an Interface is always 0 */
          UINT8  interface_status[2] = { 0, 0 };

          tx_length = MIN (wlength, (UINT16)sizeof (interface_status));
          memcpy ((VOID *)mPrivate->pSetupBuffer, &interface_status[0], tx_length);
          break;

        case GET_INTERFACE:
          /* Just sending 0s. */
          DEBUG ((EFI_D_ERROR, "%a: Get Interface D2H_I/F\r\n", __FUNCTION__));
          tx_length = MIN (wlength, (UINT16)sizeof (p_xusb_dev_context->interface_num));

          memcpy (
            (VOID *)mPrivate->pSetupBuffer,
            &p_xusb_dev_context->interface_num,
            tx_length
            );
          break;

        default:
          /* Stall if any unsupported request comes */
          DEBUG ((EFI_D_ERROR, "%a: Not support. Stall EP\r\n", __FUNCTION__));
          e = XudcStallEp (EP0_IN, TRUE);
          if (e != EFI_SUCCESS) {
            goto fail;
          }

          break;
      }

      break;

    /* Stall here, as we don't support endpoint requests here */
    case DEV2HOST_ENDPOINT:
      config = "device endpoint";
      type   = "D2H";

      switch (psetup_data[USB_SETUP_REQUEST]) {
        case GET_STATUS:
          DEBUG ((EFI_D_ERROR, "%a: Get status D2H_Ep\r\n", __FUNCTION__));
          ep_index  = 2U * (psetup_data[USB_SETUP_INDEX] & 0xFU);
          ep_index += ((psetup_data[USB_SETUP_INDEX] & 0x80U) != 0U) ? 1U : 0U;
          XudcEpGetStatus (ep_index, &tx_length, mPrivate->pSetupBuffer);
          break;

        default:
          DEBUG ((EFI_D_ERROR, "%a: NOT SUPPORTED D2H_ep:%u\r\n", __FUNCTION__, psetup_data[USB_SETUP_REQUEST]));
          e = XudcStallEp (EP0_IN, TRUE);
          if (e != EFI_SUCCESS) {
            goto fail;
          }

          break;
      }

      break;

    case HOST2DEV_ENDPOINT:
      config = "device endpoint";
      type   = "H2D";

      switch (psetup_data[USB_SETUP_REQUEST]) {
        case SET_FEATURE:
          DEBUG ((EFI_D_ERROR, "%a: Set Feature H2D_Ep\r\n", __FUNCTION__));

          switch (psetup_data[USB_SETUP_VALUE]) {
            case ENDPOINT_HALT:
              ep_index  = 2U * (psetup_data[USB_SETUP_INDEX] & 0xFU);
              ep_index += ((psetup_data[USB_SETUP_INDEX] & 0x80U) != 0U) ? 1U : 0U;
              XudcStallEp (ep_index, TRUE);

              /* Send status */
              e = XudcIssueStatusTrb (DIR_IN);
              if (e != EFI_SUCCESS) {
                goto fail;
              }

              break;

            default:
              e = XudcStallEp (EP0_IN, TRUE);
              if (e != EFI_SUCCESS) {
                goto fail;
              }

              break;
          }

          break;

        case CLEAR_FEATURE:
          DEBUG ((EFI_D_ERROR, "%a: Clear Feature H2D_Ep\r\n", __FUNCTION__));

          switch (psetup_data[USB_SETUP_VALUE]) {
            case ENDPOINT_HALT:
              /* Get the EP status, to find wether Txfer is success or not */
              ep_index  = 2U * (psetup_data[USB_SETUP_INDEX] & 0xFU);
              ep_index += ((psetup_data[USB_SETUP_INDEX] & 0x80U) != 0U) ? 1U : 0U;
              XudcStallEp (ep_index, FALSE);

              /* Send status */
              e = XudcIssueStatusTrb (DIR_IN);
              if ( e != EFI_SUCCESS) {
                goto fail;
              }

              break;

            default:
              e = XudcStallEp (EP0_IN, TRUE);
              if (e != EFI_SUCCESS) {
                goto fail;
              }

              break;
          }

          break;

        default:
          DEBUG ((EFI_D_ERROR, "%a: NOT SUPPORTED H2D_Ep:%u\r\n", __FUNCTION__, psetup_data[USB_SETUP_REQUEST]));
          /* Stall if any unsupported request comes */
          e = XudcStallEp (EP0_IN, TRUE);
          if (e != EFI_SUCCESS) {
            goto fail;
          }

          break;
      }

      break;

    default:
      config = "unknown";
      type   = "unknown";
      DEBUG ((EFI_D_ERROR, "%a: NOT SUPPORTED type %u\r\n", __FUNCTION__, psetup_data[USB_SETUP_REQUEST_TYPE]));
      /* Stall if any Un supported request comes */
      e = XudcStallEp (EP0_IN, TRUE);
      if (e != EFI_SUCCESS) {
        goto fail;
      }

      break;
  }

  BufferSize = tx_length;

  e = DmaMap (
        MapOperationBusMasterCommonBuffer,
        (VOID *)mPrivate->pSetupBuffer,
        &BufferSize,
        &dma_buf,
        &Mapping
        );
  if (e != EFI_SUCCESS) {
    return e;
  }

  if (tx_length != 0U) {
    /* Compensate buffer for xusb device view of sysram */
    e = XudcIssueDataTrb (dma_buf, tx_length, DIR_IN);
    if (e != EFI_SUCCESS) {
      goto unmap;
    }
  }

unmap:
  e = DmaUnmap (Mapping);
  if (e != EFI_SUCCESS) {
    return e;
  }

fail:
  if (e != EFI_SUCCESS) {
    DEBUG ((
      EFI_D_ERROR,
      "%a, error config: %s type: %s\r\n",
      __FUNCTION__,
      config,
      type
      ));
  }

  return e;
}

static EFI_STATUS
XudcHandlePortStatus (
  VOID
  )
{
  EFI_STATUS           Status = EFI_SUCCESS;
  UINT32               port_speed, reg_halt, status_bits_mask = 0;
  UINT32               link_state, reg_data;
  XUSB_DEVICE_CONTEXT  *p_xusb_dev_context = &s_xusb_device_context;
  UINT32               port_status;

  /* Let's see why we got here. */
  port_status = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTSC_0);
  reg_halt    = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTHALT_0);

  status_bits_mask = NV_DRF_NUM (XUSB_DEV_XHCI, PORTSC, PRC, 1) |
                     NV_DRF_NUM (XUSB_DEV_XHCI, PORTSC, PLC, 1) |
                     NV_DRF_NUM (XUSB_DEV_XHCI, PORTSC, WRC, 1) |
                     NV_DRF_NUM (XUSB_DEV_XHCI, PORTSC, CSC, 1) |
                     NV_DRF_NUM (XUSB_DEV_XHCI, PORTSC, CEC, 1);

  /* Handle PORT RESET. PR indicates reset event received.
   * PR could get cleared (port reset complete) by the time we read it
   * so check on PRC.
   */
  if (NV_DRF_VAL (XUSB_DEV_XHCI, PORTSC, PR, port_status)  != 0UL) {
    /* This is probably a good time to stop the watchdog timer. */
    p_xusb_dev_context->device_state = RESET;
  }

  if (NV_DRF_VAL (XUSB_DEV_XHCI, PORTSC, PRC, port_status)  != 0UL) {
    /* Must clear PRC */
    port_status &= (~status_bits_mask);
    port_status  = NV_FLD_SET_DRF_NUM (
                     XUSB_DEV_XHCI,
                     PORTSC,
                     PRC,
                     1,
                     port_status
                     );
    MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTSC_0, port_status);
  }

  if (NV_DRF_VAL (XUSB_DEV_XHCI, PORTSC, WPR, port_status) != 0UL) {
    /* This is probably a good time to stop the watchdog timer. */
    reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTHALT_0);
    reg_data = NV_FLD_SET_DRF_NUM (
                 XUSB_DEV_XHCI,
                 PORTHALT,
                 HALT_LTSSM,
                 0,
                 reg_data
                 );
    MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTHALT_0, reg_data);
    p_xusb_dev_context->device_state = RESET;
  }

  if (NV_DRF_VAL (XUSB_DEV_XHCI, PORTSC, WRC, port_status) != 0UL) {
    port_status &= ~status_bits_mask;
    port_status  = NV_FLD_SET_DRF_NUM (
                     XUSB_DEV_XHCI,
                     PORTSC,
                     WRC,
                     1,
                     port_status
                     );
    MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTSC_0, port_status);
  }

  port_status = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTSC_0);

  /* Connect status Change and Current Connect status should be 1
   *  to indicate successful connection to downstream port.
   */
  if (NV_DRF_VAL (XUSB_DEV_XHCI, PORTSC, CSC, port_status) != 0UL) {
    if (NV_DRF_VAL (XUSB_DEV_XHCI, PORTSC, CCS, port_status) == 1UL) {
      p_xusb_dev_context->device_state = CONNECTED;
      port_speed                       = NV_DRF_VAL (XUSB_DEV_XHCI, PORTSC, PS, port_status);
      p_xusb_dev_context->port_speed   = port_speed;

 #if 0

      /* Reload Endpoint Context if not connected in superspeed
       * after changing packet size.
       */
      if (p_xusb_dev_context->port_speed != XUSB_SUPER_SPEED) {
        EP_CONTEXT  *p_ep_context;
        p_ep_context                  = &mPriavte->pEPContext[EP0_IN];
        p_ep_context->avg_trb_len     = 8;
        p_ep_context->max_packet_size = 64;
      }

 #endif
    } else {
      /* This will never happen because Vbus is overriden to 1.
       *  if CCS=0, somebody pulled the plug.
       */
      p_xusb_dev_context->device_state = DISCONNECTED;
    }

    port_status &= ~status_bits_mask;
    port_status  = NV_FLD_SET_DRF_NUM (
                     XUSB_DEV_XHCI,
                     PORTSC,
                     CSC,
                     1,
                     port_status
                     );
    MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTSC_0, port_status);
  }

  if (NV_DRF_VAL (XUSB_DEV_XHCI, PORTHALT, STCHG_REQ, reg_halt) != 0U) {
    reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTHALT_0);
    reg_data = NV_FLD_SET_DRF_NUM (
                 XUSB_DEV_XHCI,
                 PORTHALT,
                 HALT_LTSSM,
                 0,
                 reg_data
                 );
    MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTHALT_0, reg_data);
  }

  port_status = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTSC_0);

  /* Port Link status Change */
  if (NV_DRF_VAL (XUSB_DEV_XHCI, PORTSC, PLC, port_status) != 0U) {
    port_status &= ~status_bits_mask;
    link_state   = NV_DRF_VAL (XUSB_DEV_XHCI, PORTSC, PLS, port_status);

    /* U3 or Suspend */
    if (link_state == 0x3U) {
      p_xusb_dev_context->device_state = SUSPENDED;
    } else if ((link_state == 0x0U) &&
               (p_xusb_dev_context->device_state == SUSPENDED))
    {
      p_xusb_dev_context->device_state = CONFIGURED;
      reg_data                         = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_PAUSE_0);
      MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_PAUSE_0, 0);
      MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_STCHG_0, reg_data);
    } else {
      /* No Action Required */
    }

    port_status = NV_FLD_SET_DRF_NUM (
                    XUSB_DEV_XHCI,
                    PORTSC,
                    PLC,
                    1,
                    port_status
                    );
    MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTSC_0, port_status);
  }

  /* Config Error Change */
  if (NV_DRF_VAL (XUSB_DEV_XHCI, PORTSC, CEC, port_status) != 0UL) {
    port_status  = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTSC_0);
    port_status &= ~status_bits_mask;
    port_status  = NV_FLD_SET_DRF_NUM (
                     XUSB_DEV_XHCI,
                     PORTSC,
                     CEC,
                     1,
                     port_status
                     );
    MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTSC_0, port_status);
    Status = EFI_DEVICE_ERROR;
  }

  return Status;
}

static EFI_STATUS
XudcHandleTxferEvent (
  TRANSFER_EVENT_TRB_STRUCT  *p_tx_eventrb
  )
{
  /* Wait for event to be posted */
  EFI_STATUS           e                   = EFI_SUCCESS;
  XUSB_DEVICE_CONTEXT  *p_xusb_dev_context = &s_xusb_device_context;
  LINK_TRB_STRUCT      *p_link_trb;
  DATA_TRB_STRUCT      *p_next_trb;

  TEGRABL_UNUSED (p_link_trb);

  /* Make sure update local copy for dequeue ptr */
  if (p_tx_eventrb->emp_id == EP0_IN) {
    p_xusb_dev_context->cntrl_epdequeue_ptr +=
      sizeof (TRANSFER_EVENT_TRB_STRUCT);
    p_next_trb = (DATA_TRB_STRUCT *)p_xusb_dev_context->cntrl_epdequeue_ptr;

    /* Handle Link TRB */
    if (p_next_trb->trb_type == LINK_TRB) {
      p_link_trb = (LINK_TRB_STRUCT *)p_next_trb;
      p_next_trb = mPrivate->pTxRingEP0;
    }

    p_xusb_dev_context->cntrl_epdequeue_ptr = (UINT64)p_next_trb;
  }

  if (p_tx_eventrb->emp_id == EP1_OUT) {
    p_xusb_dev_context->bulkout_epdequeue_ptr +=
      sizeof (TRANSFER_EVENT_TRB_STRUCT);
    p_next_trb = (DATA_TRB_STRUCT *)p_xusb_dev_context->bulkout_epdequeue_ptr;

    /* Handle Link TRB */
    if (p_next_trb->trb_type == LINK_TRB) {
      p_link_trb = (LINK_TRB_STRUCT *)p_next_trb;
      p_next_trb = mPrivate->pTxRingEP1Out;
    }

    p_xusb_dev_context->bulkout_epdequeue_ptr = (UINT64)p_next_trb;
  }

  if (p_tx_eventrb->emp_id == EP1_IN) {
    p_xusb_dev_context->bulkin_epdequeue_ptr +=
      sizeof (TRANSFER_EVENT_TRB_STRUCT);
    p_next_trb = (DATA_TRB_STRUCT *)p_xusb_dev_context->bulkin_epdequeue_ptr;

    /* Handle Link TRB */
    if (p_next_trb->trb_type == LINK_TRB) {
      p_link_trb = (LINK_TRB_STRUCT *)p_next_trb;
      p_next_trb = mPrivate->pTxRingEP1In;
    }

    p_xusb_dev_context->bulkin_epdequeue_ptr = (UINT64)p_next_trb;
  }

  /* Check for errors. */
  if ((p_tx_eventrb->comp_code == SUCCESS_ERR_CODE) ||
      (p_tx_eventrb->comp_code == SHORT_PKT_ERR_CODE))
  {
    if (p_tx_eventrb->emp_id == EP0_IN) {
      if (p_xusb_dev_context->wait_for_eventt == DATA_STAGE_TRB) {
        /* Send status */
        e = XudcIssueStatusTrb (DIR_OUT);

        if (e != EFI_SUCCESS) {
          e = EFI_PROTOCOL_ERROR;
          DEBUG ((EFI_D_VERBOSE, "%a: Error status_trb\r\n", __FUNCTION__));
          return e;
        }
      } else if (p_xusb_dev_context->wait_for_eventt == STATUS_STAGE_TRB) {
        if (p_xusb_dev_context->device_state == ADDRESSED_STATUS_PENDING) {
          p_xusb_dev_context->device_state = ADDRESSED;
        }

        if (p_xusb_dev_context->device_state == CONFIGURED_STATUS_PENDING) {
          p_xusb_dev_context->device_state = CONFIGURED;

          // prepare default EP 1 OUT buffer
          UINT32  DataPacket = 0;
          if (p_xusb_dev_context->port_speed == XUSB_SUPER_SPEED) {
            DataPacket = 1024;
          } else {
            DataPacket = 512;
          }

          if (mPrivate->DataPacket == NULL) {
            e = DmaAllocateBuffer (
                  EfiRuntimeServicesData,
                  EFI_SIZE_TO_PAGES (MAX_TFR_LENGTH),
                  (VOID **)&mPrivate->DataPacket
                  );

            if (e != EFI_SUCCESS) {
              e = EFI_PROTOCOL_ERROR;
              DEBUG ((EFI_D_ERROR, "%a: Error on allocate Rx buffer\r\n", __FUNCTION__));
              return e;
            }
          }

          // Only put 1 TRB for EP1 out with size 512 or 1024 as defualt
          XudcIssueNormalTrb (mPrivate->DataPacket, DataPacket, DIR_OUT);
        }
      } else {
        /* No Action Required */
      }
    }

    if (p_tx_eventrb->emp_id == EP1_IN) {
      /* TRB Tx Len will be 0 or remaining bytes. */
      p_xusb_dev_context->tx_count--;

      /* For IN, we should not have remaining bytes. Flag error */
      if (p_tx_eventrb->trb_tx_len != 0U) {
        e = EFI_PROTOCOL_ERROR;
        DEBUG ((EFI_D_VERBOSE, "%a: Error remain bytes: %d\r\n", __FUNCTION__, p_tx_eventrb->trb_tx_len));
        return e;
      }

      UINT64             bufPtr     = U64_FROM_U32 (p_tx_eventrb->trb_pointer_lo, p_tx_eventrb->trb_pointer_hi);
      NORMAL_TRB_STRUCT  *NormalTrb = (NORMAL_TRB_STRUCT *)bufPtr;
      bufPtr = U64_FROM_U32 (NormalTrb->databufptr_lo, NormalTrb->databufptr_hi);
      mPrivate->DataSentCallback (1, NormalTrb->trb_tx_len, (VOID *)bufPtr);
    }

    if (p_tx_eventrb->emp_id == EP1_OUT) {
      /* TRB Tx Len will be 0 or remaining bytes for short packet. */

      /* Short packet is not necessary an error
       * because we prime for 4K bytes. */

      UINT32  DataPacket = 0;
      if (p_xusb_dev_context->port_speed == XUSB_SUPER_SPEED) {
        DataPacket = 1024;
      } else {
        DataPacket = 512;
      }

      /* Check how many bytes need to report to Application */
      UINTN  ReportReceived = 0;

      if (mPrivate->TotalRxLength > 0) {
        if (mPrivate->CurrentRxLength == 0) {
          /* This is the case that we got a tx xfer evt for first 512 or 1024 bytes */
          ReportReceived             = DataPacket - p_tx_eventrb->trb_tx_len;
          mPrivate->CurrentRxLength += ReportReceived;
        } else if ((mPrivate->TotalRxLength - mPrivate->CurrentRxLength) < MAX_TFR_LENGTH) {
          /* This is the case for last transfer */
          UINTN  lastChunk = mPrivate->TotalRxLength - mPrivate->CurrentRxLength;
          ReportReceived             = lastChunk - p_tx_eventrb->trb_tx_len;
          mPrivate->CurrentRxLength += ReportReceived;
        } else {
          ReportReceived             = MAX_TFR_LENGTH - p_tx_eventrb->trb_tx_len;
          mPrivate->CurrentRxLength += ReportReceived;
        }
      } else {
        ReportReceived = DataPacket - p_tx_eventrb->trb_tx_len;
      }

      /* Pass current received Rx to application */
      mPrivate->DataReceivedCallback (ReportReceived, mPrivate->DataPacket);

      /* Prepare next Rx packet */
      memset (mPrivate->DataPacket, 0, MAX_TFR_LENGTH);

      /* check the data size to put the next TRB */
      if (mPrivate->CurrentRxLength == mPrivate->TotalRxLength) {
        /* All Rx download finish, prepare for next transfer */
        /* DataPacket = 512 or 1024 */
        mPrivate->CurrentRxLength = 0;
        mPrivate->TotalRxLength   = 0;
      } else if (mPrivate->TotalRxLength - mPrivate->CurrentRxLength < MAX_TFR_LENGTH) {
        /* Prepare for last chunk */
        DataPacket = (mPrivate->TotalRxLength - mPrivate->CurrentRxLength);
      } else {
        DataPacket = MAX_TFR_LENGTH;
      }

      XudcIssueNormalTrb (mPrivate->DataPacket, DataPacket, DIR_OUT);
    }

    /* This should be zero except in the case of a short packet. */
  } else if (p_tx_eventrb->comp_code == CTRL_DIR_ERR_CODE) {
    e = EFI_PROTOCOL_ERROR;
    DEBUG ((EFI_D_ERROR, "%a: comp_code == CTRL_DIR_ERR_CODE\r\n", __FUNCTION__));
  } else if (p_tx_eventrb->comp_code == CTRL_SEQ_NUM_ERR_CODE) {
    DEBUG ((
      EFI_D_ERROR,
      "%a: ERR_OUT_OF_SEQUENCE comp_code %u %u\r\n",
      __FUNCTION__,
      SUCCESS_ERR_CODE,
      CTRL_SEQ_NUM_ERR_CODE
      ));
    e = XudcHandleSetupPkt (&mPrivate->usb_setup_data[0]);
    return e;
  } else {
    e = EFI_PROTOCOL_ERROR;
    DEBUG ((EFI_D_ERROR, "%a: comp_code:0x%08x\r\n", __FUNCTION__, p_tx_eventrb->comp_code));
  }

  return e;
}

static EFI_STATUS
XudcPollForEvent (
  UINT32  timeout
  )
{
  EFI_STATUS              Status = EFI_SUCCESS;
  UINT32                  reg_data, expected_value, int_pending_mask;
  EVENT_TRB_STRUCT        *p_event_trb;
  UINT32                  trb_index;
  SETUP_EVENT_TRB_STRUCT  *p_setup_event_trb;
  XUSB_DEVICE_CONTEXT     *p_xusb_dev_context = &s_xusb_device_context;
  UINT64                  tmp_dma_addr;
  UINT64                  er_dma_start_address;

  expected_value   = 1U << SHIFT (XUSB_DEV_XHCI, ST, IP);
  int_pending_mask = SHIFTMASK (XUSB_DEV_XHCI, ST, IP);

  /* Poll for interrupt pending bit. */
  Status = XudcPollField (
             (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_ST_0),
             int_pending_mask,
             expected_value,
             timeout
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DEBUG ((EFI_D_ERROR, "%a: PORTSC_0: 0x%08x\r\n", __FUNCTION__, MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTSC_0)));

  // Clear interrupt pending
  reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_ST_0);
  reg_data = NV_FLD_SET_DRF_NUM (
               XUSB_DEV_XHCI,
               ST,
               IP,
               1,
               reg_data
               );
  MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_ST_0, reg_data);

  // read Event Ring enqueue pointer
  reg_data  = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EREPLO_0);
  reg_data &= SHIFTMASK (XUSB_DEV_XHCI, EREPLO, ADDRLO);

  tmp_dma_addr = U64_FROM_U32 (
                   reg_data,
                   MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EREPHI_0)
                   );
  er_dma_start_address = p_xusb_dev_context->dma_er_start_address;
  trb_index            = ((UINT32)(tmp_dma_addr - er_dma_start_address) /
                          sizeof (EVENT_TRB_STRUCT));
  p_xusb_dev_context->event_enqueue_ptr = (UINT64)
                                          ((EVENT_TRB_STRUCT *)&mPrivate->pEventRing[0] + trb_index);

  p_event_trb = (EVENT_TRB_STRUCT *)p_xusb_dev_context->event_dequeue_ptr;

  /* Make sure cycle state matches */
  /* TODO :Need to find suitable error value */
  if (p_event_trb->c !=  p_xusb_dev_context->event_ccs) {
    Status = EFI_NOT_FOUND;
    return Status;
  }

  while (p_event_trb->c ==  p_xusb_dev_context->event_ccs) {
    if (p_event_trb->trb_type == SETUP_EVENT_TRB) {
 #if 0
      UINT32  *tmp = (UINT32 *)p_event_trb;
      DEBUG ((EFI_D_ERROR, "%a: SETUP_EVENT_TRB addr: %p\r\n", __FUNCTION__, p_event_trb));
      DEBUG ((EFI_D_ERROR, "%a: SETUP_EVENT_TRB dw0: 0x%08x\r\n", __FUNCTION__, tmp[0]));
      DEBUG ((EFI_D_ERROR, "%a: SETUP_EVENT_TRB dw1: 0x%08x\r\n", __FUNCTION__, tmp[1]));
      DEBUG ((EFI_D_ERROR, "%a: SETUP_EVENT_TRB dw2: 0x%08x\r\n", __FUNCTION__, tmp[2]));
      DEBUG ((EFI_D_ERROR, "%a: SETUP_EVENT_TRB dw3: 0x%08x\r\n", __FUNCTION__, tmp[3]));
 #endif
      /* Check if we are waiting for setup packet */
      p_setup_event_trb = (SETUP_EVENT_TRB_STRUCT *)p_event_trb;
      memcpy (
        (VOID *)&mPrivate->usb_setup_data[0],
        (VOID *)&p_setup_event_trb->data[0],
        8
        );
 #if 0
      DEBUG ((
        EFI_D_ERROR,
        "%a: SETUP DATA: 0x%x 0x%x\r\n",
        __FUNCTION__,
        p_setup_event_trb->data[0],
        p_setup_event_trb->data[1]
        ));
 #endif
      p_xusb_dev_context->cntrl_seq_num = p_setup_event_trb->ctrl_seq_num;
      Status                            = XudcHandleSetupPkt (&mPrivate->usb_setup_data[0]);
    } else if (p_event_trb->trb_type == PORT_STATUS_CHANGE_TRB) {
 #if 0
      UINT32  *tmp = (UINT32 *)p_event_trb;
      DEBUG ((EFI_D_ERROR, "%a: PORT_STATUS_CHANGE_TRB dw0 addr: %p\r\n", __FUNCTION__, &tmp[0]));
      DEBUG ((EFI_D_ERROR, "%a: PORT_STATUS_CHANGE_TRB dw0: 0x%08x\r\n", __FUNCTION__, tmp[0]));
      DEBUG ((EFI_D_ERROR, "%a: PORT_STATUS_CHANGE_TRB dw1: 0x%08x\r\n", __FUNCTION__, tmp[1]));
      DEBUG ((EFI_D_ERROR, "%a: PORT_STATUS_CHANGE_TRB dw2: 0x%08x\r\n", __FUNCTION__, tmp[2]));
      DEBUG ((EFI_D_ERROR, "%a: PORT_STATUS_CHANGE_TRB dw3: 0x%08x\r\n", __FUNCTION__, tmp[3]));
 #endif
      /* Handle all port status changes here. */
      Status = XudcHandlePortStatus ();
    } else if (p_event_trb->trb_type == TRANSFER_EVENT_TRB) {
 #if 0
      UINT32  *tmp = (UINT32 *)p_event_trb;
      DEBUG ((EFI_D_ERROR, "%a: TRANSFER_EVENT_TRB dw0: 0x%08x\r\n", __FUNCTION__, tmp[0]));
      DEBUG ((EFI_D_ERROR, "%a: TRANSFER_EVENT_TRB dw1: 0x%08x\r\n", __FUNCTION__, tmp[1]));
      DEBUG ((EFI_D_ERROR, "%a: TRANSFER_EVENT_TRB dw2: 0x%08x\r\n", __FUNCTION__, tmp[2]));
      DEBUG ((EFI_D_ERROR, "%a: TRANSFER_EVENT_TRB dw3: 0x%08x\r\n", __FUNCTION__, tmp[3]));
 #endif
      /* Handle tx event changes here. */
      Status = XudcHandleTxferEvent (
                 (TRANSFER_EVENT_TRB_STRUCT *)p_event_trb
                 );
    } else {
      UINT32  *tmp = (UINT32 *)p_event_trb;
      DEBUG ((EFI_D_ERROR, "%a: EVENT_TRB dw0: 0x%08x\r\n", __FUNCTION__, tmp[0]));
      DEBUG ((EFI_D_ERROR, "%a: EVENT_TRB dw1: 0x%08x\r\n", __FUNCTION__, tmp[1]));
      DEBUG ((EFI_D_ERROR, "%a: EVENT_TRB dw2: 0x%08x\r\n", __FUNCTION__, tmp[2]));
      DEBUG ((EFI_D_ERROR, "%a: EVENT_TRB dw3: 0x%08x\r\n", __FUNCTION__, tmp[3]));
      /* No Action required */
    }

    /* Increment Event Dequeue Ptr.
    * Check if last element of ring to wrap around and toggle cycle bit.
    */
    if (p_xusb_dev_context->event_dequeue_ptr ==
        (UINT64)&mPrivate->pEventRing[NUM_TRB_EVENT_RING - 1U])
    {
      p_xusb_dev_context->event_dequeue_ptr = (UINT64)&mPrivate->pEventRing[0];
      p_xusb_dev_context->event_ccs        ^= 1U;
    } else {
      p_xusb_dev_context->event_dequeue_ptr += sizeof (EVENT_TRB_STRUCT);
    }

    p_event_trb = (EVENT_TRB_STRUCT *)p_xusb_dev_context->event_dequeue_ptr;

    /* Process only events posted when interrupt was triggered.
    * New posted events will be handled during the next
    * interrupt handler call.
    */
    if (p_xusb_dev_context->event_dequeue_ptr ==
        p_xusb_dev_context->event_enqueue_ptr)
    {
      break;
    }
  }

  // DEBUG ((EFI_D_ERROR, "%a: EVENT_TRB process finished\r\n",__FUNCTION__));

  reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_ERDPLO_0);
  /* Clear Event Handler Busy bit */
  if (NV_DRF_VAL (XUSB_DEV_XHCI, ERDPLO, EHB, reg_data) != 0U) {
    reg_data = NV_FLD_SET_DRF_NUM (
                 XUSB_DEV_XHCI,
                 ERDPLO,
                 EHB,
                 1,
                 reg_data
                 );
  }

  trb_index = (UINT32)(p_xusb_dev_context->event_dequeue_ptr -
                       (UINT64)mPrivate->pEventRing) / sizeof (EVENT_TRB_STRUCT);

  tmp_dma_addr = (p_xusb_dev_context->dma_er_start_address +
                  ((UINT64)trb_index * sizeof (EVENT_TRB_STRUCT)));

  reg_data = NV_FLD_SET_DRF_NUM (
               XUSB_DEV_XHCI,
               ERDPLO,
               ADDRLO,
               (U64_TO_U32_LO (tmp_dma_addr) >> 4),
               reg_data
               );

  MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_ERDPLO_0, reg_data);

  /* Set bits 63:32 of Dequeue pointer. */
  MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_ERDPHI_0, U64_TO_U32_HI (tmp_dma_addr));

  return Status;
}

static EFI_STATUS
XudcInitEpEventRing (
  VOID
  )
{
  XUSB_DEVICE_CONTEXT  *p_xusb_dev_context = &s_xusb_device_context;
  UINT32               reg_data;
  UINT8                arg;
  EFI_STATUS           Status = EFI_SUCCESS;
  UINTN                BufferSize;
  UINT64               BusAddress, tbuf;
  VOID                 *Mapping;

  /* Initialize Enqueue and Dequeue pointer of consumer context*/
  p_xusb_dev_context->event_dequeue_ptr = (UINT64)mPrivate->pEventRing;
  p_xusb_dev_context->event_enqueue_ptr = (UINT64)mPrivate->pEventRing;
  p_xusb_dev_context->event_ccs         = 1;

  /* Set event ring segment 0 and segment 1 */
  /* Segment 0 */
  BufferSize = EVENT_RING_SIZE;
  Status     = DmaMap (MapOperationBusMasterCommonBuffer, (VOID *)mPrivate->pEventRing, &BufferSize, &BusAddress, &Mapping);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  p_xusb_dev_context->dma_er_start_address = BusAddress;

  MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_ERST0BALO_0, U64_TO_U32_LO (BusAddress));
  MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_ERST0BAHI_0, U64_TO_U32_HI (BusAddress));

  /* Segment 1 */
  tbuf = (BusAddress + ((NUM_TRB_EVENT_RING/2U) * sizeof (EVENT_TRB_STRUCT)));
  MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_ERST1BALO_0, U64_TO_U32_LO (tbuf));

  MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_ERST1BAHI_0, U64_TO_U32_HI (tbuf));
  /* Write segment sizes */
  arg      = NUM_TRB_EVENT_RING/2U;
  reg_data =
    NV_DRF_NUM (XUSB_DEV_XHCI, ERSTSZ, ERST0SZ, arg) |
    NV_DRF_NUM (XUSB_DEV_XHCI, ERSTSZ, ERST1SZ, arg);
  MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_ERSTSZ_0, reg_data);

  /* Set Enqueue/Producer Cycle State for controller */
  reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EREPLO_0);
  reg_data = NV_FLD_SET_DRF_NUM (
               XUSB_DEV_XHCI,
               EREPLO,
               ECS,
               p_xusb_dev_context->event_ccs,
               reg_data
               );

  reg_data = NV_FLD_SET_DRF_NUM (
               XUSB_DEV_XHCI,
               EREPLO,
               SEGI,
               0,
               reg_data
               );

  /* Bits 3:0 are not used to indicate 16 byte aligned.
   * Shift the Enqueue Pointer before using DRF macro.
   */
  reg_data = NV_FLD_SET_DRF_NUM (
               XUSB_DEV_XHCI,
               EREPLO,
               ADDRLO,
               (U64_TO_U32_LO (BusAddress) >> 4),
               reg_data
               );

  MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EREPLO_0, reg_data);

  /* Set 63:32 bits of enqueue pointer. */
  MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EREPHI_0, U64_TO_U32_HI (BusAddress));

  /* Set the Dequeue Pointer */
  reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_ERDPLO_0);
  reg_data = NV_FLD_SET_DRF_NUM (
               XUSB_DEV_XHCI,
               ERDPLO,
               ADDRLO,
               (U64_TO_U32_LO (BusAddress) >> 4),
               reg_data
               );

  MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_ERDPLO_0, reg_data);
  /* Set bits 63:32 of Dequeue pointer. */
  MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_ERDPHI_0, U64_TO_U32_HI (BusAddress));
  return Status;
}

static EFI_STATUS
XudcInitTransferRing (
  UINT8  ep_index
  )
{
  EFI_STATUS  Status;

  Status = EFI_SUCCESS;

  /* zero out tx ring */
  if ((ep_index == EP0_IN) || (ep_index == EP0_OUT)) {
    memset (
      (VOID *)mPrivate->pTxRingEP0,
      0,
      NUM_TRB_TRANSFER_RING * sizeof (DATA_TRB_STRUCT)
      );
  } else if (ep_index == EP1_IN) {
    memset (
      (VOID *)mPrivate->pTxRingEP1In,
      0,
      NUM_TRB_TRANSFER_RING * sizeof (DATA_TRB_STRUCT)
      );
  } else if (ep_index == EP1_OUT) {
    memset (
      (VOID *)mPrivate->pTxRingEP1Out,
      0,
      NUM_TRB_TRANSFER_RING * sizeof (DATA_TRB_STRUCT)
      );
  } else {
    Status = EFI_UNSUPPORTED;
    DEBUG ((
      EFI_D_ERROR,
      "%a, Given endpoint: %u is not supported\r\n",
      __FUNCTION__,
      ep_index
      ));
  }

  return Status;
}

static EFI_STATUS
XudcPollField (
  UINT32  reg_addr,
  UINT32  mask,
  UINT32  expected_value,
  UINT32  timeout
  )
{
  UINT32      reg_data;
  EFI_STATUS  Status = EFI_SUCCESS;

  do {
    reg_data = MmioRead32 (reg_addr);

    if ((reg_data & mask) == expected_value) {
      return EFI_SUCCESS;
    }

    DeviceDiscoveryThreadMicroSecondDelay (1);
    timeout--;
  } while (timeout != 0U);

  Status   = EFI_TIMEOUT;
  reg_data = MmioRead32 (reg_addr);
  // DEBUG ((EFI_D_ERROR, "%a, pending interrupt 0x%08x\r\n", __FUNCTION__, reg_data));

  return Status;
}

static EFI_STATUS
XudcDisableEp (
  UINT8  ep_index
  )
{
  UINT32      reg_data, expected_value, mask;
  EFI_STATUS  Status = EFI_SUCCESS;
  EP_CONTEXT  *ep_info;
  UINT8       arg;

  /* Cannot disable endpoint 0. */
  if ((ep_index == EP0_IN) || (ep_index == EP0_OUT)) {
    DEBUG ((
      EFI_D_ERROR,
      "%a, Can not disable ep %u\r\n",
      __FUNCTION__,
      EP0_IN
      ));

    return EFI_INVALID_PARAMETER;
  }

  ep_info = &mPrivate->pEPContext[ep_index];

  /* Disable Endpoint */
  ep_info->ep_state = EP_DISABLED;
  arg               = 1U << ep_index;
  reg_data          = NV_DRF_NUM (XUSB_DEV_XHCI, EP_RELOAD, DCI, arg);

  MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_RELOAD_0, reg_data);
  /* TODO timeout for polling */
  mask           = 1UL << ep_index;
  expected_value = 0;
  Status         = XudcPollField (
                     mPrivate->XudcBaseAddress+XUSB_DEV_XHCI_EP_RELOAD_0,
                     mask,
                     expected_value,
                     1000
                     );
  if (Status != EFI_SUCCESS) {
    Status = EFI_TIMEOUT;
  }

  return Status;
}

static EFI_STATUS
XudcInitEpContext (
  UINT8  ep_index
  )
{
  EP_CONTEXT           *ep_info;
  XUSB_DEVICE_CONTEXT  *p_xusb_dev_context;
  LINK_TRB_STRUCT      *p_link_trb;
  UINT64               dma_buf;
  UINTN                BufferSize;
  EFI_STATUS           Status = EFI_SUCCESS;
  VOID                 *Mapping;

  if (ep_index > EP1_IN) {
    Status = EFI_INVALID_PARAMETER;
    return Status;
  }

  p_xusb_dev_context = &s_xusb_device_context;

  if (ep_index == EP0_OUT) {
    ep_index = EP0_IN;
  }

  /* Setting Ep Context */
  ep_info = &mPrivate->pEPContext[ep_index];

  /* Control Endpoint 0. */
  if (ep_index == EP0_IN) {
    memset ((VOID *)ep_info, 0, sizeof (EP_CONTEXT));
    /* Set Endpoint State to running. */
    ep_info->ep_state = EP_RUNNING;
    /* Set error count to 3 */
    ep_info->cerr = 3;
    /* Set Burst size 0 */
    ep_info->max_burst_size = 0;
    /* Set Packet Size as 512 (SS) for now */
    ep_info->max_packet_size = 512;
    /* Set CCS for controller to 1. Cycle bit should be set to 1. */
    ep_info->dcs = 1;
    /* cerr Count */
    ep_info->cec = 0x3;
    /* Initialize Producer Cycle State to 1. */
    p_xusb_dev_context->cntrl_pcs = 1;
    /* SW copy of Dequeue pointer for control endpoint. */
    p_xusb_dev_context->cntrl_epdequeue_ptr = (UINT64)mPrivate->pTxRingEP0;
    p_xusb_dev_context->cntrl_epenqueue_ptr = (UINT64)mPrivate->pTxRingEP0;

    /* EP specific Context
       Set endpoint type to Control. */
    #define EP_TYPE_CNTRL  4
    /* Average TRB length. Setup data always 8 bytes. */
    ep_info->avg_trb_len = 8;
    ep_info->ep_type     = EP_TYPE_CNTRL;

    BufferSize = TX_RING_EP0_SIZE;
    /* Set the dequeue pointer for the consumer (i.e. XUSB Controller) */
    Status = DmaMap (MapOperationBusMasterCommonBuffer, (VOID *)mPrivate->pTxRingEP0, &BufferSize, &dma_buf, &Mapping);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    ep_info->trd_dequeueptr_lo = (U64_TO_U32_LO (dma_buf) >> 4);

    ep_info->trd_dequeueptr_hi = U64_TO_U32_HI (dma_buf);

    /* Setup Link TRB. Last TRB of ring. */
    p_link_trb                 = (LINK_TRB_STRUCT *)(UINT64)&mPrivate->pTxRingEP0[NUM_TRB_TRANSFER_RING-1U];
    p_link_trb->tc             = 1;
    p_link_trb->ring_seg_ptrlo = (U64_TO_U32_LO (dma_buf) >> 4);

    p_link_trb->ring_seg_ptrhi = U64_TO_U32_HI (dma_buf);
    p_link_trb->trb_type       = LINK_TRB;
  } else {
    if (ep_index == EP1_OUT) {
      memset ((VOID *)ep_info, 0, sizeof (EP_CONTEXT));
      ep_info->ep_state = EP_RUNNING;
      /* Set error count to 3 */
      ep_info->cerr = 3;
      /* Set Burst size 0 */
      ep_info->max_burst_size = 0;
      /* Set CCS for controller to 1. Cycle bit should be set to 1. */
      ep_info->dcs = 1;

      /* Set CCS for controller to 1. Cycle bit should be set to 1.
         cerr Count */
      ep_info->cec = 0x3;

      /* Initialize Producer Cycle State to 1. */
      p_xusb_dev_context->bulkout_pcs = 1;

      /* SW copy of Dequeue pointer for endpoint 1 out. */
      p_xusb_dev_context->bulkout_epdequeue_ptr = (UINT64)mPrivate->pTxRingEP1Out;
      p_xusb_dev_context->bulkout_epenqueue_ptr = (UINT64)mPrivate->pTxRingEP1Out;

      /* EP specific Context. Set endpoint type to Bulk. */
      #define EP_TYPE_BULK_OUT  2
      if (p_xusb_dev_context->port_speed == XUSB_SUPER_SPEED) {
        ep_info->avg_trb_len     = 1024;
        ep_info->max_packet_size = 1024;
      } else if (p_xusb_dev_context->port_speed == XUSB_HIGH_SPEED) {
        ep_info->avg_trb_len     = 512;
        ep_info->max_packet_size = 512;
      } else {
        ep_info->avg_trb_len     = 512;
        ep_info->max_packet_size = 64;
      }

      ep_info->ep_type = EP_TYPE_BULK_OUT;

      BufferSize = TX_RING_EP1_OUT_SIZE;
      /* Set the dequeue pointer for the consumer (i.e. XUSB Controller) */
      Status = DmaMap (MapOperationBusMasterCommonBuffer, (VOID *)mPrivate->pTxRingEP1Out, &BufferSize, &dma_buf, &Mapping);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      ep_info->trd_dequeueptr_lo = (U64_TO_U32_LO (dma_buf) >> 4);
      ep_info->trd_dequeueptr_hi = U64_TO_U32_HI (dma_buf);

      /* Setup Link TRB. Last TRB of ring. */
      p_link_trb                 = (LINK_TRB_STRUCT *)&mPrivate->pTxRingEP1Out[NUM_TRB_TRANSFER_RING - 1U];
      p_link_trb->tc             = 1;
      p_link_trb->ring_seg_ptrlo = (U64_TO_U32_LO (dma_buf) >> 4);

      p_link_trb->ring_seg_ptrhi = U64_TO_U32_HI (dma_buf);
      p_link_trb->trb_type       = LINK_TRB;
    } else {
      /* EP IN */
      memset ((VOID *)ep_info, 0, sizeof (EP_CONTEXT));
      ep_info->ep_state = EP_RUNNING;
      /* Set error count to 3 */
      ep_info->cerr = 3;
      /* Set Burst size 0 */
      ep_info->max_burst_size = 0;
      /* Set CCS for controller to 1. Cycle bit should be set to 1. */
      ep_info->dcs = 1;

      /* Set CCS for controller to 1. Cycle bit should be set to 1.
         cerr Count */
      ep_info->cec = 0x3;

      /* Initialize Producer Cycle State to 1. */
      p_xusb_dev_context->bulkin_pcs = 1;

      /* SW copy of Dequeue pointer for control endpoint. */
      p_xusb_dev_context->bulkin_epdequeue_ptr = (UINT64)mPrivate->pTxRingEP1In;
      p_xusb_dev_context->bulkin_epenqueue_ptr = (UINT64)mPrivate->pTxRingEP1In;

      /* EP specific Context. Set endpoint type to Bulk. */
      #define EP_TYPE_BULK_IN  6
      if (p_xusb_dev_context->port_speed == XUSB_SUPER_SPEED) {
        ep_info->avg_trb_len     = 1024;
        ep_info->max_packet_size = 1024;
      } else if (p_xusb_dev_context->port_speed == XUSB_HIGH_SPEED) {
        ep_info->avg_trb_len     = 512;
        ep_info->max_packet_size = 512;
      } else {
        ep_info->avg_trb_len     = 512;
        ep_info->max_packet_size = 64;
      }

      ep_info->ep_type = EP_TYPE_BULK_IN;

      BufferSize = TX_RING_EP1_IN_SIZE;
      /* Set the dequeue pointer for the consumer (i.e. XUSB Controller) */
      Status = DmaMap (MapOperationBusMasterCommonBuffer, mPrivate->pTxRingEP1In, &BufferSize, &dma_buf, &Mapping);
      if (EFI_ERROR (Status)) {
        return Status;
      }

      ep_info->trd_dequeueptr_lo = (U64_TO_U32_LO (dma_buf) >> 4);
      ep_info->trd_dequeueptr_hi = U64_TO_U32_HI (dma_buf);

      /* Setup Link TRB. Last TRB of ring. */
      p_link_trb                 = (LINK_TRB_STRUCT *)&mPrivate->pTxRingEP1In[NUM_TRB_TRANSFER_RING - 1U];
      p_link_trb->tc             = 1;
      p_link_trb->ring_seg_ptrlo = (U64_TO_U32_LO (dma_buf) >> 4);

      p_link_trb->ring_seg_ptrhi = U64_TO_U32_HI (dma_buf);
      p_link_trb->trb_type       = LINK_TRB;
    }
  }

  BufferSize = EP_CONTEXT_SIZE;
  Status     = DmaMap (MapOperationBusMasterCommonBuffer, mPrivate->pEPContext, &BufferSize, &dma_buf, &Mapping);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (ep_index == EP0_IN) {
    p_xusb_dev_context->dma_ep_context_start_addr = dma_buf;
  }

  return Status;
}

static EFI_STATUS
XudcInitep (
  UINT8    ep_index,
  BOOLEAN  reinit
  )
{
  UINT32 expected_value;
  UINT32 mask;
  UINT32 reg_data;
  EFI_STATUS Status = EFI_SUCCESS;
  UINT8 arg;
  UINT32 temp_mask;

  Status = XudcInitTransferRing (ep_index);
  if (EFI_ERROR (Status)) {
    goto fail;
  }

  if ((ep_index == EP0_IN) || (ep_index == EP0_OUT)) {
    Status = XudcInitEpContext (EP0_IN);
    if (EFI_ERROR (Status)) {
      goto fail;
    }

    /* Make sure endpoint is not paused or halted. */
    reg_data  = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_PAUSE_0);
    temp_mask = 1UL << ep_index;
    reg_data &= ~(temp_mask);
    MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_PAUSE_0, reg_data);
    reg_data  = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_HALT_0);
    temp_mask = 1UL << ep_index;
    reg_data &= ~(temp_mask);
    MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_HALT_0, reg_data);
  } else if ((ep_index == EP1_IN) || (ep_index == EP1_OUT)) {
    Status = XudcInitEpContext (ep_index);
    if (EFI_ERROR (Status)) {
      goto fail;
    }

    if (reinit == FALSE) {
      /* Bit 2 for EP1_OUT , Bit 3 for EP1_IN
      * Force load context
      * Steps from device_mode IAS, 5.1.3.1
      */
      arg      = 1U << ep_index;
      reg_data = NV_DRF_NUM (XUSB_DEV_XHCI, EP_RELOAD, DCI, arg);
      MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_RELOAD_0, reg_data);

      mask           = 1UL << ep_index;
      expected_value = 0;
      Status         = XudcPollField (
                         mPrivate->XudcBaseAddress +
                         XUSB_DEV_XHCI_EP_RELOAD_0,
                         mask,
                         expected_value,
                         1000
                         );
      if (EFI_ERROR (Status)) {
        goto fail;
      }
    }

    /* Make sure ep is not Npaused or halted. */
    reg_data  = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_PAUSE_0);
    temp_mask = 1UL << ep_index;
    reg_data &= ~(temp_mask);
    MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_PAUSE_0, reg_data);
    reg_data  = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_HALT_0);
    temp_mask = 1UL << ep_index;
    reg_data &= ~(temp_mask);
    MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_EP_HALT_0, reg_data);
  } else {
    Status = EFI_INVALID_PARAMETER;
  }

fail:
  return Status;
}

STATIC
VOID
XudcCheckInterrupts (
  IN        VOID  *p,
  IN        VOID  *q
  )
{
  XudcPollForEvent (0x10UL);
}

STATIC
VOID
XudcSetRxLength (
  IN        UINT8  EndpointIndex,
  IN        UINTN  Size
  )
{
  if (EndpointIndex != 1UL) {
    // Current only support endpoint 1
    return;
  }

  mPrivate->TotalRxLength = Size;
}

EFI_STATUS
EFIAPI
XudcUsbDeviceSend (
  IN        UINT8  EndpointIndex,
  IN        UINTN  Size,
  IN  CONST VOID   *Buffer
  )
{
  EFI_STATUS retval                       = EFI_SUCCESS;
  XUSB_DEVICE_CONTEXT *p_xusb_dev_context = &s_xusb_device_context;

  DEBUG ((DEBUG_VERBOSE, "XudcUsbDeviceSend %u, 0x%p\n", Size, Buffer));

  retval = XudcIssueNormalTrb ((UINT8 *)Buffer, Size, DIR_IN);
  if (retval != EFI_SUCCESS) {
    return EFI_DEVICE_ERROR;
  }

  p_xusb_dev_context->tx_count++;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
XudcUsbDeviceStart (
  IN USB_DEVICE_DESCRIPTOR  *DeviceDescriptor,
  IN VOID                   **Descriptors,
  IN XUDC_RX_CALLBACK       RxCallback,
  IN XUDC_TX_CALLBACK       TxCallback
  )
{
  EFI_STATUS Status;
  UINT32 reg_data;
  XUSB_DEVICE_CONTEXT *p_xusb_dev_context = &s_xusb_device_context;

  DEBUG ((DEBUG_VERBOSE, "XudcUsbDeviceStart\n"));

  // Add XUSB Initialization Sequence.
  XudcClockPadProgramming ();

  // Intialize Event Ring
  Status = XudcInitEpEventRing ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Initialize EP0
  Status = XudcInitep (EP0_IN, FALSE);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  /* Make sure we get events due to port changes. */
  reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_CTRL_0);
  reg_data = NV_FLD_SET_DRF_DEF (
               XUSB_DEV_XHCI,
               CTRL,
               LSE,
               EN,
               reg_data
               );

  reg_data |= (1 << 4);

  MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_CTRL_0, reg_data);

  /* Initialize EndPoint Context */
  MmioWrite32 (
    mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_ECPLO_0,
    U64_TO_U32_LO (p_xusb_dev_context->dma_ep_context_start_addr)
    );

  MmioWrite32 (
    mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_ECPHI_0,
    U64_TO_U32_HI (p_xusb_dev_context->dma_ep_context_start_addr)
    );

  reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTHALT_0);
  reg_data = NV_FLD_SET_DRF_NUM (
               XUSB_DEV_XHCI,
               PORTHALT,
               STCHG_INTR_EN,
               1,
               reg_data
               );

  MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTHALT_0, reg_data);

  g_usbconfig                    = (struct tegrabl_usbf_config *)&config_fastboot;
  mPrivate->DataReceivedCallback = RxCallback;
  mPrivate->DataSentCallback     = TxCallback;
  mPrivate->DataPacket           = NULL;

  XudcEnumerate ();

  Status = gBS->CreateEvent (
                  EVT_TIMER | EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  XudcCheckInterrupts,
                  NULL,
                  &mPrivate->TimerEvent
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->SetTimer (
                  mPrivate->TimerEvent,
                  TimerPeriodic,
                  XUSB_INTERRUPT_POLL_PERIOD
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return Status;
}

static VOID
XudcClockPadProgramming (
  void
  )
{
  UINT32                     val;
  EFI_STATUS err = EFI_SUCCESS;

  /* Initialize USB Pad Registers */
  err = mPrivate->mUsbPadCtlProtocol->InitDevHw (mPrivate->mUsbPadCtlProtocol);
  if (EFI_ERROR (err)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a, Failed to Initailize USB HW: %r\r\n",
      __FUNCTION__,
      err
      ));
    goto fail;
  }

  /* BUS_MASTER, MEMORY_SPACE, IO_SPACE ENABLE */
  val = MmioRead32 (
          mPrivate->FpciBaseAddress +
          XUSB_DEV_CFG_1_0
          );
  val = NV_FLD_SET_DRF_NUM (XUSB_DEV, CFG_1, MEMORY_SPACE, 1, val);
  val = NV_FLD_SET_DRF_NUM (XUSB_DEV, CFG_1, BUS_MASTER, 1, val);
  MmioWrite32 (
    mPrivate->FpciBaseAddress +
    XUSB_DEV_CFG_1_0,
    val
    );

  /* Program BAR0 space */
  val = MmioRead32 (mPrivate->FpciBaseAddress + XUSB_DEV_CFG_4_0);

  val &= ~(XUSB_DEV_CFG_4_BASE_ADDR_MASK << XUSB_DEV_CFG_4_BASE_ADDR_SHIFT);
  val |= mPrivate->XudcBaseAddress & (XUSB_DEV_CFG_4_BASE_ADDR_MASK <<
                                      XUSB_DEV_CFG_4_BASE_ADDR_SHIFT);

  MmioWrite32 (
    mPrivate->FpciBaseAddress +
    XUSB_DEV_CFG_4_0,
    val
    );

  DeviceDiscoveryThreadMicroSecondDelay (120);
  return;

fail:
  DEBUG ((DEBUG_ERROR, "XudcClockPadProgramming fail\n"));
}

static EFI_STATUS
XudcCreateNormalTrb (
  NORMAL_TRB_STRUCT  *p_normal_trb,
  UINT64             buffer,
  UINT32             bytes,
  UINT32             dir
  )
{
  XUSB_DEVICE_CONTEXT *p_xusb_dev_context = &s_xusb_device_context;

  p_normal_trb->databufptr_lo = U64_TO_U32_LO (buffer);
  p_normal_trb->databufptr_hi = U64_TO_U32_HI (buffer);
  p_normal_trb->trb_tx_len    = bytes;

  /* Number of packets remaining.
   * BL will always queue only 1 TRB at a time.
   */
  p_normal_trb->tdsize = 0;
  if (dir == DIR_IN) {
    p_normal_trb->c = (UINT8)p_xusb_dev_context->bulkin_pcs;
  } else {
    p_normal_trb->c = (UINT8)p_xusb_dev_context->bulkout_pcs;
  }

  p_normal_trb->ent = 0;
  /* Make sure to interrupt on short packet i.e generate event. */
  p_normal_trb->isp = 1;
  /* and on Completion. */
  p_normal_trb->ioc = 1;

  p_normal_trb->trb_type = NORMAL_TRB;

  return EFI_SUCCESS;
}

static EFI_STATUS
XudcIssueNormalTrb (
  UINT8   *buffer,
  UINT32  bytes,
  UINT32  direction
  )
{
  NORMAL_TRB_STRUCT normal_trb;
  EFI_STATUS e                            = EFI_SUCCESS;
  XUSB_DEVICE_CONTEXT *p_xusb_dev_context = &s_xusb_device_context;
  UINT8 ep_index;
  dma_addr_t dma_buf;
  UINTN BufferSize = bytes;
  VOID *Mapping;

  memset ((void *)&normal_trb, 0, sizeof (NORMAL_TRB_STRUCT));
  e = DmaMap (
        MapOperationBusMasterCommonBuffer,
        (VOID *)buffer,
        &BufferSize,
        &dma_buf,
        &Mapping
        );
  if (e != EFI_SUCCESS) {
    goto fail;
  }

  e = XudcCreateNormalTrb (&normal_trb, dma_buf, bytes, direction);
  if (e != EFI_SUCCESS) {
    goto unmap;
  }

  ep_index = (direction == DIR_IN) ? EP1_IN : EP1_OUT;

  e = XudcQueueTrb (ep_index, &normal_trb, 1);
  if (e != EFI_SUCCESS) {
    goto unmap;
  }

  p_xusb_dev_context->wait_for_eventt = NORMAL_TRB;

unmap:
  e = DmaUnmap (Mapping);

fail:
  return e;
}

static EFI_STATUS
XudcEnumerate (
  VOID
  )
{
  EFI_STATUS e;
  UINT32 reg_data;
  XUSB_DEVICE_CONTEXT *p_xusb_dev_context = &s_xusb_device_context;
  static UINT8 do_once;
  UINT32 idx = 0;

  if (do_once == 0U) {
    /* Make interrupt moderation =0 to avoid delay between back
     * 2 back interrrupts.
     */
    reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_RT_IMOD_0);
    reg_data = NV_FLD_SET_DRF_NUM (
                 XUSB_DEV_XHCI,
                 RT_IMOD,
                 IMODI,
                 0,
                 reg_data
                 );
    MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_RT_IMOD_0, reg_data);

    reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTHALT_0);
    reg_data = NV_FLD_SET_DRF_NUM (
                 XUSB_DEV_XHCI,
                 PORTHALT,
                 HALT_LTSSM,
                 0,
                 reg_data
                 );
    MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTHALT_0, reg_data);

    /* Write Enable for device mode. */
    reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_CTRL_0);
    reg_data = NV_FLD_SET_DRF_NUM (
                 XUSB_DEV_XHCI,
                 CTRL,
                 ENABLE,
                 1,
                 reg_data
                 );
    MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_CTRL_0, reg_data);

    /* Force port reg */
    reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_CFG_DEV_FE_0);
    reg_data = NV_FLD_SET_DRF_NUM (
                 XUSB_DEV_XHCI,
                 CFG_DEV_FE,
                 PORTREGSEL,
                 2,
                 reg_data
                 );
    MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_CFG_DEV_FE_0, reg_data);

    reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTSC_0);
    reg_data = NV_FLD_SET_DRF_NUM (
                 XUSB_DEV_XHCI,
                 PORTSC,
                 LWS,
                 1,
                 reg_data
                 );
    reg_data = NV_FLD_SET_DRF_NUM (
                 XUSB_DEV_XHCI,
                 PORTSC,
                 PLS,
                 5,                  /* RxDetect */
                 reg_data
                 );
    MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_PORTSC_0, reg_data);

    reg_data = MmioRead32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_CFG_DEV_FE_0);
    reg_data = NV_FLD_SET_DRF_NUM (
                 XUSB_DEV_XHCI,
                 CFG_DEV_FE,
                 PORTREGSEL,
                 0,
                 reg_data
                 );
    MmioWrite32 (mPrivate->XudcBaseAddress + XUSB_DEV_XHCI_CFG_DEV_FE_0, reg_data);

    p_xusb_dev_context->device_state    = DEFAULT;
    p_xusb_dev_context->wait_for_eventt = SETUP_EVENT_TRB;
    do_once++;
  }

  while (p_xusb_dev_context->device_state != CONFIGURED && idx < 50) {
    e = XudcPollForEvent (0x100UL);
    if (e != EFI_SUCCESS) {
      DEBUG ((DEBUG_ERROR, "Poll Evt fail\n"));
      return e;
    } else {
      DEBUG ((DEBUG_VERBOSE, "Poll Evt done, device_state: %d\n", p_xusb_dev_context->device_state));
    }

    idx++;
  }

  return EFI_SUCCESS;
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
  EFI_STATUS                    Status;
  NON_DISCOVERABLE_DEVICE       *Device;
  EFI_PHYSICAL_ADDRESS          BaseAddress;
  UINTN                         RegionSize;
  XUDC_CONTROLLER_PRIVATE_DATA  *Private;
  UINT32                        Index;
  NVIDIA_POWER_GATE_NODE_PROTOCOL  *PgProtocol;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingStart:
      Status = gBS->HandleProtocol (
                      ControllerHandle,
                      &gNVIDIANonDiscoverableDeviceProtocolGuid,
                      (VOID **)&Device
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed to get non discoverable protocol"));
        return Status;
      }

      // Force non-coherent DMA type device.
      Device->DmaType = NonDiscoverableDeviceDmaTypeNonCoherent;

      BaseAddress = 0;
      Private     = NULL;
      Private     = AllocateZeroPool (sizeof (XUDC_CONTROLLER_PRIVATE_DATA));
      if (Private == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      if (CompareGuid (Device->Type, &gNVIDIANonDiscoverableXudcDeviceGuid)) {
        Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
        if (EFI_ERROR (Status)) {
          goto ErrorExit;
        }

        Private->XudcBaseAddress = BaseAddress;

        Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 1, &BaseAddress, &RegionSize);
        if (EFI_ERROR (Status)) {
          goto ErrorExit;
        }

        Private->FpciBaseAddress = BaseAddress;
        DEBUG ((DEBUG_VERBOSE, "XUDC FPCI base: 0x%lu\n", BaseAddress));
      }

      Private->ControllerHandle = ControllerHandle;

      Status = gBS->LocateProtocol (
                      &gNVIDIAUsbPadCtlProtocolGuid,
                      NULL,
                      (VOID **)&(Private->mUsbPadCtlProtocol)
                      );
      if (EFI_ERROR (Status) || (Private->mUsbPadCtlProtocol == NULL)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Couldn't find UsbPadCtl Protocol Handle %r\n",
          __FUNCTION__,
          Status
          ));
        goto ErrorExit;
      }

      if (Private->mUsbPadCtlProtocol->InitDevHw == NULL) {
        // Do not support the Xudc driver for class transfer, ex: fastboot
        return EFI_SUCCESS;
      }

      Status = gBS->HandleProtocol (ControllerHandle, &gNVIDIAPowerGateNodeProtocolGuid, (VOID **)&PgProtocol);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "PowerGateNodeProtocol not found\r\n"));
        goto ErrorExit;
      }

      for (Index = 0; Index < PgProtocol->NumberOfPowerGates; Index++) {
        DEBUG ((DEBUG_VERBOSE, "Deassert pg: %d\r\n", PgProtocol->PowerGateId[Index]));
        Status = PgProtocol->Deassert (PgProtocol, PgProtocol->PowerGateId[Index]);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "Deassert pg not found\r\n"));
          goto ErrorExit;
        }
      }

      // Powergate XUSBA/XUSBB partition again to make it in default state
      for (Index = 0; Index < PgProtocol->NumberOfPowerGates; Index++) {
        DEBUG ((DEBUG_VERBOSE, "Assert pg: %d\r\n", PgProtocol->PowerGateId[Index]));
        Status = PgProtocol->Assert (PgProtocol, PgProtocol->PowerGateId[Index]);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "Assert pg not found\r\n"));
          goto ErrorExit;
        }
      }

      // Only unpowergate XUSBA/XUSBB in XHCI DT
      for (Index = 0; Index < PgProtocol->NumberOfPowerGates; Index++) {
        DEBUG ((DEBUG_VERBOSE, "Deassert pg: %d\r\n", PgProtocol->PowerGateId[Index]));
        Status = PgProtocol->Deassert (PgProtocol, PgProtocol->PowerGateId[Index]);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "Deassert pg not found\r\n"));
          goto ErrorExit;
        }
      }

      // allocate dma memories
      Status = DmaAllocateAlignedBuffer (EfiBootServicesData, EFI_SIZE_TO_PAGES (SETUP_DATA_SIZE), 64, (VOID **)&Private->usb_setup_data);
      if (EFI_ERROR (Status)) {
        goto ErrorExit;
      }

      Status = DmaAllocateAlignedBuffer (EfiBootServicesData, EFI_SIZE_TO_PAGES (EVENT_RING_SIZE), 64, (VOID **)&Private->pEventRing);
      if (EFI_ERROR (Status)) {
        goto ErrorExit;
      }

      Status = DmaAllocateAlignedBuffer (EfiBootServicesData, EFI_SIZE_TO_PAGES (TX_RING_EP0_SIZE), 64, (VOID **)&Private->pTxRingEP0);
      if (EFI_ERROR (Status)) {
        goto ErrorExit;
      }

      Status = DmaAllocateAlignedBuffer (EfiBootServicesData, EFI_SIZE_TO_PAGES (TX_RING_EP1_OUT_SIZE), 64, (VOID **)&Private->pTxRingEP1Out);
      if (EFI_ERROR (Status)) {
        goto ErrorExit;
      }

      Status = DmaAllocateAlignedBuffer (EfiBootServicesData, EFI_SIZE_TO_PAGES (TX_RING_EP1_IN_SIZE), 64, (VOID **)&Private->pTxRingEP1In);
      if (EFI_ERROR (Status)) {
        goto ErrorExit;
      }

      Status = DmaAllocateAlignedBuffer (EfiBootServicesData, EFI_SIZE_TO_PAGES (SETUP_DATA_BUFFER_SIZE), 64, (VOID **)&Private->pSetupBuffer);
      if (EFI_ERROR (Status)) {
        goto ErrorExit;
      }

      Status = DmaAllocateAlignedBuffer (EfiBootServicesData, EFI_SIZE_TO_PAGES (EP_CONTEXT_SIZE), 64, (VOID **)&Private->pEPContext);
      if (EFI_ERROR (Status)) {
        goto ErrorExit;
      }

      Status = DmaAllocateAlignedBuffer (EfiBootServicesData, EFI_SIZE_TO_PAGES (TDATA_SIZE), 64, (VOID **)&Private->tdata);
      if (EFI_ERROR (Status)) {
        goto ErrorExit;
      }

      Status = gBS->CreateEventEx (
                      EVT_NOTIFY_SIGNAL,
                      TPL_NOTIFY,
                      OnExitBootServices,
                      Private,
                      &gEfiEventExitBootServicesGuid,
                      &Private->ExitBootServicesEvent
                      );
      if (EFI_ERROR (Status)) {
        goto ErrorExit;
      }

      Private->XudcControllerProtocol.XudcStart       = XudcUsbDeviceStart;
      Private->XudcControllerProtocol.XudcSend        = XudcUsbDeviceSend;
      Private->XudcControllerProtocol.XudcSetRxLength = XudcSetRxLength;

      Status = gBS->InstallMultipleProtocolInterfaces (
                      &ControllerHandle,
                      &gNVIDIAXudcControllerProtocolGuid,
                      &Private->XudcControllerProtocol,
                      NULL
                      );

      if (EFI_ERROR (Status)) {
        goto ErrorExit;
      }

      Private->TotalRxLength   = 0;
      Private->CurrentRxLength = 0;

      mPrivate = Private;

      break;

    default:
      break;
  }

  return EFI_SUCCESS;
ErrorExit:
  FreePool (Private);
  return Status;
}
