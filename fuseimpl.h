
#include <fuse.h>

int httpfs_open(const char *path, struct fuse_file_info *fi);
int httpfs_getattr(const char *path, struct stat *st);
int httpfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int httpfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
int httpfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int httpfs_mkdir(const char *path, mode_t mode);
int httpfs_mknod(const char *path, mode_t mode, dev_t d);
int httpfs_unlink(const char *path);
int httpfs_rmdir(const char *path);
int httpfs_symlink(const char *target, const char *path);
int httpfs_rename(const char *oldp, const char *newp);
int httpfs_link(const char *target, const char *path);
int httpfs_chmod(const char *path, mode_t mode);
int httpfs_chown(const char *path, uid_t uid, gid_t gid);
int httpfs_truncate(const char *path, off_t offset);
int httpfs_create(const char *path, mode_t mode, struct fuse_file_info *fi);

