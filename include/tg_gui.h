/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * Telegram Amiga native GUI (Phase 5b). Portable model + renderer over a thin
 * per-platform backend (Intuition/RastPort). The console/TUI line is separate
 * and untouched; both sit on the same MTProto core. See
 * docs/GUI_ARCHITECTURE.md.
 */

#ifndef TG_GUI_H
#define TG_GUI_H

/* Themes are skins over the same pen roles below; the renderer is theme-blind. */
#define TG_GUI_THEME_DARK 0  /* default modern theme */
#define TG_GUI_THEME_LIGHT 1 /* follow Workbench pens */
#define TG_GUI_THEME_AMIRC 2 /* future classic skin */

/* Colour roles. The backend resolves these to pens/RGB for the active theme so
   the portable renderer never hard-codes a colour. */
#define TG_GUI_PEN_WINDOW 0      /* window background */
#define TG_GUI_PEN_SURFACE 1     /* sidebar, panels, incoming bubble */
#define TG_GUI_PEN_TEXT 2        /* primary text */
#define TG_GUI_PEN_TEXT_DIM 3    /* secondary text, timestamps */
#define TG_GUI_PEN_ACCENT 4      /* selection, own bubble, send button */
#define TG_GUI_PEN_ACCENT_TEXT 5 /* text on an accent fill */
#define TG_GUI_PEN_SELECT 6      /* selected chat-row tint */
#define TG_GUI_PEN_BADGE 7       /* unread badge fill */
#define TG_GUI_PEN_BADGE_TEXT 8  /* unread badge number */
#define TG_GUI_PEN_COUNT 9

/* Distinct avatar / sender tints, resolved by the backend like the pens. */
#define TG_GUI_AVATAR_COLORS 6

typedef struct tg_gui_rect {
    int x;
    int y;
    int w;
    int h;
} tg_gui_rect;

/* Inline text styling, a bitmask the renderer derives from message markup
   (the *bold* / _italic_ / `code` / ~strike~ markers baked in at parse time)
   and applies through the backend's set_style hook before drawing each run. */
#define TG_GUI_STYLE_BOLD   1
#define TG_GUI_STYLE_ITALIC 2
#define TG_GUI_STYLE_CODE   4
#define TG_GUI_STYLE_STRIKE 8

/* Per-platform drawing shim. The portable renderer owns all layout and only
   calls these. The host build supplies a recording backend for the self-test. */
typedef struct tg_gui_backend tg_gui_backend;
struct tg_gui_backend {
    void *context;
    int (*width)(tg_gui_backend *backend);
    int (*height)(tg_gui_backend *backend);
    int (*line_height)(tg_gui_backend *backend);
    int (*text_width)(tg_gui_backend *backend, const char *text,
                      unsigned long length);
    void (*fill_rect)(tg_gui_backend *backend, int pen, tg_gui_rect rect);
    void (*avatar_fill)(tg_gui_backend *backend, int color_index,
                        tg_gui_rect rect);
    void (*draw_text)(tg_gui_backend *backend, int pen, int x, int baseline,
                      const char *text, unsigned long length);
    /* Apply a TG_GUI_STYLE_* bitmask to subsequent draw_text calls. NULL on
       backends that render plain (the renderer then just skips the markers). */
    void (*set_style)(tg_gui_backend *backend, int style);
};

#define TG_GUI_MAX_CHATS 32
#define TG_GUI_MAX_MESSAGES 64
#define TG_GUI_NAME_MAX 48
#define TG_GUI_TEXT_MAX 256
#define TG_GUI_TIME_MAX 8
#define TG_GUI_INITIALS_MAX 4
#define TG_GUI_REPLY_MAX 96

typedef struct tg_gui_chat {
    char name[TG_GUI_NAME_MAX];
    char preview[TG_GUI_TEXT_MAX];
    char time[TG_GUI_TIME_MAX];
    char initials[TG_GUI_INITIALS_MAX];
    int avatar_color;
    int unread;
    unsigned long index;      /* 1-based peer-cache number, to open this chat */
    unsigned long peer_id_hi; /* matches a notification to this row */
    unsigned long peer_id_lo;
    int flash;                /* a notification landed since last opened: blink the badge */
} tg_gui_chat;

/* Window mode: the normal chat UI, or one of the first-login screens shown when
   there is no saved session yet. The login screens reuse the input buffer +
   caret; the renderer paints a centered panel instead of the chat layout. */
#define TG_GUI_MODE_CHAT        0
#define TG_GUI_MODE_LOGIN_PHONE 1 /* enter phone number */
#define TG_GUI_MODE_LOGIN_CODE  2 /* enter the login code */
#define TG_GUI_MODE_LOGIN_2FA   3 /* enter the 2FA password (masked) */

/* Outgoing read state for the receipt mark; 0/none on incoming messages. */
#define TG_GUI_READ_NONE 0
#define TG_GUI_READ_SENT 1 /* delivered, not yet read by the peer */
#define TG_GUI_READ_SEEN 2 /* the peer has read it (id <= read_outbox_max) */

typedef struct tg_gui_message {
    char sender[TG_GUI_NAME_MAX];
    char text[TG_GUI_TEXT_MAX];
    char time[TG_GUI_TIME_MAX];
    int sender_color;
    int is_own;
    int is_system;
    char reply_text[TG_GUI_REPLY_MAX]; /* quoted reference; "" when not a reply */
    unsigned long id;                  /* server message id (0 = optimistic echo) */
    int read_state;                    /* TG_GUI_READ_* (own messages only) */
} tg_gui_message;

typedef struct tg_gui_state {
    tg_gui_chat chats[TG_GUI_MAX_CHATS];
    int chat_count;
    int selected_chat;
    char title[TG_GUI_NAME_MAX];
    char subtitle[TG_GUI_NAME_MAX];
    tg_gui_message messages[TG_GUI_MAX_MESSAGES];
    int message_count;
    char input[TG_GUI_TEXT_MAX];
    char status[TG_GUI_NAME_MAX];
    int theme;
    int composing;  /* the input field is focused */
    int cursor_on;  /* caret blink phase, toggled by the window's tick */
    unsigned long open_read_outbox_max; /* peer read our msgs up to this id */
    char typing[TG_GUI_NAME_MAX]; /* "...sta scrivendo" for the open chat; "" = none */
    int mode;          /* TG_GUI_MODE_* (chat vs a first-login screen) */
    int input_masked;  /* render `input` as '*' (the 2FA password field) */
} tg_gui_state;

/* Fills state with the demo conversation the GUI design was signed off on; used
   by --gui-test (real window, later) and by the self-test. */
void tg_gui_demo_state(tg_gui_state *state);

/* The shared renderer: paints the whole window for the backend's current
   geometry by issuing backend calls. Amiga backends draw to a RastPort; the
   host backend records the calls for the self-test. */
void tg_gui_paint(const tg_gui_state *state, tg_gui_backend *backend);

/* Maps a click at renderer-space (x, y) to an actionable region, so a mouse
   can drive the same things the keyboard does. Returns a chat-row index
   (0..chat_count-1) to open that chat, or one of the negative codes below. */
#define TG_GUI_HIT_NONE (-1)
#define TG_GUI_HIT_INPUT (-2) /* the message input field: start composing */
#define TG_GUI_HIT_SEND (-3)  /* the Send button */
int tg_gui_hit_test(const tg_gui_state *state, int width, int height, int lh,
                    int x, int y);

/* Toggles the leading full-window background clear in tg_gui_paint (default
   on). Turn it off for an in-place repaint of unchanged, opaque content -- the
   redraw-time measurement uses this so its repeated repaints do not flash the
   window. */
void tg_gui_set_background_clear(int enabled);

/* Portable layout self-test: paints the demo state into a recording backend and
   checks the invariants (something drawn, nothing outside the window). Prints a
   one-line result and returns 0 on success. Host-CI runnable. */
int tg_gui_self_test(void);

/* Milestone 0: opens a real native window painted by tg_gui_paint, with a
   close gadget / Q to quit, live resize, and a redraw-time + footprint
   measurement printed to stdout. The Amiga backends implement it over
   Intuition/RastPort; the host build prints a notice (use --gui-self-test).
   Returns 0 on a clean exit, non-zero when the window could not open.
   The state is mutable: number keys 1-9 and n/p move the chat selection in
   place (updating the highlighted row and the header title). */
int tg_gui_run_window(tg_gui_state *state);

#endif
