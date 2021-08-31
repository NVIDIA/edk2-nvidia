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
# Build an NVIDIA UEFI image in the current workspace using the stuart build
# system.  Assumes the workspace has been created using submodules and prepared
# using prepare_stuart.sh

# This script, along with others in this directory, serves as a working example
# of building NVIDIA UEFI images.


PLATFORM_BUILD=$1
[ -z "$PLATFORM_BUILD" ] && { echo "$0 <platform_build.py>"; exit 1; }


# Set common environment variables.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
. ${SCRIPT_DIR}/setenv_stuart.sh


_msg "Activating Python virtual environment."
. venv/bin/activate


_msg "Building DEBUG ($PLATFORM_BUILD)."
stuart_build -c $PLATFORM_BUILD --verbose --target DEBUG

_msg "Building RELEASE ($PLATFORM_BUILD)."
stuart_build -c $PLATFORM_BUILD --verbose --target RELEASE
