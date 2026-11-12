#include "vniime.h"

#include <cctype>

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

// App nền Chromium/Electron (Chrome, VS Code, Slack…) cài đặt surrounding-text
// xoá lỗi -> phải dùng preedit cho chúng.
bool isChromiumApp(const std::string &prog) {
    std::string p;
    for (char c : prog) p += (char)std::tolower((unsigned char)c);
    auto has = [&](const char *s) { return p.find(s) != std::string::npos; };
    return has("chrom") || has("electron") || has("slack") ||
           has("discord") || has("code");
}

} // namespace

VniImEngine::VniImEngine(Instance *instance)
    : instance_(instance),
      factory_([](InputContext &) { return new VniState; }) {
    instance_->inputContextManager().registerProperty("vgoVniState", &factory_);
}

// Áp engine.display() ra ô nhập: không-preedit (ghi-đè surrounding theo diff),
// hoặc fallback preedit nếu app không hỗ trợ surrounding text.
void VniImEngine::render(InputContext *ic, VniState *state) {
    std::string next = state->engine.display();
    bool canCommit = ic->capabilityFlags().test(CapabilityFlag::SurroundingText) &&
                     !isChromiumApp(ic->program());
    if (canCommit) {
        size_t offB = 0;
        int common = commonPrefixCp(state->shown, next, offB);
        int toDelete = cpCount(state->shown) - common;
        if (toDelete > 0) {
            // Không xoá quá số ký tự thật trước con trỏ (lệch pha -> mutter assert ->
            // sập GNOME shell; xem vgoime.cpp).
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

void VniImEngine::showPreedit(InputContext *ic, VniState *state) {
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

void VniImEngine::finalize(InputContext *ic, VniState *state) {
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

void VniImEngine::keyEvent(const InputMethodEntry &, KeyEvent &keyEvent) {
    if (keyEvent.isRelease()) {
        return;
    }
    auto *ic = keyEvent.inputContext();
    auto *state = ic->propertyFor(&factory_);
    Key key = keyEvent.key();

    // Esc khi đang gõ dở: lật thô <-> tiếng Việt (chỉ từ này, không ghi nhớ).
    if (key.check(FcitxKey_Escape)) {
        if (!state->engine.empty()) {
            state->engine.flip();
            render(ic, state);
            keyEvent.filterAndAccept();
        }
        return;  // không gõ dở -> để app xử Esc bình thường
    }

    // Phím tắt khác (Ctrl/Alt/Super) -> chốt từ rồi cho phím đi tiếp.
    if (key.states().testAny(
            KeyStates{KeyState::Ctrl, KeyState::Alt, KeyState::Super})) {
        finalize(ic, state);
        return;
    }

    if (key.check(FcitxKey_BackSpace)) {
        if (state->engine.empty()) return;
        state->engine.backspace();
        if (state->usingPreedit) showPreedit(ic, state);
        else render(ic, state);   // diff-overwrite: sửa đuôi; rỗng thì render xoá nốt
        keyEvent.filterAndAccept();
        return;
    }

    // Chữ cái -> nạp engine. Số (hàng phím trên, không phải bàn phím số) chỉ nạp khi
    // đang gõ dở một từ: số đứng đầu ("2026") là số thường.
    uint32_t code = Key::keySymToUnicode(key.sym());
    bool isLetter = (code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z');
    bool isKeypad = key.sym() >= FcitxKey_KP_Space && key.sym() <= FcitxKey_KP_9;
    bool isDigit = code >= '0' && code <= '9' && !isKeypad && !state->engine.empty();
    if (isLetter || isDigit) {
        state->engine.key(static_cast<char>(code));
        render(ic, state);
        keyEvent.filterAndAccept();
        return;
    }

    // Ranh giới từ (space, dấu câu, Enter, mũi tên, số đầu từ…) -> chốt rồi cho đi tiếp.
    finalize(ic, state);
}

void VniImEngine::reset(const InputMethodEntry &, InputContextEvent &event) {
    auto *ic = event.inputContext();
    auto *state = ic->propertyFor(&factory_);
    state->engine.reset();
    state->shown.clear();
    state->usingPreedit = false;
    ic->inputPanel().reset();
    ic->updatePreedit();
    ic->updateUserInterface(UserInterfaceComponent::InputPanel);
}

FCITX_ADDON_FACTORY(VniImEngineFactory)
