# Hurd unionfs
# Copyright (C) 2001, 2002 Free Software Foundation, Inc.
# Written by Moritz Schulte <moritz@duesseldorf.ccc.de>.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or *
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.

CFLAGS += -Wall -g -D_FILE_OFFSET_BITS=64 -std=gnu99 \
	  -DDEBUG
LDFLAGS += -lnetfs -lfshelp -liohelp -lthreads \
           -lports -lihash -lshouldbeinlibc

all: unionfs

unionfs: main.o node.o lnode.o ulfs.o ncache.o netfs.o \
	 lib.o options.o
	$(CC) -o $@ main.o node.o lnode.o ulfs.o options.o \
	  ncache.o netfs.o lib.o $(LDFLAGS)

unionfs.static: main.o node.o lnode.o ulfs.o ncache.o netfs.o \
		lib.o options.o
	$(CC) -static -o $@ main.o node.o lnode.o ulfs.o \
	  ncache.o netfs.o lib.o options.o $(LDFLAGS)

main.o: main.c
	$(CC) $(CFLAGS) -c $<

node.o: node.c
	$(CC) $(CFLAGS) -c $<

lnode.o: lnode.c
	$(CC) $(CFLAGS) -c $<

ulfs.o: ulfs.c
	$(CC) $(CFLAGS) -c $<

ncache.o: ncache.c
	$(CC) $(CFLAGS) -c $<

netfs.o: netfs.c
	$(CC) $(CFLAGS) -c $<

lib.o: lib.c
	$(CC) $(CFLAGS) -c $<

options.o: options.c
	$(CC) $(CFLAGS) -c $<

.PHONY: clean

clean:
	rm -rf *.o unionfs
