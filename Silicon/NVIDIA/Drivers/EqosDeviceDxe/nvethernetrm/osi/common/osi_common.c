/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
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

#include "eqos_common.h"
#include "mgbe_common.h"
#include "../osi/common/common.h"

void common_get_systime_from_mac(void *addr, nveu32_t mac, nveu32_t *sec,
				 nveu32_t *nsec)
{
	nveu64_t temp;
	nveu64_t remain;
	nveul64_t ns;
	typedef nveul64_t (*get_time)(void *addr);
	get_time i_ops[MAX_MAC_IP_TYPES] = {
		eqos_get_systime_from_mac, mgbe_get_systime_from_mac
	};

	ns = i_ops[mac](addr);

	temp = div_u64_rem((nveu64_t)ns, OSI_NSEC_PER_SEC, &remain);
	if (temp < UINT_MAX) {
		*sec = (nveu32_t)temp;
	} else {
		/* do nothing here */
	}
	if (remain < UINT_MAX) {
		*nsec = (nveu32_t)remain;
	} else {
		/* do nothing here */
	}
}

nveu32_t common_is_mac_enabled(void *addr, nveu32_t mac)
{
	typedef nveu32_t (*mac_enable_arr)(void *addr);
	mac_enable_arr i_ops[MAX_MAC_IP_TYPES] = {
		eqos_is_mac_enabled, mgbe_is_mac_enabled
	};

	return i_ops[mac](addr);
}

nveu64_t div_u64_rem(nveu64_t dividend, nveu64_t divisor,
		     nveu64_t *remain)
{
	nveu64_t ret = 0;

	if (divisor != 0U) {
		*remain = dividend % divisor;
		ret = dividend / divisor;
	} else {
		ret = 0;
	}

	return ret;
}
