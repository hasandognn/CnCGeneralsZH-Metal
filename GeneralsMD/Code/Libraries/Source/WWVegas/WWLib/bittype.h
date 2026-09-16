/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* $Header: /G/wwlib/bittype.h 4     4/02/99 1:37p Eric_c $ */
/*************************************************************************** 
 ***                  Confidential - Westwood Studios                    *** 
 *************************************************************************** 
 *                                                                         * 
 *                 Project Name : Voxel Technology                         * 
 *                                                                         * 
 *                    File Name : BITTYPE.H                                * 
 *                                                                         * 
 *                   Programmer : Greg Hjelstrom                           * 
 *                                                                         * 
 *                   Start Date : 02/24/97                                 * 
 *                                                                         * 
 *                  Last Update : February 24, 1997 [GH]                   * 
 *                                                                         * 
 *-------------------------------------------------------------------------* 
 * Functions:                                                              * 
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef BITTYPE_H
#define BITTYPE_H

/* Ported: these were spelled with "long", which is 32 bits on Win32 and 64 bits on every LP64
** platform.  uint32 is a file format here - it is what the save games, the .big indexes and the
** network packets are written in - so it is named by width rather than by C type.  Fixing it is
** not optional off Windows: at 64 bits every one of those layouts moves. */

#include <cstdint>

typedef uint8_t         uint8;
typedef uint16_t        uint16;
typedef uint32_t        uint32;
typedef unsigned int    uint;

typedef int8_t          sint8;
typedef int16_t         sint16;
typedef int32_t         sint32;
typedef int             sint;

typedef uint64_t        uint64;
typedef int64_t         sint64;

typedef float           float32;
typedef double          float64;

/* The Win32 spellings.  windows.h declares them on Windows; off it they come from the shim, which
** is the single place their widths are decided.  Defining them a second time here is what made
** DWORD 64 bits wide in this translation unit and 32 in the next one. */
#ifdef _WIN32
typedef unsigned long   DWORD;
typedef unsigned short  WORD;
typedef unsigned char   BYTE;
typedef int             BOOL;
typedef unsigned short  USHORT;
typedef const char *    LPCSTR;
typedef unsigned int    UINT;
typedef unsigned long   ULONG;
#else
#include "Platform/Win32Compat.h"
typedef unsigned short  USHORT;
#endif

#endif //BITTYPE_H
