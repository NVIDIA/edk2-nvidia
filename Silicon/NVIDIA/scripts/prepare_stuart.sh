#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.


###############################################################################
# Prepares the current workspace to build NVIDIA UEFI images using the stuart
# build system.  Assumes the workspace has been created using submodules.

# This script, along with others in this directory, serves as a working example
# of building NVIDIA UEFI images.


PLATFORM_BUILD=$1
[ -z "$PLATFORM_BUILD" ] && { echo "$0 <platform_build.py>"; exit 1; }


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


_msg "Setting up build environment ($PLATFORM_BUILD)."
stuart_setup -c $PLATFORM_BUILD

_msg "Updating build environment ($PLATFORM_BUILD)."
# Requires mono to be installed following the instructions here:
# - https://github.com/tianocore/edk2-pytool-extensions/blob/master/docs/usability/using_extdep.md
stuart_update -c $PLATFORM_BUILD

_msg "Building basetools."
python edk2/BaseTools/Edk2ToolsBuild.py -t GCC5
