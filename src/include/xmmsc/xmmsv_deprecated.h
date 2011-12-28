/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */


#ifndef __XMMSV_DEPRECATED_H__
#define __XMMSV_DEPRECATED_H__

#include "xmmsc/xmmsv_general.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup DeprecatedValueType Deprecated
 * @ingroup ValueType
 * @{
 */

/** @deprecated */
static inline xmmsv_type_t XMMSV_TYPE_UINT32_IS_DEPRECATED(void) XMMS_DEPRECATED;
static inline xmmsv_type_t
XMMSV_TYPE_UINT32_IS_DEPRECATED (void)
{
	return XMMSV_TYPE_INT32;
}
#define XMMSV_TYPE_UINT32 XMMSV_TYPE_UINT32_IS_DEPRECATED()


/** @deprecated */
int xmmsv_get_uint (const xmmsv_t *val, uint32_t *r) XMMS_DEPRECATED;

/** @} */

#ifdef __cplusplus
}
#endif

#endif
