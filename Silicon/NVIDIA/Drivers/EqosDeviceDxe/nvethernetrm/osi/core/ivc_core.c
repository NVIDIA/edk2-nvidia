// SPDX-License-Identifier: MIT

/* SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <osi_common.h>
#include <osi_core.h>
#include <ivc_core.h>
#include <osi_macsec.h>
#include "eqos_core.h"
#include "eqos_mmc.h"
#include "core_local.h"
#include "common.h"

/**
 * @brief ivc_handle_ioctl - marshell input argument to handle runtime command
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] data: OSI IOCTL data structure.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval data from PHY register on success
 * @retval -1 on failure
 */
static nve32_t
ivc_handle_ioctl (
  struct osi_core_priv_data  *osi_core,
  struct osi_ioctl           *data
  )
{
  nve32_t           ret = 0;
  ivc_msg_common_t  msg;

  osi_memset (&msg, 0, sizeof (msg));

  msg.cmd = handle_ioctl;

  /* osi_memcpy is treated as void since it is
   * an internal functin which will be always success
   */
  (void)osi_memcpy (
          (void *)&msg.data.ioctl_data,
          (void *)data,
          sizeof (struct osi_ioctl)
          );
 #ifndef OSI_STRIPPED_LIB
  if (data->cmd == OSI_CMD_CONFIG_PTP) {
    (void)osi_memcpy (
            (void *)&msg.data.ioctl_data.data.ptp_config,
            (void *)&osi_core->ptp_config,
            sizeof (struct osi_ptp_config)
            );
  }

 #endif
  ret = osi_core->osd_ops.ivc_send (osi_core, &msg, sizeof (msg));

  switch (data->cmd) {
    case OSI_CMD_READ_MMC:
      (void)osi_memcpy (
              (void *)&osi_core->mmc,
              (void *)&msg.data.mmc_s,
              sizeof (struct osi_mmc_counters)
              );
      break;

    case OSI_CMD_READ_STATS:
      (void)osi_memcpy (
              (void *)&osi_core->stats,
              (void *)&msg.data.stats_s,
              sizeof (struct osi_stats)
              );
      break;

    default:
      (void)osi_memcpy (
              (void *)data,
              (void *)&msg.data.ioctl_data,
              sizeof (struct osi_ioctl)
              );
      break;
  }

  return ret;
}

/**
 * @brief ivc_core_init - EQOS MAC, MTL and common DMA Initialization
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
ivc_core_init (
  struct osi_core_priv_data *const  osi_core
  )
{
  ivc_msg_common_t  msg;

  osi_memset (&msg, 0, sizeof (msg));

  msg.cmd = core_init;

  return osi_core->osd_ops.ivc_send (osi_core, &msg, sizeof (msg));
}

/**
 * @brief ivc_core_deinit - EQOS MAC core deinitialization
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note Required clks and resets has to be enabled
 */
static void
ivc_core_deinit (
  struct osi_core_priv_data *const  osi_core
  )
{
  ivc_msg_common_t  msg;
  nve32_t           ret = 0;

  osi_memset (&msg, 0, sizeof (msg));

  msg.cmd = core_deinit;

  ret = osi_core->osd_ops.ivc_send (osi_core, &msg, sizeof (msg));
  if (ret < 0) {
    /* handle Error */
  }
}

/**
 * @brief ivc_write_phy_reg - Write to a PHY register through MAC over MDIO bus
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] phyaddr: PHY address (PHY ID) associated with PHY
 * @param[in] phyreg: Register which needs to be write to PHY.
 * @param[in] phydata: Data to write to a PHY register.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
ivc_write_phy_reg (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    phyaddr,
  const nveu32_t                    phyreg,
  const nveu16_t                    phydata
  )
{
  ivc_msg_common_t  msg;

  osi_memset (&msg, 0, sizeof (msg));

  msg.cmd               = write_phy_reg;
  msg.args.arguments[0] = phyaddr;
  msg.args.arguments[1] = phyreg;
  msg.args.arguments[2] = phydata;
  msg.args.count        = 3;

  return osi_core->osd_ops.ivc_send (osi_core, &msg, sizeof (msg));
}

/**
 * @brief ivc_read_phy_reg - Read from a PHY register through MAC over MDIO bus
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] phyaddr: PHY address (PHY ID) associated with PHY
 * @param[in] phyreg: Register which needs to be read from PHY.
 *
 * @note MAC should be init and started. see osi_start_mac()
 *
 * @retval data from PHY register on success
 * @retval -1 on failure
 */
static nve32_t
ivc_read_phy_reg (
  struct osi_core_priv_data *const  osi_core,
  const nveu32_t                    phyaddr,
  const nveu32_t                    phyreg
  )
{
  ivc_msg_common_t  msg;

  osi_memset (&msg, 0, sizeof (msg));

  msg.cmd               = read_phy_reg;
  msg.args.arguments[0] = phyaddr;
  msg.args.arguments[1] = phyreg;
  msg.args.count        = 2;

  return osi_core->osd_ops.ivc_send (osi_core, &msg, sizeof (msg));
}

#ifdef MACSEC_SUPPORT
  #ifdef DEBUG_MACSEC

/**
 * @brief ivc_macsec_dbg_events_config - Configure Debug events
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] dbg_buf_config: Config Buffer
 *
 * @retval 0 on success
 * @retval -1 on failure.
 */
static nve32_t
ivc_macsec_dbg_events_config (
  struct osi_core_priv_data *const         osi_core,
  struct osi_macsec_dbg_buf_config *const  dbg_buf_config
  )
{
  ivc_msg_common_t  msg;
  nve32_t           ret = 0;

  osi_memset (&msg, 0, sizeof (msg));

  msg.cmd = dbg_events_config_macsec;

  (void)osi_memcpy (
          (void *)&msg.data.dbg_buf_config,
          (void *)dbg_buf_config,
          sizeof (struct osi_macsec_dbg_buf_config)
          );

  ret = osi_core->osd_ops.ivc_send (osi_core, &msg, sizeof (msg));
  if (ret != 0) {
    goto done;
  }

  (void)osi_memcpy (
          (void *)dbg_buf_config,
          (void *)&msg.data.dbg_buf_config,
          sizeof (struct osi_macsec_dbg_buf_config)
          );
done:
  return ret;
}

/**
 * @brief macsec_dbg_buf_config - Read/Write debug buffers.
 *
 * @param[in] osi_core: OSI Core private data structure.
 * @param[in] dbg_buf_config: Pointer to debug buffer config data structure.
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static nve32_t
ivc_macsec_dbg_buf_config (
  struct osi_core_priv_data *const         osi_core,
  struct osi_macsec_dbg_buf_config *const  dbg_buf_config
  )
{
  ivc_msg_common_t  msg;
  nve32_t           ret = 0;

  osi_memset (&msg, 0, sizeof (msg));

  msg.cmd = dbg_buf_config_macsec;

  (void)osi_memcpy (
          (void *)&msg.data.dbg_buf_config,
          (void *)dbg_buf_config,
          sizeof (struct osi_macsec_dbg_buf_config)
          );

  ret = osi_core->osd_ops.ivc_send (osi_core, &msg, sizeof (msg));
  if (ret != 0) {
    goto done;
  }

  (void)osi_memcpy (
          (void *)dbg_buf_config,
          (void *)&msg.data.dbg_buf_config,
          sizeof (struct osi_macsec_dbg_buf_config)
          );
done:
  return ret;
}

  #endif /* DEBUG_MACSEC */

/**
 * @brief macsec_read_mmc - To read statitics registers and update structure
 *                          variable
 *
 * Algorithm: Pass register offset and old value to helper function and
 *            update structure.
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note
 * 1) MAC/MACSEC should be init and started.
 */
static void
ivc_macsec_read_mmc (
  struct osi_core_priv_data *const  osi_core
  )
{
  ivc_msg_common_t  msg;

  osi_memset (&msg, 0, sizeof (msg));

  msg.cmd = read_mmc_macsec;

  msg.status = osi_core->osd_ops.ivc_send (osi_core, &msg, sizeof (msg));

  (void)osi_memcpy (
          (void *)&osi_core->macsec_mmc,
          (void *)&msg.data.macsec_mmc,
          sizeof (struct osi_macsec_mmc_counters)
          );
  (void)osi_memcpy (
          (void *)&osi_core->macsec_irq_stats,
          (void *)&msg.data.macsec_irq_stats,
          sizeof (struct osi_macsec_irq_stats)
          );
}

/**
 * @brief ivc_get_sc_lut_key_index - Macsec get Key_index
 *
 * @param[in] osi_core: OSI Core private data structure.
 * @param[in] sci: Secure Channel info.
 * @param[out] key_index: Key table index to program SAK.
 * @param[in] ctlr: Controller instance.
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static nve32_t
ivc_get_sc_lut_key_index (
  struct osi_core_priv_data *const  osi_core,
  nveu8_t                           *sci,
  nveu32_t                          *key_index,
  nveu16_t                          ctlr
  )
{
  ivc_msg_common_t  msg;
  nve32_t           ret = 0;

  osi_memset (&msg, 0, sizeof (msg));

  msg.cmd = macsec_get_sc_lut_key_index;
  (void)osi_memcpy (
          (void *)&msg.data.macsec_cfg.sci,
          (void *)sci,
          OSI_SCI_LEN
          );
  msg.data.macsec_cfg.ctlr = ctlr;

  ret = osi_core->osd_ops.ivc_send (osi_core, &msg, sizeof (msg));
  if (ret == 0) {
    *key_index = msg.data.macsec_cfg.key_index;
  }

  return ret;
}

/**
 * @brief ivc_macsec_config - Mac sec config.
 *
 * @param[in] osi_core: OSI Core private data structure.
 * @param[in] sc: Secure Channel info.
 * @param[in] enable: enable or disable.
 * @param[in] ctlr: Controller instance.
 * @param[out] kt_idx: Key table index to program SAK.
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static nve32_t
ivc_macsec_config (
  struct osi_core_priv_data *const  osi_core,
  struct osi_macsec_sc_info *const  sc,
  nveu32_t                          enable,
  nveu16_t                          ctlr,
  nveu16_t                          *kt_idx
  )
{
  ivc_msg_common_t  msg;
  nve32_t           ret = 0;

  osi_memset (&msg, 0, sizeof (msg));

  msg.cmd = config_macsec;
  (void)osi_memcpy (
          (void *)&msg.data.macsec_cfg.sc_info,
          (void *)sc,
          sizeof (struct osi_macsec_sc_info)
          );
  msg.data.macsec_cfg.enable = enable;
  msg.data.macsec_cfg.ctlr   = ctlr;
  msg.data.macsec_cfg.kt_idx = *kt_idx;

  ret = osi_core->osd_ops.ivc_send (osi_core, &msg, sizeof (msg));
  if (ret != 0) {
    goto done;
  }

  *kt_idx = msg.data.macsec_cfg.kt_idx;
done:
  return ret;
}

  #ifdef DEBUG_MACSEC

/**
 * @brief ivc_macsec_loopback_config - Loopback configure.
 *
 * @param[in] osi_core: OSI Core private data structure.
 * @param[in] enable: Enable or disable loopback.
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static nve32_t
ivc_macsec_loopback_config (
  struct osi_core_priv_data *const  osi_core,
  nveu32_t                          enable
  )
{
  ivc_msg_common_t  msg;
  nveu32_t          index = 0;

  osi_memset (&msg, 0, sizeof (msg));

  msg.cmd                   = loopback_config_macsec;
  msg.args.arguments[index] = enable;
  index++;
  msg.args.count = index;

  return osi_core->osd_ops.ivc_send (osi_core, &msg, sizeof (msg));
}

  #endif /* DEBUG_MACSEC */

  #ifdef MACSEC_KEY_PROGRAM

/**
 * @brief ivc_macsec_kt_config - MacSec KT configure.
 *
 * @param[in] osi_core: OSI Core private data structure.
 * @param[in] kt_config: KT config structure.
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static nve32_t
ivc_macsec_kt_config (
  struct osi_core_priv_data *const    osi_core,
  struct osi_macsec_kt_config *const  kt_config
  )
{
  ivc_msg_common_t  msg;
  nve32_t           ret = 0;

  osi_memset (&msg, 0, sizeof (msg));

  msg.cmd = kt_config_macsec;
  (void)osi_memcpy (
          (void *)&msg.data.kt_config,
          (void *)kt_config,
          sizeof (struct osi_macsec_kt_config)
          );

  ret = osi_core->osd_ops.ivc_send (osi_core, &msg, sizeof (msg));
  if (ret != 0) {
    return ret;
  }

  (void)osi_memcpy (
          (void *)kt_config,
          (void *)&msg.data.kt_config,
          sizeof (struct osi_macsec_kt_config)
          );
  return ret;
}

  #endif /* MACSEC_KEY_PROGRAM */

/**
 * @brief ivc_macsec_cipher_config - cipher configure.
 *
 * @param[in] osi_core: OSI Core private data structure.
 * @param[in] cipher: value of cipher.
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static nve32_t
ivc_macsec_cipher_config (
  struct osi_core_priv_data *const  osi_core,
  nveu32_t                          cipher
  )
{
  ivc_msg_common_t  msg;
  nveu32_t          index = 0;

  osi_memset (&msg, 0, sizeof (msg));

  msg.cmd                   = cipher_config;
  msg.args.arguments[index] = cipher;
  index++;
  msg.args.count = index;

  return osi_core->osd_ops.ivc_send (osi_core, &msg, sizeof (msg));
}

/**
 * @brief ivc_macsec_lut_config - LUT config.
 *
 * @param[in] osi_core: OSI Core private data structure.
 * @param[in] lut_config: lut data structure.
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static nve32_t
ivc_macsec_lut_config (
  struct osi_core_priv_data *const     osi_core,
  struct osi_macsec_lut_config *const  lut_config
  )
{
  ivc_msg_common_t  msg;
  nve32_t           ret = 0;

  osi_memset (&msg, 0, sizeof (msg));

  msg.cmd = lut_config_macsec;
  (void)osi_memcpy (
          (void *)&msg.data.lut_config,
          (void *)lut_config,
          sizeof (struct osi_macsec_lut_config)
          );

  ret = osi_core->osd_ops.ivc_send (osi_core, &msg, sizeof (msg));
  if (ret != 0) {
    goto done;
  }

  (void)osi_memcpy (
          (void *)lut_config,
          (void *)&msg.data.lut_config,
          sizeof (struct osi_macsec_lut_config)
          );
done:
  return ret;
}

/**
 * @brief ivc_macsec_handle_irq - handle macsec irq.
 *
 * @param[in] osi_core: OSI Core private data structure.
 *
 */
static void
ivc_macsec_handle_irq (
  OSI_UNUSED
  struct osi_core_priv_data *const  osi_core
  )
{
  OSI_CORE_INFO (
    (osi_core->osd),
    (OSI_LOG_ARG_INVALID),
    ("Nothing to handle\n"),
    (0ULL)
    );
}

/**
 * @brief ivc_macsec_deinit - De Initialize.
 *
 * @param[in] osi_core: OSI Core private data structure.
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static nve32_t
ivc_macsec_deinit (
  struct osi_core_priv_data *const  osi_core
  )
{
  ivc_msg_common_t  msg;

  osi_memset (&msg, 0, sizeof (msg));

  msg.cmd = deinit_macsec;

  return osi_core->osd_ops.ivc_send (osi_core, &msg, sizeof (msg));
}

/**
 * @brief ivc_macsec_init -Initialize.
 *
 * @param[in] osi_core: OSI Core private data structure.
 * @param[in] mtu: mtu to be set.
 * @param[in] mac_addr: device mac address.
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static nve32_t
ivc_macsec_init (
  struct osi_core_priv_data *const  osi_core,
  nveu32_t                          mtu,
  nveu8_t *const                    mac_addr
  )
{
  ivc_msg_common_t  msg;
  nveu32_t          index = 0;

  osi_memset (&msg, 0, sizeof (msg));

  msg.cmd                   = init_macsec;
  msg.args.arguments[index] = mtu;
  index++;
  msg.args.count = index;
  (void)osi_memcpy (
          (void *)&msg.data.macsec_mac_addr,
          (void *)mac_addr,
          OSI_ETH_ALEN
          );

  return osi_core->osd_ops.ivc_send (osi_core, &msg, sizeof (msg));
}

/**
 * @brief ivc_init_macsec_ops - Initialize IVC core operations.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: No
 * - De-initialization: No
 */
void
ivc_init_macsec_ops (
  void  *macsecops
  )
{
  struct osi_macsec_core_ops  *ops = (struct osi_macsec_core_ops *)macsecops;

  ops->init       = ivc_macsec_init;
  ops->deinit     = ivc_macsec_deinit;
  ops->handle_irq = ivc_macsec_handle_irq;
  ops->lut_config = ivc_macsec_lut_config;
 #ifdef MACSEC_KEY_PROGRAM
  ops->kt_config = ivc_macsec_kt_config;
 #endif /* MACSEC_KEY_PROGRAM */
  ops->cipher_config = ivc_macsec_cipher_config;
  ops->config        = ivc_macsec_config;
  ops->read_mmc      = ivc_macsec_read_mmc;
 #ifdef DEBUG_MACSEC
  ops->loopback_config   = ivc_macsec_loopback_config;
  ops->dbg_events_config = ivc_macsec_dbg_events_config;
  ops->dbg_buf_config    = ivc_macsec_dbg_buf_config;
 #endif /* DEBUG_MACSEC */
  ops->get_sc_lut_key_index = ivc_get_sc_lut_key_index;
}

#endif

/**
 * @brief vir_ivc_core_deinit - MAC core deinitialization
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @note Required clks and resets has to be enabled
 *
 * @retval Return 0
 */
static nve32_t
vir_ivc_core_deinit (
  struct osi_core_priv_data *const  osi_core
  )
{
  ivc_core_deinit (osi_core);
  return 0;
}

/**
 * @brief vir_init_core_ops - core ops initialization
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @retval Return 0
 */
static nve32_t
vir_ivc_init_core_ops (
  OSI_UNUSED
  struct osi_core_priv_data *const  osi_core
  )
{
  /* This API should not do anything as ethernet_server maintain ops
   * locally
   */
  (void)osi_core;       // unused
  return 0;
}

void
ivc_interface_init_core_ops (
  struct if_core_ops  *if_ops_p
  )
{
  if_ops_p->if_core_init     = ivc_core_init;
  if_ops_p->if_core_deinit   = vir_ivc_core_deinit;
  if_ops_p->if_write_phy_reg = ivc_write_phy_reg;
  if_ops_p->if_read_phy_reg  = ivc_read_phy_reg;
  if_ops_p->if_init_core_ops = vir_ivc_init_core_ops;
  if_ops_p->if_handle_ioctl  = ivc_handle_ioctl;
}
