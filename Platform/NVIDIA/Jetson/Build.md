# Jetson - Platform Build

This file describes how to build UEFI for the Jetson platform.

## Supported Configuration Details

The following steps have only been validated with Ubuntu 18.04 and 20.04 with
the GCC5 toolchain.

## EDK2 Developer environment

The following tools are required:
  - ARM cross compiler
  - Host build tools
  - Python 3.6 or later, including virtualenv
  - Git
  - Mono
  - Misc tools and libraries

Steps to install requirements:
1. Install all required packages:

    ``` bash
    sudo apt-get update
    sudo apt-get install build-essential uuid-dev git gcc python3 \
                         virtualenv gcc-aarch64-linux-gnu \
                         device-tree-compiler
    ```

2. Install mono following the [edk2-pytool note on NuGet on Linux](https://github.com/tianocore/edk2-pytool-extensions/blob/master/docs/usability/using_extdep.md#a-note-on-nuget-on-linux).

Note: edksetup, submodule initialization and manual installation of NASM, iASL,
etc. are **not** required.  This is handled by the stuart build system.

## Building Jetson with stuart

1. Build the Jetson platform:

    ``` bash
    edk2-nvidia/Platform/NVIDIA/Jetson/build.sh
    ```

    This script automates the build process.  Please review this script and the
    scripts in `edk2-nvidia/Silicon/NVIDIA/scripts` to understand the build
    process in detail.

    At a high-level, the build has the following steps:

    1. Create a Python virtual environment and install stuart into it.
    2. Run `stuart_update` to download required dependencies and install them locally.
    3. Run `stuart_build` to build Jetson DEBUG and RELEASE images.

2. Images and build products will be in `images/`.  Logs will be in `Build/`.
