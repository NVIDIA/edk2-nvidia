# @file
# Script to Build OVMF UEFI firmware
#
# Copyright (c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
# Portions provided under the following terms:
# Copyright (c) 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.
#
# SPDX-FileCopyrightText: Copyright (c) <year> NVIDIA CORPORATION & AFFILIATES
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary

##
import os
import logging
import io
import sys
import datetime
import shutil

from edk2toolext.environment import shell_environment
from edk2toolext.environment.uefi_build import UefiBuilder
from edk2toolext.invocables.edk2_platform_build import BuildSettingsManager
from edk2toolext.invocables.edk2_setup import SetupSettingsManager, RequiredSubmodule
from edk2toolext.invocables.edk2_update import UpdateSettingsManager
from edk2toolext.invocables.edk2_pr_eval import PrEvalSettingsManager
from edk2toollib.utility_functions import RunCmd

sys.path.insert(0,os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))),"Silicon/NVIDIA/Tools"))
from FormatUefiBinary import FormatUefiBinary
from GenVariableStore import GenVariableStore


    # ####################################################################################### #
    #                                Common Configuration                                     #
    # ####################################################################################### #
class CommonPlatform():
    ''' Common settings for this platform.  Define static data here and use
        for the different parts of stuart
    '''
    ProductName = "Jetson"
    PackagesSupported = ("Jetson",)
    ArchSupported = ("AARCH64")
    ToolChain = ("GCC5")
    TargetsSupported = ("DEBUG", "RELEASE", "NOOPT")
    Scopes = ('jetson', 'edk2-build')
    WorkspaceRoot = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    CompilerPrefix = os.path.join (os.getenv("P4ROOT"), "sw/embedded/tools/toolchains/bootlin/aarch64--glibc--stable-2020.08-1/bin/aarch64-linux-")
    FirmwareVolume="FV/UEFI_NS.Fv"
    VariableJson = os.path.join(WorkspaceRoot, "Platform/NVIDIA/Jetson/JetsonVariablesDesc.json")
    PcdDataBase = "AARCH64/MdeModulePkg/Universal/PCD/Dxe/Pcd/OUTPUT/DXEPcdDataBase.raw"
    GrubLaunch = "AARCH64/L4TLauncher.efi"

    ReportTypes ="PCD LIBRARY FLASH DEPEX BUILD_FLAGS FIXED_ADDRESS HASH EXECUTION_ORDER"
    RevisionBase = "v1.1.2"

    @classmethod
    def GetDscName(cls, InternalBuild: bool) -> str:
        ''' return the DSC given the architectures requested.

        InternalBuild: Is this an internal build
        '''
        dsc = f"Platform/NVIDIA/Jetson/Jetson.dsc"
        return dsc

    def GetOutputLauncher(self):
        return os.path.join(self.GetWorkspaceRoot(), "images/BOOTAA64.efi")

    def GetOutputBinary(self):
        return os.path.join(self.GetWorkspaceRoot(), "images/uefi_jetson.bin")

    def GetVariableBinary(self):
        return os.path.join(self.GetWorkspaceRoot(), "images/uefi_jetson_variables.bin")

    def GetReportFile(self):
        return os.path.join(self.GetWorkspaceRoot(), "reports/uefi_jetson.report")

    def IsProjectMuBuild(self):
        return os.path.isdir(os.path.join(self.GetWorkspaceRoot(),"MU_BASECORE"))

    def GetFirmwareVersion(self):
        if (os.getenv("GIT_SYNC_REVISION") is not None):
            return CommonPlatform.RevisionBase+"-"+os.getenv("GIT_SYNC_REVISION")
        else:
            result = io.StringIO()
            ret = RunCmd("git", "describe --always --dirty", workingdir=self.GetWorkspaceRoot(), outstream=result)
            if (ret == 0):
                return CommonPlatform.RevisionBase+"-"+result.getvalue()
            else:
                return CommonPlatform.RevisionBase+"-Unknown"


    def GetPackagesSupported(self):
        ''' return iterable containing package paths.
        '''
        packages = []
        alt_packages = ["edk2", "edk2-platforms", "Common/OtherSources"]

        # To avoid maintenance of this file for every new submodule
        # lets just parse the .gitmodules and add each if not already in list.
        # The GetRequiredSubmodules is designed to allow a build to optimize
        # the desired submodules but it isn't necessary for this repository.
        result = io.StringIO()
        ret = RunCmd("git", "config --file .gitmodules --get-regexp path", workingdir=self.GetWorkspaceRoot(), outstream=result)
        # Cmd output is expected to look like:
        # submodule.CryptoPkg/Library/OpensslLib/openssl.path CryptoPkg/Library/OpensslLib/openssl
        # submodule.SoftFloat.path ArmPkg/Library/ArmSoftFloatLib/berkeley-softfloat-3
        if ret == 0:
            for line in result.getvalue().splitlines():
                _, _, path = line.partition(" ")
                if path is not None:
                    if path not in [x for x in packages]:
                        packages.append(path) # add it with recursive since we don't know
        for package in alt_packages:
            if (os.path.isdir(os.path.join(self.GetWorkspaceRoot(), package))):
                if package not in [x for x in packages]:
                    packages.append(package)

        return packages

    # ####################################################################################### #
    #                         Configuration for Update & Setup                                #
    # ####################################################################################### #
class SettingsManager(UpdateSettingsManager, SetupSettingsManager, PrEvalSettingsManager):

    def GetPackagesSupported(self):
        ''' return iterable of edk2 packages supported by this build.
        These should be edk2 workspace relative paths '''
        return CommonPlatform.PackagesSupported

    def GetArchitecturesSupported(self):
        ''' return iterable of edk2 architectures supported by this build '''
        return CommonPlatform.ArchSupported

    def GetTargetsSupported(self):
        ''' return iterable of edk2 target tags supported by this build '''
        return CommonPlatform.TargetsSupported

    def GetRequiredSubmodules(self):
        ''' return iterable containing RequiredSubmodule objects.
        If no RequiredSubmodules return an empty iterable
        '''
        rs = []

        # To avoid maintenance of this file for every new submodule
        # lets just parse the .gitmodules and add each if not already in list.
        # The GetRequiredSubmodules is designed to allow a build to optimize
        # the desired submodules but it isn't necessary for this repository.
        result = io.StringIO()
        ret = RunCmd("git", "config --file .gitmodules --get-regexp path", workingdir=self.GetWorkspaceRoot(), outstream=result)
        # Cmd output is expected to look like:
        # submodule.CryptoPkg/Library/OpensslLib/openssl.path CryptoPkg/Library/OpensslLib/openssl
        # submodule.SoftFloat.path ArmPkg/Library/ArmSoftFloatLib/berkeley-softfloat-3
        if ret == 0:
            for line in result.getvalue().splitlines():
                _, _, path = line.partition(" ")
                if path is not None:
                    if path not in [x.path for x in rs]:
                        rs.append(RequiredSubmodule(path, True)) # add it with recursive since we don't know
        return rs

    def SetArchitectures(self, list_of_requested_architectures):
        ''' Confirm the requests architecture list is valid and configure SettingsManager
        to run only the requested architectures.

        Raise Exception if a list_of_requested_architectures is not supported
        '''
        unsupported = set(list_of_requested_architectures) - set(self.GetArchitecturesSupported())
        if(len(unsupported) > 0):
            errorString = ( "Unsupported Architecture Requested: " + " ".join(unsupported))
            logging.critical( errorString )
            raise Exception( errorString )
        self.ActualArchitectures = list_of_requested_architectures

    def GetWorkspaceRoot(self):
        ''' get WorkspacePath '''
        return CommonPlatform.WorkspaceRoot

    def GetActiveScopes(self):
        ''' return tuple containing scopes that should be active for this process '''
        return CommonPlatform.Scopes

    def FilterPackagesToTest(self, changedFilesList: list, potentialPackagesList: list) -> list:
        ''' Filter other cases that this package should be built
        based on changed files. This should cover things that can't
        be detected as dependencies. '''
        build_these_packages = []
        possible_packages = potentialPackagesList.copy()
        for f in changedFilesList:
            # BaseTools files that might change the build
            if "BaseTools" in f:
                if os.path.splitext(f) not in [".txt", ".md"]:
                    build_these_packages = possible_packages
                    break

            # if the azure pipeline platform template file changed
            if "platform-build-run-steps.yml" in f:
                build_these_packages = possible_packages
                break

        return build_these_packages

    def GetPlatformDscAndConfig(self) -> tuple:
        ''' If a platform desires to provide its DSC then Policy 4 will evaluate if
        any of the changes will be built in the dsc.

        The tuple should be (<workspace relative path to dsc file>, <input dictionary of dsc key value pairs>)
        '''
        return (CommonPlatform.GetDscName(False), {})

    def GetPackagesPath(self):
        ''' Return a list of paths that should be mapped as edk2 PackagesPath '''
        return CommonPlatform.GetPackagesSupported(self)

    # ####################################################################################### #
    #                         Actual Configuration for Platform Build                         #
    # ####################################################################################### #
class PlatformBuilder( UefiBuilder, BuildSettingsManager):
    def __init__(self):
        UefiBuilder.__init__(self)

    def AddCommandLineOptions(self, parserObj):
        ''' Add command line options to the argparser '''

    def RetrieveCommandLineOptions(self, args):
        '''  Retrieve command line options from the argparser '''

    def GetWorkspaceRoot(self):
        ''' get WorkspacePath '''
        return CommonPlatform.WorkspaceRoot

    def GetPackagesPath(self):
        ''' Return a list of workspace relative paths that should be mapped as edk2 PackagesPath '''
        return CommonPlatform.GetPackagesSupported(self)

    def GetActiveScopes(self):
        ''' return tuple containing scopes that should be active for this process '''
        return CommonPlatform.Scopes

    def GetName(self):
        ''' Get the name of the repo, platform, or product being build '''
        ''' Used for naming the log file, among others '''
        return CommonPlatform.ProductName

    def GetLoggingLevel(self, loggerType):
        ''' Get the logging level for a given type
        base == lowest logging level supported
        con  == Screen logging
        txt  == plain text file logging
        md   == markdown file logging
        '''
        return logging.DEBUG

    def SetPlatformEnv(self):
        logging.debug("PlatformBuilder SetPlatformEnv")
        self.env.SetValue("PRODUCT_NAME", CommonPlatform.ProductName, "Platform Hardcoded")
        if (CommonPlatform.IsProjectMuBuild(self)):
            shell_environment.GetBuildVars().SetValue("BLD_*_BUILD_PROJECT_TYPE", "PROJECT_MU", "Platform Hardcoded")
        else:
            shell_environment.GetBuildVars().SetValue("BLD_*_BUILD_PROJECT_TYPE", "EDK2", "Platform Hardcoded")
        shell_environment.GetBuildVars().SetValue("BLD_*_BUILD_DATE_TIME", datetime.datetime.now().astimezone().replace(microsecond=0).isoformat(), "Dynamic")
        shell_environment.GetBuildVars().SetValue("BLD_*_BUILDID_STRING", CommonPlatform.GetFirmwareVersion(self), "Dynamic")
        shell_environment.GetBuildVars().SetValue("TARGET_ARCH",CommonPlatform.ArchSupported, "Platform Hardcoded")
        shell_environment.GetBuildVars().SetValue("ACTIVE_PLATFORM", CommonPlatform.GetDscName(False), "Platform Hardcoded")
        shell_environment.GetBuildVars().SetValue("TOOL_CHAIN_TAG", CommonPlatform.ToolChain, "Platform Hardcoded")
        shell_environment.GetBuildVars().SetValue("BUILDREPORTING", "TRUE", "Platform Hardcoded")
        shell_environment.GetBuildVars().SetValue("BUILDREPORT_TYPES", CommonPlatform.ReportTypes, "Platform Hardcoded")
        shell_environment.GetBuildVars().SetValue("BUILDREPORT_FILE", CommonPlatform.GetReportFile(self), "Platform Hardcoded", False)
        if (os.path.isdir(os.path.dirname(CommonPlatform.CompilerPrefix))):
            os.environ["GCC5_AARCH64_PREFIX"] = CommonPlatform.CompilerPrefix
        return 0

    def PlatformPreBuild(self):
        return 0

    def PlatformPostBuild(self):
        FormatUefiBinary (os.path.join (shell_environment.GetBuildVars().GetValue("BUILD_OUTPUT_BASE"), CommonPlatform.FirmwareVolume), CommonPlatform.GetOutputBinary(self))

        if (os.path.isfile(CommonPlatform.VariableJson)):
            PcdDataBase = os.path.join(shell_environment.GetBuildVars().GetValue("BUILD_OUTPUT_BASE"),CommonPlatform.PcdDataBase)
            if (not os.path.isfile(PcdDataBase)):
                PcdDataBase = None
            GenVariableStore (CommonPlatform.VariableJson, CommonPlatform.GetVariableBinary(self), PcdDataBase)

        shutil.copyfile (os.path.join(shell_environment.GetBuildVars().GetValue("BUILD_OUTPUT_BASE"),CommonPlatform.GrubLaunch), CommonPlatform.GetOutputLauncher(self))
        return 0

    def FlashRomImage(self):
        return 0



