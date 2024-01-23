#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

###############################################################################
# Prepare and build a UEFI image for the StandaloneMm platform in the current
# workspace using the stuart build system.  Calls prepare_stuart.sh and
# build_stuart.sh.

# This script, along with the helper scripts, serves as a working example of
# building NVIDIA UEFI images.

set -e

PLATFORM_BUILD=edk2-nvidia/Platform/NVIDIA/StandaloneMm/PlatformBuild.py
HELPER_SCRIPT_DIR=edk2-nvidia/Silicon/NVIDIA/scripts

# Change directory to the root of the workspace
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd ${SCRIPT_DIR}/../../../..

# Prepare and build
${HELPER_SCRIPT_DIR}/prepare_stuart.sh ${PLATFORM_BUILD}
${HELPER_SCRIPT_DIR}/build_stuart.sh ${PLATFORM_BUILD} $@
