# Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent


###############################################################################
# Stuart build for NVIDIA Jetson UEFI firmware
#
# Run with:
# $ build_uefi.sh -c \
#      edk2-nvidia/Platform/NVIDIA/Jetson/PlatformBuild.py


from edk2nv.stuart import NVIDIASettingsManager, NVIDIAPlatformBuilder


class JetsonSettingsManager(NVIDIASettingsManager):
    ''' SettingsManager for NVIDIA's Jetson platform. '''

    def GetName(self):
        return "Jetson"

    def GetActiveScopes(self):
        return super().GetActiveScopes() + ["jetson"]

    def GetFirmwareVersionBase(self):
        return "v1.1.2"

    def GetFirmwareVolume(self):
        return "FV/UEFI_NS.Fv"

    def GetBootAppName(self):
        return "AARCH64/L4TLauncher.efi"

    def GetDscName(self):
        return "edk2-nvidia/Platform/NVIDIA/Jetson/Jetson.dsc"

    def GetVariablesDescFile(self):
        return "edk2-nvidia/Platform/NVIDIA/Jetson/JetsonVariablesDesc.json"


class PlatformBuilder(NVIDIAPlatformBuilder):
    ''' PlatformBuilder for NVIDIA's Jetson. '''
    SettingsManager = JetsonSettingsManager
