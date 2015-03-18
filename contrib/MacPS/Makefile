# Copyright (c) 1988, The Regents of the University of California.
# Edward Moy, Workstation Software Support Group, Workstation Support Serices,
# Information Systems and Technology.
#
# Permission is granted to any individual or institution to use, copy,
# or redistribute this software so long as it is not sold for profit,
# provided that this notice and the original copyright notices are
# retained.  The University of California makes no representations about the
# suitability of this software for any purpose.  It is provided "as is"
# without express or implied warranty.
#
# SCCSid = "@(#)Makefile	2.2 10/24/89"
#
# For System V, include -DSYSV in CFLAGS below.
# To specify a fixed path for macps.config, include -DCONFIGDIR=\"path\"
# in CFLAGS below (path is the full pathname of the directory).
# To do save/restore context, include -DSAVE in CFLAGS below.
#
CFLAGS = -O
MACPS = macps.o macaux.o ucbwhich.o
PREPFIX = prepfix.o macaux.o

all : macps prepfix

macps : $(MACPS)
	cc -o macps $(CFLAGS) $(MACPS)

prepfix : $(PREPFIX)
	cc -o prepfix $(CFLAGS) $(PREPFIX)

clean :
	/bin/rm -f *.o macps prepfix

spotless :
	/bin/rm -f *.o *.orig macps prepfix
