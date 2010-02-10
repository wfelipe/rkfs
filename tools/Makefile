CC = gcc
CFLAGS = -g -O0 -Wall -D__RKFS_DEBUG__ -D__RKFS_DUMP_DEBUG__
headers = mkrkfs.h rkfs.h utils.h
objects = mkrkfs.o dumprkfs.o utils.o

all: mkrkfs dumprkfs

.c.o:
	$(CC) $(CFLAGS) -c $<

mkrkfs: mkrkfs.o utils.o $(headers)
	$(CC) $(CFLAGS) -o mkrkfs mkrkfs.o utils.o

dumprkfs: dumprkfs.o utils.o $(headers)
	$(CC) $(CFLAGS) -o dumprkfs dumprkfs.o utils.o

$(objects): $(headers)

clean:
	rm -f $(objects) mkrkfs dumprkfs
