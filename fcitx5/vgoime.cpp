#include "vgoime.h"

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

// Số codepoint trong chuỗi UTF-8.
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

} // namespace

VgoEngine::VgoEngine(Instance *instance)
    : instance_(instance),
      factory_([](InputContext &) { return new VgoState; }) {
    instance_->inputContextManager().registerProperty("vgoState", &factory_);
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
    // Chế độ preedit (fallback): nội dung chưa thật trong document -> commit.
    // Chế độ surrounding: đã commit sẵn tại chỗ -> không làm gì.
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

    // Phím tắt (Ctrl/Alt/Super) -> chốt từ rồi cho phím đi tiếp.
    if (key.states().testAny(
            KeyStates{KeyState::Ctrl, KeyState::Alt, KeyState::Super})) {
        finalize(ic, state);
        return;
    }

    // Backspace
    if (key.check(FcitxKey_BackSpace)) {
        if (state->engine.empty()) {
            return; // không soạn dở -> backspace bình thường
        }
        if (state->usingPreedit) {
            state->engine.backspace();
            showPreedit(ic, state);
            keyEvent.filterAndAccept();
        } else {
            // text đã thật trong document: quên trạng thái, để app tự xoá 1 ký tự.
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
        std::string next = state->engine.display();

        if (ic->capabilityFlags().test(CapabilityFlag::SurroundingText)) {
            // KHÔNG preedit: ghi-đè tại chỗ theo diff.
            size_t offB = 0;
            int common = commonPrefixCp(state->shown, next, offB);
            int toDelete = cpCount(state->shown) - common;
            if (toDelete > 0) {
                ic->deleteSurroundingText(-toDelete, toDelete);
            }
            if (offB < next.size()) {
                ic->commitString(next.substr(offB));
            }
            state->shown = next;
            state->usingPreedit = false;
        } else {
            // fallback preedit (vd terminal)
            state->usingPreedit = true;
            state->shown.clear();
            showPreedit(ic, state);
        }
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
