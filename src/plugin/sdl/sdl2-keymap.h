/* taken from qemu and adapted to dosemu by stsp */

static const int sdl2_scancode_to_keynum[SDL_NUM_SCANCODES] = {
    [SDL_SCANCODE_A]                 = NUM_A,
    [SDL_SCANCODE_B]                 = NUM_B,
    [SDL_SCANCODE_C]                 = NUM_C,
    [SDL_SCANCODE_D]                 = NUM_D,
    [SDL_SCANCODE_E]                 = NUM_E,
    [SDL_SCANCODE_F]                 = NUM_F,
    [SDL_SCANCODE_G]                 = NUM_G,
    [SDL_SCANCODE_H]                 = NUM_H,
    [SDL_SCANCODE_I]                 = NUM_I,
    [SDL_SCANCODE_J]                 = NUM_J,
    [SDL_SCANCODE_K]                 = NUM_K,
    [SDL_SCANCODE_L]                 = NUM_L,
    [SDL_SCANCODE_M]                 = NUM_M,
    [SDL_SCANCODE_N]                 = NUM_N,
    [SDL_SCANCODE_O]                 = NUM_O,
    [SDL_SCANCODE_P]                 = NUM_P,
    [SDL_SCANCODE_Q]                 = NUM_Q,
    [SDL_SCANCODE_R]                 = NUM_R,
    [SDL_SCANCODE_S]                 = NUM_S,
    [SDL_SCANCODE_T]                 = NUM_T,
    [SDL_SCANCODE_U]                 = NUM_U,
    [SDL_SCANCODE_V]                 = NUM_V,
    [SDL_SCANCODE_W]                 = NUM_W,
    [SDL_SCANCODE_X]                 = NUM_X,
    [SDL_SCANCODE_Y]                 = NUM_Y,
    [SDL_SCANCODE_Z]                 = NUM_Z,

    [SDL_SCANCODE_1]                 = NUM_1,
    [SDL_SCANCODE_2]                 = NUM_2,
    [SDL_SCANCODE_3]                 = NUM_3,
    [SDL_SCANCODE_4]                 = NUM_4,
    [SDL_SCANCODE_5]                 = NUM_5,
    [SDL_SCANCODE_6]                 = NUM_6,
    [SDL_SCANCODE_7]                 = NUM_7,
    [SDL_SCANCODE_8]                 = NUM_8,
    [SDL_SCANCODE_9]                 = NUM_9,
    [SDL_SCANCODE_0]                 = NUM_0,

    [SDL_SCANCODE_RETURN]            = NUM_RETURN,
    [SDL_SCANCODE_ESCAPE]            = NUM_ESC,
    [SDL_SCANCODE_BACKSPACE]         = NUM_BKSP,
    [SDL_SCANCODE_TAB]               = NUM_TAB,
    [SDL_SCANCODE_SPACE]             = NUM_SPACE,
    [SDL_SCANCODE_MINUS]             = NUM_DASH,
    [SDL_SCANCODE_EQUALS]            = NUM_EQUALS,
    [SDL_SCANCODE_LEFTBRACKET]       = NUM_LBRACK,
    [SDL_SCANCODE_RIGHTBRACKET]      = NUM_RBRACK,
    [SDL_SCANCODE_BACKSLASH]         = NUM_BACKSLASH,
#if 0
    [SDL_SCANCODE_NONUSHASH]         = NUM_NONUSHASH,
#endif
    [SDL_SCANCODE_SEMICOLON]         = NUM_SEMICOLON,
    [SDL_SCANCODE_APOSTROPHE]        = NUM_APOSTROPHE,
    [SDL_SCANCODE_GRAVE]             = NUM_GRAVE,
    [SDL_SCANCODE_COMMA]             = NUM_COMMA,
    [SDL_SCANCODE_PERIOD]            = NUM_PERIOD,
    [SDL_SCANCODE_SLASH]             = NUM_SLASH,
    [SDL_SCANCODE_CAPSLOCK]          = NUM_CAPS,

    [SDL_SCANCODE_F1]                = NUM_F1,
    [SDL_SCANCODE_F2]                = NUM_F2,
    [SDL_SCANCODE_F3]                = NUM_F3,
    [SDL_SCANCODE_F4]                = NUM_F4,
    [SDL_SCANCODE_F5]                = NUM_F5,
    [SDL_SCANCODE_F6]                = NUM_F6,
    [SDL_SCANCODE_F7]                = NUM_F7,
    [SDL_SCANCODE_F8]                = NUM_F8,
    [SDL_SCANCODE_F9]                = NUM_F9,
    [SDL_SCANCODE_F10]               = NUM_F10,
    [SDL_SCANCODE_F11]               = NUM_F11,
    [SDL_SCANCODE_F12]               = NUM_F12,

    [SDL_SCANCODE_PRINTSCREEN]       = NUM_PRTSCR_SYSRQ,
    [SDL_SCANCODE_SCROLLLOCK]        = NUM_SCROLL,
    [SDL_SCANCODE_PAUSE]             = NUM_PAUSE_BREAK,
    [SDL_SCANCODE_INSERT]            = NUM_INS,
    [SDL_SCANCODE_HOME]              = NUM_HOME,
    [SDL_SCANCODE_PAGEUP]            = NUM_PGUP,
    [SDL_SCANCODE_DELETE]            = NUM_DEL,
    [SDL_SCANCODE_END]               = NUM_END,
    [SDL_SCANCODE_PAGEDOWN]          = NUM_PGDN,
    [SDL_SCANCODE_RIGHT]             = NUM_RIGHT,
    [SDL_SCANCODE_LEFT]              = NUM_LEFT,
    [SDL_SCANCODE_DOWN]              = NUM_DOWN,
    [SDL_SCANCODE_UP]                = NUM_UP,
    [SDL_SCANCODE_NUMLOCKCLEAR]      = NUM_NUM,

    [SDL_SCANCODE_KP_DIVIDE]         = NUM_PAD_SLASH,
    [SDL_SCANCODE_KP_MULTIPLY]       = NUM_PAD_AST,
    [SDL_SCANCODE_KP_MINUS]          = NUM_PAD_MINUS,
    [SDL_SCANCODE_KP_PLUS]           = NUM_PAD_PLUS,
    [SDL_SCANCODE_KP_ENTER]          = NUM_PAD_ENTER,
    [SDL_SCANCODE_KP_1]              = NUM_PAD_1,
    [SDL_SCANCODE_KP_2]              = NUM_PAD_2,
    [SDL_SCANCODE_KP_3]              = NUM_PAD_3,
    [SDL_SCANCODE_KP_4]              = NUM_PAD_4,
    [SDL_SCANCODE_KP_5]              = NUM_PAD_5,
    [SDL_SCANCODE_KP_6]              = NUM_PAD_6,
    [SDL_SCANCODE_KP_7]              = NUM_PAD_7,
    [SDL_SCANCODE_KP_8]              = NUM_PAD_8,
    [SDL_SCANCODE_KP_9]              = NUM_PAD_9,
    [SDL_SCANCODE_KP_0]              = NUM_PAD_0,
    [SDL_SCANCODE_KP_PERIOD]         = NUM_PAD_DECIMAL,

    [SDL_SCANCODE_NONUSBACKSLASH]    = NUM_LESSGREATER,
    [SDL_SCANCODE_APPLICATION]       = NUM_MENU,
#if 0
    [SDL_SCANCODE_POWER]             = NUM_POWER,
    [SDL_SCANCODE_KP_EQUALS]         = NUM_KP_EQUALS,
#endif

    [SDL_SCANCODE_F13]               = NUM_F13,
    [SDL_SCANCODE_F14]               = NUM_F14,
    [SDL_SCANCODE_F15]               = NUM_F15,
    [SDL_SCANCODE_F16]               = NUM_F16,
    [SDL_SCANCODE_F17]               = NUM_F17,
    [SDL_SCANCODE_F18]               = NUM_F18,
    [SDL_SCANCODE_F19]               = NUM_F19,
    [SDL_SCANCODE_F20]               = NUM_F20,
    [SDL_SCANCODE_F21]               = NUM_F21,
    [SDL_SCANCODE_F22]               = NUM_F22,
    [SDL_SCANCODE_F23]               = NUM_F23,
    [SDL_SCANCODE_F24]               = NUM_F24,

#if 0
    [SDL_SCANCODE_EXECUTE]           = NUM_EXECUTE,
#endif
//    [SDL_SCANCODE_HELP]              = NUM_HELP,
    [SDL_SCANCODE_MENU]              = NUM_MENU,
#if 0
    [SDL_SCANCODE_SELECT]            = NUM_SELECT,
    [SDL_SCANCODE_STOP]              = NUM_STOP,
    [SDL_SCANCODE_AGAIN]             = NUM_AGAIN,
    [SDL_SCANCODE_UNDO]              = NUM_UNDO,
    [SDL_SCANCODE_CUT]               = NUM_CUT,
    [SDL_SCANCODE_COPY]              = NUM_COPY,
    [SDL_SCANCODE_PASTE]             = NUM_PASTE,
    [SDL_SCANCODE_FIND]              = NUM_FIND,
#endif
    [SDL_SCANCODE_MUTE]              = NUM_MUTE,
    [SDL_SCANCODE_VOLUMEUP]          = NUM_VOLUMEUP,
    [SDL_SCANCODE_VOLUMEDOWN]        = NUM_VOLUMEDOWN,

#if 0
    [SDL_SCANCODE_KP_COMMA]          = NUM_KP_COMMA,
    [SDL_SCANCODE_KP_EQUALSAS400]    = NUM_KP_EQUALSAS400,

    [SDL_SCANCODE_INTERNATIONAL1]    = NUM_INTERNATIONAL1,
    [SDL_SCANCODE_INTERNATIONAL2]    = NUM_INTERNATIONAL2,
    [SDL_SCANCODE_INTERNATIONAL3]    = NUM_INTERNATIONAL3,
    [SDL_SCANCODE_INTERNATIONAL4]    = NUM_INTERNATIONAL4,
    [SDL_SCANCODE_INTERNATIONAL5]    = NUM_INTERNATIONAL5,
    [SDL_SCANCODE_INTERNATIONAL6]    = NUM_INTERNATIONAL6,
    [SDL_SCANCODE_INTERNATIONAL7]    = NUM_INTERNATIONAL7,
    [SDL_SCANCODE_INTERNATIONAL8]    = NUM_INTERNATIONAL8,
    [SDL_SCANCODE_INTERNATIONAL9]    = NUM_INTERNATIONAL9,
    [SDL_SCANCODE_LANG1]             = NUM_LANG1,
    [SDL_SCANCODE_LANG2]             = NUM_LANG2,
    [SDL_SCANCODE_LANG3]             = NUM_LANG3,
    [SDL_SCANCODE_LANG4]             = NUM_LANG4,
    [SDL_SCANCODE_LANG5]             = NUM_LANG5,
    [SDL_SCANCODE_LANG6]             = NUM_LANG6,
    [SDL_SCANCODE_LANG7]             = NUM_LANG7,
    [SDL_SCANCODE_LANG8]             = NUM_LANG8,
    [SDL_SCANCODE_LANG9]             = NUM_LANG9,
    [SDL_SCANCODE_ALTERASE]          = NUM_ALTERASE,
#endif
    [SDL_SCANCODE_SYSREQ]            = NUM_SYSRQ,
#if 0
    [SDL_SCANCODE_CANCEL]            = NUM_CANCEL,
    [SDL_SCANCODE_CLEAR]             = NUM_CLEAR,
    [SDL_SCANCODE_PRIOR]             = NUM_PRIOR,
    [SDL_SCANCODE_RETURN2]           = NUM_RETURN2,
    [SDL_SCANCODE_SEPARATOR]         = NUM_SEPARATOR,
    [SDL_SCANCODE_OUT]               = NUM_OUT,
    [SDL_SCANCODE_OPER]              = NUM_OPER,
    [SDL_SCANCODE_CLEARAGAIN]        = NUM_CLEARAGAIN,
    [SDL_SCANCODE_CRSEL]             = NUM_CRSEL,
    [SDL_SCANCODE_EXSEL]             = NUM_EXSEL,
    [SDL_SCANCODE_KP_00]             = NUM_KP_00,
    [SDL_SCANCODE_KP_000]            = NUM_KP_000,
    [SDL_SCANCODE_THOUSANDSSEPARATOR] = NUM_THOUSANDSSEPARATOR,
    [SDL_SCANCODE_DECIMALSEPARATOR]  = NUM_DECIMALSEPARATOR,
    [SDL_SCANCODE_CURRENCYUNIT]      = NUM_CURRENCYUNIT,
    [SDL_SCANCODE_CURRENCYSUBUNIT]   = NUM_CURRENCYSUBUNIT,
    [SDL_SCANCODE_KP_LEFTPAREN]      = NUM_KP_LEFTPAREN,
    [SDL_SCANCODE_KP_RIGHTPAREN]     = NUM_KP_RIGHTPAREN,
    [SDL_SCANCODE_KP_LEFTBRACE]      = NUM_KP_LEFTBRACE,
    [SDL_SCANCODE_KP_RIGHTBRACE]     = NUM_KP_RIGHTBRACE,
    [SDL_SCANCODE_KP_TAB]            = NUM_KP_TAB,
    [SDL_SCANCODE_KP_BACKSPACE]      = NUM_KP_BACKSPACE,
    [SDL_SCANCODE_KP_A]              = NUM_KP_A,
    [SDL_SCANCODE_KP_B]              = NUM_KP_B,
    [SDL_SCANCODE_KP_C]              = NUM_KP_C,
    [SDL_SCANCODE_KP_D]              = NUM_KP_D,
    [SDL_SCANCODE_KP_E]              = NUM_KP_E,
    [SDL_SCANCODE_KP_F]              = NUM_KP_F,
    [SDL_SCANCODE_KP_XOR]            = NUM_KP_XOR,
    [SDL_SCANCODE_KP_POWER]          = NUM_KP_POWER,
    [SDL_SCANCODE_KP_PERCENT]        = NUM_KP_PERCENT,
    [SDL_SCANCODE_KP_LESS]           = NUM_KP_LESS,
    [SDL_SCANCODE_KP_GREATER]        = NUM_KP_GREATER,
    [SDL_SCANCODE_KP_AMPERSAND]      = NUM_KP_AMPERSAND,
    [SDL_SCANCODE_KP_DBLAMPERSAND]   = NUM_KP_DBLAMPERSAND,
    [SDL_SCANCODE_KP_VERTICALBAR]    = NUM_KP_VERTICALBAR,
    [SDL_SCANCODE_KP_DBLVERTICALBAR] = NUM_KP_DBLVERTICALBAR,
    [SDL_SCANCODE_KP_COLON]          = NUM_KP_COLON,
    [SDL_SCANCODE_KP_HASH]           = NUM_KP_HASH,
    [SDL_SCANCODE_KP_SPACE]          = NUM_KP_SPACE,
    [SDL_SCANCODE_KP_AT]             = NUM_KP_AT,
    [SDL_SCANCODE_KP_EXCLAM]         = NUM_KP_EXCLAM,
    [SDL_SCANCODE_KP_MEMSTORE]       = NUM_KP_MEMSTORE,
    [SDL_SCANCODE_KP_MEMRECALL]      = NUM_KP_MEMRECALL,
    [SDL_SCANCODE_KP_MEMCLEAR]       = NUM_KP_MEMCLEAR,
    [SDL_SCANCODE_KP_MEMADD]         = NUM_KP_MEMADD,
    [SDL_SCANCODE_KP_MEMSUBTRACT]    = NUM_KP_MEMSUBTRACT,
    [SDL_SCANCODE_KP_MEMMULTIPLY]    = NUM_KP_MEMMULTIPLY,
    [SDL_SCANCODE_KP_MEMDIVIDE]      = NUM_KP_MEMDIVIDE,
    [SDL_SCANCODE_KP_PLUSMINUS]      = NUM_KP_PLUSMINUS,
    [SDL_SCANCODE_KP_CLEAR]          = NUM_KP_CLEAR,
    [SDL_SCANCODE_KP_CLEARENTRY]     = NUM_KP_CLEARENTRY,
    [SDL_SCANCODE_KP_BINARY]         = NUM_KP_BINARY,
    [SDL_SCANCODE_KP_OCTAL]          = NUM_KP_OCTAL,
    [SDL_SCANCODE_KP_DECIMAL]        = NUM_KP_DECIMAL,
    [SDL_SCANCODE_KP_HEXADECIMAL]    = NUM_KP_HEXADECIMAL,
#endif
    [SDL_SCANCODE_LCTRL]             = NUM_L_CTRL,
    [SDL_SCANCODE_LSHIFT]            = NUM_L_SHIFT,
    [SDL_SCANCODE_LALT]              = NUM_L_ALT,
    [SDL_SCANCODE_LGUI]              = NUM_LWIN,
    [SDL_SCANCODE_RCTRL]             = NUM_R_CTRL,
    [SDL_SCANCODE_RSHIFT]            = NUM_R_SHIFT,
    [SDL_SCANCODE_RALT]              = NUM_R_ALT,
    [SDL_SCANCODE_RGUI]              = NUM_RWIN,
#if 0
    [SDL_SCANCODE_MODE]              = NUM_MODE,
    [SDL_SCANCODE_AUDIONEXT]         = NUM_AUDIONEXT,
    [SDL_SCANCODE_AUDIOPREV]         = NUM_AUDIOPREV,
    [SDL_SCANCODE_AUDIOSTOP]         = NUM_AUDIOSTOP,
    [SDL_SCANCODE_AUDIOPLAY]         = NUM_AUDIOPLAY,
    [SDL_SCANCODE_AUDIOMUTE]         = NUM_AUDIOMUTE,
    [SDL_SCANCODE_MEDIASELECT]       = NUM_MEDIASELECT,
    [SDL_SCANCODE_WWW]               = NUM_WWW,
    [SDL_SCANCODE_MAIL]              = NUM_MAIL,
    [SDL_SCANCODE_CALCULATOR]        = NUM_CALCULATOR,
    [SDL_SCANCODE_COMPUTER]          = NUM_COMPUTER,
    [SDL_SCANCODE_AC_SEARCH]         = NUM_AC_SEARCH,
    [SDL_SCANCODE_AC_HOME]           = NUM_AC_HOME,
    [SDL_SCANCODE_AC_BACK]           = NUM_AC_BACK,
    [SDL_SCANCODE_AC_FORWARD]        = NUM_AC_FORWARD,
    [SDL_SCANCODE_AC_STOP]           = NUM_AC_STOP,
    [SDL_SCANCODE_AC_REFRESH]        = NUM_AC_REFRESH,
#endif
    [SDL_SCANCODE_AC_BOOKMARKS]      = NUM_AC_BOOKMARKS,
#if 0
    [SDL_SCANCODE_BRIGHTNESSDOWN]    = NUM_BRIGHTNESSDOWN,
    [SDL_SCANCODE_BRIGHTNESSUP]      = NUM_BRIGHTNESSUP,
    [SDL_SCANCODE_DISPLAYSWITCH]     = NUM_DISPLAYSWITCH,
    [SDL_SCANCODE_KBDILLUMTOGGLE]    = NUM_KBDILLUMTOGGLE,
    [SDL_SCANCODE_KBDILLUMDOWN]      = NUM_KBDILLUMDOWN,
    [SDL_SCANCODE_KBDILLUMUP]        = NUM_KBDILLUMUP,
    [SDL_SCANCODE_EJECT]             = NUM_EJECT,
    [SDL_SCANCODE_SLEEP]             = NUM_SLEEP,
    [SDL_SCANCODE_APP1]              = NUM_APP1,
    [SDL_SCANCODE_APP2]              = NUM_APP2,
#endif
};
