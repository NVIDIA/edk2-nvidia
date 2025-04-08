# Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

###############################################################################
# Stuart build for NVIDIA StandaloneMm UEFI firmware


from edk2nv.stuart import NVIDIASettingsManager, NVIDIAPlatformBuilder


class StandaloneMmOpteeSettingsManager(NVIDIASettingsManager):
    ''' SettingsManager for NVIDIA's StandaloneMmOptee platform. '''

    def GetName(self):
        return "StandaloneMmOptee"

    def GetFirmwareVersionBase(self):
        return "202210.7"

    def GetFirmwareVolume(self):
        return "FV/UEFI_MM.Fv"

    def GetDscName(self):
        return ("edk2-nvidia/Platform/NVIDIA/StandaloneMmOptee/"
                "StandaloneMmOptee.dsc")


class PlatformBuilder(NVIDIAPlatformBuilder):
    ''' PlatformBuilder for NVIDIA's StandaloneMmOptee. '''
    SettingsManager = StandaloneMmOpteeSettingsManager
