CXX ?= c++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2
THREAD_FLAGS = -pthread

.PHONY: all examples clean

all: concurrency

concurrency: main.cpp
	$(CXX) $(CXXFLAGS) $(THREAD_FLAGS) main.cpp -o $@

examples: s59 s60 s61 s62

s59: s59.cpp
	$(CXX) $(CXXFLAGS) $(THREAD_FLAGS) $< -o $@

s60: s60.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

s61: s61.cpp
	$(CXX) $(CXXFLAGS) $(THREAD_FLAGS) $< -o $@

s62: s62.cpp
	$(CXX) $(CXXFLAGS) $(THREAD_FLAGS) $< -o $@

clean:
	rm -f concurrency s59 s60 s61 s62
