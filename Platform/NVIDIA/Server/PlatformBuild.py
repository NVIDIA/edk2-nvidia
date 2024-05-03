# SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent


###############################################################################
# Stuart build for NVIDIA Server UEFI firmware


from edk2nv.stuart import NVIDIASettingsManager, NVIDIAPlatformBuilder


class ServerSettingsManager(NVIDIASettingsManager):
    ''' SettingsManager for NVIDIA's Server platform. '''

    def GetName(self):
        return "Server"

    def GetGuid(self):
        return "9aef2e52-dead-4f63-b895-3a504a3e63c4"

    def GetPackagesPath(self):
        return super().GetPackagesPath() + [
            "edk2-nvidia-server-gpu-sdk/", "edk2-redfish-client/"
        ]

    def GetFirmwareVolume(self):
        return "FV/UEFI_NS.Fv"

    def GetDscName(self):
        return self.GetEdk2NvidiaDir() + "Platform/NVIDIA/NVIDIA.common.dsc"

    def GetConfigFiles(self):
        return [
            self.GetEdk2NvidiaDir() + "Platform/NVIDIA/Server/Server.defconfig"
        ]

class PlatformBuilder(NVIDIAPlatformBuilder):
    ''' PlatformBuilder for NVIDIA's Server. '''
    SettingsManager = ServerSettingsManager
