OBJECTS= dpmodel.o

#use this line for profiling
#PROFILEOPTION=-pg -g
#use this line for no profiling
PROFILEOPTION=

#note:
#the -Werror can be removed to compile even if there are warnings,
#this is used to ensure that all released versions are free of warnings.

#normal compile
OPTIMIZATIONS= -O6 -ffast-math -fexpensive-optimizations
CFLAGS= -MD -Wall -Werror $(OPTIMIZATIONS) $(PROFILEOPTION)
#debug compile
#OPTIMIZATIONS= -O -g
#CFLAGS= -MD -Wall -Werror -ggdb $(OPTIMIZATIONS) $(PROFILEOPTION)

LDFLAGS= -lm $(PROFILEOPTION)

all: dpmodel

.c.o:
	gcc $(CFLAGS) -c $*.c

dpmodel: $(OBJECTS)
	gcc -o $@ $^ $(LDFLAGS)

clean:
	-rm -f dpmodel *.o *.d

.PHONY: clean

-include *.d
