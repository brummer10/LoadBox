/*
 * VST2Wrapper.cpp
 *
 * Generic VST2 (VeStige) wrapper. 
 * Does not need to be changed for a new plugin,
 * all that's required is implementing PluginAPI.h.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * 
 * Copyright (C) 2026 brummer <brummer@web.de>
 *
 */

#include "PluginAPI.h"
#include "vestige.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifndef PLUGIN_VARIANT_INDEX
#define PLUGIN_VARIANT_INDEX 0
#endif

#define FlagsChunks (1 << 5)

// Nominal VST2 buffer size for effGetParamName is only 8 chars
// (kVstMaxParamStrLen in the original Steinberg SDK); in practice
// virtually every modern host allocates a good deal more. We write up
// to VestigeMaxLabelLen-1 chars defensively, but keep parameter names
// short if you care about strict spec compliance.

typedef struct ERect {
    short top;
    short left;
    short bottom;
    short right;
} ERect;

struct wrap_plugin_t {
    AEffect* effect;
    IPluginClient* r;
    audioMasterCallback host;
    ERect editorRect;
    float sampleRate;
    std::string state;
    bool isInited;
    bool guiIsCreated;
};

/****************************************************************
 ** helper
 */

static double normalisedToPlain(const Parameter& p, double normalised) {
    return p.min + normalised * (p.max - p.min);
}

static double plainToNormalised(const Parameter& p, double plain) {
    const double range = p.max - p.min;
    return range != 0.0 ? (plain - p.min) / range : 0.0;
}

/****************************************************************
 ** Parameter handling
 */

static void setParameter(AEffect* effect, int32_t index, float value) {
    wrap_plugin_t* plug = (wrap_plugin_t*)effect->object;
    Params& params = plug->r->params();
    if (index < 0 || index >= params.getParamCount()) return;
    const double plain = normalisedToPlain(params.getParameter(index), value);
    params.setParam(index, plain);
    plug->r->onParameterChanged(index, plain);
}

static float getParameter(AEffect* effect, int32_t index) {
    wrap_plugin_t* plug = (wrap_plugin_t*)effect->object;
    Params& params = plug->r->params();
    if (index < 0 || index >= params.getParamCount()) return 0.0f;
    return (float)plainToNormalised(params.getParameter(index), params.getParam(index));
}

static void getParameterName(AEffect* effect, int32_t index, char* label) {
    wrap_plugin_t* plug = (wrap_plugin_t*)effect->object;
    Params& params = plug->r->params();
    if (index < 0 || index >= params.getParamCount()) { label[0] = '\0'; return; }
    std::strncpy(label, params.getParameter(index).name.c_str(), VestigeMaxLabelLen - 1);
    label[VestigeMaxLabelLen - 1] = '\0';
}

/****************************************************************
 ** audio processing
 */

static void processReplacing(AEffect* effect, float** inputs, float** outputs, int32_t sampleFrames) {
    wrap_plugin_t* plug = (wrap_plugin_t*)effect->object;
    const PluginDescriptor& d = getPluginDescriptor();

    for (int ch = 0; ch < d.numOutputChannels && ch < d.numInputChannels; ++ch)
        if (outputs[ch] != inputs[ch])
            std::memcpy(outputs[ch], inputs[ch], sampleFrames * sizeof(float));

    plug->r->process((uint32_t)sampleFrames, inputs, (uint32_t)d.numInputChannels,
                                              outputs, (uint32_t)d.numOutputChannels);

    // Report parameter changes the GUI made (host automation write-back).
    Params& params = plug->r->params();
    if (plug->host != nullptr && params.controllerChanged.load(std::memory_order_acquire)) {
        for (int i = 0; i < params.getParamCount(); ++i) {
            if (!params.isParamDirty(i)) continue;
            const float normalised = (float)plainToNormalised(params.getParameter(i), params.getParam(i));
            plug->host(effect, audioMasterAutomate, i, 0, nullptr, normalised);
            params.setParamDirty(i, false);
        }
        params.controllerChanged.store(false, std::memory_order_release);
    }
}

/****************************************************************
 ** state
 */

static void wrap_save_state(wrap_plugin_t* plug, void** data, int32_t* size) {
    plug->r->saveState(&plug->state);
    *size = (int32_t)plug->state.size();
    *data = (void*)plug->state.c_str(); // valid until the next saveState()/close
}

static void wrap_load_state(wrap_plugin_t* plug) {
    if (plug->state.empty()) return;
    plug->r->readState(plug->state);
}

/****************************************************************
 ** dispatcher
 */

static intptr_t dispatcher(AEffect* effect, int32_t opCode, int32_t index, intptr_t value, void* ptr, float opt) {
    wrap_plugin_t* plug = (wrap_plugin_t*)effect->object;
    const PluginDescriptor& d = getPluginDescriptor();
    const PluginVariantInfo& v = d.variants[PLUGIN_VARIANT_INDEX];

    switch (opCode) {
        case effEditGetRect: {
            int w = 0, h = 0;
            plug->r->getGuiSize(w, h);
            plug->editorRect = {0, 0, (short)h, (short)w};
            if (ptr) *(ERect**)ptr = &plug->editorRect;
            return 1;
        }
        case effGetEffectName:
            std::strncpy((char*)ptr, v.name, VestigeMaxNameLen - 1);
            ((char*)ptr)[VestigeMaxNameLen - 1] = '\0';
            return 1;
        case effGetVendorString:
            std::strncpy((char*)ptr, d.vendor, VestigeMaxNameLen - 1);
            ((char*)ptr)[VestigeMaxNameLen - 1] = '\0';
            return 1;
        case effGetProductString:
            std::strncpy((char*)ptr, d.vendor, VestigeMaxNameLen - 1);
            ((char*)ptr)[VestigeMaxNameLen - 1] = '\0';
            return 1;
        case effGetPlugCategory:
            return kPlugCategEffect;
        case effOpen:
            break;
        case effClose:
            if (plug->guiIsCreated) {
                plug->r->quitGui();
            }
            delete plug->r;
            free(plug);
            break;
        case effGetParamName:
            getParameterName(effect, index, (char*)ptr);
            break;
        case effSetSampleRate:
            plug->sampleRate = opt;
            plug->r->initEngine((uint32_t)plug->sampleRate, 25, 1);
            plug->r->selectVariant(PLUGIN_VARIANT_INDEX);
            plug->isInited = true;
            wrap_load_state(plug);
            break;
        case effEditOpen:
            plug->r->startGui(ptr);
            //plug->r->setParent(ptr);
            plug->r->showGui();
            plug->guiIsCreated = true;
            break;
        case effEditClose:
            if (plug->guiIsCreated) {
                plug->r->quitGui();
            }
            plug->guiIsCreated = false;
            break;
        case effEditIdle:
            break;
        case 23: { // effGetChunk (index: 0 = program, 1 = bank)
            void* chunkData = nullptr;
            int32_t chunkSize = 0;
            wrap_save_state(plug, &chunkData, &chunkSize);
            *(void**)ptr = chunkData;
            return chunkSize;
        }
        case 24: { // effSetChunk
            plug->state = (const char*)ptr;
            // read the state, but only once we know the sample rate
            if (plug->isInited) wrap_load_state(plug);
            break;
        }
        default: break;
    }
    return 0;
}

/****************************************************************
 ** entry point
 */

extern "C" __attribute__ ((visibility ("default")))
AEffect* VSTPluginMain(audioMasterCallback audioMaster) {
    const PluginDescriptor& d = getPluginDescriptor();
    const PluginVariantInfo& v = d.variants[PLUGIN_VARIANT_INDEX];

    wrap_plugin_t* plug = (wrap_plugin_t*)calloc(1, sizeof(wrap_plugin_t));
    AEffect* effect = (AEffect*)calloc(1, sizeof(AEffect));
    if (plug == nullptr || effect == nullptr) { free(plug); free(effect); return nullptr; }

    plug->r = createPluginInstance(PLUGIN_VARIANT_INDEX);
    plug->host = audioMaster;
    plug->effect = effect;
    plug->sampleRate = 48000.0f;
    plug->isInited = false;
    plug->guiIsCreated = false;
    effect->object = plug;

    int w = 0, h = 0;
    plug->r->getGuiSize(w, h);
    plug->editorRect = {0, 0, (short)h, (short)w};

    effect->magic              = kEffectMagic;
    effect->dispatcher         = dispatcher;
    effect->process            = nullptr;
    effect->processReplacing   = processReplacing;
    effect->setParameter       = setParameter;
    effect->getParameter       = getParameter;
    effect->numPrograms        = 1;
    effect->numParams          = plug->r->params().getParamCount();
    effect->numInputs          = d.numInputChannels;
    effect->numOutputs         = d.numOutputChannels;
    effect->flags              = effFlagsHasEditor | effFlagsCanReplacing | FlagsChunks;
    effect->uniqueID           = v.vst2UniqueId;
    effect->version            = 1000;

    return effect;
}
