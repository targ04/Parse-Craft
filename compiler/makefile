# Makefile for Stage-1 (Lexer + Parser + Parse Tree)
# Usage:
#   make
#   ./stage1exe testcase.txt parsetreeOutFile.txt
# Optional:
#   make clean

CC      := gcc
CFLAGS  := -Wall -Wextra -g -std=c11
LDFLAGS :=

# Executable name as per spec
TARGET  := stage1exe

# Sources (single folder)
SRCS := driver.c lexer.c grammar.c set.c firstfollow.c parser.c
OBJS := $(SRCS:.c=.o)

.PHONY: all clean rebuild run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Generic compile rule
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Convenience: run with required args (edit TEST/PARSE if you want)
TEST  ?= t3.txt
TREE  ?= parsetreeOutFile.txt
GRAM  ?= grammar.txt

run: $(TARGET)
	./$(TARGET) $(TEST) $(GRAM) $(TREE)

clean:
	rm -f $(OBJS) $(TARGET)

rebuild: clean all