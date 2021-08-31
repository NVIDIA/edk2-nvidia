# Jetson - Platform Build

This Build.md describes how to use the Pytools based build infrastructure
locally for Jetson.

## Supported Configuration Details

This solution for building and running Jetson has only been validated with
Ubuntu 18.04 with GCC5 toolchain.

## EDK2 Developer environment

The following tools are required:
  - ARM cross compiler
  - Host build tools
  - Python 3.6 or later, including virtualenv
  - Git
  - Mono
  - Misc tools and libraries

Steps to install requirements on Ubuntu 18.04:
1. Install all required packages:

  ``` bash
  sudo apt-get update
  sudo apt-get install build-essential uuid-dev git gcc python3 \
                       virtualenv gcc-aarch64-linux-gnu
  ```

2. Install mono following the [edk2-pytool note on NuGet on Linux](https://github.com/tianocore/edk2-pytool-extensions/blob/master/docs/usability/using_extdep.md#a-note-on-nuget-on-linux).

Note: edksetup, submodule initialization and manual installation of NASM, iASL,
etc. are **not** required.  This is handled by the stuart build system.

## Building Jetson with stuart

1. Create an EDK2 workspace by cloning the NVIDIA EDK2 repository.

    ``` bash
    git clone http://tbd/tbd.git nvidia_edk2_workspace
    ```

2. Initialize git submodule and update required submodules.  Later in the
   build, stuart may update additional submodules, but these must be updated
   now so that stuart can be launched.

    ``` bash
    git submodule init
    git submodule update edk2
    git submodule update edk2-nvidia
    ```

3. Build the Jetson platform:

    ``` bash
    edk2-nvidia/Platform/NVIDIA/Jetson/build.sh
    ```

    This script automates the build process.  Please review this script and the
    scripts in `edk2-nvidia/Silicon/NVIDIA/scripts` to understand the build
    process in detail.

    At a high-level, the build has the following steps:
    
    1. Create a Python virtual environment and install stuart into it.
    2. Run `stuart_setup` to update git submodules.
    3. Run `stuart_update` to download required dependencies and install them locally.
    4. Run `stuart_build` to build Jetson DEBUG and RELEASE images.

4. Images and build products will be in `images/`.  Logs will be in `Build/`.
