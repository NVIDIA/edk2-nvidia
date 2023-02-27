# Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent


###############################################################################
# Stuart build for NVIDIA TegraVirt UEFI firmware


from edk2nv.stuart import NVIDIASettingsManager, NVIDIAPlatformBuilder


class TegraVirtSettingsManager(NVIDIASettingsManager):
    ''' SettingsManager for NVIDIA's TegraVirt platform. '''

    def GetName(self):
        return "TegraVirt"

    def GetActiveScopes(self):
        return super().GetActiveScopes() + ["tegravirt"]

    def GetFirmwareVolume(self):
        return "FV/FVMAIN_COMPACT.Fv"

    def GetDscName(self):
        return "edk2-nvidia/Platform/NVIDIA/TegraVirt/TegraVirt.dsc"


class PlatformBuilder(NVIDIAPlatformBuilder):
    ''' PlatformBuilder for NVIDIA's TegraVirt. '''
    SettingsManager = TegraVirtSettingsManager
