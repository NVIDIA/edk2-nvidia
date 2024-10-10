# SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent


###############################################################################
# Stuart build for NVIDIA Device Tree overlays


from edk2nv.stuart import NVIDIASettingsManager, NVIDIAPlatformBuilder


class DeviceTreeSettingsManager(NVIDIASettingsManager):
    """SettingsManager for NVIDIA's Device Treey Overlays."""

    def GetName(self):
        return "DeviceTree"

    def GetGuid(self):
        return "4a17d121-7753-4341-b4e4-009550283be0"

    def GetDscName(self):
        return self.GetEdk2NvidiaDir() + "Platform/NVIDIA/DeviceTree/DeviceTree.dsc"

    def GetFirmwareVolume(self):
        return None

    def GetDtbPath(self):
        return "AARCH64/Silicon/NVIDIA/Tegra/DeviceTree/DeviceTree/OUTPUT"


class PlatformBuilder(NVIDIAPlatformBuilder):
    """PlatformBuilder for NVIDIA's Device Treey Overlays."""

    SettingsManager = DeviceTreeSettingsManager
