/* $Id$ */
/** @file
 * vboxsf - VBox Linux Shared Folders VFS, operations for symbolic links.
 */

/*
 * Copyright (C) 2010-2019 Oracle Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "vfsmod.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 8) /* no generic_readlink() before 2.6.8 */

# if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0)
static const char *sf_follow_link(struct dentry *dentry, void **cookie)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
static void *sf_follow_link(struct dentry *dentry, struct nameidata *nd)
#else
static int sf_follow_link(struct dentry *dentry, struct nameidata *nd)
#  endif
{
	struct inode *inode = dentry->d_inode;
	struct vbsf_super_info *sf_g = VBSF_GET_SUPER_INFO(inode->i_sb);
	struct sf_inode_info *sf_i = GET_INODE_INFO(inode);
	int error = -ENOMEM;
	char *path = (char *)get_zeroed_page(GFP_KERNEL);
	int rc;

	if (path) {
		error = 0;
		rc = VbglR0SfReadLink(&client_handle, &sf_g->map, sf_i->path,
				      PATH_MAX, path);
		if (RT_FAILURE(rc)) {
			LogFunc(("VbglR0SfReadLink failed, caller=%s, rc=%Rrc\n", __func__, rc));
			free_page((unsigned long)path);
			error = -EPROTO;
		}
	}
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0)
	return error ? ERR_PTR(error) : (*cookie = path);
#  else
	nd_set_link(nd, error ? ERR_PTR(error) : path);
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
	return NULL;
# else
	return 0;
# endif
#endif
}

#  if LINUX_VERSION_CODE < KERNEL_VERSION(4, 2, 0)
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
static void sf_put_link(struct dentry *dentry, struct nameidata *nd,
			void *cookie)
#else
static void sf_put_link(struct dentry *dentry, struct nameidata *nd)
#endif
{
	char *page = nd_get_link(nd);
	if (!IS_ERR(page))
		free_page((unsigned long)page);
}
#  endif

# else  /* LINUX_VERSION_CODE >= 4.5.0 */
static const char *sf_get_link(struct dentry *dentry, struct inode *inode,
			       struct delayed_call *done)
{
	struct vbsf_super_info *sf_g = VBSF_GET_SUPER_INFO(inode->i_sb);
	struct sf_inode_info *sf_i = GET_INODE_INFO(inode);
	char *path;
	int rc;

	if (!dentry)
		return ERR_PTR(-ECHILD);
	path = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!path)
		return ERR_PTR(-ENOMEM);
	rc = VbglR0SfReadLink(&client_handle, &sf_g->map, sf_i->path, PATH_MAX,
			      path);
	if (RT_FAILURE(rc)) {
		LogFunc(("VbglR0SfReadLink failed, caller=%s, rc=%Rrc\n",
			 __func__, rc));
		kfree(path);
		return ERR_PTR(-EPROTO);
	}
	set_delayed_call(done, kfree_link, path);
	return path;
}
# endif /* LINUX_VERSION_CODE >= 4.5.0 */

struct inode_operations sf_lnk_iops = {
# if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)
	.readlink = generic_readlink,
# endif
# if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	.get_link = sf_get_link
# elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0)
	.follow_link = sf_follow_link,
	.put_link = free_page_put_link,
# else
	.follow_link = sf_follow_link,
	.put_link = sf_put_link
# endif
};

#endif	/* LINUX_VERSION_CODE >= 2.6.8 */
