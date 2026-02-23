CXX      = g++
CXXFLAGS = -std=c++17 -Wall -O2 -Isrc
LDFLAGS  = -lws2_32

SRCS = src/main.cpp src/game.cpp src/server.cpp
TARGET = server.exe

.PHONY: all ts cpp run clean

all: ts cpp

ts: frontend/game.js

frontend/game.js: frontend/game.ts tsconfig.json
	npx tsc

cpp: $(TARGET)

$(TARGET): $(SRCS) src/game.h src/server.h
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS) $(LDFLAGS)

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET) frontend/game.js
