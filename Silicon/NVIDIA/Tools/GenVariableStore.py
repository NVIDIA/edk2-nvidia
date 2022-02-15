#!/usr/bin/python3

# Copyright (c) 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
"""
Program for dynamically generating variable store files from a JSON description.

Input JSON format should be as follows:
    {
        "includes":[
            "Platform/NVIDIA/Jetson/JetsonVariablesDesc.json",
            ... more json files to include ...
        ],
        "definitions":[
            {"vendor_guid":vendorGuidName,
            "variables":[
                {
                    "name":variableName,
                    "attributes"=[],
                    "type":variableType
                    "data":variableData
                },
                ... more variable objects ...
            ]},
            ... more guid objects ...
        ]
    }

Includes are paths to JSON files whose variables should be included in the
generated output file.

For variable objects, the attributes EFI_VARIABLE_NON_VOLATILE and
EFI_VARIABLE_BOOTSERVICE_ACCESS will always be set.
The attributes field is optional and can be used to specify additional
attributes if necessary.

Acceptable data types and their expected data format is as follows:
- One of the strings "UINT8", "UIN16", "UINT32", "UINT64" (uppercase)
    - In this case, the data should be a string of hex digits,
    prefixed with "0x", with a max size of the corresponding type.
- The string "ARRAY:<type>" where <type> describes the element type
    - In this case the data should be a list of valid elements
    for the given type. Nested ARRAYs should all have the same length.
    ARRAY elements of type STRUCT must all have the same shape.
- The string "STRUCT"
    - In this case the data should be a dictionary
    of struct members, with keys specifying the type
    of the member, and values specifying the data
- The string "FILE"
    - In this case, the data should be a JSON string that is a file name
    containing the binary data for the variable.
"""
from __future__ import absolute_import, division, print_function, unicode_literals

import argparse
import io
import json
import os
import struct
import sys
from builtins import str
from collections import OrderedDict
from VariableStoreUtil import (
    INT_TYPES, SUPPORTED_TYPE_STARTS, MAX_UINT8, MAX_UINT16, MAX_UINT32,
    MAX_UINT64, GUIDS, ATTRIBUTES, DEFAULT_ATTRIBUTES, FVH_FV_LENGTH,
    FVH_SIGNATURE, FVH_ATTRIBUTES, FVH_HEADER_LENGTH, FVH_HEADER_OFFSET,
    FVH_RESERVED, FVH_REVISION, FVH_BLOCKMAP0_BLOCK_SIZE,
    FVH_BLOCKMAP1_NUM_BLOCKS, FVH_BLOCKMAP1_BLOCK_SIZE, VSH_FORMATTED,
    VSH_HEALTHY, VSH_RESERVED, VSH_RESERVED1, VH_START_ID, VH_STATE,
    VH_RESERVED, VH_MONOTONIC_COUNT, VH_PUBLIC_KEY_INDEX, UEFI_SUBDIRS
)

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

def make_key_unique(key, processed_pairs):
    """
    Turns the given key into a unique key for the given dictionary.

    Appends "_#" to the given key, where # is an integer that distinguishes
    the given key from any other key in the given dictionary.

    Returns the unique key.
    """
    count = 0
    unique_key = key + "_{}".format(count)
    while unique_key in processed_pairs:
        count += 1
        unique_key = key + "_{}".format(count)
    return unique_key

def parse_json_pairs(pairs):
    """
    Custom function for parsing JSON object pairs.

    Returns an OrderedDict of field name -> value, where field name is appended
    with a _# (# being a unique integer) to support duplicate types.

    Field name will only have _# appended if it is a type
    (i.e. it starts with one of SUPPORTED_TYPE_STARTS).

    Since we are representing structs using JSON objects, we have to be able to
    support duplicate JSON field names when structs have members of the same
    type. We also have to make sure the keys are in the order they were given,
    hence the use of the OrderedDict.
    """
    processed_pairs = OrderedDict()
    for key, val in pairs:
        key_is_type = any(key.startswith(type_start)
                          for type_start in SUPPORTED_TYPE_STARTS)
        if key_is_type:
            key = make_key_unique(key, processed_pairs)

        processed_pairs[key] = val

    return processed_pairs

def find_full_workspace_path(relative_filename):
    """
    Find the full path for the given relative filename if it exists.

    The relative filename will be joined with the uefi source directory and each
    of the uefi sub directories to see if it exists.

    The uefi source path is defined by the environment variable UEFI_SOURCE.
    The sub directories are defined by the Python variable UEFI_SUBDIRS.

    If no path exists or multiple paths exist, returns None.

    Otherwise returns the full unique existing path.
    """
    if 'UEFI_SOURCE' not in os.environ:
        print("Error: UEFI_SOURCE environment variable not set.")
        print("  The UEFI_SOURCE variable is used to search for include files.")
        sys.exit(1)

    full_paths = [
        os.path.join(os.environ['UEFI_SOURCE'], sub_directory, relative_filename)
        for sub_directory
        in UEFI_SUBDIRS
    ]
    existing_paths = [path for path in full_paths if os.path.isfile(path)]

    return existing_paths[0] if len(existing_paths) == 1 else None

def get_json_field(json_object, field_name, expected_type, default_value):
    """
    If the given JSON object has the given field, returns the field value.

    If the object does not have the field and a default value is not given,
    prints an error message and exits the program.
    If the object has the field but it is not of the expected type, prints
    an error message and exits the program.
    If expected_type is None then skips type checking of the value.

    If the object has the field of the expected_type, returns the field value.
    """

    field_value = default_value
    if field_name in json_object:
        field_value = json_object[field_name]
    elif not default_value:
        object_type = "variable"
        if field_name in ['includes', 'definitions']:
            object_type = "top-level"
        elif field_name in  ['vendor_guid', 'variables']:
            object_type = "vendor GUID"
        print("JSON format error: all {} objects need "
              "a '{}' field".format(object_type, field_name))
        sys.exit(1)

    if expected_type and not isinstance(field_value, expected_type):
        type_string = "string"
        if expected_type == list:
            type_string = "array"
        elif expected_type == OrderedDict:
            type_string = "object"

        print("JSON format error: '{}' field must be "
              "given as a JSON {}".format(field_name, type_string))
        sys.exit(1)

    return field_value

def validate_json_desc(variable_json_desc):
    """
    Validates the format of the given json descriptor.

    Format should look as described at the top of the file.
    If invalid format is detected, prints error message and exits the program.

    If format is valid, returns without error.
    """
    # TODO: match finalized design doc spec
    if not isinstance(variable_json_desc, dict):
        print("JSON format error: top-level JSON must be a JSON object "
              "with the fields 'includes' and 'definitions'")
        sys.exit(1)

    includes = get_json_field(variable_json_desc, 'includes', list, None)

    for include_filename in includes:
        if not isinstance(include_filename, str):
            print("Error: 'includes' file paths must be given as JSON strings")
            sys.exit(1)

        full_include_path = find_full_workspace_path(include_filename)
        if not full_include_path:
            print("Error: could not find given include file in "
                  "UEFI_SOURCE sub directories.")
            print("  Given include path: {}".format(include_filename))
            print("  UEFI_SOURCE path: {}".format(os.environ['UEFI_SOURCE']))
            print("  Sub directories searched: {}".format(UEFI_SUBDIRS))
            sys.exit(1)

    definitions = get_json_field(variable_json_desc, 'definitions', list, None)

    # Validate each  vendor GUID object
    for vendor_guid_desc in definitions:
        vendor_guid = get_json_field(vendor_guid_desc, 'vendor_guid', str, None)
        if vendor_guid not in GUIDS:
            print("Error: Unrecognizable Vendor GUID: {}".format(vendor_guid))
            sys.exit(1)

        print("Validating vendor GUID: {}".format(vendor_guid))

        variables = get_json_field(vendor_guid_desc, 'variables', list, None)

        # Validate all of the variable objects
        for variable_desc in variables:
            variable_name = get_json_field(variable_desc, 'name', str, None)
            variable_attributes = get_json_field(
                variable_desc,
                'attributes',
                list,
                DEFAULT_ATTRIBUTES
            )

            for attribute in variable_attributes:
                if attribute not in ATTRIBUTES:
                    print("Error: Unrecognizable attribute {} "
                          "in variable {}".format(attribute, variable_name))
                    sys.exit(1)

            variable_type = get_json_field(variable_desc, 'type', str, None)
            variable_data = get_json_field(variable_desc, 'data', None, None)


            if not is_valid_data(variable_type, variable_data):
                print("Data validation failed "
                      "for {} in {}".format(variable_name, vendor_guid))
                sys.exit(1)

        print("Successfully validated {}".format(vendor_guid))

def same_shape(data_a, data_b):
    """
    Determines whether the data_a and data_b have the same shape.

    Assumes that the data values have already been type checked before
    being passed to this function (i.e. is_valid_data has been called
    on both values with the same type).

    Shape rules are as follows:
        - Two ARRAYs have the same shape if they have the same length
        and if all corresponding elements from the ARRAYs have the same shape
        - Two STRUCTs have the same shape if they have the same
        number of members and all corresponding members from
        the STRUCTs have the same shape
        - All other data types have the same shape

    Returns True if data_a and data_b have the same shape, False otherwise
    """

    # Checking if two lists have the same shape
    # Both arrays must have same length and all elements must have same shape
    if isinstance(data_b, list):
        if not isinstance(data_a, list):
            return False
        if len(data_a) != len(data_b):
            return False

        for element_a, element_b in zip(data_a, data_b):
            if not same_shape(element_a, element_b):
                return False

        return True

    # Checking if two structs have the same shape
    # Both structs must have same length and all members must have same shape
    elif isinstance(data_b, OrderedDict):
        if not isinstance(data_a, OrderedDict):
            return False
        if len(data_a) != len(data_b):
            return False

        zipped_shapes = zip(data_a.items(), data_b.items())
        for (type_a, val_a), (type_b, val_b) in zipped_shapes:
            if type_a != type_b or not same_shape(val_a, val_b):
                return False

        return True
    else:
        # If we get here we have non-collection data.
        # Since the data has been type checked, we know they are the same type
        return True

def is_valid_integer_data(data_type, data):
    """
    Validates the given integer data based on the given data_type.

    The given data_type must be one of UINT8, UINT16, UINT32, or UINT64.
    The given data should be a string of hex digits, prefixed with "0x",
    with a max size of the corresponding type.

    If the data is invalid for the given data_type, prints an error message
    and returns False;

    Returns True if the data is a valid format for the given data_type.
    """
    max_value = MAX_UINT8
    if data_type == "UINT16":
        max_value = MAX_UINT16
    elif data_type == "UINT32":
        max_value = MAX_UINT32
    elif data_type == "UINT64":
        max_value = MAX_UINT64

    if not isinstance(data, str):
        print("Data format error: {} data must be a string".format(data_type))
        return False

    if data[:2] != "0x":
        print("Data format error: {} data must start with 0x".format(data_type))
        return False
    try:
        value = int(data, 16)
        if value > max_value:
            print("Data format error: "
                  "Given value too large for {}".format(data_type))
            return False
    except ValueError:
        print("Data format error: {} data must be a valid "
              "string of hex digits (prefixed with '0x')".format(data_type))
        return False

    return True

def is_valid_data(data_type, data):
    """
    Validates the given data based on the given data_type.

    Acceptable data types and their expected data format is as follows:
        - One of the strings "UINT8", "UIN16", "UINT32", "UINT64" (uppercase)
            - In this case, the data should be a string of hex digits,
            prefixed with "0x", with a max size of the corresponding type.
        - The string "ARRAY:<type>" where <type> describes the element type
            - In this case the data should be a list of valid elements
            for the given type. Nested ARRAYs should all have the same length.
            ARRAY elements of type STRUCT must all have the same shape.
        - The string "STRUCT"
            - In this case the data should be a dictionary
            of struct members, with keys specifying the type
            of the member, and values specifying the data
        - The string "FILE"
            - In this case, the data should be a JSON string that is a file name
            containing the binary data for the variable.

    If invalid format is detected, prints an error message and returns False.

    If format is valid, returns True.
    """

    if data_type in INT_TYPES:
        return is_valid_integer_data(data_type, data)

    elif data_type == "FILE":
        if not isinstance(data, str) or not str:
            print("Data format error: FILE data should be given "
                  "as a non-empty JSON string")
            return False
        elif not find_full_workspace_path(data):
            print("Data format error: could not find file "
                  "for the given FILE path")
            return False
        return True

    elif data_type[:6] == "ARRAY:":
        if not isinstance(data, list) or not data:
            print("Data format error: ARRAY data should be given "
                  "as a non-empty JSON array")
            return False

        sub_type = data_type[6:]

        # If the subtype is STRUCT or ARRAY then we want to make sure
        # all elements have the same same shape
        shape = None
        if sub_type == "STRUCT" or sub_type[:6] == "ARRAY:":
            shape = data[0]

        for element in data:
            if not is_valid_data(sub_type, element):
                print("Data format error: ARRAY element not "
                      "valid for array type of {}".format(data_type))
                return False

            # Check that the current element has the same shape as the
            # first element. This enforces that ARRAYs of STRUCTs have
            # STRUCTs with the same shape, and that nested ARRAYs have the
            # same number of elements in all inner arrays
            if shape and not same_shape(element, shape):
                print("Data format error: element in {} doesn't match "
                      "shape of first element in array".format(data_type))
                return False
        return True

    elif data_type == "STRUCT":
        if not isinstance(data, OrderedDict) or not data:
            print("Data format error: STRUCT data should be given "
                  "as a non-empty JSON object")
            return False
        for sub_type, element in data.items():
            # Using rfind to strip the trailing unique id for the type.
            # See the function parse_json_pairs for more details
            if not is_valid_data(sub_type[:sub_type.rfind("_")], element):
                return False
        return True
    else:
        # If we get here then we encountered a data type we do not know
        print("Data format error: Unknown type: {}".format(data_type))
        return False

def get_guid_bytes(guid_list):
    """
    Returns a bytearray represented by the given guid list.

    Expects the parameter to be of the form [uint32, uint16, uint16, uint8[8]].
    Converts each value to native endianness.
    """
    return bytearray(
        struct.pack(
            "IHH8B",
            guid_list[0],
            guid_list[1],
            guid_list[2],
            *(guid_list[3])
        )
    )

def calculate_checksum_16(byte_list):
    """
    Calculates 16-bit checksum for the given bytearray.

    Uses little-endian values.

    Assumes the given bytearray has a length that is a multiple of 2.
    """
    checksum = 0
    for i in range(0, len(byte_list), 2):
        checksum += struct.unpack("H", byte_list[i:i+2])[0]
    return 0x10000 - (checksum & 0xFFFF)

def get_firmware_volume_header(size, block_size):
    """
    Returns a bytearray containing the bytes for the firmware volume header.

    Given size and the block size to be used for the variable store.

    All values use native endianness.
    """
    zero_vector = bytearray(struct.pack("QQ", 0, 0))
    file_system_guid = get_guid_bytes(GUIDS["gEfiSystemNvDataFvGuid"])
    fv_length = bytearray(struct.pack("Q", size))
    signature = bytearray(FVH_SIGNATURE, "utf-8")
    attributes = bytearray(struct.pack("I", FVH_ATTRIBUTES))
    header_len = bytearray(struct.pack("H", FVH_HEADER_LENGTH))
    checksum = bytearray(struct.pack("H", 0))
    header_offset = bytearray(struct.pack("H", FVH_HEADER_OFFSET))
    reserved = bytearray(struct.pack("B", FVH_RESERVED))
    revision = bytearray(struct.pack("B", FVH_REVISION))
    block_map0 = bytearray(
        struct.pack(
            "II",
            int(size / block_size),
            block_size
        )
    )
    block_map1 = bytearray(
        struct.pack(
            "II",
            FVH_BLOCKMAP1_NUM_BLOCKS,
            FVH_BLOCKMAP1_BLOCK_SIZE
        )
    )

    header_bytes = (zero_vector + file_system_guid + fv_length + signature
                    + attributes + header_len + checksum + header_offset
                    + reserved + revision + block_map0 + block_map1)

    checksum = bytearray(struct.pack("H", calculate_checksum_16(header_bytes)))

    # 16 bytes for zero vector
    # 16 bytes for guid
    # 8 bytes for length
    # 4 bytes for signature
    # 4 bytes for attributes
    # 2 bytes for header length
    checksum_index = 50

    header_bytes[checksum_index:checksum_index + 2] = checksum

    return header_bytes

def get_variable_store_header(size):
    """
    Returns a bytearray containing the bytes for the variable store header.

    Given the size of the variable store which is used to calculate the
    size to be used for the header.

    All values use native endianness.
    """
    variable_store_guid = get_guid_bytes(
        GUIDS["gEfiAuthenticatedVariableGuid"]
    )

    header_size = bytearray(struct.pack("I", size - FVH_HEADER_LENGTH))
    formatted = bytearray(struct.pack("B", VSH_FORMATTED))
    healthy = bytearray(struct.pack("B", VSH_HEALTHY))
    reserved = bytearray(struct.pack("H", VSH_RESERVED))
    reserved1 = bytearray(struct.pack("I", VSH_RESERVED1))

    return (variable_store_guid + header_size + formatted
            + healthy + reserved + reserved1)


def get_largest_type(pack_str):
    """
    Gets the largest formatting character in the given pack_str

    It's assumed that pack_str only contains 'B', 'H', 'I', 'Q', and 'x'.

    Returns a string representing the largest formatting character.
    """
    # Since B, H, I, Q are ordered alphabetically by size, we can just
    # remove the pad character 'x' and then take the max char in the string
    return max(pack_str.replace('x', ''))

def process_data(data_type, data):
    """
    Processes the given data of the given data_type for struct packing.

    Flattens the given data structure into a list of values and builds up a
    string that can be used with struct.pack such that the values are properly
    aligned based on their sizing and C alignment rules.

    Returns a tuple of (pack_str, data_list) where pack_str is a string that
    can be used to pack the elements in data_list.
    """

    if data_type == "UINT8":
        return "B", [int(data, 16)]
    elif data_type == "UINT16":
        return "H", [int(data, 16)]
    elif data_type == "UINT32":
        return "I", [int(data, 16)]
    elif data_type == "UINT64":
        return "Q", [int(data, 16)]
    elif data_type == "FILE":
        file_path = find_full_workspace_path(data)
        with io.open(file_path, 'rb') as input_data:
            bytes_read = bytearray(input_data.read())
            return  "{}B".format(len(bytes_read)), list(bytes_read)
    elif data_type[:6] == "ARRAY:":
        sub_type = data_type[6:]
        result_str = ""
        result_data = []
        for element in data:
            pack_str, data_list = process_data(sub_type, element)
            result_str += pack_str
            result_data += data_list

        return result_str, result_data
    else: # data_type must be STRUCT here
        size_so_far = 0
        result_str = ""
        result_data = []
        for sub_type, element in data.items():
            # Using rfind to strip the trailing unique id for the type.
            # See the function parse_json_pairs for more details
            sub_type = sub_type[:sub_type.rfind("_")]
            pack_str, data_list = process_data(sub_type, element)

            # If our inner element is also a struct, we have to make sure
            # that it starts on an aligned multiple according to its largest
            # element per nested struct alignment rules.
            if sub_type == "STRUCT":
                alignment = struct.calcsize(get_largest_type(pack_str))
                num_pad_bytes = ((alignment - (size_so_far % alignment))
                                    % alignment)
                pack_str = ('x' * num_pad_bytes) + pack_str

            size_so_far += struct.calcsize(pack_str)
            result_str += pack_str
            result_data += data_list

        # make sure whole struct is aligned to largest element
        result_str += "0" + get_largest_type(result_str)

        return result_str, result_data

def get_variable_bytes(vendor_guid_list, name, attribute_value, data_bytes):
    """
    Returns a bytearray representing the bytes for a variable section.

    The variable section contains a variable header, followed by the variable
    metadata and actual data.

    Expects vendor_guid to be a list of the format
    [uint32, uint16, uint16, uint8[8]], name to be a string
    name of the variable, attribute_value to be an int value to be set as the
    attributes for the variable, and data_bytes to be a bytearray
    containing the binary for the data (should be in desired endianness).

    All values use native endianness.
    """
    start_id = bytearray(struct.pack("H", VH_START_ID))
    state = bytearray(struct.pack("B", VH_STATE))
    reserved = bytearray(struct.pack("B", VH_RESERVED))
    monotonic_count = bytearray(struct.pack("Q", VH_MONOTONIC_COUNT))
    time_stamp = bytearray(struct.pack("QQ", 0, 0))
    public_key_index = bytearray(struct.pack("I",VH_PUBLIC_KEY_INDEX))

    # add null terminator for string
    name += "\0"
    # size will be twice the number of characters since we are using utf-16
    name_size = len(name) * 2
    name_bytes = bytearray(name, 'utf-16le')
    name_size_bytes = bytearray(struct.pack('I', name_size))

    attribute_bytes = bytearray(struct.pack('I', attribute_value))

    data_size_bytes = bytearray(struct.pack('I', len(data_bytes)))

    vendor_guid_bytes = get_guid_bytes(vendor_guid_list)

    data = (start_id + state + reserved + attribute_bytes + monotonic_count
            + time_stamp + public_key_index + name_size_bytes + data_size_bytes
            + vendor_guid_bytes + name_bytes + data_bytes)

    #Pad variable data to 4 bytes alignment
    data_extra = len(data) % 4
    if (data_extra > 0):
        data += bytes(4 - data_extra)

    return data

def get_variable_bytes_from_json_file(json_filename):
    output_bytes = bytearray()
    with io.open(json_filename, 'r', encoding='utf-8') as input_file:
        variable_json_desc = json.load(
            input_file,
            object_pairs_hook=parse_json_pairs
        )

        print("Validating data in {}...".format(json_filename))
        validate_json_desc(variable_json_desc)

        print("Processing data in {}...".format(json_filename))
        includes = variable_json_desc['includes']
        for include_filename in includes:
            # We know there will be a unique full path after validation
            full_path = find_full_workspace_path(include_filename)
            output_bytes += get_variable_bytes_from_json_file(full_path)

        definitions = variable_json_desc['definitions']
        for vendor_guid_desc in definitions:
            vendor_guid = vendor_guid_desc['vendor_guid']
            for variable_desc in vendor_guid_desc['variables']:
                name = variable_desc['name']
                attributes = variable_desc.get('attributes', [])
                # Always include the default attributes
                attributes += DEFAULT_ATTRIBUTES
                data_type = variable_desc['type']
                data = variable_desc['data']

                attribute_value = 0
                for attribute in attributes:
                    attribute_value |= ATTRIBUTES[attribute]

                data_pack_str, data_val_list = process_data(data_type, data)
                data_bytes = struct.pack(data_pack_str, *data_val_list)

                output_bytes += get_variable_bytes(
                    GUIDS[vendor_guid],
                    name,
                    attribute_value,
                    data_bytes
                )

    return output_bytes

def GenVariableStore (input_filename, output_filename, size=FVH_FV_LENGTH, block_size=FVH_BLOCKMAP0_BLOCK_SIZE):
    # Only support little endian hosts. Target alignment is little endian,
    # so by enforcing that host endian matches, then we can make use of the
    # struct module's auto-alignment and not have to manually implement it.
    # For our use cases, this seems like an ok restriction
    # but if big endian support is necessary then we will
    # need to add manual alignment logic.
    if sys.byteorder != 'little':
        print("Error: Script currently can only "
              "be run on little endian machines.")
        sys.exit(1)

    output_bytes = (get_firmware_volume_header(size, block_size)
                    + get_variable_store_header(size)
                    + get_variable_bytes_from_json_file(input_filename))

    output_bytes += bytearray(b'\xFF'*(size - len(output_bytes)))

    if (not os.path.isdir(os.path.dirname(output_filename))):
        os.mkdir(os.path.dirname(output_filename))

    with io.open(output_filename, 'wb') as output_file:
        output_file.write(output_bytes)
