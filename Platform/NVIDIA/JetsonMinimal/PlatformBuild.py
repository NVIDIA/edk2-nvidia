# SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent


###############################################################################
# Stuart build for NVIDIA Jetson UEFI firmware


from edk2nv.stuart import NVIDIASettingsManager, NVIDIAPlatformBuilder


class JetsonMinimalSettingsManager(NVIDIASettingsManager):
    ''' SettingsManager for NVIDIA's Jetson platform. '''

    def GetName(self):
        return "JetsonMinimal"

    def GetActiveScopes(self):
        return super().GetActiveScopes() + ["jetson"]

    def GetConfigFiles(self):
        return [
            self.GetEdk2NvidiaDir() + "Platform/NVIDIA/JetsonMinimal/Jetson.defconfig"
        ]

class PlatformBuilder(NVIDIAPlatformBuilder):
    ''' PlatformBuilder for NVIDIA's Jetson. '''
    SettingsManager = JetsonMinimalSettingsManager
