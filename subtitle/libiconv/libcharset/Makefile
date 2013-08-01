include ../../$(PLATFORM).mk

CFLAGS := $(CFLAGS) -Wall -I./ -I./include -I../include -DLIBDIR -Wno-write-strings
CPPFLAGS := $(CPPFLAGS) -Wall -I. -Wno-write-strings

ifeq ($(PLATFORM), android)
CFLAGS := $(CFLAGS)  -I$(NDK)/sources/android/support/include
endif
ifeq ($(DEBUG), y)
CFLAGS += -g
CPPFLAGS += -g
else
CFLAGS += -O2
CPPFLAGS += -O2
endif


SUBDIRS != 

LDFLAGS = 

RM = rm

target = libcharset.a
CSOURCES = $(wildcard lib/*.c)
COBJS := $(subst .c,.o,$(CSOURCES))

OBJS = $(CPPOBJS) $(COBJS)

MAKE = make

all: $(target)


$(target): $(OBJS)
	@for dir in $(SUBDIRS); \
		do $(MAKE) -C $$dir all || exit 1;\
	done
	$(AR) $@ $(LDFLAGS) $^

clear:
	$(RM) -f $(OBJS)
	$(RM) -f $(target)
	@for dir in $(SUBDIRS); \
		do $(MAKE) -C $$dir clear || exit 1;\
	done

info:
	echo $(OBJS)
