CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wpedantic
OPENCV_FLAGS = $(shell pkg-config --cflags --libs opencv5)

TARGET = orientation.exe
SOURCE = orientation.cpp
INSTALL_DIR = ../../bin

.PHONY: all install clean

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) $(SOURCE) -o $(TARGET) $(OPENCV_FLAGS)

install: $(TARGET)
	mkdir -p $(INSTALL_DIR)
	cp $(TARGET) $(INSTALL_DIR)/$(TARGET)

clean:
	rm -f $(TARGET) $(INSTALL_DIR)/$(TARGET)
