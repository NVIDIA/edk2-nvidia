# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent


###############################################################################
# Stuart build for NVIDIA StandaloneMm UEFI firmware


from pathlib import Path
from edk2nv.stuart import NVIDIASettingsManager, NVIDIAPlatformBuilder
from edk2nv.sptool import sptool


class StandaloneMmSettingsManager(NVIDIASettingsManager):
    ''' SettingsManager for NVIDIA's StandaloneMmJetson platform. '''

    def GetName(self):
        return "StandaloneMmJetson"

    def GetGuid(self):
        return "3a7930b4-77f6-463e-b2c1-2436b0af492b"

    def GetFirmwareVolume(self):
        return "FV/UEFI_MM.Fv"

    def GetDscName(self):
        return (self.GetEdk2NvidiaDir() +
                "Platform/NVIDIA/StandaloneMmJetson/StandaloneMmJetson.dsc"
                )

    def GetDtbManifestFile(self):
        ''' Return the name of the built DTB manifest file. '''
        return (
            "AARCH64/Silicon/NVIDIA/StandaloneMm/Manifest/ManifestStmmJetson/OUTPUT/"
            "StandaloneMmJetson.dtb"
        )

class PlatformBuilder(NVIDIAPlatformBuilder):
    ''' PlatformBuilder for NVIDIA's StandaloneMmJetson. '''
    SettingsManager = StandaloneMmSettingsManager

    def PlatformPostBuild(self):
        ''' Additional build steps for StandaloneMmJetson platform. '''
        ret = super().PlatformPostBuild()
        if ret != 0:
            return ret

        build_dir = Path(self.env.GetValue("BUILD_OUTPUT_BASE"))

        # Generate the StMM pkg file.
        target = self.settings.GetTarget()
        sptool(
            manifest_file=build_dir / self.settings.GetDtbManifestFile(),
            img_file=build_dir / self.settings.GetFirmwareVolume(),
            out_file=f"images/StandaloneMmJetson_{target}.pkg"
        )

        return 0
