# Picklelauncher makefile

PROGRAM = picklelauncher
LIB_ZIP = libunzip.a

# Build type
#BUILDTYPE = debug
BUILDTYPE = release

# Compiler flags
CXXFLAGS     = -Wall -Wextra -O3
ZIP_CFLAGS   = $(CXXFLAGS) -Wno-unused-parameter
# Linker flags


# Target compiler options
ifeq ($(BUILDTARGET),PANDORA)
PREFIX   = $(PNDSDK)
TOOLS    = bin
TARGET   = arm-none-linux-gnueabi-
INCLUDE  = $(PREFIX)/usr/include
LIBRARY  = $(PREFIX)/usr/lib
USE_SDL2 = False
CXXFLAGS += -DPANDORA
PLATFORM_LDFLAGS = -lfreetype -ltiff -lpng12 -ljpeg -lts
else
ifeq ($(BUILDTARGET),CAANOO)
PREFIX   = $(CAANOOSDK)
TOOLS    = tools/gcc-4.2.4-glibc-2.7-eabi/bin
TARGET   = arm-gph-linux-gnueabi-
INCLUDE  = $(PREFIX)/DGE/include
LIBRARY  = $(PREFIX)/DGE/lib/target
USE_SDL2 = False
CXXFLAGS += -DCAANOO
PLATFORM_LDFLAGS =
else
ifeq ($(BUILDTARGET),WIZ)
PREFIX   = $(WIZSDK)
TOOLS    = bin
TARGET   = arm-openwiz-linux-gnu-
INCLUDE  = $(PREFIX)/include
LIBRARY  = $(PREFIX)/lib
USE_SDL2 = False
CXXFLAGS += -DWIZ
PLATFORM_LDFLAGS = -lfreetype
else
ifeq ($(BUILDTARGET),GP2X)
PREFIX   = $(GP2XSDK)
TOOLS    = bin
TARGET   = arm-open2x-linux-
INCLUDE  = $(PREFIX)/include
LIBRARY  = $(PREFIX)/lib
USE_SDL2 = False
CXXFLAGS += -DGP2X
PLATFORM_LDFLAGS = -static -lfreetype -lpng12 -lpthread -ldl
else
ifeq ($(BUILDTARGET),GCW)
PREFIX   = $(GCWSDK)
TOOLS    = bin
TARGET   = mipsel-gcw0-linux-uclibc-
INCLUDE  = $(PREFIX)/mipsel-gcw0-linux-uclibc/sysroot/usr/include
LIBRARY  = $(PREFIX)/mipsel-gcw0-linux-uclibc/sysroot/usr/lib
USE_SDL2 = False
CXXFLAGS += -DGCW
PLATFORM_LDFLAGS = -lpthread
else
ifeq ($(BUILDTARGET),TRIMUI)
PREFIX   = $(TRIMUISDK)
TOOLS    = bin
TARGET   = aarch64-linux-gnu-
INCLUDE  = $(PREFIX)/include
LIBRARY  = $(PREFIX)/lib
USE_SDL2 = True
CXXFLAGS += -DTRIMUI
PLATFORM_LDFLAGS = -lfreetype -lpthread -lbz2
else # default linux
BUILDTYPE = debug
PREFIX   = /usr
TOOLS    = bin
TARGET   =
USE_SDL2 = True
INCLUDE  = $(PREFIX)/include
LIBRARY  = $(PREFIX)/lib
PLATFORM_LDFLAGS =
endif
endif
endif
endif
endif
endif

ifeq ($(USE_SDL2),True)
SDLVER=SDL2
else
SDLVER=SDL
endif

# Setup SDL target
CXXFLAGS += -I$(INCLUDE) -I$(INCLUDE)/$(SDLVER)
BASE_LDFLAGS = -L$(LIBRARY) -l$(SDLVER)_ttf -l$(SDLVER)_image -l$(SDLVER) -lz

ifeq ($(BUILDTYPE),debug)
CXXFLAGS += -DDEBUG

LDFLAGS  = $(BASE_LDFLAGS) $(PLATFORM_LDFLAGS)
else
LDFLAGS  = -s $(BASE_LDFLAGS) $(PLATFORM_LDFLAGS)
endif

# Source files
SRCS       = main.cpp cselector.cpp cprofile.cpp cconfig.cpp csystem.cpp czip.cpp cbase.cpp
SRCS_ZIP   = ioapi.c unzip.c

# Assign paths to binaries/sources/objects
BUILD      = build
SRCDIR     = src
SRCDIR_ZIP = $(SRCDIR)/unzip
OBJDIR     = $(BUILD)/objs/$(BUILDTYPE)

SRCS       := $(addprefix $(SRCDIR)/,$(SRCS)) 
OBJS       := $(addprefix $(OBJDIR)/,$(SRCS:.cpp=.o)) 
SRCS_ZIP   := $(addprefix $(SRCDIR_ZIP)/,$(SRCS_ZIP)) 
OBJS_ZIP   := $(addprefix $(OBJDIR)/,$(SRCS_ZIP:.c=.o)) 

LIB_ZIP    := $(addprefix $(OBJDIR)/,$(LIB_ZIP)) 
PROGRAM    := $(addprefix $(BUILD)/,$(PROGRAM)) 

# Assign Tools
CC  = $(PREFIX)/$(TOOLS)/$(TARGET)gcc
CXX = $(PREFIX)/$(TOOLS)/$(TARGET)g++
AR  = $(PREFIX)/$(TOOLS)/$(TARGET)ar

# Build rules
all : setup $(LIB_ZIP) $(PROGRAM)

setup:
	mkdir -p $(OBJDIR)/$(SRCDIR_ZIP)

$(LIB_ZIP): $(OBJS_ZIP)
	$(AR) rcs $(LIB_ZIP) $(OBJS_ZIP)

$(PROGRAM): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(PROGRAM) $(OBJS) $(LIB_ZIP) $(LDFLAGS) 

$(OBJDIR)/$(SRCDIR_ZIP)/%.o: $(SRCDIR_ZIP)/%.c
	$(CC) $(ZIP_CFLAGS) -c $< -o $@

$(OBJDIR)/$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(PROGRAM) $(OBJS) $(LIB_ZIP) $(OBJS_ZIP)
