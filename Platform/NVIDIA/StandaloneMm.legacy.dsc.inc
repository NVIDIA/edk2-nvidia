#
#  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#

# Override StandaloneMmPkg libraries and use the StandaloneMmPkgLegacy fork
# instead.  This file can be included by images that have not yet migrated to
# the refactored StandaloneMmPkg.


[LibraryClasses]
  FvLib|Silicon/StandaloneMmPkgLegacy/Library/FvLib/FvLib.inf
  HobLib|Silicon/StandaloneMmPkgLegacy/Library/StandaloneMmCoreHobLib/StandaloneMmCoreHobLib.inf
  MemLib|Silicon/StandaloneMmPkgLegacy/Library/StandaloneMmMemLib/StandaloneMmMemLib.inf
  MemoryAllocationLib|Silicon/StandaloneMmPkgLegacy/Library/StandaloneMmCoreMemoryAllocationLib/StandaloneMmCoreMemoryAllocationLib.inf
  PeCoffExtraActionLib|Silicon/StandaloneMmPkgLegacy/Library/StandaloneMmPeCoffExtraActionLib/StandaloneMmPeCoffExtraActionLib.inf

[LibraryClasses.common.MM_STANDALONE]
  HobLib|Silicon/StandaloneMmPkgLegacy/Library/StandaloneMmHobLib/StandaloneMmHobLib.inf
  MemoryAllocationLib|Silicon/StandaloneMmPkgLegacy/Library/StandaloneMmMemoryAllocationLib/StandaloneMmMemoryAllocationLib.inf
