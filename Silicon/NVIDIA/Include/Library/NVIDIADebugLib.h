/** @file
*
*  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __NV_DEBUG_LIB_H__
#define __NV_DEBUG_LIB_H__

#include <Library/DebugLib.h>

/**
  Alternative to ASSERT() that includes a message and a return value.

  If MDEPKG_NDEBUG is defined, this is an empty macro.

  If the expression is TRUE, no action is taken.

  If the expression is FALSE and DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED bit of
  PcdDebugProperyMask is set, calls DEBUG(Message), then calls DebugAssert().
  If the bit is not set, then the Action is performed instead.

  @param  Expression  Boolean expression.
  @param  Action      Action to take if, if
                      DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED is not set.
  @param  Msg...      Format and arguments of the DEBUG Message to write, if
                      DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED is set.

**/
#if !defined (MDEPKG_NDEBUG)
#define NV_ASSERT_RETURN(Expression, Action, Msg ...)  \
    do {                                            \
      if (DebugAssertEnabled ()) {                  \
        if (!(Expression)) {                        \
          DEBUG((DEBUG_ERROR, "ERROR " Msg));       \
          _ASSERT (Expression);                     \
          ANALYZER_UNREACHABLE ();                  \
        }                                           \
      } else {                                      \
        if (!(Expression)) { Action; }              \
      }                                             \
    } while (FALSE)
#else
#define NV_ASSERT_RETURN(Expression, Action, Msg ...)
#endif

/**
  Alternative to ASSERT_EFI_ERROR() that also returns the status code.

  Same behavior as ASSERT_EFI_ERROR(), except that if the
  DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED bit of PcdDebugProperyMask is not set, it
  returns StatusParamter.

  @param  StatusParameter  EFI_STATUS value to evaluate.
  @param  Action      Action to take if, if
                      DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED is not set.

**/
#if !defined (MDEPKG_NDEBUG)
#define NV_ASSERT_EFI_ERROR_RETURN(StatusParameter, Action)                                 \
    do {                                                                                 \
      if (DebugAssertEnabled ()) {                                                       \
        if (EFI_ERROR (StatusParameter)) {                                               \
          DEBUG ((DEBUG_ERROR, "\nASSERT_EFI_ERROR (Status = %r)\n", StatusParameter));  \
          _ASSERT (!EFI_ERROR (StatusParameter));                                        \
        }                                                                                \
      } else {                                                                           \
        if (EFI_ERROR (StatusParameter)) { Action; }                                     \
      }                                                                                  \
    } while (FALSE)
#else
#define NV_ASSERT_EFI_ERROR_RETURN(StatusParameter, Action)
#endif

#endif
