#include "engine.hpp"
#include "viet.hpp"
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <fstream>
#include <cctype>
#include <cstring>
#include <cmath>
#include <sstream>

namespace vgo {
namespace {

// Lõi chữ Việt (bảng dấu, đặt thanh, kiểm âm tiết) nằm ở viet.cpp, dùng chung với vgo-vni.
using namespace core;
int findToneTarget(const std::u32string& s, const Config& c) { return core::findToneTarget(s, c.oldStyleTone); }
void retone(std::u32string& s, const Config& c) { core::retone(s, c.oldStyleTone); }
std::u32string spellFix(std::u32string s, const Config& c) { return core::spellFix(std::move(s), c.oldStyleTone); }

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

void Engine::normalizeTone() { retone(buf_, cfg_); }

bool Engine::handleTone(int tone, char typed) {
    int tgt = findToneTarget(buf_, cfg_);
    if (tgt < 0) return false;            // không có nguyên âm -> ký tự thường

    int curTone = T_NONE, curPos = -1;
    for (int i = 0; i < (int)buf_.size(); ++i) {
        Decomp d = decompose(buf_[i]);
        if (isVowelBase(d.base) && d.tone != T_NONE) { curTone = d.tone; curPos = i; break; }
    }
    if (curTone == tone) {                // gõ đúp cùng phím dấu -> bỏ dấu + nhả chữ thật
        Decomp d = decompose(buf_[curPos]);   // (chuẩn Telex như UniKey/phone/Lotus)
        buf_[curPos] = compose(d.base, d.mod, T_NONE, d.upper);
        appendLiteral(typed);
        normalizeTone();
        return true;
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

// 'w' áp lên CẢ CỤM nguyên âm (không chỉ nguyên âm cuối), để gõ 'w' sau phụ âm
// cuối / nguyên âm sau vẫn đúng: "cuarw"->"cửa", "thuongwr"->"thưởng", "nguoiwf"->"người".
bool Engine::handleHorn(char typed) {
    int n = (int)buf_.size();
    int end = n - 1;                       // nguyên âm cuối của cụm
    while (end >= 0 && !isVowelChar(buf_[end])) --end;
    if (end < 0) {
        if (cfg_.wAlwaysU) {               // không có nguyên âm áp dụng -> chèn ư
            buf_ += compose('u', M_HORN, T_NONE, std::isupper((unsigned char)typed));
            return true;
        }
        return false;
    }
    int start = end;
    while (start > 0 && isVowelChar(buf_[start-1])) --start;
    auto baseAt = [&](int i){ return decompose(buf_[i]).base; };
    // âm đệm "qu" / "gi" + nguyên âm: u/i thuộc phụ âm đầu, không nhận sừng
    if (start == 1 && end > 1 &&
        ((baseAt(0)=='q' && baseAt(1)=='u') || (baseAt(0)=='g' && baseAt(1)=='i')))
        start = 2;

    std::vector<int> tg;                   // vị trí nhận dấu phụ
    for (int i = start; i < end; ++i)
        if (baseAt(i)=='u' && baseAt(i+1)=='o') { tg = {i, i+1}; break; }
    if (tg.empty() && end > start) {
        char b0 = baseAt(start), b1 = baseAt(start+1);
        if (b0=='o' && b1=='a') tg = {start+1};                        // oă (hoặc)
        else if ((b0=='u' && (b1=='a'||b1=='i'||b1=='u')) ||
                 (b0=='o' && b1=='i')) tg = {start};                   // ưa ưi ưu ơi
    }
    if (tg.empty()) {                      // còn lại: nguyên âm cuối là a/o/u
        char b = baseAt(end);
        if (b!='a' && b!='o' && b!='u') return false;
        tg = {end};
    }

    if (tg.size() == 2) {                  // cặp u-o
        int u = tg[0], o = tg[1];
        Decomp du = decompose(buf_[u]), dov = decompose(buf_[o]);
        if (du.mod != M_NONE && du.mod != M_HORN) return false;
        if (dov.mod == M_HORN) {           // gõ lại -> bỏ sừng cả cặp + chữ thật
            buf_[u] = compose('u', M_NONE, du.tone, du.upper);
            buf_[o] = compose('o', M_NONE, dov.tone, dov.upper);
            appendLiteral(typed);
            normalizeTone();
            return true;
        }
        if (dov.mod != M_NONE) return false;
        // Âm tiết mở "uo"+w -> "uơ" (thuở, huơ); có chữ theo sau -> "ươ".
        // "uơ" + chữ gõ tiếp sẽ được nâng thành "ươ" trong key().
        int umod = (o == n - 1 && du.mod == M_NONE) ? M_NONE : M_HORN;
        buf_[u] = compose('u', umod, du.tone, du.upper);
        buf_[o] = compose('o', M_HORN, dov.tone, dov.upper);
        normalizeTone();
        return true;
    }

    int v = tg[0];
    Decomp d = decompose(buf_[v]);
    int want = (d.base=='a') ? M_BREVE : M_HORN;
    if (d.mod == want) {                   // gõ lại -> bỏ sừng + chữ thật
        buf_[v] = compose(d.base, M_NONE, d.tone, d.upper);
        appendLiteral(typed);
        normalizeTone();
        return true;
    }
    if (d.mod != M_NONE) return false;     // đã có dấu phụ khác -> 'w' thường
    buf_[v] = compose(d.base, want, d.tone, d.upper);
    normalizeTone();
    return true;
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
            return false;
        }
    }
    // Không liền nhau: mũ cho nguyên âm cùng chữ trong cụm nguyên âm gần cuối
    // ("toio"->"tôi", "tiene"->"tiên", "daua"->"đâu").
    int end = n - 1;
    while (end >= 0 && !isVowelChar(buf_[end])) --end;
    if (end < 0) return false;
    int start = end;
    while (start > 0 && isVowelChar(buf_[start-1])) --start;
    // "oe" mở + 'o' = vần "oeo" thật (ngoẹo, khoèo) -> chữ thường
    if (lc == 'o' && end == n - 1 && end - start == 1 &&
        decompose(buf_[start]).base == 'o' && decompose(buf_[end]).base == 'e')
        return false;
    for (int i = end; i >= start; --i) {
        Decomp d = decompose(buf_[i]);
        if (d.base != lc) continue;
        if (d.mod != M_NONE) return false;  // đã có mũ/dấu phụ -> chữ thường
        buf_[i] = compose(d.base, M_CIRC, d.tone, d.upper);
        normalizeTone();
        return true;
    }
    return false;
}

bool Engine::handleDStroke(char typed) {
    int n = (int)buf_.size();
    if (n >= 1) {
        Decomp d = decompose(buf_[n-1]);
        if (d.base == 'd') {
            // "dd" ở cuối từ bắt đầu bằng 'd' + nguyên âm -> đ đầu từ ("didd"->"đi")
            Decomp d0 = decompose(buf_[0]);
            if (d.mod == M_NONE && n >= 3 && d0.base == 'd' && d0.mod == M_NONE &&
                isVowelChar(buf_[1])) {
                buf_.pop_back();
                buf_[0] = compose('d', M_STROKE, T_NONE, d0.upper);
                return true;
            }
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
    if (!handled) {
        appendLiteral(c);
        // "uơ" (mở) + chữ gõ tiếp -> "ươ": "nguow"+"i" -> "ngươi", "huow"+"ng" -> "hương"
        int n = (int)buf_.size();
        if (n >= 3) {
            Decomp du = decompose(buf_[n-3]), dov = decompose(buf_[n-2]);
            bool afterQ = (n >= 4 && decompose(buf_[n-4]).base == 'q');
            if (du.base=='u' && du.mod==M_NONE && dov.base=='o' && dov.mod==M_HORN && !afterQ)
                buf_[n-3] = compose('u', M_HORN, du.tone, du.upper);
        }
        normalizeTone();
    }
}

void Engine::backspace() {
    if (raw_.empty()) {                  // không còn phím thô -> xoá ghép cuối (an toàn)
        if (!buf_.empty()) buf_.pop_back();
        return;
    }
    std::string r = raw_;
    r.pop_back();                        // bỏ ĐÚNG phím thô cuối (kể cả phím lỡ dư)
    raw_.clear();
    buf_.clear();
    for (char c : r) key(c);             // DỰNG LẠI buf_ + raw_ từ chuỗi phím còn lại
}                                        // "bị"+lỡ'x'="bĩ" -> backspace -> "bị" (không tụt "bi"/"b")

std::string Engine::preedit() const {
    if (!plausible()) {                  // "nahawns" -> "nhắn" (Esc lật về thô)
        std::string fix = correction();
        if (!fix.empty()) return fix;
    }
    std::string out;
    for (char32_t c : spellFix(buf_, cfg_)) encodeOne(out, c);
    return out;
}

bool Engine::plausible() const { return core::plausible(buf_); }

// Âm tiết TRỌN hợp lệ (không chỉ là tiền tố): chặt hơn plausible() — kiểm cả vần
// và vài luật chính tả. Dùng cho ứng viên sửa lỗi gõ.
bool Engine::complete() const { return core::complete(buf_, cfg_.oldStyleTone); }

namespace {
// Số cặp liền nhau là phím Telex đúp (aa/ee/oo/dd) — edit tạo thêm cặp mới thì
// ứng viên là "ghép nhầm": "alafm" đảo 'a','l' -> "laafm" -> "lầm".
int telexPairs(const std::string& r) {
    int k = 0;
    for (size_t i = 0; i + 1 < r.size(); ++i) {
        char a = (char)std::tolower((unsigned char)r[i]);
        if (a == (char)std::tolower((unsigned char)r[i+1]) &&
            (a == 'a' || a == 'e' || a == 'o' || a == 'd'))
            ++k;
    }
    return k;
}
// Có nguyên âm a/e/o gõ lặp KHÔNG liền nhau ("asay" -> "ấy", "beepers" -> "bếp"):
// mũ không liền là tiện ích lúc gõ, làm chỗ dựa cho bản sửa thì quá yếu.
bool farCircumflex(const std::string& r) {
    for (size_t j = 2; j < r.size(); ++j) {
        char c = (char)std::tolower((unsigned char)r[j]);
        if (c != 'a' && c != 'e' && c != 'o') continue;
        for (size_t k = 0; k + 1 < j; ++k)
            if (std::tolower((unsigned char)r[k]) == c) return true;
    }
    return false;
}
// Hai phím nằm sát nhau trên bàn phím QWERTY (cùng hàng hoặc hàng kề, lệch < 1 phím).
bool qwertyNear(char a, char b) {
    static const char* ROWS[3] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
    static const double OFF[3] = { 0, 0.25, 0.75 };
    a = (char)std::tolower((unsigned char)a);
    b = (char)std::tolower((unsigned char)b);
    int ra = -1, rb = -1;
    double xa = 0, xb = 0;
    for (int r = 0; r < 3; ++r) {
        if (const char* p = std::strchr(ROWS[r], a)) { ra = r; xa = OFF[r] + (p - ROWS[r]); }
        if (const char* p = std::strchr(ROWS[r], b)) { rb = r; xb = OFF[r] + (p - ROWS[r]); }
    }
    if (ra < 0 || rb < 0 || a == b) return false;
    double dx = xa > xb ? xa - xb : xb - xa;
    return ra == rb ? dx == 1 : (ra - rb == 1 || rb - ra == 1) && dx < 1;
}
bool sameKey(char a, char b) {
    return std::tolower((unsigned char)a) == std::tolower((unsigned char)b);
}

// Chi phí lỗi lấy từ AOSP LatinIME (native/jni/src/suggest/policyimpl/typing/
// scoring_params.cpp + typing_weighting.h). Tên LatinIME tính theo TỪ ĐÚNG:
// "insertion" = gõ thừa 1 phím (ta xoá), "omission" = gõ thiếu (ta chèn).
constexpr float INSERTION_COST            = 0.7248f;
constexpr float INSERTION_COST_SAME_CHAR  = 0.5508f;
constexpr float INSERTION_COST_PROXIMITY_CHAR = 0.674f;
constexpr float DISTANCE_WEIGHT_LANGUAGE  = 1.1214f;
constexpr float TYPING_MAX_OUTPUT_SCORE_PER_INPUT = 0.1f;
constexpr float AUTOCORRECT_THRESHOLD     = 0.185f;   // mức "modest" (config-auto-correction-thresholds.xml)
// LatinIME chưa làm ("TODO: ... nearly equals ... behave less aggressive"): bản sửa
// nhì có điểm >= MARGIN * bản nhất -> mơ hồ, không sửa. ("cvuj": vụ 0.25 vs cụ 0.16
// -> vẫn sửa thành "vụ", vì vụ gặp nhiều hơn cụ ~13 lần.)
constexpr float AMBIGUITY_MARGIN          = 0.8f;
} // namespace

// Tự sửa 1 lỗi gõ theo mô hình LatinIME: xoá 1 phím thừa, gõ lại qua Engine, chỉ giữ âm
// tiết CÓ TRONG từ điển âm tiết; điểm = 1 - (chi phí lỗi + độ hiếm × 1.12) /
// (1.12 + 0.1 × số phím), × (1 - 1/độ dài) (calcNormalizedScore); nhận bản nhất nếu
// >= ngưỡng, không sát nút bản nhì, và không có từ Anh nào rẻ bằng.
// Khác LatinIME vì vgo gõ song ngữ không báo trước, trên bàn phím thật + Telex. Đo
// trên vn-words.txt cá nhân (lỗi thật) + 74.7k từ /usr/share/dict/words (sửa nhầm):
//   - CHỈ xoá phím thừa. Thay/chèn phím: từ Anh ngoài từ điển nào cũng cách 1 âm tiết
//     Việt đúng 1 phím thay (1279/1314 ca sửa nhầm), trên lỗi thật cũng hay đoán sai
//     ("nhhir" -> "nghỉ", "hheest" -> "chết"). Đảo phím: đưa 'w'/'s' ra sau nguyên âm
//     thành dấu ("swan" -> "săn"), lỗi thật không cần.
//   - chỉ xoá TRƯỚC nguyên âm cuối: đuôi từ là chỗ phím thanh + phụ âm cuối, va với
//     đuôi tiếng Anh ("cods" -> "có", "lass" -> "lá"); không xoá phím vừa gõ.
//   - chỉ xoá phím đúp (INSERTION_COST_SAME_CHAR) hoặc phím kề (PROXIMITY_CHAR) — phím
//     đầu từ chạm trượt ("jkhas", "bnos") là lỗi hay gặp nhất nên không phụ phí
//     INSERTION_COST_FIRST_CHAR như cảm ứng; INSERTION_COST thường chỉ cho nguyên âm
//     gõ sớm kẹp giữa phụ âm đầu ("nahawns").
//   - ứng viên tạo thêm cặp Telex đúp (aa/ee/oo/dd) hoặc dựa vào mũ không liền là
//     ghép nhầm, bỏ.
std::string Engine::correction() const {
    if (raw_ == corrRaw_) return corrOut_;
    corrRaw_ = raw_;
    corrOut_.clear();
    const std::string& r = raw_;
    int n = (int)r.size(), caps = 0;
    for (char c : r) {
        if (!std::isalpha((unsigned char)c)) return "";   // có số: gõ cẩn thận, không sửa
        if (std::isupper((unsigned char)c)) ++caps;
    }
    if (n < 2 || caps > 1) return "";                     // isMostlyCaps: không sửa

    struct Cand { float score = -1, cost = 0; std::string out; };
    std::unordered_map<std::string, Cand> byOut;         // bản hiển thị -> ứng viên điểm cao nhất
    float bestEnCost = 1e9f, maxDist = DISTANCE_WEIGHT_LANGUAGE +
                                       n * TYPING_MAX_OUTPUT_SCORE_PER_INPUT;
    int pairs = telexPairs(r);
    auto tryRaw = [&](const std::string& cand, float cost) {
        if (cand.empty() || telexPairs(cand) > pairs || farCircumflex(cand)) return;
        if (dictContains(cand)) { bestEnCost = std::min(bestEnCost, cost); return; }
        Engine e(cfg_);
        for (char c : cand) e.key(c);
        std::string key, out;                            // key = bản chữ thường để tra
        for (char32_t c : spellFix(e.buf_, cfg_)) {
            Decomp d = decompose(c);
            encodeOne(key, compose(d.base, d.mod, d.tone, false));
            encodeOne(out, c);
        }
        long cnt = vnLexCount(key);
        if (cnt <= 0) return;
        float improb = 1.0f - std::log((float)cnt) / std::log((float)vnLexMax());
        float score = 1.0f - (cost + improb * DISTANCE_WEIGHT_LANGUAGE) / maxDist;
        score *= 1.0f - 1.0f / (float)cand.size();         // calcNormalizedScore, edit distance 1
        Cand& b = byOut[out];
        if (score > b.score) b = {score, cost, out};
    };
    // Dạng biến tố của từ Anh ("warns", "sweeps", "liked") -> đang gõ Anh, không sửa
    std::string lo = r;
    for (char& c : lo) c = (char)std::tolower((unsigned char)c);
    for (const char* suf : { "s", "es", "ed", "d", "ing", "er", "ers", "est", "ly" }) {
        size_t k = std::strlen(suf);
        if (lo.size() > k + 2 && lo.compare(lo.size() - k, k, suf) == 0 &&
            dictContains(lo.substr(0, lo.size() - k)))
            return "";
    }
    auto isV = [&](int i) { return isVowelBase((char)std::tolower((unsigned char)r[i])); };
    int firstVowel = 0, lastVowel = n - 1;
    while (firstVowel < n && !isV(firstVowel)) ++firstVowel;
    while (lastVowel >= 0 && !isV(lastVowel)) --lastVowel;
    for (int i = 0; i < n; ++i) {
        if (i >= lastVowel) break;
        // gõ thừa phím i (LatinIME: insertion)
        bool same = i > 0 && sameKey(r[i-1], r[i]);   // đúp: bỏ phím sau ("Ccofn" -> "Còn")
        bool near = (i > 0 && qwertyNear(r[i-1], r[i])) || (i + 1 < n && qwertyNear(r[i+1], r[i]));
        // đúp aa/ee/oo/dd là phím dấu Telex: xoá 1 nửa là đổi dấu, không phải sửa lỗi
        // ("xuoowsng" -> "xướng" sai; đúng là bỏ 'w' kề 's' -> "xuống")
        if (same && std::strchr("aeod", std::tolower((unsigned char)r[i]))) same = false;
        // sau nguyên âm đầu: đúp phím thanh/móc là hoàn tác Telex ("thijj", "hurry"), còn
        // phím kề thì lẫn với chữ Anh ("hazy" -> "hay") -> chỉ nhận đúp phụ âm thường
        if (i > firstVowel) {
            near = false;
            if (std::strchr("sfrxjzw", std::tolower((unsigned char)r[i]))) same = false;
        }
        // nguyên âm gõ sớm 1 nhịp, kẹp giữa phụ âm đầu ("nahawns" -> "nhắn"). Đầu từ thì
        // không: từ Anh mở đầu bằng nguyên âm quá nhiều ("emoji" -> "mọi", "ether").
        bool early = i == firstVowel && i > 0 && i + 1 < n && !isV(i + 1);
        if (!same && !near && !early) continue;
        float del = same ? INSERTION_COST_SAME_CHAR
                  : near ? INSERTION_COST_PROXIMITY_CHAR : INSERTION_COST;
        tryRaw(r.substr(0, i) + r.substr(i + 1), del);
    }
    Cand best, second;
    for (auto& kv : byOut) {
        if (kv.second.score > best.score) { second = best; best = kv.second; }
        else if (kv.second.score > second.score) second = kv.second;
    }
    if (best.score < AUTOCORRECT_THRESHOLD) return "";
    if (second.score >= AMBIGUITY_MARGIN * best.score) return "";
    // Song ngữ (LatinIME không có): có từ Anh cách ĐÚNG 1 lỗi với chi phí không đắt hơn
    // -> có thể đang gõ Anh, không sửa ("rroof": "roof" vs "rồ").
    if (bestEnCost <= best.cost) return "";
    corrOut_ = best.out;
    return corrOut_;
}

// ================= Từ điển tiếng Anh =================
namespace {
std::unordered_set<std::string>& dictBaseSet() {   // chỉ en-base (ship sẵn)
    static std::unordered_set<std::string> s;
    return s;
}
std::unordered_set<std::string>& dictSet() {
    static std::unordered_set<std::string> s;
    return s;
}
std::unordered_set<std::string>& vnSet() {   // danh sách ép tiếng Việt
    static std::unordered_set<std::string> s;
    return s;
}
std::unordered_map<std::string, bool>& bigramMap() {   // "prev\traw" -> Anh?
    static std::unordered_map<std::string, bool> m;
    return m;
}
// Telex của vài từ Việt rất thường, trùng mặt chữ một từ Anh -> luôn giữ Việt.
const std::set<std::string> VN_PRIORITY = { "cos", "nos" };  // "có", "nó"

std::string lower(const std::string& s) {
    std::string r = s;
    for (char& c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}
} // namespace

void dictAddWord(const std::string& w, bool base) {
    if (w.empty()) return;
    dictSet().insert(lower(w));
    if (base) dictBaseSet().insert(lower(w));
}
void dictLoadFile(const std::string& path, bool base) {
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.back()=='\r' || line.back()==' ' || line.back()=='\t'))
            line.pop_back();
        size_t i = 0;
        while (i < line.size() && (line[i]==' ' || line[i]=='\t')) ++i;
        if (i) line = line.substr(i);
        if (!line.empty() && line[0] != '#') dictAddWord(line, base);
    }
}
bool dictContains(const std::string& w) {
    return dictSet().count(lower(w)) > 0;
}
bool dictBaseContains(const std::string& w) {
    return dictBaseSet().count(lower(w)) > 0;
}
void dictRemoveWord(const std::string& w) {
    if (!dictBaseContains(w)) dictSet().erase(lower(w));
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
void vnRemoveWord(const std::string& w) { vnSet().erase(lower(w)); }

namespace {
std::unordered_map<std::string, long>& lexMap() {   // âm tiết Việt -> số lần
    static std::unordered_map<std::string, long> m;
    return m;
}
long& lexMaxRef() { static long mx = 1; return mx; }
} // namespace
long vnLexMax() { return lexMaxRef(); }
void vnLexAdd(const std::string& syllable, long count) {
    if (syllable.empty() || count <= 0) return;
    long& c = lexMap()[syllable];
    c = std::max(c, count);
    lexMaxRef() = std::max(lexMaxRef(), count);
}
void vnLexLoadFile(const std::string& path) {
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream in(line);
        std::string w;
        long n;
        if (in >> w >> n) vnLexAdd(w, n);
    }
}
long vnLexCount(const std::string& syllable) {
    auto it = lexMap().find(syllable);
    return it == lexMap().end() ? 0 : it->second;
}

void bigramAdd(const std::string& prev, const std::string& raw, bool english) {
    if (!prev.empty() && !raw.empty()) bigramMap()[lower(prev) + "\t" + lower(raw)] = english;
}
void bigramLoadFile(const std::string& path) {
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream in(line);
        std::string prev, raw, lang;
        if (in >> prev >> raw >> lang && (lang == "en" || lang == "vn"))
            bigramAdd(prev, raw, lang == "en");
    }
}
int bigramLookup(const std::string& prev, const std::string& raw) {
    if (prev.empty()) return 0;
    auto it = bigramMap().find(lower(prev) + "\t" + lower(raw));
    if (it == bigramMap().end()) return 0;
    return it->second ? 1 : -1;
}

namespace {
// Từ IN HOA toàn bộ, ngắn (2–4 chữ): "NAS", "AWS"
bool isShortAllCaps(const std::string& s) {
    if (s.size() < 2 || s.size() > 4) return false;
    for (char c : s) if (!std::isupper((unsigned char)c)) return false;
    return true;
}
bool hasLower(const std::string& s) {
    for (char c : s) if (std::islower((unsigned char)c)) return true;
    return false;
}
} // namespace

void Engine::endWord(char32_t boundary) {
    if (!raw_.empty()) tokEn_ = isShowingRaw();
    tok_ += display();
    tokRaw_ += raw_;
    reset();
    if (boundary == ' ' || boundary == '\t' || boundary == '\n' || boundary == 0) {
        if (!tok_.empty()) { prev_ = tok_; prevRaw_ = tokRaw_; prevEn_ = tokEn_; }
        tok_.clear();
        tokRaw_.clear();
        tokEn_ = false;
    } else {
        encodeOne(tok_, boundary);
        encodeOne(tokRaw_, boundary);
    }
}

// Từ đang gõ sẽ hiện ra dạng thô (Anh) hay bản tiếng Việt?
bool Engine::isShowingRaw() const {
    if (forcedRaw_) return true;                         // Esc ép thô
    if (forcedVN_)  return false;                        // Esc ép Việt
    std::string low = lower(raw_);
    int b = bigramLookup(prev_, low);                    // ngữ cảnh: "xe bus", "it's been"
    if (!b && prevRaw_ != prev_) b = bigramLookup(prevRaw_, low);  // "has" đã hiện "há"
    if (b) return b > 0;
    if (VN_PRIORITY.count(low)) return false;
    // Từ trước ra Anh + từ này có trong từ điển Anh BASE -> theo Anh, thắng ép-Việt:
    // "is" một mình -> í, nhưng "this is"/"for us" -> Anh. (Không làm chiều ngược:
    // câu Việt hay chen từ Anh — "cần test", "có confirm".) Chỉ base: lớp tự học có
    // lẫn Esc nhầm — "thif" từng kẹt cả 2 file -> "unit test thì" ra "thif".
    if (prevEn_ && dictBaseContains(low)) return true;
    if (vnContains(low)) return false;                   // ép Việt
    if (dictContains(low)) return true;                  // từ điển Anh
    // Viết tắt IN HOA giữa câu thường ("mua NAS") -> thô. Câu IN HOA toàn bộ
    // ("VIỆT NAM VÀ") / đầu dòng thì vẫn Việt: không có từ trước chữ thường.
    if (isShortAllCaps(raw_) && hasLower(prev_)) return true;
    if (plausible()) return false;                       // còn lại: theo cấu trúc
    return correction().empty();                         // sửa được 1 lỗi gõ -> Việt
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
            e.endWord((unsigned char)ch);
            out += ch;
        }
    }
    out += e.display();
    return out;
}

} // namespace vgo
