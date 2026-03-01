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

.PHONY: all clean rebuild

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Explicit header-file dependencies so that touching a header triggers a rebuild
# of every .c that includes it.

driver.o: driver.c lexer.h lexerDef.h grammar.h grammarDef.h set.h firstfollow.h parser.h parserDef.h
	$(CC) $(CFLAGS) -c driver.c -o driver.o

lexer.o: lexer.c lexerDef.h
	$(CC) $(CFLAGS) -c lexer.c -o lexer.o

grammar.o: grammar.c grammar.h grammarDef.h
	$(CC) $(CFLAGS) -c grammar.c -o grammar.o

set.o: set.c set.h
	$(CC) $(CFLAGS) -c set.c -o set.o

firstfollow.o: firstfollow.c firstfollow.h grammarDef.h set.h
	$(CC) $(CFLAGS) -c firstfollow.c -o firstfollow.o

parser.o: parser.c parser.h parserDef.h grammarDef.h set.h firstfollow.h lexer.h lexerDef.h
	$(CC) $(CFLAGS) -c parser.c -o parser.o

clean:
	rm -f $(OBJS) $(TARGET)

rebuild: clean all