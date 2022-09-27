
CFLAGS = -O2 -ggdb -Wall
LDFLAGS = -lcurl -lfuse
DEFS = -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=29

all:
	g++ -o httpfs fuseimpl.cc main.cc httpfs.cc $(DEFS) $(CFLAGS) $(LDFLAGS)

