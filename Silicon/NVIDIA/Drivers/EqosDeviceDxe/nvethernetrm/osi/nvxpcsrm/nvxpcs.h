/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: MIT
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef INCLUDED_NVXPCS_H
#define INCLUDED_NVXPCS_H

#define NV_XPCS_SR_MII_CTRL         0x7C0000
#define XPCS_SR_MII_CTRL_RST        OSI_BIT(15)
#define XPCS_SR_MII_STS_0           0x7C0004
#define XPCS_SR_MII_STS_0_LINK_STS  OSI_BIT(2)

/**
 * @brief nv_osi_readl - Read a memory mapped register.
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
static inline nveu32_t
nv_osi_readl (
  void  *addr
  )
{
  return *(volatile nveu32_t *)addr;
}

/**
 * @brief nv_osi_writel - Write to a memory mapped register.
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
static inline void
nv_osi_writel (
  nveu32_t  val,
  void      *addr
  )
{
  *(volatile nveu32_t *)addr = val;
}

/**
 * @brief nv_xpcs_read - read from xpcs.
 *
 * Algorithm: This routine reads data from XPCS register.
 *
 * @param[in] xpcs_base: XPCS virtual base address
 * @param[in] reg_addr: register address to be read
 *
 * @retval value read from xpcs register.
 */
static inline nveu32_t
nv_xpcs_read (
  void      *xpcs_base,
  nveu32_t  reg_addr
  )
{
  nv_osi_writel (
    ((reg_addr >> XPCS_REG_ADDR_SHIFT) & XPCS_REG_ADDR_MASK),
    ((nveu8_t *)xpcs_base + XPCS_ADDRESS)
    );
  return nv_osi_readl (
           (nveu8_t *)xpcs_base +
           ((reg_addr) & XPCS_REG_VALUE_MASK)
           );
}

#ifndef OSI_STRIPPED_LIB

/**
 * @brief nv_xpcs_write - write to xpcs.
 *
 * Algorithm: This routine writes data to XPCS register.
 *
 * @param[in] xpcs_base: XPCS virtual base address
 * @param[in] reg_addr: register address for writing
 * @param[in] val: write value to register address
 */
static inline void
nv_xpcs_write (
  void      *xpcs_base,
  nveu32_t  reg_addr,
  nveu32_t  val
  )
{
  nv_osi_writel (
    ((reg_addr >> XPCS_REG_ADDR_SHIFT) & XPCS_REG_ADDR_MASK),
    ((nveu8_t *)xpcs_base + XPCS_ADDRESS)
    );
  nv_osi_writel (
    val,
    (nveu8_t *)xpcs_base +
    (((reg_addr) & XPCS_REG_VALUE_MASK))
    );
}

/**
 * @brief nv_xpcs_write_safety - write to xpcs.
 *
 * Algorithm: This routine writes data to XPCS register.
 * And verifiy by reading back the value
 *
 * @param[in] osi_core: OSI core data structure
 * @param[in] reg_addr: register address for writing
 * @param[in] val: write value to register address
 *
 * @retval 0 on success
 * @retval XPCS_WRITE_FAIL_CODE on failure
 *
 */
static inline nve32_t
nv_xpcs_write_safety (
  struct osi_core_priv_data  *osi_core,
  nveu32_t                   reg_addr,
  nveu32_t                   val
  )
{
  void      *xpcs_base = osi_core->xpcs_base;
  nveu32_t  read_val;
  /* 1 busy wait, and the remaining retries are sleeps of granularity MIN_USLEEP_10US */
  nveu32_t  retry = RETRY_ONCE;
  nveu32_t  count = 0;
  nveu32_t  once  = 0U;
  nve32_t   ret   = XPCS_WRITE_FAIL_CODE;
  nve32_t   cond  = COND_NOT_MET;

  while (cond == COND_NOT_MET) {
    nv_xpcs_write (xpcs_base, reg_addr, val);
    read_val = nv_xpcs_read (xpcs_base, reg_addr);
    if (val == read_val) {
      ret  = 0;
      cond = COND_MET;
    } else {
      if (count > retry) {
        break;
      }

      count++;
      if (once == 0U) {
        osi_core->osd_ops.udelay (OSI_DELAY_1US);

        /* udelay is a busy wait, so don't call it too frequently.
         * call it once to be optimistic, and then use usleep with
         * a longer timeout to yield to other CPU users.
         */
        once = 1U;
      } else {
        osi_core->osd_ops.usleep (MIN_USLEEP_10US);
      }
    }
  }

 #ifndef OSI_STRIPPED_LIB
  if (ret != 0) {
    OSI_CORE_ERR (
      osi_core->osd,
      OSI_LOG_ARG_HW_FAIL,
      "xpcs_write_safety failed",
      reg_addr
      );
  }

 #endif /* !OSI_STRIPPED_LIB */
  return ret;
}

#endif

#endif /* INCLUDED_NVXPCS_H */
