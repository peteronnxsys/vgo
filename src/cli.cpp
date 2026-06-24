// vgo-cli: gõ thử engine từ terminal.
//   echo "tieesng vieejt" | ./vgo-cli      (mỗi dòng = chuỗi phím Telex)
//   ./vgo-cli "tieesng vieejt"             (đối số)
#include "engine.hpp"
#include <cstdio>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    vgo::Config cfg;
    // nạp từ điển nếu có (chạy từ repo: data/en-base.txt; hoặc bản đã cài)
    vgo::dictLoadFile("data/en-base.txt");
    vgo::dictLoadFile("/usr/share/vgo/en-base.txt");
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            if (i > 1) std::printf(" ");
            std::printf("%s", vgo::typeString(argv[i], cfg).c_str());
        }
        std::printf("\n");
        return 0;
    }
    std::string line;
    while (std::getline(std::cin, line))
        std::printf("%s\n", vgo::typeString(line, cfg).c_str());
    return 0;
}
