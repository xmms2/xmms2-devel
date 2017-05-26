/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2017 XMMS2 Team
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

#ifndef __RB_RESULT_H
#define __RB_RESULT_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsc/xmmsc_stdbool.h>

void Init_Result (VALUE mXmms);

VALUE TO_XMMS_CLIENT_RESULT (VALUE xmms, xmmsc_result_t *res);

#endif
