# Configuration Manager (ACPI and SMBIOS Table Generation)
Feature Name: Configuration Manager (ACPI and SMBIOS Table Generation)

PI Phase supported: DXE

SMM Required: No

## References
https://en.wikipedia.org/wiki/Advanced_Configuration_and_Power_Interface
https://uefi.org/specs/ACPI/6.4/03_ACPI_Concepts/ACPI_Concepts.html

## Purpose
Configuration Manager module within the UEFI code parses the Device Tree and other sources and stores the parsed information in a repository of information (Platform Repository) that can then be queried by other UEFI code to gather information about the system. This data is used to generate ACPI and SMBIOS tables.

ACPI tables provide a standard way of describing the platform to the operating system which are used during boot. These allow the OS to discover and configure the hardware in the system.

## Terminology

Configuration Manager (CM) – the overarching set of code used to store and retrieve information about the system. It consists of several libraries, drivers, and protocols. Most of this code is platform-specific.

Platform Repository (ConfigurationManagerLib) – the library functions used for allocating space for storing the CM information, and the internal APIs for adding and finding stored information.

CM Descriptor – a structure that stores metadata about a CM Object (Type, data size, number of elements) and a pointer to that Object. The CM APIs often use CM Descriptors to pass information.

CM Namespace – Currently there are separate Namespaces for ARM, Standard, OEM, and SMBIOS objects. Other Namespaces will be added for supporting objects specific to other architectures.

CM Object Type – One of the types of data defined for the Namespace. The Type is enumerated within the Namespace and has a typedef for the structure of the Object.

CM ObjectId – An encoding of the Namespace and Object Type.

CM Object – a blob of data pointed to by a CM Descriptor, describing some information about the system. Its format depends on the Object Type. The blob is an array of one or more Elements of the Object Type.

CM Element – a single entry of the object type in the CM Object blob.

CM Token – a opaque unique identifier managed by the CM APIs, which is used to identify a CM Object or a CM Element. A token can be used to find the object or element in the CM and is often used to provide references between CM Objects/Elements. A multi-element Object will have a token for each individual Element and another one for the whole Object.

CM Parser – a library of code that generates one or more CM Objects and adds them to the Platform Repository. Often these Parsers parse the flattened Device Tree file to gather the information they need for creating the Objects, leading to them being named Parsers.

CM Generator – a set of code used by DynamicTablesPkg to generate and install ACPI tables and SMBIOS tables based on CM Objects. This code can be shared by multiple platforms.

## High Level Theory of Operation - Configuration Manager and Platform Repository
UEFI maintains a Device Tree file that lists all the hardware components or devices present in the platform as various nodes. Each node has a compatibility string, status of the device and various properties associated to the device.

The CM Parsers often parse the flattened device tree nodes for various devices present and gathers their properties. The devices are identified using their compatibility string. The 'status' property could be used to identify if the device is present or enabled in the system.

A system could have multiple devices with the same compatibility string and they will often be stored as the separate Elements in the same Object.

Each Object and its properties are stored in the Platform Repository as a unique Object with a unique object ID (Token) and also a unique Token for each Element in the Object (for multi-entry Objects). An individual Element can be referenced by its Token, or the whole Object can be referenced using the Object's Token.

Data is stored into and retrieved from the Platform Repository using a Descriptor. The Descriptor contains the ObjectId, Total Data Size, Data pointer, and Element Count. The ObjectId contains a namespace indicator and the type of object. The Data pointer is a pointer to a block of memory of the total size, which is divided evenly into the number of Elements. The format of each Element depends on the type of the Object. When an Object is added to the Platform Repository, the repository makes a COPY of the data pointed to by the Data pointer. This means that once an Object is added to the Platform Repository, the Object’s data cannot be modified. Note: Any pointers stored within the Data are assumed to remain valid and unmodified and will not be copied.

This is a generic example of how to create a Descriptor in the ARM namespace

```
Desc.ObjectId = CREATE_CM_ARM_OBJECT_ID (ObjectTypeEnum);
Desc.Size     = sizeof (Object);
Desc.Data     = &Object;
Desc.Count    = ARRAY_SIZE (Object);
```

Code that needs to know about the system can look up the information it needs by querying the Platform Repository for the Objects that contain the needed information. These queries are done using the Namespace and Type, and optionally a Token. The Generators for ACPI and SMBIOS are the primary consumers of Objects.

## Writing a Parser
Each Parser lives under its own subdirectory of ConfigurationManagerData. The structure looks like:
```
ParserName/
	ParserName.inf
	ParserName.c
	ParserName.h
```

The top level NVIDIA.common.dsc.inc has a reference to each Parser’s inf file under ConfigurationManagerDataDxe.inf, typically as a NULL library
```
  Silicon/NVIDIA/Drivers/ConfigurationManagerData/ConfigurationManagerDataDxe.inf {
    <LibraryClasses>
      …
      NULL|Silicon/NVIDIA/Drivers/ConfigurationManagerData/ParserName/ParserName.inf
```
In some cases, some Parsers depend on other Parsers being present. In those cases, the one being depended on is explicitly named rather than NULL, and the ones depending on it list it under [LibraryClasses] in their inf files.

The ParserName.inf file must list ConfigurationManagerDataRepoLib under [LibraryClasses] and must have these lines:
```
  LIBRARY_CLASS  = NULL|DXE_DRIVER
  CONSTRUCTOR    = Register<ParserName>Parser
```
(Note: Replace NULL with the ParserName if other parsers depend on it)

The ParserName.c file must include the toplevel parser function, as well as a macro call that creates the Register<ParserName>Parser function:
```
EFI_STATUS
EFIAPI
<ParserName> (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  ) {
…
}

REGISTER_PARSER_FUNCTION (<ParserName>, <ParserSkipString or NULL>)
```
The optional ParserSkipString (eg. "skip-iort-table") is a string that can be put into the DTB to cause the parser to be skipped during boot.

The Parser’s purpose is ultimately to add one or more Objects to the Platform Repository.

## Token management

Every Object and every Object’s Element stored in the Platform Repository is stored with an associated Token, which uniquely identifies that Object or Element.

In many cases the code that is adding the Object doesn’t care about what the Token values are and can simply let CM create tokens for the Objects and Elements. There are APIs for adding Objects that will automatically allocate Tokens, and these will return the Tokens back to the caller in case the caller needs to know what the Tokens are afterwards (See “Adding an Object to the Platform Repository” below).

In other cases, such as when an Object has a field that stores its own Token, the Object cannot be added until after the Token has already been assigned. For those cases, there is an API for allocating Tokens and then APIs to add the Objects and Elements with their associated Tokens. Parsers can use the convenience API NvAllocateCmTokens to allocate the required number of Tokens, and then add the Object with NvAddMultipleCmObjWithTokens. If Tokens need to be allocated by non-Parser code, the AllocateTokens API of the gNVIDIAConfigurationManagerTokenProtocolGuid protocol can be used to allocate the Tokens.

## Adding an Object to the Platform Repository

In most cases, a Parser build into ConfigurationManagerDataDxe is adding an Object to the Platform Repository. The Parser should use one of the convenience APIs from NvCmObjectDescUtility to add an Object:
-	NvAddSingleCmObj – For adding a single Object and letting CM determine its Token
-	NvAddMultipleCmObjGetTokens – For adding a multi-Element Object and letting CM determine its Token and its Elements’ Tokens
-	NvAddMultipleCmObjWithTokens – For adding an Object (single or multi-Element) but telling CM which Tokens to use for the Elements, and optionally which Token to use for the Object
-	NvAddMultipleCmObjWithCmObjRef – For adding a multi-Element Object, optionally telling CM which Tokens to use for the Elements, and having CM create and store a EArmObjCmRef Object that contains the Element Tokens. The Token of that EArmObjCmRef Object is returned.
-	NvExtendCmObj – For adding additional Elements to an existing CM Object
-	NvAddAcpiTableGenerator – For conditionally adding Generator to the EStdObjAcpiTableList Object (or creating the Object if it doesn’t exist yet) if it isn’t already present. Note that it will only be skipped if it is already present and the Generator’s AcpiTableData pointer matches the existing Element’s.

When an Object is added to the Platform Repository its data is copied, so the Parser should then free the Object if the caller’s copy is no longer needed.

In rare cases, code that runs before ConfigurationManagerDataDxe (such as the PCIE driver) needs to put an Object into the Platform Repository before the Platform Repository has been initialized. This can be done by installing structured data describing the Object to gNVIDIAConfigurationManagerDataObjectGuid, as illustrated below:

```
  EDKII_PLATFORM_REPOSITORY_INFO                    *RepoInfo;
…
  RepoInfo[0].CmObjectId    = CREATE_CM_ARM_OBJECT_ID (EArmObjPciConfigSpaceInfo);
  RepoInfo[0].CmObjectToken = CM_NULL_TOKEN; //Let CM assign a Token
  RepoInfo[0].CmObjectSize  = ConfigSpaceInfoSize;
  RepoInfo[0].CmObjectCount = NumberOfHandles;
  RepoInfo[0].CmObjectPtr   = ConfigSpaceInfo;
…
  NewHandle = 0;
  Status    = gBS->InstallMultipleProtocolInterfaces (
                     &NewHandle,
                     &gNVIDIAConfigurationManagerDataObjectGuid,
                     RepoInfo,
                     NULL
                     );
```

Then, when the ProtocolBasedObjectsParser runs it reads this data and adds it to the Platform Repository in the following manner:
1.	If CmObjectToken is NULL and CmObjectId isn’t in the Platform Repository, simply add it as a new Object
2.	If CmObjectToken is NULL and CmObjectId is already in the PlatformRepository, extend the Object with the new Elements
3.	If CmObjectToken is non-NULL and CmObjectCount is 1 or CmObjectId is EArmObjCmRef, then add the Object and use CmObjectToken as its Token
4.	If CmObjectToken is non-NULL and CmObjectCount is more than 1 and CmObjectId is not EArmObjCmRef, then use CmObjectToken to find a EArmObjCmRef that contains the tokens to use for the Elements and add the Object using the EArmObjCmRef data as the tokens for the Elements.

Note that to use method 4, the code needs to use the gNVIDIAConfigurationManagerTokenProtocolGuid protocol to allocate N+1 Tokens and then use one of those Tokens with method 3 to store an EArmObjCmRef Object containing the other N Tokens first.

## Getting information from the Platform Repository

If a Parser needs to look up an Object or Element in the Platform Repository it can use the NvFindEntry convenience function.

Other code can use the GetObject API of the gEdkiiConfigurationManagerProtocolGuid protocol.

## Modules
### edk2-nvidia/Silicon/NVIDIA/Library/ConfigurationManagerLib
This library allocates and initializes the Platform Repository structure and contains APIs used by the CM to manipulate it.
### edk2-nvidia/Silicon/NVIDIA/Drivers/ConfigurationManager
This driver implements the UEFI protocol for Getting an Object from the Platform Repository. The Set protocol is not implemented.
### edk2-nvidia/Silicon/NVIDIA/Drivers/ConfigurationManagerData
This driver populates the Platform Repository using Parsers to generate objects. Each parser is a sub-library for this driver. The top level NVIDIA INF file controls which Parser libraries are included in the build.
### edk2-nvidia/Silicon/NVIDIA/Drivers/ConfigurationManagerTokenDxe
This driver provides a protocol for allocating the Tokens used by the CM.
### edk2/DynamicTablesPkg/Library/[Acpi, Common, Smbios]
These libraries have the ACPI and SMBIOS table Generators.

## ACPI Table generation
ACPI Data tables contain simple data and no AML byte code. These tables include MADT, IORT, PPTT, SRAT etc.

Some ACPI tables include AML code produced from the ACPI Source Language (ASL). These include the DSDT or any SSDTs.

The ACPI table Generators look for various Objects using their ObjectIds. Different tables list different devices or components and they are looked up using the ObjectIds (and sometimes Tokens) and put together into a table.

For eg. the PPTT table looks for Cache and Core configuration and lists the Processor hierarchy nodes for all the Caches and cores that are enabled in the system.

## Viewing ACPI Tables
UEFI Shell can be used to view the generated ACPI Tables using the 'acpiview' command.

The “iasl -l $TABLE_NAME” command can be used to create the AML disassembly of an ACPI table present under /sys/firmware/acpi/tables/

## SMBIOS Table generation
Currently all the data needed for the SMBIOS tables is parsed by the "SmbiosParser" Parser and the subroutines it calls. It has a subroutine for each table type (or in some cases a set of inter-related tables), which lives in its own subdirectory of SmbiosParser. The top level SmbiosParser.c file has a local array listing the subroutines to call. It calls each one in turn to parse and store the information needed for generating those supported tables. Generators for the SMBIOS tables then pull the information they need from the Platform Repository to generate the tables.

To add a support for a new SMBIOS table (type ##), you need to:
1. Create a subdirectory (ConfigurationSmbiosType##) for its subroutine
2. Write the SmbiosType##Parser.c subroutine, with the signature
```
EFI_STATUS
EFIAPI
InstallSmbiosType##Cm (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN OUT CM_SMBIOS_PRIVATE_DATA    *Private
  )
```
3. Add the new subroutine to the CmInstallSmbiosRecords array in SmbiosParser.c
4. Add the new file to SmbiosParser.inf under [Sources]
5. Write a Generator (SmbiosType##Generator.c) in DynamicTablesPkg to generate the table based on the information stored in the Platform Repository (if the Generator doesn't already exist)

## Viewing SMBIOS Tables
The “dmidecode” command can be used to view a parsed output of the SMBIOS tables

## List of supported ACPI Tables
```
APIC
APMT
BERT
BGRT
DSDT
EINJ
ERST
FACP
FPDT
GTDT
HEST
HMAT
IORT
MCFG
MPAM
PPTT
SDEI
SLIT
SPCR
SPMI
SRAT
SSDT
WSMT
```
## List of supported SMBIOS Tables
```
Type 0
Type 1
Type 2
Type 3
Type 4
Type 7
Type 8
Type 9
Type 11
Type 13
Type 16
Type 17
Type 19
Type 32
Type 38
Type 39
Type 41
Type 43
Type 45
```

Table list last updated April 17, 2024