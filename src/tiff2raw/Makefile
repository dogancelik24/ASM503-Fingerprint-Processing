CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic
TIFF_CFLAGS = $(shell pkg-config --cflags libtiff-4)
TIFF_LIBS = $(shell pkg-config --libs libtiff-4)

TARGET = tiff2raw.exe
SOURCE = tiff2raw.c
INSTALL_DIR = ../../bin

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(TIFF_CFLAGS) $(SOURCE) -o $(TARGET) $(TIFF_LIBS)

install: $(TARGET)
	mkdir -p $(INSTALL_DIR)
	cp $(TARGET) $(INSTALL_DIR)/$(TARGET)

clean:
	rm -f $(TARGET) $(INSTALL_DIR)/$(TARGET)

.PHONY: all install clean
