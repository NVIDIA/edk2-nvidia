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

#ifndef INCLUDED_TYPE_H
#define INCLUDED_TYPE_H
/*
 * @addtogroup typedef related info
 *
 * @brief typedefs that indicate size and signness
 * @{
 */
/* Following added to avoid misraC 4.6
 * Here we are defining intermediate type
 */
/** intermediate type for unsigned int */
typedef unsigned int		my_uint32_t;
/** intermediate type for int */
typedef int			my_int32_t;
/** intermediate type for unsigned short */
typedef unsigned short		my_uint16_t;
/** intermediate type for short */
typedef short			my_int16_t;
/** intermediate type for char */
typedef char			my_int8_t;
/** intermediate type for unsigned char */
typedef unsigned char		my_uint8_t;
/** intermediate type for unsigned long long */
typedef unsigned long long 	my_ulint_64;
/** intermediate type for long */
typedef unsigned long		my_uint64_t;

/* Actual type used in code */
/** typedef equivalent to unsigned int */
typedef my_uint32_t		nveu32_t;
/** typedef equivalent to int */
typedef my_int32_t		nve32_t;
/** typedef equivalent to unsigned short */
typedef my_uint16_t		nveu16_t;
/** typedef equivalent to short */
typedef my_int16_t		nve16_t;
/** typedef equivalent to char */
typedef my_int8_t		nve8_t;
/** typedef equivalent to unsigned char */
typedef my_uint8_t		nveu8_t;
/** typedef equivalent to unsigned long  long */
typedef my_ulint_64		nveul64_t;
/** typedef equivalent to long long */
typedef my_uint64_t		nveu64_t;
/** @} */

#endif /* INCLUDED_TYPE_H */
