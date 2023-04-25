# Copyright (c) 2021-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent


###############################################################################
# Stuart build for NVIDIA Server UEFI firmware


from edk2nv.stuart import NVIDIASettingsManager, NVIDIAPlatformBuilder


class ServerSettingsManager(NVIDIASettingsManager):
    ''' SettingsManager for NVIDIA's Server platform. '''

    def GetName(self):
        return "Server"

    def GetActiveScopes(self):
        return super().GetActiveScopes() + ["server"]

    def GetPackagesPath(self):
        return super().GetPackagesPath() + [
            "edk2-nvidia-server-gpu-sdk"
        ]

    def GetFirmwareVolume(self):
        return "FV/UEFI_NS.Fv"

    def GetDscName(self):
        return ("edk2-nvidia/Platform/NVIDIA/Server/Server.dsc")

    def GetConfigFiles(self):
        return ["edk2-nvidia/Platform/NVIDIA/Server/Server.defconfig"]

class PlatformBuilder(NVIDIAPlatformBuilder):
    ''' PlatformBuilder for NVIDIA's Server. '''
    SettingsManager = ServerSettingsManager
