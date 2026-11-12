// vgo — lõi chữ Việt thuần (không phụ thuộc kiểu gõ): bảng dấu, đặt thanh, kiểm âm tiết.
// Dùng chung cho engine Telex (engine.cpp) và VNI (vni.cpp).
#pragma once
#include <string>

namespace vgo {
namespace core {

// ---- Modifiers (dấu phụ trên nguyên âm / đ) ----
enum Mod { M_NONE = 0, M_CIRC, M_BREVE, M_HORN, M_STROKE };
// ---- Tones, index 0..5: không, sắc, huyền, hỏi, ngã, nặng ----
constexpr int T_NONE = 0;

struct Decomp { char base; int mod; int tone; bool upper; };

char32_t decodeOne(const char* s);
void encodeOne(std::string& out, char32_t c);
Decomp decompose(char32_t c);
char32_t compose(char base, int mod, int tone, bool upper);
bool isVowelBase(char b);
bool isVowelChar(char32_t c);

// oldStyleTone: true = "hòa/thúy", false = "hoà/thuý"
int findToneTarget(const std::u32string& s, bool oldStyleTone);  // nguyên âm mang thanh, -1 = không có
void retone(std::u32string& s, bool oldStyleTone);               // dời thanh về đúng chỗ
std::u32string spellFix(std::u32string s, bool oldStyleTone);    // ie->iê, ưo->ươ, uo->uô (lúc hiển thị)

bool plausible(const std::u32string& s);                 // âm tiết (hoặc tiền tố) hợp lệ?
bool complete(const std::u32string& s, bool oldStyleTone);  // âm tiết TRỌN hợp lệ?

} // namespace core
} // namespace vgo
