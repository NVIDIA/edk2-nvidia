NVIDIA Jetson UEFI/ACPI Experimental Firmware Version 1.1.1

# Introduction

These instructions explain how to install and boot Linux on
NVIDIA Jetson platforms using UEFI firmware.

This software is intended for use with the mainline Linux kernel.
It is not currently compatible with the L4T kernel.

# Experimental Feature Notice

This software is experimental. It may be incompatible with standard JetPack
functionality including online software update functionality for boot firmware.
This software is provided for experimental purposes, and can be reverted by
following standard flashing instructions for JetPack software provided with
the Jetson Developer Kit.

# Supported Platforms
- Jetson AGX Xavier
- Jetson Xavier NX

# UEFI features available for all supported platforms
- eMMC
- Ethernet (on-board EQoS controller)
- HTTP boot
- NVMe (disabled by default for ACPI, see 'Enabling PCIe Support for ACPI')
- PCIe (disabled by default for ACPI, see 'Enabling PCIe Support for ACPI')
- PXE boot
- SD-card (see 'Limitations')
- Switching between ACPI and device tree
- USB Mass Storage
- USB Ethernet (AX88772b)

# UEFI features supported only on Jetson AGX Xavier
- SATA

# Limitations
- SetVariable is not supported at UEFI runtime for Jetson AGX Xavier
- SetTime is not supported at UEFI runtime for Jetson AGX Xavier
- On Jetson Xavier NX, when booting in device tree mode the kernel by default
  will disable the clocks needed for runtime variable operation, this can be
  disabled by passing "clk_ignore_unused" as a command line option to the kernel.
- Only SDR25 is supported for SD-cards when booting Linux with ACPI
  Users may use alternative storage devices, such as USB mass storage,
  for better performance
- Graphics Output Protocol (GOP) is not supported and so only serial
  console support is available

# Known Issues
- None

# Version History
- 1.1.1:
    - Added support for configuring SPCR/DBG2 and added support for sub-type 5
      entries for Operating Systems that support that.
    - Corrected issue where SDHCI ACPI entries had same UID
    - Corrected issue where 12v PCIe supply would not be enabled
    - Updated ACPI DSDT/SSDT compliance revision to 2
    - Reserve USB firmware region to prevent issues where Operating Systems
      could corrupt data
    - Added AHCI support for ACPI boot when PCIe is not enabled in OS
    - Added Power Button support in ACPI
- 1.1.0:
    - Added support for SD-cards and Jetson Xavier NX
    - Fixed support for USB-C port J512 on Jetson AGX Xavier
- 1.0.0:
    - Initial release supporting Jetson AGX Xavier

# Flashing instructions
## Setup

Download the following software from the [Jetson Download Center](https://developer.nvidia.com/embedded/downloads)

        L4T Jetson Driver Package, version L4T_VERSION
        L4T Sample Root File System, version L4T_VERSION

Extract the driver package tarball.

    $ tar xjf Jetson_Linux_RL4T_VERSION_aarch64.tbz2

Extract the nvidia-l4t-jetson-uefi-UEFI_PACKAGE_VERSION.tbz2 package provided
over the top of the extracted driver package and navigate to the Linux_for_Tegra
subdirectory.

    $ tar xpf nvidia-l4t-jetson-uefi-UEFI_PACKAGE_VERSION.tbz2
    $ cd Linux_for_Tegra

Note that if you are not planning to use the L4T root filesystem you may skip
this step. Extract the root filesystem to the rootfs subdirectory and run the
apply_binaries.sh script. Please ensure that you have installed the
`qemu-user-static` package before running the `apply_binaries.sh` script.

    $ cd rootfs/
    $ sudo tar xjf /path/to/Tegra_Linux_Sample-Root-Filesystem_RL4T_VERSION_aarch64.tbz2
    $ cd ..
    $ sudo ./apply_binaries.sh


## Linux Serial Console

For the Jetson AGX Xavier platform, the Linux serial console is accessible via
the micro-USB connector J501. When you connect a USB cable to the micro-USB
connector, you should see four serial USB devices, and the Linux serial console
is available on the 3rd of the four.

For the Jetson Xavier NX platform, the Linux serial console is accessible via
pins 8 (UART TX) and 10 (UART RX) on the 40-pin expansion header J12.

When you boot Linux, add the following to the Linux kernel command line to
direct the kernel output to the serial console.

    console=ttyS0,115200n8

You can enable Linux early console support by adding the 'earlycon' parameter
to the Linux kernel command line. For Jetson AGX Xavier add:

    earlycon=uart8250,mmio32,0x3110000

For Jetson Xavier NX add:

    earlycon=uart8250,mmio32,0x3100000

For booting Linux with ACPI, the Tegra 8250 driver (located in the Linux kernel
source file drivers/tty/serial/8250/8250_tegra.c) is required for the serial
console.


## Booting UEFI-bootable Linux Distributions

If you are planning to boot a UEFI-bootable Linux distribution from external
media, such as a USB drive, place the Jetson Developer Kit in Recovery Mode
and execute the following command with the appropriate configuration to flash
the UEFI firmware. Otherwise, please refer to the section “Booting L4T with
mainline Linux."

    $ sudo ./flash.sh <config> external

Where `<config>` is:

- `jetson-xavier-uefi-min` for Jetson AGX Xavier with Device-Tree
- `jetson-xavier-uefi-acpi-min` for Jetson AGX Xavier with ACPI
- `jetson-xavier-nx-uefi` for Jetson Xavier NX with Device-Tree
- `jetson-xavier-nx-uefi-acpi` for Jetson Xavier NX with ACPI

The OS hardware description can be changed without flashing the device as well,
see “Booting Linux with Device-Tree/ACPI”.


## Booting L4T with mainline Linux

Clone the Linux kernel tree:

    $ git clone git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
    $ git checkout v5.12

Configure the Linux kernel:

    $ export ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu
    $ export KBUILD_OUTPUT=/path/to/kernel/build
    $ make defconfig

Adjust the configuration:

    $ scripts/config --file $KBUILD_OUTPUT/.config --enable STMMAC_ETH
    $ scripts/config --file $KBUILD_OUTPUT/.config --enable STMMAC_PLATFORM
    $ scripts/config --file $KBUILD_OUTPUT/.config --enable DWMAC_DWC_QOS_ETH
    $ scripts/config --file $KBUILD_OUTPUT/.config --enable MARVELL_PHY
    $ scripts/config --file $KBUILD_OUTPUT/.config --set-val CMA_SIZE_MBYTES 256

Build the kernel:

    $ make

Install the modules:

    $ sudo make INSTALL_MOD_PATH=/path/to/l4t-rootfs/ modules_install

Update the L4T serial console for booting with the EDK2 firmware:

    $ cd Linux_for_Tegra
    $ sudo sed -i 's/nv-oem-config-uart-port=ttyGS0/nv-oem-config-uart-port=ttyS0/' \
      "rootfs/etc/nv-oem-config.conf"

To boot Linux with Device-Tree, put the device into Recovery Mode and run the
following command to flash.

    $ sudo ./flash.sh -K $KBUILD_OUTPUT/arch/arm64/boot/Image \
      -d $KBUILD_OUTPUT/arch/arm64/boot/dts/nvidia/<dtb> <config> internal

Where `<config>` is:
- `jetson-xavier-uefi` for Jetson AGX Xavier
- `jetson-xavier-nx-uefi-emmc` for Jetson Xavier NX (eMMC)
- `jetson-xavier-nx-uefi-sd` for Jetson Xavier NX (SD-card)

Where `<dtb>` is:
- `tegra194-p2972-0000.dtb` for Jetson AGX Xavier
- `tegra194-p3509-0000+p3668-0001.dtb` for Jetson Xavier NX (eMMC)
- `tegra194-p3509-0000+p3668-0000.dtb` for Jetson Xavier NX (SD-card)

To boot Linux with ACPI, put the device into Recovery Mode and run the
following command to flash.

    $ sudo ./flash.sh -K $KBUILD_OUTPUT/arch/arm64/boot/Image <config> internal

Where `<config>` is:
- `jetson-xavier-uefi-acpi` for Jetson AGX Xavier
- `jetson-xavier-nx-uefi-acpi-emmc` for Jetson Xavier NX (eMMC)
- `jetson-xavier-nx-uefi-acpi-sd` for Jetson Xavier NX (SD-card)


## Booting Linux with Device-Tree/ACPI

To change between device tree and ACPI at boot the following steps can be used:

1. Press the Escape key when console displays "Press ESCAPE for boot options"
1. Select "Device Manager" from the menu.
1. Select "O/S Hardware Description Selection" from the menu.
1. To select Device Tree, scroll down and select "Device Tree".
1. To select ACPI, scroll down and select "ACPI".
1. Press the Escape key to go back to previous screen.
1. At the prompt, press "Y"
1. Press the Escape key once again to go to UEFI menu.
1. Select "Reset"


## Enabling PCIe Support for ACPI

PCIe support is disabled by default because prior to Linux v5.13, the PCIe
driver does not include the necessary changes to enable ACPI support. If these
change are not present, then Linux may be unable to boot. The patches required
for enabling PCIe with ACPI when booting pre-v5.13 Linux kernels, are found
below.

https://lore.kernel.org/linux-acpi/20210416134537.19474-1-vidyas@nvidia.com/
https://lore.kernel.org/linux-pci/20210610064134.336781-1-jonathanh@nvidia.com/

PCIe support for ACPI can be enabled with the following steps:

1. Press escape when the console displays "Press ESCAPE for boot options"
1. Select "Device Manager" from the menu.
1. Select "NVIDIA Resource Configuration" from the menu.
1. Select "PCIe Configuration" from the menu.
1. Change "Enable PCIe in OS" to Enabled.
1. Press the Escape key twice to go back to the previous screen.
1. At the prompt, press "Y".
1. Press Escape once again to go to the UEFI menu.
1. Select "Reset"
