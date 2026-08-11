/*
 * LoadBox.c
 *
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 * 
 * Copyright (C) 2025 brummer <brummer@web.de>
 */

#ifdef STANDALONE
#include "standalone.h"
#elif defined(CLAPPLUG)
#include "plug.h"
#else
#include "lv2_plugin.cc"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "widgets.cc"

void set_custom_theme(X11_UI *ui) {
    ui->main.color_scheme->normal = (Colors) {
         /* cairo    / r  / g  / b  / a  /  */
        .fg =       { 0.686, 0.729, 0.773, 1.000},
        .bg =       { 0.083, 0.083, 0.083, 1.000},
        .base =     { 0.093, 0.093, 0.093, 1.000},
        .text =     { 0.686, 0.729, 0.773, 1.000},
        .shadow =   { 0.000, 0.000, 0.000, 0.200},
        .frame =    { 0.000, 0.000, 0.000, 1.000},
        .light =    { 0.100, 0.100, 0.100, 1.000}
    };

    ui->main.color_scheme->prelight = (Colors) {
         /* cairo    / r  / g  / b  / a  /  */
        .fg =       { 0.600, 0.600, 0.600, 1.000},
        .bg =       { 0.250, 0.250, 0.250, 1.000},
        .base =     { 0.300, 0.300, 0.300, 1.000},
        .text =     { 1.000, 1.000, 1.000, 1.000},
        .shadow =   { 0.100, 0.100, 0.100, 0.400},
        .frame =    { 0.033, 0.033, 0.033, 1.000},
        .light =    { 0.300, 0.300, 0.300, 1.000}
    };

    ui->main.color_scheme->selected = (Colors) {
         /* cairo    / r  / g  / b  / a  /  */
        .fg =       { 0.900, 0.900, 0.900, 1.000},
        .bg =       { 0.083, 0.083, 0.083, 1.000},
        .base =     { 0.500, 0.180, 0.180, 1.000},
        .text =     { 1.000, 1.000, 1.000, 1.000},
        .shadow =   { 0.800, 0.180, 0.180, 0.200},
        .frame =    { 0.500, 0.180, 0.180, 1.000},
        .light =    { 0.500, 0.180, 0.180, 1.000}
    };

    ui->main.color_scheme->active = (Colors) {
         /* cairo    / r  / g  / b  / a  /  */
        .fg =       { 0.000, 1.000, 1.000, 1.000},
        .bg =       { 0.000, 0.000, 0.000, 1.000},
        .base =     { 0.180, 0.380, 0.380, 1.000},
        .text =     { 0.750, 0.750, 0.750, 1.000},
        .shadow =   { 0.180, 0.380, 0.380, 0.500},
        .frame =    { 0.180, 0.380, 0.380, 1.000},
        .light =    { 0.180, 0.380, 0.380, 1.000}
    };

    ui->main.color_scheme->insensitive = (Colors) {
         /* cairo    / r  / g  / b  / a  /  */
        .fg =       { 0.450, 0.450, 0.450, 0.500},
        .bg =       { 0.100, 0.100, 0.100, 0.500},
        .base =     { 0.000, 0.000, 0.000, 0.500},
        .text =     { 0.900, 0.900, 0.900, 0.500},
        .shadow =   { 0.000, 0.000, 0.000, 0.100},
        .frame =    { 0.000, 0.000, 0.000, 0.500},
        .light =    { 0.100, 0.100, 0.100, 0.500}
    };
}

static void file_load_response(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    ModelPicker* m = (ModelPicker*) w->parent_struct;
    Widget_t *p = (Widget_t*)w->parent;
    X11_UI *ui = (X11_UI*) p->parent_struct;
    if(user_data !=NULL) {
        free(m->filename);
        m->filename = NULL;
        m->filename = strdup(*(const char**)user_data);
        sendFileName(ui, m);
        free(m->filename);
        m->filename = NULL;
        m->filename = strdup("None");
        expose_widget(ui->win);
        ui->loop_counter = 12;
    }
}

void set_ctl_val_from_host(Widget_t *w, float value) {
    xevfunc store = w->func.value_changed_callback;
    w->func.value_changed_callback = dummy_callback;
    adj_set_value(w->adj, value);
    w->func.value_changed_callback = *(*store);
}

static void file_menu_callback(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    ModelPicker* m = (ModelPicker*) w->parent_struct;
    Widget_t *p = (Widget_t*)w->parent;
    X11_UI *ui = (X11_UI*) p->parent_struct;
    X11_UI_Private_t *ps = (X11_UI_Private_t*)ui->private_ptr;
    if (!m->filepicker->file_counter) return;
    int v = (int)adj_get_value(w->adj);
    if (v >= m->filepicker->file_counter) {
        free(ps->fname);
        ps->fname = NULL;
        asprintf(&ps->fname, "%s", "None");
    } else {
        free(ps->fname);
        ps->fname = NULL;
        asprintf(&ps->fname, "%s%s%s", m->dir_name, PATH_SEPARATOR, m->filepicker->file_names[v]);
    }
    file_load_response(m->filebutton, (void*)&ps->fname);
}

void plugin_set_window_size(int *w,int *h,const char * plugin_uri) {
    (*w) = 610; //set initial width of main window
    (*h) = 160; //set initial height of main window (top bar + one rack unit)
}

const char* plugin_set_name() {
    return "LoadBox"; //set plugin name to display on UI
}

void plugin_create_controller_widgets(X11_UI *ui, const char * plugin_uri) {

    ui->win->label = plugin_set_name();
    // connect the expose func
    ui->win->func.expose_callback = draw_window;

    X11_UI_Private_t *ps =(X11_UI_Private_t*)malloc(sizeof(X11_UI_Private_t));
    ui->private_ptr = (void*)ps;
    ps->ir.filename = strdup("None");
    ps->ir1.filename = strdup("None");
    ps->ir.dir_name = NULL;
    ps->ir1.dir_name = NULL;
    ps->ir.model = 3;
    ps->ir1.model = 4;
    ps->fname = NULL;
    ps->ir.filepicker = (FilePicker*)malloc(sizeof(FilePicker));
    fp_init(ps->ir.filepicker, "/");
    asprintf(&ps->ir.filepicker->filter ,"%s", ".nam|.wav|.WAV");
    ps->ir.filepicker->use_filter = 1;
    ps->ir1.filepicker = (FilePicker*)malloc(sizeof(FilePicker));
    fp_init(ps->ir1.filepicker, "/");
    asprintf(&ps->ir1.filepicker->filter ,"%s", ".nam|.wav|.WAV");
    ps->ir1.filepicker->use_filter = 1;

    ui->widget[9] = add_lv2_switch (ui->widget[9], ui->win, 14, "Enable", ui, 505,  12, 50, 50);

// IR
    ui->elem[0] = create_widget(&ui->main, ui->win, 5, 50, 600, 110);
    ui->elem[0]->parent_struct = ui;
    ui->elem[0]->label = "LoadBox";
    // rack mount background colour
    set_widget_color(ui->elem[0], (Color_state)0, (Color_mod)1, 0.176, 0.176, 0.176,1.0);
    // rack mount foreground colour
    set_widget_color(ui->elem[0], (Color_state)0, (Color_mod)0, 0.322, 0.322, 0.322,1.0);
    ui->elem[0]->func.expose_callback = draw_ir_elem;

    ui->widget[0] = add_lv2_knob (ui->widget[0], ui->elem[0], 7, "Gain (L)", ui, 55, 15, 70, 80);
    set_adjustment(ui->widget[0]->adj, 0.0, 0.0, -20.0, 20.0, 0.2, CL_CONTINUOS);
    // controller label colour
    set_widget_color(ui->widget[0], (Color_state)0, (Color_mod)0, 0.592, 0.612, 0.631,1.0);
    // controller background colour
    set_widget_color(ui->widget[0], (Color_state)0, (Color_mod)1, 0.083, 0.083, 0.083, 1.0);
    // controller label colour hover
    set_widget_color(ui->widget[0], (Color_state)1, (Color_mod)0, 0.694, 0.714, 0.737,1.0);

    ui->widget[6] = add_lv2_knob (ui->widget[6], ui->elem[0], 34, "Mix", ui, 55, 15, 70, 80);
    set_adjustment(ui->widget[6]->adj, 0.5, 0.5, 0.0, 1.0, 0.01, CL_CONTINUOS);
    // controller label colour
    set_widget_color(ui->widget[6], (Color_state)0, (Color_mod)0, 0.592, 0.612, 0.631,1.0);
    // controller background colour
    set_widget_color(ui->widget[6], (Color_state)0, (Color_mod)1, 0.083, 0.083, 0.083, 1.0);
    // controller label colour hover
    set_widget_color(ui->widget[6], (Color_state)1, (Color_mod)0, 0.694, 0.714, 0.737,1.0);

    ps->ir.fbutton = add_lv2_button(ps->ir.fbutton, ui->elem[0], "", ui, 445, 20, 22, 30);
    ps->ir.fbutton->parent_struct = (void*)&ps->ir;
    combobox_set_pop_position(ps->ir.fbutton, 0);
    combobox_set_entry_length(ps->ir.fbutton, 60);
    combobox_add_entry(ps->ir.fbutton, "None");
    ps->ir.fbutton->func.value_changed_callback = file_menu_callback;

    ps->ir.filebutton = add_lv2_irfile_button (ps->ir.filebutton, ui->elem[0], -3, "IR File", ui, 140, 24, 25, 25);
    ps->ir.filebutton->parent_struct = (void*)&ps->ir;
    ps->ir.filebutton->func.user_callback = file_load_response;

    ui->widget[2] = add_lv2_toggle_button (ui->widget[2], ui->elem[0], 9, "", ui, 170, 24, 25, 25);
    ui->widget[4] = add_lv2_erase_button (ui->widget[4], ui->elem[0], 17, "", ui, 470, 24, 25, 25);

//IR 1
    ui->widget[1] = add_lv2_knob (ui->widget[1], ui->elem[0], 8, "Gain (R)", ui, 510, 15, 70, 80);
    set_adjustment(ui->widget[1]->adj, 0.0, 0.0, -20.0, 20.0, 0.2, CL_CONTINUOS);
    // controller label colour
    set_widget_color(ui->widget[1], (Color_state)0, (Color_mod)0, 0.592, 0.612, 0.631,1.0);
    // controller background colour
    set_widget_color(ui->widget[1], (Color_state)0, (Color_mod)1, 0.083, 0.083, 0.083, 1.0);
    // controller label colour hover
    set_widget_color(ui->widget[1], (Color_state)1, (Color_mod)0, 0.694, 0.714, 0.737,1.0);

    ui->widget[7] = add_lv2_knob (ui->widget[7], ui->elem[0], 35, "Master", ui, 510, 15, 70, 80);
    set_adjustment(ui->widget[7]->adj, 0.0, 0.0, -20.0, 20.0, 0.2, CL_CONTINUOS);
    // controller label colour
    set_widget_color(ui->widget[7], (Color_state)0, (Color_mod)0, 0.592, 0.612, 0.631,1.0);
    // controller background colour
    set_widget_color(ui->widget[7], (Color_state)0, (Color_mod)1, 0.083, 0.083, 0.083, 1.0);
    // controller label colour hover
    set_widget_color(ui->widget[7], (Color_state)1, (Color_mod)0, 0.694, 0.714, 0.737,1.0);

    ps->ir1.fbutton = add_lv2_button(ps->ir1.fbutton, ui->elem[0], "", ui, 445, 64, 22, 30);
    ps->ir1.fbutton->parent_struct = (void*)&ps->ir1;
    combobox_set_pop_position(ps->ir1.fbutton, 0);
    combobox_set_entry_length(ps->ir1.fbutton, 60);
    combobox_add_entry(ps->ir1.fbutton, "None");
    ps->ir1.fbutton->func.value_changed_callback = file_menu_callback;

    ps->ir1.filebutton = add_lv2_irfile_button (ps->ir1.filebutton, ui->elem[0], -4, "IR File", ui, 140, 68, 25, 25);
    ps->ir1.filebutton->parent_struct = (void*)&ps->ir1;
    ps->ir1.filebutton->func.user_callback = file_load_response;

    ui->widget[3] = add_lv2_toggle_button (ui->widget[3], ui->elem[0], 10, "", ui, 170, 68, 25, 25);
    ui->widget[5] = add_lv2_erase_button (ui->widget[5], ui->elem[0], 18, "", ui, 470, 68, 25, 25);

    // switch between Stereo and Mix output
    ui->widget[8] = add_lv2_vswitch (ui->widget[8], ui->elem[0], 33, "Stereo", ui, 20, 18, 35, 80);
}


void plugin_cleanup(X11_UI *ui) {
    X11_UI_Private_t *ps = (X11_UI_Private_t*)ui->private_ptr;
    free(ps->fname);
    free(ps->ir.filename);
    free(ps->ir.dir_name);
    free(ps->ir1.filename);
    free(ps->ir1.dir_name);
    fp_free(ps->ir.filepicker);
    free(ps->ir.filepicker);
    fp_free(ps->ir1.filepicker);
    free(ps->ir1.filepicker);
    // clean up used sources when needed
}


#ifdef __cplusplus
}
#endif
