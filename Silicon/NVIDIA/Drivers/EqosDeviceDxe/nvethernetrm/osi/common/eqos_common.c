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

#include "eqos_common.h"
#include "../osi/common/common.h"

nveul64_t eqos_get_systime_from_mac(void *addr)
{
	nveul64_t ns1, ns2, ns = 0;
	nveu32_t varmac_stnsr, temp1;
	nveu32_t varmac_stsr;

	varmac_stnsr = osi_readl((nveu8_t *)addr + EQOS_MAC_STNSR);
	temp1 = (varmac_stnsr & EQOS_MAC_STNSR_TSSS_MASK);
	ns1 = (nveul64_t)temp1;

	varmac_stsr = osi_readl((nveu8_t *)addr + EQOS_MAC_STSR);

	varmac_stnsr = osi_readl((nveu8_t *)addr + EQOS_MAC_STNSR);
	temp1 = (varmac_stnsr & EQOS_MAC_STNSR_TSSS_MASK);
	ns2 = (nveul64_t)temp1;

	/* if ns1 is greater than ns2, it means nsec counter rollover
	 * happened. In that case read the updated sec counter again
	 */
	if (ns1 >= ns2) {
		varmac_stsr = osi_readl((nveu8_t *)addr + EQOS_MAC_STSR);
		/* convert sec/high time value to nanosecond */
		if (varmac_stsr < UINT_MAX) {
			ns = ns2 + (varmac_stsr * OSI_NSEC_PER_SEC);
		}
	} else {
		/* convert sec/high time value to nanosecond */
		if (varmac_stsr < UINT_MAX) {
			ns = ns1 + (varmac_stsr * OSI_NSEC_PER_SEC);
		}
	}

	return ns;
}

nveu32_t eqos_is_mac_enabled(void *addr)
{
	nveu32_t enable = OSI_DISABLE;
	nveu32_t reg;

	reg = osi_readl((nveu8_t *)addr + EQOS_MAC_MCR);
	if ((reg & (EQOS_MCR_TE | EQOS_MCR_RE)) ==
		(EQOS_MCR_TE | EQOS_MCR_RE)) {
		enable = OSI_ENABLE;
	}

	return enable;
}
