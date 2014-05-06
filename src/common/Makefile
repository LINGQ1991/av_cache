WORKDIR = ../..
BINDIR = $(WORKDIR)/bin
SRCDIR = $(WORKDIR)/src
LIBDIR = $(WORKDIR)/lib

MAKE = make
CC = g++
CFLAGS = -Wall -pipe -g -O0
LIBS = 
LDFLAGS = -L/home/dast/lib -L/usr/local/ssl/lib -L/opt/curl/curl-realse-x86/lib/ -lcurl
INCLUDE = -I/opt/curl/curl-realse-x86/include/ -I$(SRCDIR)/include -I.
AR = ar
ARFLAGS = cr
RM = -rm -f
TARGETS = $(LIBDIR)/libcommon.so
OBJS = common.o 
SUBDIRS = 

all : subdirs $(TARGETS)

subdirs : 
	@for dir in $(SUBDIRS); \
        do $(MAKE) -C $$dir || exit 1; \
        done

$(TARGETS) : $(LIBS) $(OBJS)
#$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	$(CC) $(CFLAGS) -fPIC -shared $^ -o $@ $(LDFLAGS)
	#$(AR) $(ARFLAGS) $@ $^

%.o : %.c
#$(CC) $(CFLAGS) -c $(INCLUDE) $< -o $@
	$(CC) $(CFLAGS) -fPIC -shared -c $(INCLUDE) $< -o $@


clean:
	@for dir in $(SUBDIRS); \
        do $(MAKE) -C $$dir clean || exit 1; \
        done
	$(RM) $(OBJS) $(TARGETS)

.PHONY: all subdirs clean

