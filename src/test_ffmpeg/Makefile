WORKDIR = ../..
BINDIR = $(WORKDIR)/bin
SRCDIR = $(WORKDIR)/src
LIBDIR = $(WORKDIR)/lib
FFMPEGLIBDIR = /opt/ffmpeg/ffmpeg-release-x86/lib

MAKE = make
CC = g++
CFLAGS = -Wall -pipe -g -O0
LIBS = 
LDFLAGS = -L$(LIBDIR) -L$(FFMPEGLIBDIR) -lavformat -lavcodec -lavutil -lm -lpthread -lz -lqueue
INCLUDE = -I$(SRCDIR)/include -I/opt/ffmpeg/ffmpeg-release-x86/include -I.
AR = ar
ARFLAGS = cr
RM = -rm -f
TARGETS = test
OBJS = main.o ffmpeg.o
SUBDIRS = 

all : subdirs $(TARGETS)

subdirs : 
	@for dir in $(SUBDIRS); \
        do $(MAKE) -C $$dir || exit 1; \
        done

$(TARGETS) : $(LIBS) $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	#$(CC) $(CFLAGS) -fPIC -shared $^ -o $@ $(LDFLAGS)
	#$(AR) $(ARFLAGS) $@ $^

%.o : %.cpp
	$(CC) $(CFLAGS) -c $(INCLUDE) $< -o $@
	#$(CC) $(CFLAGS) -fPIC -shared -c $(INCLUDE) $< -o $@


clean:
	@for dir in $(SUBDIRS); \
        do $(MAKE) -C $$dir clean || exit 1; \
        done
	$(RM) $(OBJS) $(TARGETS)

.PHONY: all subdirs clean

