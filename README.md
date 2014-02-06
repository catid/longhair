# Longhair
## Fast Cauchy Reed-Solomon Packet Error Correction Codes

For most small packetized data under 32 packets, the most efficient error correction
method is provided by the Cauchy Reed-Solomon (CRS) approach.  If the data is split
into more than 32 pieces, the best option available is [Wirehair](https://github.com/catid/wirehair).

The [Jerasure](https://github.com/tsuraan/Jerasure) library provides an
implementation of CRS codes that is optimized for large storage applications,
but it is not tuned for error correction of network packets.  This library
improves on Jerasure's performance for this particular use case.

A simple C API is provided to make it easy to incorporate into existing
projects.


## TODO

This is a work in progress.  What is here should be 100% working for k (number of packets) < 256.

The final version of this library will also have an optimized API for k < 64,
which should be quite a bit faster.  In practice I will only be using it for
k < 32, and switching to Wirehair for k >= 32.


## Benchmarks

For small k this thing is ridiculously fast on my laptop:

~~~
Using 1296 bytes per block (ie. packet/chunk size); must be a multiple of 8 bytes
Encoded k=9 data blocks with m=2 recovery blocks in 11 usec : 1060.36 MB/s
+ Decoded 1 erasures in 5 usec : 2332.8 MB/s
Encoded k=9 data blocks with m=2 recovery blocks in 4 usec : 2916 MB/s
+ Decoded 2 erasures in 8 usec : 1458 MB/s
Encoded k=9 data blocks with m=3 recovery blocks in 12 usec : 972 MB/s
+ Decoded 1 erasures in 2 usec : 5832 MB/s
Encoded k=9 data blocks with m=3 recovery blocks in 9 usec : 1296 MB/s
+ Decoded 2 erasures in 8 usec : 1458 MB/s
Encoded k=9 data blocks with m=3 recovery blocks in 8 usec : 1458 MB/s
+ Decoded 3 erasures in 14 usec : 833.143 MB/s
Encoded k=9 data blocks with m=4 recovery blocks in 28 usec : 416.571 MB/s
+ Decoded 1 erasures in 2 usec : 5832 MB/s
Encoded k=9 data blocks with m=4 recovery blocks in 12 usec : 972 MB/s
+ Decoded 2 erasures in 8 usec : 1458 MB/s
Encoded k=9 data blocks with m=4 recovery blocks in 12 usec : 972 MB/s
+ Decoded 3 erasures in 15 usec : 777.6 MB/s
Encoded k=9 data blocks with m=4 recovery blocks in 11 usec : 1060.36 MB/s
+ Decoded 4 erasures in 21 usec : 555.429 MB/s
Encoded k=9 data blocks with m=5 recovery blocks in 19 usec : 613.895 MB/s
+ Decoded 1 erasures in 1 usec : 11664 MB/s
Encoded k=9 data blocks with m=5 recovery blocks in 17 usec : 686.118 MB/s
+ Decoded 2 erasures in 8 usec : 1458 MB/s
Encoded k=9 data blocks with m=5 recovery blocks in 17 usec : 686.118 MB/s
+ Decoded 3 erasures in 15 usec : 777.6 MB/s
Encoded k=9 data blocks with m=5 recovery blocks in 16 usec : 729 MB/s
+ Decoded 4 erasures in 23 usec : 507.13 MB/s
Encoded k=9 data blocks with m=5 recovery blocks in 16 usec : 729 MB/s
+ Decoded 5 erasures in 32 usec : 364.5 MB/s
Encoded k=9 data blocks with m=6 recovery blocks in 28 usec : 416.571 MB/s
+ Decoded 1 erasures in 1 usec : 11664 MB/s
Encoded k=9 data blocks with m=6 recovery blocks in 22 usec : 530.182 MB/s
+ Decoded 2 erasures in 8 usec : 1458 MB/s
Encoded k=9 data blocks with m=6 recovery blocks in 21 usec : 555.429 MB/s
+ Decoded 3 erasures in 15 usec : 777.6 MB/s
Encoded k=9 data blocks with m=6 recovery blocks in 22 usec : 530.182 MB/s
+ Decoded 4 erasures in 22 usec : 530.182 MB/s
Encoded k=9 data blocks with m=6 recovery blocks in 21 usec : 555.429 MB/s
+ Decoded 5 erasures in 32 usec : 364.5 MB/s
Encoded k=9 data blocks with m=6 recovery blocks in 21 usec : 555.429 MB/s
+ Decoded 6 erasures in 43 usec : 271.256 MB/s
~~~


## Encoder speed discussion

I found that the encoder speed is directly related to the number of error
correction blocks and doesn't depend on the amount of data to protect:

![alt text](https://github.com/catid/longhair/raw/master/docs/EncoderSpeed.png "Speed of Encoder for k, m")

The rows are values of k (amount of data) and the columns are values of m (number of recovery blocks added).

Darker is better.  The main point of this plot is to just show that k doesn't
factor into the performance of the code.

This means that if the number of recovery blocks to generate is small, then it
may be worthwhile using the GF(256) version of the codec even for larger amounts
of data.  The break-even point for performance is around m < 20 symbols, where
the simpler CRS codes start performing better than Wirehair for encoding.


#### Credits

This software was written entirely by myself ( Christopher A. Taylor <mrcatid@gmail.com> ).  If you
find it useful and would like to buy me a coffee, consider [tipping](https://www.gittip.com/catid/).

