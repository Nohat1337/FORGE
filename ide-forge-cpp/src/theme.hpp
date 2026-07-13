#pragma once

#include <string>
#include <unordered_map>

namespace forge_studio {

enum class ThemeType {
    DARK,
    DRACULA,
    NORD,
    ONE_DARK,
    MONOKAI,
    GRUVBOX,
    SOLARIZED_DARK,
    SOLARIZED_LIGHT,
    LIGHT
};

struct Theme {
    std::string name;
    
    // Editor
    uint32_t editor_bg;
    uint32_t editor_fg;
    uint32_t gutter_bg;
    uint32_t gutter_fg;
    uint32_t gutter_current_bg;
    uint32_t gutter_current_fg;
    uint32_t selection_bg;
    uint32_t cursor_bg;
    uint32_t cursor_fg;
    uint32_t line_current_bg;
    
    // Menu
    uint32_t menu_bg;
    uint32_t menu_fg;
    uint32_t menu_sel_bg;
    uint32_t menu_sel_fg;
    
    // Status bar
    uint32_t status_bg;
    uint32_t status_fg;
    
    // Panels
    uint32_t panel_bg;
    uint32_t panel_fg;
    uint32_t panel_border;
    
    // Accents
    uint32_t accent1;
    uint32_t accent2;
    uint32_t accent3;
    uint32_t accent4;
    uint32_t accent5;
    uint32_t accent6;
    
    // Syntax
    uint32_t keyword_control;
    uint32_t keyword_decl;
    uint32_t keyword_other;
    uint32_t string;
    uint32_t string_interp;
    uint32_t number;
    uint32_t comment;
    uint32_t operator_;
    uint32_t function;
    uint32_t builtin;
    uint32_t type;
    uint32_t variable;
    uint32_t punctuation;
    uint32_t preproc;
    uint32_t error;
};

const Theme& get_theme(ThemeType type);
const Theme& get_theme(const std::string& name);
ThemeType get_theme_type(const std::string& name);
std::string theme_type_to_string(ThemeType type);
std::string get_current_theme_name();
void set_current_theme(ThemeType type);
void set_current_theme(const std::string& name);

ThemeType get_next_theme(ThemeType current);
ThemeType get_prev_theme(ThemeType current);

} // namespace forge_studio