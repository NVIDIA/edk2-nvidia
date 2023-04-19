/*
 * Intel ACPI Component Architecture
 * iASL Compiler/Disassembler version 20180105 (64-bit version)
 * Copyright (c) 2020 - 2023, NVIDIA Corporation. All rights reserved.
 * Copyright (c) 2000 - 2018 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * Template for [DSDT] ACPI Table (AML byte code table)
 */

#include <TH500/TH500Definitions.h>
#include <Protocol/BpmpIpc.h>
#include <AcpiPowerMeter.h>

DefinitionBlock ("BpmpSsdtSocket0.aml", "SSDT", 2, "NVIDIA", "BPMP_S0", 0x00000001)
{
  Scope(_SB)
  {
    //---------------------------------------------------------------------
    // BPMP Device
    //---------------------------------------------------------------------

    Device (BPM0)
    {
      Name (_HID, EISAID("PNP0C02")) // Motherboard resources
      Name (_UID, "BPMP IPC Socket 0")
      Name (TBUF, 0xFFFFFFFFFFFFFFFF)
      Name (TIME, 0xFF)
      Name (LSTM, 0)
      Name (PWRV, 0x00000000)

      OperationRegion (BPTX, SystemMemory, BPMP_TX_MAILBOX_SOCKET_0, BPMP_CHANNEL_SIZE)
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

      OperationRegion (BPRX, SystemMemory, BPMP_RX_MAILBOX_SOCKET_0, BPMP_CHANNEL_SIZE)
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
        OperationRegion (DRBL, SystemMemory, BPMP_DOORBELL_SOCKET_0, BPMP_DOORBELL_SIZE)
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

        While (RWCT == RRCT) {
          Sleep (10)
        }
        Increment (RRCT)
        Return (Package() {RERR, RDAT})
      }

      Method (TEMP, 1, Serialized, 0, IntObj, IntObj) {
        Local0 = Buffer(8){}
        CreateDWordField (Local0, 0x00, CMD)
        CreateDWordField (Local0, 0x04, ZONE)
        CMD = ZONE_TEMP
        ZONE = Arg0
        Local1 = \_SB.BPM0.BIPC (MRQ_THERMAL, Local0)
        CreateDWordField (DerefOf (Index (Local1, 0)), 0x00, ERR)
        If (ERR != 0) {
          Return (2732)
        }
        CreateDWordField (DerefOf (Index (Local1, 1)), 0x00, TEMP)
        Local3 = TEMP / 100
        Local4 = 2732
        Add (Local3, Local4, Local3)
        Return (Local3)
      }

      Method (TELM, 2, Serialized, 0, IntObj, {IntObj, IntObj}) {
        Local0 = 0
        Local1 = 0
        Local2 = TH500_BPMP_IPC_CALL_INTERVAL_50MS
        Local3 = 0
        Local4 = 0
        Local5 = 0

        If ((Arg0 < TH500_MODULE_PWR) || (Arg0 > TH500_SOC_PWR)) {
          Return (PWR_METER_ERR_RETURN)
        }

        If ((Arg1 != PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) && (Arg1 != PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC)) {
          Return (PWR_METER_ERR_RETURN)
        }

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
          Local1 = \_SB.BPM0.BIPC (MRQ_TELEMETRY, Local0)
          CreateDWordField (DerefOf (Index (Local1, 0)), 0x00, ERR)
          If (ERR != 0) {
            Return (PWR_METER_ERR_RETURN)
          }
          CreateQWordField (DerefOf (Index (Local1, 1)), 0x00, BUFF)

          OperationRegion (TELD, SystemMemory, BUFF, 0x180)
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
            SAPW, 32,
            Offset (360),
            VFG0, 32,
            VFG1, 32,
            VFG2, 32
          }

          Switch (Arg0) {
            Case (TH500_MODULE_PWR) {
              If (Arg1 == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
                And (VFG0, TH500_MODULE_PWR_IDX_VALID_FLAG, Local5)
                If (Local5 > 0) {
                  Store (MPWR, PWRV)
                }
              }
              If (Arg1 == PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC) {
                And (VFG2, TH500_MODULE_PWR_1SEC_IDX_VALID_FLAG, Local5)
                If (Local5 > 0) {
                  Store (MAPW, PWRV)
                }
              }
              Break
            }

            Case (TH500_TH500_PWR) {
              If (Arg1 == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
                And (VFG0, TH500_TH500_PWR_IDX_VALID_FLAG, Local5)
                If (Local5 > 0) {
                  Store (TPWR, PWRV)
                }
              }
              If (Arg1 == PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC) {
                And (VFG2, TH500_TH500_PWR_1SEC_IDX_VALID_FLAG, Local5)
                If (Local5 > 0) {
                  Store (TAPW, PWRV)
                }
              }
              Break
            }

            Case (TH500_CPU_PWR) {
              If (Arg1 == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
                And (VFG0, TH500_CPU_PWR_IDX_VALID_FLAG, Local5)
                If (Local5 > 0) {
                  Store (CPWR, PWRV)
                }
              }
              If (Arg1 == PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC) {
                And (VFG2, TH500_CPU_PWR_1SEC_IDX_VALID_FLAG, Local5)
                If (Local5 > 0) {
                  Store (CAPW, PWRV)
                }
              }
              Break
            }

            Case (TH500_SOC_PWR) {
              If (Arg1 == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
                And (VFG0, TH500_SOC_PWR_IDX_VALID_FLAG, Local5)
                If (Local5 > 0) {
                  Store (SPWR, PWRV)
                }
              }
              If (Arg1 == PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC) {
                And (VFG2, TH500_SOC_PWR_1SEC_IDX_VALID_FLAG, Local5)
                If (Local5 > 0) {
                  Store (SAPW, PWRV)
                }
              }
              Break
            }
          }

          If (TIME != 0) {
            Store (Timer(), LSTM)
          }
        }
        Return (PWRV)
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

        Local1 = \_SB.BPM0.BIPC (MRQ_PWR_LIMIT, Local0)
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

        Local1 = \_SB.BPM0.BIPC (MRQ_PWR_LIMIT, Local0)
        CreateDWordField (DerefOf (Index (Local1, 0)), 0x00, ERR)
        if (ERR != 0) {
          Return (PWR_METER_ERR_RETURN)
        }
        CreateDWordField (DerefOf (Index (Local1, 1)), 0x00, PLIM)
        Return (PLIM)
      }
    }

    // Socket 0 CPUs 0-83
    External(\_SB.C000.C000)
    External(\_SB.C000.C002)
    External(\_SB.C000.C003)
    External(\_SB.C000.C004)
    External(\_SB.C000.C005)
    External(\_SB.C000.C006)
    External(\_SB.C000.C007)
    External(\_SB.C000.C008)
    External(\_SB.C000.C009)
    External(\_SB.C000.C00A)
    External(\_SB.C000.C00B)
    External(\_SB.C000.C00C)
    External(\_SB.C000.C00E)
    External(\_SB.C000.C010)
    External(\_SB.C000.C011)
    External(\_SB.C000.C012)
    External(\_SB.C000.C013)
    External(\_SB.C000.C014)
    External(\_SB.C000.C015)
    External(\_SB.C000.C016)
    External(\_SB.C000.C017)
    External(\_SB.C000.C018)
    External(\_SB.C000.C019)
    External(\_SB.C000.C01A)
    External(\_SB.C000.C01C)
    External(\_SB.C000.C01D)
    External(\_SB.C000.C01E)
    External(\_SB.C000.C01F)
    External(\_SB.C000.C020)
    External(\_SB.C000.C021)
    External(\_SB.C000.C022)
    External(\_SB.C000.C023)
    External(\_SB.C000.C024)
    External(\_SB.C000.C025)
    External(\_SB.C000.C026)
    External(\_SB.C000.C027)
    External(\_SB.C000.C028)
    External(\_SB.C000.C029)
    External(\_SB.C000.C02A)
    External(\_SB.C000.C02B)
    External(\_SB.C000.C02C)
    External(\_SB.C000.C02D)
    External(\_SB.C000.C02E)
    External(\_SB.C000.C02F)
    External(\_SB.C000.C030)
    External(\_SB.C000.C031)
    External(\_SB.C000.C032)
    External(\_SB.C000.C033)
    External(\_SB.C000.C034)
    External(\_SB.C000.C035)
    External(\_SB.C000.C036)
    External(\_SB.C000.C037)
    External(\_SB.C000.C038)
    External(\_SB.C000.C03A)
    External(\_SB.C000.C03B)
    External(\_SB.C000.C03C)
    External(\_SB.C000.C03D)
    External(\_SB.C000.C03E)
    External(\_SB.C000.C03F)
    External(\_SB.C000.C040)
    External(\_SB.C000.C041)
    External(\_SB.C000.C042)
    External(\_SB.C000.C043)
    External(\_SB.C000.C044)
    External(\_SB.C000.C046)
    External(\_SB.C000.C048)
    External(\_SB.C000.C049)
    External(\_SB.C000.C04A)
    External(\_SB.C000.C04B)
    External(\_SB.C000.C04C)
    External(\_SB.C000.C04D)
    External(\_SB.C000.C04E)
    External(\_SB.C000.C04F)
    External(\_SB.C000.C050)
    External(\_SB.C000.C051)
    External(\_SB.C000.C052)

    //---------------------------------------------------------------------
    // Module Power Device Socket 0
    //---------------------------------------------------------------------
    Device (PM00)
    {
      Name (_HID, "ACPI000D")
      Name (_UID, 00)
      Name (CAI, PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS)
      Name (HWLT, 0)
      Name (MINP, 0x00000000)
      Name (MAXP, 0x00000000)

      // _PMD method return code - List of power meter devices
      Name (PMD, Package() {
        \_SB.PM00
      })

      // _PMC method result code
      Name (PMC, Package() {
        BIT (PWR_METER_SUPPORTS_MEASUREMENT),
        PWR_METER_MEASUREMENT_IN_MW,
        PWR_METER_MEASURE_OP_PWR,
        PWR_METER_MEASUREMENT_ACCURACY_100,
        \_SB.PM00.CAI,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC,
        PWR_METER_HYSTERESIS_MARGIN_UNKNOWN,
        PWR_METER_HW_LIMIT_RW,
        \_SB.PM00.MINP,
        \_SB.PM00.MAXP,
        "",                                   // Model Number - NULL
        "",                                   // Serial Number - NULL
        "Module Power Socket 0"               // OEM Information - "Module Power Socket 0"
      })

      Method (_PMC) {
        Return (PMC)
      }

      Method (_PAI, 1, Serialized, 0, IntObj, IntObj) {
        If (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS, CAI)
          Notify (\_SB.PM00, PWR_METER_NOTIFY_CONFIG)
          Return (PWR_METER_SUCCESS)
        } ElseIf (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC, CAI)
          Notify (\_SB.PM00, PWR_METER_NOTIFY_CONFIG)
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
        Local0 = \_SB.BPM0.TELM(TH500_MODULE_PWR, CAI)
        Return (Local0)
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
    // TH500 Power Device Socket 0
    //---------------------------------------------------------------------
    Device (PM01)
    {
      Name (_HID, "ACPI000D")
      Name (_UID, 01)
      Name (CAI, PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS)
      Name (HWLT, 0)
      Name (MINP, 0xAAAAAAAA)
      Name (MAXP, 0xAAAAAAAA)

      // _PMD method return code - List of power meter devices
      Name (PMD, Package() {
        \_SB.PM01
      })

      // _PMC method result code
      Name (PMC, Package() {
        BIT (PWR_METER_SUPPORTS_MEASUREMENT) | BIT (PWR_METER_SUPPORTS_HW_LIMITS) |
        BIT (PWR_METER_SUPPORTS_NOTIFY_HW_LIMITS),
        PWR_METER_MEASUREMENT_IN_MW,
        PWR_METER_MEASURE_OP_PWR,
        PWR_METER_MEASUREMENT_ACCURACY_100,
        \_SB.PM01.CAI,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC,
        PWR_METER_HYSTERESIS_MARGIN_UNKNOWN,
        PWR_METER_HW_LIMIT_RW,
        \_SB.PM01.MINP,
        \_SB.PM01.MAXP,
        "",                                  // Model Number - NULL
        "",                                  // Serial Number - NULL
        "TH500 Power Socket 0"               // OEM Information - "TH500 Power Socket 0"
      })

      Method (_PMC) {
        Return (PMC)
      }

      Method (_PAI, 1, Serialized, 0, IntObj, IntObj) {
        If (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS, CAI)
          Notify (\_SB.PM01, PWR_METER_NOTIFY_CONFIG)
          Return (PWR_METER_SUCCESS)
        } ElseIf (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC, CAI)
          Notify (\_SB.PM01, PWR_METER_NOTIFY_CONFIG)
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
        Local0 = \_SB.BPM0.TELM(TH500_TH500_PWR, CAI)
        Return (Local0)
      }

      Method(_GHL) {
        Local0 = 0
        Local1 = TH500_PWR_LIMIT_ID_TH500_INP_EDPC_MW
        Local0 = \_SB.BPM0.GPRL (Local1)
        Return (Local0)
      }

      Method (_SHL, 1, Serialized, 0, IntObj, IntObj) {
        Local0 = 0
        Local1 = TH500_PWR_LIMIT_ID_TH500_INP_EDPC_MW
        If ((Arg0 <= MINP) && (Arg0 >= MAXP)) {
          Return (PWR_METER_OUT_OF_RANGE)
        }
        Local0 = \_SB.BPM0.SPRL (Local1, Arg0)
        If (Local0 == 0) {
          If (Arg0 != HWLT) {
            Store (Arg0, HWLT)
            Notify (\_SB.PM01, PWR_METER_NOTIFY_CAP)
          }
          Notify (\_SB.PM01, PWR_METER_NOTIFY_CAPPING)
        }
        Return (Local0)
      }
    }

    //---------------------------------------------------------------------
    // CPU Power Device Socket 0
    //---------------------------------------------------------------------
    Device (PM02)
    {
      Name (_HID, "ACPI000D")
      Name (_UID, 02)
      Name (CAI, PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS)
      Name (HWLT, 0)
      Name (MINP, 0x00000000)
      Name (MAXP, 0x00000000)

      // _PMD method return code - List of power meter devices
      Name (PMD, Package() {
        \_SB.PM02
      })

      // _PMC method result code
      Name (PMC, Package() {
        BIT (PWR_METER_SUPPORTS_MEASUREMENT),
        PWR_METER_MEASUREMENT_IN_MW,
        PWR_METER_MEASURE_OP_PWR,
        PWR_METER_MEASUREMENT_ACCURACY_100,
        \_SB.PM02.CAI,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC,
        PWR_METER_HYSTERESIS_MARGIN_UNKNOWN,
        PWR_METER_HW_LIMIT_RW,
        \_SB.PM02.MINP,
        \_SB.PM02.MAXP,
        "",                                // Model Number - NULL
        "",                                // Serial Number - NULL
        "CPU Power Socket 0"               // OEM Information - "CPU Power Socket 0"
      })

      Method (_PMC) {
        Return (PMC)
      }

      Method (_PAI, 1, Serialized, 0, IntObj, IntObj) {
        If (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS, CAI)
          Notify (\_SB.PM02, PWR_METER_NOTIFY_CONFIG)
          Return (PWR_METER_SUCCESS)
        } ElseIf (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC, CAI)
          Notify (\_SB.PM02, PWR_METER_NOTIFY_CONFIG)
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
        Local0 = \_SB.BPM0.TELM(TH500_CPU_PWR, CAI)
        Return (Local0)
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
    // SOC Power Device Socket 0
    //---------------------------------------------------------------------
    Device (PM03)
    {
      Name (_HID, "ACPI000D")
      Name (_UID, 03)
      Name (CAI, PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS)
      Name (HWLT, 0)
      Name (MINP, 0x00000000)
      Name (MAXP, 0x00000000)

      // _PMD method return code - List of power meter devices
      Name (PMD, Package() {
        \_SB.PM03
      })

      // _PMC method result code
      Name (PMC, Package() {
        BIT (PWR_METER_SUPPORTS_MEASUREMENT),
        PWR_METER_MEASUREMENT_IN_MW,
        PWR_METER_MEASURE_OP_PWR,
        PWR_METER_MEASUREMENT_ACCURACY_100,
        \_SB.PM03.CAI,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC,
        PWR_METER_HYSTERESIS_MARGIN_UNKNOWN,
        PWR_METER_HW_LIMIT_RW,
        \_SB.PM03.MINP,
        \_SB.PM03.MAXP,
        "",                                // Model Number - NULL
        "",                                // Serial Number - NULL
        "SoC Power Socket 0"               // OEM Information - "SoC Power Socket 0"
      })

      Method (_PMC) {
        Return (PMC)
      }

      Method (_PAI, 1, Serialized, 0, IntObj, IntObj) {
        If (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS, CAI)
          Notify (\_SB.PM03, PWR_METER_NOTIFY_CONFIG)
          Return (PWR_METER_SUCCESS)
        } ElseIf (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC, CAI)
          Notify (\_SB.PM03, PWR_METER_NOTIFY_CONFIG)
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
        Local0 = \_SB.BPM0.TELM(TH500_SOC_PWR, CAI)
        Return (Local0)
      }

      Method (_GHL) {
        Return (PWR_METER_ERR_RETURN)
      }

      Method (_SHL, 1, Serialized, 0, IntObj, IntObj) {
        Store (Arg0, HWLT)
        Return (PWR_METER_UNKNOWN_HW_ERR)
      }
    }
  }
}
