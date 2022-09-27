
#include <fuse.h>
#include <errno.h>
#include "fuseimpl.h"
#include "httpfs.h"

std::pair<std::string, std::string> pathdecompose(std::string path) {
	auto p = path.find_last_of('/');
	if (p == std::string::npos)
		return std::make_pair("/", path);
	return std::make_pair(path.substr(0, p+1), path.substr(p+1));
}

int httpfs_open(const char *path, struct fuse_file_info *fi) {
	return 0;   // TODO check it exists?
}

int httpfs_getattr(const char *path, struct stat *st) {
	HttpFSServer *s = ((HttpFSServer*)fuse_get_context()->private_data);

	if (!strcmp(path, "/")) {
		memset(st, 0, sizeof(*st));
		st->st_mode = S_IRUSR | S_IRGRP | S_IFDIR;
		st->st_atime = 0;
		st->st_mtime = 0;
		st->st_ctime = 0;
		st->st_nlink = 1;
		st->st_uid = getuid();
		st->st_gid = getgid();
		return 0;
	}

	auto dirfile = pathdecompose(path);

	HttpFSServer::DirEntry entry;
	if (!s->readDir(dirfile.first, entry))
		return -EIO;

	// Check file in entries
	if (!entry.entries.count(dirfile.second))
		return -ENOENT;

	*st = entry.entries.at(dirfile.second);
	return 0;
}

int httpfs_read(const char *path, char *buf, size_t size,
                off_t offset, struct fuse_file_info *fi) {
	// Perform a GET query with partial content
	HttpFSServer *s = ((HttpFSServer*)fuse_get_context()->private_data);
	int ret = s->readBlock(path, buf, offset, size);
	if (ret < 0)
		return -EIO;
	return ret;
}

int httpfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info *fi) {
	// Perform GET query, and parse autoindex response
	HttpFSServer *s = ((HttpFSServer*)fuse_get_context()->private_data);

	HttpFSServer::DirEntry entry;
	if (!s->readDir(path, entry))
		return -EIO;

	for (auto it : entry.entries)
		filler(buf, it.first.c_str(), &it.second, 0);

	return 0;
}

int httpfs_write(const char *path, const char *buf, size_t size,
                 off_t offset, struct fuse_file_info *fi) {
	return -EACCES;   // Only read-only support
}

int httpfs_mkdir(const char *path, mode_t mode) {
	return -EACCES;   // Only read-only support
}

int httpfs_mknod(const char *path, mode_t mode, dev_t d) {
	return -EACCES;   // Only read-only support
}

int httpfs_unlink(const char *path) {
	return -EACCES;   // Only read-only support
}

int httpfs_rmdir(const char *path) {
	return -EACCES;   // Only read-only support
}

int httpfs_symlink(const char *target, const char *path) {
	return -EACCES;   // Only read-only support
}

int httpfs_rename(const char *oldp, const char *newp) {
	return -EACCES;   // Only read-only support
}

int httpfs_link(const char *target, const char *path) {
	return -EACCES;   // Only read-only support
}

int httpfs_chmod(const char *path, mode_t mode) {
	return -EACCES;   // Only read-only support
}

int httpfs_chown(const char *path, uid_t uid, gid_t gid) {
	return -EACCES;   // Only read-only support
}

int httpfs_truncate(const char *path, off_t offset) {
	return -EACCES;   // Only read-only support
}

int httpfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
	return -EACCES;   // Only read-only support
}

