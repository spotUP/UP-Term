/* be32: big-endian 32-bit read and write on a byte pointer, portable C89.
 * IFF chunk sizes, prefs files and cache records all hold longs this way;
 * one copy here, byte-wise so alignment and host endianness never matter. */
#ifndef BE32_H
#define BE32_H

void be32_put(unsigned char *p, unsigned long v);
unsigned long be32_get(const unsigned char *p);

#endif
