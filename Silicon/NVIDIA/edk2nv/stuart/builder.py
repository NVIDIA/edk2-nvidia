# Copyright (c) Microsoft Corporation.
# SPDX-FileCopyrightText: Copyright (c) 2021-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

import os
import sys
import datetime
import time
import logging
import shutil
from pathlib import Path
from edk2toolext.environment.uefi_build import UefiBuilder
from edk2toolext.environment import shell_environment
from edk2toolext.environment.conf_mgmt import ConfMgmt


__all__ = [
    "NVIDIAPlatformBuilder",
]


_base_populate_conf_dir = ConfMgmt.populate_conf_dir
''' Stuart's implementation of ConfMgmt.populate_conf_dir().  We're going to
    wrap this.
'''


reason_required = "Required by NVIDIA platforms"
reason_setman = "Set in platform SettingsManager"
reason_dynamic = "Dynamically populated"


class NVIDIAPlatformBuilder(UefiBuilder):
    ''' Base class for NVIDIA PlatformBuilders. '''

    def __init__(self):
        super().__init__()
        self.config_out = None
        self.kconf_syms = {}

        # Create an instance of our SettingsManager to use.
        # - stuart's invokeables framework finds the SettingsManager and uses
        # it, but the edk2_platform_build invokeable doesn't pass the
        # SettingsManager into UefiBuilder.  So, we have to create our own
        # instance.
        # - We definitely want the settings manager.  In order to support
        # stuart, the settings manager already has nearly everything we need.
        # We just need to add a few things to support our build extensions.
        self.settings = self.SettingsManager()

    def AddToolsDefInclude(self, confdir_path):
        ''' Include NVIDIA's tools_def.inc in tools_def.txt.

            Silicon/NVIDIA/tools_def.inc will contain NVIDIA overrides to
            tools_def.txt.  This function will modify the base tools_def.txt,
            which was copied from edk2's templates, to include this file.
        '''
        # Check to see if we've already added the include
        with open(confdir_path / "tools_def.txt", "r") as tools_def:
            datafile = tools_def.readlines()
            for line in datafile:
                if "!include Silicon/NVIDIA/tools_def.inc" in line:
                    # Found it.  We're done, so we can return.
                    return

        # Need to add it.
        with open(confdir_path / "tools_def.txt", "a") as tools_def:
            tools_def.write("\r\n# NVIDIA overrides\r\n!include Silicon/NVIDIA/tools_def.inc\r\n")


    def HookConfDirCreation(self):
        ''' Hook the Conf directory creation step.

            This allows NVIDIA-specific changes to how the Conf directory is
            created.
            - Use a platform-specific Conf directory.
            - Allow minor changes to Conf files while still using the upstream
              templates.

            Convince stuart to use `settings.GetConfDirName()` as the Conf
            directory.  If the Conf directory does not exist, it will be
            populated with the template.

            Stuart is hard-coded to always use <workspace_root>/Conf as its
            Conf directory.  However, we want to have platform-specific Conf
            directories.  In the near term, that avoids race conditions when
            building multiple platforms in parallel.  In the long term, should
            we choose to customize the contents of the Conf directory for a
            given platform, we'll have a way to do that.

            build.py allows the Conf directory to be moved via command line or
            via env.  Since stuart wraps our call to build.py, we'll rely on
            the env.

            Sometimes, we want to make minor changes to the Conf files, e.g.
            tools_def.txt.  We'll do that in this hook.  Alternatively, we
            could fork the templates, but then we'd have to maintain the forks
            and continually reintegrate upstream changes to the templates.
        '''
        ws_dir = Path(self.settings.GetWorkspaceRoot())
        confdir_path = ws_dir / self.settings.GetConfDirName()
        confdir_name = str(confdir_path)
        platform_builder = self

        # Stuart will populate the conf directory with templates.  Since it
        # doesn't ask a SettingsManager for the name of the Conf directory, we
        # need to monkey-patch in an override.
        def hooked_populate_conf_dir(self, conf_folder_path, *args, **kwargs):
            _base_populate_conf_dir(self, confdir_name, *args, **kwargs)
            platform_builder.AddToolsDefInclude(confdir_path)

        ConfMgmt.populate_conf_dir = hooked_populate_conf_dir

        # Stuart doesn't look for "<search_path>/target.txt", but looks for
        # "<search_path>/Conf/target.txt" instead.  Rather than add another
        # "Conf" to our path, we'll no-op it with a symlink.
        confconf_path = confdir_path / "Conf"
        if not confconf_path.exists():
            confdir_path.mkdir(parents=True, exist_ok=True)
            confconf_path.symlink_to(".")

    def ParseTargetFile(self):
        ''' Let the user know about expected "error" messages. '''
        # See comments in SetPlatformEnv() for an explanation.
        logging.debug("The following 'Can't set value' messages are expected")
        return super().ParseTargetFile()

    def ConvertToDos(self, in_file, out_file):
        ''' Save in_file as out_file, but with DOS-style line endings. '''
        with open(in_file, "r") as fin:
            with open(out_file, "w", newline="\r\n") as fout:
                for line in fin:
                    fout.write(line)

    #######################################
    # UefiBuilder hooks

    def AddPlatformCommandLineOptions(self, parserObj):
        ''' Add build-specific command-line options.

            The stuart_build command lacks a --target option, but the other
            commands, e.g. stuart_setup, stuart_update, inherit one from
            Edk2MultiPkgAwareInvocable.  This is the opposite of what we want
            for NVIDIA builds.  Other vendors use the multi-pkg aware features
            to control which scopes are used during setup and update.  That
            seems appropriate for architecture, but not for target.  We see
            target as a build-time option, not a setup-time option.

            To work-around stuart, we'll add the --target option here, where
            only stuart_build will hit it, but retrieve and handle it in
            settings.
        '''
        super().AddPlatformCommandLineOptions(parserObj)

        # Add --target for builds
        parserObj.add_argument('--target',
                               dest="nvidia_target", default="DEBUG",
                               help="Platform target to build")

        # build.py uses "-n" and make uses "-j" or "--jobs".  Split the
        # difference and accept a little of both.
        parserObj.add_argument('-n', '--jobs',
                               dest="JOBS", type=int,
                               help="Number of concurrent build jobs to run")

        parserObj.add_argument("--menuconfig", dest="MENUCONFIG",
                               action='store_true', default=False, help="Show configuration menu before build.")
        parserObj.add_argument("--skip-config", dest="SKIPCONFIG",
                               action='store_true', default=False, help="Skip configuration steps.")
        parserObj.add_argument("--skipallbuild", dest="SKIPALLBUILD",
                               action='store_true', default=False, help="Skip all build steps.")

    def RetrievePlatformCommandLineOptions(self, args):
        super().RetrievePlatformCommandLineOptions(args)
        ''' Retrieve command line options from the argparser namespace '''
        self._jobs = args.JOBS
        self._menuconfig = args.MENUCONFIG
        self._skipconfig = args.SKIPCONFIG
        if args.SKIPALLBUILD:
            self.SkipPostBuild = True
            self.SkipBuild = True
            self.SkipPreBuild = True

    def GetMaxJobs(self):
        ''' Return the value of the --jobs option.

        Defaults to `None`, telling stuart to use its default, which is
        num_cpus.
        '''
        return self._jobs

    def LoadKConfig(self):
        ''' Create a Kconfig object, load our config, and return it.

        '''
        from kconfiglib import Kconfig

        ws_dir = Path(self.settings.GetWorkspaceRoot())

        kconf_file = self.settings.GetKConfigFile()
        if (kconf_file == None):
            return 0
        kconf_path = ws_dir / kconf_file

        kconf = Kconfig(kconf_path, warn_to_stderr=False,
                    suppress_traceback=True)
        kconf.warn_assign_undef = True
        kconf.warn_assign_override = False
        kconf.warn_assign_redun = False

        configs = []

        # Start with the global defconfig
        global_defconfig = Path(
            self.settings.GetEdk2NvidiaDir()) / "Platform" / "NVIDIA" / "NVIDIA.defconfig"
        configs.append(str(global_defconfig))

        # Add the platform's configs
        configs += self.settings.GetConfigFiles()

        # Load configs, allowing each to override previous configuration
        print(kconf.load_config(ws_dir / configs[0]))
        for config in configs[1:]:
            # replace=False creates a merged configuration
            print(kconf.load_config(ws_dir / config, replace=False))

        if self.config_out.is_file ():
            print(kconf.load_config(self.config_out, replace=False))

        kconf.write_config(os.devnull)

        # Keep the config values
        self.kconf_syms.update(kconf.syms)

        return kconf

    def BuildConfigFile(self):
        ''' Builds the kconfig .config file for platform if needed.

        '''
        ws_dir = Path(self.settings.GetWorkspaceRoot())
        config_fullpath = ws_dir / self.settings.GetNvidiaConfigDir()

        self.config_out = config_fullpath / ".config"
        self.defconfig_out = config_fullpath / "defconfig"
        config_out_dsc = config_fullpath / "config.dsc.inc"

        kconf = self.LoadKConfig()

        if kconf.warnings:
            # Put a blank line between warnings to make them easier to read
            for warning in kconf.warnings:
                print("\n" + warning, file=sys.stderr)

            # Turn all warnings into errors, so that e.g. assignments to undefined
            # Kconfig symbols become errors.
            #
            # A warning is generated by this script whenever a symbol gets a
            # different value than the one it was assigned. Keep that one as just a
            # warning for now.
            raise ValueError("Aborting due to Kconfig warnings")

        # Write the merged configuration
        tmp_config = self.config_out.with_suffix(".tmp")
        kconf.write_config(tmp_config)
        self.ConvertToDos(tmp_config, self.config_out)
        tmp_config.unlink()
        print(f"Configuration saved to {self.config_out}")

        # If menuconfig was requested, run it now.  It will update .config in
        # place.
        if self._menuconfig:
            from menuconfig import menuconfig
            os.environ["KCONFIG_CONFIG"] = str(self.config_out)
            menuconfig(kconf)

            # Reload the config
            kconf = self.LoadKConfig()

        # Generate a minimal config.
        tmp_config = self.defconfig_out.with_suffix(".tmp")
        kconf.write_min_config(tmp_config,
                                     header=self.settings.GetDefconfigHeader())
        self.ConvertToDos(tmp_config, self.defconfig_out)
        tmp_config.unlink()
        print(f"Minimal configuration saved to {self.defconfig_out}")

        # Create version of config that edk2 can consume
        with open(self.config_out, "r") as f, open(config_out_dsc, "w") as fo:
            for line in f:
                # strip the file of "
                line = line.replace('"', "").replace("'", "")

                # Skip empty variables. edk2 cannot define something as empty,
                # so leave it as undefined.
                if line.startswith("CONFIG_") and line.strip().endswith("="):
                    line = "# Undefining empty: " + line

                # Keep the new line
                fo.write(line)

        return 0

    def SetPlatformEnv(self):
        ''' Setup the environment for this platform.

            Called by UefiBuilder.SetEnv() prior to the build and after some
            basic defaults have been added.  Values from target.txt and the
            DEFINE section in the platform's DSC/FDF file are added after this.

            Some values are used directly by stuart while most are passed
            through the shell environment or the build environment.

            Shell environment values can be set as follows:

                 shell_env = shell_environment.GetEnvironment()
                 shell_env.set_shell_var("KEY", "value")

            Build environment values can be set two ways.  If set as follows,
            they cannot be overridden:

                 self.env.SetValue("KEY", "value", "reason")

            If the build environment value is set as follows, it can be
            overridden by a subclass, target.txt, etc.

                 shell_env = shell_environment.GetEnvironment()
                 shell_env.set_build_var("KEY", "value")

            Build environment variables eventually find their way into make.
        '''
        # Hook the Conf directory creation step.
        # - This allows us to augment the behavior with our own special sauce.
        # - This isn't an "env" thing, but this is the first callback,
        # __init__() is too early, and the next callback is too late.  Given
        # the options available, this is the right place.
        self.HookConfDirCreation()

        logging.debug("Setting env from SettingsManager")

        # Generate .config file.
        # - Do this early so we can use kconfig to set build values.
        if not self._skipconfig:
            defconf = self.settings.GetConfigFiles()
            if defconf:
                self.BuildConfigFile ()

        # Preempt the contents of target.txt.
        #
        # If we don't provide a target.txt for a platform, which is normally
        # the case, stuart will copy in a template filled with defaults.  Then
        # stuart will load those defaults and start using them, which is most
        # likely not what we want.
        #
        # Ideally, we'd load target.txt first, then override where the subclass
        # had an opinion.  Unfortunately, "after target.txt" is too late;
        # stuart immediately uses some values without giving us a chance to
        # override them first.  Instead, we'll have to set values here and not
        # allow target.txt to override them.  This works fine, but results in
        # annoying "Can't set value" messages.  We'll just have to ignore
        # those.

        ws_dir = Path(self.settings.GetWorkspaceRoot())

        # ACTIVE_PLATFORM
        # - If not provided by the SettingsManager, the value in target.txt
        # will be taken.
        dsc_name = self.settings.GetDscName()
        if dsc_name:
            self.env.SetValue("ACTIVE_PLATFORM", dsc_name, reason_setman)

        # TARGET - always take the --target argument via GetTarget().  We
        # can't defer to target.txt here because we use GetTarget() to name
        # the directory target.txt lives in (via GetConfDirName()).
        self.env.SetValue("TARGET", self.settings.GetTarget(), reason_required)

        # MAX_CONCURRENT_THREAD_NUMBER - always take the --jobs argument, if
        # one was provided.
        max_jobs = self.GetMaxJobs()
        if max_jobs:
            self.env.SetValue("MAX_CONCURRENT_THREAD_NUMBER", max_jobs,
                              reason_required)

        # TARGET_ARCH - always AARCH64 on NVIDIA platforms.
        self.env.SetValue("TARGET_ARCH", "AARCH64", reason_required)

        # TOOL_CHAIN_TAG
        # - If not provided by the SettingsManager, the value in target.txt
        # will be taken.
        toolchain_tag = self.settings.GetToolchainTag()
        if toolchain_tag:
            self.env.SetValue("TOOL_CHAIN_TAG", toolchain_tag, reason_setman)

        # Set the build name
        self.env.SetValue("BLD_*_BUILD_NAME",
                    self.settings.GetName(), reason_dynamic)

        # Set the build guid.  Use the kconfig value, if we have one, but allow
        # GetGuid() to override.
        build_guid = self.settings.GetGuid()
        if not build_guid and "PLATFORM_GUID" in self.kconf_syms:
            build_guid = self.kconf_syms["PLATFORM_GUID"].str_value
        if not build_guid:
            if self.settings.GetConfigFiles():
                raise ValueError("PLATFORM_GUID must be provided.")
            else:
                raise NotImplementedError(
                    "GetGuid() must be implemented in NVIDIASettingsManager "
                    "subclasses."
                )
        self.env.SetValue("BLD_*_BUILD_GUID", build_guid, reason_setman)

        # Set additional build variables
        cur_time = datetime.datetime.now()
        build_ts = cur_time.astimezone().replace(microsecond=0).isoformat()
        self.env.SetValue("BLD_*_BUILD_DATE_TIME", build_ts, reason_dynamic)
        epoch_time = int(time.time())
        self.env.SetValue("BLD_*_BUILD_EPOCH", epoch_time, reason_dynamic)
        self.env.SetValue("BLD_*_BUILD_PROJECT_TYPE", "EDK2", reason_required)
        self.env.SetValue("BLD_*_BUILDID_STRING",
                          self.settings.GetFirmwareVersion(), reason_dynamic)

        # Setup build reporting
        self.env.SetValue("BUILDREPORTING", "TRUE", reason_required)
        self.env.SetValue("BUILDREPORT_TYPES",
                          self.settings.GetReportTypes(), reason_required)
        self.env.SetValue("BUILDREPORT_FILE",
                          str(ws_dir / self.settings.GetReportFile()),
                          reason_setman)

        # Set shell env
        shell_environment.GetEnvironment().set_shell_var(
            f"{toolchain_tag}_AARCH64_PREFIX",
            self.settings.GetCrossCompilerPrefix())
        shell_environment.GetEnvironment().set_shell_var(
            f"DTCPP_PREFIX",
            self.settings.GetCrossCompilerPrefix())
        # - Needed by build.py.
        confdir_path = ws_dir / self.settings.GetConfDirName()
        shell_environment.GetEnvironment().set_shell_var(
            "CONF_PATH", str(confdir_path))

        # Must return 0 to indicate success.
        return 0

    def PlatformPreBuild(self):
        return 0

    def PlatformPostBuild(self):
        ''' Additional build steps for NVIDIA platforms. '''
        from edk2nv.FormatUefiBinary import FormatUefiBinary

        ws_dir = Path(self.settings.GetWorkspaceRoot())
        build_dir = Path(self.env.GetValue("BUILD_OUTPUT_BASE"))
        target = self.settings.GetTarget()

        # Store the path to the build directory in a place an upstream build
        # system can find it.
        builddirfile = ws_dir / self.settings.GetBuildDirFile()
        builddirfile.parent.mkdir(parents=True, exist_ok=True)
        builddirfile.write_text(str(build_dir))

        # Store the firmware version string.  This will match the string printed
        # in the image banner.
        buildid = self.settings.GetFirmwareVersion()
        buildidfile = ws_dir / self.settings.GetBuildIdFile()
        buildidfile.parent.mkdir(parents=True, exist_ok=True)
        buildidfile.write_text(str(buildid))

        # Remove the Conf link we added earlier.  It can cause problems for
        # tools, such as find, that want to spider the build directory.  Since
        # we're done building, we don't need it any more.
        confdir_path = ws_dir / self.settings.GetConfDirName()
        confconf_path = confdir_path / "Conf"
        if confconf_path.is_symlink():
            confconf_path.unlink()

        # Generate the firmware image, if appropriate for this platform
        fw_rel = self.settings.GetFirmwareVolume()
        if fw_rel:
            fw_vol = build_dir / fw_rel
            fw_img = ws_dir / self.settings.GetFirmwareImageFile()
            logging.info("Generating uefi image %s", fw_img)
            fw_img.parent.mkdir(parents=True, exist_ok=True)
            FormatUefiBinary(str(fw_vol), str(fw_img))

            # If we also have a config file, save it relative to the image
            # file.  This records the config used to build the firmware image.
            if self.config_out:
                fw_conf = fw_img.with_name("config_" + fw_img.name).with_suffix("")
                shutil.copyfile(self.config_out, fw_conf)

        # Copy the boot app, if appropriate for this platform
        boot_rel = self.settings.GetBootAppName()
        if boot_rel:
            boot_path = build_dir / boot_rel
            boot_out = ws_dir / self.settings.GetBootAppFile()
            logging.info("Copying boot app %s", boot_out)
            boot_out.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(boot_path, boot_out)

        # Copy DTBs, if appropriate for this platform
        dtb_path = self.settings.GetDtbPath()
        if dtb_path:
            full_dtb_path = build_dir / dtb_path

            # Copy each generated DTB
            for src_dtb in full_dtb_path.glob("*.dtb"):
                dest_dtb = self.settings.GetDtbFile(src_dtb.stem)
                logging.info("Copying DTB %s", dest_dtb)
                shutil.copyfile(src_dtb, dest_dtb)

        return 0

    def PlatformFlashImage(self):
        logging.critical("Flash Image not supported")
        return 1
