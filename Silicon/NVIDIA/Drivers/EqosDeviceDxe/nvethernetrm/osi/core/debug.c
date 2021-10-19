/*
 * Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
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

#ifdef OSI_DEBUG
#include "debug.h"

/**
 * @brief core_dump_struct - Dumps a given structure.
 *
 * @param[in] osi_core: OSI DMA private data structure.
 * @param[in] ptr: Pointer to structure.
 * @param[in] size: Size of structure to dump.
 *
 */
static void core_dump_struct(struct osi_core_priv_data *osi_core,
			     unsigned char *ptr,
			     unsigned long size)
{
	nveu32_t i = 0, rem, j;
	unsigned long temp;

	if (ptr == OSI_NULL) {
		osi_core->osd_ops.printf(osi_core, OSI_DEBUG_TYPE_STRUCTS,
					 "pointer is NULL\n");
		return;
	}

	rem = i % 4;
	temp = size - rem;

	for (i = 0; i < temp; i += 4) {
		osi_core->osd_ops.printf(osi_core, OSI_DEBUG_TYPE_STRUCTS,
					 "%02x%02x%02x%02x", ptr[i], ptr[i + 1],
					 ptr[i + 2], ptr[i + 3]);
		j = i;
	}

	for (i = j; i < size; i++) {
		osi_core->osd_ops.printf(osi_core, OSI_DEBUG_TYPE_STRUCTS, "%x",
					ptr[i]);
	}
}

/**
 * @brief core_structs_dump - Dumps OSI CORE structure.
 *
 * @param[in] osi_core: OSI CORE private data structure.
 */
void core_structs_dump(struct osi_core_priv_data *osi_core)
{
	struct core_local *l_core = (struct core_local *)osi_core;

	osi_core->osd_ops.printf(osi_core, OSI_DEBUG_TYPE_STRUCTS,
				 "CORE struct size = %lu",
				 sizeof(struct osi_core_priv_data));
	core_dump_struct(osi_core, (unsigned char *)osi_core,
			 sizeof(struct osi_core_priv_data));
#ifdef MACSEC_SUPPORT
	osi_core->osd_ops.printf(osi_core, OSI_DEBUG_TYPE_STRUCTS,
				 "MACSEC ops size = %lu",
				 sizeof(struct osi_macsec_core_ops));
	core_dump_struct(osi_core, (unsigned char *)osi_core->macsec_ops,
			 sizeof(struct osi_macsec_core_ops));

	osi_core->osd_ops.printf(osi_core, OSI_DEBUG_TYPE_STRUCTS,
				 "MACSEC LUT status size = %lu",
				 sizeof(struct osi_macsec_lut_status));
	core_dump_struct(osi_core, (unsigned char *)osi_core->macsec_ops,
			 sizeof(struct osi_macsec_lut_status));
#endif
	osi_core->osd_ops.printf(osi_core, OSI_DEBUG_TYPE_STRUCTS,
				 "HW features size = %lu",
				 sizeof(struct osi_hw_features));
	core_dump_struct(osi_core, (unsigned char *)osi_core->hw_feature,
			 sizeof(struct osi_hw_features));
	osi_core->osd_ops.printf(osi_core, OSI_DEBUG_TYPE_STRUCTS,
				 "core local size = %lu",
				 sizeof(struct core_local));
	core_dump_struct(osi_core, (unsigned char *)l_core,
			 sizeof(struct core_local));
	osi_core->osd_ops.printf(osi_core, OSI_DEBUG_TYPE_STRUCTS,
				 "core ops size = %lu",
				 sizeof(struct core_ops));
	core_dump_struct(osi_core, (unsigned char *)l_core->ops_p,
			 sizeof(struct core_ops));
	osi_core->osd_ops.printf(osi_core, OSI_DEBUG_TYPE_STRUCTS,
				 "if_ops_p struct size = %lu",
				 sizeof(struct if_core_ops));
	core_dump_struct(osi_core, (unsigned char *)l_core->if_ops_p,
			 sizeof(struct if_core_ops));
}

/**
 * @brief reg_dump - Dumps MAC DMA registers
 *
 * @param[in] osi_core: OSI core private data structure.
 */
void core_reg_dump(struct osi_core_priv_data *osi_core)
{
	unsigned int max_addr;
	unsigned int addr = 0x0;
	unsigned int reg_val;

	switch (osi_core->mac_ver) {
	case OSI_EQOS_MAC_5_00:
		max_addr = 0x12E4;
		break;
	case OSI_EQOS_MAC_5_30:
		max_addr = 0x14EC;
		break;
	case OSI_MGBE_MAC_3_10:
		max_addr = 0x35FC;
		break;
	default:
		return;
	}

	while (1) {
		if (addr > max_addr)
			break;

		reg_val = osi_readla(osi_core,
				     (nveu8_t *)osi_core->base + addr);
		osi_core->osd_ops.printf(osi_core, OSI_DEBUG_TYPE_REG,
					 "%x: %x\n", addr, reg_val);
		addr += 4;
	}
}
#endif
