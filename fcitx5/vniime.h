// vgo-vni — fcitx5 input method addon (bọc engine VNI thuần). Module riêng, không
// đụng vgo Telex: cùng cơ chế hiển thị (không preedit, ghi-đè theo diff; Chromium/
// Electron/terminal -> preedit), nhưng không từ điển, không tự học, không tự sửa.
#ifndef VGO_FCITX5_VNIIME_H
#define VGO_FCITX5_VNIIME_H

#include <string>

#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>

#include "vni.hpp"

// Trạng thái soạn thảo riêng từng ô nhập.
class VniState : public fcitx::InputContextProperty {
public:
    vgo::VniEngine engine;
    std::string shown;          // text đã commit cho từ đang gõ (chế độ surrounding)
    bool usingPreedit = false;  // true nếu ô này phải fallback preedit
};

class VniImEngine : public fcitx::InputMethodEngineV2 {
public:
    explicit VniImEngine(fcitx::Instance *instance);

    void keyEvent(const fcitx::InputMethodEntry &entry,
                  fcitx::KeyEvent &keyEvent) override;
    void reset(const fcitx::InputMethodEntry &entry,
               fcitx::InputContextEvent &event) override;

private:
    void render(fcitx::InputContext *ic, VniState *state);       // áp display() ra ô nhập
    void showPreedit(fcitx::InputContext *ic, VniState *state);  // fallback
    void finalize(fcitx::InputContext *ic, VniState *state);     // chốt từ + dọn

    fcitx::Instance *instance_;
    fcitx::FactoryFor<VniState> factory_;
};

class VniImEngineFactory : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override {
        return new VniImEngine(manager->instance());
    }
};

#endif
