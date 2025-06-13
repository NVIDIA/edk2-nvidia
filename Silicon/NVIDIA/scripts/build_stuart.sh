#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

###############################################################################
# Build an NVIDIA UEFI image in the current workspace using the stuart build
# system.  Assumes the workspace has been created using submodules and prepared
# using prepare_stuart.sh

# This script, along with others in this directory, serves as a working example
# of building NVIDIA UEFI images.

# Set common environment variables.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
. ${SCRIPT_DIR}/setenv_stuart.sh

_msg "Activating Python virtual environment."
find_venv_activate
. ${VENV_ACTIVATE}

STUART_BUILD_OPTIONS=${STUART_BUILD_OPTIONS:---verbose}

if [[ "${UEFI_SKIP_UPDATE}" ]]; then
  STUART_BUILD_OPTIONS+=" --skip-verify"
fi

stuart_build -c "$@" ${STUART_BUILD_OPTIONS}

