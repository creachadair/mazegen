##
## Name:    Makefile
## Purpose: Makefile for maze generation library and driver
## Author:  M. J. Fromberger <http://www.dartmouth.edu/~sting/>
##
## Copyright (C) 1998, 2004 M. J. Fromberger, All Rights Reserved
##
## Permission is hereby granted, free of charge, to any person obtaining a copy
## of this software and associated documentation files (the "Software"), to
## deal in the Software without restriction, including without limitation the
## rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
## sell copies of the Software, and to permit persons to whom the Software is
## furnished to do so, subject to the following conditions:
## 
## The above copyright notice and this permission notice shall be included in
## all copies or substantial portions of the Software.
## 
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
## IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
## FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
## AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
## LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
## FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
## IN THE SOFTWARE.
## 

CC=gcc
CFLAGS=-Wall -O2 $(shell pkg-config --cflags gdlib)
LDFLAGS=$(shell pkg-config --libs gdlib)
LIBS=-lgd
TARGETS=mazegen

.PHONY: clean distclean dist

FILES=Makefile maze.h maze.c mazegen.c README
VERS=2.3

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

all default: $(TARGETS)

$(TARGETS):%: maze.o %.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	rm -f *~ *.o core

distclean: clean
	rm -f mazegen mazegen-$(VERS).zip

dist: distclean $(FILES)
	if [ -d maze/ ] ; then rm -rf maze/ ; fi
	mkdir maze/
	cp $(FILES) maze/
	zip -9r mazegen-$(VERS).zip maze/
	rm -rf maze/
	@ echo "<done>"

# Here there be dragons
