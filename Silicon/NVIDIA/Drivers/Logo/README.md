# NVIDIA UEFI Logo Driver

This directory contains the NVIDIA UEFI Logo DXE driver and associated resources for displaying platform logos during boot. The driver supports both scaling and non-scaling display modes and is designed for easy logo updates and customization.

## How the Logo Driver Works

### Overview
- The driver implements the `Edkii Platform Logo Protocol` to provide a platform logo to the UEFI firmware.
- It loads one or more logo images (BMP format) defined in an IDF (Image Definition File).
- The driver selects the most appropriate logo image based on the current screen resolution and configuration.
- The logo can be displayed at its native size (non-scaling mode) or scaled to fit a portion of the screen (scaling mode).

### Scaling vs. Non-Scaling Modes

#### Non-Scaling Mode
- The logo is displayed at its original (native) resolution.
- The driver selects the largest logo image that fits within the screen dimensions.
- The logo is centered horizontally and positioned vertically according to the `PcdLogoCenterY` platform configuration.
- No image scaling is performed; the image is shown pixel-for-pixel.

#### Scaling Mode
- The logo is scaled to fit a target area of the screen, determined by the `PcdLogoScreenRatio` platform configuration (e.g., 382 = 38.2% of screen height).
- The driver selects the logo image closest in size to the target area, then scales it using nearest-neighbor scaling.
- The scaled logo is centered horizontally and positioned vertically according to `PcdLogoCenterY`.
- Scaling ensures the logo appears at a consistent size across different screen resolutions.

### Logo Selection Logic
- The driver iterates through all images defined in the IDF file.
- In non-scaling mode, it picks the largest image that fits the screen.
- In scaling mode, it picks the image closest in size to the target area, then scales as needed.
- The driver supports multiple logo images for different resolutions.

## Updating the Logo Files

### 1. Prepare New Logo Images
- Create new BMP files for each desired resolution (e.g., `nvidiagray480.bmp`, `nvidiagray720.bmp`, `nvidiagray1080.bmp`).
- Place the BMP files in the driver directory.

### 2. Create or Update the IDF File
- The IDF (Image Definition File) lists the logo images and assigns them symbolic names.
- Example `LogoMultipleGray.idf`:
  ```
  #image IMG_LOGO_0 nvidiagray480.bmp
  #image IMG_LOGO_1 nvidiagray720.bmp
  #image IMG_LOGO_2 nvidiagray1080.bmp
  ```
- Add or update lines for each new BMP file.

### 3. Update the Logo Image Array C File
- A C file (such as `LogoMultipleGray.c` or `LogoSingleBlack.c`) defines an array of logo image tokens that correspond to the images listed in the IDF file.
- For example, `LogoMultipleGray.c`:
  ```c
  EFI_IMAGE_ID  mLogoImageId[] = {
    IMAGE_TOKEN (IMG_LOGO_0),
    IMAGE_TOKEN (IMG_LOGO_1),
    IMAGE_TOKEN (IMG_LOGO_2)
  };
  UINTN  mLogoImageIdCount = sizeof (mLogoImageId) / sizeof (mLogoImageId[0]);
  ```
- For a single logo, `LogoSingleBlack.c`:
  ```c
  EFI_IMAGE_ID  mLogoImageId[] = {
    IMAGE_TOKEN (IMG_LOGO)
  };
  UINTN  mLogoImageIdCount = sizeof (mLogoImageId) / sizeof (mLogoImageId[0]);
  ```
- **To customize:**
  - Update the array to match the symbolic names in your IDF file.
  - Add or remove entries as you add or remove images.
  - Ensure the order matches your intended logo selection logic.

### 4. Update the INF File
- The INF file must list all BMP, IDF, and C files in the `[Sources]` section:
  ```ini
  [Sources]
    Logo.c
    LogoMultipleGray.c
    LogoMultipleGray.idf
    nvidiagray480.bmp
    nvidiagray720.bmp
    nvidiagray1080.bmp
    # ... other sources
  ```
- Ensure the `[UserExtensions.TianoCore."ExtraFiles"]` section includes the IDF file if required by your build system.

### 5. Rebuild the Driver
- Run the EDK2 build system to rebuild the driver.
- The new logo images will be embedded in the driver and available for display.

## Adding a New Logo Set
1. Add new BMP files to the directory.
2. Create a new IDF file listing the images.
3. Create a new C file defining the image token array for the new set.
4. Create a new INF file referencing the new IDF, C, and BMP files.
5. Update the build system to include the new INF.

## Example Directory Structure
```
Logo/
├── Logo.c
├── LogoMultipleGray.c
├── LogoMultipleGray.idf
├── LogoMultipleGray.inf
├── nvidiagray480.bmp
├── nvidiagray720.bmp
├── nvidiagray1080.bmp
├── ...
```

## References
- [EDK2 HII Documentation](https://github.com/tianocore/edk2/tree/master/MdeModulePkg/Universal/HiiDatabaseDxe)
- [UEFI Platform Logo Protocol](https://github.com/tianocore/edk2/blob/master/MdeModulePkg/Include/Protocol/PlatformLogo.h)

## Support
For questions or contributions, please contact the NVIDIA UEFI team or open an issue in the project repository.
