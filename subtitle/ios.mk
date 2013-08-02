CC := clang
CXX := clang
AR := libtool -o
CFLAGS=--sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS6.1.sdk -arch armv7 -DOS_IOS -DNDEBUG -Wno-deprecated-declarations
CXXFLAGS=--sysroot=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS6.1.sdk -arch armv7 -DOS_IOS -DNDEBUG -Wno-deprecated-declarations
