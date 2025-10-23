#include <app/config/Config.h>
#include <common/toolkit/Serialization.h>
#include <common/toolkit/SocketTk.h>
#include <common/toolkit/NicAddressFilter.h>


// Could maybe use this in other places...
typedef struct Reader Reader;
struct Reader
{
   const char *ptr;
   const char *end;
   bool good;
   int c; // current char
   char token[64]; // current token
   size_t tokensize;
};

static inline void reader_init(Reader *reader, const char *ptr, size_t len)
{
   memset(reader, 0, sizeof *reader);
   reader->ptr = ptr;
   reader->end = ptr + len;
   if (reader->ptr == reader->end)
   {
      reader->good = false;
      reader->c = 0;
   }
   else
   {
      reader->good = true;
      reader->c = *reader->ptr;
   }
}

static inline void nextchar(Reader *reader)
{
   ++ reader->ptr;
   if (reader->ptr == reader->end)
   {
      reader->good = false;
      reader->c = 0;
   }
   else
   {
      reader->c = *reader->ptr;
   }
}

static inline bool more(Reader *reader)
{
   while (reader->good && reader->c <= 0x20)
      nextchar(reader);
   return reader->good;
}







typedef struct NicAddressFilterEntry NicAddressFilterEntry;
struct NicAddressFilterEntry
{
   bool invert;  // prevent matches
   bool name_valid;
   bool addr_valid;
   bool family_valid;
   bool nicType_valid;

   char name[IFNAMSIZ];
   struct in6_addr addr;
   sa_family_t family;  // AF_INET or AF_INET6
   NicAddrType_t nicType;
};

typedef struct NicAddressFilter NicAddressFilter;
struct NicAddressFilter
{
   NicAddressFilterEntry* filterArray;
   size_t filterArrayLen;
};



/**
 * @return SIZE_MAX if not in the list (or explicitly forbidden)
 */
size_t NicAddressFilter_getPosition(NicAddressFilter* this, const NicAddress *nicAddr)
{
   sa_family_t family = ipv6_addr_v4mapped(&nicAddr->ipAddr) ? AF_INET : AF_INET6;
   size_t i;

   for (i = 0; i < this->filterArrayLen; i++)
   {
      NicAddressFilterEntry *entry = &this->filterArray[i];
      bool matches = true;

      if (entry->name_valid)
      {
         matches = matches && !strcmp(entry->name, nicAddr->name);
      }
      if (entry->addr_valid)
      {
         matches = matches && ipv6_addr_equal(&entry->addr, &nicAddr->ipAddr);
      }
      if (entry->family_valid)
      {
         matches = matches && (entry->family == family);
      }
      if (entry->nicType_valid)
      {
         matches = matches && (entry->nicType == nicAddr->nicType);
      }

      if (matches)
      {
         if (entry->invert)
         {
            return SIZE_MAX;
         }
         else
         {
            return i;
         }
      }
   }

   return SIZE_MAX;
}

/**
 * Note: Empty filter will always return allowed (i.e. "true").
 * Note: Will always allow the loopback address.
 */
bool NicAddressFilter_isAllowed(NicAddressFilter* this, const NicAddress *nicAddr)
{
   if(!this->filterArrayLen)
      return true;

   return NicAddressFilter_getPosition(this, nicAddr) < SIZE_MAX;
}

static bool parse_NicAddressFilterEntry(const char *line, size_t len, struct NicAddressFilterEntry *out)
{
   NicAddressFilterEntry filterEntry = {0};
   Reader *reader = &(Reader) {0};
   size_t column;

   reader_init(reader, line, len);

   // we have a bit of a weird lexical syntax.
   // Generally we split simply on whitespace boundaries which, while
   // not super flexible, is easy.
   // But at the start of the line we allow a '!' character which may
   // or may not be enclosed by whitespace

   if (more(reader) && reader->c == '!')
   {
      filterEntry.invert = true;
      nextchar(reader);
   }

   for (column = 0; more(reader); ++column)
   {
      // Split next token
      {
         reader->tokensize = 0;
         while (reader->good && reader->c > 0x20)
         {
            if (reader->tokensize >= sizeof reader->token - 1)
            {
               reader->token[reader->tokensize] = 0; // terminating zero
               printk_fhgfs(KERN_WARNING, "token too long: %s...\n", reader->token);
               return false;
            }
            reader->token[reader->tokensize++] = reader->c;
            nextchar(reader);
         }
         reader->token[reader->tokensize] = 0; // terminating zero
      }

      // "Parse" token
      {
         const char *token = reader->token;
         size_t tokensize = reader->tokensize;

         // each column can be '*' in which case any corresponding value matches
         if (!strcmp(token, "*"))
         {
            continue;
         }

         switch (column)
         {
         case 0:
            // Interface name
            {
               if (tokensize >= sizeof filterEntry.name - 1)
               {
                  printk_fhgfs(KERN_WARNING, "interface name too long: %s\n", token);
                  return false;
               }
               else if (strchr(token, '/'))
               {
                  printk_fhgfs(KERN_WARNING, "'/' character may not be part of an interface name (column 1): %s\n", token);
                  return false;
               }
               else
               {
                  scnprintf(filterEntry.name, sizeof filterEntry.name, "%s", token);
                  filterEntry.name_valid = true;
               }
               break;
            }
         case 1:
            // IP address. FIXME this is duplicated code
            {
               struct in_addr ipv4;
               struct in6_addr ipv6;

               if (in4_pton(token, tokensize, (void *) &ipv4.s_addr, '\0', NULL))
               {
                  filterEntry.addr = beegfs_mapped_ipv4(ipv4);
                  filterEntry.addr_valid = true;
               }
               else if (in6_pton(token, tokensize, (void *) ipv6.s6_addr, '\0', NULL))
               {
                  filterEntry.addr = ipv6;
                  filterEntry.addr_valid = true;
               }
               else
               {
                  printk_fhgfs(KERN_WARNING, "Failed to parse ip address in interfaces list: %s\n", reader->token);
                  return false;
               }
               break;
            }
         case 2:
            // Address family
            {
               if (!strcmp(token, "4"))
               {
                  filterEntry.family = AF_INET;
                  filterEntry.family_valid = true;
               }
               else if (!strcmp(token, "6"))
               {
                  filterEntry.family = AF_INET6;
                  filterEntry.family_valid = true;
               }
               else
               {
                  printk_fhgfs(KERN_WARNING, "Only '4' and '6' are valid address families (column 3): %s\n", token);
                  return false;
               }
               break;
            }
         case 3:
            // NICADDRTYPE
            {
               if (!strcmp(token, "tcp"))
               {
                  filterEntry.nicType = NICADDRTYPE_STANDARD;
                  filterEntry.nicType_valid = true;
               }
               else if (!strcmp(token, "rdma"))
               {
                  filterEntry.nicType = NICADDRTYPE_RDMA;
                  filterEntry.nicType_valid = true;
               }
               else
               {
                  printk_fhgfs(KERN_WARNING, "Only 'tcp' and 'rdma' are valid nic address types (column 4): %s\n", token);
                  return false;
               }
               break;
            }
         default:
            {
               printk_fhgfs(KERN_WARNING, "Invalid column %zu specified. There are currently 4 columns: "
                     "interface name (1), IP address (2), IP family (3), tcp/rdma (4)\n", column);
               return false;
            }
         }
      }
   }

   *out = filterEntry;
   return true;
}

/**
 * @param filename path to filter file.
 * @return false if a specified file could not be opened.
 */
NicAddressFilter* NicAddressFilter_construct(StrCpyList *filterList)
{
   NicAddressFilter *this = NULL;

   this = kmalloc(sizeof(*this), GFP_NOFS);
   if (! this)
      return NULL;

   this->filterArray = NULL;
   this->filterArrayLen = 0;

   if (StrCpyList_length(filterList))
   {
      // file did contain some lines
      StrCpyListIter iter;

      // list length => preallocate maximal length of final array
      this->filterArray = kmalloc(StrCpyList_length(filterList) * sizeof(NicAddressFilterEntry), GFP_NOFS);
      if (!this->filterArray)
         goto error;

      StrCpyListIter_init(&iter, filterList);
      for (; !StrCpyListIter_end(&iter); StrCpyListIter_next(&iter))
      {
         NicAddressFilterEntry entry = {0};
         char* line = StrCpyListIter_value(&iter);
         size_t len = strlen(line);

         if (! parse_NicAddressFilterEntry(line, len, &entry))
            goto error; // just give up. Configuration file should be without errors

         this->filterArray[this->filterArrayLen] = entry;
         this->filterArrayLen += 1;
      }
   }

out:
   return this;

error:
   if (this)
   {
      kfree(this->filterArray);
      this->filterArray = NULL;
      kfree(this);
      this = NULL;
   }
   goto out;
}

void NicAddressFilter_destruct(NicAddressFilter* this)
{
   SAFE_KFREE(this->filterArray);
   kfree(this);
}
