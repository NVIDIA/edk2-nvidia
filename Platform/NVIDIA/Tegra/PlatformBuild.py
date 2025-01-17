# SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent


###############################################################################
# Stuart build for NVIDIA Tegra UEFI firmware
#
# To build a Tegra image, an initial defconfig must be provided, which will
# determine the SOC and high-level feature set.  From there, the image can be
# further configured.
#
# The image name will be taken from the initial defconfig file's name.


import sys
import shutil
from pathlib import Path
from edk2nv.stuart import NVIDIASettingsManager, NVIDIAPlatformBuilder


class TegraSettingsManager(NVIDIASettingsManager):
    ''' SettingsManager for NVIDIA Tegra images. '''

    def AddCommandLineOptions(self, parserObj):
        super().AddCommandLineOptions(parserObj)

        parserObj.add_argument(
            '--init-defconfig', dest='defconfig', type=str,
            help='Initialize configuration using the given defconfig file.  '
                 'Required, if this is the first build in this workspace.  '
                 'Optional, if the workspace has already been configured.',
            action="store", default=None)

    def RetrieveCommandLineOptions(self, args):
        super().RetrieveCommandLineOptions(args)

        self._defconfig = args.defconfig

        # If we were given a defconfig, initialize the workspace.
        if self._defconfig:
            defconfig_path = Path(self._defconfig)

            # Verify the defconfig file is something we can use.
            if not defconfig_path.exists():
                print(f"defconfig file does not exist: {defconfig_path}")
                sys.exit(1)
            if not defconfig_path.is_file():
                print(f"defconfig is not a file: {defconfig_path}")
                sys.exit(1)

            # Take the image name from the defconfig file name. For example, if
            # we were given "t264_general.defconfig", then the image name will
            # be "t264_general".
            image_name = defconfig_path.stem

            # Make sure we have a useful image name.
            if not image_name or image_name == "defconfig":
                print(f"defconfig filename must be in the format 'image_name.defconfig'")
                sys.exit(1)

            # Remember the image name for future builds.
            ws_dir = Path(self.GetWorkspaceRoot())
            image_name_path = ws_dir / self.GetImageNameFile()
            image_name_path.parent.mkdir(parents=True, exist_ok=True)
            image_name_path.write_text(image_name)

            # Create config directory
            nvconf_dir = ws_dir / self.GetNvidiaConfigDir()
            nvconf_dir.mkdir(parents=True, exist_ok=True)

            # Backup any existing defconfig, before we clobber it.
            image_defconfig_path = nvconf_dir / "defconfig"
            if image_defconfig_path.exists():
                image_defconfig_path.rename(image_defconfig_path.with_suffix(".bkp"))

            # Init the config with the defconfig
            shutil.copyfile(defconfig_path, image_defconfig_path)

    def GetName(self):
        ''' Get the name of the platform being built.

            This implementation will return the contents of
            "nvidia-config/ImageName", if it exists.  Otherwise, it will raise
            an error requiring the --init-defconfig argument to be passed.
        '''
        ws_dir = Path(self.GetWorkspaceRoot())
        image_name_path = ws_dir / self.GetImageNameFile()

        if not image_name_path.exists():
            print("The workspace is not configured.")
            print("Use --init-defconfig to configure the workspace with an initial defconfig.")
            sys.exit(1)

        return image_name_path.read_text()

    def GetConfigFiles(self):
        ''' Return the list of defconfig files that will used for this build.

            This implementation will return nvidia-config/<name>/defconfig.
        '''
        return [str(Path(self.GetNvidiaConfigDir()) / "defconfig")]

    def GetImageNameFile(self):
        ''' Get the name of the file containing the image name. '''
        return str(Path(self.GetNvidiaConfigRoot()) / "ImageName")


class PlatformBuilder(NVIDIAPlatformBuilder):
    ''' PlatformBuilder for NVIDIA Tegra images. '''
    SettingsManager = TegraSettingsManager
