# SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent


###############################################################################
# Stuart build for NVIDIA Jetson UEFI firmware


from edk2nv.stuart import NVIDIASettingsManager, NVIDIAPlatformBuilder


class JetsonSettingsManager(NVIDIASettingsManager):
    ''' SettingsManager for NVIDIA's Jetson platform. '''

    def GetName(self):
        return "Jetson"

    def GetGuid(self):
        return "b175f7b7-0cb0-446e-b338-0e0d0f688de8"

    def GetFirmwareVolume(self):
        return "FV/UEFI_NS.Fv"

    def GetBootAppName(self):
        return "AARCH64/L4TLauncher.efi"

    def GetDscName(self):
        return "edk2-nvidia/Platform/NVIDIA/NVIDIA.common.dsc"

    def GetDtbPath(self):
        return "AARCH64/Silicon/NVIDIA/Tegra/DeviceTree/DeviceTree/OUTPUT"

    def GetConfigFiles(self):
        return ["edk2-nvidia/Platform/NVIDIA/Jetson/Jetson.defconfig"]

class PlatformBuilder(NVIDIAPlatformBuilder):
    ''' PlatformBuilder for NVIDIA's Jetson. '''
    SettingsManager = JetsonSettingsManager
