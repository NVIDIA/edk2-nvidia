# Introduction

These instructions explain how to get an edk2/edk2-platforms build running
on the NVIDIA Jetson platforms.

# Experimental Feature Notice

This software is experimental. It may be incompatible with standard JetPack
functionality for the Jetson AGX Xavier Developer Kit, including online software
update functionality for boot firmware. This software is provided for
experimental purposes, and can be reverted by following standard flashing
instructions for JetPack software in the Jetson AGX Xavier Developer Kit.

# Supported Platforms
- Jetson AGX Xavier

# UEFI features supported
- eMMC
- PCIe (this requires enabling PCIe support if used as boot media with ACPI)
- NVMe device
- SATA drive (onboard eSATA port is a PCIe device)
- USB mass storage
- USB NIC (AX88772b)
- EQoS NIC on board
- PXE
- HTTP boot support
- Switching between ACPI and device tree

# Limitations
- SetVariable is not supported at UEFI runtime
- SetTime is not supported at UEFI runtime

# Known Issues

There are no known issues associated with this feature.

# Flashing instructions
## Setup

Download the following software from https://developer.nvidia.com/embedded/downloads
        L4T Jetson Driver Package, version L4T_VERSION
        L4T Sample Root File System, version L4T_VERSION

Extract the driver package tarball and navigate to the `Linux_for_Tegra`
subdirectory.

    $ tar xjf Tegra186_Linux_RL4T_VERSION_aarch64.tbz2

Extract the nvidia-l4t-jetson-uefi-UEFI_PACKAGE_VERSION.tbz2 package provided over the top of the
extracted driver package and navigate to the Linux_for_Tegra subdirectory.

    $ tar xjf nvidia-l4t-jetson-uefi-UEFI_PACKAGE_VERSION.tbz2
    $ cd Linux_for_Tegra

Note that if you are not planning to use the L4T root filesystem you may skip this step.
Extract the root filesystem to the rootfs subdirectory and run the apply_binaries.sh script.
Please ensure that you have installed the `qemu-user-static` package before running the
`apply_binaries.sh` script.

    $ cd rootfs/
    $ sudo tar xjf /path/to/Tegra_Linux_Sample-Root-Filesystem_RL4T_VERSION_aarch64.tbz2
    $ cd ..
    $ sudo ./apply_binaries.sh


## Linux Serial Console

The Linux serial console is accessible via the micro-USB connector J501 on the
Jetson AGX Xavier platform. When you connect a USB cable to the micro-USB connector,
you should see four serial USB devices, and the Linux serial console is available
on the 3rd of the four. When you boot Linux on Jetson AGX Xavier, add the following
to the Linux kernel command line to direct the kernel output to the serial console.

    console=ttyS0,115200n8

You can enable Linux early console support for Jetson AGX Xavier by adding the
following string to the Linux kernel command line.

    earlycon=uart8250,mmio32,0x3110000

For booting Linux with ACPI on Jetson AGX Xavier the following Tegra 8250 driver
is required.

https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/tty/serial/8250/8250_tegra.c


## Booting UEFI-bootloadable Linux Distributions

If you are planning to boot a UEFI-bootable Linux distribution from external
media, such as a USB drive, then you may simply place the Jetson AGX Xavier
Developer Kit in Recovery Mode and execute the following command to flash the UEFI
firmware. Otherwise, please refer to the section “Booting L4T with mainline
Linux."

To specify ACPI boot by default:

    $ sudo ./flash.sh jetson-xavier-uefi-acpi internal

To specify DT boot by default:

    $ sudo ./flash.sh jetson-xavier-uefi internal

The OS hardware description can be changed without flashing the device as well, see “Booting Linux with
Device-Tree/ACPI”.


## Booting L4T with mainline Linux

Although it is possible to boot mainline Linux kernels without any out-of-tree
patches, NVIDIA provides the following Linux v5.4 kernel as an example for use
with Jetson AGX Xavier. All patches apart from the ACPI PCIe patches have been
accepted in the mainline Linux kernel.

Clone the Linux kernel tree:

    $ git clone -b tegra/uefi-5.7 https://github.com/jonhunter/linux.git

Configure the Linux kernel:

    $ export ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu
    $ export KBUILD_OUTPUT=/path/to/kernel/build
    $ make defconfig

Adjust the configuration:

    $ scripts/config --file $KBUILD_OUTPUT/.config --disable ARM64_4K_PAGES
    $ scripts/config --file $KBUILD_OUTPUT/.config --enable ARM64_64K_PAGES
    $ scripts/config --file $KBUILD_OUTPUT/.config --disable ARM64_VA_BITS_42
    $ scripts/config --file $KBUILD_OUTPUT/.config --disable ARM64_USER_VA_BITS_52
    $ scripts/config --file $KBUILD_OUTPUT/.config --enable ARM64_VA_BITS_48
    $ scripts/config --file $KBUILD_OUTPUT/.config --disable ARM64_PA_BITS_52
    $ scripts/config --file $KBUILD_OUTPUT/.config --enable ARM64_PA_BITS_48
    $ scripts/config --file $KBUILD_OUTPUT/.config --enable STMMAC_ETH
    $ scripts/config --file $KBUILD_OUTPUT/.config --enable STMMAC_PLATFORM
    $ scripts/config --file $KBUILD_OUTPUT/.config --enable DWMAC_DWC_QOS_ETH
    $ scripts/config --file $KBUILD_OUTPUT/.config --set-val CMA_SIZE_MBYTES 256

Build the kernel:

    $ make

Install the modules:

    $ sudo make INSTALL_MOD_PATH=/path/to/rootfs/ modules_install

Update the L4T serial console for booting with the EDK2 firmware:

    $ cd Linux_for_Tegra
    $ sudo sed -i 's/^CHIP=.*/CHIP="tegra194"/' "rootfs/etc/systemd/nv-oem-config.sh"
    $ sudo sed -i 's/ttyTCU0/ttyS0/' "rootfs/etc/systemd/nv-oem-config.sh"
    $ sudo sed -i 's/ttyTCU0/ttyS0/' "rootfs/etc/nv-oem-config.conf.t194"

Put the device into Recovery Mode and run the following command to flash.

    $ sudo ./flash.sh -K $KBUILD_OUTPUT/arch/arm64/boot/Image \
      -d $KBUILD_OUTPUT/arch/arm64/boot/dts/nvidia/tegra194-p2972-0000.dtb \
      jetson-xavier-uefi-acpi internal


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
1. Select "Linux" from GRUB menu.


## Enabling PCIe Support

PCIe support is disabled by default because when you boot Linux with ACPI, out
of tree patches are required. If these patches are not present, Linux may be
unable to boot. PCIe support can be enabled with the following steps:

1. Press escape when the console displays "Press ESCAPE for boot options"
1. Select "Device Manager" from the menu.
1. Select "NVIDIA Resource Configuration" from the menu.
1. Select "PCIe Configuration" from the menu.
1. Change "Enable PCIe in OS" to Enabled.
1. Press the Escape key twice to go back to the previous screen.
1. At the prompt, press "Y".
1. Press Escape once again to go to the UEFI menu.
1. Select "Reset"

The following out-of-tree patches are required for PCIe when booting Linux with
ACPI.

- http://patchwork.ozlabs.org/project/linux-pci/patch/20200110191500.9538-2-vidyas@nvidia.com
- http://patchwork.ozlabs.org/project/linux-pci/patch/20200110191500.9538-3-vidyas@nvidia.com

