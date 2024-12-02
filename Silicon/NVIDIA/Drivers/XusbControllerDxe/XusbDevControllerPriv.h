/** @file

  XUDC Controller Driver private structures

  SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef XUSB_DEV_CONTROLLER_PRIV_H
#define XUSB_DEV_CONTROLLER_PRIV_H

#ifndef MK_SHIFT_CONST
#define MK_SHIFT_CONST(_constant_)  (_constant_ ## U)
#endif
#ifndef MK_MASK_CONST
#define MK_MASK_CONST(_constant_)  (_constant_ ## U)
#endif
#ifndef MK_ENUM_CONST
#define MK_ENUM_CONST(_constant_)  (_constant_ ## U)
#endif
#ifndef MK_ADDR_CONST
#define MK_ADDR_CONST(_constant_)  (_constant_ ## U)
#endif

/* XUSB DEV FPCI Reg */
#define XUSB_DEV_CFG_1_0                     MK_ADDR_CONST(0x00000004)
#define XUSB_DEV_CFG_1_0_MEMORY_SPACE_RANGE  1:1
#define XUSB_DEV_CFG_1_0_BUS_MASTER_RANGE    2:2
#define XUSB_DEV_CFG_4_0                     MK_ADDR_CONST(0x00000010)

/* XUSB DEV Reg */
#define XUSB_DEV_XHCI_DB_0                 MK_ADDR_CONST(0x00000004)
#define XUSB_DEV_XHCI_DB_0_TARGET_SHIFT    MK_SHIFT_CONST(8)
#define XUSB_DEV_XHCI_DB_0_TARGET_FIELD    (MK_SHIFT_CONST(0xff) << XUSB_DEV_XHCI_DB_0_TARGET_SHIFT)
#define XUSB_DEV_XHCI_DB_0_STREAMID_RANGE  31:16

#define XUSB_DEV_XHCI_ERSTSZ_0                MK_ADDR_CONST(0x00000008)
#define XUSB_DEV_XHCI_ERSTSZ_0_ERST0SZ_RANGE  15:0
#define XUSB_DEV_XHCI_ERSTSZ_0_ERST1SZ_RANGE  31:16

#define XUSB_DEV_XHCI_ERST0BALO_0  MK_ADDR_CONST(0x00000010)

#define XUSB_DEV_XHCI_ERST0BAHI_0  MK_ADDR_CONST(0x00000014)

#define XUSB_DEV_XHCI_ERST1BALO_0  MK_ADDR_CONST(0x00000018)

#define XUSB_DEV_XHCI_ERST1BAHI_0  MK_ADDR_CONST(0x0000001C)

#define XUSB_DEV_XHCI_ERDPLO_0               MK_ADDR_CONST(0x00000020)
#define XUSB_DEV_XHCI_ERDPLO_0_EHB_SHIFT     MK_SHIFT_CONST(3)
#define XUSB_DEV_XHCI_ERDPLO_0_EHB_FIELD     (MK_SHIFT_CONST(0x1) << XUSB_DEV_XHCI_ERDPLO_0_EHB_SHIFT)
#define XUSB_DEV_XHCI_ERDPLO_0_EHB_RANGE     3:3
#define XUSB_DEV_XHCI_ERDPLO_0_ADDRLO_RANGE  31:4

#define XUSB_DEV_XHCI_ERDPHI_0  MK_ADDR_CONST(0x00000024)

#define XUSB_DEV_XHCI_EREPLO_0               MK_ADDR_CONST(0x00000028)
#define XUSB_DEV_XHCI_EREPLO_0_ECS_RANGE     0:0
#define XUSB_DEV_XHCI_EREPLO_0_SEGI_RANGE    1:1
#define XUSB_DEV_XHCI_EREPLO_0_ADDRLO_SHIFT  MK_SHIFT_CONST(4)
#define XUSB_DEV_XHCI_EREPLO_0_ADDRLO_FIELD  (MK_SHIFT_CONST(0xfffffff) << XUSB_DEV_XHCI_EREPLO_0_ADDRLO_SHIFT)
#define XUSB_DEV_XHCI_EREPLO_0_ADDRLO_RANGE  31:4

#define XUSB_DEV_XHCI_EREPHI_0  MK_ADDR_CONST(0x0000002C)

#define XUSB_DEV_XHCI_CTRL_0               MK_ADDR_CONST(0x00000030)
#define XUSB_DEV_XHCI_CTRL_0_RUN_SHIFT     MK_SHIFT_CONST(0)
#define XUSB_DEV_XHCI_CTRL_0_RUN_FIELD     (MK_SHIFT_CONST(0x1) << XUSB_DEV_XHCI_CTRL_0_RUN_SHIFT)
#define XUSB_DEV_XHCI_CTRL_0_RUN_STOP      MK_ENUM_CONST(0x00000000)
#define XUSB_DEV_XHCI_CTRL_0_RUN_RUN       MK_ENUM_CONST(0x00000001)
#define XUSB_DEV_XHCI_CTRL_0_LSE_SHIFT     MK_SHIFT_CONST(1)
#define XUSB_DEV_XHCI_CTRL_0_LSE_FIELD     (MK_SHIFT_CONST(0x1) << XUSB_DEV_XHCI_CTRL_0_LSE_SHIFT)
#define XUSB_DEV_XHCI_CTRL_0_LSE_EN        MK_ENUM_CONST(0x00000001)
#define XUSB_DEV_XHCI_CTRL_0_DEVADR_RANGE  30:24
#define XUSB_DEV_XHCI_CTRL_0_ENABLE_RANGE  31:31

#define XUSB_DEV_XHCI_ST_0           MK_ADDR_CONST(0x00000034)
#define XUSB_DEV_XHCI_ST_0_RC_SHIFT  MK_SHIFT_CONST(0)
#define XUSB_DEV_XHCI_ST_0_RC_FIELD  (MK_SHIFT_CONST(0x1) << XUSB_DEV_XHCI_ST_0_RC_SHIFT)
#define XUSB_DEV_XHCI_ST_0_RC_CLEAR  MK_ENUM_CONST(0x00000001)
#define XUSB_DEV_XHCI_ST_0_IP_SHIFT  MK_SHIFT_CONST(4)
#define XUSB_DEV_XHCI_ST_0_IP_FIELD  (MK_SHIFT_CONST(0x1) << XUSB_DEV_XHCI_ST_0_IP_SHIFT)
#define XUSB_DEV_XHCI_ST_0_IP_RANGE  4:4

#define XUSB_DEV_XHCI_PORTSC_0            MK_ADDR_CONST(0x0000003C)
#define XUSB_DEV_XHCI_PORTSC_0_CCS_SHIFT  MK_SHIFT_CONST(0)
#define XUSB_DEV_XHCI_PORTSC_0_CCS_FIELD  (MK_SHIFT_CONST(0x1) << XUSB_DEV_XHCI_PORTSC_0_CCS_SHIFT)
#define XUSB_DEV_XHCI_PORTSC_0_PR_SHIFT   MK_SHIFT_CONST(4)
#define XUSB_DEV_XHCI_PORTSC_0_PR_FIELD   (MK_SHIFT_CONST(0x1) << XUSB_DEV_XHCI_PORTSC_0_PR_SHIFT)
#define XUSB_DEV_XHCI_PORTSC_0_PLS_SHIFT  MK_SHIFT_CONST(5)
#define XUSB_DEV_XHCI_PORTSC_0_PLS_FIELD  (MK_SHIFT_CONST(0xf) << XUSB_DEV_XHCI_PORTSC_0_PLS_SHIFT)
#define XUSB_DEV_XHCI_PORTSC_0_PLS_RANGE  8:5
#define XUSB_DEV_XHCI_PORTSC_0_PS_SHIFT   MK_SHIFT_CONST(10)
#define XUSB_DEV_XHCI_PORTSC_0_PS_FIELD   (MK_SHIFT_CONST(0xf) << XUSB_DEV_XHCI_PORTSC_0_PS_SHIFT)
#define XUSB_DEV_XHCI_PORTSC_0_LWS_RANGE  16:16
#define XUSB_DEV_XHCI_PORTSC_0_CSC_SHIFT  MK_SHIFT_CONST(17)
#define XUSB_DEV_XHCI_PORTSC_0_CSC_FIELD  (MK_SHIFT_CONST(0x1) << XUSB_DEV_XHCI_PORTSC_0_CSC_SHIFT)
#define XUSB_DEV_XHCI_PORTSC_0_CSC_RANGE  17:17
#define XUSB_DEV_XHCI_PORTSC_0_WRC_SHIFT  MK_SHIFT_CONST(19)
#define XUSB_DEV_XHCI_PORTSC_0_WRC_FIELD  (MK_SHIFT_CONST(0x1) << XUSB_DEV_XHCI_PORTSC_0_WRC_SHIFT)
#define XUSB_DEV_XHCI_PORTSC_0_WRC_RANGE  19:19
#define XUSB_DEV_XHCI_PORTSC_0_PRC_SHIFT  MK_SHIFT_CONST(21)
#define XUSB_DEV_XHCI_PORTSC_0_PRC_FIELD  (MK_SHIFT_CONST(0x1) << XUSB_DEV_XHCI_PORTSC_0_PRC_SHIFT)
#define XUSB_DEV_XHCI_PORTSC_0_PRC_RANGE  21:21
#define XUSB_DEV_XHCI_PORTSC_0_PLC_SHIFT  MK_SHIFT_CONST(22)
#define XUSB_DEV_XHCI_PORTSC_0_PLC_FIELD  (MK_SHIFT_CONST(0x1) << XUSB_DEV_XHCI_PORTSC_0_PLC_SHIFT)
#define XUSB_DEV_XHCI_PORTSC_0_PLC_RANGE  22:22
#define XUSB_DEV_XHCI_PORTSC_0_CEC_SHIFT  MK_SHIFT_CONST(23)
#define XUSB_DEV_XHCI_PORTSC_0_CEC_FIELD  (MK_SHIFT_CONST(0x1) << XUSB_DEV_XHCI_PORTSC_0_CEC_SHIFT)
#define XUSB_DEV_XHCI_PORTSC_0_CEC_RANGE  23:23
#define XUSB_DEV_XHCI_PORTSC_0_WPR_SHIFT  MK_SHIFT_CONST(30)
#define XUSB_DEV_XHCI_PORTSC_0_WPR_FIELD  (MK_SHIFT_CONST(0x1) << XUSB_DEV_XHCI_PORTSC_0_WPR_SHIFT)

#define XUSB_DEV_XHCI_RT_IMOD_0              MK_ADDR_CONST(0x00000038)
#define XUSB_DEV_XHCI_RT_IMOD_0_IMODI_RANGE  15:0

#define XUSB_DEV_XHCI_ECPLO_0  MK_ADDR_CONST(0x00000040)

#define XUSB_DEV_XHCI_ECPHI_0  MK_ADDR_CONST(0x00000044)

#define XUSB_DEV_XHCI_PORTPM_0  MK_ADDR_CONST(0x0000004C)

#define XUSB_DEV_XHCI_EP_HALT_0  MK_ADDR_CONST(0x00000050)

#define XUSB_DEV_XHCI_EP_PAUSE_0  MK_ADDR_CONST(0x00000054)

#define XUSB_DEV_XHCI_EP_RELOAD_0            MK_ADDR_CONST(0x00000058)
#define XUSB_DEV_XHCI_EP_RELOAD_0_DCI_RANGE  31:0

#define XUSB_DEV_XHCI_EP_STCHG_0  MK_ADDR_CONST(0x0000005C)

#define XUSB_DEV_XHCI_PORTHALT_0                      MK_ADDR_CONST(0x0000006C)
#define XUSB_DEV_XHCI_PORTHALT_0_HALT_LTSSM_RANGE     0:0
#define XUSB_DEV_XHCI_PORTHALT_0_STCHG_REQ_SHIFT      MK_SHIFT_CONST(20)
#define XUSB_DEV_XHCI_PORTHALT_0_STCHG_REQ_FIELD      (MK_SHIFT_CONST(0x1) << XUSB_DEV_XHCI_PORTHALT_0_STCHG_REQ_SHIFT)
#define XUSB_DEV_XHCI_PORTHALT_0_STCHG_INTR_EN_RANGE  24:24

#define XUSB_DEV_XHCI_HSFSPI_COUNT0_0  MK_ADDR_CONST(0x00000100)

#define XUSB_DEV_XHCI_HSFSPI_COUNT16_0                   MK_ADDR_CONST(0x0000019C)
#define XUSB_DEV_XHCI_HSFSPI_COUNT16_0_CHIRP_FAIL_RANGE  29:0

#define XUSB_DEV_XHCI_CFG_DEV_FE_0                   MK_ADDR_CONST(0x0000085C)
#define XUSB_DEV_XHCI_CFG_DEV_FE_0_PORTREGSEL_RANGE  3:0

#define NV_DRF_SHIFT(d, r, f)      ((UINT32)(d##_##r##_0_##f##_SHIFT))
#define NV_DRF_SHIFTMASK(d, r, f)  ((UINT32)(d##_##r##_0_##f##_FIELD))
#define SHIFT(PERIPH, REG, FIELD) \
        NV_DRF_SHIFT(PERIPH, REG, FIELD)
#define SHIFTMASK(PERIPH, REG, FIELD) \
        NV_DRF_SHIFTMASK(PERIPH, REG, FIELD)
#define NV_DRF_MASK(d, r, f)    (NV_DRF_SHIFTMASK(d, r, f) >> NV_DRF_SHIFT(d, r, f))
#define NV_DRF_VAL(d, r, f, v)  ((((UINT32)(v)) >> NV_DRF_SHIFT(d, r, f)) & NV_DRF_MASK(d, r, f))
#define NV_FIELD_MASK(x)        (0xFFFFFFFFUL>>(31-((1?x)%32)+((0?x)%32)))
#define NV_FIELD_SHIFT(x)       ((0?x)%32)
#define NV_DRF_DEF(d, r, f, c)  (((UINT32)(d##_##r##_0_##f##_##c)) << NV_DRF_SHIFT(d, r, f))

#define NV_FIELD_SHIFTMASK(x)  (NV_FIELD_MASK(x)<< (NV_FIELD_SHIFT(x)))

#define NV_FLD_SET_DRF_DEF(d, r, f, c, v) \
    ((((UINT32)(v)) & ~NV_DRF_SHIFTMASK(d, r, f)) | NV_DRF_DEF(d, r, f, c))

#define NV_DRF_NUM(d, r, f, n) \
    (((n)& NV_FIELD_MASK(d##_##r##_0_##f##_RANGE)) << \
        NV_FIELD_SHIFT(d##_##r##_0_##f##_RANGE))

#define NV_FLD_SET_DRF_NUM(d, r, f, n, v) \
    (((v) & ~NV_FIELD_SHIFTMASK(d##_##r##_0_##f##_RANGE)) | NV_DRF_NUM(d,r,f,n))

/* Desc macros */
#define USB_DEV_DESCRIPTOR_SIZE    18
#define USB_BOS_DESCRIPTOR_SIZE    22
#define USB_MANF_STRING_LENGTH     26
#define USB_PRODUCT_STRING_LENGTH  8
#define USB_SERIAL_NUM_LENGTH      12
#define USB_LANGUAGE_ID_LENGTH     4
#define USB_DEV_QUALIFIER_LENGTH   10
#define USB_DEV_STATUS_LENGTH

#define USB_DEVICE_SELF_POWERED  1

/* Feature Select */
#define ENDPOINT_HALT         0
#define DEVICE_REMOTE_WAKEUP  1
#define TEST_MODE             2
/* USB 3.0 defined */
#define U1_ENABLE   48
#define U2_ENABLE   49
#define LTM_ENABLE  50

/* USB Setup Packet Byte Offsets */
#define USB_SETUP_REQUEST_TYPE  0
#define USB_SETUP_REQUEST       1
#define USB_SETUP_VALUE         2
#define USB_SETUP_DESCRIPTOR    3
#define USB_SETUP_INDEX         4
#define USB_SETUP_LENGTH        6

/* USB Setup Packet Request Type */
#define HOST2DEV_DEVICE     0x00
#define HOST2DEV_INTERFACE  0x01
#define HOST2DEV_ENDPOINT   0x02
#define DEV2HOST_DEVICE     0x80
#define DEV2HOST_INTERFACE  0x81
#define DEV2HOST_ENDPOINT   0x82

/*  USB Setup Packet Request */
#define GET_STATUS         0
#define CLEAR_FEATURE      1
#define SET_FEATURE        3
#define SET_ADDRESS        5
#define GET_DESCRIPTOR     6
#define SET_DESCRIPTOR     7
#define GET_CONFIGURATION  8
#define SET_CONFIGURATION  9
#define GET_INTERFACE      10
#define SET_INTERFACE      11
#define SYNCH_FRAME        12
#define SET_SEL            48
#define SET_ISOCH_DELAY    49

/* USB Descriptor Type */
#define USB_DT_DEVICE                 1
#define USB_DT_CONFIG                 2
#define USB_DT_STRING                 3
#define USB_DT_INTERFACE              4
#define USB_DT_ENDPOINT               5
#define USB_DT_DEVICE_QUALIFIER       6
#define USB_DT_OTHER_SPEED_CONFIG     7
#define USB_DT_INTERFACE_POWER        8
#define USB_DT_INTERFACE_ASSOCIATION  11
#define USB_DT_BOS                    15
#define USB_DT_DEVICE_CAPABILITY      16
#define USB_DT_SS_USB_EP_COMPANION    48

#define BCDUSB_VERSION_LSB  0
#define BCDUSB_VERSION_MSB  2

#define BCDUSB3_VERSION_LSB  0
#define BCDUSB3_VERSION_MSB  3
#define EP0_PKT_SIZE         9

/* Misc macros */
#define EP_RUNNING   1
#define EP_DISABLED  0

#define SETUP_PACKET_BUFFER_NUM  2

#define DIR_OUT  0
#define DIR_IN   1U

/* descriptor gets only 4 low bits */
#define NVTBOOT_USBF_DESCRIPTOR_SKU_MASK  0xF

/* Usb speed */
#define XUSB_FULL_SPEED   1U
#define XUSB_HIGH_SPEED   3U
#define XUSB_SUPER_SPEED  4U

/* EndPoint types */
#define EP_TYPE_CNTRL     4
#define EP_TYPE_BULK_OUT  2
#define EP_TYPE_BULK_IN   6

/* TRB Types */
#define NONE_TRB                0
#define NORMAL_TRB              1
#define DATA_STAGE_TRB          3U
#define STATUS_STAGE_TRB        4U
#define LINK_TRB                6U
#define TRANSFER_EVENT_TRB      32U
#define PORT_STATUS_CHANGE_TRB  34U
#define SETUP_EVENT_TRB         63U

/* Error codes */
#define TRB_ERR_CODE           5
#define SUCCESS_ERR_CODE       1U
#define DATA_BUF_ERR_CODE      2
#define SHORT_PKT_ERR_CODE     13U
#define CTRL_SEQ_NUM_ERR_CODE  223U
#define CTRL_DIR_ERR_CODE      222U

/* XUSB speed */
#define XUSB_SUPERSPEED  0x4
#define XUSB_HIGHSPEED   0x3
#define XUSB_FULLSPEED   0x2

/* endpoint number */
#define  EP0_IN   0U        /* Bi-directional */
#define  EP0_OUT  1U        /* Note: This is not used. */
#define  EP1_OUT  2U
#define  EP1_IN   3U
#define  EPX_MAX  0xFFFFU   /* half word size */

// #define USB_IN 0U
// #define USB_OUT 1U

/**
 * @brief Defines the device state
*/
/* macro device state */
typedef UINT32 device_state_t;
#define DEFAULT                    0U
#define CONNECTED                  1U
#define DISCONNECTED               2U
#define RESET                      3U
#define ADDRESSED_STATUS_PENDING   4U
#define ADDRESSED                  5U
#define CONFIGURED_STATUS_PENDING  6U
#define CONFIGURED                 7U
#define SUSPENDED                  8U

/**
 * @brief USB function interface structure
 */
typedef struct {
  /* As Producer (of Control TRBs) EP0_IN*/
  UINT64            cntrl_epenqueue_ptr;
  UINT64            cntrl_epdequeue_ptr;
  UINT32            cntrl_pcs; /* Producer Cycle State */
  /* As Producer (of Transfer TRBs) for EP1_OUT*/
  UINT64            bulkout_epenqueue_ptr;
  UINT64            bulkout_epdequeue_ptr;
  UINT32            bulkout_pcs; /* Producer Cycle State */
  /* As Producer (of Transfer TRBs) for EP1_IN*/
  UINT64            bulkin_epenqueue_ptr;
  UINT64            bulkin_epdequeue_ptr;
  UINT32            bulkin_pcs; /* Producer Cycle State */
  /* As Consumer (of Event TRBs) */
  UINT64            event_enqueue_ptr;
  UINT64            event_dequeue_ptr;
  UINT32            event_ccs;                 /* Consumer Cycle State */
  UINT64            dma_er_start_address;      /* DMA addr for endpoint ring start ptr*/
  UINT64            dma_ep_context_start_addr; /* DMA addr for ep context start ptr*/
  device_state_t    device_state;
  UINT32            initialized;
  UINT32            enumerated;
  UINT32            bytes_txfred;
  UINT32            tx_count;
  UINT32            cntrl_seq_num;
  UINT32            setup_pkt_index;
  UINT32            config_num;
  UINT32            interface_num;
  UINT32            wait_for_eventt;
  UINT32            port_speed;
} XUSB_DEVICE_CONTEXT;

/**
 * @brief Template used to parse event TRBs
 */
typedef struct {
  UINT32    rsvd0;
  UINT32    rsvd1;
  /* DWord2 begin */
  UINT32    rsvd2     : 24;
  UINT32    comp_code : 8;      /* Completion Code */
  /* DWord2 end */
  UINT32    c         : 1;   /* Cycle Bit */
  UINT32    rsvd3     : 9;
  UINT32    trb_type  : 6;    /* Identifies the type of TRB */
  /* (Setup Event TRB, Port status Change TRB etc) */
  UINT32    emp_id    : 5;       /* Endpoint ID. */
  UINT32    rsvd4     : 11;
  /* DWord3 end */
} EVENT_TRB_STRUCT;

/**
 * @brief Defines the structure for Setup Event TRB
 */
typedef struct {
  UINT32    data[2];
  /* DWord2 begin */
  UINT32    ctrl_seq_num : 16;     /* Control Sequence Number */
  UINT32    rsvddw2_0    : 8;
  UINT32    comp_code    : 8;     /* Completion Code */
  /* DWord2 end */
  UINT32    c            : 1;    /* Cycle bit */
  UINT32    rsvddw3_0    : 9;
  UINT32    trb_type     : 6;   /* TrbType = 63 for Setup Event TRB */
  UINT32    emp_id       : 5;   /* Endpoint ID. */
  UINT32    rsvddw3_1    : 11;
  /* DWord3 end */
} SETUP_EVENT_TRB_STRUCT;

/**
 * @brief Defines the structure for status TRB
 */
typedef struct {
  UINT32    rsvdw0;
  UINT32    rsvddw1;
  /* DWord2 begin */
  UINT32    rsvddw2_0  : 22;
  UINT32    int_target : 10;  /* Interrupter Target */
  /* DWord2 end */
  UINT32    c          : 1; /* Cycle bit */
  UINT32    ent        : 1; /* Evaluate Next TRB */
  UINT32    rsvddw3_0  : 2;
  UINT32    ch         : 1; /* Chain bit */
  UINT32    ioc        : 1; /* Interrupt on Completion */
  UINT32    rsvd4      : 4;
  UINT32    trb_type   : 6;
  UINT32    dir        : 1;
  UINT32    rsvddw3_1  : 15;
  /* DWord3 end */
} STATUS_TRB_STRUCT;

/**
 * @brief Defines the structure for data status TRB
 */
typedef struct {
  UINT32    databufptr_lo;
  UINT32    databufptr_hi;
  /* DWord2 begin */
  UINT32    trb_tx_len : 17;
  UINT32    tdsize     : 5;
  UINT32    int_target : 10;
  /* DWord2 end */
  UINT32    c          : 1; /* Cycle bit */
  UINT32    ent        : 1; /* Evaluate Next TRB */
  UINT32    isp        : 1; /* Interrupt on Short Packet */
  UINT32    ns         : 1; /* No Snoop */
  UINT32    ch         : 1; /* Chain bit */
  UINT32    ioc        : 1; /* Interrupt on Completion */
  UINT32    rsvddw3_0  : 4;
  UINT32    trb_type   : 6;
  UINT32    dir        : 1;
  UINT32    rsvddw3_1  : 15;
  /* DWord3 end */
} DATA_TRB_STRUCT;

/**
 * @brief Defines the structure for Normal TRB
 */
typedef struct {
  UINT32    databufptr_lo;
  UINT32    databufptr_hi;
  /* DWord2 begin */
  UINT32    trb_tx_len : 17;
  UINT32    tdsize     : 5;
  UINT32    int_target : 10;
  /* DWord2 end */
  UINT32    c          : 1; /* Cycle bit */
  UINT32    ent        : 1; /* Evaluate Next TRB */
  UINT32    isp        : 1; /* Interrupt on Short Packet */
  UINT32    ns         : 1; /* No Snoop */
  UINT32    ch         : 1; /* Chain bit */
  UINT32    ioc        : 1; /* Interrupt on Completion */
  UINT32    idt        : 1; /* Immediate data */
  UINT32    rsvddw3_0  : 2;
  UINT32    bei        : 1; /* Block Event Interrupt */
  UINT32    trb_type   : 6;
  UINT32    rsvddw3_1  : 16;
  /* DWord3 end */
} NORMAL_TRB_STRUCT;

/**
 * @brief Defines the structure for Transfer event TRB
 */
typedef struct {
  UINT32    trb_pointer_lo;
  UINT32    trb_pointer_hi;
  /* DWord1 end */
  UINT32    trb_tx_len : 24;
  UINT32    comp_code  : 8;     /* Completion Code */
  /* DWord2 end */
  UINT32    c          : 1;  /* Cycle Bit */
  UINT32    rsvddw3_0  : 1;
  UINT32    ed         : 1; /* Event data. (Immediate data in 1st 2 words or not) */
  UINT32    rsvddw3_1  : 7;
  UINT32    trb_type   : 6;   /* Identifies the type of TRB */
  /* (Setup Event TRB, Port status Change TRB etc) */
  UINT32    emp_id     : 5;      /* Endpoint ID. */
  UINT32    rsvddw3_2  : 11;
  /* DWord3 end */
} TRANSFER_EVENT_TRB_STRUCT;

/**
 * @brief Defines the structure for Link TRB
 */
typedef struct {
  /* DWORD0 */
  UINT32    rsvddW0_0      : 4;
  UINT32    ring_seg_ptrlo : 28;
  /* DWORD1 */
  UINT32    ring_seg_ptrhi;
  /* DWORD2 */
  UINT32    rsvddw2_0      : 22;
  UINT32    int_target     : 10;
  /* DWORD3 */
  UINT32    c              : 1;
  UINT32    tc             : 1;
  UINT32    rsvddw3_0      : 2;
  UINT32    ch             : 1;
  UINT32    ioc            : 1;
  UINT32    rsvddw3_1      : 4;
  UINT32    trb_type       : 6;
  UINT32    rsvddw3_2      : 16;
} LINK_TRB_STRUCT;

/**
 * @brief Defines the structure for endpoint context
 */
typedef struct {
  /* DWORD0 */
  UINT32    ep_state             : 3;
  UINT32    rsvddW0_0            : 5;
  UINT32    mult                 : 2;
  UINT32    max_pstreams         : 5;
  UINT32    lsa                  : 1;
  UINT32    interval             : 8;
  UINT32    rsvddW0_1            : 8;
  /* DWORD1 */
  UINT32    rsvddw1_0            : 1;
  UINT32    cerr                 : 2;
  UINT32    ep_type              : 3;
  UINT32    rsvddw1_1            : 1;
  UINT32    hid                  : 1;
  UINT32    max_burst_size       : 8;
  UINT32    max_packet_size      : 16;
  /* DWORD2 */
  UINT32    dcs                  : 1;
  UINT32    rsvddw2_0            : 3;
  UINT32    trd_dequeueptr_lo    : 28;
  /* DWORD3 */
  UINT32    trd_dequeueptr_hi;
  /* DWORD4 */
  UINT32    avg_trb_len          : 16;
  UINT32    max_esit_payload     : 16;
  /******* Nvidia specific from here ******/
  /* DWORD5 */
  UINT32    event_data_txlen_acc : 24;
  UINT32    rsvddw5_0            : 1;
  UINT32    ptd                  : 1;
  UINT32    sxs                  : 1;
  UINT32    seq_num              : 5;
  /* DWORD6 */
  UINT32    cprog                : 8;
  UINT32    sbyte                : 7;
  UINT32    tp                   : 2;
  UINT32    rec                  : 1;
  UINT32    cec                  : 2;
  UINT32    ced                  : 1;
  UINT32    hsp1                 : 1;
  UINT32    rty1                 : 1;
  UINT32    std                  : 1;
  UINT32    status               : 8;
  /* DWORD7 */
  UINT32    data_offset          : 17;
  UINT32    rsvddw6_0            : 4;
  UINT32    lpa                  : 1;
  UINT32    num_trb              : 5;
  UINT32    num_p                : 5;
  /* DWORD8 */
  UINT32    scratch_pad0;
  /* DWORD8 */
  UINT32    scratch_pad1;
  /* DWORD10 */
  UINT32    cping                : 8;
  UINT32    sping                : 8;
  UINT32    tc                   : 2;
  UINT32    ns                   : 1;
  UINT32    ro                   : 1;
  UINT32    tlm                  : 1;
  UINT32    dlm                  : 1;
  UINT32    hsp2                 : 1;
  UINT32    rty2                 : 1;
  UINT32    stop_rec_req         : 8;
  /* DWORD11 */
  UINT32    device_addr          : 8;
  UINT32    hub_addr             : 8;
  UINT32    root_port_num        : 8;
  UINT32    slot_id              : 8;
  /* DWORD12 */
  UINT32    routing_string       : 20;
  UINT32    speed                : 4;
  UINT32    lpu                  : 1;
  UINT32    mtt                  : 1;
  UINT32    hub                  : 1;
  UINT32    dci                  : 5;
  /* DWORD13 */
  UINT32    tthub_slot_id        : 8;
  UINT32    ttport_num           : 8;
  UINT32    ssf                  : 4;
  UINT32    sps                  : 2;
  UINT32    int_target           : 10;
  /* DWORD14 */
  UINT32    frz                  : 1;
  UINT32    end                  : 1;
  UINT32    elm                  : 1;
  UINT32    mrx                  : 1;
  UINT32    ep_linklo            : 28;
  /* DWORD15 */
  UINT32    ep_linkhi;
} EP_CONTEXT;

#endif
