SYSROOT=$(NDK)/platforms/android-9/arch-arm
TOOLCHAINS_BASE=$(NDK)/toolchains/arm-linux-androideabi-4.6/prebuilt/$(HOST_TAG)/bin
CC=$(TOOLCHAINS_BASE)/arm-linux-androideabi-gcc --sysroot=$(SYSROOT) 
CXX=$(TOOLCHAINS_BASE)/arm-linux-androideabi-g++ --sysroot=$(SYSROOT) 
AR=$(TOOLCHAINS_BASE)/arm-linux-androideabi-ar crs
CFLAGS= -fpic -ffunction-sections -funwind-tables -fstack-protector -no-canonical-prefixes -march=armv5te -mtune=xscale -msoft-float -mthumb -Os -g -DNDEBUG -fomit-frame-pointer -fno-strict-aliasing -finline-limit=64 
CXXFLAGS= -fpic -ffunction-sections -funwind-tables -fstack-protector -no-canonical-prefixes -march=armv5te -mtune=xscale -msoft-float -fno-exceptions -fno-rtti -mthumb -Os -g -DNDEBUG -fomit-frame-pointer -fno-strict-aliasing -finline-limit=64

CFLAGS   := $(CFLAGS)

CXXFLAGS := $(CXXFLAGS) -I$(NDK)/sources/cxx-stl/stlport/stlport -I$(NDK)/sources/cxx-stl/gabi++/include
