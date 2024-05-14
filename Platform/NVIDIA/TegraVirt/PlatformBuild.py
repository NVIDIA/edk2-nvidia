# SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent


###############################################################################
# Stuart build for NVIDIA TegraVirt UEFI firmware


from edk2nv.stuart import NVIDIASettingsManager, NVIDIAPlatformBuilder


class TegraVirtSettingsManager(NVIDIASettingsManager):
    ''' SettingsManager for NVIDIA's TegraVirt platform. '''

    def GetName(self):
        return "TegraVirt"

    def GetGuid(self):
        return "fb0e2152-1441-49e0-b376-5f8593d66678"

    def GetFirmwareVolume(self):
        return "FV/FVMAIN_COMPACT.Fv"

    def GetDscName(self):
        return self.GetEdk2NvidiaDir() + "Platform/NVIDIA/TegraVirt/TegraVirt.dsc"


class PlatformBuilder(NVIDIAPlatformBuilder):
    ''' PlatformBuilder for NVIDIA's TegraVirt. '''
    SettingsManager = TegraVirtSettingsManager
