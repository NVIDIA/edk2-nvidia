# SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent


###############################################################################
# Stuart build for NVIDIA StandaloneMm UEFI firmware


from pathlib import Path
from edk2nv.stuart import NVIDIASettingsManager, NVIDIAPlatformBuilder
from edk2nv.sptool import sptool


class StandaloneMmSettingsManager(NVIDIASettingsManager):
    ''' SettingsManager for NVIDIA's StandaloneMm platform. '''

    def GetName(self):
        return "StandaloneMm"

    def GetGuid(self):
        return "fb0e2152-1441-49e0-b376-5f8593d66678"

    def GetFirmwareVolume(self):
        return "FV/UEFI_MM.Fv"

    def GetDscName(self):
        return (self.GetEdk2NvidiaDir() +
                "Platform/NVIDIA/StandaloneMm/StandaloneMm.dsc"
                )

    def GetDtbManifestFile(self):
        ''' Return the name of the built DTB manifest file. '''
        return (
            "AARCH64/Silicon/NVIDIA/StandaloneMm/Manifest/Manifest/OUTPUT/"
            "StandaloneMm.dtb"
        )

class PlatformBuilder(NVIDIAPlatformBuilder):
    ''' PlatformBuilder for NVIDIA's StandaloneMm. '''
    SettingsManager = StandaloneMmSettingsManager

    def PlatformPostBuild(self):
        ''' Additional build steps for StandaloneMm platform. '''
        ret = super().PlatformPostBuild()
        if ret != 0:
            return ret

        build_dir = Path(self.env.GetValue("BUILD_OUTPUT_BASE"))

        # Generate the StMM pkg file.
        target = self.settings.GetTarget()
        sptool(
            manifest_file=build_dir / self.settings.GetDtbManifestFile(),
            img_file=build_dir / self.settings.GetFirmwareVolume(),
            out_file=f"images/StandaloneMm_{target}.pkg"
        )

        return 0
