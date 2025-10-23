#include <app/config/Config.h>
#include <common/toolkit/Serialization.h>
#include <common/toolkit/SocketTk.h>
#include <common/toolkit/NetFilter.h>

NetFilter* NetFilter_construct(const char* filename)
{
   NetFilter* this = kmalloc(sizeof(*this), GFP_NOFS);

   if(!this ||
      !NetFilter_init(this, filename) )
   {
      kfree(this);
      return NULL;
   }

   return this;
}

void NetFilter_uninit(NetFilter* this)
{
   SAFE_KFREE(this->filterArray);
}

void NetFilter_destruct(NetFilter* this)
{
   NetFilter_uninit(this);

   kfree(this);
}

/**
 * Note: Empty filter will always return allowed (i.e. "true").
 * Note: Will always allow the loopback address.
 */
bool NetFilter_isAllowed(NetFilter* this, struct in6_addr ipAddr)
{
   if(!this->filterArrayLen)
      return true;

   return NetFilter_isContained(this, ipAddr);
}

static struct in6_addr beegfs_mask_in6_addr(struct in6_addr a, struct in6_addr b)
{
   a.s6_addr32[0] &= b.s6_addr32[0];
   a.s6_addr32[1] &= b.s6_addr32[1];
   a.s6_addr32[2] &= b.s6_addr32[2];
   a.s6_addr32[3] &= b.s6_addr32[3];
   return a;
}

static struct in6_addr beegfs_make_in6_addr_mask_from_prefix(int prefix_len)
{
   struct in6_addr mask = {0};
   BUG_ON(! (0 <= prefix_len && prefix_len <= 128));
   for (int i = 0; i < 16; i++)
   {
      if (prefix_len < 8 * i)
         mask.s6_addr[i] = 0x00;
      else if (prefix_len >= 8 * (i + 1))
         mask.s6_addr[i] = 0xff;
      else
         mask.s6_addr[i] = 0xff << (8 * (i + 1) - prefix_len);
   }
   return mask;
}

/**
 * Note: Always implicitly contains the loopback address.
 */
bool NetFilter_isContained(NetFilter* this, struct in6_addr addr)
{
   size_t i;

   if (ipv6_addr_v4mapped(&addr) && addr.s6_addr32[3] == htonl(INADDR_LOOPBACK))
      return true;

   if (ipv6_addr_equal(&addr, &in6addr_loopback))
      return true;

   for (i = 0; i < this->filterArrayLen; i++)
   {
      struct in6_addr mask = this->filterArray[i].mask;
      struct in6_addr compare = this->filterArray[i].compare;
      struct in6_addr masked = beegfs_mask_in6_addr(addr, mask);
      if (ipv6_addr_equal(&masked, &compare))
         return true;
   }

   return false;
}

static bool parse_NetFilterEntry(const char *line, size_t len, struct NetFilterEntry *out)
{
   // TODO we should probably centralize this parsing code
   struct in_addr ipv4;
   struct in6_addr ipv6;
   const char *slash = NULL;
   u8 prefix_len;
   bool isv4;

   if (in4_pton(line, len, (void *) &ipv4.s_addr, '/', &slash))
   {
      isv4 = true;
   }
   else if (in6_pton(line, len, (void *) ipv6.s6_addr, '/', &slash))
   {
      isv4 = false;
   }
   else
   {
      printk_fhgfs(KERN_WARNING, "Failed to parse ip address in net filter file: %s\n", line);
      goto parsefail;
   }

   if (kstrtou8(slash + 1, 10, &prefix_len))
   {
      printk_fhgfs(KERN_WARNING, "Failed to parse prefix length in net filter file: %s\n", line);
      goto parsefail;
   }

   if (isv4 && prefix_len > 32)
   {
      printk_fhgfs(KERN_WARNING, "Invalid prefix length in net filter file: %s, (max prefix length allowed for IPv4 address: 32, got: %u)\n", line, (unsigned) prefix_len);
      goto parsefail;
   }

   if (! isv4 && prefix_len > 128)
   {
      printk_fhgfs(KERN_WARNING, "Invalid prefix length in net filter file: %s, (max prefix length allowed for IPv6 address: 128, got: %u)\n", line, (unsigned) prefix_len);
      goto parsefail;
   }

   // Push new filter list entry
   {
      struct in6_addr addr;
      struct in6_addr mask;
      if (isv4)
      {
         // IPv4 addrs have 32 bits. Add 96 bits in front to get 128 IPv6 bits.
         addr = beegfs_mapped_ipv4(ipv4);
         mask = beegfs_make_in6_addr_mask_from_prefix(prefix_len + 96);
      }
      else
      {
         addr = ipv6;
         mask = beegfs_make_in6_addr_mask_from_prefix(prefix_len);
      }

      *out = (NetFilterEntry) {0};
      out->mask = mask;
      out->compare = beegfs_mask_in6_addr(addr, mask);
   }

   return true;

parsefail:
   return false;
}

/**
 * @param filename path to filter file.
 * @return false if a specified file could not be opened.
 */
static bool __NetFilter_prepareArray(NetFilter* this, const char* filename)
{
   bool loadRes = true;
   StrCpyList filterList;

   this->filterArray = NULL;
   this->filterArrayLen = 0;

   if(!StringTk_hasLength(filename) )
   { // no file specified => no filter entries
      return true;
   }


   StrCpyList_init(&filterList);

   loadRes = Config_loadStringListFile(filename, &filterList);
   if(!loadRes)
   { // file error
      printk_fhgfs(KERN_WARNING,
         "Unable to load configured net filter file: %s\n", filename);
      return false;
   }

   if(StrCpyList_length(&filterList) )
   { // file did contain some lines
      StrCpyListIter iter;

      // list length => preallocate maximal length of final array
      this->filterArray = kmalloc(StrCpyList_length(&filterList) * sizeof(NetFilterEntry), GFP_NOFS);
      if(!this->filterArray)
         goto err_alloc;
      this->filterArrayLen = 0;

      StrCpyListIter_init(&iter, &filterList);

      for( ; !StrCpyListIter_end(&iter); StrCpyListIter_next(&iter) )
      {
         const char* line = StrCpyListIter_value(&iter);
         size_t len = strlen(line);
         NetFilterEntry entry;

         if (parse_NetFilterEntry(line, len, &entry))
         {
            this->filterArray[this->filterArrayLen] = entry;
            this->filterArrayLen += 1;
         }
      }
   }

   StrCpyList_uninit(&filterList);
   return true;

err_alloc:
   StrCpyList_uninit(&filterList);
   return false;
}

/**
 * @param filename path to filter file.
 */
bool NetFilter_init(NetFilter* this, const char* filename)
{
   return __NetFilter_prepareArray(this, filename);
}
