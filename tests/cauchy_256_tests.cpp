#include "cauchy_256.h"

#include <iostream>
#include <iomanip>
#include <cassert>
#include <fstream>
using namespace std;

#include "AbyssinianPRNG.hpp"
#include "Clock.hpp"
using namespace cat;

static Clock m_clock;

static void print(const void *data, int bytes) {
	const u8 *in = reinterpret_cast<const u8 *>( data );

	cout << hex;
	for (int ii = 0; ii < bytes; ++ii) {
		cout << (int)in[ii] << " ";
	}
	cout << dec << endl;
}


#define CAT_ENCODE_TIMES_ONLY
#define CAT_WORST_CASE_BENCHMARK
#define CAT_REASONABLE_RECOVERY_COUNT

// Jerasure wrapper for performance comparison:

#include "jerasure/jerasure.h"
#include "jerasure/cauchy.h"

int jerasure_encode(int k, int m, const void *data, void *recovery_blocks, int block_bytes) {
/*	// TODO: Pick the smallest w that works to take advantage of Jerasure's flexibility
	int w = 1;
	while ((1 << w) < (k + m)) {
		w <<= 1;
	}
*/
	static const int w = 6;

	int *matrix = cauchy_original_coding_matrix(k, m, w);
	if (matrix == NULL) {
		return -1;
	}

	cauchy_improve_coding_matrix(k, m, w, matrix);

	int *bitmatrix = jerasure_matrix_to_bitmatrix(k, m, w, matrix);

	char *inputs[256];
	const char *input_data = (const char *)data;
	for (int ii = 0; ii < k; ++ii) {
		inputs[ii] = (char*)input_data + ii * block_bytes;
	}

	char *coding[256];
	char *output_data = (char *)recovery_blocks;
	for (int ii = 0; ii < m; ++ii) {
		coding[ii] = output_data + ii * block_bytes;
	}

	// TODO: Better to not use smart?
	int **smart = jerasure_smart_bitmatrix_to_schedule(k, m, w, bitmatrix);

	jerasure_schedule_encode(k, m, w, smart, inputs, coding, block_bytes, block_bytes/w);

	return 0;
}

int jerasure_decode(int k, int m, Block *blocks, int block_bytes) {
	static const int w = 8;

	int *matrix = cauchy_original_coding_matrix(k, m, w);
	if (matrix == NULL) {
		return -1;
	}

	cauchy_improve_coding_matrix(k, m, w, matrix);

	int *bitmatrix = jerasure_matrix_to_bitmatrix(k, m, w, matrix);

	// TODO: Better to not use smart?
	int **smart = jerasure_smart_bitmatrix_to_schedule(k, m, w, bitmatrix);

	int *erasures = new int[k];
	char **inputs = new char *[k];
	char **coding = new char *[m];

	for (int ii = 0; ii < k; ++ii) {
		erasures[ii] = 0;
	}

	int erasure_index = 0;
	for (int ii = 0; ii < k; ++ii) {
		int row = blocks[ii].row;

		if (row < k) {
			erasures[row] = -1;
			inputs[row] = (char*)blocks[ii].data;
		} else {
			coding[erasure_index++] = (char*)blocks[ii].data;
		}
	}

	// Set up erasures array
	int erasure_count = 0;
	for (int ii = 0; ii < k; ++ii) {
		if (erasures[ii] == 0) {
			erasures[erasure_count++] = ii;
			inputs[ii] = coding[erasure_count++];
		}
	}
	erasures[erasure_count] = -1;

	// TODO: Better to not use smart?
	jerasure_schedule_decode_lazy(k, m, w, bitmatrix, erasures, inputs, coding, block_bytes, block_bytes/w, 1);

	return 0;
}

int main() {
	m_clock.OnInitialize();

	cauchy_256_init();

	m_clock.usec();

	cout << "Cauchy RS Codec Unit Tester" << endl;

	int block_bytes = 6 * 248; // a multiple of 6

	cout << "Using " << block_bytes << " bytes per block (ie. packet/chunk size); must be a multiple of 8 bytes" << endl;

	Abyssinian prng;
	prng.Initialize(m_clock.msec(), Clock::cycles());

	u8 heat_map[256 * 256] = { 0 };

	for (int block_count = 1; block_count < 256; ++block_count) {
#ifdef CAT_REASONABLE_RECOVERY_COUNT
		for (int recovery_block_count = 1; recovery_block_count <= (block_count / 2) && recovery_block_count < (256 - block_count); ++recovery_block_count) {
#else
		for (int recovery_block_count = 1; recovery_block_count < (256 - block_count); ++recovery_block_count) {
#endif
			u8 *data = new u8[block_bytes * block_count];
			u8 *recovery_blocks = new u8[block_bytes * recovery_block_count];
			Block *blocks = new Block[block_count];

			double sum_encode = 0;

			int erasures_count;
#ifdef CAT_WORST_CASE_BENCHMARK
			erasures_count = min(recovery_block_count, block_count); {
#else
# ifdef CAT_ENCODE_TIMES_ONLY
			for (erasures_count = 1; erasures_count <= 4 && erasures_count <= recovery_block_count && erasures_count <= block_count; ++erasures_count) {
# else
			for (erasures_count = 1; erasures_count <= recovery_block_count && erasures_count <= block_count; ++erasures_count) {
# endif
#endif
				for (int ii = 0; ii < block_bytes * block_count; ++ii) {
					data[ii] = (u8)prng.Next();
				}

				double t0 = m_clock.usec();

				assert(!jerasure_encode(block_count, recovery_block_count, data, recovery_blocks, block_bytes));

				double t1 = m_clock.usec();
				double encode_time = t1 - t0;
				sum_encode += encode_time;

				cout << "Encoded k=" << block_count << " data blocks with m=" << recovery_block_count << " recovery blocks in " << encode_time << " usec : " << (block_bytes * block_count / encode_time) << " MB/s" << endl;

#ifndef CAT_ENCODE_TIMES_ONLY
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

				assert(!jerasure_decode(block_count, recovery_block_count, blocks, block_bytes));

				t1 = m_clock.usec();
				double decode_time = t1 - t0;

				cout << "+ Decoded " << erasures_count << " erasures in " << decode_time << " usec : " << (block_bytes * block_count / decode_time) << " MB/s" << endl;
/*
				for (int ii = 0; ii < erasures_count; ++ii) {
					const u8 *orig = data + ii * block_bytes;
					print(orig, block_bytes / 8);
					print(blocks[ii].data, block_bytes / 8);
				}
*/
				for (int ii = 0; ii < erasures_count; ++ii) {
					const u8 *orig = data + ii * block_bytes;
					assert(!memcmp(blocks[ii].data, orig, block_bytes));
				}
#endif // CAT_ENCODE_TIMES_ONLY
			}

			double avg_encode = sum_encode / erasures_count;
			int speed = block_bytes * block_count / avg_encode;

			u8 map_value = 0;

			if (speed < 10) {
				map_value = 1;
			} else if (speed < 50) {
				map_value = 2;
			} else if (speed < 100) {
				map_value = 3;
			} else if (speed < 200) {
				map_value = 4;
			} else if (speed < 300) {
				map_value = 5;
			} else if (speed < 400) {
				map_value = 6;
			} else if (speed < 500) {
				map_value = 7;
			} else {
				map_value = 8;
			}

			heat_map[block_count * 256 + recovery_block_count] = map_value;

			delete []data;
			delete []recovery_blocks;
			delete []blocks;
		}
	}

	ofstream file;
	file.open("docs/heatmap.txt");

	for (int ii = 0; ii < 256; ++ii) {
		for (int jj = 0; jj < 256; ++jj) {
			u8 map_value = heat_map[ii * 256 + jj];

			file << (int)map_value << " ";
		}
		file << endl;
	}

	m_clock.OnFinalize();

	return 0;
}

