#ifndef OPEN_FHGFSTYPES_H_
#define OPEN_FHGFSTYPES_H_

#include <linux/in.h>
#include <linux/time.h>

#include <common/toolkit/Time.h>

struct fhgfs_stat
{
   umode_t mode;
   unsigned int nlink;
   uid_t uid;
   gid_t gid;
   loff_t size;
   uint64_t blocks;
   Time atime;
   Time mtime;
   Time ctime; // attrib change time (not creation time)
   unsigned int metaVersion;
};
typedef struct fhgfs_stat fhgfs_stat;



#endif /* OPEN_FHGFSTYPES_H_ */
