/** @file
*  Rcm Dxe
*
*  SPDX-FileCopyrightText: Copyright (c) 2020-2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __RCMDXE_H__
#define __RCMDXE_H__

#define MAX_BLOB_INFO           64
#define IMAGE_TYPE_KERNEL       45
#define T194_IMAGE_TYPE_KERNEL  37

typedef struct {
  UINT32    ImgType;
  UINT32    Offset;
  UINT32    LoadAddress;
  UINT32    Size;
} TEGRABL_BLOBINFO;

typedef struct {
  UINT8               BlobMagic[4];
  UINT8               Padding[4];
  UINT8               Digest[64];
  UINT8               Salt[4];
  UINT32              BlobEntries;
  TEGRABL_BLOBINFO    BlobInfo[MAX_BLOB_INFO];
} TEGRABL_BLOBHEADER;

typedef struct {
  UINT8               BlobMagic[4];
  UINT32              BlobEntries;
  TEGRABL_BLOBINFO    BlobInfo[MAX_BLOB_INFO];
} T194_TEGRABL_BLOBHEADER;

#endif // __RCMDXE_H__
