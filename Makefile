CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Isrc

.PHONY: all test clean
all: vgo-test vgo-cli

vgo-test: src/engine.cpp test/tests.cpp src/engine.hpp
	$(CXX) $(CXXFLAGS) src/engine.cpp test/tests.cpp -o $@

vgo-cli: src/engine.cpp src/cli.cpp src/engine.hpp
	$(CXX) $(CXXFLAGS) src/engine.cpp src/cli.cpp -o $@

test: vgo-test
	./vgo-test

clean:
	rm -f vgo-test vgo-cli
