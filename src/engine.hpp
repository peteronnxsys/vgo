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
    void reset() { buf_.clear(); raw_.clear(); forcedRaw_ = false; forcedVN_ = false; }

    // Ngữ cảnh (cho bigram + luật IN HOA): token vừa gõ xong, tách bằng khoảng trắng.
    // endWord = chốt từ đang gõ trước ranh giới `boundary` (0 = không có) rồi reset().
    // "it's been": tới lúc gõ "been" thì prev() == "it's".
    void endWord(char32_t boundary);
    void clearContext() { tok_.clear(); tokRaw_.clear(); prev_.clear(); prevRaw_.clear(); }
    const std::string& prev() const { return prev_; }
    bool empty() const { return buf_.empty(); }
    std::string preedit() const;        // chuỗi đã biến đổi tiếng Việt (UTF-8)
    const std::string& raw() const { return raw_; }  // phím thô đã gõ

    // Có thể là một âm tiết tiếng Việt hợp lệ (hoặc tiền tố của một âm tiết)?
    bool plausible() const;
    // Nút lật 2 chiều (Esc): ép từ đang gõ về thô (Anh) hoặc về tiếng Việt.
    void forceRaw() { forcedRaw_ = true; forcedVN_ = false; }
    void forceVN()  { forcedVN_ = true;  forcedRaw_ = false; }
    // Hiện tại từ đang gõ ra dạng thô (Anh) hay bản tiếng Việt?
    bool isShowingRaw() const;

    // Chuỗi để hiển thị (tầng tích hợp dùng cái này): isShowingRaw ? thô : tiếng Việt.
    std::string display() const;

private:
    Config cfg_;
    std::u32string buf_;       // buffer đã biến đổi
    std::string raw_;          // phím thô đã gõ (cho fallback tiếng Anh)
    bool forcedRaw_ = false;   // Esc đã ép từ này về thô (Anh)
    bool forcedVN_ = false;    // Esc đã ép từ này về tiếng Việt
    std::string tok_;          // token hiện tại (phần đã chốt, chưa gặp khoảng trắng)
    std::string prev_;         // token trước (đã gặp khoảng trắng)
    std::string tokRaw_, prevRaw_;  // như trên nhưng là phím thô ("has" dù đã hiện "há")

    void appendLiteral(char c);
    void normalizeTone();

    bool handleTone(int tone, char typed);
    bool handleRemoveTone(char typed);
    bool handleHorn(char typed);
    bool handleCircum(char lc, char typed);
    bool handleDStroke(char typed);
};

// ---- Từ điển tiếng Anh (chia sẻ toàn tiến trình; nạp 1 lần) ----
// Khi chuỗi phím thô khớp một từ trong từ điển -> coi là tiếng Anh, nhả thô.
void dictAddWord(const std::string& w);          // thêm 1 từ (lowercase hoá)
void dictLoadFile(const std::string& path);      // nạp file, mỗi dòng 1 từ; thiếu file thì bỏ qua
bool dictContains(const std::string& w);         // tra (lowercase)

// ---- Danh sách ÉP TIẾNG VIỆT (ưu tiên hơn từ điển Anh) ----
// Từ Việt có mặt chữ trùng một từ Anh (vd "been"->bên) -> ép biến đổi Việt.
void vnAddWord(const std::string& w);
void vnLoadFile(const std::string& path);
bool vnContains(const std::string& w);

// ---- BIGRAM: từ trước + từ đang gõ -> ép Anh/Việt (thắng 2 danh sách trên; Esc thắng tất) ----
// File: mỗi dòng "từ_trước phím_thô en|vn", vd "it's been en", "xe bus en".
// từ_trước = chữ đã hiện ra (UTF-8, vd "đi"), phím_thô = Telex đã gõ.
void bigramAdd(const std::string& prev, const std::string& raw, bool english);
void bigramLoadFile(const std::string& path);
// 1 = Anh, -1 = Việt, 0 = không có luật
int bigramLookup(const std::string& prev, const std::string& raw);

// Tiện ích cho test/CLI: nạp cả chuỗi phím, tự commit khi gặp ký tự không phải chữ.
std::string typeString(const std::string& keys, Config cfg = {});

} // namespace vgo
