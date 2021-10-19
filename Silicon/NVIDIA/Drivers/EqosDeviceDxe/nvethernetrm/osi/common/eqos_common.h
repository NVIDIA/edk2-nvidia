/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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

#ifndef INCLUDED_EQOS_COMMON_H
#define INCLUDED_EQOS_COMMON_H

#include <local_common.h>

/**
 * @brief PTP Time read registers
 * @{
 */
#define EQOS_MAC_STSR			0x0B08
#define EQOS_MAC_STNSR			0x0B0C
#define EQOS_MAC_STNSR_TSSS_MASK	0x7FFFFFFFU
/** @} */

/**
 * @brief Common MAC MCR register and its bits
 * @{
 */
#define EQOS_MAC_MCR			0x0000
#define EQOS_MCR_TE			OSI_BIT(0)
#define EQOS_MCR_RE			OSI_BIT(1)
/** @} */

/**
 * @brief eqos_get_systime - Get system time from MAC
 *
 * @param[in] addr: Base address indicating the start of
 *                  memory mapped IO region of the MAC.
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
nveul64_t eqos_get_systime_from_mac(void *addr);

/**
 * @brief eqos_is_mac_enabled - Checks if MAC is enabled or not.
 *
 * @param[in] addr: Base address indicating the start of
 *                  memory mapped IO region of the MAC.
 *
 * @pre MAC should be init and started. see osi_start_mac()
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 *
 * @retval OSI_ENABLE if MAC enabled.
 * @retval OSI_DISABLE otherwise.
 */
nveu32_t eqos_is_mac_enabled(void *addr);
#endif /* INCLUDED_EQOS_COMMON_H */
