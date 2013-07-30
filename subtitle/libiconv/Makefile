include ../$(PLATFORM).mk

CFLAGS := $(CFLAGS) -Wall -I./libcharset/include -I./include -Wno-write-strings
CPPFLAGS := $(CPPFLAGS) -Wall -I. -Wno-write-strings

ifeq ($(PLATFORM), ios)
CFLAGS += -Wunused-function
endif

ifeq ($(DEBUG), y)
CFLAGS += -g
CPPFLAGS += -g
else
CFLAGS += -O2
CPPFLAGS += -O2
endif

SUBDIRS := libcharset
LDFLAGS = libcharset/libcharset.a

RM = rm

target = libiconv.a
CSOURCES = lib/genaliases.c lib/genaliases2.c lib/genflags.c lib/gentranslit.c lib/iconv.c
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
	@echo $(OBJS)
	@echo $(SUBDIRS)       
