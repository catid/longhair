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



#### Credits

This software was written entirely by myself ( Christopher A. Taylor <mrcatid@gmail.com> ).  If you
find it useful and would like to buy me a coffee, consider [tipping](https://www.gittip.com/catid/).

