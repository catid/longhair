#include "cauchy_256.h"

#include <iostream>
#include <cassert>
using namespace std;

#include "AbyssinianPRNG.hpp"
#include "Clock.hpp"
using namespace cat;

static void print(const u8 *data, int bytes) {
	int sep = bytes / 8;
	for (int ii = 0; ii < bytes; ++ii) {
		if (ii % sep == 0) {
			cout << ": ";
		}
		cout << (int)data[ii] << " ";
	}
	cout << endl;
}

static void print_words(const u64 *row, int words) {
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

static void print_matrix(const u64 *matrix, int word_stride, int rows) {
	cout << "Printing matrix with " << word_stride << " words per row, and " << rows << " rows:" << endl;
	for (int ii = 0; ii < rows; ++ii) {
		print_words(matrix, word_stride);
		matrix += word_stride;
	}
}

int main() {
	m_clock.OnInitialize();

	cauchy_init();

	m_clock.usec();

	cout << "Cauchy matrix solver" << endl;

	int block_bytes = 8 * 162; // a multiple of 8
	int block_count = 2;
	int recovery_block_count = 2;

	u8 *data = new u8[block_bytes * block_count];
	u8 *recovery_blocks = new u8[block_bytes * recovery_block_count];

	Abyssinian prng;
	prng.Initialize(0);
	for (int ii = 0; ii < block_bytes * block_count; ++ii) {
		data[ii] = (u8)prng.Next();
	}

	double t0 = m_clock.usec();

	assert(cauchy_encode(block_count, recovery_block_count, data, recovery_blocks, block_bytes));

	double t1 = m_clock.usec();

	cout << "Cauchy encode in " << (t1 - t0) << " usec" << endl;

	Block *blocks = new Block[block_count];

	for (int ii = 0; ii < block_count; ++ii) {
		blocks[ii].data = data + ii * block_bytes;
		blocks[ii].row = ii;
	}

	// Erase first block
	const int erasures_count = 1;
	const int replace_row = 1;
	int original_remaining = block_count - erasures_count;
	blocks[replace_row].data = recovery_blocks + block_bytes;
	blocks[replace_row].row = block_count + 1;
	//blocks[0].data = recovery_blocks + block_bytes;
	//blocks[0].row = block_count + 1;
	int erasures[1] = {
		replace_row
	};

	t0 = m_clock.usec();

	assert(cauchy_decode(block_count, recovery_block_count, blocks, block_bytes));

	t1 = m_clock.usec();

	for (int ii = 0; ii < erasures_count; ++ii) {
		int erasure_index = erasures[ii];
		const u8 *orig = data + erasure_index * block_bytes;
		assert(!memcmp(blocks[erasure_index].data, orig, block_bytes));
	}

	cout << "Cauchy decode in " << (t1 - t0) << " usec" << endl;

	m_clock.OnFinalize();

	delete []data;
	delete []recovery_blocks;

	return 0;
}

