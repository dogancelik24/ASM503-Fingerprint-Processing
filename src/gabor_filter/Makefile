CXX = g++
TARGET = gabor_filter.exe
SOURCE = gabor_filter.cpp

PKG_CONFIG ?= pkg-config
OPENCV = opencv5

CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wpedantic \
           $(shell $(PKG_CONFIG) --cflags $(OPENCV))

LDLIBS = $(shell $(PKG_CONFIG) --libs $(OPENCV))

INSTALL_DIR = ../../bin

.PHONY: all install clean

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) $(SOURCE) -o $(TARGET) $(LDLIBS)

install: $(TARGET)
	mkdir -p $(INSTALL_DIR)
	cp $(TARGET) $(INSTALL_DIR)/$(TARGET)

clean:
	rm -f $(TARGET) $(INSTALL_DIR)/$(TARGET)
