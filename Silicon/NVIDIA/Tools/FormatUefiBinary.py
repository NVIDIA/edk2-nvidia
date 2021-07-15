#!/usr/bin/python

# Copyright (c) 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.
#
# SPDX-FileCopyrightText: Copyright (c) <year> NVIDIA CORPORATION & AFFILIATES
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary

import argparse
import io
import os
import sys

DEFAULT_ALIGNMENT = 0x10000

def check_file_exists(filename):
    """
    Checks that the given filename exists.

    If the file does not exist, prints an error and exits.

    Otherwise returns silently
    """
    if filename and not os.path.isfile(filename):
        print("Error: could not find given file:\n"
              "    {}".format(filename))
        sys.exit(1)

def parse_command_line_args():
    """
    Parses the command line arguments for the program.

    There are two required positional arguments, the first being the
    input file name and the second being the output file name.
    """

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "input_file",
        metavar="INPUT_FILE",
        help="Input UEFI FV file name."
    )
    parser.add_argument(
        "output_file",
        metavar="OUTPUT_FILE",
        help="Output UEFI binary file name."
    )
    parser.add_argument(
        "--alignment",
        type=int,
        default=DEFAULT_ALIGNMENT,
        help=("Required alignment of the output file given as a decimal value. "
              "Default value is {}.".format(DEFAULT_ALIGNMENT))
    )

    args = parser.parse_args()

    check_file_exists(args.input_file)

    return (
        args.input_file,
        args.output_file,
        args.alignment
    )

def FormatUefiBinary (input_filename, output_filename, alignment=DEFAULT_ALIGNMENT):
    with io.open(input_filename, 'rb') as input_file:
        output_bytes = input_file.read()

    unaligned_bytes = os.path.getsize (input_filename) % alignment
    if unaligned_bytes!= 0:
        output_bytes += bytearray(b'\xFF'*(alignment - unaligned_bytes))

    if (not os.path.isdir(os.path.dirname(output_filename))):
        os.mkdir(os.path.dirname(output_filename))

    with io.open(output_filename, 'wb') as output_file:
        output_file.write(output_bytes)


def main():
    (input_filename, output_filename, alignment) = parse_command_line_args()

    FormatUefiBinary (input_filename, output_filename, alignment)
    print("Successfully formatted uefi binary to {}".format(output_filename))

if __name__ == '__main__':
    main()
