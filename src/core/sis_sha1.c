#include "sis_sha1.h"

static char _encoding_table[] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
  'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
  'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
  'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
  'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
  'w', 'x', 'y', 'z', '0', '1', '2', '3',
  '4', '5', '6', '7', '8', '9', '+', '/'
};
static int _mod_table[] = {0, 2, 1};

static inline const unsigned int _rol(const unsigned int value, const unsigned int steps)
{
  return ((value << steps) | (value >> (32 - steps)));
}

static inline void _clear_w_buffer(unsigned int* buffert)
{
  int pos = 0;
  for (pos = 16; --pos >= 0;)
  {
    buffert[pos] = 0;
  }
}

void _inner_hash(unsigned int* result, unsigned int* w)
{
  unsigned int a = result[0];
  unsigned int b = result[1];
  unsigned int c = result[2];
  unsigned int d = result[3];
  unsigned int e = result[4];
  int round = 0;
#define sha1macro(func,val) \
{ \
const unsigned int t = _rol(a, 5) + (func) + e + val + w[round]; \
e = d; \
d = c; \
c = _rol(b, 30); \
b = a; \
a = t; \
}
  while (round < 16)
  {
    sha1macro((b & c) | (~b & d), 0x5a827999)
    ++round;
  }
  while (round < 20)
  {
    w[round] = _rol((w[round - 3] ^ w[round - 8] ^ w[round - 14] ^ w[round - 16]), 1);
    sha1macro((b & c) | (~b & d), 0x5a827999)
    ++round;
  }
  while (round < 40)
  {
    w[round] = _rol((w[round - 3] ^ w[round - 8] ^ w[round - 14] ^ w[round - 16]), 1);
    sha1macro(b ^ c ^ d, 0x6ed9eba1)
    ++round;
  }
  while (round < 60)
  {
    w[round] = _rol((w[round - 3] ^ w[round - 8] ^ w[round - 14] ^ w[round - 16]), 1);
    sha1macro((b & c) | (b & d) | (c & d), 0x8f1bbcdc)
    ++round;
  }
  while (round < 80)
  {
    w[round] = _rol((w[round - 3] ^ w[round - 8] ^ w[round - 14] ^ w[round - 16]), 1);
    sha1macro(b ^ c ^ d, 0xca62c1d6)
    ++round;
  }
#undef sha1macro
  result[0] += a;
  result[1] += b;
  result[2] += c;
  result[3] += d;
  result[4] += e;
}

void sis_shacalc(const char* src, char* dest)
{
  unsigned char hash[20];
  int bytelength = strlen(src);
  unsigned int result[5] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0 };
  const unsigned char* sarray = (const unsigned char*) src;
  unsigned int w[80];
  const int endOfFullBlocks = bytelength - 64;
  int endCurrentBlock;
  int currentBlock = 0;
  while (currentBlock <= endOfFullBlocks)
  {
    endCurrentBlock = currentBlock + 64;
    int roundPos = 0;
    for (roundPos = 0; currentBlock < endCurrentBlock; currentBlock += 4)
    {
      w[roundPos++] = (unsigned int) sarray[currentBlock + 3]
        | (((unsigned int) sarray[currentBlock + 2]) << 8)
        | (((unsigned int) sarray[currentBlock + 1]) << 16)
        | (((unsigned int) sarray[currentBlock]) << 24);
    }
    _inner_hash(result, w);
  }
  endCurrentBlock = bytelength - currentBlock;
  _clear_w_buffer(w);
  int lastBlockBytes = 0;
  for (;lastBlockBytes < endCurrentBlock; ++lastBlockBytes)
  {
    w[lastBlockBytes >> 2] |= (unsigned int) sarray[lastBlockBytes + currentBlock] << ((3 - (lastBlockBytes & 3)) << 3);
  }
  w[lastBlockBytes >> 2] |= 0x80 << ((3 - (lastBlockBytes & 3)) << 3);
  if (endCurrentBlock >= 56)
  {
    _inner_hash(result, w);
    _clear_w_buffer(w);
  }
  w[15] = bytelength << 3;
  _inner_hash(result, w);
  int hashByte = 0;
  for (hashByte = 20; --hashByte >= 0;)
  {
    hash[hashByte] = (result[hashByte >> 2] >> (((3 - hashByte) & 0x3) << 3)) & 0xff;
  }
  int i = 0;
  int j = 0;
  for (i = 0, j = 0; i < 20;) {
    uint32_t octet_a = i < 20 ? hash[i++] : 0;
    uint32_t octet_b = i < 20 ? hash[i++] : 0;
    uint32_t octet_c = i < 20 ? hash[i++] : 0;
    uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
    dest[j++] = _encoding_table[(triple >> 3 * 6) & 0x3F];
    dest[j++] = _encoding_table[(triple >> 2 * 6) & 0x3F];
    dest[j++] = _encoding_table[(triple >> 1 * 6) & 0x3F];
    dest[j++] = _encoding_table[(triple >> 0 * 6) & 0x3F];
  }
  for (i = 0; i < _mod_table[20 % 3]; i++) {
    dest[28 - 1 - i] = '=';
  }
}
