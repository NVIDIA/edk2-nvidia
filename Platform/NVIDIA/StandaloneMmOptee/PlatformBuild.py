# Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

###############################################################################
# Stuart build for NVIDIA StandaloneMm UEFI firmware
#
# Run with:
# $ build_uefi.sh -c \
#      edk2-nvidia/Platform/NVIDIA/StandaloneMmOptee/PlatformBuild.py


from edk2nv.stuart import NVIDIASettingsManager, NVIDIAPlatformBuilder


class StandaloneMmOpteeSettingsManager(NVIDIASettingsManager):
    ''' SettingsManager for NVIDIA's StandaloneMmOptee platform. '''

    def GetName(self):
        return "StandaloneMmOptee"

    def GetFirmwareVersionBase(self):
        return "v1.1.1"

    def GetFirmwareVolume(self):
        return "FV/UEFI_MM.Fv"

    def GetDscName(self):
        return ("edk2-nvidia/Platform/NVIDIA/StandaloneMmOptee/"
                "StandaloneMmOptee.dsc")


class PlatformBuilder(NVIDIAPlatformBuilder):
    ''' PlatformBuilder for NVIDIA's StandaloneMmOptee. '''
    SettingsManager = StandaloneMmOpteeSettingsManager
