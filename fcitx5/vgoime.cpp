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

// Đường dẫn từ điển cá nhân (mày tự nuôi): $XDG_CONFIG_HOME/vgo/words.txt
std::string userDictPath() {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) return std::string(xdg) + "/vgo/words.txt";
    const char *home = getenv("HOME");
    return std::string(home ? home : "") + "/.config/vgo/words.txt";
}

bool g_dictLoaded = false;
void loadDictOnce() {
    if (g_dictLoaded) return;
    g_dictLoaded = true;
    vgo::dictLoadFile("/usr/share/vgo/en-base.txt");  // base chuẩn (ship sẵn)
    vgo::dictLoadFile(userDictPath());                // lớp cá nhân (tự học)
}

// Nút restore "dạy" từ: thêm vào từ điển + ghi vào file cá nhân (nếu chưa có).
void learnWord(std::string w) {
    for (char &c : w) c = (char)std::tolower((unsigned char)c);
    if (w.size() < 2 || vgo::dictContains(w)) return;
    vgo::dictAddWord(w);
    std::string path = userDictPath();
    try {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    } catch (...) {}
    std::ofstream f(path, std::ios::app);
    if (f) f << w << "\n";
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
    if (ic->capabilityFlags().test(CapabilityFlag::SurroundingText)) {
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

    // Nút RESTORE (Esc khi đang gõ dở): khoá từ về thô + học vào từ điển cá nhân.
    // Dùng phím không-modifier vì app nuốt tổ hợp Ctrl trước khi tới vgo.
    if (key.check(FcitxKey_Escape)) {
        if (!state->engine.empty()) {
            std::string word = state->engine.raw();
            state->engine.forceRaw();
            render(ic, state);
            learnWord(word);
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
            state->engine.reset();
            state->shown.clear();
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
