# SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent


###############################################################################
# Stuart build for NVIDIA ServerIgx UEFI firmware


from edk2nv.stuart import NVIDIASettingsManager, NVIDIAPlatformBuilder


class ServerIgxSettingsManager(NVIDIASettingsManager):
    ''' SettingsManager for NVIDIA's ServerIgx platform. '''

    def GetName(self):
        return "ServerIgx"

    def GetDtbPath(self):
        return "AARCH64/Silicon/NVIDIA/Tegra/DeviceTree/DeviceTree/OUTPUT"

    def GetConfigFiles(self):
        return [
            self.GetEdk2NvidiaDir() + "Platform/NVIDIA/ServerIgx/ServerIgx.defconfig"
        ]

class PlatformBuilder(NVIDIAPlatformBuilder):
    ''' PlatformBuilder for NVIDIA's ServerIgx. '''
    SettingsManager = ServerIgxSettingsManager
