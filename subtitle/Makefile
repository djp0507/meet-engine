include $(PLATFORM).mk

CFLAGS := $(CFLAGS) -Wall -I./libass -Wno-write-strings
CPPFLAGS := $(CPPFLAGS) -Wall -I./libass -Wno-write-strings

ifeq ($(DEBUG), y)
CFLAGS += -g
CPPFLAGS += -g
else
CFLAGS += -O2
CPPFLAGS += -O2
endif

SUBDIRS := enca libass libiconv

LIBS := libass/libass.a enca/libenca.a libiconv/libiconv.a

LDFLAGS = $(LIBS)

RM = rm

target = libsubtitle.a
CPPSOURCES = $(wildcard *.cpp)
CPPOBJS := $(subst .cpp,.o,$(CPPSOURCES))

CSOURCES = $(wildcard *.c)
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
