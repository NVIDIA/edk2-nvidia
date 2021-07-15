# Jetson - Platform Build

This Build.md describes how to use the Pytools based build infrastructure locally for Jetson.

## Supported Configuration Details

This solution for building and running Jetson has only been validated with Ubuntu 18.04 with GCC5 toolchain.

## EDK2 Developer environment

- [Python 3.8.x - Download & Install](https://www.python.org/downloads/)
- [GIT - Download & Install](https://git-scm.com/download/)
- [Edk2 Source](https://github.com/tianocore/edk2)
- For building Basetools and other host applications

  ``` bash
  sudo apt-get update
  sudo apt-get install gcc g++ make uuid-dev
  ```

Note: edksetup, Submodule initialization and manual installation of NASM, iASL, or
the required cross-compiler toolchains are **not** required, this is handled by the
Pytools build system.

## Building with Pytools for EmulatorPkg

1. [Optional] Create a Python Virtual Environment - generally once per workspace

    ``` bash
    python -m venv <name of virtual environment>
    ```

2. [Optional] Activate Virtual Environment - each time new shell opened
    - Linux

      ```bash
      source <name of virtual environment>/bin/activate
      ```

    - Windows

      ``` bash
      <name of virtual environment>/Scripts/activate.bat
      ```

2. Add edk2 as submodule

    ``` bash
    git submodule add https://github.com/tianocore/edk2 edk2
    ```

3. Install Pytools - generally once per virtual env or whenever pip-requirements.txt changes

    ``` bash
    pip install --upgrade -r edk2/pip-requirements.txt
    ```

4. Initialize & Update Submodules - only when submodules updated

    ``` bash
    stuart_setup -c Platform/NVIDIA/Jetson/PlatformBuild.py
    ```

5. Initialize & Update Dependencies - only as needed when ext_deps change

    ``` bash
    stuart_update -c Platform/NVIDIA/Jetson/PlatformBuild.py
    ```

6. Compile the basetools if necessary - only when basetools C source files change

    ``` bash
    python edk2/BaseTools/Edk2ToolsBuild.py -t GCC5
    ```

7. Compile Firmware

    ``` bash
    stuart_build -c Platform/NVIDIA/Jetson/PlatformBuild.py
    ```

    - use `stuart_build -c Platform/NVIDIA/Jetson/PlatformBuild.py -h` option to see additional
    options like `--clean`


### Notes

1. Configuring *ACTIVE_PLATFORM* and *TARGET_ARCH* in Conf/target.txt is **not** required. This
   environment is set by PlatformBuild.py based upon the `[-a <TARGET_ARCH>]` parameter.

**NOTE:** Logging the execution output will be in the normal stuart log as well as to your console.

### Custom Build Options

### Passing Build Defines

## References

- [Installing and using Pytools](https://github.com/tianocore/edk2-pytool-extensions/blob/master/docs/using.md#installing)
- More on [python virtual environments](https://docs.python.org/3/library/venv.html)
