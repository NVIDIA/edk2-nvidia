# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent


###############################################################################
# Stuart build for NVIDIA StandaloneMm UEFI firmware


from pathlib import Path
from edk2nv.stuart import NVIDIASettingsManager, NVIDIAPlatformBuilder
from edk2nv.sptool import sptool


class StandaloneMmSettingsManager(NVIDIASettingsManager):
    ''' SettingsManager for NVIDIA's StandaloneMmT268 platform. '''

    def GetName(self):
        return "StandaloneMmT268"

    def GetGuid(self):
        return "ec47fe53-1b2e-4af0-bfa9-c38b78a7907d"

    def GetFirmwareVolume(self):
        return "FV/UEFI_MM.Fv"

    def GetDscName(self):
        return (self.GetEdk2NvidiaDir() +
                "Platform/NVIDIA/StandaloneMmT268/StandaloneMmT268.dsc"
                )

    def GetDtbManifestFile(self):
        ''' Return the name of the built DTB manifest file. '''
        return (
            "AARCH64/Silicon/NVIDIA/StandaloneMm/Manifest/ManifestStmmJetson/OUTPUT/"
            "StandaloneMmJetson.dtb"
        )

class PlatformBuilder(NVIDIAPlatformBuilder):
    ''' PlatformBuilder for NVIDIA's StandaloneMmT268. '''
    SettingsManager = StandaloneMmSettingsManager

    def PlatformPostBuild(self):
        ''' Additional build steps for StandaloneMmT268 platform. '''
        ret = super().PlatformPostBuild()
        if ret != 0:
            return ret

        build_dir = Path(self.env.GetValue("BUILD_OUTPUT_BASE"))

        # Generate the StMM pkg file.
        target = self.settings.GetTarget()
        sptool(
            manifest_file=build_dir / self.settings.GetDtbManifestFile(),
            img_file=build_dir / self.settings.GetFirmwareVolume(),
            out_file=f"images/StandaloneMmT268_{target}.pkg"
        )

        return 0
