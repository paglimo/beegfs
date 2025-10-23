#ifndef FHGFSXATTRHANDLERS_H_
#define FHGFSXATTRHANDLERS_H_

#include <linux/xattr.h>

int FhgfsXAttr_init_security(struct inode *inode, struct inode *dir, const struct qstr *qstr);

#if defined(KERNEL_HAS_CONST_XATTR_CONST_PTR_HANDLER)

extern const struct xattr_handler* const fhgfs_xattr_handlers_selinux[];
#ifdef KERNEL_HAS_GET_ACL
extern const struct xattr_handler* const fhgfs_xattr_handlers_acl[];
extern const struct xattr_handler* const fhgfs_xattr_handlers[];
#endif
extern const struct xattr_handler* const fhgfs_xattr_handlers_noacl[];

#elif defined(KERNEL_HAS_CONST_XATTR_HANDLER)

extern const struct xattr_handler* fhgfs_xattr_handlers_selinux[];
#ifdef KERNEL_HAS_GET_ACL
extern const struct xattr_handler* fhgfs_xattr_handlers_acl[];
extern const struct xattr_handler* fhgfs_xattr_handlers[];
#endif
extern const struct xattr_handler* fhgfs_xattr_handlers_noacl[];

#else

extern struct xattr_handler* fhgfs_xattr_handlers_selinux[];
#ifdef KERNEL_HAS_GET_ACL
extern struct xattr_handler* fhgfs_xattr_handlers_acl[];
extern struct xattr_handler* fhgfs_xattr_handlers[];
#endif
extern struct xattr_handler* fhgfs_xattr_handlers_noacl[];
#endif

/**
 * The get-function which is used for all the security.* xattrs.
 */
#if defined(KERNEL_HAS_XATTR_HANDLERS_INODE_ARG)
int FhgfsXAttr_getSecurity(const struct xattr_handler* handler, struct dentry* dentry,
      struct inode* inode, const char* name, void* value, size_t size);
#elif defined(KERNEL_HAS_XATTR_HANDLER_PTR_ARG)
int FhgfsXAttr_getSecurity(const struct xattr_handler* handler, struct dentry* dentry,
      const char* name, void* value, size_t size);
#endif

#endif /* FHGFSXATTRHANDLERS_H_ */
