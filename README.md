
httpfs+
=======

httpfs+ is a small fuse-based filesystem driver over HTTP(s). It allows
users to mount a remote HTTP location locally (in a read only mode) using
(almost) no application on the server side.

How to use
----------

On the server side we use Nginx autoindex so that the server can read the
contents of all directories. This is rather simple to configure, for instance:

```
	location / {
		index .non-existent-file;
		autoindex on;
		autoindex_format json;
		root /path/of/interest;
	}
```

Unfortunately it seems it is not possible to disable the `index` module
(`ngx_http_index_module`), so we use some random file name that won't exist.
Then we enable autoindex in JSON mode.

Please note that this exposes all your files and directories to the world
(unless you configure some sort of authentication mechanism or firewall).

Once this is set-up, it is as simple as:

```
  ./httpfs --url=http://your.host:port/path/ /some/mountpoint
```

