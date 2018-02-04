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

#include <stdint.h>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

#define CAUCHY_256_VERSION 2

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
extern void cauchy_256_deinit();

typedef struct _Cauchy256 {
    unsigned char matrix[256 * 256];
    uint64_t bitMatrix[65280]; // Max size is recovery_count=255 -> bitrows = 2040, bitstride = 32, so size = 65280
    unsigned char* buffer;
    size_t maxBlockBytes;
    size_t maxBufferSize;
} Cauchy256;

extern Cauchy256 *cauchy_256_create(size_t maxBlockBytes);
extern void cauchy_256_destroy(Cauchy256 *c256);

// Descriptor for received data block
typedef struct _Block {
	unsigned char *data;
	unsigned char row;
} Block;


/*
 * Cauchy encode
 *
 * This produces a set of recovery blocks that should be transmitted after the
 * original data blocks.
 *
 * It takes in k equal-sized blocks and produces m equal-sized recovery blocks.
 * The input block pointer array allows more natural usage of the library.
 * The output recovery blocks are stored end-to-end in the recovery_blocks.
 *
 * The number of bytes per block (block_bytes) should be a multiple of 8.
 *
 * The sum of k and m should be less than or equal to 256: k + m <= 256.
 *
 * When transmitting the data, the block index of the data should be sent,
 * and the recovery block index is also needed.  The decoder should also
 * be provided with the values of k, m, and block_bytes used for encoding.
 *
 * Returns 0 on success, and any other code indicates failure.
 */
extern int cauchy_256_encode(Cauchy256* c256, int k, int m, const unsigned char *data_ptrs[], void *recovery_blocks, int block_bytes);

/*
 * Cauchy decode
 *
 * This recovers the original data from the recovery data in the provided
 * blocks.
 *
 * You should provide the same k, m, block_bytes values used by the encoder.
 *
 * The blocks array contains pointers to data buffers each with block_bytes.
 * This array allows you to arrange the blocks in memory in any way that is
 * convenient.
 *
 * The "row" should be set to the block index of the original data.
 * For example the second packet should be row = 1.  The "row" should be set to
 * k + i for the i'th recovery block.  For example the first recovery block row
 * is k, and the second recovery block row is k + 1.
 *
 * I recommend filling in recovery blocks at the end of the array, and filling
 * in original data from the start.  This way when the function completes, all
 * the missing data will be clustered at the end.
 *
 * Returns 0 on success, and any other code indicates failure.
 */
extern int cauchy_256_decode(Cauchy256* c256, int k, int m, Block *blocks, int block_bytes);


#ifdef __cplusplus
}
#endif

#endif // CAT_CAUCHY_256_HPP

