#!/usr/bin/env bash

# Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

###############################################################################
# Build and run host-based tests in the current workspace using the stuart
# build system.  Calls prepare_stuart.sh and test_stuart.sh.

# This script, along with the helper scripts, serves as a working example of
# building NVIDIA UEFI images.

set -e

TEST_BUILD=edk2-nvidia/Platform/NVIDIA/HostBasedTests/TestBuild.py
HELPER_SCRIPT_DIR=edk2-nvidia/Silicon/NVIDIA/scripts

# Change directory to the root of the workspace
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd ${SCRIPT_DIR}/../../../..

# Prepare and build
${HELPER_SCRIPT_DIR}/prepare_stuart.sh ${TEST_BUILD}
${HELPER_SCRIPT_DIR}/test_stuart.sh HostBasedTests ${TEST_BUILD}
