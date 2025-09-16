CXX = x86_64-w64-mingw32-g++
CXXFLAGS = -std=c++17 -O3 -static -static-libgcc -static-libstdc++
TARGET = xmrdesk.exe
SOURCE = main.cpp

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE) -lws2_32

clean:
	rm -f $(TARGET)

.PHONY: all clean
