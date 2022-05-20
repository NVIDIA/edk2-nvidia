# Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

import os
import sys
from pathlib import Path
from edk2toolext.invocables.edk2_update import UpdateSettingsManager
from edk2toolext.invocables.edk2_setup import (
    SetupSettingsManager, RequiredSubmodule
)
from edk2toolext.invocables.edk2_pr_eval import PrEvalSettingsManager
from edk2toolext.invocables.edk2_platform_build import BuildSettingsManager
from edk2toolext.invocables.edk2_ci_setup import CiSetupSettingsManager
from edk2toolext.invocables.edk2_ci_build import CiBuildSettingsManager
from edk2toollib.utility_functions import RunCmd
from edk2toolext.environment import shell_environment


__all__ = [
    "NVIDIASettingsManager",
    "NVIDIACiSettingsManager",
]


reason_setman = "Set in platform CiSettingsManager"


class AbstractNVIDIASettingsManager(UpdateSettingsManager,
                                    SetupSettingsManager):
    ''' Abstract stuart SettingsManager. '''

    def GetName(self):
        ''' Get the name of the platform being built. '''
        raise NotImplementedError(
            "GetName() must be implemented in NVIDIASettingsManager "
            "subclasses."
        )

    #######################################
    # Edk2InvocableSettingsInterface

    def GetWorkspaceRoot(self):
        ''' Return the root of the workspace.

            This implementation will defer to the WORKSPACE environment
            variable.
        '''
        workspace = os.getenv("WORKSPACE")
        if workspace:
            # Use pathlib to normalize it, but stuart requires it to be a
            # string, not a PathLike.
            return str(Path(os.getenv("WORKSPACE")))
        else:
            raise AttributeError("WORKSPACE not defined")

    def GetPackagesPath(self):
        ''' Return paths that should be mapped as edk2 PACKAGE_PATH.

            This is the list of directories, relative to the workspace, where
            the build will look for packages.
        '''
        # NOTE: These paths must use a trailing slash to ensure stuart treats
        # them properly when computing relative paths.
        packages_paths = [path + "/" for path in self._insert_pkgs_paths]
        packages_paths.extend([
            "edk2/BaseTools/", "edk2/", "edk2-platforms/", "edk2-nvidia/",
            "edk2-nvidia-non-osi/"
        ])

        return packages_paths

    def GetActiveScopes(self):
        ''' List of scopes we need for this platform. '''
        return ['edk2-build']

    def AddCommandLineOptions(self, parserObj):
        ''' Add command line options to the argparser '''
        super().AddCommandLineOptions(parserObj)

        parserObj.add_argument(
            '--insert-packages-path', dest='nvidia_pkgs_paths', type=str,
            help='Insert the given path into the beginning of the list of '
            'package paths.  Allows build time overrides.',
            action="append", default=[])
        parserObj.add_argument(
            '--require-submodule', dest='nvidia_submodules', type=str,
            help='Add a required submodule.',
            action="append", default=[])

    def RetrieveCommandLineOptions(self, args):
        ''' Retrieve command line options from the argparser namespace '''
        super().RetrieveCommandLineOptions(args)

        self._insert_pkgs_paths = args.nvidia_pkgs_paths
        self._added_submodules = args.nvidia_submodules

    #######################################
    # MultiPkgAwareSettingsInterface

    def GetPackagesSupported(self):
        ''' No-op.

            We don't use SetPackages(), so we don't need to implement this
            method.
        '''
        return []

    def GetArchitecturesSupported(self):
        ''' No-op.

            We don't use SetArchitectures(), so we don't need to implement this
            method.
        '''
        return []

    def GetTargetsSupported(self):
        ''' No-op.

            We don't use SetTargets(), so we don't need to implement this
            method.
        '''
        return []

    #######################################
    # SetupSettingsManager

    def GetRequiredSubmodules(self):
        ''' Return a list of git submodules required for NVIDIA builds.

            These will be populated during the stuart setup step.  Subclasses
            can override and append to the list.
        '''
        # We might like to be more selective in which submodules to load, but
        # stuart_setup doesn't handle nested submodules selectively.  You can
        # choose which top-level submodules to load and whether to recursively
        # load them, but you can't say "don't load jansson inside edk2". You
        # either get edk2 with jansson or no edk2 at all.
        #
        # Given this limitation, the best we can do is tell stuart about our
        # top-level package paths and let it load them recursively.
        submodules = []
        submodules.extend([
            RequiredSubmodule(submodule)
            for submodule in self._added_submodules
        ])
        submodules.extend([
            RequiredSubmodule("edk2"),
            RequiredSubmodule("edk2-platforms"),
            RequiredSubmodule("edk2-nvidia"),
            RequiredSubmodule("edk2-nvidia-non-osi"),
        ])
        return submodules


class NVIDIASettingsManager(AbstractNVIDIASettingsManager,
                            PrEvalSettingsManager, BuildSettingsManager,
                            metaclass=shell_environment.Singleton):
    ''' Base SettingsManager for various stuart build steps.

        Implements the SettingsManager for update, setup, pr-eval, and build
        steps for portions common to all NVIDIA platforms.  Platforms must
        provide a subclass in their PlatformBuid.py.
    '''

    #######################################
    # Edk2InvocableSettingsInterface

    def RetrieveCommandLineOptions(self, args):
        ''' Retrieve command line options from the argparser namespace '''
        super().RetrieveCommandLineOptions(args)

        if hasattr(args, "nvidia_target"):
            # We're in the build step.  Pick up the target argument we added in
            # builder.py.  See the comments in AddPlatformCommandLineOptions()
            # to understand this bit of hack.
            self._target = args.nvidia_target
        else:
            # We're not in the build step.  Make sure the multi-pkg aware
            # options were not used.  We don't support them.
            if (args.packageList or args.requested_arch or
                    args.requested_target):
                print("The --pkg, --arch, --target are not supported")
                sys.exit(1)

    #######################################
    # NVIDIA settings
    # - Additional settings for NVIDIAPlatformBuilder

    def GetFirmwareVersionBase(self):
        ''' Return the base firmware version as a string.

            The return from this method will be used as the prefix when setting
            BUILDID_STRING.
        '''
        raise NotImplementedError(
            "GetFirmwareVersionBase() must be implemented in "
            "NVIDIASettingsManager subclasses."
        )

    def GetFirmwareVersion(self):
        ''' Return the firmware version as a string.

            This implementation will attempt to generate a unique string from
            the git revision and append it to GetFirmwareVersionBase().  If the
            GIT_SYNC_REVISION environment variable is set, it will be used
            instead.

            The return from this method will be used to set BUILDID_STRING.
            Subclasses may override it to generate the BUILDID differently.
        '''
        base = self.GetFirmwareVersionBase()

        if os.getenv("GIT_SYNC_REVISION") is not None:
            return base + "-" + os.getenv("GIT_SYNC_REVISION")
        else:
            import io
            result = io.StringIO()
            ret = RunCmd("git", "-C edk2-nvidia describe --always --dirty",
                         workingdir=self.GetWorkspaceRoot(), outstream=result)
            if (ret == 0):
                return base + "-" + result.getvalue()
            else:
                return base + "-Unknown"

    def GetFirmwareVolume(self):
        ''' Return the flash volume to use when generating the firmware image.

            Must match a flash volume in the platform's FDF file.

            The return must be a string and identify a path relative to the
            platform's build output directory.
        '''
        raise NotImplementedError(
            "GetFirmwareVolume() must be implemented in "
            "NVIDIASettingsManager subclasses."
        )

    def GetFirmwareImageFile(self):
        ''' Return the name of the firmware image.

            The firmware image will be generated from the firmware volume and
            stored to this filename.  This default implementation
            will use "images/uefi_{platform_name}_{target}.bin".

            Returned as a string identifying a path relative to the workspace
            root.
        '''
        platform_name = self.GetName()
        target = self.GetTarget()
        return f"images/uefi_{platform_name}_{target}.bin"

    def GetDscName(self):
        ''' Optionally return the path to the platform's DSC file.

            If `None`, the value is taken from target.txt.  Otherwise, this
            will override target.txt

            The path must be relative to GetWorkspaceRoot().

            This will be used to set ACTIVE_PLATFORM.
        '''
        return None

    def GetToolchainTag(self):
        ''' Optionally return the toolchain identifier.

            Defaults to GCC5.  If `None`, the value is taken from target.txt.
            Otherwise, this will override target.txt

            This will be used to set TOOL_CHAIN_TAG.
        '''
        return "GCC5"

    def GetReportTypes(self):
        ''' Return the build report types.

            This will be used to set BUILDREPORT_TYPES.
        '''
        return ("PCD LIBRARY FLASH DEPEX BUILD_FLAGS FIXED_ADDRESS "
                "HASH EXECUTION_ORDER")

    def GetReportFile(self):
        ''' Return the build report filename.

            The report will copied to this location after the build.  Returned
            as a string.  This default implementation will use
            "reports/{platform_name}_{target}.report"
        '''
        platform_name = self.GetName()
        target = self.GetTarget()
        return f"reports/{platform_name}_{target}.report"

    def GetCrossCompilerPrefix(self):
        ''' Return prefix to the toolchain.

            This implementation will defer to the CROSS_COMPILER_PREFIX
            environment variable.
        '''
        prefix = os.getenv("CROSS_COMPILER_PREFIX")
        if prefix:
            # Use pathlib to normalize it, but stuart requires it
            # to be a string, not a PathLike.
            return str(Path(os.getenv("CROSS_COMPILER_PREFIX")))
        else:
            raise AttributeError("CROSS_COMPILER_PREFIX not defined")

    def GetTarget(self):
        ''' Return the value of the --target option.
        '''
        return self._target

    def GetConfDirName(self):
        ''' Return the name of the Conf directory.

            This directory name will include the target so that targets
            can be built in parallel.  Returned as a string.  This default
            implementation will use "Conf/{platform_name}/{target}".
        '''
        platform_name = self.GetName()
        target = self.GetTarget()
        return f"Conf/{platform_name}/{target}"

    def GetVariablesDescFile(self):
        ''' Optionally, the name of this platform's VariablesDesc.json file.

            If the platform does not have a or require a VariablesDesc.json
            file, this method should return `None`.
        '''
        return None

    def GetVariableImageFile(self):
        ''' Return the name of the variable store image.

            The variable store will be generated and stored to this filename.
            Returned as a string.  This default implementation will use
            "images/variables_{platform_name}_{target}.bin".
        '''
        platform_name = self.GetName()
        target = self.GetTarget()
        return f"images/variables_{platform_name}_{target}.bin"

    def GetBootAppName(self):
        ''' Optionally, the build name of this platform's boot app.

            If the platform does not have a boot app, this method should return
            `None`.

            Returns a path relative to the build directory.
        '''
        return None

    def GetBootAppFile(self):
        ''' Return the file name of the boot app.

            We'll copy the built boot app to this location.  This default
            implementation will use
            "images/BOOTAA64_{platform_name}_{target}.efi".

            Returns a path relative to the workspace.
        '''
        platform_name = self.GetName()
        target = self.GetTarget()
        return f"images/BOOTAA64_{platform_name}_{target}.efi"

    def GetBuildDirFile(self):
        ''' Return the file name of the build dir file.

            This file will contain the full path to the build directory. Useful
            when an upstream build system needs access to arbitrary build
            artifacts.  This default implementation will use
            "images/builddir_{platform_name}_{target}.txt".

            Returns a path relative to the workspace.
        '''
        platform_name = self.GetName()
        target = self.GetTarget()
        return f"images/builddir_{platform_name}_{target}.txt"


class NVIDIACiSettingsManager(AbstractNVIDIASettingsManager,
                              CiSetupSettingsManager, CiBuildSettingsManager,
                              metaclass=shell_environment.Singleton):
    ''' Base SettingsManager for various stuart CI steps.

        Implement some sane defaults for CI steps.
    '''
    def __init__(self, *args, **kwargs):
        ''' Initialize the SettingsManager and set up build environment.

        This is the best opportunity we have to set the build environment.
        Unlike the "build" step, the "ci_build" step doesn't provide a callback
        like SetPlatformEnv().
        '''
        super().__init__(*args, **kwargs)
        env = shell_environment.GetBuildVars()

        # TOOL_CHAIN_TAG
        # - If not provided by the SettingsManager, the value in target.txt
        # will be taken.
        toolchain_tag = self.GetToolchainTag()
        if toolchain_tag:
            env.SetValue("TOOL_CHAIN_TAG", toolchain_tag, reason_setman)

    def GetArchitecturesSupported(self):
        ''' return iterable of edk2 architectures supported by this build '''
        return ("X64",)

    def GetTargetsSupported(self):
        ''' return iterable of edk2 target tags supported by this build '''
        return ("NOOPT",)

    def GetActiveScopes(self):
        # Add the "host-based-test" scope, which will trigger the plugin that
        # runs the unittests after the build.
        return super().GetActiveScopes() + ["cibuild", "host-based-test"]

    #######################################
    # NVIDIA settings

    def GetToolchainTag(self):
        ''' Return the toolchain identifier.

            At this time, we only support CI runs with the GCC5 toolchain.

            This will be used to set TOOL_CHAIN_TAG.
        '''
        return "GCC5"
