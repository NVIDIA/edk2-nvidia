/** @file
  SSDT for TH500 Socket 1 devices

  Copyright (c) 2022, NVIDIA Corporation. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  Template for [SSDT] ACPI Table (AML byte code table)
**/

#include <TH500/TH500Definitions.h>
#include <Protocol/BpmpIpc.h>

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
      Name (PSVT, ((TH500_THERMAL_ZONE_PSV * 10) + 2732))
      Name (CRTT, ((TH500_THERMAL_ZONE_CRT * 10) + 2732))
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

      Method (BIPC, 2, Serialized, 0, PkgObj, {IntObj, BuffObj}) {
        OperationRegion (DRBL, SystemMemory, BPMP_DOORBELL_SOCKET_3, BPMP_DOORBELL_SIZE)
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
        Local1 = \_SB.BPM3.BIPC (MRQ_THERMAL, Local0)
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

    // Thermal Zones (upto 12 per socket)
    // TZ30 to TZ3B

    ThermalZone (TZ30) {
      Method(_TMP) { Return (\_SB.BPM3.TEMP (TH500_THERMAL_ZONE_CPU0) )} // get current temp
      Name(_PSL, Package (){
                            \_SB.C003.C000,
                            \_SB.C003.C002,
                            \_SB.C003.C003,
                            \_SB.C003.C004,
                            \_SB.C003.C005,
                            \_SB.C003.C006,
                            \_SB.C003.C007,
                            \_SB.C003.C008,
                            \_SB.C003.C009 }) // passive cooling devices
      Method(_PSV) { Return (\_SB.BPM3.PSVT) }
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM3.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt 3 CPU0"))
    }

    ThermalZone (TZ31) {
      Method(_TMP) { Return (\_SB.BPM3.TEMP (TH500_THERMAL_ZONE_CPU1) )} // get current temp
      Name(_PSL, Package (){
                            \_SB.C003.C00A,
                            \_SB.C003.C00B,
                            \_SB.C003.C00C,
                            \_SB.C003.C00E,
                            \_SB.C003.C010,
                            \_SB.C003.C011,
                            \_SB.C003.C012,
                            \_SB.C003.C013,
                            \_SB.C003.C014 }) // passive cooling devices
      Method(_PSV) { Return (\_SB.BPM3.PSVT) }
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM3.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt 3 CPU1"))
    }

    ThermalZone (TZ32) {
      Method(_TMP) { Return (\_SB.BPM3.TEMP (TH500_THERMAL_ZONE_CPU2) )} // get current temp
      Name(_PSL, Package (){
                            \_SB.C003.C015,
                            \_SB.C003.C016,
                            \_SB.C003.C017,
                            \_SB.C003.C018,
                            \_SB.C003.C019,
                            \_SB.C003.C01A,
                            \_SB.C003.C01C,
                            \_SB.C003.C01D,
                            \_SB.C003.C01E }) // passive cooling devices
      Method(_PSV) { Return (\_SB.BPM3.PSVT) }
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM3.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt 3 CPU2"))
    }

    ThermalZone (TZ33) {
      Method(_TMP) { Return (\_SB.BPM3.TEMP (TH500_THERMAL_ZONE_CPU3) )} // get current temp
      Name(_PSL, Package (){
                            \_SB.C003.C01F,
                            \_SB.C003.C020,
                            \_SB.C003.C021,
                            \_SB.C003.C022,
                            \_SB.C003.C023,
                            \_SB.C003.C024,
                            \_SB.C003.C025,
                            \_SB.C003.C026,
                            \_SB.C003.C027 }) // passive cooling devices
      Method(_PSV) { Return (\_SB.BPM3.PSVT) }
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM3.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt 3 CPU3"))
    }

    ThermalZone (TZ34) {
      Method(_TMP) { Return (\_SB.BPM3.TEMP (TH500_THERMAL_ZONE_SOC0) )} // get current temp
      Name(_PSL, Package (){
                            \_SB.C003.C028,
                            \_SB.C003.C029,
                            \_SB.C003.C02A,
                            \_SB.C003.C02B,
                            \_SB.C003.C02C,
                            \_SB.C003.C02D,
                            \_SB.C003.C02E,
                            \_SB.C003.C02F,
                            \_SB.C003.C030 }) // passive cooling devices
      Method(_PSV) { Return (\_SB.BPM3.PSVT) }
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM3.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt 3 SOC0"))
    }

    ThermalZone (TZ35) {
      Method(_TMP) { Return (\_SB.BPM3.TEMP (TH500_THERMAL_ZONE_SOC1) )} // get current temp
      Name(_PSL, Package (){
                            \_SB.C003.C031,
                            \_SB.C003.C032,
                            \_SB.C003.C033,
                            \_SB.C003.C034,
                            \_SB.C003.C035,
                            \_SB.C003.C036,
                            \_SB.C003.C037,
                            \_SB.C003.C038,
                            \_SB.C003.C03A }) // passive cooling devices
      Method(_PSV) { Return (\_SB.BPM3.PSVT) }
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM3.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt 3 SOC1"))
    }

    ThermalZone (TZ36) {
      Method(_TMP) { Return (\_SB.BPM3.TEMP (TH500_THERMAL_ZONE_SOC2) )} // get current temp
      Name(_PSL, Package (){,
                            \_SB.C003.C03B,
                            \_SB.C003.C03C,
                            \_SB.C003.C03D,
                            \_SB.C003.C03E,
                            \_SB.C003.C03F,
                            \_SB.C003.C040,
                            \_SB.C003.C041,
                            \_SB.C003.C042,
                            \_SB.C003.C043 }) // passive cooling devices
      Method(_PSV) { Return (\_SB.BPM3.PSVT) }
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM3.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt 3 SOC2"))
    }

    ThermalZone (TZ37) {
      Method(_TMP) { Return (\_SB.BPM3.TEMP (TH500_THERMAL_ZONE_SOC3) )} // get current temp
      Name(_PSL, Package (){
                            \_SB.C003.C044,
                            \_SB.C003.C046,
                            \_SB.C003.C048,
                            \_SB.C003.C049,
                            \_SB.C003.C04A,
                            \_SB.C003.C04B,
                            \_SB.C003.C04C,
                            \_SB.C003.C04D,
                            \_SB.C003.C04E }) // passive cooling devices
      Method(_PSV) { Return (\_SB.BPM3.PSVT) }
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM3.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt 3 SOC3"))
    }

    ThermalZone (TZ38) {
      Method(_TMP) { Return (\_SB.BPM3.TEMP (TH500_THERMAL_ZONE_SOC4) )} // get current temp
      Name(_PSL, Package (){
                            \_SB.C003.C04F,
                            \_SB.C003.C050,
                            \_SB.C003.C051,
                            \_SB.C003.C052 }) // passive cooling devices
      Method(_PSV) { Return (\_SB.BPM3.PSVT) }
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM3.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt 3 SOC4"))
    }

    ThermalZone (TZ39) {
      Method(_TMP) { Return (\_SB.BPM3.TEMP (TH500_THERMAL_ZONE_TJ_MAX) )} // get current temp
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM3.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt 3 TJMax"))
    }

    ThermalZone (TZ3A) {
      Method(_TMP) { Return (\_SB.BPM3.TEMP (TH500_THERMAL_ZONE_TJ_MIN) )} // get current temp
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM3.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt 3 TJMin"))
    }

    ThermalZone (TZ3B) {
      Method(_TMP) { Return (\_SB.BPM3.TEMP (TH500_THERMAL_ZONE_TJ_AVG) )} // get current temp
      Name(_TC1, 1)  // TODO: get correct values
      Name(_TC2, 1)  // TODO: get correct values
      Method(_CRT) { Return (\_SB.BPM3.CRTT) }
      Name(_TSP, 1)  // TODO: get correct values
      Name(_TZP, TEMP_POLL_TIME_100MS)
      Name (_STR, Unicode ("Thermal Zone Skt 3 TJAvg"))
    }
  } //Scope(_SB)
}