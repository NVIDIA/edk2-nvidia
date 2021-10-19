/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef INCLUDED_MGBE_COMMON_H
#define INCLUDED_MGBE_COMMON_H

/**
 * @addtogroup MGBE-MAC MGBE MAC common HW feature registers
 *
 * @{
 */
#define MGBE_MAC_STSR			0x0D08
#define MGBE_MAC_STNSR			0x0D0C
#define MGBE_MAC_STNSR_TSSS_MASK	0x7FFFFFFFU

#define MGBE_MAC_TX			0x0000
#define MGBE_MCR_TE			OSI_BIT(0)
/** @} */


nveul64_t mgbe_get_systime_from_mac(void *addr);
nveu32_t mgbe_is_mac_enabled(void *addr);

#endif /* INCLUDED_MGBE_COMMON_H */
