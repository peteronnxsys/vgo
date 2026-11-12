CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Isrc

# Repo có thể chỉ có Telex (src/engine.cpp), chỉ VNI (src/vni.cpp) hoặc cả hai:
# có file nào thì build + test bộ gõ đó.
TELEX := $(wildcard src/engine.cpp)
VNI   := $(wildcard src/vni.cpp)
ALL   := $(if $(TELEX),vgo-test vgo-cli) $(if $(VNI),vgo-vni-cli)
TESTS := $(if $(TELEX),test-telex test-vikey) $(if $(VNI),test-vni)

.PHONY: all test test-telex test-vikey test-vni share clean lex
all: $(ALL)

vgo-test: src/engine.cpp src/viet.cpp test/tests.cpp src/engine.hpp src/viet.hpp
	$(CXX) $(CXXFLAGS) src/engine.cpp src/viet.cpp test/tests.cpp -o $@

vgo-cli: src/engine.cpp src/viet.cpp src/cli.cpp src/engine.hpp src/viet.hpp
	$(CXX) $(CXXFLAGS) src/engine.cpp src/viet.cpp src/cli.cpp -o $@

vgo-vni-cli: src/viet.cpp src/vni.cpp src/vni-cli.cpp src/viet.hpp src/vni.hpp
	$(CXX) $(CXXFLAGS) src/viet.cpp src/vni.cpp src/vni-cli.cpp -o $@

# Dựng lại data/vn-syllables.txt: make lex FREQ=đường/dẫn/vi_50k.txt
mklex: src/engine.cpp src/viet.cpp tools/mklex.cpp src/engine.hpp
	$(CXX) $(CXXFLAGS) src/engine.cpp src/viet.cpp tools/mklex.cpp -o $@

lex: mklex
	./mklex < $(FREQ) > data/vn-syllables.txt

# make test = mọi bộ test của bộ gõ đang có
test: $(TESTS)

test-telex: vgo-test
	./vgo-test

test-vikey: vgo-cli
	bash test/run-cases.sh

test-vni: vgo-vni-cli
	bash test/run-cases.sh test/vni-cases.txt ./vgo-vni-cli

# 2 gói source + bản build sẵn để chia sẻ: dist/share/vgo-*.tar.gz, vgo-vni-*.tar.gz
share:
	@test -f tools/mkshare.sh || { echo "make share chỉ chạy trong repo gốc"; exit 1; }
	bash tools/mkshare.sh

clean:
	rm -f vgo-test vgo-cli vgo-vni-cli mklex
	rm -rf dist
