# Makefile для 5_1_final.c

CC     = gcc
CFLAGS = -Wall -Wextra -std=c11
TARGET = final
SRC    = 5_1_final.c       

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

test: $(TARGET)
	./testscript.sh

clean:
	rm -f $(TARGET) *.o *.copy

run: $(TARGET)
	./$(TARGET) filetocopy.txt

.PHONY: all test clean