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
#ifndef INCLUDED_COMMON_H
#define INCLUDED_COMMON_H

#include "../osi/common/type.h"
#include <osi_common.h>

/**
 * @addtogroup Generic helper macros
 *
 * @brief These are Generic helper macros used at various places.
 * @{
 */
#define RETRY_COUNT	1000U
#define COND_MET	0
#define COND_NOT_MET	1
/** @} */


/**
 * @brief Maximum number of supported MAC IP types (EQOS and MGBE)
 */
#define MAX_MAC_IP_TYPES       2U

/**
 * @brief osi_readl_poll_timeout - Periodically poll an address until
 * a condition is met or a timeout occurs
 *
 * @param[in] addr: Memory mapped address.
 * @param[in] val: Variable to read the value.
 * @param[in] cond: Break condition (usually involving @val).
 * @param[in] delay_us: Maximum time to sleep between reads in us.
 * @param[in] retry: Retry count.

 * @note Physical address has to be memmory mapped.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
#define osi_readl_poll_timeout(addr, fn, val, cond, delay_us, retry) \
({ \
	unsigned int count = 0; \
	while (count++ < retry) { \
		val = osi_readl((unsigned char *)addr); \
		if ((cond)) { \
			break; \
		} \
		fn(delay_us); \
	} \
	(cond) ? 0 : -1; \
})

struct osi_core_priv_data;

/**
 * @brief osi_lock_init - Initialize lock to unlocked state.
 *
 * @note
 * Algorithm:
 *  - Set lock to unlocked state.
 *
 * @param[in] lock - Pointer to lock to be initialized
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
static inline void osi_lock_init(nveu32_t *lock)
{
	*lock = OSI_UNLOCKED;
}

/**
 * @brief osi_lock_irq_enabled - Spin lock. Busy loop till lock is acquired.
 *
 * @note
 * Algorithm:
 *  - Atomic compare and swap operation till lock is held.
 *
 * @param[in] lock - Pointer to lock to be acquired.
 *
 * @note
 *  - Does not disable irq. Do not call this API to acquire any
 *    lock that is shared between top/bottom half. It will result in deadlock.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void osi_lock_irq_enabled(nveu32_t *lock)
{
	/* __sync_val_compare_and_swap(lock, old value, new value) returns the
	 * old value if successful.
	 */
	while (__sync_val_compare_and_swap(lock, OSI_UNLOCKED, OSI_LOCKED) !=
	      OSI_UNLOCKED) {
		/* Spinning.
		 * Will deadlock if any ISR tried to lock again.
		 */
	}
}

/**
 * @brief osi_unlock_irq_enabled - Release lock.
 *
 * @note
 * Algorithm:
 *  - Atomic compare and swap operation to release lock.
 *
 * @param[in] lock - Pointer to lock to be released.
 *
 * @note
 *  - Does not disable irq. Do not call this API to release any
 *    lock that is shared between top/bottom half.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void osi_unlock_irq_enabled(nveu32_t *lock)
{
	if (__sync_val_compare_and_swap(lock, OSI_LOCKED, OSI_UNLOCKED) !=
	    OSI_LOCKED) {
		/* Do nothing. Already unlocked */
	}
}

/**
 * @brief osi_readl - Read a memory mapped register.
 *
 * @param[in] addr: Memory mapped address.
 *
 * @pre Physical address has to be memory mapped.
 *
 * @return Data from memory mapped register - success.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 */
static inline nveu32_t osi_readl(void *addr)
{
	return *(volatile nveu32_t *)addr;
}

/**
 * @brief osi_writel - Write to a memory mapped register.
 *
 * @param[in] val:  Value to be written.
 * @param[in] addr: Memory mapped address.
 *
 * @pre Physical address has to be memory mapped.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 */
static inline void osi_writel(nveu32_t val, void *addr)
{
	*(volatile nveu32_t *)addr = val;
}

#ifdef ETHERNET_SERVER
nveu32_t osi_readla(void *priv, void *addr);

void osi_writela(void *priv, nveu32_t val, void *addr);
#else
/**
 * @brief osi_readla - Read a memory mapped register.
 *
 * @ note
 * The difference between osi_readla & osi_readl is osi_core argument.
 * In case of ethernet server, osi_core used to define policy for each VM.
 * In case of non virtualization osi_core argument is ignored.
 *
 * @param[in] priv: Priv address.
 * @param[in] addr: Memory mapped address.
 *
 * @note Physical address has to be memmory mapped.
 *
 * @return Data from memory mapped register - success.
 */
static inline nveu32_t osi_readla(void *priv, void *addr)
{
	return *(volatile nveu32_t *)addr;
}

/**
 *
 * @ note
 * @brief osi_writela - Write to a memory mapped register.
 * The difference between osi_writela & osi_writel is osi_core argument.
 * In case of ethernet server, osi_core used to define policy for each VM.
 * In case of non virtualization osi_core argument is ignored.
 *
 * @param[in] priv: Priv address.
 * @param[in] val:  Value to be written.
 * @param[in] addr: Memory mapped address.
 *
 * @note Physical address has to be memmory mapped.
 */
static inline void osi_writela(void *priv, nveu32_t val, void *addr)
{
	*(volatile nveu32_t *)addr = val;
}
#endif

/**
 * @brief validate_mac_ver_update_chans - Validates mac version and update chan
 *
 * @param[in] mac_ver: MAC version read.
 * @param[out] max_chans: Maximum channel number.
 *
 * @note MAC has to be out of reset.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 *
 * @retval 0 - for not Valid MAC
 * @retval 1 - for Valid MAC
 */
static inline nve32_t validate_mac_ver_update_chans(nveu32_t mac_ver,
						    nveu32_t *max_chans)
{
	switch (mac_ver) {
	case OSI_EQOS_MAC_4_10:
	case OSI_EQOS_MAC_5_00:
		*max_chans = OSI_EQOS_XP_MAX_CHANS;
		break;
	case OSI_EQOS_MAC_5_30:
		*max_chans = OSI_EQOS_MAX_NUM_CHANS;
		break;
	case OSI_MGBE_MAC_3_00:
	case OSI_MGBE_MAC_3_10:
	case OSI_MGBE_MAC_4_00:
		*max_chans = OSI_MGBE_MAX_NUM_CHANS;
		break;
	default:
		return 0;
	}

	return 1;
}

/**
 * @brief osi_memset - osi memset
 *
 * @param[out] s: source that need to be set
 * @param[in] c: value to fill in source
 * @param[in] count: first n bytes of source
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void osi_memset(void *s, nveu32_t c, nveu64_t count)
{
	nveu8_t *xs = OSI_NULL;
	nveu64_t temp = count;

	if (s == OSI_NULL) {
		return;
	}
	xs = (nveu8_t *)s;
	while (temp != 0UL) {
		if (c < OSI_UCHAR_MAX) {
			*xs = (nveu8_t)c;
			xs++;
		}
		temp--;
	}
}

/**
 * @brief osi_memcpy - osi memcpy
 *
 * @param[out] dest: destination pointer
 * @param[in] src: source pointer
 * @param[in] n: number bytes of source
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline nve32_t osi_memcpy(void *dest, void *src, nveu64_t n)
{
	nve8_t *csrc = (nve8_t *)src;
	nve8_t *cdest = (nve8_t *)dest;
	nveu64_t i = 0;

	if (src == OSI_NULL || dest == OSI_NULL) {
		return -1;
	}
	for (i = 0; i < n; i++) {
		cdest[i] = csrc[i];
	}

	return 0;
}

static inline nve32_t osi_memcmp(void *dest, void *src, nve32_t n)
{
	nve32_t i;
	nve8_t *csrc = (nve8_t *)src;
	nve8_t *cdest = (nve8_t *)dest;

	if (src == OSI_NULL || dest == OSI_NULL)
		return -1;

	for (i = 0; i < n; i++) {
		if (csrc[i] < cdest[i]) {
			return -1;
		} else if (csrc[i] > cdest[i]) {
			return 1;
		}
	}
	return 0;
}
#endif
