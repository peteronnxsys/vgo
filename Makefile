CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Isrc

.PHONY: all test test-vikey clean lex
all: vgo-test vgo-cli

vgo-test: src/engine.cpp test/tests.cpp src/engine.hpp
	$(CXX) $(CXXFLAGS) src/engine.cpp test/tests.cpp -o $@

vgo-cli: src/engine.cpp src/cli.cpp src/engine.hpp
	$(CXX) $(CXXFLAGS) src/engine.cpp src/cli.cpp -o $@

# Dựng lại data/vn-syllables.txt: make lex FREQ=đường/dẫn/vi_50k.txt
mklex: src/engine.cpp tools/mklex.cpp src/engine.hpp
	$(CXX) $(CXXFLAGS) src/engine.cpp tools/mklex.cpp -o $@

lex: mklex
	./mklex < $(FREQ) > data/vn-syllables.txt

test: vgo-test
	./vgo-test

test-vikey: vgo-cli
	bash test/run-cases.sh

clean:
	rm -f vgo-test vgo-cli mklex
