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

#ifndef LOCAL_COMMON_H
#define LOCAL_COMMON_H

#include <osi_common.h>

/**
 * @brief TX timestamp helper MACROS
 * @{
 */
#define CHAN_START_POSITION 6U
#define PKT_ID_CNT	((nveu32_t)1 << CHAN_START_POSITION)
/* First 6 bytes of idx and last 4 bytes of chan(+1 to avoid pkt_id to be 0) */
#define GET_TX_TS_PKTID(idx, c) (((++(idx)) & (PKT_ID_CNT - 1U)) | \
				 (((c) + 1U) << CHAN_START_POSITION))
/** @} */

/**
 *@brief div_u64_rem - updates remainder and returns Quotient
 *
 * @note
 * Algorithm:
 *  - Dividend will be divided by divisor and stores the
 *    remainder value and returns quotient
 *
 * @param[in] dividend: Dividend value
 * @param[in] divisor: Divisor value
 * @param[out] remain: Remainder
 *
 * @pre MAC IP should be out of reset and need to be initialized as the
 *      requirements
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval Quotient
 */
nveu64_t div_u64_rem(nveu64_t dividend, nveu64_t divisor,
		     nveu64_t *remain);

/**
 * @brief common_get_systime_from_mac - Get system time
 *
 * @param[in] addr: Address of base register.
 * @param[in] mac: MAC HW type.
 * @param[out] sec: Value read in Seconds.
 * @param[out] nsec: Value read in Nano seconds.
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
void common_get_systime_from_mac(void *addr, nveu32_t mac,
				 nveu32_t *sec, nveu32_t *nsec);

/**
 * @brief common_is_mac_enabled - Checks if MAC is enabled or not.
 *
 * @param[in] addr: Address of base register.
 * @param[in] mac: MAC HW type.
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
nveu32_t common_is_mac_enabled(void *addr, nveu32_t mac);
#endif /* LOCAL_COMMON_H */
