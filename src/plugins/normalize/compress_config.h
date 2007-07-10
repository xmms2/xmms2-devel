/*  AudioCompress
 *  Copyright (C) 2002-2003 trikuare studios (http://trikuare.cx)
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

/* config.h
** Default values for the configuration, and also a few random debug things
*/

#ifndef COMPRESS_CONFIG_H
#define COMPRESS_CONFIG_H

/*** Version information ***/
#define ACVERSION "1.5.2"

/*** Monitoring stuff ***/
//#define DEBUG			/* Show debugging information */
//#define STATS			/* Show statistics on stderr */

/*** Default configuration stuff ***/
#define SHOWMON 1		/* Whether to show the monitor */
#define ANTICLIP 0		/* Strict clipping protection */
#define TARGET 25000		/* Target level */

#define GAINMAX 32		/* The maximum amount to amplify by */
#define GAINSHIFT 10		/* How fine-grained the gain is */
#define GAINSMOOTH 8		/* How much inertia ramping has*/
#define BUCKETS 400		/* How long of a history to store */

#endif

