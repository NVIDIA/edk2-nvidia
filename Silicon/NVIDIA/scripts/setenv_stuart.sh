# SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

# Some old versions of virtualenv will create venv/local/bin/activate instead
# of venv/bin/activate.
# Sets VENV_ACTIVATE.
function find_venv_activate() {
  VENV_ACTIVATE=venv/local/bin/activate
  if [ ! -e ${VENV_ACTIVATE} ]; then
    VENV_ACTIVATE=venv/bin/activate
  fi
}

# verify we are at the root of the workspace
if [ -d edk2 ]; then
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

# Verify we have at least Python 3.10
if python3 -c "import sys; sys.exit(1) if sys.version_info < (3, 10) else sys.exit(0)"; then
  _msg "found Python 3.10 or later."
else
  _die 3 "Python3 must 3.10 or later."
fi

# Export the root of our workspace.  The stuart build system needs this.
export WORKSPACE=`pwd`

# Add NVIDIA Python modules to the python path
export PYTHONPATH=${SCRIPT_DIR}/..:${PYTHONPATH}

# Use the cross-compiler installed on the host
export CROSS_COMPILER_PREFIX=/usr/bin/aarch64-linux-gnu-
