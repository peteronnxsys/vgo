// vgo — Vietnamese input engine (pure logic, no OS dependency)
// Phase 1: Telex composition. Testable standalone via CLI.
#pragma once
#include <string>
#include <cstdint>

namespace vgo {

enum class Method { Telex, VNI };

struct Config {
    // Đặt dấu thanh cho oa/oe/uy ở âm tiết mở:
    //   true  = kiểu cũ  -> "hòa", "thúy", "khoẻ" (dấu về nguyên âm đầu)
    //   false = kiểu mới -> "hoà", "thuý", "khoẻ" (dấu về nguyên âm sau)
    bool oldStyleTone = true;
    // 'w' đứng một mình (không có nguyên âm để biến đổi):
    //   false = ra chữ 'w' thường (đỡ phá tiếng Anh; gõ ư bằng 'uw')
    //   true  = tự thành 'ư'
    bool wAlwaysU = false;
    Method method = Method::Telex;
};

// Engine xử lý MỘT âm tiết / từ đang gõ. Tầng tích hợp (fcitx/CLI) quyết định
// khi nào commit (gặp space / dấu câu) rồi gọi reset().
class Engine {
public:
    explicit Engine(Config cfg = {}) : cfg_(cfg) {}

    void key(char c);          // nạp 1 phím ASCII
    void backspace();          // xoá 1 ký tự cuối
    void reset() { buf_.clear(); raw_.clear(); }
    bool empty() const { return buf_.empty(); }
    std::string preedit() const;   // chuỗi đã biến đổi tiếng Việt (UTF-8)

    // Có thể là một âm tiết tiếng Việt hợp lệ (hoặc tiền tố của một âm tiết)?
    bool plausible() const;
    // Chuỗi để hiển thị: bản tiếng Việt nếu plausible, ngược lại = phím thô.
    // Đây là cái tầng tích hợp nên dùng (tự xử tiếng Anh lẫn lộn).
    std::string display() const { return plausible() ? preedit() : raw_; }

private:
    Config cfg_;
    std::u32string buf_;       // buffer đã biến đổi
    std::string raw_;          // phím thô đã gõ (cho fallback tiếng Anh)

    void appendLiteral(char c);
    void normalizeTone();

    bool handleTone(int tone, char typed);
    bool handleRemoveTone(char typed);
    bool handleHorn(char typed);
    bool handleCircum(char lc, char typed);
    bool handleDStroke(char typed);
};

// Tiện ích cho test/CLI: nạp cả chuỗi phím, tự commit khi gặp ký tự không phải chữ.
std::string typeString(const std::string& keys, Config cfg = {});

} // namespace vgo
