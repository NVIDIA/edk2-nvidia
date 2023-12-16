# SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent


###############################################################################
# Stuart build for NVIDIA Jetson UEFI firmware


from edk2nv.stuart import NVIDIASettingsManager, NVIDIAPlatformBuilder


class JetsonMinimalSettingsManager(NVIDIASettingsManager):
    ''' SettingsManager for NVIDIA's Jetson platform. '''

    def GetName(self):
        return "JetsonMinimal"

    def GetGuid(self):
        return "f98bcf32-fd20-4ba9-ada4-e0406947ca3c"

    def GetActiveScopes(self):
        return super().GetActiveScopes() + ["jetson"]

    def GetFirmwareVolume(self):
        return "FV/UEFI_NS.Fv"

    def GetDscName(self):
        return "edk2-nvidia/Platform/NVIDIA/NVIDIA.common.dsc"

    def GetConfigFiles(self):
        return ["edk2-nvidia/Platform/NVIDIA/JetsonMinimal/Jetson.defconfig"]

class PlatformBuilder(NVIDIAPlatformBuilder):
    ''' PlatformBuilder for NVIDIA's Jetson. '''
    SettingsManager = JetsonMinimalSettingsManager
