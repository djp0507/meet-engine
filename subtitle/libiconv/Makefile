include ../$(PLATFORM).mk

CFLAGS := $(CFLAGS) -Wall -I./libcharset/include -I./include -Wno-write-strings
CPPFLAGS := $(CPPFLAGS) -Wall -I. -Wno-write-strings

CFLAGS += -Wunused-function

ifeq ($(DEBUG), y)
CFLAGS += -g
CPPFLAGS += -g
else
CFLAGS += -O2
CPPFLAGS += -O2
endif

SUBDIRS := libcharset

ifeq ($(PLATFORM), android)
LDFLAGS = 
else
LDFLAGS = libcharset/libcharset.a
endif

RM = rm

target = libiconv.a
CSOURCES = lib/iconv.c
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
