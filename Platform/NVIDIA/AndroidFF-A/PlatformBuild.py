# SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent


###############################################################################
# Stuart build for NVIDIA AndroidFF-A UEFI firmware


from edk2nv.stuart import NVIDIASettingsManager, NVIDIAPlatformBuilder


class AndroidFFASettingsManager(NVIDIASettingsManager):
    ''' SettingsManager for NVIDIA's AndroidFF-A platform. '''

    def GetName(self):
        return "AndroidFF-A"

    def GetGuid(self):
        return "c193fdd8-7109-47a7-bf0e-02a94962d787"

    def GetActiveScopes(self):
        return super().GetActiveScopes() + ["jetson"]

    def GetConfigFiles(self):
        return [
            self.GetEdk2NvidiaDir() + "Platform/NVIDIA/AndroidFF-A/AndroidFF-A.defconfig"
        ]

class PlatformBuilder(NVIDIAPlatformBuilder):
    ''' PlatformBuilder for NVIDIA's AndroidFF-A. '''
    SettingsManager = AndroidFFASettingsManager
