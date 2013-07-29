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

target = libenca.a
CSOURCES = $(wildcard lib/*.c)
COBJS := $(subst .c,.o,$(CSOURCES))

OBJS = $(CPPOBJS) $(COBJS)

MAKE = make

all: $(target)


$(target): $(OBJS)
	$(AR) $@ $(LDFLAGS) $^

clear:
	$(RM) -f $(OBJS)
	$(RM) -f $(target)

info:
	echo $(OBJS)
