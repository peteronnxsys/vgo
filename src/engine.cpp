#include "engine.hpp"
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <fstream>
#include <cctype>

namespace vgo {
namespace {

// ---- Modifiers (dấu phụ trên nguyên âm / đ) ----
enum Mod { M_NONE = 0, M_CIRC, M_BREVE, M_HORN, M_STROKE };
// ---- Tones, index 0..5: không, sắc, huyền, hỏi, ngã, nặng ----
constexpr int T_NONE = 0;

struct Form {
    char base;          // 'a','e','i','o','u','y','d'
    int  mod;
    const char* g[6];   // lowercase, theo index thanh
    const char* G[6];   // uppercase
};

// Bảng chữ tiền-kết-hợp (NFC) cho mọi nguyên âm + đ.
const Form FORMS[] = {
    {'a', M_NONE,  {"a","á","à","ả","ã","ạ"}, {"A","Á","À","Ả","Ã","Ạ"}},
    {'a', M_BREVE, {"ă","ắ","ằ","ẳ","ẵ","ặ"}, {"Ă","Ắ","Ằ","Ẳ","Ẵ","Ặ"}},
    {'a', M_CIRC,  {"â","ấ","ầ","ẩ","ẫ","ậ"}, {"Â","Ấ","Ầ","Ẩ","Ẫ","Ậ"}},
    {'e', M_NONE,  {"e","é","è","ẻ","ẽ","ẹ"}, {"E","É","È","Ẻ","Ẽ","Ẹ"}},
    {'e', M_CIRC,  {"ê","ế","ề","ể","ễ","ệ"}, {"Ê","Ế","Ề","Ể","Ễ","Ệ"}},
    {'i', M_NONE,  {"i","í","ì","ỉ","ĩ","ị"}, {"I","Í","Ì","Ỉ","Ĩ","Ị"}},
    {'o', M_NONE,  {"o","ó","ò","ỏ","õ","ọ"}, {"O","Ó","Ò","Ỏ","Õ","Ọ"}},
    {'o', M_CIRC,  {"ô","ố","ồ","ổ","ỗ","ộ"}, {"Ô","Ố","Ồ","Ổ","Ỗ","Ộ"}},
    {'o', M_HORN,  {"ơ","ớ","ờ","ở","ỡ","ợ"}, {"Ơ","Ớ","Ờ","Ở","Ỡ","Ợ"}},
    {'u', M_NONE,  {"u","ú","ù","ủ","ũ","ụ"}, {"U","Ú","Ù","Ủ","Ũ","Ụ"}},
    {'u', M_HORN,  {"ư","ứ","ừ","ử","ữ","ự"}, {"Ư","Ứ","Ừ","Ử","Ữ","Ự"}},
    {'y', M_NONE,  {"y","ý","ỳ","ỷ","ỹ","ỵ"}, {"Y","Ý","Ỳ","Ỷ","Ỹ","Ỵ"}},
    {'d', M_STROKE,{"đ",nullptr,nullptr,nullptr,nullptr,nullptr},
                   {"Đ",nullptr,nullptr,nullptr,nullptr,nullptr}},
};
constexpr int NFORMS = sizeof(FORMS) / sizeof(FORMS[0]);

// ---- UTF-8 helpers ----
char32_t decodeOne(const char* s) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(s);
    if (p[0] < 0x80) return p[0];
    if ((p[0] >> 5) == 0x6) return ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
    if ((p[0] >> 4) == 0xE)
        return ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    return ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) |
           ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
}
void encodeOne(std::string& out, char32_t c) {
    if (c < 0x80) { out += char(c); }
    else if (c < 0x800) {
        out += char(0xC0 | (c >> 6));
        out += char(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        out += char(0xE0 | (c >> 12));
        out += char(0x80 | ((c >> 6) & 0x3F));
        out += char(0x80 | (c & 0x3F));
    } else {
        out += char(0xF0 | (c >> 18));
        out += char(0x80 | ((c >> 12) & 0x3F));
        out += char(0x80 | ((c >> 6) & 0x3F));
        out += char(0x80 | (c & 0x3F));
    }
}

struct Decomp { char base; int mod; int tone; bool upper; };

struct RevEntry { int form; int tone; bool upper; };
const std::unordered_map<char32_t, RevEntry>& revMap() {
    static const std::unordered_map<char32_t, RevEntry> m = [] {
        std::unordered_map<char32_t, RevEntry> r;
        for (int f = 0; f < NFORMS; ++f)
            for (int t = 0; t < 6; ++t) {
                if (FORMS[f].g[t]) r[decodeOne(FORMS[f].g[t])] = {f, t, false};
                if (FORMS[f].G[t]) r[decodeOne(FORMS[f].G[t])] = {f, t, true};
            }
        return r;
    }();
    return m;
}

Decomp decompose(char32_t c) {
    auto& m = revMap();
    auto it = m.find(c);
    if (it != m.end())
        return {FORMS[it->second.form].base, FORMS[it->second.form].mod,
                it->second.tone, it->second.upper};
    // ASCII letter (phụ âm / nguyên âm trần) hoặc ký tự khác
    if (c < 128 && std::isalpha((int)c)) {
        bool up = std::isupper((int)c);
        return {(char)std::tolower((int)c), M_NONE, T_NONE, up};
    }
    return {(char)c, M_NONE, T_NONE, false};
}

char32_t compose(char base, int mod, int tone, bool upper) {
    for (int f = 0; f < NFORMS; ++f) {
        if (FORMS[f].base == base && FORMS[f].mod == mod) {
            const char* s = upper ? FORMS[f].G[tone] : FORMS[f].g[tone];
            if (!s) s = upper ? FORMS[f].G[0] : FORMS[f].g[0]; // đ + tone -> đ
            return decodeOne(s);
        }
    }
    // phụ âm thường: trả lại ASCII
    char c = base;
    return upper ? (char32_t)std::toupper((int)c) : (char32_t)c;
}

bool isVowelBase(char b) {
    return b=='a'||b=='e'||b=='i'||b=='o'||b=='u'||b=='y';
}
bool isVowelChar(char32_t c) { return isVowelBase(decompose(c).base); }

// Tìm nguyên âm sẽ mang dấu thanh. Trả index trong s, hoặc -1.
int findToneTarget(const std::u32string& s, const Config& cfg) {
    int n = (int)s.size();
    std::vector<int> W;
    for (int i = 0; i < n; ++i)
        if (isVowelChar(s[i])) W.push_back(i);
    if (W.empty()) return -1;

    auto baseAt = [&](int i){ return decompose(s[i]).base; };

    if (W.size() > 1) {
        // "qu" : u là âm đệm, không mang dấu
        if (baseAt(0) == 'q' && baseAt(1) == 'u') {
            W.erase(std::remove(W.begin(), W.end(), 1), W.end());
        }
        // "gi" + nguyên âm : i là phụ âm đầu, không mang dấu
        else if (baseAt(0) == 'g' && baseAt(1) == 'i' && n >= 3 && isVowelChar(s[2])) {
            W.erase(std::remove(W.begin(), W.end(), 1), W.end());
        }
    }
    if (W.empty()) return -1;

    // Ưu tiên nguyên âm có dấu phụ (â ê ô ă ơ ư). "ươ" -> ơ (cái sau).
    std::vector<int> marked;
    for (int idx : W) {
        int m = decompose(s[idx]).mod;
        if (m == M_CIRC || m == M_BREVE || m == M_HORN) marked.push_back(idx);
    }
    if (!marked.empty()) return marked.back();

    if (W.size() == 1) return W[0];

    int lastV = W.back();
    bool hasFinal = (lastV < n - 1);   // còn phụ âm cuối phía sau
    if (hasFinal) return W.back();

    // Âm tiết mở, >=2 nguyên âm, không có dấu phụ
    int pen = W[W.size() - 2], last = W.back();
    char bp = baseAt(pen), bl = baseAt(last);
    if ((bp=='o'&&bl=='a') || (bp=='o'&&bl=='e') || (bp=='u'&&bl=='y'))
        return cfg.oldStyleTone ? pen : last;
    return pen;
}

int toneOf(char lc) {
    switch (lc) { case 's':return 1; case 'f':return 2; case 'r':return 3;
                  case 'x':return 4; case 'j':return 5; default:return 0; }
}

} // anonymous namespace

// ================= Engine =================

void Engine::appendLiteral(char c) {
    buf_ += compose((char)std::tolower((unsigned char)c), M_NONE, T_NONE,
                    std::isupper((unsigned char)c));
}

void Engine::normalizeTone() {
    int tone = T_NONE, pos = -1;
    for (int i = 0; i < (int)buf_.size(); ++i) {
        Decomp d = decompose(buf_[i]);
        if (isVowelBase(d.base) && d.tone != T_NONE) { tone = d.tone; pos = i; break; }
    }
    if (tone == T_NONE) return;
    for (int i = 0; i < (int)buf_.size(); ++i) {
        Decomp d = decompose(buf_[i]);
        if (isVowelBase(d.base) && d.tone != T_NONE)
            buf_[i] = compose(d.base, d.mod, T_NONE, d.upper);
    }
    int tgt = findToneTarget(buf_, cfg_);
    if (tgt < 0) tgt = pos;
    Decomp d = decompose(buf_[tgt]);
    buf_[tgt] = compose(d.base, d.mod, tone, d.upper);
}

bool Engine::handleTone(int tone, char typed) {
    (void)typed;
    int tgt = findToneTarget(buf_, cfg_);
    if (tgt < 0) return false;            // không có nguyên âm -> ký tự thường

    int curTone = T_NONE, curPos = -1;
    for (int i = 0; i < (int)buf_.size(); ++i) {
        Decomp d = decompose(buf_[i]);
        if (isVowelBase(d.base) && d.tone != T_NONE) { curTone = d.tone; curPos = i; break; }
    }
    if (curTone == tone) {                // gõ đúp cùng phím dấu (fat-finger) -> lờ đi,
        return true;                      // giữ nguyên từ. (Từ Anh đã do từ điển/restore lo.)
    }
    if (curPos >= 0) {                    // gỡ dấu cũ
        Decomp d = decompose(buf_[curPos]);
        buf_[curPos] = compose(d.base, d.mod, T_NONE, d.upper);
    }
    Decomp d = decompose(buf_[tgt]);
    buf_[tgt] = compose(d.base, d.mod, tone, d.upper);
    return true;
}

bool Engine::handleRemoveTone(char typed) {
    (void)typed;
    for (int i = 0; i < (int)buf_.size(); ++i) {
        Decomp d = decompose(buf_[i]);
        if (isVowelBase(d.base) && d.tone != T_NONE) {
            buf_[i] = compose(d.base, d.mod, T_NONE, d.upper);
            return true;
        }
    }
    return false;                         // không có dấu -> 'z' thường
}

bool Engine::handleHorn(char typed) {
    int n = (int)buf_.size();
    // "uo" -> "ươ" (sừng cả hai)
    if (n >= 2) {
        Decomp a = decompose(buf_[n-2]), b = decompose(buf_[n-1]);
        if (a.base=='u'&&a.mod==M_NONE && b.base=='o'&&b.mod==M_NONE) {
            buf_[n-2] = compose('u', M_HORN, a.tone, a.upper);
            buf_[n-1] = compose('o', M_HORN, b.tone, b.upper);
            normalizeTone();
            return true;
        }
    }
    // nguyên âm cuối là a/o/u
    int v = -1;
    for (int i = n-1; i >= 0; --i) if (isVowelChar(buf_[i])) { v = i; break; }
    if (v >= 0) {
        Decomp d = decompose(buf_[v]);
        if (d.base=='a' || d.base=='o' || d.base=='u') {
            int want = (d.base=='a') ? M_BREVE : M_HORN;
            if (d.mod == want) {          // gõ lại -> bỏ sừng + chữ thật
                buf_[v] = compose(d.base, M_NONE, d.tone, d.upper);
                appendLiteral(typed);
                normalizeTone();
                return true;
            }
            if (d.mod == M_NONE) {
                buf_[v] = compose(d.base, want, d.tone, d.upper);
                normalizeTone();
                return true;
            }
            return false;                 // đã có dấu phụ khác -> 'w' thường
        }
    }
    if (cfg_.wAlwaysU) {                   // không có nguyên âm áp dụng -> chèn ư
        buf_ += compose('u', M_HORN, T_NONE, std::isupper((unsigned char)typed));
        return true;
    }
    return false;
}

bool Engine::handleCircum(char lc, char typed) {
    int n = (int)buf_.size();
    if (n >= 1) {
        Decomp d = decompose(buf_[n-1]);
        if (d.base == lc) {
            if (d.mod == M_NONE) {
                buf_[n-1] = compose(d.base, M_CIRC, d.tone, d.upper);
                normalizeTone();
                return true;
            }
            if (d.mod == M_CIRC) {        // gõ lần 3 -> bỏ mũ + chữ thật
                buf_[n-1] = compose(d.base, M_NONE, d.tone, d.upper);
                appendLiteral(typed);
                normalizeTone();
                return true;
            }
        }
    }
    return false;
}

bool Engine::handleDStroke(char typed) {
    int n = (int)buf_.size();
    if (n >= 1) {
        Decomp d = decompose(buf_[n-1]);
        if (d.base == 'd') {
            if (d.mod == M_NONE) {
                buf_[n-1] = compose('d', M_STROKE, T_NONE, d.upper);
                return true;
            }
            if (d.mod == M_STROKE) {      // đ + d -> dd
                buf_[n-1] = compose('d', M_NONE, T_NONE, d.upper);
                appendLiteral(typed);
                return true;
            }
        }
    }
    return false;
}

void Engine::key(char c) {
    raw_ += c;   // lưu phím thô để fallback tiếng Anh
    if (!std::isalpha((unsigned char)c)) { appendLiteral(c); return; }
    char lc = (char)std::tolower((unsigned char)c);
    bool handled = false;
    switch (lc) {
        case 's': case 'f': case 'r': case 'x': case 'j':
            handled = handleTone(toneOf(lc), c); break;
        case 'z':
            handled = handleRemoveTone(c); break;
        case 'w':
            handled = handleHorn(c); break;
        case 'a': case 'e': case 'o':
            handled = handleCircum(lc, c); break;
        case 'd':
            handled = handleDStroke(c); break;
        default: break;
    }
    if (!handled) { appendLiteral(c); normalizeTone(); }
}

void Engine::backspace() {
    if (!buf_.empty()) buf_.pop_back();
    if (!raw_.empty()) raw_.pop_back();
}

std::string Engine::preedit() const {
    std::string out;
    for (char32_t c : buf_) encodeOne(out, c);
    return out;
}

namespace {
// Phụ âm đầu hợp lệ (đ gộp vào 'd'). "" = không có phụ âm đầu.
const std::set<std::string> ONSETS = {
    "", "b","c","ch","d","g","gh","gi","h","k","kh","l","m","n","ng","ngh",
    "nh","p","ph","q","qu","r","s","t","th","tr","v","x"
};
// Phụ âm cuối hợp lệ.
const std::set<std::string> CODAS = { "", "c","ch","m","n","ng","nh","p","t" };

bool isPrefixOfSome(const std::set<std::string>& set, const std::string& s) {
    for (const auto& e : set)
        if (e.size() >= s.size() && e.compare(0, s.size(), s) == 0) return true;
    return false;
}
} // namespace

bool Engine::plausible() const {
    int n = (int)buf_.size();
    if (n == 0) return true;
    int i = 0;
    std::string onset, coda;
    while (i < n && !isVowelChar(buf_[i])) { onset += decompose(buf_[i]).base; ++i; }
    bool hasVowel = (i < n);
    while (i < n &&  isVowelChar(buf_[i])) ++i;          // cụm nguyên âm
    while (i < n && !isVowelChar(buf_[i])) { coda += decompose(buf_[i]).base; ++i; }
    if (i < n) return false;        // còn nguyên âm sau coda -> >1 âm tiết -> tiếng Anh

    if (!hasVowel) return isPrefixOfSome(ONSETS, onset);  // mới có phụ âm đầu
    if (!ONSETS.count(onset)) return false;
    return isPrefixOfSome(CODAS, coda);
}

// ================= Từ điển tiếng Anh =================
namespace {
std::unordered_set<std::string>& dictSet() {
    static std::unordered_set<std::string> s;
    return s;
}
std::unordered_set<std::string>& vnSet() {   // danh sách ép tiếng Việt
    static std::unordered_set<std::string> s;
    return s;
}
// Telex của vài từ Việt rất thường, trùng mặt chữ một từ Anh -> luôn giữ Việt.
const std::set<std::string> VN_PRIORITY = { "cos", "nos" };  // "có", "nó"

std::string lower(const std::string& s) {
    std::string r = s;
    for (char& c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}
} // namespace

void dictAddWord(const std::string& w) {
    if (!w.empty()) dictSet().insert(lower(w));
}
void dictLoadFile(const std::string& path) {
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.back()=='\r' || line.back()==' ' || line.back()=='\t'))
            line.pop_back();
        size_t i = 0;
        while (i < line.size() && (line[i]==' ' || line[i]=='\t')) ++i;
        if (i) line = line.substr(i);
        if (!line.empty() && line[0] != '#') dictAddWord(line);
    }
}
bool dictContains(const std::string& w) {
    return dictSet().count(lower(w)) > 0;
}

void vnAddWord(const std::string& w) {
    if (!w.empty()) vnSet().insert(lower(w));
}
void vnLoadFile(const std::string& path) {
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.back()=='\r' || line.back()==' ' || line.back()=='\t'))
            line.pop_back();
        size_t i = 0;
        while (i < line.size() && (line[i]==' ' || line[i]=='\t')) ++i;
        if (i) line = line.substr(i);
        if (!line.empty() && line[0] != '#') vnAddWord(line);
    }
}
bool vnContains(const std::string& w) {
    return vnSet().count(lower(w)) > 0;
}

// Từ đang gõ sẽ hiện ra dạng thô (Anh) hay bản tiếng Việt?
bool Engine::isShowingRaw() const {
    if (forcedRaw_) return true;                         // Esc ép thô
    if (forcedVN_)  return false;                        // Esc ép Việt
    std::string low = lower(raw_);
    if (VN_PRIORITY.count(low) || vnContains(low)) return false;  // ép Việt
    if (dictContains(low)) return true;                  // từ điển Anh
    return !plausible();                                 // còn lại: theo cấu trúc
}

std::string Engine::display() const {
    return isShowingRaw() ? raw_ : preedit();
}

std::string typeString(const std::string& keys, Config cfg) {
    Engine e(cfg);
    std::string out;
    for (char ch : keys) {
        if (std::isalpha((unsigned char)ch)) {
            e.key(ch);
        } else {
            out += e.display();
            e.reset();
            out += ch;
        }
    }
    out += e.display();
    return out;
}

} // namespace vgo
