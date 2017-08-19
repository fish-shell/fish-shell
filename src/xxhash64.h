// //////////////////////////////////////////////////////////
// xxhash64.h
// Copyright (c) 2016 Stephan Brumme. All rights reserved.
// see http://create.stephan-brumme.com/disclaimer.html
//
#pragma once
#include <stdint.h> // for uint32_t and uint64_t
/// XXHash (64 bit), based on Yann Collet's descriptions, see http://cyan4973.github.io/xxHash/
/** How to use:
    uint64_t myseed = 0;
    XXHash64 myhash(myseed);
    myhash.add(pointerToSomeBytes,     numberOfBytes);
    myhash.add(pointerToSomeMoreBytes, numberOfMoreBytes); // call add() as often as you like to ...
    // and compute hash:
    uint64_t result = myhash.hash();
    // or all of the above in one single line:
    uint64_t result2 = XXHash64::hash(mypointer, numBytes, myseed);
    Note: my code is NOT endian-aware !
**/
class XXHash64
{
public:
  /// create new XXHash (64 bit)
  /** @param seed your seed value, even zero is a valid seed **/
  explicit XXHash64(uint64_t seed)
  {
    state[0] = seed + Prime1 + Prime2;
    state[1] = seed + Prime2;
    state[2] = seed;
    state[3] = seed - Prime1;
    bufferSize  = 0;
    totalLength = 0;
  }
  /// add a chunk of bytes
  /** @param  input  pointer to a continuous block of data
      @param  length number of bytes
      @return false if parameters are invalid / zero **/
  bool add(const void* input, uint64_t length)
  {
    // no data ?
    if (!input || length == 0)
      return false;
    totalLength += length;
    // byte-wise access
    const unsigned char* data = (const unsigned char*)input;
    // unprocessed old data plus new data still fit in temporary buffer ?
    if (bufferSize + length < MaxBufferSize)
    {
      // just add new data
      while (length-- > 0)
        buffer[bufferSize++] = *data++;
      return true;
    }
    // point beyond last byte
    const unsigned char* stop      = data + length;
    const unsigned char* stopBlock = stop - MaxBufferSize;
    // some data left from previous update ?
    if (bufferSize > 0)
    {
      // make sure temporary buffer is full (16 bytes)
      while (bufferSize < MaxBufferSize)
        buffer[bufferSize++] = *data++;
      // process these 32 bytes (4x8)
      process(buffer, state[0], state[1], state[2], state[3]);
    }
    // copying state to local variables helps optimizer A LOT
    uint64_t s0 = state[0], s1 = state[1], s2 = state[2], s3 = state[3];
    // 32 bytes at once
    while (data <= stopBlock)
    {
      // local variables s0..s3 instead of state[0]..state[3] are much faster
      process(data, s0, s1, s2, s3);
      data += 32;
    }
    // copy back
    state[0] = s0; state[1] = s1; state[2] = s2; state[3] = s3;
    // copy remainder to temporary buffer
    bufferSize = stop - data;
    for (unsigned int i = 0; i < bufferSize; i++)
      buffer[i] = data[i];
    // done
    return true;
  }
  /// get current hash
  /** @return 64 bit XXHash **/
  uint64_t hash() const
  {
    // fold 256 bit state into one single 64 bit value
    uint64_t result;
    if (totalLength >= MaxBufferSize)
    {
      result = rotateLeft(state[0],  1) +
               rotateLeft(state[1],  7) +
               rotateLeft(state[2], 12) +
               rotateLeft(state[3], 18);
      result = (result ^ processSingle(0, state[0])) * Prime1 + Prime4;
      result = (result ^ processSingle(0, state[1])) * Prime1 + Prime4;
      result = (result ^ processSingle(0, state[2])) * Prime1 + Prime4;
      result = (result ^ processSingle(0, state[3])) * Prime1 + Prime4;
    }
    else
    {
      // internal state wasn't set in add(), therefore original seed is still stored in state2
      result = state[2] + Prime5;
    }
    result += totalLength;
    // process remaining bytes in temporary buffer
    const unsigned char* data = buffer;
    // point beyond last byte
    const unsigned char* stop = data + bufferSize;
    // at least 8 bytes left ? => eat 8 bytes per step
    for (; data + 8 <= stop; data += 8)
      result = rotateLeft(result ^ processSingle(0, *(uint64_t*)data), 27) * Prime1 + Prime4;
    // 4 bytes left ? => eat those
    if (data + 4 <= stop)
    {
      result = rotateLeft(result ^ (*(uint32_t*)data) * Prime1,   23) * Prime2 + Prime3;
      data  += 4;
    }
    // take care of remaining 0..3 bytes, eat 1 byte per step
    while (data != stop)
      result = rotateLeft(result ^ (*data++) * Prime5,            11) * Prime1;
    // mix bits
    result ^= result >> 33;
    result *= Prime2;
    result ^= result >> 29;
    result *= Prime3;
    result ^= result >> 32;
    return result;
  }
  /// combine constructor, add() and hash() in one static function (C style)
  /** @param  input  pointer to a continuous block of data
      @param  length number of bytes
      @param  seed your seed value, e.g. zero is a valid seed
      @return 64 bit XXHash **/
  static uint64_t hash(const void* input, uint64_t length, uint64_t seed)
  {
    XXHash64 hasher(seed);
    hasher.add(input, length);
      return hasher.hash();
  }
private:
  /// magic constants :-)
  static const uint64_t Prime1 = 11400714785074694791ULL;
  static const uint64_t Prime2 = 14029467366897019727ULL;
  static const uint64_t Prime3 =  1609587929392839161ULL;
  static const uint64_t Prime4 =  9650029242287828579ULL;
  static const uint64_t Prime5 =  2870177450012600261ULL;
  /// temporarily store up to 31 bytes between multiple add() calls
  static const uint64_t MaxBufferSize = 31+1;
  uint64_t      state[4];
  unsigned char buffer[MaxBufferSize];
  unsigned int  bufferSize;
  uint64_t      totalLength;
  /// rotate bits, should compile to a single CPU instruction (ROL)
  static inline uint64_t rotateLeft(uint64_t x, unsigned char bits)
  {
    return (x << bits) | (x >> (64 - bits));
  }
  /// process a single 64 bit value
  static inline uint64_t processSingle(uint64_t previous, uint64_t input)
  {
    return rotateLeft(previous + input * Prime2, 31) * Prime1;
  }
  /// process a block of 4x4 bytes, this is the main part of the XXHash32 algorithm
  static inline void process(const void* data, uint64_t& state0, uint64_t& state1, uint64_t& state2, uint64_t& state3)
  {
    const uint64_t* block = (const uint64_t*) data;
    state0 = processSingle(state0, block[0]);
    state1 = processSingle(state1, block[1]);
    state2 = processSingle(state2, block[2]);
    state3 = processSingle(state3, block[3]);
  }
};
