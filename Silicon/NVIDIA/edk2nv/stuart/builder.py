# Copyright (c) Microsoft Corporation.
# Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

import datetime
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

        # Create an instance of our SettingsManager to use.
        # - stuart's invokeables framework finds the SettingsManager and uses
        # it, but the edk2_platform_build invokeable doesn't pass the
        # SettingsManager into UefiBuilder.  So, we have to create our own
        # instance.
        # - We definitely want the settings manager.  In order to support
        # stuart, the settings manager already has nearly everything we need.
        # We just need to add a few things to support our build extensions.
        self.settings = self.SettingsManager()

    def MoveConfDir(self):
        ''' Use a platform-specific Conf directory.

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
        '''
        ws_dir = Path(self.settings.GetWorkspaceRoot())
        confdir_path = ws_dir / self.settings.GetConfDirName()
        confdir_name = str(confdir_path)

        # Stuart will populate the conf directory with templates.  Since it
        # doesn't ask a SettingsManager for the name of the Conf directory, we
        # need to monkey-patch in an override.
        def hooked_populate_conf_dir(self, conf_folder_path, *args, **kwargs):
            _base_populate_conf_dir(self, confdir_name, *args, **kwargs)

        ConfMgmt.populate_conf_dir = hooked_populate_conf_dir

        # Add this Conf directory to UefiBuilder's search path.  When it's
        # looking for "Conf/target.txt", for example, it will look in each of
        # these directories.
        self.mws.PACKAGES_PATH.append(confdir_name)

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

    def RetrievePlatformCommandLineOptions(self, args):
        ''' Retrieve command line options from the argparser namespace '''
        self._jobs = args.JOBS

    def GetMaxJobs(self):
        ''' Return the value of the --jobs option.

        Defaults to `None`, telling stuart to use its default, which is
        num_cpus.
        '''
        return self._jobs

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
        # Move the Conf directory.  This isn't an "env" thing, but this is the
        # first callback, __init__() is too early, and the next callback is too
        # late.  Given the options available, this is the right place.
        self.MoveConfDir()

        logging.debug("Setting env from SettingsManager")

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

        # Set additional build variables
        cur_time = datetime.datetime.now()
        build_ts = cur_time.astimezone().replace(microsecond=0).isoformat()
        self.env.SetValue("BLD_*_BUILD_DATE_TIME", build_ts, reason_dynamic)
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
        from FormatUefiBinary import FormatUefiBinary

        ws_dir = Path(self.settings.GetWorkspaceRoot())
        build_dir = Path(self.env.GetValue("BUILD_OUTPUT_BASE"))
        target = self.settings.GetTarget()

        # Store the path to the build directory in a place an upstream build
        # system can find it.
        builddirfile = ws_dir / self.settings.GetBuildDirFile()
        builddirfile.parent.mkdir(parents=True, exist_ok=True)
        builddirfile.write_text(str(build_dir))

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

        # Copy the boot app, if appropriate for this platform
        boot_rel = self.settings.GetBootAppName()
        if boot_rel:
            boot_path = build_dir / boot_rel
            boot_out = ws_dir / self.settings.GetBootAppFile()
            logging.info("Copying boot app %s", boot_out)
            boot_out.parent.mkdir(parents=True, exist_ok=True)
            FormatUefiBinary(str(fw_vol), str(fw_img))
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
