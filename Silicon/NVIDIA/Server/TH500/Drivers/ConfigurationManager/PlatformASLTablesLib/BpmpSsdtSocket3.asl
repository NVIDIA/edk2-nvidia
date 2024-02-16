/** @file
  SSDT for TH500 Socket 3 devices

  SPDX-FileCopyrightText: Copyright (c) 2022 - 2023, NVIDIA Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  Template for [SSDT] ACPI Table (AML byte code table)
**/

#include <TH500/TH500Definitions.h>
#include <Protocol/BpmpIpc.h>
#include <AcpiPowerMeter.h>

DefinitionBlock ("BpmpSsdtSocket3.aml", "SSDT", 2, "NVIDIA", "BPMP_S3", 0x00000001)
{
  Scope(_SB) {
    //---------------------------------------------------------------------
    // BPMP Device
    //---------------------------------------------------------------------

    Device (BPM3)
    {
      Name (_HID, EISAID("PNP0C02")) // Motherboard resources
      Name (_UID, "BPMP IPC Socket 3")
      Name (TBUF, 0xFFFFFFFFFFFFFFFF)
      Name (TIME, 0xFF)
      Name (LSTM, 0)
      Name (PWRV, 0x00000000)

      OperationRegion (BPTX, SystemMemory, BPMP_TX_MAILBOX_SOCKET_3, BPMP_CHANNEL_SIZE)
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

      OperationRegion (BPRX, SystemMemory, BPMP_RX_MAILBOX_SOCKET_3, BPMP_CHANNEL_SIZE)
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

      OperationRegion (DRBL, SystemMemory, BPMP_DOORBELL_SOCKET_3, BPMP_DOORBELL_SIZE)
      Field (DRBL, AnyAcc, NoLock, Preserve) {
        TRIG, 4,
        ENA,  4,
        RAW,  4,
        PEND, 4
      }

      Method (BIPC, 2, Serialized, 0, PkgObj, {IntObj, BuffObj}) {
        If ((TSTA != IVC_STATE_ESTABLISHED) ||
            (RSTA != IVC_STATE_ESTABLISHED) ||
            (RWCT != RRCT) || (TWCT != TRCT)) {
          //Reset IVC channel
          TSTA = IVC_STATE_SYNC
          Store (One, TRIG)

          Local0 = 0
          While ((TSTA != IVC_STATE_ESTABLISHED) ||
                 (RSTA != IVC_STATE_ESTABLISHED)) {
            If (Local0 == BPMP_RESPONSE_TIMEOUT_US) {
              RERR = 1
              Return (Package() {RERR, RDAT})
            }

            If (RSTA == IVC_STATE_SYNC) {
              TWCT = 0
              RRCT = 0
              TSTA = IVC_STATE_ACK
              Store (One, TRIG)
            } ElseIf (TSTA == IVC_STATE_ACK) {
              TSTA = IVC_STATE_ESTABLISHED
              Store (One, TRIG)
            } ElseIf ((TSTA == IVC_STATE_SYNC) && (RSTA == IVC_STATE_ACK)) {
              TWCT = 0
              RRCT = 0
              TSTA = IVC_STATE_ESTABLISHED
              Store (One, TRIG)
            }
            Local0++
            Stall(1)
          }
        }

        TMRQ = Arg0
        TFLA = One
        TDAT = Arg1
        Increment (TWCT)
        Store (One, TRIG)

        Local0 = 0
        While (RWCT == RRCT) {
          If (Local0 == BPMP_RESPONSE_TIMEOUT_US) {
            RERR = 1
            Return (Package() {RERR, RDAT})
          }
          Stall (1)
          Local0++
        }
        Increment (RRCT)
        Return (Package() {RERR, RDAT})
      }

      Method (CONV, 1, Serialized, 0, IntObj, IntObj) {
        Local0 = Arg0
        Local2 = Local0 >> TH500_TWOS_COMP_SHIFT
        If (Local2 > 0) {
            //two's complement for negatives
            Local3 = (Local0 ^ XOR_MASK) + 1
            Local1 = 2732 - (Local3 / 100)
        } Else {
            Local1 = (Local0 / 100) + 2732
        }
        Return (Local1)
      }

      Method (TEMP, 1, Serialized, 0, IntObj, IntObj) {
        Local0 = Buffer(8){}
        CreateDWordField (Local0, 0x00, CMD)
        CreateDWordField (Local0, 0x04, ZONE)
        CMD = ZONE_TEMP
        ZONE = Arg0
        Local1 = \_SB.BPM3.BIPC (MRQ_THERMAL, Local0)
        CreateDWordField (DerefOf (Index (Local1, 0)), 0x00, ERR)
        if (ERR != 0) {
          Return (2732)
        }
        CreateDWordField (DerefOf (Index (Local1, 1)), 0x00, TEMP)
        Local2 = \_SB.BPM3.CONV(TEMP)
        Return (Local2)
      }

      Method (TELM, 2, Serialized, 0, IntObj, {IntObj, IntObj}) {
        Local0 = 0
        Local1 = 0
        Local2 = TH500_BPMP_IPC_CALL_INTERVAL_50MS
        Local3 = 0
        Local4 = 0
        Local5 = 0
        Local6 = 0

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
          Local1 = \_SB.BPM3.BIPC (MRQ_TELEMETRY, Local0)
          CreateDWordField (DerefOf (Index (Local1, 0)), 0x00, ERR)
          If (ERR != 0) {
            Return (PWR_METER_ERR_RETURN)
          }
          Store (DerefOf (Index (Local1, 1)), Local6)
          Or (Local6, TH500_AMAP_START_SOCKET_3, Local6)
          CreateQWordField (Local6, 0x00, BUFF)

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

          Store (Zero, PWRV)
          Switch (Arg0) {
            Case (TH500_MODULE_PWR) {
              If (Arg1 == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
                And (VFG0, TH500_MODULE_PWR_IDX_VALID_FLAG, Local5)
                If (Local5 > 0) {
                  Store (MPWR, PWRV)
                } Else {
                  Store (PWR_METER_ERR_RETURN, PWRV)
                }
              }
              If (Arg1 == PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC) {
                And (VFG2, TH500_MODULE_PWR_1SEC_IDX_VALID_FLAG, Local5)
                If (Local5 > 0) {
                  Store (MAPW, PWRV)
                } Else {
                  Store (PWR_METER_ERR_RETURN, PWRV)
                }
              }
              Break
            }

            Case (TH500_TH500_PWR) {
              If (Arg1 == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
                And (VFG0, TH500_TH500_PWR_IDX_VALID_FLAG, Local5)
                If (Local5 > 0) {
                  Store (TPWR, PWRV)
                } Else {
                  Store (PWR_METER_ERR_RETURN, PWRV)
                }
              }
              If (Arg1 == PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC) {
                And (VFG2, TH500_TH500_PWR_1SEC_IDX_VALID_FLAG, Local5)
                If (Local5 > 0) {
                  Store (TAPW, PWRV)
                } Else {
                  Store (PWR_METER_ERR_RETURN, PWRV)
                }
              }
              Break
            }

            Case (TH500_CPU_PWR) {
              If (Arg1 == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
                And (VFG0, TH500_CPU_PWR_IDX_VALID_FLAG, Local5)
                If (Local5 > 0) {
                  Store (CPWR, PWRV)
                } Else {
                  Store (PWR_METER_ERR_RETURN, PWRV)
                }
              }
              If (Arg1 == PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC) {
                And (VFG2, TH500_CPU_PWR_1SEC_IDX_VALID_FLAG, Local5)
                If (Local5 > 0) {
                  Store (CAPW, PWRV)
                } Else {
                  Store (PWR_METER_ERR_RETURN, PWRV)
                }
              }
              Break
            }

            Case (TH500_SOC_PWR) {
              If (Arg1 == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
                And (VFG0, TH500_SOC_PWR_IDX_VALID_FLAG, Local5)
                If (Local5 > 0) {
                  Store (SPWR, PWRV)
                } Else {
                  Store (PWR_METER_ERR_RETURN, PWRV)
                }
              }
              If (Arg1 == PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC) {
                And (VFG2, TH500_SOC_PWR_1SEC_IDX_VALID_FLAG, Local5)
                If (Local5 > 0) {
                  Store (SAPW, PWRV)
                } Else {
                  Store (PWR_METER_ERR_RETURN, PWRV)
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

        Local1 = \_SB.BPM3.BIPC (MRQ_PWR_LIMIT, Local0)
        CreateDWordField (DerefOf (Index (Local1, 0)), 0x00, ERR)
        if (ERR != 0) {
          Return (PWR_METER_UNKNOWN_HW_ERR)
        }
        Return (PWR_METER_SUCCESS)
      }

      Method (GPRL, 1, Serialized, 0, IntObj, IntObj) {
        Local0 = Buffer (8) {}

        CreateDWordField (Local0, 0x00, CMD)
        CreateDWordField (Local0, 0x04, LIID)

        CMD = TH500_PWR_LIMIT_CURR_CAP
        LIID = Arg0

        Local1 = \_SB.BPM3.BIPC (MRQ_PWR_LIMIT, Local0)
        CreateDWordField (DerefOf (Index (Local1, 0)), 0x00, ERR)
        if (ERR != 0) {
          Return (PWR_METER_ERR_RETURN)
        }
        CreateDWordField (DerefOf (Index (Local1, 1)), 0x00, PLIM)
        Return (PLIM)
      }
    }

    // Socket 1 CPUs 0-83
    External(\_SB.C003.C000)
    External(\_SB.C003.C002)
    External(\_SB.C003.C003)
    External(\_SB.C003.C004)
    External(\_SB.C003.C005)
    External(\_SB.C003.C006)
    External(\_SB.C003.C007)
    External(\_SB.C003.C008)
    External(\_SB.C003.C009)
    External(\_SB.C003.C00A)
    External(\_SB.C003.C00B)
    External(\_SB.C003.C00C)
    External(\_SB.C003.C00E)
    External(\_SB.C003.C010)
    External(\_SB.C003.C011)
    External(\_SB.C003.C012)
    External(\_SB.C003.C013)
    External(\_SB.C003.C014)
    External(\_SB.C003.C015)
    External(\_SB.C003.C016)
    External(\_SB.C003.C017)
    External(\_SB.C003.C018)
    External(\_SB.C003.C019)
    External(\_SB.C003.C01A)
    External(\_SB.C003.C01C)
    External(\_SB.C003.C01D)
    External(\_SB.C003.C01E)
    External(\_SB.C003.C01F)
    External(\_SB.C003.C020)
    External(\_SB.C003.C021)
    External(\_SB.C003.C022)
    External(\_SB.C003.C023)
    External(\_SB.C003.C024)
    External(\_SB.C003.C025)
    External(\_SB.C003.C026)
    External(\_SB.C003.C027)
    External(\_SB.C003.C028)
    External(\_SB.C003.C029)
    External(\_SB.C003.C02A)
    External(\_SB.C003.C02B)
    External(\_SB.C003.C02C)
    External(\_SB.C003.C02D)
    External(\_SB.C003.C02E)
    External(\_SB.C003.C02F)
    External(\_SB.C003.C030)
    External(\_SB.C003.C031)
    External(\_SB.C003.C032)
    External(\_SB.C003.C033)
    External(\_SB.C003.C034)
    External(\_SB.C003.C035)
    External(\_SB.C003.C036)
    External(\_SB.C003.C037)
    External(\_SB.C003.C038)
    External(\_SB.C003.C03A)
    External(\_SB.C003.C03B)
    External(\_SB.C003.C03C)
    External(\_SB.C003.C03D)
    External(\_SB.C003.C03E)
    External(\_SB.C003.C03F)
    External(\_SB.C003.C040)
    External(\_SB.C003.C041)
    External(\_SB.C003.C042)
    External(\_SB.C003.C043)
    External(\_SB.C003.C044)
    External(\_SB.C003.C046)
    External(\_SB.C003.C048)
    External(\_SB.C003.C049)
    External(\_SB.C003.C04A)
    External(\_SB.C003.C04B)
    External(\_SB.C003.C04C)
    External(\_SB.C003.C04D)
    External(\_SB.C003.C04E)
    External(\_SB.C003.C04F)
    External(\_SB.C003.C050)
    External(\_SB.C003.C051)
    External(\_SB.C003.C052)

    //---------------------------------------------------------------------
    // Thermal Zone for TLimit
    //---------------------------------------------------------------------

    ThermalZone (TZL3) {
      OperationRegion (TL30, SystemMemory, TH500_TLIMIT_SOCKET_3, TH500_TLIMIT_REGSIZE)
      Field (TL30, AnyAcc, NoLock, Preserve) {
        TLIM, 32
      }
      Method(_TMP) {
        Local0 = 0
        Local0 = \_SB.BPM3.CONV(TLIM)
        return (Local0)
      }
      Method(_CRT) { Return (TH500_THERMAL_ZONE_CRT + 2732) }
      Name (_STR, Unicode ("Thermal Zone Skt3 TLimit"))
    }

    //---------------------------------------------------------------------
    // Module Power Device Socket 3
    //---------------------------------------------------------------------
    Device (PM30)
    {
      Name (_HID, "ACPI000D")
      Name (_UID, 30)
      Name (_STA, 0)
      Name (CAI, PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS)
      Name (HWLT, 0)
      Name (MINP, 0x00000000)
      Name (MAXP, 0x00000000)

      // _PMD method return code - List of power meter devices
      Name (PMD, Package() {
        \_SB.PM30
      })

      // _PMC method result code
      Name (PMC, Package() {
        BIT (PWR_METER_SUPPORTS_MEASUREMENT),
        PWR_METER_MEASUREMENT_IN_MW,
        PWR_METER_MEASURE_OP_PWR,
        PWR_METER_MEASUREMENT_ACCURACY_100,
        \_SB.PM30.CAI,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC,
        PWR_METER_HYSTERESIS_MARGIN_UNKNOWN,
        PWR_METER_HW_LIMIT_RW,
        \_SB.PM30.MINP,
        \_SB.PM30.MAXP,
        "",                                   // Model Number - NULL
        "",                                   // Serial Number - NULL
        "Module Power Socket 3"               // OEM Information - "Module Power Socket 3"
      })

      Method (_PMC) {
        Return (PMC)
      }

      Method (_PAI, 1, Serialized, 0, IntObj, IntObj) {
        If (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS, CAI)
          Notify (\_SB.PM30, PWR_METER_NOTIFY_CONFIG)
          Return (PWR_METER_SUCCESS)
        } ElseIf (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC, CAI)
          Notify (\_SB.PM30, PWR_METER_NOTIFY_CONFIG)
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
        Local0 = \_SB.BPM3.TELM(TH500_MODULE_PWR, CAI)
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
    // TH500 Power Device Socket 3
    //---------------------------------------------------------------------
    Device (PM31)
    {
      Name (_HID, "ACPI000D")
      Name (_UID, 31)
      Name (_STA, 0)
      Name (CAI, PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS)
      Name (HWLT, 0)
      Name (MINP, 0xAAAAAAAA)
      Name (MAXP, 0xAAAAAAAA)

      // _PMD method return code - List of power meter devices
      Name (PMD, Package() {
        \_SB.PM31
      })

      // _PMC method result code
      Name (PMC, Package() {
        BIT (PWR_METER_SUPPORTS_MEASUREMENT) | BIT (PWR_METER_SUPPORTS_HW_LIMITS) |
        BIT (PWR_METER_SUPPORTS_NOTIFY_HW_LIMITS),
        PWR_METER_MEASUREMENT_IN_MW,
        PWR_METER_MEASURE_OP_PWR,
        PWR_METER_MEASUREMENT_ACCURACY_100,
        \_SB.PM31.CAI,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC,
        PWR_METER_HYSTERESIS_MARGIN_UNKNOWN,
        PWR_METER_HW_LIMIT_RW,
        \_SB.PM31.MINP,
        \_SB.PM31.MAXP,
        "",                                  // Model Number - NULL
        "",                                  // Serial Number - NULL
        "Grace Power Socket 3"               // OEM Information - "Grace Power Socket 3"
      })

      Method (_PMC) {
        Return (PMC)
      }

      Method (_PAI, 1, Serialized, 0, IntObj, IntObj) {
        If (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS, CAI)
          Notify (\_SB.PM31, PWR_METER_NOTIFY_CONFIG)
          Return (PWR_METER_SUCCESS)
        } ElseIf (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC, CAI)
          Notify (\_SB.PM31, PWR_METER_NOTIFY_CONFIG)
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
        Local0 = \_SB.BPM3.TELM(TH500_TH500_PWR, CAI)
        Return (Local0)
      }

      Method(_GHL) {
        Local0 = 0
        Local1 = TH500_PWR_LIMIT_ID_TH500_INP_EDPC_MW
        Local0 = \_SB.BPM3.GPRL (Local1)
        Return (Local0)
      }

      Method (_SHL, 1, Serialized, 0, IntObj, IntObj) {
        Local0 = 0
        Local1 = TH500_PWR_LIMIT_ID_TH500_INP_EDPC_MW
        If ((Arg0 <= MINP) && (Arg0 >= MAXP)) {
          Return (PWR_METER_OUT_OF_RANGE)
        }
        Local0 = \_SB.BPM3.SPRL (Local1, Arg0)
        If (Local0 == 0) {
          If (Arg0 != HWLT) {
            Store (Arg0, HWLT)
            Notify (\_SB.PM31, PWR_METER_NOTIFY_CAP)
          }
          Notify (\_SB.PM31, PWR_METER_NOTIFY_CAPPING)
        }
        Return (Local0)
      }
    }

    //---------------------------------------------------------------------
    // CPU Power Device Socket 3
    //---------------------------------------------------------------------
    Device (PM32)
    {
      Name (_HID, "ACPI000D")
      Name (_UID, 32)
      Name (_STA, 0)
      Name (CAI, PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS)
      Name (HWLT, 0)
      Name (MINP, 0x00000000)
      Name (MAXP, 0x00000000)

      // _PMD method return code - List of power meter devices
      Name (PMD, Package() {
        \_SB.PM32
      })

      // _PMC method result code
      Name (PMC, Package() {
        BIT (PWR_METER_SUPPORTS_MEASUREMENT),
        PWR_METER_MEASUREMENT_IN_MW,
        PWR_METER_MEASURE_OP_PWR,
        PWR_METER_MEASUREMENT_ACCURACY_100,
        \_SB.PM32.CAI,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC,
        PWR_METER_HYSTERESIS_MARGIN_UNKNOWN,
        PWR_METER_HW_LIMIT_RW,
        \_SB.PM32.MINP,
        \_SB.PM32.MAXP,
        "",                                // Model Number - NULL
        "",                                // Serial Number - NULL
        "CPU Power Socket 3"               // OEM Information - "CPU Power Socket 3"
      })

      Method (_PMC) {
        Return (PMC)
      }

      Method (_PAI, 1, Serialized, 0, IntObj, IntObj) {
        If (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS, CAI)
          Notify (\_SB.PM32, PWR_METER_NOTIFY_CONFIG)
          Return (PWR_METER_SUCCESS)
        } ElseIf (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC, CAI)
          Notify (\_SB.PM32, PWR_METER_NOTIFY_CONFIG)
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
        Local0 = \_SB.BPM3.TELM(TH500_CPU_PWR, CAI)
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
    // SOC Power Device Socket 3
    //---------------------------------------------------------------------
    Device (PM33)
    {
      Name (_HID, "ACPI000D")
      Name (_UID, 33)
      Name (_STA, 0)
      Name (CAI, PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS)
      Name (HWLT, 0)
      Name (MINP, 0x00000000)
      Name (MAXP, 0x00000000)

      // _PMD method return code - List of power meter devices
      Name (PMD, Package() {
        \_SB.PM33
      })

      // _PMC method result code
      Name (PMC, Package() {
        BIT (PWR_METER_SUPPORTS_MEASUREMENT),
        PWR_METER_MEASUREMENT_IN_MW,
        PWR_METER_MEASURE_OP_PWR,
        PWR_METER_MEASUREMENT_ACCURACY_100,
        \_SB.PM33.CAI,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS,
        PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC,
        PWR_METER_HYSTERESIS_MARGIN_UNKNOWN,
        PWR_METER_HW_LIMIT_RW,
        \_SB.PM33.MINP,
        \_SB.PM33.MAXP,
        "",                                // Model Number - NULL
        "",                                // Serial Number - NULL
        "SysIO Power Socket 3"             // OEM Information - "SysIO Power Socket 3"
      })

      Method (_PMC) {
        Return (PMC)
      }

      Method (_PAI, 1, Serialized, 0, IntObj, IntObj) {
        If (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_50MS, CAI)
          Notify (\_SB.PM33, PWR_METER_NOTIFY_CONFIG)
          Return (PWR_METER_SUCCESS)
        } ElseIf (Arg0 == PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC) {
          Store (PWR_METER_MEASUREMENT_SAMPLING_TIME_1SEC, CAI)
          Notify (\_SB.PM33, PWR_METER_NOTIFY_CONFIG)
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
        Local0 = \_SB.BPM3.TELM(TH500_SOC_PWR, CAI)
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
  } //Scope(_SB)
}
