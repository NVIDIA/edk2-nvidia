# SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

###############################################################################
# Stuart build for NVIDIA StandaloneMm UEFI firmware


from edk2nv.stuart import NVIDIASettingsManager, NVIDIAPlatformBuilder


class StandaloneMmOpteeSettingsManager(NVIDIASettingsManager):
    ''' SettingsManager for NVIDIA's StandaloneMmOptee platform. '''

    def GetName(self):
        return "StandaloneMmOptee"

    def GetGuid(self):
        return "fb0e2152-1441-49e0-b376-5f8593d66678"

    def GetFirmwareVolume(self):
        return "FV/UEFI_MM.Fv"

    def GetDscName(self):
        return (self.GetEdk2NvidiaDir() +
                "Platform/NVIDIA/StandaloneMmOptee/StandaloneMmOptee.dsc"
                )


class PlatformBuilder(NVIDIAPlatformBuilder):
    ''' PlatformBuilder for NVIDIA's StandaloneMmOptee. '''
    SettingsManager = StandaloneMmOpteeSettingsManager
