#!/usr/bin/python3

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

"""
Utilities for parsing a pcd database file. Right now the only functionality
supported is for DynamicHii variables that are used in GenVariableStore.py.

Future functionality should be easy to incorporate in the PcdDatabase class
"""
import argparse
import io
import os
import struct
import sys
from collections import namedtuple

# Various PCD_TYPE definitions
PCD_TYPE_SHIFT = 28
PCD_TYPE_DATA = 0 << PCD_TYPE_SHIFT
PCD_TYPE_HII = 8 << PCD_TYPE_SHIFT
PCD_TYPE_VPD = 4 << PCD_TYPE_SHIFT
PCD_TYPE_STRING = 1 << PCD_TYPE_SHIFT
PCD_TYPE_MASK = (PCD_TYPE_DATA | PCD_TYPE_HII | PCD_TYPE_VPD | PCD_TYPE_STRING)

PCD_DATUM_TYPE_SHIFT = 24

PCD_DATUM_TYPE_POINTER = 0 << PCD_DATUM_TYPE_SHIFT
PCD_DATUM_TYPE_UINT8 = 1 << PCD_DATUM_TYPE_SHIFT
PCD_DATUM_TYPE_UINT16 = 2 << PCD_DATUM_TYPE_SHIFT
PCD_DATUM_TYPE_UINT32 = 4 << PCD_DATUM_TYPE_SHIFT
PCD_DATUM_TYPE_UINT64 = 8 << PCD_DATUM_TYPE_SHIFT

PCD_DATUM_MASK = (PCD_DATUM_TYPE_POINTER |
                  PCD_DATUM_TYPE_UINT8   |
                  PCD_DATUM_TYPE_UINT16  |
                  PCD_DATUM_TYPE_UINT32  |
                  PCD_DATUM_TYPE_UINT64)

PCD_DATUM_TYPE_SHIFT2 = 20

PCD_DATUM_TYPE_UINT8_BOOLEAN = 1 << PCD_DATUM_TYPE_SHIFT2

PCD_DATABASE_OFFSET_MASK = (~(PCD_TYPE_MASK |
                              PCD_DATUM_MASK |
                              PCD_DATUM_TYPE_UINT8_BOOLEAN))

# struct format string for a GUID
GUID_FORMAT = "IHH8B"

# Describes the edk2 C type PCD_DATABASE_INIT
PcdDbHeader = namedtuple(
    "PcdDbHeader",
    ("guid build_version length sku_id all_sku_length uninit_db_size "
     "local_token_table_offset ex_map_table_offset guid_table_offset "
     "str_table_offset size_table_offset sku_id_table_offset "
     "pcd_name_table_offset local_token_count ex_token_count guid_table_count")
)

# Describes the edk2 C type VARIABLE_HEAD
VariableHead = namedtuple(
    "VariableHead",
    ("string_index default_value_offset guid_table_index "
     "offset attributes property reserved")
)

class PcdDatabase(object):
    """
    Class for storing and parsing a raw pcd database file.

    Right now most of the functionality is used to parse DynamicHii pcds for
    the GenVariableStore.py script.
    """
    def __init__(self, pcd_db_filename):
        """
        Creates a PcdDatabase object using the given pcd database filename.

        If the filename doesn't exist, then the program will halt with an error.

        Otherwise assumes the given pcd database file is in a valid format and
        will use it to retrieve data.
        """
        with io.open(pcd_db_filename, 'rb') as pcd_db_file:
            self.pcd_db = bytearray(pcd_db_file.read())

        # Have to parse the guids separate so that we can build up a structure
        # that makes sense for guids ([uint32, uint16, uint16, uint8[8]])
        guid_vals = list(struct.unpack_from(GUID_FORMAT, self.pcd_db))
        self.guid_list = [
            guid_vals[0],
            guid_vals[1],
            guid_vals[2],
            guid_vals[3:]
        ]

        header_vals = [guid_vals]
        header_vals.extend(struct.unpack_from(
            "IIQIIIIIIIIIHHH",
            self.pcd_db,
            16
        ))

        self.header = PcdDbHeader._make(header_vals)

    def get_name(self, string_index):
        """
        Returns a string name from the given index in the string table.

        Assumes the string is in utf-16 format.
        """
        offset = self.header.str_table_offset + string_index
        index = offset
        while self.pcd_db[index] is not 0 or self.pcd_db[index + 1] is not 0:
            index += 2
        name = self.pcd_db[offset:index]
        return name.decode('utf-16')

    def get_local_token_number(self, index):
        """
        Returns a local token number from the given index in the token table.
        """
        return struct.unpack_from(
            "I",
            self.pcd_db,
            self.header.local_token_table_offset + index * 4
        )[0]

    def get_variable_head(self, offset):
        """
        Returns a VariableHead namedtuple from the given offset in the database.
        """
        return VariableHead._make(
            struct.unpack_from("IIHHIHH", self.pcd_db, offset)
        )

    def get_guid(self, guid_index):
        """
        Returns a guid_list for the given guid_index in the guid table.

        The guid_list is of the form [uint32, uint16, uint16, uint8[8]].
        """
        offset = self.header.guid_table_offset + guid_index * 16
        guid_vals = struct.unpack_from(GUID_FORMAT, self.pcd_db, offset)
        return [guid_vals[0], guid_vals[1], guid_vals[2], guid_vals[3:]]

    def get_size_table_entry(self, local_token, entry):
        """
        Returns the size of the given local_token/entry from the size table.

        Determines the index in the size table based on the local_token.
        Entry should be either 0 or 1, defining which of the two entries
        in the size table should be used for the given local_token.
        """
        entry_offset = local_token * 2 + (entry % 2)
        return struct.unpack_from(
            "H",
            self.pcd_db,
            self.header.size_table_offset + entry_offset
        )[0]

    def get_size(self, local_token):
        """
        Returns the size of the variable with the given local_token.
        """
        datum_size = (local_token & PCD_DATUM_MASK) >> PCD_DATUM_TYPE_SHIFT
        if datum_size == 0:
            if local_token & PCD_DATUM_MASK == PCD_TYPE_VPD:
                datum_size = self.get_size_table_entry(local_token, 0)
            else:
                datum_size = self.get_size_table_entry(local_token, 1)

        return datum_size

    def get_hii_data(self, local_token, offset, size):
        """
        Returns a bytearray for the variable with the given local_token.

        Given a local_token for the variable, the data offset of the variable,
        and the size of the variable.
        """
        if local_token & PCD_TYPE_STRING:
            string_index = struct.unpack_from(
                "I",
                self.pcd_db,
                offset
            )[0]
            offset = self.header.str_table_offset + string_index

        return self.pcd_db[offset:offset + size]

    def get_all_hii_variables(self):
        """
        Returns a list of tuples describing the hii variables in this database.

        The tuples have the form (guid_list, name, attributes, data), where
        guid_list is the guid in form [uint32, uint16, uint16, uint8[8]],
        name is a string name of the variable, attributes is a integer value
        describing the attributes of the varialbe, and data is a bytearray
        of the data for the variable (in native endianness).
        """
        result = []
        for i in range(self.header.local_token_count):
            local_token = self.get_local_token_number(i)
            if local_token & PCD_TYPE_HII:
                offset = local_token & PCD_DATABASE_OFFSET_MASK
                variable_head = self.get_variable_head(offset)
                name = self.get_name(variable_head.string_index)
                guid = self.get_guid(variable_head.guid_table_index)
                size = self.get_size(local_token) + variable_head.offset
                data = self.get_hii_data(
                    local_token,
                    variable_head.default_value_offset,
                    size
                )
                result.append((guid, name, variable_head.attributes, data))

        return result
