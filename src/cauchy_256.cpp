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

#include "cauchy_256.h"

/*
 * Cauchy Reed Solomon (CRS) codes [1]
 *
 * For general purpose error correction under ~32 symbols it is either the best
 * option, or it is more flexible (due to patents/other awkwardness) than the
 * alternatives.
 *
 * CRS codes are parameterized primarily by m, k, and w:
 * 	k = Number of original data blocks.
 * 	m = Number of redundant error correction blocks.
 * 	w = Exponent of the binary extension Galois field used.  eg. GF(2^w)
 *
 * The choice of w limits k and m by the relation: k + m <= 2^w
 * So if w = 8, it can generate up to 256 blocks of original + correction data.
 *
 * In practice if you want to send more than 256 blocks of data there are
 * definitely more efficient options than CRS codes that scale much more
 * gracefully, so w = 8 is a flexible choice that does not require incredibly
 * large tables and does not require an irritating data massaging step to fit
 * input into the field.
 *
 * Note that m = 1 is a degenerate case where the best solution is to just XOR
 * all of the k input data blocks together.  So CRS codes are interesting for
 * 1 < m < 32.
 *
 * These codes have been thoroughly explored by Dr. James Plank over the past
 * ~10 years [1].  In this time there has not been a lot of work on improving
 * Jerasure [2] to speed up CRS codes for small datasets.
 *
 * For example, all of the existing work on Jerasure is in reference to disk or
 * cloud storage applications where the file pieces are many megabytes in size.
 * A neglected area of interest is packet error correction codes, where the
 * data is small and the setup time for the codes is critical.
 *
 * Jerasure is designed to be generic, so it has best matrices for m = 2 for
 * all of the values of w that may be of interest.  But it does not attempt to
 * optimize for m > 2, which is a huge missed opportunity for better speed for
 * packet error correction.
 *
 * Jerasure only tries one generator polynomial for GF(256) instead of
 * exploring all 16 of the possible generators to find minimal Cauchy matrices.
 * 6% improvement was possible.
 *
 * Jerasure uses a "matrix improvement" formula to quickly derive an optimal
 * Cauchy matrix modified to reduce the number of ones.  I came up with a new
 * approach that initializes much faster while yielding roughly 30% fewer ones
 * in the resulting matrix, in trade for a 30KB precomputed table.
 *
 * It may be possible to speed up the codec using other values of w, but a
 * generic implementation that uses w = 7 will not run faster than a
 * specialized implementation that uses w = 8, and speed above 400 MB/s is not
 * especially meaningful even if it can be achieved.
 *
 * [1] "Optimizing Cauchy Reed-Solomon Codes for Fault-Tolerant Storage Applications" (2005)
 *	http://web.eecs.utk.edu/~plank/plank/papers/CS-05-569.pdf
 * [2] "Jerasure 2.0 A Library in C/C++ Facilitating Erasure Coding for Storage Applications" (2014)
 * 	http://jerasure2.googlecode.com/svn/trunk/jerasure3/documentation/paper.pdf
 */

/*
 * Improvement on Jerasure's 8x8 submatrix generation:
 *
 * Instead of bit-slicing across all 8 rows, I byte-slice instead.
 * Specifically, in Jerasure it is done like the following.
 *
 * For example, if a GF(16) element is "9", then in Jerasure it would
 * be split up between bitmatrix rows like this:
 *
 *	1...
 *	0...
 *	0...
 *	1...
 *
 * where each column is the previous column multiplied by 2.
 *
 * This requires expensive bit operations to separate out each of the bits into
 * each of the rows.
 *
 * However, the transpose of these submatrices is also invertible.
 *
 * A hand-wavy proof is that you can swap the X[] and Y[] values that generate
 * the Cauchy matrix and it is still invertible, so taking the transpose of
 * each element should be okay.  This was experimentally verified.
 *
 * So this code slices up the GF(256) elements like this:
 *
 * 1001
 * ....
 * ....
 * ....
 *
 * where each *row* is the previous row times 2.
 *
 * This completely eliminates the bit twiddling and works just as well.
 */

/*
 * Up to 300% performance increase using windowing:
 *
 * The encoder has a very difficult task of generating all of the recovery
 * symbols.  The decoder often does not need any of them, so the performance
 * of the codec can be judged by how fast the encoder runs.  In practical
 * applications, if the encoder is too slow, then it will not be used.
 *
 * To speed up the encoder specifically, I recognized that the performance
 * of the encoder varies with m but not with k.  To run faster with larger m,
 * I re-used a windowed approach for bitmatrix multiplication from Wirehair:
 *
 * For example since each element of the GF(256) matrix represents an 8x8
 * submatrix in the bitmatrix, and each column bit represents an offset into
 * the input data, there are many rows that repeat the same bit patterns.
 * Since this is an MDS code, the number of repeats for 4 bits should be
 * roughly 8 * m / 16 = m / 2.  So as m increases, it makes increasing sense
 * to precalculate combinations of the input data and work on sets of bits.
 *
 * For a concrete example:
 *
 * 	1000 -> "G"
 * 	0100 -> "L"
 * 	0010 -> "A"
 * 	0001 -> "D"
 *	1101 = "G" + "L" + "D"
 *	0101 = "L" + "D"
 *	1110 = "G" + "L" + "A"
 *	1011 = "G" + "A" + "D"
 *
 * The upper identity matrix maps to the original data.  This implicitly exists
 * in the code and does not need to be actually constructed.  This would be a
 * k = 4 case.  The final 4 rows are the redundant blocks and so m = 4.  In
 * this simple example, w = 1.  Note that the redundant blocks are linear
 * combinations of the original blocks.
 *
 * Choosing a 2-bit window to calculate the redundant blocks results in a table
 * with 4 entries:
 *
 *	T[00] = (don't care)
 *	T[10] = "G"
 *	T[01] = "L"
 *	T[11] = "G" + "L" <= only actual precomputation
 *
 *	T'[00] = (don't care)
 *	T'[10] = "A"
 *	T'[01] = "D"
 *	T'[11] = "A" + "D" <= only actual precomputation
 *
 * And the first two columns of bits for the bottom four rows becomes:
 *
 *	(11) (01) = T[11] + T'[01]
 *	(01) (01) = T[01] + T'[01]
 *	(11) (10) = T[11] + T'[10]
 *	(10) (11) = T[10] + T'[11]
 *
 * Instead of calculating "G" + "L" twice, it can just be looked up from the
 * table.  Now imagine a larger table and many more rows.  For this library,
 * m = 4 means 32 binary rows, so the advantage of windowing becomes apparent.
 * The number of memory accesses is decreased dramatically.  Since the speed
 * of the code drops significantly as m increases, this optimization attacks
 * the problem directly.
 *
 * This library uses two 4-bit lookup tables because the bitmatrix is a
 * multiple of w=8 bits in width.  This also allows for avoiding storing the
 * bitmatrix in memory - All the work can be done in registers.
 * This 4-bit window technique starts being useful in practice at m = 5, and
 * improves the encoder speed by up to 300%.
 *
 * Jerasure does attempt to do some row-reuse, but it tries to reuse the
 * *entire* bitmatrix row in its "smart schedule" mode.  This has very limited
 * performance impact and actually hurts performance in most of my tests.
 *
 * Windowed bitmatrix multiplication is implemented in win_encode().
 * A variation of this window technique is also used in the decoder for speed;
 * it is done on triangular matrices during Gaussian elimination.
 */

//#define CAT_CAUCHY_LOG

// Debugging
#ifdef CAT_CAUCHY_LOG
#include <iostream>
#include <cassert>
using namespace std;
#define DLOG(x) x

#else
#define DLOG(x)
#endif

//#include "BitMath.hpp"
#include "MemXOR.hpp"
#include "MemSwap.hpp"
using namespace cat;

// Constants for precomputed table for window method
static const int PRECOMP_TABLE_SIZE = 11; // Number of non-zero elements
static const int PRECOMP_TABLE_THRESH = 4; // Min recovery rows to use window
// NOTE: Some of the code assumes that threshold is at least 3.

#ifdef CAT_CAUCHY_LOG

static void print_word(const u64 row, int bits)
{
	for (int jj = 0; jj < bits; ++jj) {
		if (row & ((u64)1 << jj)) {
			cout << "1";
		} else {
			cout << "0";
		}
	}
	cout << endl;
}

static void print_words(const u64 *row, int words)
{
	for (int ii = 0; ii < words; ++ii) {
		for (int jj = 0; jj < 64; ++jj) {
			if (row[ii] & ((u64)1 << jj)) {
				cout << "1";
			} else {
				cout << "0";
			}
		}
	}
	cout << endl;
}

static void print_matrix(const u64 *matrix, int word_stride, int rows)
{
	cout << "Printing matrix with " << word_stride << " words per row, and " << rows << " rows:" << endl;

	for (int ii = 0; ii < rows; ++ii) {
		print_words(matrix, word_stride);
		matrix += word_stride;
	}
}

#endif // CAT_CAUCHY_LOG



//// GF(256) math

// Tables generated with optimal polynomial 0x187 = 110000111b

static const u16 GFC256_LOG_TABLE[256] = {
512,255,1,99,2,198,100,106,3,205,199,188,101,126,107,42,4,141,206,78,
200,212,189,225,102,221,127,49,108,32,43,243,5,87,142,232,207,172,79,131,
201,217,213,65,190,148,226,180,103,39,222,240,128,177,50,53,109,69,33,18,
44,13,244,56,6,155,88,26,143,121,233,112,208,194,173,168,80,117,132,72,
202,252,218,138,214,84,66,36,191,152,149,249,227,94,181,21,104,97,40,186,
223,76,241,47,129,230,178,63,51,238,54,16,110,24,70,166,34,136,19,247,
45,184,14,61,245,164,57,59,7,158,156,157,89,159,27,8,144,9,122,28,
234,160,113,90,209,29,195,123,174,10,169,145,81,91,118,114,133,161,73,235,
203,124,253,196,219,30,139,210,215,146,85,170,67,11,37,175,192,115,153,119,
150,92,250,82,228,236,95,74,182,162,22,134,105,197,98,254,41,125,187,204,
224,211,77,140,242,31,48,220,130,171,231,86,179,147,64,216,52,176,239,38,
55,12,17,68,111,120,25,154,71,116,167,193,35,83,137,251,20,93,248,151,
46,75,185,96,15,237,62,229,246,135,165,23,58,163,60,183};

static const u8 GFC256_EXP_TABLE[512*2+1] = {
1,2,4,8,16,32,64,128,135,137,149,173,221,61,122,244,111,222,59,118,
236,95,190,251,113,226,67,134,139,145,165,205,29,58,116,232,87,174,219,49,
98,196,15,30,60,120,240,103,206,27,54,108,216,55,110,220,63,126,252,127,
254,123,246,107,214,43,86,172,223,57,114,228,79,158,187,241,101,202,19,38,
76,152,183,233,85,170,211,33,66,132,143,153,181,237,93,186,243,97,194,3,
6,12,24,48,96,192,7,14,28,56,112,224,71,142,155,177,229,77,154,179,
225,69,138,147,161,197,13,26,52,104,208,39,78,156,191,249,117,234,83,166,
203,17,34,68,136,151,169,213,45,90,180,239,89,178,227,65,130,131,129,133,
141,157,189,253,125,250,115,230,75,150,171,209,37,74,148,175,217,53,106,212,
47,94,188,255,121,242,99,198,11,22,44,88,176,231,73,146,163,193,5,10,
20,40,80,160,199,9,18,36,72,144,167,201,21,42,84,168,215,41,82,164,
207,25,50,100,200,23,46,92,184,247,105,210,35,70,140,159,185,245,109,218,
51,102,204,31,62,124,248,119,238,91,182,235,81,162,195,1,2,4,8,16,
32,64,128,135,137,149,173,221,61,122,244,111,222,59,118,236,95,190,251,113,
226,67,134,139,145,165,205,29,58,116,232,87,174,219,49,98,196,15,30,60,
120,240,103,206,27,54,108,216,55,110,220,63,126,252,127,254,123,246,107,214,
43,86,172,223,57,114,228,79,158,187,241,101,202,19,38,76,152,183,233,85,
170,211,33,66,132,143,153,181,237,93,186,243,97,194,3,6,12,24,48,96,
192,7,14,28,56,112,224,71,142,155,177,229,77,154,179,225,69,138,147,161,
197,13,26,52,104,208,39,78,156,191,249,117,234,83,166,203,17,34,68,136,
151,169,213,45,90,180,239,89,178,227,65,130,131,129,133,141,157,189,253,125,
250,115,230,75,150,171,209,37,74,148,175,217,53,106,212,47,94,188,255,121,
242,99,198,11,22,44,88,176,231,73,146,163,193,5,10,20,40,80,160,199,
9,18,36,72,144,167,201,21,42,84,168,215,41,82,164,207,25,50,100,200,
23,46,92,184,247,105,210,35,70,140,159,185,245,109,218,51,102,204,31,62,
124,248,119,238,91,182,235,81,162,195,1,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

static const u8 GFC256_INV_TABLE[256] = {
0,1,195,130,162,126,65,90,81,54,63,172,227,104,45,42,235,155,27,53,
220,30,86,165,178,116,52,18,213,100,21,221,182,75,142,251,206,233,217,161,
110,219,15,44,43,14,145,241,89,215,58,244,26,19,9,80,169,99,50,245,
201,204,173,10,91,6,230,247,71,191,190,68,103,123,183,33,175,83,147,255,
55,8,174,77,196,209,22,164,214,48,7,64,139,157,187,140,239,129,168,57,
29,212,122,72,13,226,202,176,199,222,40,218,151,210,242,132,25,179,185,135,
167,228,102,73,149,153,5,163,238,97,3,194,115,243,184,119,224,248,156,92,
95,186,34,250,240,46,254,78,152,124,211,112,148,125,234,17,138,93,188,236,
216,39,4,127,87,23,229,120,98,56,171,170,11,62,82,76,107,203,24,117,
192,253,32,74,134,118,141,94,158,237,70,69,180,252,131,2,84,208,223,108,
205,60,106,177,61,200,36,232,197,85,113,150,101,28,88,49,160,38,111,41,
20,31,109,198,136,249,105,12,121,166,66,246,207,37,154,16,159,189,128,96,
144,47,114,133,51,59,231,67,137,225,143,35,193,181,146,79};

u8 * CAT_RESTRICT GFC256_MUL_TABLE = 0;
u8 * CAT_RESTRICT GFC256_DIV_TABLE = 0;

static void GFC256Init()
{
	if (GFC256_MUL_TABLE) {
		return;
	}

	// Allocate table memory 65KB x 2
	GFC256_MUL_TABLE = new u8[256 * 256 * 2];
	GFC256_DIV_TABLE = GFC256_MUL_TABLE + 256 * 256;

	u8 *m = GFC256_MUL_TABLE, *d = GFC256_DIV_TABLE;

	// Unroll y = 0 subtable
	for (int x = 0; x < 256; ++x) {
		m[x] = d[x] = 0;
	}

	// For each other y value,
	for (int y = 1; y < 256; ++y) {
		// Calculate log(y) for mult and 255 - log(y) for div
		const u8 log_y = (u8)GFC256_LOG_TABLE[y];
		const u8 log_yn = 255 - log_y;

		// Next subtable
		m += 256;
		d += 256;

		// Unroll x = 0
		m[0] = 0;
		d[0] = 0;

		// Calculate x * y, x / y
		for (int x = 1; x < 256; ++x) {
			int log_x = GFC256_LOG_TABLE[x];

			m[x] = GFC256_EXP_TABLE[log_x + log_y];
			d[x] = GFC256_EXP_TABLE[log_x + log_yn];
		}
	}
}

extern "C" int _cauchy_256_init(int expected_version)
{
	if (expected_version != CAUCHY_256_VERSION) {
		return -1;
	}

	GFC256Init();

	return 0;
}

// return x * y in GF(256)
// For repeated multiplication by a constant, it is faster to put the constant in y.
static CAT_INLINE u8 GFC256Multiply(u8 x, u8 y)
{
	return GFC256_MUL_TABLE[((u32)y << 8) + x];
}

// return x / y in GF(256)
// Memory-access optimized for constant divisors in y.
static CAT_INLINE u8 GFC256Divide(u8 x, u8 y)
{
	return GFC256_DIV_TABLE[((u32)y << 8) + x];
}


//// Cauchy matrix

#include "cauchy_tables_256.inc"

#define CAT_CAUCHY_MATRIX_STACK_SIZE 1024

// Precondition: m > 1
static const u8 *cauchy_matrix(int k, int m, int &stride,
		u8 stack[CAT_CAUCHY_MATRIX_STACK_SIZE], bool &dynamic_memory)
{
	dynamic_memory = false;

	switch (m) {
	case 2:
		stride = 254;
		return CAUCHY_MATRIX_2;
	case 3:
		stride = 253;
		return CAUCHY_MATRIX_3;
	case 4:
		stride = 252;
		return CAUCHY_MATRIX_4;
	case 5:
		stride = 251;
		return CAUCHY_MATRIX_5;
	case 6:
		stride = 250;
		return CAUCHY_MATRIX_6;
	}

	u8 *matrix = stack;
	int matrix_size = k * (m - 1);
	if (matrix_size > CAT_CAUCHY_MATRIX_STACK_SIZE) {
		matrix = new u8[matrix_size];
		dynamic_memory = true;
	}

	// Get X[] and Y[] vectors
	const u8 *Y = CAUCHY_MATRIX_Y; // Y[0] = 0
	int n = m - 7; // X[0] = 1
	const u8 *X = CAUCHY_MATRIX_X + n*249 - n*(n + 1)/2;

	//   A B C D E <- X[]
	// F 1 1 1 1 1
	// G a b c d e
	// H f g h i j
	//
	// F = 0, A = 1

	u8 *row = matrix;
	for (int y = 1; y < m; ++y) {
		u8 G = Y[y - 1];

		// Unroll x = 0
		*row++ = GFC256_INV_TABLE[1 ^ G];
		for (int x = 1; x < k; ++x) {
			u8 B = X[x - 1];

			// b = (B + F) / (B + G), F = 0
			*row++ = GFC256Divide(B, B ^ G);
		}
	}
	stride = k;

	return matrix;
}


//// Decoder

// Specialized fast decoder for m = 1
static void cauchy_decode_m1(int k, Block *blocks, int block_bytes)
{
	// Find erased row
	Block *erased = 0;
	for (int ii = 0; ii < k; ++ii) {
		if (blocks[ii].row >= k) {
            erased = &blocks[ii];
            DLOG(cout << "Found erased row " << ii << " on block row " << (int)erased->row << endl;)
			break;
		}
	}

    // If nothing was erased,
    if (!erased)
    {
        // Nothing to decode!
        return;
    }

	// XOR all other blocks into the recovery block
	u8 *out = erased->data;
	const u8 *in = 0;

    // Identify which row was lost.
    bool original[256] = {}; // init to all false

	// For each block,
	for (int ii = 0; ii < k; ++ii) {
		Block *block = blocks + ii;
		if (block != erased) {
            if (block->row < k) {
                original[block->row] = true; // set seen rows to true
            }
			if (!in) {
				in = block->data;
			} else {
				memxor_add(out, in, block->data, block_bytes);
				in = 0;
			}
		}
	}

	// Complete XORs
	if (in) {
		memxor(out, in, block_bytes);
	}

    // Update the row.
    for (int ii = 0; ii < k; ++ii) {
        if (!original[ii]) {
            erased->row = ii; // only not seen row is the right one
            break;
        }
    }
}

// Sort blocks into original and recovery blocks
static void sort_blocks(int k, Block *blocks,
		Block *original[256], int &original_count,
		Block *recovery[256], int &recovery_count, u8 erasures[256])
{
	Block *block = blocks;
	original_count = 0;
	recovery_count = 0;

	// Initialize erasures to zeroes
	for (int ii = 0; ii < k; ++ii) {
		erasures[ii] = 0;
	}

	// For each input block,
	for (int ii = 0; ii < k; ++ii, ++block) {
		int row = block->row;

		// If it is an original block,
		if (row < k) {
			original[original_count++] = block;
			erasures[row] = 1;
		} else {
			recovery[recovery_count++] = block;
		}
	}

	// Identify erasures
	for (int ii = 0, erasure_count = 0; ii < 256 && erasure_count < recovery_count; ++ii) {
		if (!erasures[ii]) {
			erasures[erasure_count++] = ii;
		}
	}
}

// Windowed version of eliminate_original
static void win_original(Block *original[256], int original_count,
						 Block *recovery[256], int recovery_count,
						 const u8 *matrix, int stride, int subbytes,
						 u8 **tables[2])
{
	// For each column to generate,
	for (int jj = 0; jj < original_count; ++jj) {
		Block *original_block = original[jj];
		int original_row = original_block->row;

		const u8 *column = matrix + original_row;
		const u8 *data = original_block->data;

		// Fill in tables
		for (int ii = 0; ii < 2; ++ii, data += subbytes * 4) {
			u8 **table = tables[ii];
			table[1] = (u8 *)data;
			table[2] = (u8 *)data + subbytes;
			table[4] = (u8 *)data + subbytes * 2;
			table[8] = (u8 *)data + subbytes * 3;

			memxor_set(table[3], table[1], table[2], subbytes);
			memxor_set(table[6], table[2], table[4], subbytes);
			memxor_set(table[5], table[1], table[4], subbytes);
			memxor_set(table[7], table[1], table[6], subbytes);
			memxor_set(table[9], table[1], table[8], subbytes);
			memxor_set(table[12], table[4], table[8], subbytes);
			memxor_set(table[10], table[2], table[8], subbytes);
			memxor_set(table[11], table[3], table[8], subbytes);
			memxor_set(table[13], table[1], table[12], subbytes);
			memxor_set(table[14], table[2], table[12], subbytes);
			memxor_set(table[15], table[3], table[12], subbytes);
		}

		const int row_offset = original_count + recovery_count + 1;

		// For each of the rows,
		for (int ii = 0; ii < recovery_count; ++ii) {
			Block *recovery_block = recovery[ii];
			int matrix_row = recovery_block->row - row_offset;

			const u8 *row = column + stride * matrix_row;
			u8 *dest = recovery_block->data;

			// If this matrix element is an 8x8 identity matrix,
			if (matrix_row < 0 || row[0] == 1) {
				// XOR whole block at once
				memxor(dest, original_block->data, subbytes * 8);
			} else {
				u8 slice = row[0];

				// Generate 8x8 submatrix and XOR in bits as needed
				for (int bit_y = 0;; ++bit_y) {
					int low = slice & 15;
					int high = slice >> 4;

					// Add
					if (low && high) {
						memxor_add(dest, tables[0][low], tables[1][high], subbytes);
					} else if (low) {
						memxor(dest, tables[0][low], subbytes);
					} else {
						memxor(dest, tables[1][high], subbytes);
					}
					dest += subbytes;

					if (bit_y >= 7) {
						break;
					}

					slice = GFC256Multiply(slice, 2);
				}
			}
		}
	}
}

static void eliminate_original(Block *original[256], int original_count,
							   Block *recovery[256], int recovery_count,
							   const u8 *matrix, int stride, int subbytes)
{
	DLOG(cout << "Eliminating original:" << endl;)

	int row_offset = original_count + recovery_count + 1;

	// For each recovery block,
	for (int ii = 0; ii < recovery_count; ++ii) {
		Block *recovery_block = recovery[ii];
		int matrix_row = recovery_block->row - row_offset;
		const u8 *row = matrix + stride * matrix_row;

		DLOG(cout << "+ From recovery block " << ii << " at row " << matrix_row << ":" << endl;)

		// For each original block,
		for (int jj = 0; jj < original_count; ++jj) {
			Block *original_block = original[jj];
			int original_row = original_block->row;
			u8 *dest = recovery_block->data;

			DLOG(cout << "++ Eliminating original column " << original_row << endl;)

			// If this matrix element is an 8x8 identity matrix,
			if (matrix_row < 0 || row[original_row] == 1) {
				// XOR whole block at once
				memxor(dest, original_block->data, subbytes * 8);
				DLOG(cout << "XOR" << endl;)
			} else {
				// Grab the matrix entry for this row,
				u8 slice = row[original_row];

				// XOR in bits set in 8x8 submatrix
				for (int bit_y = 0;; ++bit_y) {
					const u8 *src = original_block->data;

					for (int bit_x = 0; bit_x < 8; ++bit_x, src += subbytes) {
						if (slice & (1 << bit_x)) {
							memxor(dest, src, subbytes);
						}
					}

					// Stop after 8 bits
					if (bit_y >= 7) {
						break;
					}

					// Calculate next slice
					slice = GFC256Multiply(slice, 2);
					dest += subbytes;
				}
			}
		}
	}
}

static u64 *generate_bitmatrix(int k, Block *recovery[256], int recovery_count,
						const u8 *matrix, int stride, const u8 erasures[256],
						int &bitstride)
{
	// Allocate the bitmatrix
	int bitrows = recovery_count * 8;
	bitstride = (bitrows + 63) / 64;
	u64 *bitmatrix = new u64[bitstride * bitrows];
	u64 *bitrow = bitmatrix;

	// For each recovery block,
	for (int ii = 0; ii < recovery_count; ++ii) {
		Block *recovery_block = recovery[ii];

		// If first row of matrix,
		int recovery_row = recovery_block->row - k;
		if (recovery_row == 0) {
			// Write 8x8 identity submatrix pattern across each bit row
			u64 pattern = 0x0101010101010101ULL;

			for (int ii = 0; ii < 8; ++ii, pattern <<= 1, bitrow += bitstride) {
				for (int x = 0; x < bitstride; ++x) {
					bitrow[x] = pattern;
				}
			}
		} else {
			const u8 *row = matrix + (recovery_row - 1) * stride;
			int remaining = recovery_count;
			const u8 *erasure = erasures;

			// Otherwise read the elements of the matrix:

			DLOG(cout << "For recovery row " << recovery_row << endl;)

			// Generate eight 64-bit columns of the bitmatrix at a time
			while (remaining > 0) {
				// Take up to 8 columns at a time
				int limit = remaining;
				if (limit > 8) {
					limit = 8;
				}
				remaining -= limit;

				// Unroll first loop
				u64 w[8];
				u8 slice = row[*erasure++];
				w[0] = (u64)slice;

				DLOG(cout << "+ Generating 8x8 submatrix from slice=" << (int)slice << endl;)

				for (int ii = 1; ii < 8; ++ii) {
					slice = GFC256Multiply(slice, 2);
					w[ii] = (u64)slice;
				}

				// For each remaining 8 bit slice,
				for (int shift = 8; --limit > 0; shift += 8) {
					slice = row[*erasure++];
					DLOG(cout << "+ Generating 8x8 submatrix from slice=" << (int)slice << endl;)
					w[0] |= (u64)slice << shift;

					for (int ii = 1; ii < 8; ++ii) {
						slice = GFC256Multiply(slice, 2);
						w[ii] |= (u64)slice << shift;
					}
				}

				// Write 64-bit column of bitmatrix
				u64 *out = bitrow;
				for (int ii = 0; ii < 8; ++ii, out += bitstride) {
					out[0] = w[ii];
				}
				++bitrow;
			}

			bitrow += bitstride * 7;
		}

		// Set the row to what the final recovered row will be
		recovery_block->row = erasures[ii];
	}

	return bitmatrix;
}

/*
 * This version of GE is complicated by performing the operations in two steps:
 *
 * 1) The first round of operations finds the pivots,
 * 2) and the second round runs the data XOR operations.
 *
 * The data XOR operations are selected by the bits left behind while choosing
 * the pivots, so the rows that get XOR'd together need to be masked to avoid
 * clearing the low bits.
 */

// Windowed version of Gaussian elimination
static void win_gaussian_elimination(int rows, Block *recovery[256],
									 u64 *bitmatrix, int bitstride,
									 int subbytes, u8 **tables[2])
{
	const int bit_rows = rows * 8;
	u64 mask = 1;
	u64 *base = bitmatrix;

	// First find all the pivots.  This is similar to the unwindowed version,
	// except that the bitmatrix low bits are not cleared, and the data is not
	// XOR'd together:

	// For each pivot to find,
	for (int pivot = 0; pivot < bit_rows - 1; ++pivot, mask = CAT_ROL64(mask, 1), base += bitstride) {
		const int pivot_word = pivot >> 6;
		u64 *offset = base + pivot_word;
		u64 *row = offset;

		// For each option,
		for (int option = pivot; option < bit_rows; ++option, row += bitstride) {
			// If bit in this row is set,
			if (row[0] & mask) {
				u8 *src = recovery[pivot >> 3]->data + (pivot & 7) * subbytes;

				DLOG(cout << "Found pivot " << pivot << endl;)
				DLOG(print_matrix(bitmatrix, bitstride, bit_rows);)

				// If the rows were out of order,
				if (option != pivot) {
					// Reorder data into the right place
					u8 *data = recovery[option >> 3]->data + (option & 7) * subbytes;
					memswap(src, data, subbytes);

					// Reorder matrix rows
					memswap(row - pivot_word, base, bitstride << 3);
				}

				u64 *other = row;

				// For each other row,
				while (++option < bit_rows) {
					other += bitstride;

					// If that row also has the bit set,
					if (other[0] & mask) {
						DLOG(cout << "Eliminating from row " << option << endl;)

						other[0] ^= offset[0] & (~(mask - 1) ^ mask);

						// For each remaining word,
						for (int ii = 1; ii < bitstride - pivot_word; ++ii) {
							other[ii] ^= offset[ii];
						}
					}
				}

				// Stop here
				break;
			}
		}
	}

	// Use window method to XOR the bulk of the data:

	// Name tables
	u8 **lo_table = tables[0];
	u8 **hi_table = tables[1];

	// For each column to generate,
	for (int x = 0; x < rows - 3; ++x) {
		Block *block_x = recovery[x];
		const u8 *data = block_x->data;
		const u64 *bit_row = bitmatrix + bitstride * (x * 8 + 1) + (x / 8);
		int bit_shift = (x % 8) * 8;

		DLOG(print_matrix(bitmatrix, bitstride, rows * 8);)
		DLOG(cout << "win_gaussian_elimination: " << x << endl;)

		// For each of the two 4-bit windows,
		for (int table_index = 0; table_index < 2; ++table_index) {
			// Fill in lookup table
			u8 **table = tables[table_index];
			table[1] = (u8 *)data;
			table[2] = (u8 *)data + subbytes;
			table[4] = (u8 *)data + subbytes * 2;
			table[8] = (u8 *)data + subbytes * 3;

			// On second loop,
			if (table_index == 1) {
				// Clear the upper right square
				for (int ii = 1; ii <= 8; ii <<= 1) {
					int w = (u8)(bit_row[0] >> bit_shift) & 15;
					bit_row += bitstride;

					DLOG(cout << "For upper-right square at " << ii << " : ";)
					DLOG(print_word(w, 4);)

					if (w) {
						memxor(hi_table[ii], lo_table[w], subbytes);
					}
				}

				// Fix some variables for the second loop
				bit_row -= bitstride * 3;
				bit_shift += 4;
			} else {
				data += subbytes * 4;
			}

			DLOG(cout << "For triangle " << table_index << ":" << endl;)
			DLOG(print_word(bit_row[0] >> bit_shift, 4);)

			// Clear triangle
			u64 word = bit_row[0] >> bit_shift;
			bit_row += bitstride;
			if (word & 1) {
				memxor(table[2], table[1], subbytes);
			}

			DLOG(print_word(bit_row[0] >> bit_shift, 4);)

			word = bit_row[0] >> bit_shift;
			bit_row += bitstride;
			if (word & 1) {
				memxor(table[4], table[1], subbytes);
			}
			if (word & 2) {
				memxor(table[4], table[2], subbytes);
			}

			DLOG(print_word(bit_row[0] >> bit_shift, 4);)

			word = bit_row[0] >> bit_shift;
			bit_row += bitstride;
			if (word & 1) {
				memxor(table[8], table[1], subbytes);
			}
			if (word & 2) {
				memxor(table[8], table[2], subbytes);
			}
			if (word & 4) {
				memxor(table[8], table[4], subbytes);
			}

			// Generate table
			memxor_set(table[3], table[1], table[2], subbytes);
			memxor_set(table[6], table[2], table[4], subbytes);
			memxor_set(table[5], table[1], table[4], subbytes);
			memxor_set(table[7], table[1], table[6], subbytes);
			memxor_set(table[9], table[1], table[8], subbytes);
			memxor_set(table[12], table[4], table[8], subbytes);
			memxor_set(table[10], table[2], table[8], subbytes);
			memxor_set(table[11], table[3], table[8], subbytes);
			memxor_set(table[13], table[1], table[12], subbytes);
			memxor_set(table[14], table[2], table[12], subbytes);
			memxor_set(table[15], table[3], table[12], subbytes);
		} // next 4-bit window

		// Fix bit shift back to the start of the window
		bit_shift -= 4;

		// For each of the rows,
		for (int y = x + 1; y < rows; ++y) {
			Block *block_y = recovery[y];
			u8 *dest = block_y->data;

			DLOG(cout << "For row " << y << " at " << (u64)dest << endl;)

			for (int jj = 0; jj < 8; ++jj, bit_row += bitstride, dest += subbytes) {
				u8 slice = (u8)(bit_row[0] >> bit_shift);
				int low = slice & 15;
				int high = slice >> 4;

				DLOG(cout << "Applying slice: ";)
				DLOG(print_word(slice, 8);)

				// Add
				if (low && high) {
					memxor_add(dest, lo_table[low], hi_table[high], subbytes);
				} else if (low) {
					memxor(dest, lo_table[low], subbytes);
				} else {
					memxor(dest, hi_table[high], subbytes);
				}
			}
		}
	}

	int pivot = bit_rows - 3 * 8;
	mask = (u64)1 << (pivot & 63);
	base = bitmatrix + (pivot + 1) * bitstride;

	// Clear final 3 columns
	for (; pivot < bit_rows - 1; ++pivot, mask = CAT_ROL64(mask, 1), base += bitstride) {
		const u8 *src = recovery[pivot >> 3]->data + (pivot & 7) * subbytes;
		const u64 *bit_row = base + (pivot >> 6);

		DLOG(cout << "GE pivot " << pivot << endl;)

		for (int other_row = pivot + 1; other_row < bit_rows; ++other_row, bit_row += bitstride) {
			if (bit_row[0] & mask) {
				u8 *dest = recovery[other_row >> 3]->data + (other_row & 7) * subbytes;

				DLOG(cout << "+ Foresub to row " << other_row << endl;)

				memxor(dest, src, subbytes);
			}
		}
	}
}

static void gaussian_elimination(int rows, Block *recovery[256], u64 *bitmatrix,
								 int bitstride, int subbytes)
{
	const int bit_rows = rows * 8;
	u64 mask = 1;
	u64 *base = bitmatrix;

	// For each pivot to find,
	for (int pivot = 0; pivot < bit_rows - 1; ++pivot, mask = CAT_ROL64(mask, 1), base += bitstride) {
		const int pivot_word = pivot >> 6;
		u64 *offset = base + pivot_word;
		u64 *row = offset;

		// For each option,
		for (int option = pivot; option < bit_rows; ++option, row += bitstride) {
			// If bit in this row is set,
			if (row[0] & mask) {
				// Prepare to add in data
				DLOG(cout << "Found pivot " << pivot << endl;)
				DLOG(print_matrix(bitmatrix, bitstride, bit_rows);)

				u8 *src = recovery[pivot >> 3]->data + (pivot & 7) * subbytes;

				// If the rows were out of order,
				if (option != pivot) {
					u8 *data = recovery[option >> 3]->data + (option & 7) * subbytes;

					// Reorder data into the right place
					memswap(src, data, subbytes);

					// Reorder matrix rows
					memswap(row, offset, (bitstride - pivot_word) << 3);
				}

				// For each other row,
				u64 *other = row;
				while (++option < bit_rows) {
					other += bitstride;

					// If that row also has the bit set,
					if (other[0] & mask) {
						DLOG(cout << "Eliminating from row " << option << endl;)

						other[0] ^= offset[0];

						// For each remaining word,
						for (int ii = 1; ii < bitstride - pivot_word; ++ii) {
							other[ii] ^= offset[ii];
						}

						// Add in the data
						u8 *dest = recovery[option >> 3]->data + (option & 7) * subbytes;

						memxor(dest, src, subbytes);
					}
				}

				// Stop here
				break;
			}
		}
	}
}

// Windowed version of back-substitution
static void win_back_substitution(int rows, Block *recovery[256], u64 *bitmatrix,
								  int bitstride, int subbytes, u8 **tables[2])
{
	// Name tables
	u8 **lo_table = tables[1];
	u8 **hi_table = tables[0];

	// For each column to generate,
	for (int x = rows - 1; x >= 3; --x) {
		Block *block_x = recovery[x];
		u8 *data = block_x->data + subbytes * 4;
		u64 *bit_row = bitmatrix + bitstride * ((x + 1) * 8 - 2) + (x / 8);
		int bit_shift = (x % 8) * 8 + 4;

		DLOG(print_matrix(bitmatrix, bitstride, rows * 8);)
		DLOG(cout << "win_back_sub: " << x << endl;)

		// For each of the two 4-bit windows,
		for (int table_index = 0; table_index < 2; ++table_index) {
			// Fill in lookup table
			u8 **table = tables[table_index];
			table[1] = (u8 *)data;
			table[2] = (u8 *)data + subbytes;
			table[4] = (u8 *)data + subbytes * 2;
			table[8] = (u8 *)data + subbytes * 3;

			// On second loop,
			if (table_index == 1) {
				// Clear the upper right square
				for (int ii = 8; ii > 0; ii >>= 1) {
					int w = (u8)(bit_row[0] >> bit_shift) & 15;
					bit_row -= bitstride;

					DLOG(cout << "For upper-right square at " << ii << " : ";)
					DLOG(print_word(w, 4);)

					if (w) {
						memxor(lo_table[ii], hi_table[w], subbytes);
					}
				}

				// Fix some variables for the second loop
				bit_row += bitstride * 3;
				bit_shift -= 4;
			} else {
				data -= subbytes * 4;
			}

			DLOG(cout << "For triangle " << table_index << ":" << endl;)
			DLOG(print_word(bit_row[0] >> bit_shift, 4);)

			// Clear triangle
			u64 word = bit_row[0] >> bit_shift;
			bit_row -= bitstride;
			if (word & 8) {
				memxor(table[4], table[8], subbytes);
			}

			DLOG(print_word(bit_row[0] >> bit_shift, 4);)

			word = bit_row[0] >> bit_shift;
			bit_row -= bitstride;
			if (word & 8) {
				memxor(table[2], table[8], subbytes);
			}
			if (word & 4) {
				memxor(table[2], table[4], subbytes);
			}

			DLOG(print_word(bit_row[0] >> bit_shift, 4);)

			word = bit_row[0] >> bit_shift;
			bit_row -= bitstride;
			if (word & 8) {
				memxor(table[1], table[8], subbytes);
			}
			if (word & 4) {
				memxor(table[1], table[4], subbytes);
			}
			if (word & 2) {
				memxor(table[1], table[2], subbytes);
			}

			// Generate table
			memxor_set(table[3], table[1], table[2], subbytes);
			memxor_set(table[6], table[2], table[4], subbytes);
			memxor_set(table[5], table[1], table[4], subbytes);
			memxor_set(table[7], table[1], table[6], subbytes);
			memxor_set(table[9], table[1], table[8], subbytes);
			memxor_set(table[12], table[4], table[8], subbytes);
			memxor_set(table[10], table[2], table[8], subbytes);
			memxor_set(table[11], table[3], table[8], subbytes);
			memxor_set(table[13], table[1], table[12], subbytes);
			memxor_set(table[14], table[2], table[12], subbytes);
			memxor_set(table[15], table[3], table[12], subbytes);
		} // next 4-bit window

		// For each of the rows,
		for (int y = x - 1; y >= 0; --y) {
			Block *block_y = recovery[y];

			u8 *dest = block_y->data + 7 * subbytes;

			DLOG(cout << "For row " << y << " at " << (u64)dest << endl;)

			for (int jj = 0; jj < 8; ++jj, bit_row -= bitstride, dest -= subbytes) {
				u8 slice = (u8)(bit_row[0] >> bit_shift);
				int low = slice & 15;
				int high = slice >> 4;

				DLOG(cout << "Applying slice: ";)
				DLOG(print_word(slice, 8);)

				// Add
				if (low && high) {
					memxor_add(dest, lo_table[low], hi_table[high], subbytes);
				} else if (low) {
					memxor(dest, lo_table[low], subbytes);
				} else {
					memxor(dest, hi_table[high], subbytes);
				}
			}
		}
	}

	int pivot = 3 * 8 - 1;
	u64 mask = (u64)1 << (pivot & 63);
	const u64 *base = bitmatrix + ((pivot - 1) * bitstride);

	// Clear remaining 3 columns
	for (; pivot > 0; --pivot, mask = CAT_ROR64(mask, 1), base -= bitstride) {
		const u8 *src = recovery[pivot >> 3]->data + (pivot & 7) * subbytes;
		const u64 *bit_row = base + (pivot >> 6);

		DLOG(cout << "BS pivot " << pivot << endl;)

		for (int other_row = pivot - 1; other_row >= 0; --other_row, bit_row -= bitstride) {
			if (bit_row[0] & mask) {
				u8 *dest = recovery[other_row >> 3]->data + (other_row & 7) * subbytes;

				DLOG(cout << "+ Backsub to row " << other_row << endl;)

				memxor(dest, src, subbytes);
			}
		}
	}
}

static void back_substitution(int rows, Block *recovery[256], u64 *bitmatrix,
							  int bitstride, int subbytes)
{
	for (int pivot = rows * 8 - 1; pivot > 0; --pivot) {
		const u8 *src = recovery[pivot >> 3]->data + (pivot & 7) * subbytes;
		const u64 *offset = bitmatrix + (pivot >> 6);
		const u64 mask = (u64)1 << (pivot & 63);

		DLOG(cout << "BS pivot " << pivot << endl;)

		for (int other_row = pivot - 1; other_row >= 0; --other_row) {
			if (offset[bitstride * other_row] & mask) {
				DLOG(cout << "+ Backsub to row " << other_row << endl;)
				u8 *dest = recovery[other_row >> 3]->data + (other_row & 7) * subbytes;

				memxor(dest, src, subbytes);
			}
		}
	}
}

extern "C" int cauchy_256_decode(int k, int m, Block *blocks, int block_bytes)
{
	// If there is only one input block,
	if (k <= 1) {
		// The block is already the same as original data
		blocks[0].row = 0;
		return 0;
	}

	// For the special case of one erasure,
	if (m == 1) {
		cauchy_decode_m1(k, blocks, block_bytes);
		return 0;
	}

	// Sort blocks into original and recovery
	Block *recovery[256];
	int recovery_count;
	Block *original[256];
	int original_count;
	u8 erasures[256];
	sort_blocks(k, blocks, original, original_count, recovery, recovery_count, erasures);

	DLOG(cout << "Recovery rows(" << recovery_count << "):" << endl;
	for (int ii = 0; ii < recovery_count; ++ii) {
		cout << "+ Element " << ii << " fills in for erased row " << (int)erasures[ii] << " with recovery row " << (int)recovery[ii]->row << endl;
	}
	cout << "Original rows(" << original_count << "):" << endl;
	for (int ii = 0; ii < original_count; ++ii) {
		cout << "+ Element " << ii << " points to original row " << (int)original[ii]->row << endl;
	})

	// If nothing is erased,
	if (recovery_count <= 0) {
		return 0;
	}

	// Otherwise there is a restriction on what inputs we can handle
	if ((k + m > 256) || (block_bytes % 8 != 0)) {
		return -1;
	}

	// The Cauchy matrix is selected in a way that has a small
	// number of ones set in the binary representation used here.
	// A combination of precomputation and heuristics provides a
	// near-optimal matrix selection for each value of k, m.

	GFC256Init();

	const int subbytes = block_bytes / 8;

	// Precomputation window workspace
	u8 *precomp = 0;
	u8 **precomp_tables[2];
	u8 *table_stack[16 * 2];

	// If precomputation window is being used,
	if (recovery_count > PRECOMP_TABLE_THRESH) {
		precomp = new u8[subbytes * PRECOMP_TABLE_SIZE * 2];

		precomp_tables[0] = table_stack;
		precomp_tables[1] = table_stack + 16;
		for (int ii = 0; ii < 16*2; ++ii) {
			table_stack[ii] = 0;
		}

		// Fill in tables
		u8 *precomp_ptr = precomp;
		for (int ii = 0; ii < 2; ++ii, precomp_ptr += subbytes * PRECOMP_TABLE_SIZE) {
			u8 **table = precomp_tables[ii];

			table[3] = precomp_ptr;
			table[5] = precomp_ptr + subbytes;
			table[6] = precomp_ptr + subbytes * 2;
			table[7] = precomp_ptr + subbytes * 3;
			for (int jj = 9; jj < 16; ++jj) {
				table[jj] = precomp_ptr + subbytes * (jj - 5);
			}
		}
	}

	// Generate Cauchy matrix
	int stride;
	u8 stack_space[CAT_CAUCHY_MATRIX_STACK_SIZE];
	bool dynamic_matrix;
	const u8 *matrix = cauchy_matrix(k, m, stride, stack_space, dynamic_matrix);

	// From the Cauchy matrix, each byte value can be expanded into
	// an 8x8 submatrix containing a minimal number of ones.
	// The rows that made it through from the original data provide
	// some of the column values for the matrix, so those can be
	// eliminated immediately.  This is useful because it conceptually
	// zeroes out those eliminated matrix elements.  And so when it
	// comes time to laborously generate the bitmatrix and solve it
	// with Gaussian elimination, that bitmatrix can be smaller since
	// it does not need to include these rows and columns.

	// If original data exists,
	if (original_count > 0) {
		// Eliminate original data from recovery rows
		if (recovery_count > PRECOMP_TABLE_THRESH) {
			win_original(original, original_count, recovery, recovery_count, matrix, stride, subbytes, precomp_tables);
		} else {
			eliminate_original(original, original_count, recovery, recovery_count, matrix, stride, subbytes);
		}
	}

	// Now that the columns that are missing have been identified,
	// it is time to generate a bitmatrix to represent the original
	// rows that have been XOR'd together to produce the recovery data.
	// This matrix is guaranteed to be inverible as it was selected
	// from the rows/columns of a Cauchy matrix.

	// Generate square bitmatrix for erased columns from recovery rows
	int bitstride;
	u64 *bitmatrix = generate_bitmatrix(k, recovery, recovery_count, matrix,
										stride, erasures, bitstride);

	DLOG(print_matrix(bitmatrix, bitstride, recovery_count * 8);)

	// Finally, solving the matrix.
	// The most efficient approach is Gaussian elimination: An alternative
	// would be to recursively solve submatrices.  However, since the initial
	// matrix is sparse it is undesirable to add matrix rows together.
	// By working to put the matrix in upper-triangular form, the number of
	// row additions is reduced by about half.  And then a solution can be
	// immediately found without performing more row additions.

	// Gaussian elimination to put matrix in upper triangular form
	if (recovery_count > PRECOMP_TABLE_THRESH) {
		win_gaussian_elimination(recovery_count, recovery, bitmatrix, bitstride, subbytes, precomp_tables);

		// The matrix is now in an upper-triangular form, and can be worked from
		// right to left to conceptually produce an identity matrix.  The matrix
		// itself is not adjusted since the important result is the output values.

		DLOG(print_matrix(bitmatrix, bitstride, recovery_count * 8);)

		// Use back-substitution to solve value for each column
		win_back_substitution(recovery_count, recovery, bitmatrix, bitstride, subbytes, precomp_tables);
	} else {
		// Non-windowed version:
		gaussian_elimination(recovery_count, recovery, bitmatrix, bitstride, subbytes);

		DLOG(print_matrix(bitmatrix, bitstride, recovery_count * 8);)

		back_substitution(recovery_count, recovery, bitmatrix, bitstride, subbytes);
	}

	// Free temporary workspace
	delete []bitmatrix;
	if (dynamic_matrix) {
		delete []matrix;
	}
	if (precomp) {
		delete []precomp;
	}

	return 0;
}


//// Encoder

// Windowed version of encoder
static void win_encode(int k, int m, const u8 *matrix, int stride,
					   const u8 **data, u8 *out, int subbytes)
{
	static const int PRECOMP_TABLE_SIZE = 11;

	u8 *precomp = new u8[subbytes * PRECOMP_TABLE_SIZE * 2];
	u8 *table_stack[16 * 2] = {0};
	u8 **tables[2] = {
		table_stack, table_stack + 16
	};

	// Fill in tables
	u8 *precomp_ptr = precomp;
	for (int ii = 0; ii < 2; ++ii, precomp_ptr += subbytes * PRECOMP_TABLE_SIZE) {
		u8 **table = tables[ii];
		table[3] = precomp_ptr;
		table[5] = precomp_ptr + subbytes;
		table[6] = precomp_ptr + subbytes * 2;
		table[7] = precomp_ptr + subbytes * 3;
		for (int jj = 9; jj < 16; ++jj) {
			table[jj] = precomp_ptr + subbytes * (jj - 5);
		}
	}

	// For each column to generate,
	for (int x = 0; x < k; ++x, ++matrix) {
		const u8 *row = matrix;
		u8 *src = (u8 *)data[x]; // cast to fit table type

		// Fill in tables
		for (int ii = 0; ii < 2; ++ii, src += subbytes * 4) {
			u8 **table = tables[ii];

			table[1] = (u8 *)src;
			table[2] = (u8 *)src + subbytes;
			table[4] = (u8 *)src + subbytes * 2;
			table[8] = (u8 *)src + subbytes * 3;

			memxor_set(table[3], table[1], table[2], subbytes);
			memxor_set(table[6], table[2], table[4], subbytes);
			memxor_set(table[5], table[1], table[4], subbytes);
			memxor_set(table[7], table[1], table[6], subbytes);
			memxor_set(table[9], table[1], table[8], subbytes);
			memxor_set(table[12], table[4], table[8], subbytes);
			memxor_set(table[10], table[2], table[8], subbytes);
			memxor_set(table[11], table[3], table[8], subbytes);
			memxor_set(table[13], table[1], table[12], subbytes);
			memxor_set(table[14], table[2], table[12], subbytes);
			memxor_set(table[15], table[3], table[12], subbytes);
		}

		// For each of the rows,
		u8 *dest = out;
		for (int y = 1; y < m; ++y, row += stride) {
			u8 slice = row[0];

			// Generate 8x8 submatrix and XOR in bits as needed
			for (int bit_y = 0;; ++bit_y) {
				int low = slice & 15;
				int high = slice >> 4;

				// Add
				if (low && high) {
					memxor_add(dest, tables[0][low], tables[1][high], subbytes);
				} else if (low) {
					memxor(dest, tables[0][low], subbytes);
				} else {
					memxor(dest, tables[1][high], subbytes);
				}
				dest += subbytes;

				if (bit_y >= 7) {
					break;
				}

				slice = GFC256Multiply(slice, 2);
			}
		}
	}

	delete []precomp;
}

extern "C" int cauchy_256_encode(int k, int m, const u8 *data[],
								 void *vrecovery_blocks, int block_bytes)
{
	u8 *recovery_blocks = reinterpret_cast<u8 *>( vrecovery_blocks );

	// If only one input block,
	if (k <= 1) {
		// For each output block,
		for (int ii = 0; ii < m; ++ii, recovery_blocks += block_bytes) {
			// Copy it directly to output
			memcpy(recovery_blocks, data[0], block_bytes);
		}

		return 0;
	}

	// XOR all input blocks together
	memxor_set(recovery_blocks, data[0], data[1], block_bytes);

	for (int x = 2; x < k; ++x) {
		memxor(recovery_blocks, data[x], block_bytes);
	}

	// If only one recovery block needed,
	if (m == 1) {
		// We're already done!
		return 0;
	}

	// Otherwise there is a restriction on what inputs we can handle
	if ((k + m > 256) || (block_bytes % 8 != 0)) {
		return -1;
	}

	GFC256Init();

	// Generate Cauchy matrix
	int stride;
	u8 stack_space[CAT_CAUCHY_MATRIX_STACK_SIZE];
	bool dynamic_matrix;
	const u8 *matrix = cauchy_matrix(k, m, stride, stack_space, dynamic_matrix);

	// The first 8 rows of the bitmatrix are always the same, 8x8 identity
	// matrices all the way across.  So we don't even bother generating those
	// with a bitmatrix.  In fact the initial XOR for m=1 case has already
	// taken care of these bitmatrix rows.

	// Start on the second recovery block
	u8 *out = recovery_blocks + block_bytes;
	const int subbytes = block_bytes >> 3;

	// Clear output buffer
	memset(out, 0, block_bytes * (m - 1));

	// If the number of symbols to generate gets larger,
	if (m > 4) {
		// Start using a windowed approach to encoding
		win_encode(k, m, matrix, stride, data, out, subbytes);
	} else {
		const u8 *row = matrix;

		// For each remaining row to generate,
		for (int y = 1; y < m; ++y, row += stride, out += block_bytes) {
			const u8 *column = row;

			// For each symbol column,
			for (int x = 0; x < k; ++x, ++column) {
				const u8 *src = data[x];
				u8 slice = column[0];
				u8 *dest = out;

				DLOG(cout << "ENCODE: Using " << (int)slice << " at " << x << ", " << y << endl;)

				// Generate 8x8 submatrix and XOR in bits as needed
				for (int bit_y = 0;; ++bit_y) {
					const u8 *src_x = src;

					for (int bit_x = 0; bit_x < 8; ++bit_x, src_x += subbytes) {
						if (slice & (1 << bit_x)) {
							memxor(dest, src_x, subbytes);
						}
					}

					if (bit_y >= 7) {
						break;
					}

					slice = GFC256Multiply(slice, 2);
					dest += subbytes;
				}
			}
		}
	}

	if (dynamic_matrix) {
		delete []matrix;
	}

	return 0;
}

