/** @file
  SSDT for TH500 Socket 1 devices

  Copyright (c) 2022 - 2023, NVIDIA Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  Template for [SSDT] ACPI Table (AML byte code table)
**/

#include <TH500/TH500Definitions.h>
#include <Protocol/BpmpIpc.h>
#include <AcpiPowerMeter.h>

DefinitionBlock ("BpmpSsdtSocket1.aml", "SSDT", 2, "NVIDIA", "BPMP_S1", 0x00000001)
{
  Scope(_SB) {
    //---------------------------------------------------------------------
    // BPMP Device
    //---------------------------------------------------------------------

    Device (BPM1)
    {
      Name (_HID, EISAID("PNP0C02")) // Motherboard resources
      Name (_UID, "BPMP IPC Socket 1")
      Name (TBUF, 0xFFFFFFFFFFFFFFFF)
      Name (TIME, 0xFF)
      Name (LSTM, 0)
      Name (PWRV, 0x00000000)

      OperationRegion (BPTX, SystemMemory, BPMP_TX_MAILBOX_SOCKET_1, BPMP_CHANNEL_SIZE)
      Field (BPTX, AnyAcc, NoLock, Preserve) {
        TWCT, 32,
        TSTA, 32,
        Offset (64),
        TRCT, 32,
        Offset (128),
        TMRQ, 32,
        TFLA, 32,
        TDAT, 960
      }

      OperationRegion (BPRX, SystemMemory, BPMP_RX_MAILBOX_SOCKET_1, BPMP_CHANNEL_SIZE)
      Field (BPRX, AnyAcc, NoLock, Preserve) {
        RWCT, 32,
        RSTA, 32,
        Offset (64),
        RRCT, 32,
        Offset (128),
        RERR, 32,
        RFLG, 32,
        RDAT, 960
      }

      Method (BIPC, 2, Serialized, 0, PkgObj, {IntObj, BuffObj}) {
        OperationRegion (DRBL, SystemMemory, BPMP_DOORBELL_SOCKET_1, BPMP_DOORBELL_SIZE)
        Field (DRBL, AnyAcc, NoLock, Preserve) {
          TRIG, 4,
          ENA,  4,
          RAW,  4,
          PEND, 4
        }

        TMRQ = Arg0
        TFLA = One
        TDAT = Arg1
        Increment (TWCT)
        Store (One, TRIG)

        Local0 = 0
        While ((RWCT == RRCT) && (Local0 < 10)) {
          Sleep (10)
          Local0++
        }
        If (Local0 < 10) {
          Increment (RRCT)
          Return (Package() {RERR, RDAT})
        } Else {
          // Returning an error code that is ASL compatible
          // instead of a BPMP Timeout error code
          RERR = 1
          Return (Package() {RERR, RDAT})
        }
      }

      Method (TEMP, 1, Serialized, 0, IntObj, IntObj) {
        Local0 = Buffer(8){}
        CreateDWordField (Local0, 0x00, CMD)
        CreateDWordField (Local0, 0x04, ZONE)
        CMD = ZONE_TEMP
        ZONE = Arg0
        Local1 = \_SB.BPM1.BIPC (MRQ_THERMAL, Local0)
        CreateDWordField (DerefOf (Index (Local1, 0)), 0x00, ERR)
        if (ERR != 0) {
          Return (2732)
        }
        CreateDWordField (DerefOf (Index (Local1, 1)), 0x00, TEMP)
        Local3 = TEMP / 100
        Local4 = 2732
        Add (Local3, Local4, Local3)
        Return (Local3)
      }

      Method (TELM, 1, Serialized, 0, IntObj, IntObj) {
        Local0 = 0
        Local1 = 0
        Local2 = TH500_BPMP_IPC_CALL_INTERVAL_50MS
        Local3 = 0
        Local4 = 0

        If (TIME != 0) {
          If (LSTM > Timer()) {
            Store(Timer(), LSTM)
          }
          Add (LSTM, Local2, Local3)
          If (LGreater (Timer(), Local3)) {
            Local4 = 1
          } Else {
            Local4 = 0
          }
        }

        If (LOr((TIME == 0), Local4)) {
          Local1 = \_SB.BPM1.BIPC (MRQ_TELEMETRY, Local0)
          CreateDWordField (DerefOf (Index (Local1, 0)), 0x00, ERR)
          If (ERR != 0) {
            Return (PWR_METER_ERR_RETURN)
          }

          If (TIME != 0) {
            Store (Timer(), LSTM)
          }
        }
        Return (TBUF)
      }

      Method (SPRL, 2, Serialized, 0, IntObj, {IntObj, IntObj}) {
        Local0 = Buffer (20) {}

        CreateDWordField (Local0, 0x00, CMD)
        CreateDWordField (Local0, 0x04, LIID)
        CreateDWordField (Local0, 0x08, LISR)
        CreateDWordField (Local0, 0x0C, LITY)
        CreateDWordField (Local0, 0x10, LISE)

        CMD = TH500_PWR_LIMIT_SET
        LIID = Arg0
        LISR = TH500_PWR_LIMIT_SRC_INB
        LITY = TH500_PWR_LIMIT_TYPE_TARGET_CAP
        LISE = Arg1

        Local1 = \_SB.BPM1.BIPC (MRQ_PWR_LIMIT, Local0)
        CreateDWordField (DerefOf (Index (Local1, 0)), 0x00, ERR)
        if (ERR != 0) {
          Return (PWR_METER_UNKNOWN_HW_ERR)
        }
        Return (PWR_METER_SUCCESS)
      }

      Method (GPRL, 1, Serialized, 0, IntObj, IntObj) {
        Local0 = Buffer (16) {}

        CreateDWordField (Local0, 0x00, CMD)
        CreateDWordField (Local0, 0x04, LIID)
        CreateDWordField (Local0, 0x08, LISR)
        CreateDWordField (Local0, 0x0C, LITY)

        CMD = TH500_PWR_LIMIT_GET
        LIID = Arg0
        LISR = TH500_PWR_LIMIT_SRC_INB
        LITY = TH500_PWR_LIMIT_TYPE_TARGET_CAP

        Local1 = \_SB.BPM1.BIPC (MRQ_PWR_LIMIT, Local0)
        CreateDWordField (DerefOf (Index (Local1, 0)), 0x00, ERR)
        if (ERR != 0) {
          Return (PWR_METER_ERR_RETURN)
        }
        CreateDWordField (DerefOf (Index (Local1, 1)), 0x00, PLIM)
        Return (PLIM)
      }
    }

    // Socket 1 CPUs 0-83
    External(\_SB.C001.C000)
    External(\_SB.C001.C002)
    External(\_SB.C001.C003)
    External(\_SB.C001.C004)
    External(\_SB.C001.C005)
    External(\_SB.C001.C006)
    External(\_SB.C001.C007)
    External(\_SB.C001.C008)
    External(\_SB.C001.C009)
    External(\_SB.C001.C00A)
    External(\_SB.C001.C00B)
    External(\_SB.C001.C00C)
    External(\_SB.C001.C00E)
    External(\_SB.C001.C010)
    External(\_SB.C001.C011)
    External(\_SB.C001.C012)
    External(\_SB.C001.C013)
    External(\_SB.C001.C014)
    External(\_SB.C001.C015)
    External(\_SB.C001.C016)
    External(\_SB.C001.C017)
    External(\_SB.C001.C018)
    External(\_SB.C001.C019)
    External(\_SB.C001.C01A)
    External(\_SB.C001.C01C)
    External(\_SB.C001.C01D)
    External(\_SB.C001.C01E)
    External(\_SB.C001.C01F)
    External(\_SB.C001.C020)
    External(\_SB.C001.C021)
    External(\_SB.C001.C022)
    External(\_SB.C001.C023)
    External(\_SB.C001.C024)
    External(\_SB.C001.C025)
    External(\_SB.C001.C026)
    External(\_SB.C001.C027)
    External(\_SB.C001.C028)
    External(\_SB.C001.C029)
    External(\_SB.C001.C02A)
    External(\_SB.C001.C02B)
    External(\_SB.C001.C02C)
    External(\_SB.C001.C02D)
    External(\_SB.C001.C02E)
    External(\_SB.C001.C02F)
    External(\_SB.C001.C030)
    External(\_SB.C001.C031)
    External(\_SB.C001.C032)
    External(\_SB.C001.C033)
    External(\_SB.C001.C034)
    External(\_SB.C001.C035)
    External(\_SB.C001.C036)
    External(\_SB.C001.C037)
    External(\_SB.C001.C038)
    External(\_SB.C001.C03A)
    External(\_SB.C001.C03B)
    External(\_SB.C001.C03C)
    External(\_SB.C001.C03D)
    External(\_SB.C001.C03E)
    External(\_SB.C001.C03F)
    External(\_SB.C001.C040)
    External(\_SB.C001.C041)
    External(\_SB.C001.C042)
    External(\_SB.C001.C043)
    External(\_SB.C001.C044)
    External(\_SB.C001.C046)
    External(\_SB.C001.C048)
    External(\_SB.C001.C049)
    External(\_SB.C001.C04A)
    External(\_SB.C001.C04B)
    External(\_SB.C001.C04C)
    External(\_SB.C001.C04D)
    External(\_SB.C001.C04E)
    External(\_SB.C001.C04F)
    External(\_SB.C001.C050)
    External(\_SB.C001.C051)
    External(\_SB.C001.C052)

    //---------------------------------------------------------------------
    // Module Power Device Socket 1
    //---------------------------------------------------------------------
    Device (PM10)
    {
      Name (_HID, "ACPI000D")
      Name (_UID, 10)
      Name (CAI, PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS)
      Name (HWLT, 0)
      Name (MINP, 0x00000000)
      Name (MAXP, 0x00000000)

      // _PMD method return code - List of power meter devices
      Name (PMD, Package() {
        \_SB.PM10
      })

      // _PMC method result code
      Name (PMC, Package() {
        BIT (PWR_METER_SUPPORTS_MEASUREMENT),
        PWR_METER_MEASUREMENT_IN_MW,
        PWR_METER_MEASURE_OP_PWR,
        PWR_METER_MEASUREMENT_ACCURACY_100,
        \_SB.PM10.CAI,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC,
        PWR_METER_HYSTERESIS_MARGIN_UNKNOWN,
        PWR_METER_HW_LIMIT_RW,
        \_SB.PM10.MINP,
        \_SB.PM10.MAXP,
        "",                                   // Model Number - NULL
        "",                                   // Serial Number - NULL
        "Module Power Socket 1"               // OEM Information - "Module Power Socket 1"
      })

      Method (_PMC) {
        Return (PMC)
      }

      Method (_PAI, 1, Serialized, 0, IntObj, IntObj) {
        If (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS, CAI)
          Notify (\_SB.PM10, PWR_METER_NOTIFY_CONFIG)
          Return (PWR_METER_SUCCESS)
        } ElseIf (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC, CAI)
          Notify (\_SB.PM10, PWR_METER_NOTIFY_CONFIG)
          Return (PWR_METER_SUCCESS)
        }
        Return (PWR_METER_OUT_OF_RANGE)
      }

      Method (_GAI) {
        Return (CAI)
      }

      Method (_PMD) {
        Return (PMD)
      }

      Method (_PMM) {
        Local0 = 0
        Local1 = 0
        Local0 = \_SB.BPM1.TELM(0)
        If (Local0 == PWR_METER_ERR_RETURN) {
          Return (PWR_METER_ERR_RETURN)
        }
        CreateQWordField (Local0, 0x00, TELB)
        OperationRegion (TELD, SystemMemory, TELB,  0x180)
        Field (TELD, AnyAcc, NoLock, Preserve) {
          Offset (16),
          MPWR, 32,
          TPWR, 32,
          CPWR, 32,
          SPWR, 32,
          Offset (288),
          MAPW, 32,
          TAPW, 32,
          CAPW, 32,
          SAPR, 32,
          Offset (360),
          VFG0, 32,
          VFG1, 32,
          VFG2, 32
        }

        If (CAI == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
          And (VFG0, TH500_MODULE_PWR_IDX_VALID_FLAG, Local1)
          If (Local1 > 0) {
            Return (MPWR)
          }
        } Else {
          And (VFG2, TH500_MODULE_PWR_1SEC_IDX_VALID_FLAG, Local1)
          If (Local1 > 0) {
            Return (MAPW)
          }
        }
        Return (PWR_METER_ERR_RETURN)
      }

      Method (_GHL) {
        Return (PWR_METER_ERR_RETURN)
      }

      Method (_SHL, 1, Serialized, 0, IntObj, IntObj) {
        Store (Arg0, HWLT)
        Return (PWR_METER_UNKNOWN_HW_ERR)
      }
    }

    //---------------------------------------------------------------------
    // TH500 Power Device Socket 1
    //---------------------------------------------------------------------
    Device (PM11)
    {
      Name (_HID, "ACPI000D")
      Name (_UID, 11)
      Name (CAI, PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS)
      Name (HWLT, 0)
      Name (MINP, 0xAAAAAAAA)
      Name (MAXP, 0xAAAAAAAA)

      // _PMD method return code - List of power meter devices
      Name (PMD, Package() {
        \_SB.PM11
      })

      // _PMC method result code
      Name (PMC, Package() {
        BIT (PWR_METER_SUPPORTS_MEASUREMENT) | BIT (PWR_METER_SUPPORTS_HW_LIMITS) |
        BIT (PWR_METER_SUPPORTS_NOTIFY_HW_LIMITS),
        PWR_METER_MEASUREMENT_IN_MW,
        PWR_METER_MEASURE_OP_PWR,
        PWR_METER_MEASUREMENT_ACCURACY_100,
        \_SB.PM11.CAI,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC,
        PWR_METER_HYSTERESIS_MARGIN_UNKNOWN,
        PWR_METER_HW_LIMIT_RW,
        \_SB.PM11.MINP,
        \_SB.PM11.MAXP,
        "",                                  // Model Number - NULL
        "",                                  // Serial Number - NULL
        "TH500 Power Socket 1"               // OEM Information - "TH500 Power Socket 1"
      })

      Method (_PMC) {
        Return (PMC)
      }

      Method (_PAI, 1, Serialized, 0, IntObj, IntObj) {
        If (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS, CAI)
          Notify (\_SB.PM11, PWR_METER_NOTIFY_CONFIG)
          Return (PWR_METER_SUCCESS)
        } ElseIf (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC, CAI)
          Notify (\_SB.PM11, PWR_METER_NOTIFY_CONFIG)
          Return (PWR_METER_SUCCESS)
        }
        Return (PWR_METER_OUT_OF_RANGE)
      }

      Method (_GAI) {
        Return (CAI)
      }

      Method (_PMD) {
        Return (PMD)
      }

      Method (_PMM) {
        Local0 = 0
        Local1 = 0
        Local0 = \_SB.BPM1.TELM(0)
        If (Local0 == PWR_METER_ERR_RETURN) {
          Return (PWR_METER_ERR_RETURN)
        }
        CreateQWordField (Local0, 0x00, TELB)
        OperationRegion (TELD, SystemMemory, TELB,  0x180)
        Field (TELD, AnyAcc, NoLock, Preserve) {
          Offset (16),
          MPWR, 32,
          TPWR, 32,
          CPWR, 32,
          SPWR, 32,
          Offset (288),
          MAPW, 32,
          TAPW, 32,
          CAPW, 32,
          SAPR, 32,
          Offset (360),
          VFG0, 32,
          VFG1, 32,
          VFG2, 32
        }

        If (CAI == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
          And (VFG0, TH500_TH500_PWR_IDX_VALID_FLAG, Local1)
          If (Local1 > 0) {
            Return (TPWR)
          }
        } Else {
          And (VFG2, TH500_TH500_PWR_1SEC_IDX_VALID_FLAG, Local1)
          If (Local1 > 0) {
            Return (TAPW)
          }
        }
        Return (PWR_METER_ERR_RETURN)
      }

      Method(_GHL) {
        Local0 = 0
        Local1 = TH500_PWR_LIMIT_ID_TH500_INP_EDPC_MW
        Local0 = \_SB.BPM1.GPRL (Local1)
        Return (Local0)
      }

      Method (_SHL, 1, Serialized, 0, IntObj, IntObj) {
        Local0 = 0
        Local1 = TH500_PWR_LIMIT_ID_TH500_INP_EDPC_MW
        If ((Arg0 <= MINP) && (Arg0 >= MAXP)) {
          Return (PWR_METER_OUT_OF_RANGE)
        }
        Local0 = \_SB.BPM1.SPRL (Local1, Arg0)
        If (Local0 == 0) {
          If (Arg0 != HWLT) {
            Store (Arg0, HWLT)
            Notify (\_SB.PM11, PWR_METER_NOTIFY_CAP)
          }
          Notify (\_SB.PM11, PWR_METER_NOTIFY_CAPPING)
        }
        Return (Local0)
      }
    }

    //---------------------------------------------------------------------
    // CPU Power Device Socket 1
    //---------------------------------------------------------------------
    Device (PM12)
    {
      Name (_HID, "ACPI000D")
      Name (_UID, 12)
      Name (CAI, PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS)
      Name (HWLT, 0)
      Name (MINP, 0x00000000)
      Name (MAXP, 0x00000000)

      // _PMD method return code - List of power meter devices
      Name (PMD, Package() {
        \_SB.PM12
      })

      // _PMC method result code
      Name (PMC, Package() {
        BIT (PWR_METER_SUPPORTS_MEASUREMENT),
        PWR_METER_MEASUREMENT_IN_MW,
        PWR_METER_MEASURE_OP_PWR,
        PWR_METER_MEASUREMENT_ACCURACY_100,
        \_SB.PM12.CAI,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC,
        PWR_METER_HYSTERESIS_MARGIN_UNKNOWN,
        PWR_METER_HW_LIMIT_RW,
        \_SB.PM12.MINP,
        \_SB.PM12.MAXP,
        "",                                // Model Number - NULL
        "",                                // Serial Number - NULL
        "CPU Power Socket 1"               // OEM Information - "CPU Power Socket 1"
      })

      Method (_PMC) {
        Return (PMC)
      }

      Method (_PAI, 1, Serialized, 0, IntObj, IntObj) {
        If (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS, CAI)
          Notify (\_SB.PM12, PWR_METER_NOTIFY_CONFIG)
          Return (PWR_METER_SUCCESS)
        } ElseIf (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC, CAI)
          Notify (\_SB.PM12, PWR_METER_NOTIFY_CONFIG)
          Return (PWR_METER_SUCCESS)
        }
        Return (PWR_METER_OUT_OF_RANGE)
      }

      Method (_GAI) {
        Return (CAI)
      }

      Method (_PMD) {
        Return (PMD)
      }

      Method (_PMM) {
        Local0 = 0
        Local1 = 0
        Local0 = \_SB.BPM1.TELM(0)
        If (Local0 == PWR_METER_ERR_RETURN) {
          Return (PWR_METER_ERR_RETURN)
        }
        CreateQWordField (Local0, 0x00, TELB)
        OperationRegion (TELD, SystemMemory, TELB,  0x180)
        Field (TELD, AnyAcc, NoLock, Preserve) {
          Offset (16),
          MPWR, 32,
          TPWR, 32,
          CPWR, 32,
          SPWR, 32,
          Offset (288),
          MAPW, 32,
          TAPW, 32,
          CAPW, 32,
          SAPR, 32,
          Offset (360),
          VFG0, 32,
          VFG1, 32,
          VFG2, 32
        }

        If (CAI == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
          And (VFG0, TH500_CPU_PWR_IDX_VALID_FLAG, Local1)
          If (Local1 > 0) {
            Return (CPWR)
          }
        } Else {
          And (VFG2, TH500_CPU_PWR_1SEC_IDX_VALID_FLAG, Local1)
          If (Local1 > 0) {
            Return (CAPW)
          }
        }
        Return (PWR_METER_ERR_RETURN)
      }

      Method (_GHL) {
        Return (PWR_METER_ERR_RETURN)
      }

      Method (_SHL, 1, Serialized, 0, IntObj, IntObj) {
        Store (Arg0, HWLT)
        Return (PWR_METER_UNKNOWN_HW_ERR)
      }
    }

    //---------------------------------------------------------------------
    // SOC Power Device Socket 1
    //---------------------------------------------------------------------
    Device (PM13)
    {
      Name (_HID, "ACPI000D")
      Name (_UID, 13)
      Name (CAI, PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS)
      Name (HWLT, 0)
      Name (MINP, 0x00000000)
      Name (MAXP, 0x00000000)

      // _PMD method return code - List of power meter devices
      Name (PMD, Package() {
        \_SB.PM13
      })

      // _PMC method result code
      Name (PMC, Package() {
        BIT (PWR_METER_SUPPORTS_MEASUREMENT),
        PWR_METER_MEASUREMENT_IN_MW,
        PWR_METER_MEASURE_OP_PWR,
        PWR_METER_MEASUREMENT_ACCURACY_100,
        \_SB.PM13.CAI,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC,
        PWR_METER_HYSTERESIS_MARGIN_UNKNOWN,
        PWR_METER_HW_LIMIT_RW,
        \_SB.PM13.MINP,
        \_SB.PM13.MAXP,
        "",                                // Model Number - NULL
        "",                                // Serial Number - NULL
        "SoC Power Socket 1"               // OEM Information - "SoC Power Socket 1"
      })

      Method (_PMC) {
        Return (PMC)
      }

      Method (_PAI, 1, Serialized, 0, IntObj, IntObj) {
        If (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS, CAI)
          Notify (\_SB.PM13, PWR_METER_NOTIFY_CONFIG)
          Return (PWR_METER_SUCCESS)
        } ElseIf (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC, CAI)
          Notify (\_SB.PM13, PWR_METER_NOTIFY_CONFIG)
          Return (PWR_METER_SUCCESS)
        }
        Return (PWR_METER_OUT_OF_RANGE)
      }

      Method (_GAI) {
        Return (CAI)
      }

      Method (_PMD) {
        Return (PMD)
      }

      Method (_PMM) {
        Local0 = 0
        Local1 = 0
        Local0 = \_SB.BPM1.TELM(0)
        If (Local0 == PWR_METER_ERR_RETURN) {
          Return (PWR_METER_ERR_RETURN)
        }
        CreateQWordField (Local0, 0x00, TELB)
        OperationRegion (TELD, SystemMemory, TELB,  0x180)
        Field (TELD, AnyAcc, NoLock, Preserve) {
          Offset (16),
          MPWR, 32,
          TPWR, 32,
          CPWR, 32,
          SPWR, 32,
          Offset (288),
          MAPW, 32,
          TAPW, 32,
          CAPW, 32,
          SAPR, 32,
          Offset (360),
          VFG0, 32,
          VFG1, 32,
          VFG2, 32
        }

        If (CAI == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
          And (VFG0, TH500_SOC_PWR_IDX_VALID_FLAG, Local1)
          If (Local1 > 0) {
            Return (SPWR)
          }
        } Else {
          And (VFG2, TH500_SOC_PWR_1SEC_IDX_VALID_FLAG, Local1)
          If (Local1 > 0) {
            Return (SAPR)
          }
        }
        Return (PWR_METER_ERR_RETURN)
      }

      Method (_GHL) {
        Return (PWR_METER_ERR_RETURN)
      }

      Method (_SHL, 1, Serialized, 0, IntObj, IntObj) {
        Store (Arg0, HWLT)
        Return (PWR_METER_UNKNOWN_HW_ERR)
      }
    }
  } //Scope(_SB)
}
