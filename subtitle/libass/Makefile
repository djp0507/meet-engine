include ../$(PLATFORM).mk

CFLAGS := $(CFLAGS) -Wall -I./libass -Wno-write-strings
CPPFLAGS := $(CPPFLAGS) -Wall -I. -I../enca/lib -Wno-write-strings

ifeq ($(DEBUG), y)
CFLAGS += -g
CPPFLAGS += -g
else
CFLAGS += -O2
CPPFLAGS += -O2
endif

RM = rm

target = libass.a
CSOURCES = libass/ass.c libass/ass_library.c libass/libass_glue.c
COBJS := $(subst .c,.o,$(CSOURCES))

OBJS = $(CPPOBJS) $(COBJS)

MAKE = make

all: $(target)


$(target): $(OBJS)
	for dir in $(SUBDIRS); \
		do $(MAKE) -C $$dir all || exit 1;\
	done
	$(AR) $@ $(LDFLAGS) $^

clear:
	$(RM) -f $(OBJS)
	$(RM) -f $(target)

info:
	echo $(OBJS)
