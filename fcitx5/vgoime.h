// vgo — fcitx5 input method addon (bọc engine Telex thuần).
// Chế độ chính: KHÔNG preedit — commit thẳng + xoá-ghi-đè tại chỗ bằng
// surrounding text (giống UniKey/Lotus, không phá autofill). Tối ưu remote:
// in-process, ghi-đè theo diff (chỉ phần đuôi đổi), tự track ký tự đã commit.
// App không hỗ trợ surrounding text -> fallback preedit (vd terminal).
#ifndef VGO_FCITX5_VGOIME_H
#define VGO_FCITX5_VGOIME_H

#include <string>

#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>

#include "engine.hpp"

// Trạng thái soạn thảo riêng từng ô nhập.
class VgoState : public fcitx::InputContextProperty {
public:
    vgo::Engine engine;
    std::string shown;          // text đã commit cho từ đang gõ (chế độ surrounding)
    bool usingPreedit = false;  // true nếu ô này phải fallback preedit
};

class VgoEngine : public fcitx::InputMethodEngineV2 {
public:
    explicit VgoEngine(fcitx::Instance *instance);

    void keyEvent(const fcitx::InputMethodEntry &entry,
                  fcitx::KeyEvent &keyEvent) override;
    void reset(const fcitx::InputMethodEntry &entry,
               fcitx::InputContextEvent &event) override;

private:
    void showPreedit(fcitx::InputContext *ic, VgoState *state);  // fallback
    void finalize(fcitx::InputContext *ic, VgoState *state);     // chốt từ + dọn

    fcitx::Instance *instance_;
    fcitx::FactoryFor<VgoState> factory_;
};

class VgoEngineFactory : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override {
        return new VgoEngine(manager->instance());
    }
};

#endif
