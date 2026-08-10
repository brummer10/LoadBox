/*
 * PluginAPI.h
 *
 * Generic, format-agnostic interface that VST3Wrapper.cpp,
 * VST2Wrapper.cpp and CLAPWrapper.cpp are written against.
 * To reuse these wrappers unchanged in a new project you only need to:
 *
 *   1. Implement IPluginClient for your plugin/engine
 *   2. Fill in a PluginDescriptor instance
 *   3. Implement getPluginDescriptor() and createPluginInstance()
 *      (declared at the bottom of this file)
 *
 * VST3Wrapper.cpp, VST2Wrapper.cpp and CLAPWrapper.cpp
 * themselves should not need any modification.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <string>
#include <vector>

#include "Parameter.h" // Parameter / Params - already format-agnostic

// Opaque native window handle: X11 Window (Linux), HWND (Windows),
// NSView* (macOS). The wrapper never dereferences this itself, it only
// forwards it to the plugin implementation, which knows what it is.
using WindowHandle = void*;

// Descriptor: everything a host needs to list and instantiate the
// plugin. Covers VST3, VST2 and CLAP metadata together.

// A "variant" is an independently instantiable plugin that shares the
// same engine/GUI class but e.g. runs in a different processing mode
// A plugin without such variants simply declares exactly one entry.
struct PluginVariantInfo {
    const char* id;                 // stable, unique id, e.g.
                                     // "com.vendor.PluginName.Variant"
                                     // used as-is as the CLAP plugin id
    const char* name;                // display name
    const char* description;         // short one-liner
    uint8_t     vst3Uid[16] = {0};   // 16 raw bytes for the VST3
                                      // class_id; leave as {0} for
                                      // CLAP-only plugins
    int32_t     vst2UniqueId = 0;    // classic VST2 FourCC unique id
    std::vector<int> hiddenParams;   // parameter indices hidden from
                                      // the host in this variant (e.g.
                                      // parameters that have no effect 
                                      // in this variant)
};

struct PluginDescriptor {
    const char* vendor;
    const char* url;
    const char* email;

    const char* vst3Category;       // e.g. "Audio Module Class"
    const char* vst3SubCategories;  // e.g. "Fx|EQ"
    const char* vst3SdkVersion;     // e.g. "VST 3.7.9"

    const char* const *clapFeature;        // e.g. CLAP_PLUGIN_FEATURE_AUDIO_EFFECT

    const char* version;

    // Fixed channel count of the plugin's single main audio bus, on
    // each side. 1 = mono, 2 = stereo. Wrappers build their bus/
    // port descriptions (VST3 speaker arrangement, CLAP audio ports)
    // from these two numbers
    int numInputChannels = 2;
    int numOutputChannels = 2;

    std::vector<PluginVariantInfo> variants; // at least 1 entry
};

// IPluginClient: the interface every plugin implements exactly once,
// shared by all format wrappers (VST3, VST2, CLAP, ...).
class IPluginClient {
public:
    virtual ~IPluginClient() = default;

    // lifecycle
    virtual void initEngine(uint32_t sampleRate, int32_t priority, int32_t policy) = 0;

    // Called on creation and again on every setup/activate, so the
    // engine knows which variant (see PluginVariantInfo) is currently
    // active.
    virtual void selectVariant(int variantIndex) = 0;

    virtual uint32_t getLatencySamples() const = 0;

    // audio processing
    // Generic multi-channel, in-place capable. inputs/outputs are arrays
    // of channel pointers; numInputs/numOutputs match
    // PluginDescriptor::numInputChannels/numOutputChannels (the wrapper
    // builds these arrays from the host's actual bus buffers each block).
    virtual void process(uint32_t nframes, float* const* inputs, uint32_t numInputs,
                                            float* const* outputs, uint32_t numOutputs) = 0;

    // parameters
    virtual Params& params() = 0;

    // Called by the wrapper right after a parameter value has been
    // applied due to host/user automation. This is where any
    // plugin-specific side-effect logic belongs
    // which used to be hard-wired into the wrappers.
    virtual void onParameterChanged(int /*id*/, double /*value*/) {}

    // Text representation of a parameter value. Default is "%.2f" /
    // atof, which is fine for most numeric parameters - override where
    // needed (e.g. for Hz/dB units or enum text).
    virtual void valueToText(int /*id*/, double value, char* out, size_t outSize) const {
        std::snprintf(out, outSize, "%.2f", value);
    }
    virtual double textToValue(int /*id*/, const char* text) const {
        return std::atof(text);
    }

    // state
    virtual void readState(const std::string& state) = 0;
    virtual void saveState(std::string* state) = 0;

    // GUI
    // Own top-level window without a host parent (e.g. CLAP "floating").
    virtual void startGui() = 0;
    // GUI embedded into a host window.
    virtual void startGui(WindowHandle parent) = 0;
    virtual void showGui() = 0;
    virtual void hideGui() = 0;
    virtual void quitGui() = 0;
    virtual void setParent(WindowHandle parent) = 0;
    virtual void getGuiSize(int& width, int& height) const = 0;
    virtual bool resizeGui(int width, int height) = 0; // false = not possible

    // Optional HiDPI scaling; defaults to "not supported".
    virtual bool setGuiScale(double /*scale*/) { return false; }
};

// Factory functions every plugin implements exactly once.
const PluginDescriptor& getPluginDescriptor();
IPluginClient* createPluginInstance(int variantIndex);
