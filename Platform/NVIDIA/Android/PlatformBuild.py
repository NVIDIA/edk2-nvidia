# SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent


###############################################################################
# Stuart build for NVIDIA Android UEFI firmware


from edk2nv.stuart import NVIDIASettingsManager, NVIDIAPlatformBuilder


class AndroidSettingsManager(NVIDIASettingsManager):
    ''' SettingsManager for NVIDIA's Android platform. '''

    def GetName(self):
        return "Android"

    def GetActiveScopes(self):
        return super().GetActiveScopes() + ["jetson"]

    def GetConfigFiles(self):
        return [
            self.GetEdk2NvidiaDir() + "Platform/NVIDIA/Android/Android.defconfig"
        ]

class PlatformBuilder(NVIDIAPlatformBuilder):
    ''' PlatformBuilder for NVIDIA's Android. '''
    SettingsManager = AndroidSettingsManager
