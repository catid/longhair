/*
	Copyright (c) 2014 Christopher A. Taylor.  All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	* Redistributions of source code must retain the above copyright notice,
	  this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	* Neither the name of Longhair nor the names of its contributors may be
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

#ifndef CAT_CAUCHY_256_HPP
#define CAT_CAUCHY_256_HPP

#ifdef __cplusplus
extern "C" {
#endif

#define CAUCHY_256_VERSION 1

/*
 * Verify binary compatibility with the API on startup.
 *
 * Example:
 * 	if (!cauchy_256_init()) exit(1);
 *
 * Returns non-zero on success.
 * Returns 0 if the API level does not match.
 */
extern int _cauchy_256_init(int expected_version);
#define cauchy_256_init() _cauchy_256_init(CAUCHY_256_VERSION)


// Descriptor for received data block
typedef struct _Block {
	unsigned char *data;
	unsigned char row;
} Block;


/*
 * Cauchy encode
 *
 * Returns a valid state object on success.
 * Returns 0 on failure.
 */
extern int cauchy_256_encode(int k, int m, const void *data, void *recovery_blocks, int block_bytes);

/*
 * Cauchy decode
 *
 * Returns a valid state object on success.
 * Returns 0 on failure.
 */
extern int cauchy_256_decode(int k, int m, Block *blocks, int block_bytes);


#ifdef __cplusplus
}
#endif

#endif // CAT_CAUCHY_256_HPP

