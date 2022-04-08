# Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent



###############################################################################
# Sets environment variables for building NVIDIA UEFI images using the stuart
# build system.  Assumes the workspace has been created using submodules.

# This script, along with others in this directory, serves as a working example
# of building NVIDIA UEFI images.


# exit the script on error
set -e

# echo an informational message
_msg() {
  echo "nvidia: $1"
}

# echo an error message, then exit
_die() {
  echo "error: $2"
  exit $1
}

# verify we are at the root of the workspace
if [ -d edk2 -a -d edk2-nvidia ]; then
  _msg "building from workspace rooted at: `pwd`"
else
  _die 1 "must be launched from the workspace root."
fi

# verify the build module exists
if [ ! -f ${PLATFORM_BUILD} ]; then
  _die 1 "Build module (${PLATFORM_BUILD}) does not exist"
fi

# verify the given command exists in path.  if it does not, complain and exit
# the script.
verify_cmd_exists() {
  if which $1 > /dev/null; then
    _msg "found command: $1."
  else
    _die 2 "Missing command: $1.  Please install $1 and re-run this script."
  fi
}

# Verify we have prereqs
verify_cmd_exists python3
verify_cmd_exists virtualenv
verify_cmd_exists mono
verify_cmd_exists aarch64-linux-gnu-gcc

# Export the root of our workspace.  The stuart build system needs this.
export WORKSPACE=`pwd`

# Add NVIDIA Python modules to the python path
export PYTHONPATH=$WORKSPACE/edk2-nvidia/Silicon/NVIDIA:$PYTHONPATH
export PYTHONPATH=$WORKSPACE/edk2-nvidia/Silicon/NVIDIA/Tools:$PYTHONPATH

# Use the cross-compiler installed on the host
export CROSS_COMPILER_PREFIX=/usr/bin/aarch64-linux-gnu-
