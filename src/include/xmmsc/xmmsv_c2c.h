/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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

#ifndef __XMMS_C2C_H__
#define __XMMS_C2C_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Helper functions to extract fields from c2c messages */
int xmmsv_c2c_message_get_sender (xmmsv_t *c2c_msg) XMMS_PUBLIC;
int xmmsv_c2c_message_get_id (xmmsv_t *c2c_msg) XMMS_PUBLIC;
int xmmsv_c2c_message_get_destination (xmmsv_t *c2c_msg) XMMS_PUBLIC;
xmmsv_t *xmmsv_c2c_message_get_payload (xmmsv_t *c2c_msg) XMMS_PUBLIC;

#ifdef __cplusplus
}
#endif

#endif
