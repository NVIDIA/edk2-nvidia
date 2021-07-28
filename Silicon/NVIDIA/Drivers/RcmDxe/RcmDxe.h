/** @file
*  Rcm Dxe
*
*  Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
*  Portions provided under the following terms:
*  Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2021 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
*
**/

#ifndef __RCMDXE_H__
#define __RCMDXE_H__


#define MAX_BLOB_INFO     64
#define IMAGE_TYPE_KERNEL 45


typedef struct {
  UINT32 ImgType;
  UINT32 Offset;
  UINT32 LoadAddress;
  UINT32 Size;
} TEGRABL_BLOBINFO;


typedef struct {
  UINT8            BlobMagic[4];
  UINT8            Padding[4];
  UINT8            Digest[64];
  UINT8            Salt[4];
  UINT32           BlobEntries;
  TEGRABL_BLOBINFO BlobInfo[MAX_BLOB_INFO];
} TEGRABL_BLOBHEADER;


#endif // __RCMDXE_H__
