/** @file
*
*  Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _STMMHAF_H_
#define _STMMHAF_H_

#include <Uefi.h>

/* Request the PA of the STMM_FW NS shared buffer */
#define STMM_GET_NS_BUFFER  0xC0270001

typedef struct {
  PHYSICAL_ADDRESS    NsBufferAddr;
  UINTN               NsBufferSize;
  PHYSICAL_ADDRESS    SecBufferAddr;
  UINTN               SecBufferSize;
  PHYSICAL_ADDRESS    DTBAddress;
} STMM_COMM_BUFFERS;

#endif //_STMMHAF_H_
