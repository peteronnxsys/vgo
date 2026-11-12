#include "vni.hpp"
#include "viet.hpp"
#include <cctype>
#include <vector>

namespace vgo {
using namespace core;

namespace {

bool hasDigit(const std::u32string& s) {
    for (char32_t c : s) if (c >= '0' && c <= '9') return true;
    return false;
}

// Cụm nguyên âm cuối [start, end]; end = -1 nếu chưa có nguyên âm.
void lastVowelRun(const std::u32string& s, int& start, int& end) {
    end = (int)s.size() - 1;
    while (end >= 0 && !isVowelChar(s[end])) --end;
    start = end;
    while (start > 0 && isVowelChar(s[start-1])) --start;
}

// "oo" chỉ có trước ng/c (xoong, boong, moóc): "room"+2 không được thành "roòm".
bool ooOk(const std::u32string& s) {
    int start, end;
    lastVowelRun(s, start, end);
    if (end < 1 || end - start != 1) return true;
    if (decompose(s[start]).base != 'o' || decompose(s[end]).base != 'o') return true;
    std::string coda;
    for (int i = end + 1; i < (int)s.size(); ++i) coda += decompose(s[i]).base;
    return coda.empty() || coda == "n" || coda == "ng" || coda == "c";
}

} // namespace

void VniEngine::appendLiteral(char c) {
    buf_ += compose((char)std::tolower((unsigned char)c), M_NONE, T_NONE,
                    std::isupper((unsigned char)c));
}

void VniEngine::normalizeTone() { retone(buf_, cfg_.oldStyleTone); }

void VniEngine::key(char c) {
    raw_ += c;
    if (c >= '0' && c <= '9') {
        // Đã có số thô trong từ ("h264", "a1" sau hoàn tác) -> số tiếp theo cũng thô.
        if (hasDigit(buf_) || !applyDigit(c - '0', c)) appendLiteral(c);
        return;
    }
    appendLiteral(c);
    if (!std::isalpha((unsigned char)c)) return;
    // "uơ" (mở) + chữ gõ tiếp -> "ươ": "huo7"+"ng" -> "hương"
    int n = (int)buf_.size();
    if (n >= 3) {
        Decomp du = decompose(buf_[n-3]), dov = decompose(buf_[n-2]);
        bool afterQ = (n >= 4 && decompose(buf_[n-4]).base == 'q');
        if (du.base=='u' && du.mod==M_NONE && dov.base=='o' && dov.mod==M_HORN && !afterQ)
            buf_[n-3] = compose('u', M_HORN, du.tone, du.upper);
    }
    normalizeTone();
}

// Áp số d lên buf_. Hoàn tác (gõ lại cùng dấu) luôn nhận; biến đổi thường chỉ nhận
// khi kết quả vẫn là âm tiết Việt (hoặc tiền tố) — không thì trả false để số ra thô.
bool VniEngine::applyDigit(int d, char typed) {
    std::u32string save = buf_;
    size_t before = buf_.size();
    bool ok = false;
    switch (d) {
        case 1: case 2: case 3: case 4: case 5: ok = handleTone(d, typed); break;
        case 0: ok = handleRemoveTone(); break;
        case 6: ok = handleCircum(typed); break;
        case 7: ok = handleHorn(typed); break;
        case 8: ok = handleBreve(typed); break;
        case 9: ok = handleDStroke(typed); break;
    }
    if (!ok) return false;
    if (buf_.size() > before) return true;     // hoàn tác: đã nhả số thô
    bool closed = false;                       // đã có phụ âm cuối -> kiểm cả âm tiết
    int start, end;
    lastVowelRun(buf_, start, end);
    closed = end >= 0 && end < (int)buf_.size() - 1;
    if (!plausible(buf_) || (closed && !complete(buf_, cfg_.oldStyleTone)) || !ooOk(buf_)) {
        buf_ = save;
        return false;
    }
    return true;
}

bool VniEngine::handleTone(int tone, char typed) {
    int tgt = findToneTarget(buf_, cfg_.oldStyleTone);
    if (tgt < 0) return false;            // không có nguyên âm -> số thường
    int curTone = T_NONE, curPos = -1;
    for (int i = 0; i < (int)buf_.size(); ++i) {
        Decomp d = decompose(buf_[i]);
        if (isVowelBase(d.base) && d.tone != T_NONE) { curTone = d.tone; curPos = i; break; }
    }
    if (curPos >= 0) {                    // gỡ dấu cũ
        Decomp d = decompose(buf_[curPos]);
        buf_[curPos] = compose(d.base, d.mod, T_NONE, d.upper);
    }
    if (curTone == tone) {                // gõ lại cùng số -> bỏ dấu + nhả số thật
        appendLiteral(typed);
        return true;
    }
    Decomp d = decompose(buf_[tgt]);
    buf_[tgt] = compose(d.base, d.mod, tone, d.upper);
    return true;
}

bool VniEngine::handleRemoveTone() {
    for (int i = 0; i < (int)buf_.size(); ++i) {
        Decomp d = decompose(buf_[i]);
        if (isVowelBase(d.base) && d.tone != T_NONE) {
            buf_[i] = compose(d.base, d.mod, T_NONE, d.upper);
            return true;
        }
    }
    return false;                         // không có dấu -> số 0 thường
}

// 6: mũ cho a/e/o gần cuối nhất trong cụm nguyên âm ("tie6ng", "uo6ng", "toi6").
bool VniEngine::handleCircum(char typed) {
    int start, end;
    lastVowelRun(buf_, start, end);
    for (int i = end; i >= start && i >= 0; --i) {
        Decomp d = decompose(buf_[i]);
        if (d.base != 'a' && d.base != 'e' && d.base != 'o') continue;
        if (d.mod == M_CIRC) {            // gõ lại -> bỏ mũ + số thật
            buf_[i] = compose(d.base, M_NONE, d.tone, d.upper);
            appendLiteral(typed);
        } else {                          // trần / ă / ơ -> mũ
            buf_[i] = compose(d.base, M_CIRC, d.tone, d.upper);
            if (d.base == 'o' && i > start && decompose(buf_[i-1]).mod == M_HORN) {
                Decomp du = decompose(buf_[i-1]);   // "ươ" + 6 -> "uô"
                buf_[i-1] = compose(du.base, M_NONE, du.tone, du.upper);
            }
        }
        normalizeTone();
        return true;
    }
    return false;
}

// 7: móc, cùng luật chọn nguyên âm với 'w' của Telex nhưng không làm trăng cho 'a'.
bool VniEngine::handleHorn(char typed) {
    int n = (int)buf_.size();
    int start, end;
    lastVowelRun(buf_, start, end);
    if (end < 0) return false;
    auto baseAt = [&](int i){ return decompose(buf_[i]).base; };
    // âm đệm "qu" / "gi" + nguyên âm: u/i thuộc phụ âm đầu, không nhận móc
    if (start == 1 && end > 1 &&
        ((baseAt(0)=='q' && baseAt(1)=='u') || (baseAt(0)=='g' && baseAt(1)=='i')))
        start = 2;

    std::vector<int> tg;
    for (int i = start; i < end; ++i)
        if (baseAt(i)=='u' && baseAt(i+1)=='o') { tg = {i, i+1}; break; }
    if (tg.empty() && end > start) {
        char b0 = baseAt(start), b1 = baseAt(start+1);
        if ((b0=='u' && (b1=='a'||b1=='i'||b1=='u')) || (b0=='o' && b1=='i'))
            tg = {start};                                              // ưa ưi ưu ơi
    }
    if (tg.empty()) {                      // còn lại: o/u gần cuối nhất
        for (int i = end; i >= start; --i)
            if (baseAt(i)=='o' || baseAt(i)=='u') { tg = {i}; break; }
        if (tg.empty()) return false;
    }

    if (tg.size() == 2) {                  // cặp u-o
        int u = tg[0], o = tg[1];
        Decomp du = decompose(buf_[u]), dov = decompose(buf_[o]);
        if (dov.mod == M_HORN) {           // gõ lại -> bỏ móc cả cặp + số thật
            buf_[u] = compose('u', M_NONE, du.tone, du.upper);
            buf_[o] = compose('o', M_NONE, dov.tone, dov.upper);
            appendLiteral(typed);
            normalizeTone();
            return true;
        }
        // Âm tiết mở "uo"+7 -> "uơ" (thuở, huơ); có chữ theo sau -> "ươ" (xem key()).
        int umod = (o == n - 1 && du.mod == M_NONE) ? M_NONE : M_HORN;
        buf_[u] = compose('u', umod, du.tone, du.upper);
        buf_[o] = compose('o', M_HORN, dov.tone, dov.upper);
        normalizeTone();
        return true;
    }

    int v = tg[0];
    Decomp d = decompose(buf_[v]);
    if (d.mod == M_HORN) {                 // gõ lại -> bỏ móc + số thật
        buf_[v] = compose(d.base, M_NONE, d.tone, d.upper);
        appendLiteral(typed);
    } else {
        buf_[v] = compose(d.base, M_HORN, d.tone, d.upper);
    }
    normalizeTone();
    return true;
}

// 8: trăng cho 'a' trong cụm nguyên âm ("a8n" -> "ăn", "hoa8c" -> "hoăc").
bool VniEngine::handleBreve(char typed) {
    int start, end;
    lastVowelRun(buf_, start, end);
    for (int i = end; i >= start && i >= 0; --i) {
        Decomp d = decompose(buf_[i]);
        if (d.base != 'a') continue;
        if (d.mod == M_BREVE) {            // gõ lại -> bỏ trăng + số thật
            buf_[i] = compose('a', M_NONE, d.tone, d.upper);
            appendLiteral(typed);
        } else {
            buf_[i] = compose('a', M_BREVE, d.tone, d.upper);
        }
        normalizeTone();
        return true;
    }
    return false;
}

// 9: đ cho 'd' đầu từ, gõ ở đâu trong từ cũng được ("d9i", "di9" -> "đi").
bool VniEngine::handleDStroke(char typed) {
    if (buf_.empty()) return false;
    Decomp d = decompose(buf_[0]);
    if (d.base != 'd') return false;
    if (d.mod == M_STROKE) {               // gõ lại -> bỏ gạch + số thật
        buf_[0] = compose('d', M_NONE, T_NONE, d.upper);
        appendLiteral(typed);
    } else {
        buf_[0] = compose('d', M_STROKE, T_NONE, d.upper);
    }
    return true;
}

void VniEngine::backspace() {
    if (raw_.empty()) return;
    std::string r = raw_;
    r.pop_back();                          // bỏ ĐÚNG phím thô cuối
    reset();
    for (char c : r) key(c);               // dựng lại từ phím còn lại
}

bool VniEngine::isShowingRaw() const {
    // "a4"/"A4" (khổ giấy): "ã" đứng một mình không phải từ -> giữ thô.
    // ("a5" -> "ạ" thì vẫn Việt: "vâng ạ".)
    bool raw = (raw_ == "a4" || raw_ == "A4");
    // Số đã thành dấu nhưng chữ gõ SAU làm hỏng âm tiết ("ma3"+"t" = "mảt") -> thô,
    // như UniKey. Có số thô trong buf_ ("a11" -> "a1") thì người gõ đã chọn rồi, để yên.
    if (!raw && raw_.find_first_of("0123456789") != std::string::npos && !hasDigit(buf_)) {
        int start, end;
        lastVowelRun(buf_, start, end);
        bool closed = end >= 0 && end < (int)buf_.size() - 1;
        raw = !plausible(buf_) || (closed && !complete(buf_, cfg_.oldStyleTone));
    }
    return flipped_ ? !raw : raw;
}

std::string VniEngine::display() const {
    if (isShowingRaw()) return raw_;
    std::string out;
    for (char32_t c : spellFix(buf_, cfg_.oldStyleTone)) encodeOne(out, c);
    return out;
}

std::string vniTypeString(const std::string& keys, VniConfig cfg) {
    VniEngine e(cfg);
    std::string out;
    for (char ch : keys) {
        if (std::isalnum((unsigned char)ch)) {
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
