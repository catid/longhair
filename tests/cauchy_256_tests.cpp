#include "cauchy_256.h"

#include <iostream>
#include <iomanip>
#include <cassert>
using namespace std;

#include "AbyssinianPRNG.hpp"
#include "Clock.hpp"
using namespace cat;

static Clock m_clock;

/*
static void print(const void *data, int bytes) {
	const u8 *in = reinterpret_cast<const u8 *>( data );

	cout << hex;
	for (int ii = 0; ii < bytes; ++ii) {
		cout << (int)in[ii] << " ";
	}
	cout << dec << endl;
}
*/

int main() {
	m_clock.OnInitialize();

	cauchy_256_init();

	m_clock.usec();

	cout << "Cauchy matrix solver" << endl;

	int block_bytes = 8 * 162; // a multiple of 8

	cout << "Using " << block_bytes << " bytes per block (ie. packet/chunk size); must be a multiple of 8 bytes" << endl;

	Abyssinian prng;
	prng.Initialize(m_clock.msec(), Clock::cycles());

	for (int block_count = 2; block_count < 255; ++block_count) {
		for (int recovery_block_count = 2; recovery_block_count < (256 - block_count); ++recovery_block_count) {
			u8 *data = new u8[block_bytes * block_count];
			u8 *recovery_blocks = new u8[block_bytes * recovery_block_count];
			Block *blocks = new Block[block_count];

			for (int erasures_count = 1; erasures_count <= recovery_block_count && erasures_count <= block_count; ++erasures_count) {
				for (int ii = 0; ii < block_bytes * block_count; ++ii) {
					data[ii] = (u8)prng.Next();
				}

				double t0 = m_clock.usec();

				assert(!cauchy_256_encode(block_count, recovery_block_count, data, recovery_blocks, block_bytes));

				double t1 = m_clock.usec();
				double encode_time = t1 - t0;

				cout << "Encoded k=" << block_count << " data blocks with m=" << recovery_block_count << " recovery blocks in " << encode_time << " usec : " << (block_bytes * block_count / encode_time) << " MB/s" << endl;

				for (int ii = 0; ii < erasures_count; ++ii) {
					int erasure_index = recovery_block_count - ii - 1;
					blocks[ii].data = recovery_blocks + erasure_index * block_bytes;
					blocks[ii].row = block_count + erasure_index;
				}

				for (int ii = erasures_count; ii < block_count; ++ii) {
					blocks[ii].data = data + ii * block_bytes;
					blocks[ii].row = ii;
				}

				t0 = m_clock.usec();

				assert(!cauchy_256_decode(block_count, recovery_block_count, blocks, block_bytes));

				t1 = m_clock.usec();
				double decode_time = t1 - t0;

				cout << "+ Decoded " << erasures_count << " erasures in " << decode_time << " usec : " << (block_bytes * block_count / decode_time) << " MB/s" << endl;

				for (int ii = 0; ii < erasures_count; ++ii) {
					const u8 *orig = data + ii * block_bytes;
					assert(!memcmp(blocks[ii].data, orig, block_bytes));
				}
			}

			delete []data;
			delete []recovery_blocks;
			delete []blocks;
		}
	}

	m_clock.OnFinalize();

	return 0;
}

