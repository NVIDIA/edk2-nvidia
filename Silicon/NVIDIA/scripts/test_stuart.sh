#!/usr/bin/env bash

# Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent



###############################################################################
# Run an NVIDIA host-based tests in the current workspace using the stuart
# build system.  Assumes the workspace has been created using submodules and
# prepared using prepare_stuart.sh

# This script, along with others in this directory, serves as a working example
# of building NVIDIA UEFI images.


PACKAGE=$1
PLATFORM_BUILD=$2
[ -z "$PLATFORM_BUILD" ] && { echo "$0 <package> <platform_build.py>"; exit 1; }


# Set common environment variables.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
. ${SCRIPT_DIR}/setenv_stuart.sh


_msg "Activating Python virtual environment."
. venv/bin/activate


_msg "Testing ($PLATFORM_BUILD)."
stuart_ci_build -t NOOPT -a X64 -p ${PACKAGE} -c ${PLATFORM_BUILD} --verbose
