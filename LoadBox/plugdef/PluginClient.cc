
/*
 * PluginClient.cc
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * 
 * Copyright (C) 2025 brummer <brummer@web.de>
 */

#include "LoadBox.cc"
#include "PluginAPI.h"

#if defined (VST3IPLUG)
#include "travesty/base.h"
#elif defined (CLAPIPLUG)
#include "clap/plugin-features.h"
#elif defined (VST2IPLUG)
#include "vestige.h"
#endif

#include <cstring>


class LoadBoxClient : public IPluginClient {
public:
    void initEngine(uint32_t sampleRate, int32_t priority, int32_t policy) override {
        irLoader.initEngine(sampleRate, priority, policy);
    }

    void selectVariant(int /*variantIndex*/) override {}

    uint32_t getLatencySamples() const override {
        uint32_t latency = 0;
        const_cast<LoadBox&>(irLoader).getLatency(&latency);
        return latency;
    }

    void process(uint32_t nframes, float* const* inputs, uint32_t numInputs,
                                    float* const* outputs, uint32_t numOutputs) override {

        float* const left = numOutputs > 0 ? outputs[0] : nullptr;
        float* const right = numOutputs > 1 ? outputs[1] : left;
        const float* const inL = numInputs > 0 ? inputs[0] : nullptr;
        const float* const inR = numInputs > 1 ? inputs[1] : inL;

        // true stereo: left input feeds the left IR/NAM path, right input
        // feeds the right IR/NAM path. A mono host input is duplicated to
        // both channels as a fallback.
        if (inL != nullptr && left != nullptr && left != inL)
            std::memcpy(left, inL, nframes * sizeof(float));
        if (inR != nullptr && right != nullptr && right != inR)
            std::memcpy(right, inR, nframes * sizeof(float));

        irLoader.process(nframes, left, right);
    }

    Params& params() override { return irLoader.param; }

    void readState(const std::string& state) override { irLoader.readState(state); }
    void saveState(std::string* state) override { irLoader.saveState(state); }

    void startGui() override { irLoader.startGui(); }

    void startGui(WindowHandle parent) override {
        irLoader.startGui(reinterpret_cast<Window>(parent));
    }

    void showGui() override { irLoader.showGui(); }
    void hideGui() override { irLoader.hideGui(); }
    void quitGui() override { irLoader.quitGui(); }

    void setParent(WindowHandle parent) override {
        irLoader.setParent(reinterpret_cast<Window>(parent));
    }

    void getGuiSize(int& width, int& height) const override {
        if (irLoader.TopWin != nullptr) {
            width = static_cast<int>(irLoader.TopWin->width);
            height = static_cast<int>(irLoader.TopWin->height);
        } else {
            width = 610;
            height = 160;
        }
    }

    bool resizeGui(int width, int height) override {
        if (irLoader.TopWin == nullptr) return false;
        os_resize_window(irLoader.getMain()->dpy, irLoader.TopWin, width, height);
        return true;
    }

    bool setGuiScale(double scale) override {
        irLoader.getMain()->hdpi = scale;
        return true;
    }

private:
    LoadBox irLoader;
};

// PluginDescriptor
static PluginDescriptor buildDescriptor() {
    PluginDescriptor d{};
    d.vendor           = "brummer10";
    d.url              = "https://github.com/brummer10/LoadBox";
    d.email            = "mailto:brummer-@web.de";

#if defined (VST3IPLUG)
    d.vst3Category      = "Audio Module Class";
    d.vst3SubCategories = "Fx|Tools";
    d.vst3SdkVersion    = "VST 3.7.9";
#elif defined (CLAPIPLUG)
    static const char *features[] = { CLAP_PLUGIN_FEATURE_AUDIO_EFFECT , CLAP_PLUGIN_FEATURE_UTILITY, NULL};
    d.clapFeature      = features;
#endif
    d.version          = "0.1.0";

    d.numInputChannels  = 2;
    d.numOutputChannels = 2;

    PluginVariantInfo main{};
    main.id            = "com.brummer10.LoadBox";
    main.name           = "LoadBox";
    main.description    = "Stereo impulse response (and NAM outboard profile) loader";

#if defined (VST3IPLUG)
    {
        v3_tuid uid = V3_ID(0x420e073e, 0x98cb4ccd, 0x8c402315, 0x8030a433);
        std::memcpy(main.vst3Uid, uid, sizeof(main.vst3Uid));
    }
#endif

#if defined (VST2IPLUG)
    main.vst2UniqueId = CCONST('I', 'r', 'L', 'd');
#endif
    d.variants = { main };
    return d;
}

const PluginDescriptor& getPluginDescriptor() {
    static const PluginDescriptor d = buildDescriptor();
    return d;
}

IPluginClient* createPluginInstance(int /*variantIndex*/) {
    return new LoadBoxClient();
}
