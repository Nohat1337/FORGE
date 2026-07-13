#include "theme.hpp"
#include "terminal.hpp"
#include <algorithm>

namespace Theme {

static Type currentType = Type::DARK;
static Colors customColors;

const Colors& get(Type type) {
    static Colors dark = {
        // UI
        ansi::bg256(235), ansi::bg256(234), ansi::bg256(23), ansi::bg256(233),
        ansi::bg256(233), ansi::bg256(23), ansi::bg256(237), ansi::bg256(208), ansi::bg256(236),
        // Text
        ansi::fg256(253), ansi::fg256(240), ansi::fg256(254), ansi::fg256(242),
        ansi::fg256(114), ansi::fg256(222), ansi::fg256(208), ansi::fg256(208),
        ansi::fg256(226), ansi::fg256(81), ansi::fg256(198), ansi::fg256(244),
        ansi::fg256(75), ansi::fg256(198), ansi::fg256(196),
        // Accents
        ansi::fg256(208), ansi::fg256(226), ansi::fg256(81), ansi::fg256(114),
        ansi::fg256(198), ansi::fg256(177),
        // Cursor
        ansi::fg256(232), ansi::fg256(208),
    };

    static Colors dracula = {
        ansi::bg256(235), ansi::bg256(236), ansi::bg256(62), ansi::bg256(235),
        ansi::bg256(234), ansi::bg256(62), ansi::bg256(238), ansi::bg256(208), ansi::bg256(237),
        ansi::fg256(253), ansi::fg256(242), ansi::fg256(253), ansi::fg256(102),
        ansi::fg256(149), ansi::fg256(209), ansi::fg256(203), ansi::fg256(203),
        ansi::fg256(189), ansi::fg256(117), ansi::fg256(203), ansi::fg256(245),
        ansi::fg256(117), ansi::fg256(203), ansi::fg256(196),
        ansi::fg256(203), ansi::fg256(189), ansi::fg256(117), ansi::fg256(149),
        ansi::fg256(203), ansi::fg256(141),
        ansi::fg256(235), ansi::fg256(208),
    };

    static Colors nord = {
        ansi::bg256(236), ansi::bg256(235), ansi::bg256(24), ansi::bg256(235),
        ansi::bg256(234), ansi::bg256(24), ansi::bg256(237), ansi::bg256(109), ansi::bg256(237),
        ansi::fg256(253), ansi::fg256(241), ansi::fg256(253), ansi::fg256(109),
        ansi::fg256(144), ansi::fg256(214), ansi::fg256(167), ansi::fg256(167),
        ansi::fg256(208), ansi::fg256(110), ansi::fg256(167), ansi::fg256(245),
        ansi::fg256(110), ansi::fg256(167), ansi::fg256(196),
        ansi::fg256(167), ansi::fg256(214), ansi::fg256(110), ansi::fg256(144),
        ansi::fg256(167), ansi::fg256(139),
        ansi::fg256(236), ansi::fg256(109),
    };

    static Colors oneDark = {
        ansi::bg256(234), ansi::bg256(235), ansi::bg256(237), ansi::bg256(235),
        ansi::bg256(233), ansi::bg256(237), ansi::bg256(238), ansi::bg256(166), ansi::bg256(236),
        ansi::fg256(188), ansi::fg256(240), ansi::fg256(188), ansi::fg256(244),
        ansi::fg256(152), ansi::fg256(215), ansi::fg256(208), ansi::fg256(208),
        ansi::fg256(215), ansi::fg256(81), ansi::fg256(180), ansi::fg256(245),
        ansi::fg256(81), ansi::fg256(180), ansi::fg256(196),
        ansi::fg256(208), ansi::fg256(215), ansi::fg256(81), ansi::fg256(152),
        ansi::fg256(180), ansi::fg256(175),
        ansi::fg256(234), ansi::fg256(208),
    };

    static Colors monokai = {
        ansi::bg256(234), ansi::bg256(235), ansi::bg256(237), ansi::bg256(235),
        ansi::bg256(233), ansi::bg256(237), ansi::bg256(238), ansi::bg256(208), ansi::bg256(236),
        ansi::fg256(253), ansi::fg256(242), ansi::fg256(253), ansi::fg256(102),
        ansi::fg256(190), ansi::fg256(222), ansi::fg256(208), ansi::fg256(208),
        ansi::fg256(226), ansi::fg256(117), ansi::fg256(174), ansi::fg256(245),
        ansi::fg256(117), ansi::fg256(174), ansi::fg256(196),
        ansi::fg256(208), ansi::fg256(226), ansi::fg256(117), ansi::fg256(190),
        ansi::fg256(174), ansi::fg256(135),
        ansi::fg256(234), ansi::fg256(208),
    };

    static Colors gruvboxDark = {
        ansi::bg256(234), ansi::bg256(235), ansi::bg256(130), ansi::bg256(235),
        ansi::bg256(233), ansi::bg256(130), ansi::bg256(237), ansi::bg256(166), ansi::bg256(236),
        ansi::fg256(223), ansi::fg256(245), ansi::fg256(223), ansi::fg256(245),
        ansi::fg256(142), ansi::fg256(214), ansi::fg256(166), ansi::fg256(166),
        ansi::fg256(214), ansi::fg256(117), ansi::fg256(203), ansi::fg256(245),
        ansi::fg256(109), ansi::fg256(203), ansi::fg256(196),
        ansi::fg256(166), ansi::fg256(214), ansi::fg256(117), ansi::fg256(142),
        ansi::fg256(203), ansi::fg256(175),
        ansi::fg256(234), ansi::fg256(166),
    };

    static Colors solarizedDark = {
        ansi::bg256(234), ansi::bg256(235), ansi::bg256(24), ansi::bg256(235),
        ansi::bg256(234), ansi::bg256(24), ansi::bg256(237), ansi::bg256(166), ansi::bg256(236),
        ansi::fg256(188), ansi::fg256(241), ansi::fg256(188), ansi::fg256(101),
        ansi::fg256(142), ansi::fg256(174), ansi::fg256(166), ansi::fg256(166),
        ansi::fg256(166), ansi::fg256(75), ansi::fg256(125), ansi::fg256(101),
        ansi::fg256(75), ansi::fg256(125), ansi::fg256(160),
        ansi::fg256(166), ansi::fg256(174), ansi::fg256(75), ansi::fg256(142),
        ansi::fg256(125), ansi::fg256(133),
        ansi::fg256(234), ansi::fg256(166),
    };

    static Colors light = {
        ansi::bg256(255), ansi::bg256(250), ansi::bg256(31), ansi::bg256(245),
        ansi::bg256(245), ansi::bg256(31), ansi::bg256(230), ansi::bg256(202), ansi::bg256(240),
        ansi::fg256(232), ansi::fg256(240), ansi::fg256(16), ansi::fg256(100),
        ansi::fg256(28), ansi::fg256(130), ansi::fg256(166), ansi::fg256(166),
        ansi::fg256(136), ansi::fg256(25), ansi::fg256(161), ansi::fg256(100),
        ansi::fg256(27), ansi::fg256(161), ansi::fg256(196),
        ansi::fg256(166), ansi::fg256(136), ansi::fg256(25), ansi::fg256(28),
        ansi::fg256(161), ansi::fg256(92),
        ansi::fg256(255), ansi::fg256(202),
    };

    switch (type) {
        case Type::DARK: return dark;
        case Type::DRACULA: return dracula;
        case Type::NORD: return nord;
        case Type::ONEDARK: return oneDark;
        case Type::MONOKAI: return monokai;
        case Type::GRUVBOX_DARK: return gruvboxDark;
        case Type::SOLARIZED_DARK: return solarizedDark;
        case Type::LIGHT: return light;
        case Type::CUSTOM: return customColors;
        default: return dark;
    }
}

const Colors& getCurrent() {
    return get(currentType);
}

void set(Type type) {
    currentType = type;
}

void setCustom(const Colors& colors) {
    customColors = colors;
    currentType = Type::CUSTOM;
}

std::string tokenColor(const std::string& tokenType) {
    const Colors& c = getCurrent();
    if (tokenType == "keyword") return c.fg_keyword;
    if (tokenType == "keyword_control") return c.fg_keyword_control;
    if (tokenType == "keyword_decl") return c.fg_keyword;
    if (tokenType == "keyword_other") return c.fg_keyword;
    if (tokenType == "string") return c.fg_string;
    if (tokenType == "number") return c.fg_number;
    if (tokenType == "comment") return c.fg_comment;
    if (tokenType == "operator") return c.fg_operator;
    if (tokenType == "builtin") return c.fg_builtin;
    if (tokenType == "function") return c.fg_function;
    if (tokenType == "type") return c.fg_type;
    if (tokenType == "punctuation") return c.fg_punctuation;
    if (tokenType == "preproc") return c.fg_preproc;
    if (tokenType == "error") return c.fg_error;
    return c.fg_editor;
}

}