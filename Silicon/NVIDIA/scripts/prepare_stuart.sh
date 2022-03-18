#!/usr/bin/env bash

# Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent


###############################################################################
# Prepares the current workspace to build NVIDIA UEFI images using the stuart
# build system.  Assumes the workspace has been created using submodules.

# This script, along with others in this directory, serves as a working example
# of building NVIDIA UEFI images.


PLATFORM_BUILD=$1
[ -z "${PLATFORM_BUILD}" ] && { echo "$0 <platform_build.py>"; exit 1; }


# Set common environment variables.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
. ${SCRIPT_DIR}/setenv_stuart.sh


# Create a python virtual env, if we haven't already.
if [ ! -e venv/bin/activate ]; then
  _msg "Creating Python virtual environment in `pwd`/venv..."
  virtualenv -p python3 venv
  . venv/bin/activate
  _msg "Installing required Python packages..."
  pip install --upgrade -r edk2/pip-requirements.txt
else
  _msg "Activating Python virtual environment."
  . venv/bin/activate
fi

if [ -d .git ]; then
  _msg "Updating submodules (${PLATFORM_BUILD})."
  stuart_setup ${STUART_SETUP_OPTIONS} -c ${PLATFORM_BUILD}
else
  _msg "Building from tarball"
fi

_msg "Updating build environment (${PLATFORM_BUILD})."
# Requires mono to be installed following the instructions here:
# - https://github.com/tianocore/edk2-pytool-extensions/blob/master/docs/usability/using_extdep.md
stuart_update ${STUART_UPDATE_OPTIONS} -c ${PLATFORM_BUILD}

_msg "Building basetools."
python edk2/BaseTools/Edk2ToolsBuild.py -t GCC5
