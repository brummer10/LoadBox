/*
 * CLAPWrapper.cpp
 *
 * Generic CLAP wrapper
 * Does not need to be changed for a new plugin,
 * all that's required is implementing PluginAPI.h.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */

#include <clap.h>
#include <ext/params.h>
#include <events.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "PluginAPI.h"

typedef struct wrap_plugin_t wrap_plugin_t;

#if defined(_WIN32)
#define GUIAPI CLAP_WINDOW_API_WIN32
#else
#define GUIAPI CLAP_WINDOW_API_X11
#endif

// per-plugin-instance data
struct wrap_plugin_t {
    clap_plugin_t plugin;
    const clap_host_t *host;
    IPluginClient *r;
    int variantIndex;
    std::string state;
    bool guiIsCreated;
    bool isActive;
};

static bool isHiddenInVariant(const PluginVariantInfo& v, int idx) {
    for (int hid : v.hiddenParams)
        if (hid == idx) return true;
    return false;
}

/****************************************************************
 ** Parameter handling
 */

static uint32_t params_count(const clap_plugin_t* plugin) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    return (uint32_t)plug->r->params().getParamCount();
}

static bool params_get_info(const clap_plugin_t* plugin, uint32_t param_index, clap_param_info_t* param_info) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    Params& params = plug->r->params();
    if ((int)param_index >= params.getParamCount()) return false;
    const auto& def = params.getParameter(param_index);
    memset(param_info, 0, sizeof(*param_info));
    param_info->id = def.id;
    strncpy(param_info->name, def.name.c_str(), CLAP_NAME_SIZE-1);
    strncpy(param_info->module, def.group.c_str(), CLAP_PATH_SIZE-1);
    param_info->default_value = def.def;
    param_info->min_value = def.min;
    param_info->max_value = def.max;
    uint32_t flags = CLAP_PARAM_IS_AUTOMATABLE;
    if (def.isStepped) flags |= CLAP_PARAM_IS_STEPPED;
    const PluginVariantInfo& variant = getPluginDescriptor().variants[plug->variantIndex];
    if (isHiddenInVariant(variant, (int)param_index)) flags |= CLAP_PARAM_IS_HIDDEN;
    param_info->flags = flags;
    param_info->cookie = nullptr;
    return true;
}

static bool params_get_value(const clap_plugin_t* plugin, clap_id param_id, double* value) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    Params& params = plug->r->params();
    if ((int)param_id < 0 || (int)param_id >= params.getParamCount()) return false;
    *value = params.getParam(param_id);
    return true;
}

static bool params_value_to_text(const clap_plugin_t* plugin, clap_id param_id, double value, char* out, uint32_t size) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    if ((int)param_id < 0 || (int)param_id >= plug->r->params().getParamCount()) return false;
    plug->r->valueToText((int)param_id, value, out, size);
    return true;
}

static bool params_text_to_value(const clap_plugin_t* plugin, clap_id param_id, const char* text, double* out_value) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    if ((int)param_id < 0 || (int)param_id >= plug->r->params().getParamCount()) return false;
    *out_value = plug->r->textToValue((int)param_id, text);
    return true;
}

static void sync_params_to_plug(const clap_plugin_t *plugin, const clap_event_header_t *hdr) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    if (hdr->space_id == CLAP_CORE_EVENT_SPACE_ID) {
        switch (hdr->type) {
            case CLAP_EVENT_PARAM_VALUE: {
                const clap_event_param_value_t *ev = (const clap_event_param_value_t *)hdr;
                plug->r->params().setParam(ev->param_id, ev->value);
                plug->r->onParameterChanged((int)ev->param_id, ev->value);
                break;
            }
        }
    }
}

static void sync_params_to_host(const clap_plugin_t *plugin, const clap_output_events_t *out) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    Params& params = plug->r->params();
    for (int i = 0; i < params.getParamCount(); i++) {
        if (params.isParamDirty(i)) {
            clap_event_param_value_t event = {};
            event.header.size = sizeof(event);
            event.header.time = 0;
            event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            event.header.type = CLAP_EVENT_PARAM_VALUE;
            event.header.flags = 0;
            event.param_id = i;
            event.cookie = NULL;
            event.note_id = -1;
            event.port_index = -1;
            event.channel = -1;
            event.key = -1;
            event.value = params.getParam(i);
            out->try_push(out, &event.header);
            params.setParamDirty(i, false);
        }
    }
}

static void params_flush(const clap_plugin_t *plugin,
                        const clap_input_events_t *in,
                        const clap_output_events_t *out) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    Params& params = plug->r->params();
    for (uint32_t i = 0; i < in->size(in); ++i) {
        const clap_event_header_t *ev = in->get(in, i);
        if (ev->type == CLAP_EVENT_PARAM_VALUE) {
            auto *p = (const clap_event_param_value_t *)ev;
            if (p->param_id >= 0 && (int)p->param_id < params.getParamCount()) {
                params.setParam(p->param_id, p->value);
            }
        }
    }
}

const clap_plugin_params_t wrap_params = {
    .count         = params_count,
    .get_info      = params_get_info,
    .get_value     = params_get_value,
    .value_to_text = params_value_to_text,
    .text_to_value = params_text_to_value,
    .flush         = params_flush,
};

/****************************************************************
 ** define the audio ports
 ** (single main bus per direction; channel counts come from
 **  PluginDescriptor::numInputChannels/numOutputChannels)
 */

static uint32_t audio_ports_count(const clap_plugin_t*, bool /*is_input*/) {
    return 1; // exactly one main bus per direction
}

static const char* portTypeForChannels(int channels) {
    if (channels == 1) return CLAP_PORT_MONO;
    if (channels == 2) return CLAP_PORT_STEREO;
    return nullptr; // no standard CLAP port type beyond mono/stereo
}

static bool audio_ports_get(const clap_plugin_t*, uint32_t index, bool is_input, clap_audio_port_info_t *info) {
    if (index > 0) return false;
    const PluginDescriptor& d = getPluginDescriptor();
    const int channels = is_input ? d.numInputChannels : d.numOutputChannels;
    info->id = index;
    snprintf(info->name, sizeof(info->name), "%s", is_input ? "Input" : "Output");
    info->channel_count = channels;
    info->port_type = portTypeForChannels(channels);
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    return true;
}

static const clap_plugin_audio_ports_t audio_ports = {
    .count = audio_ports_count,
    .get = audio_ports_get,
};

/****************************************************************
 ** Latency reporting
 */

static uint32_t wrap_latency_get(const clap_plugin_t *plugin) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    return plug->r->getLatencySamples();
}

static const clap_plugin_latency_t latency_extension = {
    .get = wrap_latency_get,
};

/****************************************************************
 ** save and load states
 */

static bool wrap_state_save(const clap_plugin_t *plugin, const clap_ostream_t *stream) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    plug->r->saveState(&plug->state);
    stream->write(stream, plug->state.c_str(), strlen(plug->state.c_str()));
    return true;
}

static bool wrap_state_load(const clap_plugin_t *plugin, const clap_istream_t *stream) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    char _state[2048] = {0};
    char *curr = _state;
    int thisread = stream->read(stream, curr, 2047);
    if (thisread < 0) return false;
    _state[thisread] = '\0';
    plug->state = _state;
    if (plug->isActive) plug->r->readState(plug->state);
    // actually applied in wrap_activate(), once the engine has been
    // initialized (the sample rate must already be known at that
    // point).
    return true;
}

static const clap_plugin_state_t state_extension = {
    .save = wrap_state_save,
    .load = wrap_state_load,
};

/****************************************************************
 ** GUI handling
 */

static void wrap_gui_destroy(const clap_plugin *plugin);

static bool wrap_gui_is_api_supported(const clap_plugin *plugin, const char *api, bool is_floating) {
    return strcmp(api, GUIAPI) == 0;
}

static bool wrap_gui_get_preferred_api(const clap_plugin_t *plugin, const char **api, bool *isFloating) {
    *api = GUIAPI;
    *isFloating = false;
    return true;
}

static bool wrap_gui_set_scale(const clap_plugin_t *plugin, double scale) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    return plug->r->setGuiScale(scale);
}

static bool wrap_gui_get_size(const clap_plugin_t *plugin, uint32_t *width, uint32_t *height) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    int w = 0, h = 0;
    plug->r->getGuiSize(w, h);
    *width = (uint32_t)w;
    *height = (uint32_t)h;
    return true;
}

static bool wrap_gui_can_resize(const clap_plugin_t *plugin) {
    return true;
}

static bool wrap_gui_get_resize_hints(const clap_plugin_t *plugin, clap_gui_resize_hints_t *hints) {
    return false;
}

static bool wrap_gui_adjust_size(const clap_plugin_t *plugin, uint32_t *width, uint32_t *height) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    return plug->r->resizeGui(*width, *height);
}

static bool wrap_gui_set_transient(const clap_plugin_t *plugin, const clap_window_t *window) {
    return false;
}

static void wrap_gui_suggest_title(const clap_plugin_t *plugin, const char *title) {
}

static bool wrap_gui_create(const clap_plugin *plugin, const char *api, bool is_floating) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    if (strcmp(api, GUIAPI) == 0) {
        if (!plug->guiIsCreated) {
            plug->r->startGui();
        }
        plug->guiIsCreated = true;
        return true;
    }
    return false;
}

static void wrap_gui_destroy(const clap_plugin *plugin) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    if (plug->guiIsCreated) {
        plug->r->quitGui();
    }
    plug->guiIsCreated = false;
}

static bool wrap_gui_show(const clap_plugin *plugin) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    plug->r->showGui();
    return true;
}

static bool wrap_gui_hide(const clap_plugin *plugin) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    plug->r->hideGui();
    return true;
}

// embed the GUI
static bool wrap_gui_set_parent(const clap_plugin_t *plugin, const clap_window_t *window) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
#if defined(_WIN32)
    WindowHandle native = (WindowHandle)window->win32;
#else
    WindowHandle native = (WindowHandle)window->x11;
#endif
    if (!plug->guiIsCreated) {
        plug->r->startGui(native);
    }
    plug->guiIsCreated = true;
    plug->r->setParent(native);
    return true;
}

static bool wrap_gui_set_size(const clap_plugin_t *plugin, uint32_t width, uint32_t height) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    return plug->r->resizeGui((int)width, (int)height);
}

// Main thread callback (plugins with their own main loop usually don't need this)
static void wrap_on_main_thread(const clap_plugin_t *plugin) {
}

static const clap_plugin_gui_t extensionGUI = {
    .is_api_supported = wrap_gui_is_api_supported,
    .get_preferred_api = wrap_gui_get_preferred_api,
    .create = wrap_gui_create,
    .destroy = wrap_gui_destroy,
    .set_scale = wrap_gui_set_scale,
    .get_size = wrap_gui_get_size,
    .can_resize = wrap_gui_can_resize,
    .get_resize_hints = wrap_gui_get_resize_hints,
    .adjust_size = wrap_gui_adjust_size,
    .set_size = wrap_gui_set_size,
    .set_parent = wrap_gui_set_parent,
    .set_transient = wrap_gui_set_transient,
    .suggest_title = wrap_gui_suggest_title,
    .show = wrap_gui_show,
    .hide = wrap_gui_hide,
};

/****************************************************************
 ** Plugin handling
 */

static bool wrap_init(const clap_plugin_t *plugin) {
    return true;
}

static void wrap_destroy(const clap_plugin_t *plugin) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    wrap_gui_destroy(plugin);
    delete plug->r;
    delete plug;
}

static clap_process_status wrap_process(const clap_plugin_t *plugin, const clap_process_t *process) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    const PluginDescriptor& d = getPluginDescriptor();
    const uint32_t inCh = process->audio_inputs[0].channel_count;
    const uint32_t outCh = process->audio_outputs[0].channel_count;
    if ((int)inCh != d.numInputChannels || (int)outCh != d.numOutputChannels) {
        return CLAP_PROCESS_ERROR;
    }

    float* const* inputs = process->audio_inputs[0].data32;
    float* const* outputs = process->audio_outputs[0].data32;
    uint32_t nframes = process->frames_count;
    const uint32_t nev = process->in_events->size(process->in_events);
    uint32_t ev_index = 0;
    uint32_t next_ev_frame = nev > 0 ? 0 : nframes;

    Params& params = plug->r->params();
    if (params.controllerChanged.load(std::memory_order_acquire)) {
        sync_params_to_host(plugin, process->out_events);
        params.controllerChanged.store(false, std::memory_order_release);
    }

    for (uint32_t i = 0; i < nframes; ++i) {
        while (ev_index < nev && next_ev_frame == i) {
            const clap_event_header_t *hdr = process->in_events->get(process->in_events, ev_index);
            if (hdr->time != i) {
                next_ev_frame = hdr->time;
                break;
            }
            sync_params_to_plug(plugin, hdr);
            ++ev_index;

            if (ev_index == nev) {
                next_ev_frame = nframes;
                break;
            }
        }
    }

    for (uint32_t ch = 0; ch < outCh && ch < inCh; ++ch)
        if (outputs[ch] != inputs[ch])
            memcpy(outputs[ch], inputs[ch], nframes * sizeof(float));

    plug->r->process(nframes, inputs, inCh, outputs, outCh);
    return CLAP_PROCESS_CONTINUE;
}

static bool wrap_activate(const struct clap_plugin *plugin,
                             double                    sample_rate,
                             uint32_t                  min_frames_count,
                             uint32_t                  max_frames_count) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    plug->r->initEngine((uint32_t)sample_rate, 25, 1);
    plug->isActive = true;
    plug->r->selectVariant(plug->variantIndex);
    if (!plug->state.empty()) plug->r->readState(plug->state);
    return true;
}

static void wrap_deactivate(const struct clap_plugin *plugin) {
    wrap_plugin_t *plug = (wrap_plugin_t *)plugin->plugin_data;
    if (!plug->state.empty()) plug->state.clear();
}

static bool wrap_start_processing(const struct clap_plugin *plugin) { return true; }
static void wrap_stop_processing(const struct clap_plugin *plugin) {}
static void wrap_reset(const struct clap_plugin *plugin) {}

static const void *wrap_get_extension(const clap_plugin_t *plugin, const char *id) {
    if (!strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &audio_ports;
    if (!strcmp(id, CLAP_EXT_LATENCY)) return &latency_extension;
    if (!strcmp(id, CLAP_EXT_GUI)) return &extensionGUI;
    if (!strcmp(id, CLAP_EXT_PARAMS)) return &wrap_params;
    if (!strcmp(id, CLAP_EXT_STATE)) return &state_extension;
    return NULL;
}

// statically held CLAP descriptors, generated from PluginDescriptor
static std::vector<clap_plugin_descriptor_t> gClapDescriptors;

static void ensureClapDescriptorsBuilt() {
    if (!gClapDescriptors.empty()) return;
    const PluginDescriptor& d = getPluginDescriptor();
    gClapDescriptors.resize(d.variants.size());
    for (size_t i = 0; i < d.variants.size(); ++i) {
        const PluginVariantInfo& v = d.variants[i];
        clap_plugin_descriptor_t desc = {};
        desc.clap_version = CLAP_VERSION_INIT;
        desc.id = v.id;
        desc.name = v.name;
        desc.vendor = d.vendor;
        desc.url = d.url;
        desc.manual_url = d.url;
        desc.support_url = d.url;
        desc.version = d.version;
        desc.description = v.description;
        desc.features = d.clapFeature;
        gClapDescriptors[i] = desc;
    }
}

static const clap_plugin_t *wrap_create(const clap_host_t *host, int variantIndex) {
    ensureClapDescriptorsBuilt();
    wrap_plugin_t *plug = new wrap_plugin_t{};
    if (!plug) return NULL;
    plug->r = createPluginInstance(variantIndex);
    plug->variantIndex = variantIndex;
    plug->guiIsCreated = false;
    plug->isActive = false;
    plug->plugin.desc = &gClapDescriptors[variantIndex];
    plug->plugin.plugin_data = plug;
    plug->plugin.init = wrap_init;
    plug->plugin.destroy = wrap_destroy;
    plug->plugin.activate = wrap_activate;
    plug->plugin.deactivate = wrap_deactivate;
    plug->plugin.start_processing = wrap_start_processing;
    plug->plugin.stop_processing = wrap_stop_processing;
    plug->plugin.reset = wrap_reset;
    plug->plugin.process = wrap_process;
    plug->plugin.get_extension = wrap_get_extension;
    plug->plugin.on_main_thread = wrap_on_main_thread;
    plug->host = host;
    return &plug->plugin;
}

/****************************************************************
 ** the factory entry
 */

static uint32_t plugin_factory_get_plugin_count(const struct clap_plugin_factory *factory) {
    return (uint32_t)getPluginDescriptor().variants.size();
}

static const clap_plugin_descriptor_t *plugin_factory_get_descriptor
                    (const struct clap_plugin_factory *factory, uint32_t index) {
    ensureClapDescriptorsBuilt();
    if (index >= gClapDescriptors.size()) return nullptr;
    return &gClapDescriptors[index];
}

static const clap_plugin_t *plugin_factory_create
                        (const struct clap_plugin_factory *factory,
                        const clap_host_t *host, const char *plugin_id) {

    if (!clap_version_is_compatible(host->clap_version)) {
        return NULL;
    }

    const PluginDescriptor& d = getPluginDescriptor();
    for (size_t i = 0; i < d.variants.size(); ++i) {
        if (!strcmp(plugin_id, d.variants[i].id))
            return wrap_create(host, (int)i);
    }
    return NULL;
}

static const clap_plugin_factory_t plugin_factory = {
    .get_plugin_count = plugin_factory_get_plugin_count,
    .get_plugin_descriptor = plugin_factory_get_descriptor,
    .create_plugin = plugin_factory_create,
};

static const void *entry_get_factory(const char *factory_id) {
    return &plugin_factory;
}

static bool entry_init(const char *plugin_path) {
    return true;
}

static void entry_deinit(void) {
}

/****************************************************************
 ** Finally the CLAP plugin entry export
 */

extern "C" const clap_plugin_entry_t clap_entry = {
    .clap_version = CLAP_VERSION_INIT,
    .init = entry_init,
    .deinit = entry_deinit,
    .get_factory = entry_get_factory,
};
