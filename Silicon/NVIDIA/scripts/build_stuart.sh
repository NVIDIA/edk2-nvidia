#!/usr/bin/env bash

# Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent



###############################################################################
# Build an NVIDIA UEFI image in the current workspace using the stuart build
# system.  Assumes the workspace has been created using submodules and prepared
# using prepare_stuart.sh

# This script, along with others in this directory, serves as a working example
# of building NVIDIA UEFI images.


PLATFORM_BUILD=$1
[ -z "${PLATFORM_BUILD}" ] && { echo "$0 <platform_build.py>"; exit 1; }


# Set common environment variables.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
. ${SCRIPT_DIR}/setenv_stuart.sh


_msg "Activating Python virtual environment."
. venv/bin/activate

STUART_BUILD_OPTIONS=${STUART_BUILD_OPTIONS:---verbose}

if [[ -z "${UEFI_RELEASE_ONLY}" ]]; then
  _msg "Building DEBUG ($PLATFORM_BUILD)."
  stuart_build -c ${PLATFORM_BUILD} ${STUART_BUILD_OPTIONS} --target DEBUG
fi

if [[ -z "${UEFI_DEBUG_ONLY}" ]]; then
  _msg "Building RELEASE ($PLATFORM_BUILD)."
  stuart_build -c ${PLATFORM_BUILD} ${STUART_BUILD_OPTIONS} --target RELEASE
fi
