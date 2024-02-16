/** @file
  GIC parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef GIC_PARSER_H_
#define GIC_PARSER_H_

/** GicD parser function for T194.

  The following structure is populated:
  typedef struct CmArmGicDInfo {
    /// The Physical Base address for the GIC Distributor.
    UINT64    PhysicalBaseAddress;      // {Populated}

    // The global system interrupt
    //  number where this GIC Distributor's
    //  interrupt inputs start.
    UINT32    SystemVectorBase;         // 0

    // The GIC version as described
    // by the GICD structure in the
    // ACPI Specification.
    UINT8     GicVersion;               // 0
  } CM_ARM_GICD_INFO;

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
GicDParserT194 (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  );

/** GicD parser function

  The following structure is populated:
  typedef struct CmArmGicDInfo {
    /// The Physical Base address for the GIC Distributor.
    UINT64    PhysicalBaseAddress;

    // The global system interrupt
    //  number where this GIC Distributor's
    //  interrupt inputs start.
    UINT32    SystemVectorBase;

    // The GIC version as described
    // by the GICD structure in the
    // ACPI Specification.
    UINT8     GicVersion;
  } CM_ARM_GICD_INFO;

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
GicDParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  );

/** GicC parser function.

  The following structures are populated:
  - EArmObjGicCInfo

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.
  @param [out] TokenMapPtr     The tokens corresponding to the GicC objects.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
GicCParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch,
  OUT CM_OBJECT_TOKEN              **TokenMapPtr OPTIONAL
  );

/** Lpi parser function.

  The following structures are populated:
  - EArmObjLpiInfo
  - EArmObjCmRef (LpiTokens)

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.
  @param [out] Token           The token for the array of object tokens.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
LpiParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch,
  OUT CM_OBJECT_TOKEN              *Token OPTIONAL
  );

/** GicRedistributor parser function

  The following structure is populated:
  typedef struct CmArmGicRedistInfo {
    // The physical address of a page range
    // containing all GIC Redistributors.
    //
    UINT64    DiscoveryRangeBaseAddress;

    /// Length of the GIC Redistributor Discovery page range
    UINT32    DiscoveryRangeLength;
  } CM_ARM_GIC_REDIST_INFO;

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
GicRedistributorParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  );

/** GicIts parser function

  The following structure is populated:
  typedef struct CmArmGicItsInfo {
    /// The GIC ITS ID
    UINT32    GicItsId;

    /// The physical address for the Interrupt Translation Service
    UINT64    PhysicalBaseAddress;

    /// The proximity domain to which the logical processor belongs.
    ///  This field is used to populate the GIC ITS affinity structure
    ///  in the SRAT table.
    UINT32    ProximityDomain;
  } CM_ARM_GIC_ITS_INFO;

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
GicItsParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  );

/** GIC MSI Frame parser function

The following structure is populated:
typedef struct CmArmGicMsiFrameInfo {
  /// The GIC MSI Frame ID
  UINT32    GicMsiFrameId;

  /// The Physical base address for the MSI Frame
  UINT64    PhysicalBaseAddress;

  /// The GIC MSI Frame flags
  /// as described by the GIC MSI frame
  /// structure in the ACPI Specification.
  UINT32    Flags;

  /// SPI Count used by this frame
  UINT16    SPICount;

  /// SPI Base used by this frame
  UINT16    SPIBase;
} CM_ARM_GIC_MSI_FRAME_INFO;

A parser parses a Device Tree to populate a specific CmObj type. None,
one or many CmObj can be created by the parser.
The created CmObj are then handed to the parser's caller through the
HW_INFO_ADD_OBJECT interface.
This can also be a dispatcher. I.e. a function that not parsing a
Device Tree but calling other parsers.

@param [in]  ParserHandle    A handle to the parser instance.
@param [in]  FdtBranch       When searching for DT node name, restrict
                             the search to this Device Tree branch.

@retval EFI_SUCCESS             The function completed successfully.
@retval EFI_ABORTED             An error occurred.
@retval EFI_INVALID_PARAMETER   Invalid parameter.
@retval EFI_NOT_FOUND           Not found.
@retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
GicMsiFrameParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  );

#endif // GIC_PARSER_H_
