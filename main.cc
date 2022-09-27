
#include <iostream>
#include <fuse.h>
#include "fuseimpl.h"
#include "httpfs.h"

static const struct fuse_operations operations = {
	.getattr   = httpfs_getattr,
	.mknod     = httpfs_mknod,
	.mkdir     = httpfs_mkdir,
	.unlink    = httpfs_unlink,
	.rmdir     = httpfs_rmdir,
	.symlink   = httpfs_symlink,
	.rename    = httpfs_rename,
	.link      = httpfs_link,
	.chmod     = httpfs_chmod,
	.chown     = httpfs_chown,
	.truncate  = httpfs_truncate,
	.open      = httpfs_open,
	.read      = httpfs_read,
	.write     = httpfs_write,
	.readdir   = httpfs_readdir,
	.create    = httpfs_create,
};

static struct options {
	const char *url;
	int meta_cache_ttl;
	int show_help;
} options;

#define OPTION(t, p)  { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
	OPTION("--url=%s", url),
	OPTION("--meta-cache-ttl=%d", meta_cache_ttl),
	OPTION("-h", show_help),
	OPTION("--help", show_help),
	FUSE_OPT_END
};


int main(int argc, char **argv) {
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	// Defaults
	options.url = NULL;
	options.meta_cache_ttl = 60;    // 1 minute is usually enough for most operations
	options.show_help = 0;

	if (fuse_opt_parse(&args, &options, option_spec, NULL) < 0)
		return 1;

	if (options.show_help) {
		printf("usage: %s [options] <mountpoint>\n\n", argv[0]);
		printf("File-system specific options:\n"
		       "    --url=<s>               URL of the HTTP(s) server\n"
		       "    --meta-cache-ttl=<d>    Metadata cache TTL (seconds)\n"
		       "\n");

		fuse_opt_add_arg(&args, "--help");
		return fuse_main(args.argc, args.argv, &operations, NULL);
	}

	if (!options.url) {
		printf("`url` is a required argument to mount a filesystem!\n");
		return 1;
	}

	HttpFSServer *serv = new HttpFSServer(options.url, options.meta_cache_ttl);

	int ret = fuse_main(args.argc, args.argv, &operations, serv);
	fuse_opt_free_args(&args);
	return ret;
}



