##### Win32 variables #####

WIN32_EXE=dpmodel.exe
WIN32_LDFLAGS=-lm

##### Unix variables #####

UNIX_EXE=dpmodel
UNIX_LDFLAGS=-lm

##### Common variables #####

CC=gcc
CFLAGS=-MD -Wall -O2

ifdef windir
CMD_RM=del
else
CMD_RM=rm -f
endif

##### Commands #####

.PHONY: all mingw clean

all:
ifdef windir
	$(MAKE) EXE=$(WIN32_EXE) LDFLAGS="$(WIN32_LDFLAGS)" $(WIN32_EXE)
else
	$(MAKE) EXE=$(UNIX_EXE) LDFLAGS="$(UNIX_LDFLAGS)" $(UNIX_EXE)
endif

mingw:
	@$(MAKE) EXE=$(WIN32_EXE) LDFLAGS="$(WIN32_LDFLAGS)" $(WIN32_EXE)

.c.o:
	$(CC) $(CFLAGS) -c $*.c

$(EXE): dpmodel.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	-$(CMD_RM) $(WIN32_EXE)
	-$(CMD_RM) $(UNIX_EXE)
	-$(CMD_RM) *.o
