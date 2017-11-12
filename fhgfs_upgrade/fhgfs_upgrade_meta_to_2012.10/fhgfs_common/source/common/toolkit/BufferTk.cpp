#include "BufferTk.h"

#define get16bits(d) (*((const uint16_t *) (d)))


/**
 * Note: This is the Hsieh hash function, which is available under old BSD-style license.
 * (It performs very well on x86 and PowerPC archs compared to other famous hash functions.)
 *
 * @data the buffer for which you want the hash value to be computed (arbitrary length)
 * @len length of the data buffer
 */
uint32_t BufferTk::hash32(const char* data, int len)
{
   uint32_t hash = len, tmp;
   int rem;

   if(unlikely(len <= 0 || data == NULL) )
      return 0;

   rem = len & 3;
   len >>= 2;

   /* Main loop */
   for(; len > 0; len--)
   {
      hash += get16bits(data);
      tmp = (get16bits(data+2) << 11) ^ hash;
      hash = (hash << 16) ^ tmp;
      data += 2 * sizeof(uint16_t);
      hash += hash >> 11;
   }

   /* Handle end cases */
   switch(rem)
   {
      case 3:
         hash += get16bits(data);
         hash ^= hash << 16;
         hash ^= data[sizeof(uint16_t)] << 18;
         hash += hash >> 11;
         break;
      case 2:
         hash += get16bits(data);
         hash ^= hash << 11;
         hash += hash >> 17;
         break;
      case 1:
         hash += *data;
         hash ^= hash << 10;
         hash += hash >> 1;
   }

   /* Force "avalanching" of final 127 bits */
   hash ^= hash << 3;
   hash += hash >> 5;
   hash ^= hash << 4;
   hash += hash >> 17;
   hash ^= hash << 25;
   hash += hash >> 6;

   return hash;
}
