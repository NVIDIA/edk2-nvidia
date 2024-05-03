# SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent

###############################################################################
# Stuart build for host-based unittests.


from edk2nv.stuart import NVIDIACiSettingsManager


class HostBasedTestSettingsManager(NVIDIACiSettingsManager):
    ''' CiSettingsManager for host-based tests. '''

    def GetName(self):
        return "HostBasedTests"

    def GetPackagesPath(self):
        return super().GetPackagesPath() + [
            self.GetEdk2NvidiaDir() + "Platform/NVIDIA/"
        ]
