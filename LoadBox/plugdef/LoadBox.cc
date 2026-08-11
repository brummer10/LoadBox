/*
 * LoadBox.cc
 *
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 * 
 * Copyright (C) 2025 brummer <brummer@web.de>
 */

#include <atomic>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <cmath>

#include <locale.h>

#include "engine.h"
#include "ParallelThread.h"
#include "Parameter.h"
#define CLAPPLUG
#include "LoadBox.c"

class LoadBox
{
public:
    Widget_t*               TopWin;
    Params                  param;

    LoadBox() : engine(), param() {
        workToDo.store(false, std::memory_order_release);
        ui = (X11_UI*)malloc(sizeof(X11_UI));
        ui->private_ptr = NULL;
        ui->need_resize = 1;
        ui->loop_counter = 4;
        ui->uiKnowSampleRate = false;
        ui->setVerbose = false;
        ui->uiSampleRate = 0;
        ui->f_index = 0;
        title = "IR Loader";
        firstLoop = true;
        p = 0;
        registerParameters();
        for(int i = 0;i<CONTROLS;i++)
            ui->widget[i] = NULL;
    }

    ~LoadBox() {
        fetch.stop();
        free(ui->private_ptr);
        free(ui);
    }

    void registerParameters() {
        //                  name             group   min, max, def, step   value              isStepped  type
        param.registerParam("Enable",         "Global", 0,1,1,1,     (void*)&engine.bypass,        true,  IS_UINT);

        param.registerParam("IR Out Gain L",  "IR",    -20,20,0,0.1, (void*)&engine.IRoutputGain,   false, Is_FLOAT);
        param.registerParam("IR Out Gain R",  "IR",    -20,20,0,0.1, (void*)&engine.IRoutputGain1,  false, Is_FLOAT);

        param.registerParam("IR Mode",        "IR",     0,1,0,1,     (void*)&engine.IRmode,         true,  IS_UINT);
        param.registerParam("IR Mix",         "IR",     0,1,0.5,0.01,(void*)&engine.IRmix,          false, Is_FLOAT);
        param.registerParam("Master",         "IR",    -20,20,0,0.1, (void*)&engine.MasterOutGain,  false, Is_FLOAT);
    }

    void startGui(Window window) {
        main_init(&ui->main);
        if (ui->main.hdpi > 1.6) ui->main.hdpi = 1.6;
        set_custom_theme(ui);
        int w = 1;
        int h = 1;
        plugin_set_window_size(&w,&h,"clap_plugin");
        #if defined(_WIN32)
        TopWin  = create_window(&ui->main, (HWND) window, 0, 0, w, h);
        #else
        TopWin  = create_window(&ui->main, (Window) window, 0, 0, w, h);
        #endif
        TopWin->flags |= HIDE_ON_DELETE;
        ui->win = create_widget(&ui->main, TopWin, 0, 0, w, h);
        ui->win->scale.gravity = NORTHWEST;
        widget_set_title(TopWin, title.c_str());
        ui->win->parent_struct = ui;
        ui->win->private_struct = (void*)this;
        plugin_create_controller_widgets(ui,"clap_plugin");
        fetch.startTimeout(60);
        fetch.set<LoadBox, &LoadBox::runGui>(this);
    }

    void startGui() {
        main_init(&ui->main);
        if (ui->main.hdpi > 1.6) ui->main.hdpi = 1.6;
        set_custom_theme(ui);
        int w = 1;
        int h = 1;
        plugin_set_window_size(&w,&h,"clap_plugin");
        TopWin  = create_window(&ui->main, os_get_root_window(&ui->main, IS_WINDOW), 0, 0, w, h);
        TopWin->flags |= HIDE_ON_DELETE;
        ui->win = create_widget(&ui->main, TopWin, 0, 0, w, h);
        widget_set_title(TopWin, title.c_str());
        ui->win->parent_struct = ui;
        ui->win->private_struct = (void*)this;
        plugin_create_controller_widgets(ui,"clap_plugin");
        fetch.startTimeout(60);
        fetch.set<LoadBox, &LoadBox::runGui>(this);
    }

    void showGui() {
        engine._notify_ui.store(true, std::memory_order_release);
        getEngineValues();
        widget_show_all(TopWin);
        firstLoop = true;
    }

    void setParent(Window window) {
        #if defined(_WIN32)
        SetParent(TopWin->widget, (HWND) window);
        #else
        XReparentWindow(ui->main.dpy, TopWin->widget, (Window) window, 0, 0);
        #endif
        p = window;
    }

    void checkParentWindowSize(int width, int height) {
        #if defined (IS_VST2)
        if (!p) return;
        int host_width = 1;
        int host_height = 1;
        #if defined(_WIN32)
        RECT rect;
        if (GetClientRect((HWND) p, &rect)) {
            host_width  = rect.right - rect.left;
            host_height = rect.bottom - rect.top;
        }
        #else
        XWindowAttributes attrs;
        if (XGetWindowAttributes(ui->main.dpy, p, &attrs)) {
            host_width  = attrs.width;
            host_height = attrs.height;
        }
        #endif
        if ((host_width != width && host_width != 1) ||
            (host_height != height && host_height != 1)) {
            os_resize_window(ui->main.dpy, TopWin, host_width, host_height);
        }
        #endif
    }

    void hideGui() {
        widget_hide(TopWin);
        firstLoop = false;
    }

    void quitGui() {
        fetch.stop();
        cleanup();
        main_quit(&ui->main);
    }

    void runGui() {
        checkEngine();
        if (firstLoop) {
            checkParentWindowSize(TopWin->width, TopWin->height);
            firstLoop = false;
        }
        if (param.paramChanged.load(std::memory_order_acquire)) {
            getEngineValues();
            param.paramChanged.store(false, std::memory_order_release);
        }
        run_embedded(&ui->main);
    }

    // check output ports from engine
    void checkEngine() {
        if (workToDo.load(std::memory_order_acquire)) {
            if (engine.xrworker.getProcess()) {
                workToDo.store(false, std::memory_order_release);
                engine.xrworker.runProcess();
            }
        } else if (engine._notify_ui.load(std::memory_order_acquire)) {
            engine._notify_ui.store(false, std::memory_order_release);
            X11_UI_Private_t *ps = (X11_UI_Private_t*)ui->private_ptr;
            get_file(engine.ir_file, &ps->ir);
            get_file(engine.ir_file1, &ps->ir1);
            expose_widget(ui->win);
            engine._cd.store(0, std::memory_order_release);
        }
    }

    Xputty *getMain() {
        return &ui->main;
    }

    irloader::Engine *getEngine() {
        return &engine;
    }

    void initEngine(uint32_t rate, int32_t prio, int32_t policy) {
        engine.init(rate, prio, policy);
        engine.bypass = 1;
        param.setParamDirty(0 , true);
        param.controllerChanged.store(true, std::memory_order_release);
    }

    void enableEngine(int on) {
        adj_set_value(ui->widget[9]->adj, static_cast<float>(on));
    }

    inline void process(uint32_t n_samples, float* output, float* output1) {
        engine.process(n_samples, output, output1);
    }

    void getLatency(uint32_t* latency) {
        (*latency) = static_cast<uint32_t>(engine.latency);
    }

    void getEngineValues() {
        adj_set_value(ui->widget[0]->adj, engine.IRoutputGain);
        adj_set_value(ui->widget[1]->adj, engine.IRoutputGain1);
        adj_set_value(ui->widget[2]->adj, static_cast<float>(engine.conv.get_normalisation()));
        adj_set_value(ui->widget[3]->adj, static_cast<float>(engine.conv1.get_normalisation()));
        adj_set_value(ui->widget[6]->adj, engine.IRmix);
        adj_set_value(ui->widget[7]->adj, engine.MasterOutGain);
        adj_set_value(ui->widget[8]->adj, static_cast<float>(engine.IRmode));
        adj_set_value(ui->widget[9]->adj, static_cast<float>(engine.bypass));
    }

    // send value changes from GUI to the engine
    void sendValueChanged(int port, float value) {
        switch (port) {
            // 0 + 1 audio ports (L/R in)
            case 7:
                engine.IRoutputGain = value;
                param.setParamDirty(1 , true);
            break;
            case 8:
                engine.IRoutputGain1 = value;
                param.setParamDirty(2 , true);
            break;
            case 9:
            {
                engine._cd.fetch_add(1, std::memory_order_relaxed);
                engine.conv.set_normalisation(static_cast<uint32_t>(value));
                if (engine.ir_file.compare("None") != 0) {
                    workToDo.store(true, std::memory_order_release);
                }
            }
            break;
            case 10:
            {
                engine._cd.fetch_add(2, std::memory_order_relaxed);
                engine.conv1.set_normalisation(static_cast<uint32_t>(value));
                if (engine.ir_file1.compare("None") != 0) {
                    workToDo.store(true, std::memory_order_release);
                }
            }
            break;
            case 14:
                engine.bypass = static_cast<uint32_t>(value);
                param.setParamDirty(0 , true);
            break;
            case 17:
            {
                engine._cd.fetch_add(1, std::memory_order_relaxed);
                engine.ir_file = "None";
                workToDo.store(true, std::memory_order_release);
            }
            break;
            case 18:
            {
                engine._cd.fetch_add(2, std::memory_order_relaxed);
                engine.ir_file1 = "None";
                workToDo.store(true, std::memory_order_release);
            }
            break;
            // 22 latency label, 23 xrun label: read-only, no case needed
            case 33:
                engine.IRmode = static_cast<uint32_t>(value);
                param.setParamDirty(3 , true);
            break;
            case 34:
                engine.IRmix = value;
                param.setParamDirty(4 , true);
            break;
            case 35:
                engine.MasterOutGain = value;
                param.setParamDirty(5 , true);
            break;
            default:
            break;
        }
        // inform the process thread that a controller value was changed by the GUI thread
        param.controllerChanged.store(true, std::memory_order_release);
    }

    // send a file name from GUI to the engine
    void sendFileName(ModelPicker* m) {
        if ((strcmp(m->filename, "None") == 0) || ends_with(m->filename, "nam") ||
                ends_with(m->filename, "wav") || ends_with(m->filename, "WAV")) {

            int model = m->model;
            switch(model) {
                case 3:
                    engine.ir_file = m->filename;
                    engine._cd.fetch_add(1, std::memory_order_relaxed);
                break;
                case 4:
                    engine.ir_file1 = m->filename;
                    engine._cd.fetch_add(2, std::memory_order_relaxed);
                break;
                default :
                break;
            }
            workToDo.store(true, std::memory_order_release);
        } else return;
    }

    float check_stod (const std::string& str) {
        char* point = localeconv()->decimal_point;
        if (std::string(".") != point) {
            std::string::size_type point_it = str.find(".");
            std::string temp_str = str;
            if (point_it != std::string::npos)
                temp_str.replace(point_it, point_it + 1, point);
            return std::stod(temp_str);
        } else return std::stod(str);
    }

    std::string remove_sub(std::string a, std::string b) {
        std::string::size_type fpos = a.find(b);
        if (fpos != std::string::npos )
            a.erase(a.begin() + fpos, a.begin() + fpos + b.length());
        return (a);
    }

    void readState(std::string _stream) {
        std::string stream = _stream;
        std::string line;
        std::string key;
        std::string value;
        std::size_t pos = _stream.find("|");
        while (pos != std::string::npos) {
            line = stream.substr(0, pos);
            std::istringstream buf(line);
            buf >> key;
            buf >> value;
            if (key.compare("[CONTROLS]") == 0) {
                engine.IRoutputGain = check_stod(value);
                buf >> value;
                engine.IRoutputGain1 = check_stod(value);
                buf >> value;
                engine.conv.set_normalisation((int)check_stod(value));
                buf >> value;
                engine.conv1.set_normalisation((int)check_stod(value));
                buf >> value;
                engine.bypass = static_cast<uint32_t>(check_stod(value));
                buf >> value;
                engine.IRmode = static_cast<uint32_t>(check_stod(value));
                buf >> value;
                engine.IRmix = check_stod(value);
                buf >> value;
                engine.MasterOutGain = check_stod(value);
            } else if (key.compare("[IrFile]") == 0) {
                engine.ir_file = remove_sub(line, "[IrFile] ");
                engine._cd.fetch_add(1, std::memory_order_relaxed);
            } else if (key.compare("[IrFile1]") == 0) {
                engine.ir_file1 = remove_sub(line, "[IrFile1] ");
                engine._cd.fetch_add(2, std::memory_order_relaxed);
            }
            key.clear();
            value.clear();
            stream = stream.substr(pos+1);
            pos = stream.find("|");
            if (pos == std::string::npos) break;
        }
        workToDo.store(true, std::memory_order_release);
        if (engine.xrworker.getProcess()) {
            workToDo.store(false, std::memory_order_release);
            engine.xrworker.runProcess();
        }
    }

    void saveState(std::string *state) {
        std::ostringstream buffer;
        buffer << "[CONTROLS] ";
        buffer << engine.IRoutputGain << " ";
        buffer << engine.IRoutputGain1 << " ";
        buffer << engine.conv.get_normalisation() << " ";
        buffer << engine.conv1.get_normalisation() << " ";
        buffer << engine.bypass << " ";
        buffer << engine.IRmode << " ";
        buffer << engine.IRmix << " ";
        buffer << engine.MasterOutGain << " ";
        buffer << "|";
        buffer << "[IrFile] " << engine.ir_file << "|";
        buffer << "[IrFile1] " << engine.ir_file1 << "|";
        (*state) = buffer.str();
    }

    void cleanup() {
        plugin_cleanup(ui);
        free(ui->private_ptr);
        ui->private_ptr = NULL;
    }

private:
    ParallelThread          fetch;
    X11_UI*                 ui;
    irloader::Engine        engine;
    Window                  p;
    std::atomic<bool>       workToDo;
    std::string             title;
    bool                    firstLoop;

    // rebuild file menu when needed
    void rebuild_file_menu(ModelPicker *m) {
        xevfunc store = m->fbutton->func.value_changed_callback;
        m->fbutton->func.value_changed_callback = dummy_callback;
        combobox_delete_entrys(m->fbutton);
        fp_get_files(m->filepicker, m->dir_name, 0, 1);
        int active_entry = m->filepicker->file_counter-1;
        for(uint32_t i = 0;i<m->filepicker->file_counter;i++) {
            combobox_add_entry(m->fbutton, m->filepicker->file_names[i]);
            if (strcmp(basename(m->filename),m->filepicker->file_names[i]) == 0)
                active_entry = i;
        }
        combobox_add_entry(m->fbutton, "None");
        adj_set_value(m->fbutton->adj, active_entry);
        combobox_set_menu_size(m->fbutton, min(14, m->filepicker->file_counter+1));
        m->fbutton->func.value_changed_callback = store;
    }

    // confirmation from engine that a file is loaded
    inline void get_file(std::string fileName, ModelPicker *m) {
        if (!fileName.empty() && (fileName.compare("None") != 0)) {
            const char* uri = fileName.c_str();
            if (strcmp(uri, (const char*)m->filename) !=0) {
                free(m->filename);
                m->filename = NULL;
                m->filename = strdup(uri);
                char *dn = strdup(dirname((char*)uri));
                if (m->dir_name == NULL || strcmp((const char*)m->dir_name,
                                                        (const char*)dn) !=0) {
                    free(m->dir_name);
                    m->dir_name = NULL;
                    m->dir_name = strdup(dn);
                    FileButton *filebutton = (FileButton*)m->filebutton->private_struct;
                    filebutton->path = m->dir_name;
                    rebuild_file_menu(m);
                }
                free(dn);
            }
        } else if (strcmp(m->filename, "None") != 0) {
            free(m->filename);
            m->filename = NULL;
            m->filename = strdup("None");
        }
    }

};


/****************************************************************
 ** connect value change messages from the GUI (C) to the engine (C++)
 */

// send value changes from GUI to the engine
void sendValueChanged(X11_UI *ui, int port, float value) {
    LoadBox *r = (LoadBox*)ui->win->private_struct;
    r->sendValueChanged(port, value);
}

// send a file name from GUI to the engine
void sendFileName(X11_UI *ui, ModelPicker* m){
    LoadBox *r = (LoadBox*)ui->win->private_struct;
    r->sendFileName(m);
}
