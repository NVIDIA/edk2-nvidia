#
# This module builds a secure partition pkg file.
#
# Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#

import struct
import os
import logging

PAGE_SIZE = 4096
OFFSET_MAGIC = struct.pack("!I", 0x52415346)


def sptool(manifest_file, img_file, out_file):
    logging.info("Generating secure partition pkg: %s", out_file)
    logging.info("  from image: %s", img_file)
    logging.info("  from manifest: %s", manifest_file)

    # Header is defined as 6*U32, which is a Python structure of format 'IIIIII'
    header_structure = 'IIIIII'

    if not os.path.exists(img_file):
        logging.fatal("Cannot find image file: %s" % img_file)
        return 1
    if not os.path.exists(manifest_file):
        logging.fatal("Cannot find DTB file: %s" % manifest_file)
        return 1

    try:
        with open(manifest_file, mode='rb') as file:
            manifest_data = file.read()
    except Exception as e:
        logging.error("Could not read DTB file", exc_info=True)

    try:
        with open(img_file, mode='rb') as file:
            img_data = file.read()
    except Exception as e:
        logging.error("Could not read image file", exc_info=True)

    # Prepare the header, magic spells "SPKG", version 1.
    magic = 0x474B5053
    version = 1
    # The Manifest DTB goes after the header, offset is size of header (6*U32)
    dtb_offset = 6*4
    dtb_size = len(manifest_data)
    # The firmware images goes after the DTB and is PAGE_SIZE aligned
    fw_offset = int((dtb_size+dtb_offset) / PAGE_SIZE)*PAGE_SIZE + PAGE_SIZE
    fw_size = len(img_data)
    #Empty space between Manifest and image
    space = bytearray(fw_offset - dtb_size - dtb_offset)

    header = struct.pack(header_structure, magic, version, dtb_offset, dtb_size, fw_offset, fw_size)

    # Check if a magic is present in DTB and replace it with the actual fw_offset
    if OFFSET_MAGIC in manifest_data:
        manifest_data = manifest_data.replace(OFFSET_MAGIC, bytearray(struct.pack("!I", fw_offset)))
        logging.info("Patched Manifest with Image offset")

    try:
        with open(out_file, 'wb') as f:
            f.write(header)
            f.write(manifest_data)
            f.write(space)
            f.write(img_data)
    except Exception as e:
        logging.error("Could not write output file", exc_info=True)
        return 1

    logging.info("Wrote PKG into: %s Entrypoint-offset: 0x%x" % (out_file, fw_offset))
    return 0
