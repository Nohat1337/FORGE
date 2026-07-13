#pragma once

#include <string>
#include <vector>
#include <map>

namespace Theme {
    enum class Type {
        DARK,
        LIGHT,
        SOLARIZED_DARK,
        SOLARIZED_LIGHT,
        DRACULA,
        NORD,
        ONEDARK,
        MONOKAI,
        GRUVBOX_DARK,
        CUSTOM,
    };

    struct Colors {
        // UI
        std::string bg_editor;
        std::string bg_panel;
        std::string bg_status;
        std::string bg_menu;
        std::string bg_gutter;
        std::string bg_gutter_current;
        std::string bg_selection;
        std::string bg_cursor;
        std::string bg_line_current;

        // Text
        std::string fg_editor;
        std::string fg_gutter;
        std::string fg_gutter_current;
        std::string fg_comment;
        std::string fg_string;
        std::string fg_number;
        std::string fg_keyword;
        std::string fg_keyword_control;
        std::string fg_builtin;
        std::string fg_function;
        std::string fg_operator;
        std::string fg_punctuation;
        std::string fg_type;
        std::string fg_preproc;
        std::string fg_error;

        // Accents
        std::string accent1;
        std::string accent2;
        std::string accent3;
        std::string accent4;
        std::string accent5;
        std::string accent6;

        // Cursor
        std::string cursor_fg;
        std::string cursor_bg;
    };

    const Colors& get(Type type);
    const Colors& getCurrent();
    void set(Type type);
    void setCustom(const Colors& colors);

    // Syntax token type to color mapping
    std::string tokenColor(const std::string& tokenType);
}