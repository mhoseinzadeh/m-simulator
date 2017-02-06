##----------------------------------------------------------------------------
# Created with genmake.pl v1.1 on Fri Aug 10 14:56:45 2012
# genmake.pl home: http://muquit.com/muquit/software/
# Copryright: GNU GPL (http://www.gnu.org/copyleft/gpl.html)
##----------------------------------------------------------------------------
rm=/bin/rm -f
CC= g++
DEFS=  
PROGNAME= ../bin/msim-c
INCLUDES=  -I.
LIBS=


DEFINES= $(INCLUDES) $(DEFS) -DSYS_UNIX=1
CFLAGS= -g $(DEFINES)

SRCS = compile.cpp error.cpp evaluation.cpp exception.cpp fixedcodes.cpp functions.cpp main.cpp parser.cpp syntax.cpp variablelist.cpp 

OBJS = compile.o error.o evaluation.o exception.o fixedcodes.o functions.o main.o parser.o syntax.o variablelist.o 

.c.o:
	$(rm) $@
	$(CC) $(CFLAGS) -c $*.c

all: $(PROGNAME)

$(PROGNAME) : $(OBJS)
	$(CC) $(CFLAGS) -o $(PROGNAME) $(OBJS) $(LIBS)

clean:
	$(rm) $(OBJS) $(PROGNAME) core *~
