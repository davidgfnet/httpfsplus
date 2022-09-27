
TARGET = httpfs
CFLAGS = -O2 -ggdb -Wall `pkg-config --cflags fuse`
LDFLAGS = -lcurl `pkg-config --libs fuse`
DEFS = -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=29

all:
	$(CXX) -o $(TARGET) fuseimpl.cc main.cc httpfs.cc $(DEFS) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(TARGET)

install:
	install -D $(TARGET) $(DESTDIR)$(bindir)/$(TARGET)

