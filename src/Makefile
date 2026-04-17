CC     = gcc
CFLAGS = -Wall -Wextra -I../include

SRCS = main.c \
       common/logging.c \
       common/utils.c \
       runner/runner.c \
       runner/gcc.c \
       runner/cppcheck.c \
       runner/flawfinder.c \
       runner/coverity.c \
       parser/parser.c \
       parser/gcc_parser.c \
       parser/cppcheck_parser.c \
       parser/flawfinder_parser.c \
       parser/coverity_parser.c \
       tool_finder.c

OBJS = $(SRCS:.c=.o)

TARGET = ../build/analyzer

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
