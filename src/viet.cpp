// Lõi chữ Việt dùng chung cho vgo (Telex) và vgo-vni: bảng dấu, đặt thanh, kiểm âm tiết.
#include "viet.hpp"
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <set>
#include <cctype>

namespace vgo {
namespace core {
namespace {

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

} // namespace

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

namespace {
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

} // namespace

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
int findToneTarget(const std::u32string& s, bool oldStyleTone) {
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
        return oldStyleTone ? pen : last;
    return pen;
}

// Dời dấu thanh (nếu có) về đúng nguyên âm theo findToneTarget.
void retone(std::u32string& s, bool oldStyleTone) {
    int tone = T_NONE, pos = -1;
    for (int i = 0; i < (int)s.size(); ++i) {
        Decomp d = decompose(s[i]);
        if (isVowelBase(d.base) && d.tone != T_NONE) { tone = d.tone; pos = i; break; }
    }
    if (tone == T_NONE) return;
    for (int i = 0; i < (int)s.size(); ++i) {
        Decomp d = decompose(s[i]);
        if (isVowelBase(d.base) && d.tone != T_NONE)
            s[i] = compose(d.base, d.mod, T_NONE, d.upper);
    }
    int tgt = findToneTarget(s, oldStyleTone);
    if (tgt < 0) tgt = pos;
    Decomp d = decompose(s[tgt]);
    s[tgt] = compose(d.base, d.mod, tone, d.upper);
}

// Ép nguyên âm đôi theo chính tả (chỉ lúc hiển thị, buf_ giữ nguyên để 'w'/'e'
// gõ sau vẫn áp được). Tiếng Việt không có các vần trần này khi có chữ theo sau:
//   ie/ye + phụ âm/u -> iê/yê   ("tiengs"->"tiếng", "yeu"->"yêu", "gieng"->"giêng")
//   ưo + chữ         -> ươ      ("huwongs"->"hướng", "ruwou"->"rượu")
//   uo + phụ âm cuối -> uô      ("muons"->"muốn"); "uoi" mơ hồ (tuổi/người) -> để yên
std::u32string spellFix(std::u32string s, bool oldStyleTone) {
    int n = (int)s.size();
    int a = 0;
    while (a < n && !isVowelChar(s[a])) ++a;
    bool changed = false;
    // i+2 < n: cặp phải có chữ theo sau (cặp ở cuối = âm tiết còn mở, để yên)
    for (int i = a; i + 2 < n && isVowelChar(s[i+1]); ++i) {
        Decomp d0 = decompose(s[i]), d1 = decompose(s[i+1]);
        bool nextCons = !isVowelChar(s[i+2]);
        bool nextU = decompose(s[i+2]).base == 'u';
        // "ye" chỉ đứng đầu vần (yên) hoặc sau u (uyên); "dyeing" thì không
        bool yeOk = d0.base=='i' || i == a || decompose(s[i-1]).base == 'u';
        int want = M_NONE;
        if ((d0.base=='i' || d0.base=='y') && yeOk && d0.mod==M_NONE &&
            d1.base=='e' && d1.mod==M_NONE && (nextCons || nextU))
            want = M_CIRC;
        else if (d0.base=='u' && d0.mod==M_HORN && d1.base=='o' && d1.mod==M_NONE)
            want = M_HORN;
        else if (d0.base=='u' && d0.mod==M_NONE && d1.base=='o' && d1.mod==M_NONE && nextCons)
            want = M_CIRC;
        if (want != M_NONE) {
            s[i+1] = compose(d1.base, want, d1.tone, d1.upper);
            changed = true;
            break;
        }
    }
    if (changed) retone(s, oldStyleTone);
    return s;
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

bool plausible(const std::u32string& buf) {
    int n = (int)buf.size();
    if (n == 0) return true;
    int i = 0;
    std::string onset, coda;
    while (i < n && !isVowelChar(buf[i])) { onset += decompose(buf[i]).base; ++i; }
    bool hasVowel = (i < n);
    while (i < n &&  isVowelChar(buf[i])) ++i;          // cụm nguyên âm
    while (i < n && !isVowelChar(buf[i])) { coda += decompose(buf[i]).base; ++i; }
    if (i < n) return false;        // còn nguyên âm sau coda -> >1 âm tiết -> tiếng Anh

    if (!hasVowel) return isPrefixOfSome(ONSETS, onset);  // mới có phụ âm đầu
    if (!ONSETS.count(onset)) return false;
    return isPrefixOfSome(CODAS, coda);
}

namespace {
// Vần (cụm nguyên âm, bỏ thanh; đã tách âm đệm của qu/gi) -> có được theo sau
// bởi phụ âm cuối không: 'O' = chỉ mở, 'C' = phải có cuối, 'B' = cả hai.
const std::unordered_map<std::string, char> NUCLEI = {
    {"a",'B'}, {"ă",'C'}, {"â",'C'}, {"e",'B'}, {"ê",'B'}, {"i",'B'}, {"o",'B'},
    {"ô",'B'}, {"ơ",'B'}, {"u",'B'}, {"ư",'B'}, {"y",'O'}, {"oo",'C'},
    {"ai",'O'}, {"ao",'O'}, {"au",'O'}, {"ay",'O'}, {"âu",'O'}, {"ây",'O'},
    {"eo",'O'}, {"êu",'O'}, {"ia",'O'}, {"iu",'O'}, {"oi",'O'}, {"ôi",'O'},
    {"ơi",'O'}, {"ui",'O'}, {"ưi",'O'}, {"ưu",'O'}, {"ưa",'O'}, {"ua",'O'},
    {"uơ",'O'}, {"oa",'B'}, {"oe",'B'}, {"uê",'B'}, {"uy",'B'}, {"oă",'C'},
    {"uâ",'C'}, {"iê",'C'}, {"yê",'C'}, {"uô",'C'}, {"ươ",'C'},
    {"oai",'O'}, {"oay",'O'}, {"oeo",'O'}, {"uây",'O'}, {"uôi",'O'}, {"uya",'O'},
    {"uyu",'O'}, {"ươi",'O'}, {"ươu",'O'}, {"iêu",'O'}, {"yêu",'O'}, {"uyê",'C'},
};
} // namespace

// Âm tiết TRỌN hợp lệ (không chỉ là tiền tố): chặt hơn plausible() — kiểm cả vần
// và vài luật chính tả. Dùng cho ứng viên sửa lỗi gõ.
bool complete(const std::u32string& buf, bool oldStyleTone) {
    std::u32string s = spellFix(buf, oldStyleTone);
    int n = (int)s.size(), i = 0;
    std::string onset, coda;
    std::vector<Decomp> nuc;
    while (i < n && !isVowelChar(s[i])) { onset += decompose(s[i]).base; ++i; }
    while (i < n &&  isVowelChar(s[i])) nuc.push_back(decompose(s[i++]));
    while (i < n && !isVowelChar(s[i])) { coda += decompose(s[i]).base; ++i; }
    if (i < n || nuc.empty() || !ONSETS.count(onset) || !CODAS.count(coda)) return false;
    // qu / gi + nguyên âm: u / i thuộc phụ âm đầu
    if (onset == "q") {
        if (nuc[0].base != 'u' || nuc[0].mod != M_NONE || nuc.size() < 2) return false;
        nuc.erase(nuc.begin());
    } else if (onset == "g" && nuc[0].base == 'i' && nuc.size() >= 2) {
        nuc.erase(nuc.begin());
        onset = "gi";                    // "giết", "giêng": ê sau gi, không phải g+e
    }
    std::string key;
    int tone = T_NONE;
    for (auto& d : nuc) {
        encodeOne(key, compose(d.base, d.mod, T_NONE, false));
        if (d.tone != T_NONE) tone = d.tone;
    }
    auto it = NUCLEI.find(key);
    if (it == NUCLEI.end()) return false;
    if (coda.empty() ? it->second == 'C' : it->second == 'O') return false;
    char v0 = nuc[0].base;
    bool front = (v0 == 'i' || v0 == 'e' || v0 == 'y');
    if ((onset == "k" || onset == "gh" || onset == "ngh") && !front) return false;
    if ((onset == "c" || onset == "ng") && front) return false;
    if (onset == "g" && v0 == 'e') return false;
    const Decomp& vl = nuc.back();
    if ((coda == "ch" || coda == "nh") &&
        !((vl.base == 'a' && vl.mod == M_NONE) || (vl.base == 'e' && vl.mod == M_CIRC) ||
          vl.base == 'i' || vl.base == 'y'))
        return false;
    // vần tắc (c/ch/p/t) chỉ mang sắc / nặng (hoặc chưa gõ thanh)
    if ((coda == "c" || coda == "ch" || coda == "p" || coda == "t") &&
        tone != T_NONE && tone != 1 && tone != 5)
        return false;
    return true;
}

} // namespace core
} // namespace vgo
