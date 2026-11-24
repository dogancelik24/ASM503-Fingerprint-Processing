CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -pedantic

TARGET = raw2bmp
INSTALL_DIR = /c/ASM503/bin

all: $(TARGET)

$(TARGET): raw2bmp.c
	$(CC) $(CFLAGS) raw2bmp.c -o $(TARGET)

install: $(TARGET)
	mkdir -p $(INSTALL_DIR)
	cp $(TARGET) $(INSTALL_DIR)/$(TARGET).exe

clean:
	rm -f $(TARGET) $(INSTALL_DIR)/$(TARGET).exe

.PHONY: all install clean
