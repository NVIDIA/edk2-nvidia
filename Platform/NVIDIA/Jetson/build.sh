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
# Prepare and build a UEFI image for the Jetson platform in the current
# workspace using the stuart build system.  Calls prepare_stuart.sh and
# build_stuart.sh.

# This script, along with the helper scripts, serves as a working example of
# building NVIDIA UEFI images.

set -e

PLATFORM_BUILD=edk2-nvidia/Platform/NVIDIA/Jetson/PlatformBuild.py
HELPER_SCRIPT_DIR=edk2-nvidia/Silicon/NVIDIA/scripts

# Change directory to the root of the workspace
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd ${SCRIPT_DIR}/../../../..

# Prepare and build
${HELPER_SCRIPT_DIR}/prepare_stuart.sh ${PLATFORM_BUILD}
${HELPER_SCRIPT_DIR}/build_stuart.sh ${PLATFORM_BUILD}
