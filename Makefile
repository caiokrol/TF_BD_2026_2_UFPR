CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2
TARGET  = fdtool

SRCS    = main.c parser.c algorithms.c
OBJS    = $(SRCS:.c=.o)
HDRS    = fdtool.h

.PHONY: all clean rebuild

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

rebuild: clean all