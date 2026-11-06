// mklex: lọc danh sách tần suất "từ số_lần" thành từ điển âm tiết Việt cho vgo.
//   ./mklex < vi_50k.txt > data/vn-syllables.txt
// Giữ dòng khi: đổi ra phím Telex -> gõ lại qua Engine ra ĐÚNG chữ đó + là âm tiết trọn.
// (Lọc bỏ từ Anh, tên riêng, rác của nguồn; chính engine vgo là thước đo.)
#include "engine.hpp"
#include <cstdio>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

using vgo::Engine;

namespace {
// Một ký tự UTF-8 (1 nguyên âm có dấu / đ) -> {phím chữ + dấu phụ, phím thanh}.
// Dựng bằng chính Engine: gõ thử mọi tổ hợp rồi ghi lại kết quả.
std::map<std::string, std::pair<std::string, char>> buildCharMap() {
    std::map<std::string, std::pair<std::string, char>> m;
    const char* bases[] = { "a","aa","aw","e","ee","i","o","oo","ow","u","uw","y" };
    const char tones[] = { 0, 's', 'f', 'r', 'x', 'j' };
    for (const char* b : bases)
        for (char t : tones) {
            Engine e;
            std::string keys = b;
            if (t) keys += t;
            for (char c : keys) e.key(c);
            m[e.preedit()] = { b, t };
        }
    m["đ"] = { "dd", 0 };
    return m;
}
}  // namespace

int main() {
    auto cmap = buildCharMap();
    std::string line;
    std::printf("# Âm tiết tiếng Việt + tần suất (cho vgo tự sửa gõ nhầm).\n"
                "# Nguồn: hermitdave/FrequencyWords, content/2018/vi/vi_50k.txt\n"
                "# (OpenSubtitles 2018; CC-BY-SA-4.0). Lọc bằng tools/mklex.cpp.\n");
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string w;
        long n;
        if (!(in >> w >> n)) continue;
        if (n < 5) continue;    // đuôi < 5 lần phần lớn là lỗi gõ trong phụ đề ("thạt", "khũng")
        std::string keys;
        char tone = 0;
        bool ok = true;
        for (size_t i = 0; i < w.size() && ok;) {
            unsigned char c = (unsigned char)w[i];
            size_t len = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
            std::string ch = w.substr(i, len);
            i += len;
            if (len == 1) {
                if (c < 'a' || c > 'z') ok = false;
                else keys += (char)c;
                continue;
            }
            auto it = cmap.find(ch);
            if (it == cmap.end()) { ok = false; break; }
            keys += it->second.first;
            if (it->second.second) {
                if (tone) ok = false;     // 2 dấu thanh -> không phải 1 âm tiết
                tone = it->second.second;
            }
        }
        if (!ok || keys.empty()) continue;
        if (tone) keys += tone;
        Engine e;
        for (char c : keys) e.key(c);
        if (e.preedit() == w && e.complete()) std::printf("%s %ld\n", w.c_str(), n);
    }
    return 0;
}
