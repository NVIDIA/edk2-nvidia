# Minimal Jetson binary

This build disables all hardware not needed to boot off of eMMC on Orin

## Details
Build is configured to launch built-in L4TLauncher binary in BDS
Security is provided by encrypted load targets for things loaded off emcc (kernel, initrd, etc)
UEFI Secure boot is enabled and expected to be setup via device tree methods
Persistant variables are not supported
