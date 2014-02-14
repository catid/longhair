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

#ifndef CAT_RAINBOW_HPP
#define CAT_RAINBOW_HPP

#ifdef __cplusplus
extern "C" {
#endif

#define RAINBOW_VERSION 1

#define RAINBOW_OVERHEAD 1

/*
 * Verify binary compatibility with the API on startup.
 *
 * Example:
 * 	if (!rainbow_init()) exit(1);
 *
 * Returns non-zero on success.
 * Returns 0 if the API level does not match.
 */
extern int _rainbow_init(int expected_version);
#define rainbow_init() _rainbow_init(RAINBOW_VERSION)

void (OnRainbowDecode *)(const void *data, int bytes);


//// Encoder

typedef struct _RainbowEncoder {
	char internal[128];
} RainbowEncoder;

/*
 * Allocates an encoder object.
 *
 * Max bytes is expected to be on the order of 1400 bytes.
 */
extern int rainbow_create_encoder(RainbowEncoder *encoder, int max_bytes, int memory_msec);

/*
 * Data pointer must remain valid.
 */
extern int rainbow_encode(RainbowEncoder *encoder, const void *data, int bytes, char overhead[RAINBOW_OVERHEAD]);

/*
 * Provides number of bytes written.
 */
extern int rainbow_encode_extra(RainbowEncoder *encoder, void *data, int *bytes);


//// Decoder

typedef struct _RainbowDecoder {
	char internal[128];
} RainbowDecoder;

/*
 * On decode function callback
 */
extern int rainbow_create_decoder(RainbowDecoder *decoder, int max_bytes, OnRainbowDecode on_decode);

/*
 * Provide original data to the decoder
 */
extern int rainbow_decode(RainbowDecoder *decoder, const void *data, int bytes, char overhead[RAINBOW_OVERHEAD]);

/*
 * Provide extra data to the decoder
 */
extern int rainbow_decode_extra(RainbowDecoder *decoder, const void *data, int bytes);


#ifdef __cplusplus
}
#endif

#endif // CAT_RAINBOW_HPP

