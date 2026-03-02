CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall

SRCS = IHTC_Data.cc IHTC_Driver.cc
TARGET = IHTC_Driver

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
