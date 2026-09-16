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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// CRC.h ///////////////////////////////////////////////////////////////
// A class encapsulating CRC calculation
// Author: Matthew D. Campbell, October 2001

#pragma once

#ifndef _CRC_H_
#define _CRC_H_

#include "Lib/BaseType.h"
#include "winsock2.h" // for htonl

#ifdef _DEBUG

class CRC
{
public:
	CRC() { crc = 0; }

	void computeCRC( const void *buf, Int len );		///< Compute the CRC for a buffer, added into current CRC
	void clear( void ) { crc = 0; }									///< Clears the CRC to 0
//	UnsignedInt get( void ) { return htonl(crc); }	///< Get the combined CRC
	UnsignedInt get( void );

private:
	void addCRC( UnsignedByte val );									///< CRC a 4-byte block

	UnsignedInt crc;
};

#else

// optimized inline only version
class CRC
{
public:
	CRC(void) { crc=0; }

  /// Compute the CRC for a buffer, added into current CRC
	__forceinline void computeCRC( const void *buf, Int len )
  {
    if (!buf||len<1)
      return;
    
    /* C++ version left in for reference purposes
	  for (UnsignedByte *uintPtr=(UnsignedByte *)buf;len>0;len--,uintPtr++)
    {
    	int hibit;
    	if (crc & 0x80000000) 
      {
		    hibit = 1;
	    } 
      else 
      {
		    hibit = 0;
	    }

	    crc <<= 1;
	    crc += *uintPtr;
	    crc += hibit;
    }
    */

    /* Ported: off Windows this is the C++ version kept in the comment above, which the author
    ** verified produces the same data as the assembly.  The two are the same three steps - carry
    ** out the top bit, shift left, add the byte and the carry back in - and this CRC feeds the
    ** desync check, so anything but bit-identical would show up as every network game
    ** disagreeing on the first frame.
    **
    ** The assembly is kept for MSVC because that build is unchanged by this port.  It is worth
    ** noting that the comment above it is right: the block uses EBX, ESI and EDI without the
    ** compiler knowing, and it survives on luck. */
#ifdef _MSC_VER
    unsigned *crcPtr=&crc;
    _asm
    {
      push ebx
      push esi
      push edi
      mov esi,[buf]
      mov ecx,[len]
      dec ecx
      mov edi,[crcPtr]
      mov ebx,dword ptr [edi]
      xor eax,eax
    lp:
      mov al,byte ptr [esi]
      shl ebx,1
      inc esi
      adc ebx,eax
      dec ecx
      jns lp
      mov dword ptr [edi],ebx
      pop edi
      pop esi
      pop ebx
    };
#else
    for (const UnsignedByte *uintPtr=(const UnsignedByte *)buf; len>0; len--, uintPtr++)
    {
      unsigned hibit = (crc & 0x80000000u) ? 1u : 0u;
      crc <<= 1;
      crc += *uintPtr;
      crc += hibit;
    }
#endif
  }

  /// Clears the CRC to 0
	void clear( void ) 
  { 
    crc = 0; 
  }									

  ///< Get the combined CRC
	UnsignedInt get( void ) const
  {
    return crc;
  }

private:
	UnsignedInt crc;
};

#endif

#endif // _CRC_H_
