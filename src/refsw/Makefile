#DEBUG=1

WORKDIR = ../..
BINDIR = $(WORKDIR)/bin
SRCDIR = $(WORKDIR)/src
LIBDIR = $(WORKDIR)/lib
FFMPEGLIBDIR = /opt/ffmpeg/ffmpeg-release-x86/lib

SHARED_LIBRARYNAME = playpump
SOURCEFILES = playpump.c

BASE_DIR = ../..
LIB_INCLUDE_DIR += ../include/
LIBRARY_DIR += $(LIBDIR)
LIBRARY_DIR += $(FFMPEGLIBDIR)
LIBRARY_DIR += $(LIBDIR)/refsw/nexus/bin

INSTALL_DIR += $(LIBDIR)

ifndef PLATFORM
$(error PLATFORM is not defined)
endif
NEXUS_TOP := $(LIBDIR)/refsw/nexus
# include cross-compiler definitions
include $(NEXUS_TOP)/platforms/$(PLATFORM)/build/platform_app.inc


LIB_INCLUDE_DIR += $(NEXUS_TOP)/base/include
LIB_INCLUDE_DIR += $(NEXUS_TOP)/base/include/linuxuser
LIB_INCLUDE_DIR += $(NEXUS_TOP)/platforms/97241/src
LIB_INCLUDE_DIR += $(NEXUS_TOP)/platforms/common/src
LIB_INCLUDE_DIR += $(NEXUS_TOP)/../magnum/basemodules/reg
LIB_INCLUDE_DIR += $(NEXUS_TOP)/../magnum/basemodules/int/7429
LIB_INCLUDE_DIR += $(NEXUS_TOP)/../magnum/basemodules/int
LIB_INCLUDE_DIR += $(NEXUS_TOP)/../magnum/basemodules/tmr/7429
LIB_INCLUDE_DIR += $(NEXUS_TOP)/../magnum/basemodules/mem
LIB_INCLUDE_DIR += $(NEXUS_TOP)/../magnum/commonutils/avc/7429
LIB_INCLUDE_DIR += $(NEXUS_TOP)/../magnum/commonutils/pxl
LIB_INCLUDE_DIR += $(NEXUS_TOP)/../magnum/commonutils/sur/7429
LIB_INCLUDE_DIR += $(NEXUS_TOP)/../magnum/commonutils/fmt
LIB_INCLUDE_DIR += $(NEXUS_TOP)/../magnum/commonutils/mrc/7429
LIB_INCLUDE_DIR += $(NEXUS_TOP)/../magnum/portinginterface/pwr/7429
LIB_INCLUDE_DIR += $(NEXUS_TOP)/../rockford/middleware/platform/nexus/
LIB_INCLUDE_DIR += $(NEXUS_TOP)/../rockford/middleware/v3d/interface/khronos/include/

NEXUS_APP_DEFINES += CFG_EVIDEO
# This is the minimum needed to compile and link with Nexus
CFLAGS += $(subst -pedantic,,$(NEXUS_CFLAGS)) $(addprefix -I,$(NEXUS_APP_INCLUDE_PATHS)) $(addprefix -D,$(NEXUS_APP_DEFINES))  -DSINGLE_PROCESS -std=gnu99 

#MWLIB    = 
MIDDLELIB= nexus CFffmpeg
SYSLIB   = pthread

LIBDIRS = $(addprefix -L,$(LIBRARY_DIR)) 
LIBS    = $(addprefix -l,$(MWLIB) $(MIDDLELIB) $(SYSLIB))

NEXUS_APP_DEFINES+=CFG_EVIDEO
#
# Remove phony targets from make contention
#

.SUFFIXES:
.SUFFIXES: .c .cpp .o .h

.PHONY: all clean .depend install uninstall dummy

CFLAGS += $(addprefix -D, $(DEFINES)) $(addprefix -I,$(LIB_INCLUDE_DIR)) -finstrument-functions
CXXFLAGS = $(CFLAGS)
ARFLAGS  = rcs
LDFLAGS += $(LIBDIRS) $(LIBS) 
#LDFLAGS += --disable-shared

ifdef DEBUG
  CFLAGS += -g -O0
  LDFLAGS += -g
endif

#Q_ :=

define COMPILE_C
$(Q_)$(CC) $(CFLAGS) -o $@ -fpic -c $<
endef

define COMPILE_CXX
$(Q_)$(CXX) $(CFLAGS) -o $@ -fPIC -c $<
endef

ECHO=echo
COPY=cp -a

OBJS = $(patsubst %.c, %.o, $(patsubst %.cpp, %.o, $(SOURCEFILES)))
OBJS += $(patsubst %.c, %.o, $(EXTRA_SOURCE))
OBJS += $(patsubst %.c, %.o, $(EXTRA_OBJS))


ifdef EXECUTABLENAME
BUILD_TARGET = $(EXECUTABLENAME)

$(EXECUTABLENAME): $(OBJS)
	@$(ECHO) --- Building $@
	$(CC) -o $@ $^ $(LDFLAGS)
endif

ifdef STATIC_LIBRARYNAME
BUILD_TARGET = $(STATIC_LIBRARYNAME)

$(STATIC_LIBRARYNAME): $(OBJS)
	@$(ECHO) --- Building $@
	$(AR) $(ARFLAGS) $@ $^ $(LDFLAGS)
endif

ifdef SHARED_LIBRARYNAME
BUILD_TARGET = $(SHARED_LIBRARYNAME)

$(SHARED_LIBRARYNAME): $(OBJS)
	@$(ECHO) --- Building $@
	$(Q_)$(CC) -shared -o $@ $^ $(LDFLAGS)
endif

#
# Makefile rules
#

all: $(BUILD_TARGET)
	@$(ECHO) "--- Complete successfully ---"

clean:
	$(RM) $(BUILD_TARGET) $(CVBS_SHARED_LIBRARYNAME) $(HDMI_SHARED_LIBRARYNAME) $(OBJS) .depend

%.o: %.cpp 
	@$(ECHO) --- Compiling $<
	$(COMPILE_CXX)
%.o: %.c
	@$(ECHO) --- Compiling $<
	$(COMPILE_C)

install: $(BUILD_TARGET)
	cp -a $(BUILD_TARGET) $(INSTALL_DIR)
	cp -a cbb_platform_define.h $(BASE_DIR)/platform/brcm7241/candidates/include/platform
#	cp -a $(BUILD_TARGET) /opt/brcm/nfsroot/usr/lib

.depend:	Makefile $(SOURCEFILES)
		@$(ECHO) --- Making depend
		$(Q_)$(CC) -M $(CFLAGS) $(SOURCEFILES) > $@
sinclude .depend 
