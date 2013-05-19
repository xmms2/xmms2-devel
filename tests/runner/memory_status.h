/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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

#ifndef __MEMORY_STATUS_H__
#define __MEMORY_STATUS_H__

typedef enum memory_status_St {
	MEMORY_OK    = 1 << 0,
	MEMORY_LEAK  = 1 << 1,
	MEMORY_ERROR = 1 << 2
} memory_status_t;

/**
 * Gather a baseline for leaks and errors.
 */
void memory_status_calibrate (void);

/**
 * Check if any leaks or errors have been introduced since baseline.
 * @return status flag of memory_status_t.
 */
int memory_status_verify (void);

#endif
