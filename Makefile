# Copyright (C) 2018 alx@fastestcode.org
#  
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

PREFIX = /usr
MANDIR = $(PREFIX)/share/man
RESDIR = $(PREFIX)/share/X11/app-defaults

INCDIRS = -I/usr/local/include
LIBDIRS = -L/usr/local/lib

CFLAGS += -O2 -DPREFIX='"$(PREFIX)"' $(INCDIRS)
toolbox_libs =  -lXm -lXt -lX11
xmsm_libs = -lcrypt -lXm -lXt -lXss -lXinerama -lX11

toolbox_objs = tbmain.o tbparse.o
xmsm_objs = smmain.o

app_defaults = XmSm.ad XmToolbox.ad

executables = xmsm xmtoolbox xmsession

all: $(executables) $(app_defaults) 

xmtoolbox: $(toolbox_objs)
	$(CC) -o $@ $(LIBDIRS) $(toolbox_objs) $(toolbox_libs)

xmsm: $(xmsm_objs)
	$(CC) -o $@ $(LIBDIRS) $(xmsm_objs) $(xmsm_libs)

xmsession: xmsession.src
	sed s%PREFIX%$(PREFIX)%g xmsession.src > $@
	chmod 775 $@

XmSm.ad: XmSm.ad.src
	sed s%PREFIX%$(PREFIX)%g XmSm.ad.src > $@

XmToolbox.ad: XmToolbox.ad.src
	sed s%PREFIX%$(PREFIX)%g XmToolbox.ad.src > $@

install:
	install -m775 xmsession $(PREFIX)/bin/xmsession
	install -m775 xmtoolbox $(PREFIX)/bin/xmtoolbox
	install -m4775 xmsm $(PREFIX)/bin/xmsm
	install -m775 -d $(MANDIR)/man1
	install -m664 xmtoolbox.1 $(MANDIR)/man1/xmtoolbox.1
	install -m664 xmsm.1 $(MANDIR)/man1/xmsm.1
	install -m 775 -d $(RESDIR)
	if ! [ -f $(RESDIR)/XmSm ]; then \
	install -m664 XmSm.ad $(RESDIR)/XmSm; fi
	if ! [ -f $(RESDIR)/XmToolbox ]; then \
	install -m664 XmToolbox.ad $(RESDIR)/XmToolbox; fi

clean:
	-rm .depend $(toolbox_objs) $(xmsm_objs) $(executables) $(app_defaults)

.depend:
	$(CC) -MM $(INCDIRS) $(toolbox_objs:.o=.c) $(xmsm_objs:.o=.c) > $@

-include .depend
