CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -O2 -fPIC
LDFLAGS = -shared
LDLIBS = -lcjson -lrabbitmq -lpthread

LIBNAME = libwaggle.so

SRCS = \
  src/plugin/plugin.c \
  src/plugin/config.c \
  src/plugin/rabbitmq.c \
  src/plugin/uploader.c \
  src/timeutil.c \
  src/wagglemsg.c

OBJS = $(SRCS:.c=.o)

all: $(LIBNAME)

$(LIBNAME): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(LIBNAME)

.PHONY: all clean
