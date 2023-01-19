/** @file
  SSDT for TH500 Socket 1 devices

  Copyright (c) 2022 - 2023, NVIDIA Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  Template for [SSDT] ACPI Table (AML byte code table)
**/

#include <TH500/TH500Definitions.h>
#include <Protocol/BpmpIpc.h>

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
      Name (PSVT, ((TH500_THERMAL_ZONE_PSV * 10) + 2732))
      Name (CRTT, ((TH500_THERMAL_ZONE_CRT * 10) + 2732))
      Name (TBUF, 0xFFFFFFFFFFFFFFFF)
      Name (TIME, 0xFF)
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

      Name (LSTM, 0)

      Method (TELM, 1, Serialized, 0, IntObj, IntObj) {
        Local0 = Buffer(8) {}
        Local1 = Buffer(384) {}
        Local2 = 500000
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
          If (LOr (Local4, (Arg0 != 0))) {
            Local1 = \_SB.BPM1.BIPC (MRQ_TELEMETRY, Local0)
            CreateDWordField (DerefOf (Index (Local1, 0)), 0x00, ERR)
            If (ERR != 0) {
              Return (0)
            }
            Store (Timer(), LSTM)
          }
        } Else {
          Local1 = \_SB.BPM1.BIPC (MRQ_TELEMETRY, Local0)
          CreateDWordField (DerefOf (Index (Local1, 0)), 0x00, ERRR)
          If (ERRR != 0) {
            Return (0)
          }
        }
        Return (TBUF)
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

    // Thermal Zones (upto 12 per socket)
    // TZ10 to TZ1B

    ThermalZone (TZ10) {
      Method(_TMP) { Return (\_SB.BPM1.TEMP (TH500_THERMAL_ZONE_CPU0) )} // get current temp
      Name(_PSL, Package (){
                            \_SB.C001.C000,
                            \_SB.C001.C002,
                            \_SB.C001.C003,
                            \_SB.C001.C004,
                            \_SB.C001.C005,
                            \_SB.C001.C006,
                            \_SB.C001.C007,
                            \_SB.C001.C008,
                            \_SB.C001.C009 }) // passive cooling devices
      Method(_PSV) { Return (\_SB.BPM1.PSVT) }
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM1.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt1 CPU0"))
    }

    ThermalZone (TZ11) {
      Method(_TMP) { Return (\_SB.BPM1.TEMP (TH500_THERMAL_ZONE_CPU1) )} // get current temp
      Name(_PSL, Package (){
                            \_SB.C001.C00A,
                            \_SB.C001.C00B,
                            \_SB.C001.C00C,
                            \_SB.C001.C00E,
                            \_SB.C001.C010,
                            \_SB.C001.C011,
                            \_SB.C001.C012,
                            \_SB.C001.C013,
                            \_SB.C001.C014 }) // passive cooling devices
      Method(_PSV) { Return (\_SB.BPM1.PSVT) }
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM1.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt1 CPU1"))
    }

    ThermalZone (TZ12) {
      Method(_TMP) { Return (\_SB.BPM1.TEMP (TH500_THERMAL_ZONE_CPU2) )} // get current temp
      Name(_PSL, Package (){
                            \_SB.C001.C015,
                            \_SB.C001.C016,
                            \_SB.C001.C017,
                            \_SB.C001.C018,
                            \_SB.C001.C019,
                            \_SB.C001.C01A,
                            \_SB.C001.C01C,
                            \_SB.C001.C01D,
                            \_SB.C001.C01E }) // passive cooling devices
      Method(_PSV) { Return (\_SB.BPM1.PSVT) }
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM1.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt1 CPU2"))
    }

    ThermalZone (TZ13) {
      Method(_TMP) { Return (\_SB.BPM1.TEMP (TH500_THERMAL_ZONE_CPU3) )} // get current temp
      Name(_PSL, Package (){
                            \_SB.C001.C01F,
                            \_SB.C001.C020,
                            \_SB.C001.C021,
                            \_SB.C001.C022,
                            \_SB.C001.C023,
                            \_SB.C001.C024,
                            \_SB.C001.C025,
                            \_SB.C001.C026,
                            \_SB.C001.C027 }) // passive cooling devices
      Method(_PSV) { Return (\_SB.BPM1.PSVT) }
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM1.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt1 CPU3"))
    }

    ThermalZone (TZ14) {
      Method(_TMP) { Return (\_SB.BPM1.TEMP (TH500_THERMAL_ZONE_SOC0) )} // get current temp
      Name(_PSL, Package (){
                            \_SB.C001.C028,
                            \_SB.C001.C029,
                            \_SB.C001.C02A,
                            \_SB.C001.C02B,
                            \_SB.C001.C02C,
                            \_SB.C001.C02D,
                            \_SB.C001.C02E,
                            \_SB.C001.C02F,
                            \_SB.C001.C030 }) // passive cooling devices
      Method(_PSV) { Return (\_SB.BPM1.PSVT) }
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM1.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt1 SOC0"))
    }

    ThermalZone (TZ15) {
      Method(_TMP) { Return (\_SB.BPM1.TEMP (TH500_THERMAL_ZONE_SOC1) )} // get current temp
      Name(_PSL, Package (){
                            \_SB.C001.C031,
                            \_SB.C001.C032,
                            \_SB.C001.C033,
                            \_SB.C001.C034,
                            \_SB.C001.C035,
                            \_SB.C001.C036,
                            \_SB.C001.C037,
                            \_SB.C001.C038,
                            \_SB.C001.C03A }) // passive cooling devices
      Method(_PSV) { Return (\_SB.BPM1.PSVT) }
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM1.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt1 SOC1"))
    }

    ThermalZone (TZ16) {
      Method(_TMP) { Return (\_SB.BPM1.TEMP (TH500_THERMAL_ZONE_SOC2) )} // get current temp
      Name(_PSL, Package (){,
                            \_SB.C001.C03B,
                            \_SB.C001.C03C,
                            \_SB.C001.C03D,
                            \_SB.C001.C03E,
                            \_SB.C001.C03F,
                            \_SB.C001.C040,
                            \_SB.C001.C041,
                            \_SB.C001.C042,
                            \_SB.C001.C043 }) // passive cooling devices
      Method(_PSV) { Return (\_SB.BPM1.PSVT) }
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM1.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt1 SOC2"))
    }

    ThermalZone (TZ17) {
      Method(_TMP) { Return (\_SB.BPM1.TEMP (TH500_THERMAL_ZONE_SOC3) )} // get current temp
      Name(_PSL, Package (){
                            \_SB.C001.C044,
                            \_SB.C001.C046,
                            \_SB.C001.C048,
                            \_SB.C001.C049,
                            \_SB.C001.C04A,
                            \_SB.C001.C04B,
                            \_SB.C001.C04C,
                            \_SB.C001.C04D,
                            \_SB.C001.C04E }) // passive cooling devices
      Method(_PSV) { Return (\_SB.BPM1.PSVT) }
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM1.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt1 SOC3"))
    }

    ThermalZone (TZ18) {
      Method(_TMP) { Return (\_SB.BPM1.TEMP (TH500_THERMAL_ZONE_SOC4) )} // get current temp
      Name(_PSL, Package (){
                            \_SB.C001.C04F,
                            \_SB.C001.C050,
                            \_SB.C001.C051,
                            \_SB.C001.C052 }) // passive cooling devices
      Method(_PSV) { Return (\_SB.BPM1.PSVT) }
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM1.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt1 SOC4"))
    }

    ThermalZone (TZ19) {
      Method(_TMP) { Return (\_SB.BPM1.TEMP (TH500_THERMAL_ZONE_TJ_MAX) )} // get current temp
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM1.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt1 TJMax"))
    }

    ThermalZone (TZ1A) {
      Method(_TMP) { Return (\_SB.BPM1.TEMP (TH500_THERMAL_ZONE_TJ_MIN) )} // get current temp
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM1.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt1 TJMin"))
    }

    ThermalZone (TZ1B) {
      Method(_TMP) { Return (\_SB.BPM1.TEMP (TH500_THERMAL_ZONE_TJ_AVG) )} // get current temp
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM1.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt1 TJAvg"))
    }

    //---------------------------------------------------------------------
    // Module Power Device Socket 1
    //---------------------------------------------------------------------
    Device (PM10)
    {
      Name (_HID, "ACPI000D")
      Name (_UID, 10)
      Name (CAI, 50)
      Name (CNT, 0)
      Name (MFLG, 16)
      Name (MAFG, 256)

      // _PMD method return code - List of power meter devices
      Name (PMD, Package() {
        \_SB.PM10
      })

      // _PMC method result code
      Name (PMC, Package() {
        0x00000001,                           // Supported capabilities - Measurement
        0x00000000,                           // Measurement Unit - mW
        0x00000001,                           // Measurement Type - Output Power
        0x000186A0,                           // Measurement Accuracy - 100.000%
        0x00000032,                           // Measurement Sampling Time - 50ms
        0x00000032,                           // Minimum Averaging Interval - 50ms
        0x000003E8,                           // Maximum Averaging Interval - 1s
        0xFFFFFFFF,                           // Hysteresis Margin - Information is unavailable
        0x00000000,                           // Hardware Limit Is Configurable - The limit is read-only
        0x00000000,                           // Minimum Configurable Hardware Limit - Ignored
        0x00000000,                           // Maximum Configurable Hardware Limit - Ignored
        "",                                   // Model Number - NULL
        "",                                   // Serial Number - NULL
        "Module Power Socket 1"               // OEM Information - "Module Power Socket 1"
      })

      Method (_PMC) {
        Return (PMC)
      }

      Method (_PAI, 1, Serialized, 0, IntObj, IntObj) {
        If (Arg0 == 50) {
          Store (50, CAI)
          Return (0)
        } ElseIf (Arg0 == 1000) {
          Store (1000, CAI)
          Return (0)
        }
        Return (1)
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
        If (Local0 == 0) {
          Return (0xFFFFFFFF)
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

        If (CAI == 50) {
          And (VFG0, MFLG, Local1)
          If (Local1 > 0) {
            Return (MPWR)
          }
        } Else {
          And (VFG2, MAFG, Local1)
          If (Local1 > 0) {
            Return (MAPW)
          }
        }
        Return (0xFFFFFFFF)
      }
    }

    //---------------------------------------------------------------------
    // TH500 Power Device Socket 1
    //---------------------------------------------------------------------
    Device (PM11)
    {
      Name (_HID, "ACPI000D")
      Name (_UID, 11)
      Name (CAI, 50)
      Name (CNT, 0)
      Name (MFLG, 32)
      Name (MAFG, 512)

      // _PMD method return code - List of power meter devices
      Name (PMD, Package() {
        \_SB.PM11
      })

      // _PMC method result code
      Name (PMC, Package() {
        0x00000001,                          // Supported capabilities - Measurement
        0x00000000,                          // Measurement Unit - mW
        0x00000001,                          // Measurement Type - Output Power
        0x000186A0,                          // Measurement Accuracy - 100.000%
        0x00000032,                          // Measurement Sampling Time - 50ms
        0x00000032,                          // Minimum Averaging Interval - 50ms
        0x000003E8,                          // Maximum Averaging Interval - 1s
        0xFFFFFFFF,                          // Hysteresis Margin - Information is unavailable
        0x00000000,                          // Hardware Limit Is Configurable - The limit is read-only
        0x00000000,                          // Minimum Configurable Hardware Limit - Ignored
        0x00000000,                          // Maximum Configurable Hardware Limit - Ignored
        "",                                  // Model Number - NULL
        "",                                  // Serial Number - NULL
        "TH500 Power Socket 1"               // OEM Information - "TH500 Power Socket 1"
      })

      Method (_PMC) {
        Return (PMC)
      }

      Method (_PAI, 1, Serialized, 0, IntObj, IntObj) {
        If (Arg0 == 50) {
          Store (50, CAI)
          Return (0)
        } ElseIf (Arg0 == 1000) {
          Store (1000, CAI)
          Return (0)
        }
        Return (1)
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
        If (Local0 == 0) {
          Return (0xFFFFFFFF)
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

        If (CAI == 50) {
          And (VFG0, MFLG, Local1)
          If (Local1 > 0) {
            Return (TPWR)
          }
        } Else {
          And (VFG2, MAFG, Local1)
          If (Local1 > 0) {
            Return (TAPW)
          }
        }
        Return (0xFFFFFFFF)
      }
    }

    //---------------------------------------------------------------------
    // CPU Power Device Socket 1
    //---------------------------------------------------------------------
    Device (PM12)
    {
      Name (_HID, "ACPI000D")
      Name (_UID, 12)
      Name (CAI, 50)
      Name (CNT, 0)
      Name (MFLG, 64)
      Name (MAFG, 1024)

      // _PMD method return code - List of power meter devices
      Name (PMD, Package() {
        \_SB.PM12
      })

      // _PMC method result code
      Name (PMC, Package() {
        0x00000001,                        // Supported capabilities - Measurement
        0x00000000,                        // Measurement Unit - mW
        0x00000001,                        // Measurement Type - Output Power
        0x000186A0,                        // Measurement Accuracy - 100.000%
        0x00000032,                        // Measurement Sampling Time - 50ms
        0x00000032,                        // Minimum Averaging Interval - 50ms
        0x000003E8,                        // Maximum Averaging Interval - 1s
        0xFFFFFFFF,                        // Hysteresis Margin - Information is unavailable
        0x00000000,                        // Hardware Limit Is Configurable - The limit is read-only
        0x00000000,                        // Minimum Configurable Hardware Limit - Ignored
        0x00000000,                        // Maximum Configurable Hardware Limit - Ignored
        "",                                // Model Number - NULL
        "",                                // Serial Number - NULL
        "CPU Power Socket 1"               // OEM Information - "CPU Power Socket 1"
      })

      Method (_PMC) {
        Return (PMC)
      }

      Method (_PAI, 1, Serialized, 0, IntObj, IntObj) {
        If (Arg0 == 50) {
          Store (50, CAI)
          Return (0)
        } ElseIf (Arg0 == 1000) {
          Store (1000, CAI)
          Return (0)
        }
        Return (1)
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
        If (Local0 == 0) {
          Return (0xFFFFFFFF)
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

        If (CAI == 50) {
          And (VFG0, MFLG, Local1)
          If (Local1 > 0) {
            Return (CPWR)
          }
        } Else {
          And (VFG2, MAFG, Local1)
          If (Local1 > 0) {
            Return (CAPW)
          }
        }
        Return (0xFFFFFFFF)
      }
    }

    //---------------------------------------------------------------------
    // SOC Power Device Socket 1
    //---------------------------------------------------------------------
    Device (PM13)
    {
      Name (_HID, "ACPI000D")
      Name (_UID, 13)
      Name (CAI, 50)
      Name (CNT, 0)
      Name (MFLG, 128)
      Name (MAFG, 2048)

      // _PMD method return code - List of power meter devices
      Name (PMD, Package() {
        \_SB.PM13
      })

      // _PMC method result code
      Name (PMC, Package() {
        0x00000001,                        // Supported capabilities - Measurement
        0x00000000,                        // Measurement Unit - mW
        0x00000001,                        // Measurement Type - Output Power
        0x000186A0,                        // Measurement Accuracy - 100.000%
        0x00000032,                        // Measurement Sampling Time - 50ms
        0x00000032,                        // Minimum Averaging Interval - 50ms
        0x000003E8,                        // Maximum Averaging Interval - 1s
        0xFFFFFFFF,                        // Hysteresis Margin - Information is unavailable
        0x00000000,                        // Hardware Limit Is Configurable - The limit is read-only
        0x00000000,                        // Minimum Configurable Hardware Limit - Ignored
        0x00000000,                        // Maximum Configurable Hardware Limit - Ignored
        "",                                // Model Number - NULL
        "",                                // Serial Number - NULL
        "SoC Power Socket 1"               // OEM Information - "SoC Power Socket 1"
      })

      Method (_PMC) {
        Return (PMC)
      }

      Method (_PAI, 1, Serialized, 0, IntObj, IntObj) {
        If (Arg0 == 50) {
          Store (50, CAI)
          Return (0)
        } ElseIf (Arg0 == 1000) {
          Store (1000, CAI)
          Return (0)
        }
        Return (1)
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
        If (Local0 == 0) {
          Return (0xFFFFFFFF)
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

        If (CAI == 50) {
          And (VFG0, MFLG, Local1)
          If (Local1 > 0) {
            Return (SPWR)
          }
        } Else {
          And (VFG2, MAFG, Local1)
          If (Local1 > 0) {
            Return (SAPR)
          }
        }
        Return (0xFFFFFFFF)
      }
    }
  } //Scope(_SB)
}
