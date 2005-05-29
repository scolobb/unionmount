# Hurd unionfs
# Copyright (C) 2001, 2002, 2003, 2005 Free Software Foundation, Inc.
# Written by Jeroen Dekkers <jeroen@dekkers.cx>.
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

CPP = gcc -E -x c
MIGCOM = mig -cc cat - /dev/null

CFLAGS += -Wall -g -O2 -D_FILE_OFFSET_BITS=64 -std=gnu99 \
	  -DDEBUG
LDFLAGS += -lnetfs -lfshelp -liohelp -lthreads \
           -lports -lihash -lshouldbeinlibc
OBJS = main.o node.o lnode.o ulfs.o ncache.o netfs.o \
       lib.o options.o pattern.o stow.o update.o

MIGCOMSFLAGS = -prefix stow_
fs_notify-MIGSFLAGS = -imacros ./stow-mutations.h


# How to build RPC stubs

# We always need this setting, because libc does not include the bogus names.
MIGCOMFLAGS := -subrprefix __

# User settable variables:
#	mig-sheader-prefix prepend to foo_S.h for name of foo.defs stub header
# 	MIGSFLAGS	   flags to CPP when building server stubs and headers
#	foo-MIGSFLAGS	   same, but only for interface `foo'
# 	MIGCOMSFLAGS	   flags to MiG when building server stubs and headers
#	foo-MIGCOMSFLAGS   same, but only for interface `foo'
# 	MIGUFLAGS	   flags to CPP when building user stubs and headers
#	foo-MIGUFLAGS	   same, but only for interface `foo'
# 	MIGCOMUFLAGS	   flags to MiG when building user stubs and headers
#	foo-MIGCOMUFLAGS   same, but only for interface `foo'
#	CPPFLAGS	   flags to CPP

# Implicit rules for building server and user stubs from mig .defs files.

# These chained rules could be (and used to be) single rules using pipes.
# But it's convenient to be able to explicitly make the intermediate
# files when you want to deal with a problem in the MiG stub generator.
$(mig-sheader-prefix)%_S.h %Server.c: %.sdefsi
	$(MIGCOM) $(MIGCOMFLAGS) $(MIGCOMSFLAGS) $($*-MIGCOMSFLAGS) \
		    -sheader $(mig-sheader-prefix)$*_S.h -server $*Server.c \
		    -user /dev/null -header /dev/null < $<

%.sdefsi: %.defs
	$(CPP) $(CPPFLAGS) $(MIGSFLAGS) $($*-MIGSFLAGS) -DSERVERPREFIX=S_ $< -o $@

vpath %.defs $(prefix)/include/hurd



all: unionfs

unionfs: $(OBJS) fs_notifyServer.o
	$(CC) -o $@ $(OBJS) fs_notifyServer.o $(LDFLAGS)

unionfs.static: $(OBJS) fs_notifyServer.o
	$(CC) -static -o $@ $(OBJS) fs_notifyServer.o $(LDFLAGS)

fs_notifyServer.o: fs_notifyServer.c

.PHONY: clean

clean:
	rm -rf *.o fs_notifyServer.c fs_notify_S.h unionfs
