/** @file

  NVIDIA GPU DSD AML Generation Protocol Handler.

  Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

///
/// Libraries
///

#include <Library/AmlLib/AmlLib.h>

#include "GpuDsdAmlGeneration.h"
#include "core/GPUMemoryInfo.h"

///
/// Local Macro definitions
///

///
/// Prototypes of the protocol function implementations
///

EFI_STATUS
EFIAPI
GpuDsdAmlGenerationGetDsdNode (
  IN NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL  *This,
  OUT AML_NODE_HANDLE                        *Node
  );

EFI_STATUS
EFIAPI
GpuDsdAmlGenerationGetEgmBasePa (
  IN NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL  *This,
  OUT UINT64                                 *EgmBasePa
  );

EFI_STATUS
EFIAPI
GpuDsdAmlGenerationGetEgmSize (
  IN NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL  *This,
  OUT UINT64                                 *EgmSize
  );

EFI_STATUS
EFIAPI
GpuDsdAmlGenerationGetMemorySize (
  IN NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL  *This,
  OUT UINT64                                 *MemorySize
  );

///
/// Protocol template declaration
///

NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL_PRIVATE_DATA  mPrivateDataTemplate = {
  /* .Signature */ NVIDIA_GPU_DSD_AML_GENERATION_PRIVATE_DATA_SIGNATURE,
  /* .Handle */ NULL,
  /* .GpuDsdAmlGenerationProtocol */ {
    GpuDsdAmlGenerationGetDsdNode,
    GpuDsdAmlGenerationGetMemorySize,
    GpuDsdAmlGenerationGetEgmBasePa,
    GpuDsdAmlGenerationGetEgmSize
  }
};

///
/// Private functions
///

///
/// Public functions
///

/** Prep the AML DSD node for the controller
  @param ControllerHandle [IN]  Controller handle
  @param GpuNodeParam [IN OUT]  Node to add the DSD to
  @retval Status of the call
             EFI_SUCCESS            - call was successful
             EFI_INVALID_PARAMETER  - ControllerHandle is NULL
                                    - GetGpuMemoryInfo returned an error
                                    - GpuMemInfo is NULL
**/
EFI_STATUS
EFIAPI
GenerateGpuAmlDsdNode (
  IN        EFI_HANDLE              ControllerHandle,
  IN  OUT   AML_OBJECT_NODE_HANDLE  *GpuNodeParam
  )
{
  EFI_STATUS              Status;
  AML_OBJECT_NODE_HANDLE  DsdNode;
  AML_OBJECT_NODE_HANDLE  PackageNode;
  CHAR8                   *Name;
  UINT64                  Value;
  GPU_MEMORY_INFO         *GpuMemInfo;

  if (NULL == ControllerHandle) {
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == GpuNodeParam) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "%a: GPU DSD AML Node generation status {%p, %p}\n", __FUNCTION__, ControllerHandle, GpuNodeParam));

  Status = GetGPUMemoryInfo (ControllerHandle, &GpuMemInfo);
  DEBUG ((DEBUG_INFO, "%a: GPU DSD AML Node generation status {%p, %p}\n", __FUNCTION__, ControllerHandle, &GpuMemInfo));
  ASSERT_EFI_ERROR (Status);

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "GetGPUMemoryInfo"
      " Status = '%r'\n",
      Status
      ));
  }

  DEBUG ((
    DEBUG_ERROR,
    "GetGPUMemoryInfo"
    " Status = '%r'\n",
    Status
    ));

  if (EFI_ERROR (Status)) {
    return EFI_INVALID_PARAMETER;
  }

  DsdNode = NULL;

  // ASL: Name (_DSD, Package () {})
  Status = AmlCodeGenNamePackage ("_DSD", NULL, &DsdNode);

  if (EFI_ERROR (Status)) {
    goto error_handler;
  }

  Status = AmlAddDeviceDataDescriptorPackage (&gDsdDevicePropertyGuid, DsdNode, &PackageNode);
  if (EFI_ERROR (Status)) {
    goto error_handler;
  }

  //
  // Add _DSD Package containing properties
  //

  //  ASL:
  //    Package () {
  //      Package(2) { "nvda_gpu_mem_base_pa", 0x400000000000 },
  //      Package(2) { "nvda_gpu_mem_pxm_start", (16 + ((0) * 8)) },
  //      Package(2) { "nvda_gpu_mem_pxm_count", 8 },
  //      Package(2) { "nvda_gpu_mem_size", 0x10000000 },
  //    }

  //
  // Add individual package properties in the properties package
  //

  //  ASL:
  //      Package(2) { "nvda_gpu_mem_base_pa", 0x400000000000 },

  Name  = GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_BASE_PA].PropertyName;
  Value = GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_BASE_PA].PropertyValue;

  Status = AmlAddNameIntegerPackage (
             Name,
             Value,
             PackageNode
             );
  ASSERT_EFI_ERROR (Status);

  //  ASL:
  //      Package(2) { "nvda_gpu_mem_pxm_start", (16 + ((0) * 8)) },

  Name  = GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_PXM_START].PropertyName;
  Value = GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_PXM_START].PropertyValue;

  Status = AmlAddNameIntegerPackage (
             Name,
             Value,
             PackageNode
             );
  ASSERT_EFI_ERROR (Status);

  //  ASL:
  //      Package(2) { "nvda_gpu_mem_pxm_count", 8 },

  Name  = GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_PXM_COUNT].PropertyName;
  Value = GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_PXM_COUNT].PropertyValue;

  Status = AmlAddNameIntegerPackage (
             Name,
             Value,
             PackageNode
             );
  ASSERT_EFI_ERROR (Status);

  //  ASL:
  //      Package(2) { "nvda_gpu_mem_size", 0x10000000 },

  Name  = GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_SIZE].PropertyName;
  Value = GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_MEM_SIZE].PropertyValue;
  if (Value != 0) {
    Status = AmlAddNameIntegerPackage (
               Name,
               Value,
               PackageNode
               );
    ASSERT_EFI_ERROR (Status);
  }

  if (GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_SIZE].PropertyValue != 0) {
    Name  = GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_BASE_PA].PropertyName;
    Value = GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_BASE_PA].PropertyValue;

    Status = AmlAddNameIntegerPackage (
               Name,
               Value,
               PackageNode
               );
    ASSERT_EFI_ERROR (Status);

    Name  = GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_SIZE].PropertyName;
    Value = GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_SIZE].PropertyValue;

    Status = AmlAddNameIntegerPackage (
               Name,
               Value,
               PackageNode
               );
    ASSERT_EFI_ERROR (Status);

    Name  = GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_PXM].PropertyName;
    Value = GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_PXM].PropertyValue;

    Status = AmlAddNameIntegerPackage (
               Name,
               Value,
               PackageNode
               );
    ASSERT_EFI_ERROR (Status);
  } else {
    // SKIP EGM properties
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "Error creating Gpu DSD AML package properties"
      " Status = '%r'\n",
      Status
      ));
    return Status;
  }

  if (!EFI_ERROR (Status)) {
    if (NULL != GpuNodeParam) {
      *GpuNodeParam = DsdNode;
    }

    return EFI_SUCCESS;
  }

error_handler:

  if (DsdNode != NULL) {
    AmlDeleteTree ((AML_NODE_HANDLE)DsdNode);
  }

  return Status;
}

/**
  Return a pointer to the DSD AML node being generated for the Gpu node

  @param[in]   This             Instance of GPU DSD AML generation protocol
  @param[out]  Node             Returned pointer to the AML DSD Node generated for the Gpu

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_OUT_OF_RESOURCES  There was not enough memory to generate the AML DSD Node.
  @retval EFI_NOT_READY         There is no Configuration Manager available for the Gpu instance.
  @retval EFI_INVALID_PARAMETER This or Node was NULL.
*/
EFI_STATUS
EFIAPI
GpuDsdAmlGenerationGetDsdNode (
  IN NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL  *This,
  OUT AML_NODE_HANDLE                        *Node
  )
{
  EFI_STATUS                                           Status;
  NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL_PRIVATE_DATA  *Private;
  AML_OBJECT_NODE_HANDLE                               GpuDsdAmlNode;

  if (NULL == This) {
    return EFI_INVALID_PARAMETER;
  }

  Private = NVIDIA_GPU_DSD_AML_GENERATION_PRIVATE_DATA_FROM_THIS (This);

  DEBUG ((DEBUG_INFO, "%a: GPU DSD AML Node generation status {%p, %p}\n", __FUNCTION__, Private, Node));

  if (NULL == Private) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  Status = GenerateGpuAmlDsdNode (Private->Handle, &GpuDsdAmlNode);

  DEBUG ((DEBUG_INFO, "%a: GPU DSD AML Node generation status '%r'\n", __FUNCTION__, Status));

  if (NULL != Node) {
    *Node = (AML_NODE_HANDLE)GpuDsdAmlNode;
  }

  return Status;
}

/**
  Return a pointer to the size of the EGM carveout for the socket.

  @param[in]   This             Instance of GPU DSD AML generation protocol
  @param[out]  MemorySize       Returned pointer to the Memory Size
  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_NOT_READY         There is no Configuration Manager available for the Gpu instance.
  @retval EFI_INVALID_PARAMETER This or Node was NULL.
*/
EFI_STATUS
EFIAPI
GpuDsdAmlGenerationGetMemorySize (
  IN NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL  *This,
  OUT UINT64                                 *MemorySize
  )
{
  NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL_PRIVATE_DATA  *Private;

  Private = NVIDIA_GPU_DSD_AML_GENERATION_PRIVATE_DATA_FROM_THIS (This);
  DEBUG ((DEBUG_INFO, "%a: GPU Memory Size status {%p, %p}\n", __FUNCTION__, Private, MemorySize));

  if (NULL == Private) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == MemorySize) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "%a: GPU Memory Size status {0x%016x} Handle [%p]\n", __FUNCTION__, *MemorySize, Private->Handle));

  *MemorySize = GetGPUMemSize (Private->Handle);
  DEBUG ((DEBUG_INFO, "%a: GPU Memory Size = 0x%016x\n", __FUNCTION__, *MemorySize));

  return EFI_SUCCESS;
}

/**
  Return a pointer to the size of the EGM carveout for the socket.

  @param[in]   This             Instance of GPU DSD AML generation protocol
  @param[out]  EgmBasePa        Returned pointer to the EGM Base PA
  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_NOT_READY         There is no Configuration Manager available for the Gpu instance.
  @retval EFI_INVALID_PARAMETER This or Node was NULL.
*/
EFI_STATUS
EFIAPI
GpuDsdAmlGenerationGetEgmBasePa (
  IN NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL  *This,
  OUT UINT64                                 *EgmBasePa
  )
{
  EFI_STATUS                                           Status;
  NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL_PRIVATE_DATA  *Private;
  GPU_MEMORY_INFO                                      *GpuMemInfo;

  Private = NVIDIA_GPU_DSD_AML_GENERATION_PRIVATE_DATA_FROM_THIS (This);
  DEBUG ((DEBUG_INFO, "%a: GPU DSD AML Node generation status {%p, %p}\n", __FUNCTION__, Private, EgmBasePa));

  if (NULL == Private) {
    // ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == EgmBasePa) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "%a: GPU DSD AML Node generation status {0x%016x} Handle [%p]\n", __FUNCTION__, *EgmBasePa, Private->Handle));

  Status = GetGPUMemoryInfo (Private->Handle, &GpuMemInfo);
  DEBUG ((DEBUG_INFO, "%a: GPU Memory Info status '%r'\n", __FUNCTION__, Status));

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "GetGPUMemoryInfo"
      " Status = '%r'\n",
      Status
      ));
  }

  if (EFI_ERROR (Status)) {
    return EFI_INVALID_PARAMETER;
  }

  *EgmBasePa = GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_BASE_PA].PropertyValue;

  return Status;
}

/**
  Return a pointer to the size of the EGM carveout for the socket.

  @param[in]   This             Instance of GPU DSD AML generation protocol
  @param[out]  EgmSize          Returned pointer to the EGM Size
  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_NOT_READY         There is no Configuration Manager available for the Gpu instance.
  @retval EFI_INVALID_PARAMETER This or Node was NULL.
*/
EFI_STATUS
EFIAPI
GpuDsdAmlGenerationGetEgmSize (
  IN NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL  *This,
  OUT UINT64                                 *EgmSize
  )
{
  EFI_STATUS                                           Status;
  NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL_PRIVATE_DATA  *Private;
  GPU_MEMORY_INFO                                      *GpuMemInfo;

  Private = NVIDIA_GPU_DSD_AML_GENERATION_PRIVATE_DATA_FROM_THIS (This);

  if (NULL == Private) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == EgmSize) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  Status = GetGPUMemoryInfo (Private->Handle, &GpuMemInfo);
  DEBUG ((DEBUG_INFO, "%a: GPU Memory Info status '%r'\n", __FUNCTION__, Status));

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "GetGPUMemoryInfo"
      " Status = '%r'\n",
      Status
      ));
  }

  if (EFI_ERROR (Status)) {
    return EFI_INVALID_PARAMETER;
  }

  *EgmSize = GpuMemInfo->Entry[GPU_MEMORY_INFO_PROPERTY_INDEX_EGM_SIZE].PropertyValue;

  return Status;
}

///
/// Install/Uninstall protocol
///

/** Install the GPU DSD AML Generation Protocol on the Controller Handle
    @param[in] Handle       Controller Handle to install the protocol on
    @retval EFI_STATUS      EFI_SUCCESS          - Successfully installed protocol on Handle
                            EFI_OUT_OF_RESOURCES - Protocol memory allocation failed.
                            (pass through OpenProtocol)
                            (pass through InstallMultipleProtocolInterfaces)
**/
EFI_STATUS
EFIAPI
InstallGpuDsdAmlGenerationProtocolInstance (
  IN EFI_HANDLE  Handle
  )
{
  EFI_STATUS                                           Status = EFI_SUCCESS;
  NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL               *GpuDsdAmlGeneration;
  NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL_PRIVATE_DATA  *Private;

  //
  // Only allow a signle GpuDsdAmlGenerationProtocol instance to be installed.
  //

  Status = gBS->OpenProtocol (
                  Handle,
                  &gEfiNVIDIAGpuDSDAMLGenerationProtocolGuid,
                  (VOID **)&GpuDsdAmlGeneration,
                  NULL,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );

  DEBUG_CODE_BEGIN ();
  DEBUG ((DEBUG_ERROR, "%a: Check for previously installed GPU DSD AML Node generation status '%r'\n", __FUNCTION__, Status));
  DEBUG_CODE_END ();
  if (!EFI_ERROR (Status)) {
    return EFI_ALREADY_STARTED;
  }

  //
  // Allocate GPU DSD AML Generation Protocol instance
  //
  Private = AllocateCopyPool (
              sizeof (NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL_PRIVATE_DATA),
              &mPrivateDataTemplate
              );
  DEBUG_CODE_BEGIN ();
  DEBUG ((DEBUG_INFO, "%a: Handle :[%p]\n", __FUNCTION__, Handle));
  DEBUG ((DEBUG_INFO, "%a: GPU DSD AML Node generation Protocol:fn[GpuDsdAmlGenerationGetDsdNode:'%p']\n", __FUNCTION__, GpuDsdAmlGenerationGetDsdNode));
  DEBUG_CODE_END ();
  if (NULL == Private) {
    DEBUG ((DEBUG_ERROR, "ERROR: GPU DSD AML Generation Protocol instance allocation failed.\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto cleanup;
  }

  Private->Handle = Handle;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Private->Handle,
                  &gEfiNVIDIAGpuDSDAMLGenerationProtocolGuid,
                  (VOID **)&Private->GpuDsdAmlGenerationProtocol,
                  NULL
                  );

  DEBUG_CODE_BEGIN ();
  DEBUG ((DEBUG_INFO, "%a: GPU DSD AML Node generation status '%r'\n", __FUNCTION__, Status));
  DEBUG_CODE_END ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR: Protocol install error on Handle [%p]. Status = '%r'.\n", Handle, Status));
    // return Status;
  }

cleanup:
  if (EFI_ERROR (Status)) {
    if (NULL == Private) {
      FreePool (Private);
    }
  }

  return Status;
}

/** Uninstall the GPU DSD AML Generation Protocol from the Controller Handle
    @param[in] Handle       Controller Handle to uninstall the protocol on
    @retval EFI_STATUS      EFI_SUCCESS
                            (pass through OpenProtocol)
                            (pass through UninstallMultipleProtocolInterfaces)
**/
EFI_STATUS
EFIAPI
UninstallGpuDsdAmlGenerationProtocolInstance (
  IN EFI_HANDLE  Handle
  )
{
  // Needs to retrieve protocol for CR private structure access and to release pool on private data allocation.
  EFI_STATUS                                           Status;
  NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL               *GpuDsdAmlGeneration;
  NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL_PRIVATE_DATA  *Private;

  Status = gBS->OpenProtocol (
                  Handle,
                  &gEfiNVIDIAGpuDSDAMLGenerationProtocolGuid,
                  (VOID **)&GpuDsdAmlGeneration,
                  NULL,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR: Protocol open error on Handle [%p]. Status = '%r'.\n", Handle, Status));
    return Status;
  }

  /* Recover Private Data from protocol instance */
  Private = NVIDIA_GPU_DSD_AML_GENERATION_PRIVATE_DATA_FROM_THIS (GpuDsdAmlGeneration);

  Status = gBS->UninstallMultipleProtocolInterfaces (
                  Private->Handle,
                  &gEfiNVIDIAGpuDSDAMLGenerationProtocolGuid,
                  (VOID **)&Private->GpuDsdAmlGenerationProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR: Protocol Uninstall error on Handle[%p]. Status = '%r'.\n", Handle, Status));
    return Status;
  }

  // Free allocation
  FreePool (Private);

  return EFI_SUCCESS;
}
