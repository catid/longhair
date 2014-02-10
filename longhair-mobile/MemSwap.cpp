/*
	Copyright (c) 2014 Christopher A. Taylor.  All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	* Redistributions of source code must retain the above copyright notice,
	  this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	* Neither the name of LibCat nor the names of its contributors may be
	  used to endorse or promote products derived from this software without
	  specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#include "MemSwap.hpp"
using namespace cat;

void cat::memswap(void * CAT_RESTRICT vx, void * CAT_RESTRICT vy, int bytes)
{
	// Primary engine
	u64 * CAT_RESTRICT x64 = reinterpret_cast<u64 *>( vx );
	u64 * CAT_RESTRICT y64 = reinterpret_cast<u64 *>( vy );

	// Handle remaining multiples of 8 bytes
	while (bytes >= 8) {
		u64 temp = x64[0];
		x64[0] = y64[0];
		y64[0] = temp;
		bytes -= 8;
		++x64;
		++y64;
	}

	// Handle final <8 bytes
	u8 * CAT_RESTRICT x = reinterpret_cast<u8 *>( x64 );
	u8 * CAT_RESTRICT y = reinterpret_cast<u8 *>( y64 );
	u8 t;
	u32 t32;

	switch (bytes) {
	case 7: t = x[6]; x[6] = y[6]; y[6] = t;
	case 6:	t = x[5]; x[5] = y[5]; y[5] = t;
	case 5:	t = x[4]; x[4] = y[4]; y[4] = t;
	case 4:	t32 = *(u32*)x; *(u32*)x = *(u32*)y; *(u32*)y = t32;
		break;
	case 3:	t = x[2]; x[2] = y[2]; y[2] = t;
	case 2:	t = x[1]; x[1] = y[1]; y[1] = t;
	case 1:	t = x[0]; x[0] = y[0]; y[0] = t;
	case 0:
	default:
		break;
	}
}

