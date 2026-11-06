#include "vgoime.h"

#include <cctype>
#include <cstdlib>      // getenv
#include <fstream>
#include <filesystem>

#include <fcitx/addoninstance.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <fcitx/text.h>

using namespace fcitx;

namespace {

// Độ dài (byte) của 1 codepoint UTF-8 tính từ byte đầu.
int cpLen(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c >> 5) == 0x6) return 2;
    if ((c >> 4) == 0xE) return 3;
    return 4;
}
int cpCount(const std::string &s) {
    int n = 0;
    for (size_t i = 0; i < s.size(); i += cpLen((unsigned char)s[i])) ++n;
    return n;
}
// Số codepoint trùng ở đầu giữa a và b; đặt offB = byte-offset tương ứng trong b.
int commonPrefixCp(const std::string &a, const std::string &b, size_t &offB) {
    size_t i = 0, j = 0;
    int cp = 0;
    while (i < a.size() && j < b.size()) {
        int la = cpLen((unsigned char)a[i]);
        int lb = cpLen((unsigned char)b[j]);
        if (la != lb || a.compare(i, la, b, j, lb) != 0) break;
        i += la;
        j += lb;
        ++cp;
    }
    offB = j;
    return cp;
}

// Thư mục cấu hình cá nhân: $XDG_CONFIG_HOME/vgo (mặc định ~/.config/vgo)
std::string cfgDir() {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) return std::string(xdg) + "/vgo";
    const char *home = getenv("HOME");
    return std::string(home ? home : "") + "/.config/vgo";
}
std::string userDictPath()   { return cfgDir() + "/words.txt"; }     // học: tiếng Anh
std::string userVNDictPath() { return cfgDir() + "/vn-words.txt"; }  // học: ép tiếng Việt
std::string userBigramPath() { return cfgDir() + "/bigram.txt"; }    // bigram cá nhân (sửa tay)

// App nền Chromium/Electron (Chrome, VS Code, Slack…) cài đặt surrounding-text
// xoá lỗi -> phải dùng preedit cho chúng.
bool isChromiumApp(const std::string &prog) {
    std::string p;
    for (char c : prog) p += (char)std::tolower((unsigned char)c);
    auto has = [&](const char *s) { return p.find(s) != std::string::npos; };
    return has("chrom") || has("electron") || has("slack") ||
           has("discord") || has("code");
}

bool g_dictLoaded = false;
void loadDictOnce() {
    if (g_dictLoaded) return;
    g_dictLoaded = true;
    vgo::dictLoadFile("/usr/share/vgo/en-base.txt", true);  // base Anh chuẩn (ship sẵn)
    vgo::dictLoadFile(userDictPath());                // Anh: lớp cá nhân (tự học)
    vgo::vnLoadFile("/usr/share/vgo/vn-base.txt");    // ép-Việt: base (ship sẵn)
    vgo::vnLoadFile(userVNDictPath());                // ép-Việt: lớp cá nhân (tự học)
    vgo::vnLexLoadFile("/usr/share/vgo/vn-syllables.txt");  // âm tiết + tần suất: tự sửa gõ nhầm
    vgo::bigramLoadFile("/usr/share/vgo/bigram-base.txt");  // ngữ cảnh từ trước: base
    vgo::bigramLoadFile(userBigramPath());                  // ngữ cảnh: lớp cá nhân
}

void appendLine(const std::string &path, const std::string &w) {
    try {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    } catch (...) {}
    std::ofstream f(path, std::ios::app);
    if (f) f << w << "\n";
}
// Xoá mọi dòng đúng bằng w khỏi file cá nhân.
void removeLine(const std::string &path, const std::string &w) {
    std::ifstream in(path);
    if (!in) return;
    std::string out, line;
    bool found = false;
    while (std::getline(in, line)) {
        if (line == w) { found = true; continue; }
        out += line + "\n";
    }
    in.close();
    if (!found) return;
    std::ofstream f(path, std::ios::trunc);
    if (f) f << out;
}
// Esc "dạy" từ: thêm vào danh sách tương ứng + ghi file cá nhân (nếu chưa có),
// và GỠ khỏi danh sách bên kia — lần lật gần nhất thắng (trước đây từ kẹt ở cả 2
// file thì ép-Việt luôn thắng, lật về Anh không bao giờ ăn: "this is" -> "thí í").
void learnEnglish(std::string w) {
    for (char &c : w) c = (char)std::tolower((unsigned char)c);
    if (w.size() < 2) return;
    if (vgo::vnContains(w)) { vgo::vnRemoveWord(w); removeLine(userVNDictPath(), w); }
    if (vgo::dictContains(w)) return;
    vgo::dictAddWord(w);
    appendLine(userDictPath(), w);
}
void learnVN(std::string w) {
    for (char &c : w) c = (char)std::tolower((unsigned char)c);
    if (w.size() < 2) return;
    // Gỡ khỏi lớp Anh cá nhân (bộ nhớ + file) ngay, không đợi restart. Base giữ
    // nguyên: luật "từ trước ra Anh" vẫn cần biết "is" là từ Anh.
    vgo::dictRemoveWord(w);
    removeLine(userDictPath(), w);
    if (vgo::vnContains(w)) return;
    vgo::vnAddWord(w);
    appendLine(userVNDictPath(), w);
}

// Esc có từ trước -> chỉ nhớ ĐÚNG CẶP này (bigram), không nhớ cả từ:
// lật "xe bus" sang Anh không làm "cho con bus" hết ra "bú".
void learnBigram(std::string prev, std::string w, bool english) {
    for (char &c : prev) c = (char)std::tolower((unsigned char)c);
    for (char &c : w) c = (char)std::tolower((unsigned char)c);
    if (w.size() < 2 || vgo::bigramLookup(prev, w) == (english ? 1 : -1)) return;
    vgo::bigramAdd(prev, w, english);
    appendLine(userBigramPath(), prev + " " + w + (english ? " en" : " vn"));
}

} // namespace

VgoEngine::VgoEngine(Instance *instance)
    : instance_(instance),
      factory_([](InputContext &) { return new VgoState; }) {
    instance_->inputContextManager().registerProperty("vgoState", &factory_);
    loadDictOnce();
}

// Áp engine.display() ra ô nhập: không-preedit (ghi-đè surrounding theo diff),
// hoặc fallback preedit nếu app không hỗ trợ surrounding text.
void VgoEngine::render(InputContext *ic, VgoState *state) {
    std::string next = state->engine.display();
    // Chromium/Electron: surrounding-text xoá lỗi -> dùng preedit (else-branch).
    bool canCommit = ic->capabilityFlags().test(CapabilityFlag::SurroundingText) &&
                     !isChromiumApp(ic->program());
    if (canCommit) {
        size_t offB = 0;
        int common = commonPrefixCp(state->shown, next, offB);
        int toDelete = cpCount(state->shown) - common;
        if (toDelete > 0) {
            // An toàn: KHÔNG xoá quá số ký tự thực có trước con trỏ. Nếu state->shown
            // lệch pha với ô nhập thật (app tự xoá field, cursor nhảy, focus đổi, phím
            // rớt qua RDP…), toDelete có thể lớn hơn text thật -> mutter
            // meta_wayland_text_input_focus_delete_surrounding assert (before >= start)
            // -> abort -> sập cả GNOME shell. Clamp theo surroundingText: xấu nhất chỉ
            // sót vài ký tự (cosmetic), KHÔNG bao giờ sập session.
            const auto &sur = ic->surroundingText();
            if (sur.isValid() && toDelete > static_cast<int>(sur.cursor()))
                toDelete = static_cast<int>(sur.cursor());
            if (toDelete > 0) ic->deleteSurroundingText(-toDelete, toDelete);
        }
        if (offB < next.size()) ic->commitString(next.substr(offB));
        state->shown = next;
        state->usingPreedit = false;
    } else {
        state->usingPreedit = true;
        state->shown.clear();
        showPreedit(ic, state);
    }
}

void VgoEngine::showPreedit(InputContext *ic, VgoState *state) {
    std::string s = state->engine.display();
    Text preedit;
    preedit.append(s, TextFormatFlag::Underline);
    preedit.setCursor(static_cast<int>(s.size()));
    if (ic->capabilityFlags().test(CapabilityFlag::Preedit)) {
        ic->inputPanel().setClientPreedit(preedit);
    } else {
        ic->inputPanel().setPreedit(preedit);
    }
    ic->updatePreedit();
    ic->updateUserInterface(UserInterfaceComponent::InputPanel);
}

// boundary = ký tự ranh giới vừa gõ (space, dấu câu) để giữ ngữ cảnh cho bigram;
// 0 = phím khác (Enter, mũi tên, Ctrl+…) -> con trỏ có thể đã nhảy, bỏ ngữ cảnh.
void VgoEngine::finalize(InputContext *ic, VgoState *state, uint32_t boundary) {
    // Chốt bản tự sửa bằng space/dấu câu -> cho phép Backspace hoàn tác (chỉ chế độ
    // surrounding; preedit = Chromium, xoá text đã commit không tin được).
    state->canRevert = boundary && !state->usingPreedit && !state->engine.empty() &&
                       state->engine.isCorrected();
    if (state->canRevert) {
        state->revRaw = state->engine.raw();
        state->revShown = state->engine.display();
        state->revSep = utf8::UCS4ToUTF8(boundary);
    }
    if (state->usingPreedit && !state->engine.empty()) {
        ic->commitString(state->engine.display());
        ic->inputPanel().reset();
        ic->updatePreedit();
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
    }
    state->engine.endWord(boundary);
    if (!boundary) state->engine.clearContext();
    state->shown.clear();
    state->usingPreedit = false;
}

// Backspace ngay sau khi chốt bản tự sửa: thay "còn " bằng "ccofn " (LatinIME
// revertCommit). Chỉ làm khi text thật trước con trỏ khớp ĐÚNG cái đã chốt — lệch pha
// (app tự sửa text, con trỏ nhảy, phím rớt qua RDP) thì trả false để Backspace thường,
// không bao giờ xoá mù (xem clamp trong render()). Không học gì: lỡ tay 1 lần không được
// tắt vĩnh viễn tự sửa cho từ đó; muốn dạy thì Esc.
bool VgoEngine::revertCorrection(InputContext *ic, VgoState *state) {
    if (!state->canRevert) return false;
    state->canRevert = false;
    const std::string committed = state->revShown + state->revSep;
    const auto &sur = ic->surroundingText();
    if (!sur.isValid() || sur.anchor() != sur.cursor()) return false;
    const std::string &text = sur.text();
    size_t cur = 0;
    for (unsigned i = 0; i < sur.cursor() && cur < text.size(); ++i)
        cur += cpLen((unsigned char)text[cur]);
    if (cur < committed.size() ||
        text.compare(cur - committed.size(), committed.size(), committed) != 0)
        return false;
    int len = cpCount(committed);
    ic->deleteSurroundingText(-len, len);
    ic->commitString(state->revRaw + state->revSep);
    state->engine.clearContext();
    return true;
}

void VgoEngine::keyEvent(const InputMethodEntry &, KeyEvent &keyEvent) {
    if (keyEvent.isRelease()) {
        return;
    }
    auto *ic = keyEvent.inputContext();
    auto *state = ic->propertyFor(&factory_);
    Key key = keyEvent.key();
    // Hoàn tác tự sửa chỉ dành cho Backspace NGAY sau khi chốt
    if (!key.check(FcitxKey_BackSpace)) state->canRevert = false;

    // Nút LẬT 2 CHIỀU (Esc khi đang gõ dở): Anh <-> Việt + tự nhớ.
    // Dùng phím không-modifier vì app nuốt tổ hợp Ctrl trước khi tới vgo.
    if (key.check(FcitxKey_Escape)) {
        if (!state->engine.empty()) {
            std::string word = state->engine.raw();
            const std::string &prev = state->engine.prev();  // rỗng = đầu dòng / mất ngữ cảnh
            if (state->engine.isShowingRaw()) {   // đang ra Anh -> lật sang Việt
                // Chỉ LƯU khi bản Việt là âm tiết (hoặc tiền tố) hợp lệ / viết tắt không
                // nguyên âm ("đc"). Gõ nhầm ("ccofn") thì lật lần này thôi: lưu typo chỉ
                // làm bẩn danh sách — tự sửa gõ nhầm đã lo phần đó.
                bool keep = state->engine.plausible() ||
                            word.find_first_of("aeiouyAEIOUY") == std::string::npos;
                state->engine.forceVN();
                if (keep) { if (prev.empty()) learnVN(word); else learnBigram(prev, word, false); }
            } else {                              // đang ra Việt -> lật sang Anh
                state->engine.forceRaw();
                if (prev.empty()) learnEnglish(word); else learnBigram(prev, word, true);
            }
            render(ic, state);
            keyEvent.filterAndAccept();
            return;
        }
        return;  // không gõ dở -> để app xử Esc bình thường
    }

    // Phím tắt khác (Ctrl/Alt/Super) -> chốt từ rồi cho phím đi tiếp.
    if (key.states().testAny(
            KeyStates{KeyState::Ctrl, KeyState::Alt, KeyState::Super})) {
        finalize(ic, state);
        return;
    }

    // Backspace
    if (key.check(FcitxKey_BackSpace)) {
        if (state->engine.empty()) {
            if (revertCorrection(ic, state)) {
                keyEvent.filterAndAccept();
                return;
            }
            state->engine.clearContext();  // app xoá chữ đã chốt -> ngữ cảnh không còn đúng
            return;
        }
        if (state->usingPreedit) {
            state->engine.backspace();
            showPreedit(ic, state);
            keyEvent.filterAndAccept();
        } else {
            // No-preedit: lùi 1 phím rồi GHI-ĐÈ lại app text (cùng cơ chế gõ thường),
            // KHÔNG reset — để "bị"+lỡ'x'="bĩ" backspace về "bị" thay vì để app tụt xuống "b".
            state->engine.backspace();
            render(ic, state);   // diff-overwrite: sửa đuôi; rỗng thì render xoá nốt
            keyEvent.filterAndAccept();
        }
        return;
    }

    // Chữ cái a-z / A-Z -> nạp engine.
    uint32_t code = Key::keySymToUnicode(key.sym());
    bool isLetter = (code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z');
    if (isLetter) {
        state->engine.key(static_cast<char>(code));
        render(ic, state);
        keyEvent.filterAndAccept();
        return;
    }

    // Ranh giới từ (space, dấu câu, Enter, mũi tên…) -> chốt rồi cho đi tiếp.
    bool printable = code >= 0x20 && code != 0x7f;
    finalize(ic, state, printable ? code : 0);
}

void VgoEngine::reset(const InputMethodEntry &, InputContextEvent &event) {
    auto *ic = event.inputContext();
    auto *state = ic->propertyFor(&factory_);
    state->engine.reset();
    state->engine.clearContext();
    state->shown.clear();
    state->usingPreedit = false;
    ic->inputPanel().reset();
    ic->updatePreedit();
    ic->updateUserInterface(UserInterfaceComponent::InputPanel);
}

FCITX_ADDON_FACTORY(VgoEngineFactory)
