#include "vgoime.h"

#include <cctype>
#include <cstdlib>      // getenv
#include <fstream>
#include <filesystem>

#include <fcitx/addoninstance.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
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
    vgo::dictLoadFile("/usr/share/vgo/en-base.txt");  // base Anh chuẩn (ship sẵn)
    vgo::dictLoadFile(userDictPath());                // Anh: lớp cá nhân (tự học)
    vgo::vnLoadFile("/usr/share/vgo/vn-base.txt");    // ép-Việt: base (ship sẵn)
    vgo::vnLoadFile(userVNDictPath());                // ép-Việt: lớp cá nhân (tự học)
}

void appendLine(const std::string &path, const std::string &w) {
    try {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    } catch (...) {}
    std::ofstream f(path, std::ios::app);
    if (f) f << w << "\n";
}
// Esc "dạy" từ: thêm vào danh sách tương ứng + ghi file cá nhân (nếu chưa có).
void learnEnglish(std::string w) {
    for (char &c : w) c = (char)std::tolower((unsigned char)c);
    if (w.size() < 2 || vgo::dictContains(w)) return;
    vgo::dictAddWord(w);
    appendLine(userDictPath(), w);
}
void learnVN(std::string w) {
    for (char &c : w) c = (char)std::tolower((unsigned char)c);
    if (w.size() < 2 || vgo::vnContains(w)) return;
    vgo::vnAddWord(w);
    appendLine(userVNDictPath(), w);
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
        if (toDelete > 0) ic->deleteSurroundingText(-toDelete, toDelete);
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

void VgoEngine::finalize(InputContext *ic, VgoState *state) {
    if (state->usingPreedit && !state->engine.empty()) {
        ic->commitString(state->engine.display());
        ic->inputPanel().reset();
        ic->updatePreedit();
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
    }
    state->engine.reset();
    state->shown.clear();
    state->usingPreedit = false;
}

void VgoEngine::keyEvent(const InputMethodEntry &, KeyEvent &keyEvent) {
    if (keyEvent.isRelease()) {
        return;
    }
    auto *ic = keyEvent.inputContext();
    auto *state = ic->propertyFor(&factory_);
    Key key = keyEvent.key();

    // Nút LẬT 2 CHIỀU (Esc khi đang gõ dở): Anh <-> Việt + tự nhớ.
    // Dùng phím không-modifier vì app nuốt tổ hợp Ctrl trước khi tới vgo.
    if (key.check(FcitxKey_Escape)) {
        if (!state->engine.empty()) {
            std::string word = state->engine.raw();
            if (state->engine.isShowingRaw()) {   // đang ra Anh -> lật sang Việt
                state->engine.forceVN();
                learnVN(word);
            } else {                              // đang ra Việt -> lật sang Anh
                state->engine.forceRaw();
                learnEnglish(word);
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
    finalize(ic, state);
}

void VgoEngine::reset(const InputMethodEntry &, InputContextEvent &event) {
    auto *ic = event.inputContext();
    auto *state = ic->propertyFor(&factory_);
    state->engine.reset();
    state->shown.clear();
    state->usingPreedit = false;
    ic->inputPanel().reset();
    ic->updatePreedit();
    ic->updateUserInterface(UserInterfaceComponent::InputPanel);
}

FCITX_ADDON_FACTORY(VgoEngineFactory)
