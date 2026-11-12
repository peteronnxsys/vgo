// vgo-vni-cli: gõ thử engine VNI từ terminal.
//   ./vgo-vni-cli "Tie6ng1 Vie65t"        -> Tiếng Việt
//   echo "xin cha2o" | ./vgo-vni-cli      (mỗi dòng = chuỗi phím VNI)
#include "vni.hpp"
#include <cstdio>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            if (i > 1) std::printf(" ");
            std::printf("%s", vgo::vniTypeString(argv[i]).c_str());
        }
        std::printf("\n");
        return 0;
    }
    std::string line;
    while (std::getline(std::cin, line))
        std::printf("%s\n", vgo::vniTypeString(line).c_str());
    return 0;
}
