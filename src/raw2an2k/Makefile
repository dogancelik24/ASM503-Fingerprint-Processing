CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic

TARGET = raw2an2k.exe
SOURCE = raw2an2k.c
INSTALL_DIR = ../../bin

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET)

install: $(TARGET)
	mkdir -p $(INSTALL_DIR)
	cp $(TARGET) $(INSTALL_DIR)/$(TARGET)

clean:
	rm -f $(TARGET) $(INSTALL_DIR)/$(TARGET)

.PHONY: all install clean
