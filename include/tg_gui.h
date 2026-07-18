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
#define TG_GUI_PEN_READ 9        /* read-receipt double check (azure on own bubble) */
#define TG_GUI_PEN_COUNT 10

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
    /* OPTIONAL: draw the peer's real avatar (decoded stripped thumb) into rect.
       Returns 1 when it drew, 0 to make the renderer fall back to the classic
       initials square. NULL on backends without image support (host tests). */
    int (*avatar_image)(tg_gui_backend *backend, unsigned long peer_id_hi,
                        unsigned long peer_id_lo, tg_gui_rect rect);
};

/* Sidebar capacity. Raised from 32 so a congested account shows far more of its
   chats at once; m68k stays lower for its tighter RAM box. Must stay <=
   TG_CHAT_LIST_MAX and TG_MTPROTO_PEER_CACHE_MAX (the display is their min). */
#if defined(__m68k__)
#define TG_GUI_MAX_CHATS 64
#else
#define TG_GUI_MAX_CHATS 128
#endif
#define TG_GUI_MAX_MESSAGES 100 /* deeper backlog at open (was 64); the open
                                   getHistory loads ~90, leaving room for live ones */
#define TG_GUI_NAME_MAX 48
#define TG_GUI_TEXT_MAX 256

/* '@' mention autocomplete popup (composer). */
#define TG_GUI_MENTION_MAX 5
#define TG_GUI_MENTION_LEN 40
/* Received-message body buffer, decoupled from the 256-byte preview/input/name
   size so long messages are not truncated at ingestion. The MTProto parse holds
   up to TG_MTPROTO_MESSAGE_TEXT_MAX = 4096, so PPC/AROS use the full 4096 to show
   ANY message in full; m68k is capped lower for its 2 MB box. 64 of these live in
   tg_gui_state, which the gui-live path keeps STATIC (off the stack). */
#if defined(__m68k__)
#define TG_GUI_MSG_TEXT_MAX 2048
#else
#define TG_GUI_MSG_TEXT_MAX 4096
#endif
#define TG_GUI_HISTORY_MAX 16
#define TG_GUI_TIME_MAX 8
#define TG_GUI_INITIALS_MAX 4
#define TG_GUI_REPLY_MAX 96
#define TG_GUI_SEARCH_MAX 64

/* Width of the custom-drawn vertical scrollbars; shared with the event loop so
   a click maps to the same strip the painter drew. */
#define TG_GUI_SCROLLBAR_W 14

/* Sidebar row index sentinel for the pinned "Saved Messages" (self chat) row.
   Canonical home (tg_gui_session.h re-exports it via this header): the UI layer
   needs it too, e.g. to offer Edit/Delete on every message of the self chat,
   where the server clears the out flag (Telegram semantics: nothing in Saved
   Messages is "outgoing", yet everything there is yours to edit/delete). */
#define TG_GUI_SAVED_PEER_INDEX 0xfffffffeUL

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
    char text[TG_GUI_MSG_TEXT_MAX];
    char time[TG_GUI_TIME_MAX];
    int sender_color;
    int is_own;
    int is_system;
    char reply_text[TG_GUI_REPLY_MAX]; /* quoted reference; "" when not a reply */
    unsigned long id;                  /* server message id (0 = optimistic echo) */
    int read_state;                    /* TG_GUI_READ_* (own messages only) */
    unsigned long from_id_hi;          /* sender user id, to name a live typing peer */
    unsigned long from_id_lo;
    int has_document;                  /* F9: a document is attached (Download) */
} tg_gui_message;

typedef struct tg_gui_state {
    tg_gui_chat chats[TG_GUI_MAX_CHATS];
    int chat_count;
    int selected_chat;
    char title[TG_GUI_NAME_MAX];
    char subtitle[TG_GUI_NAME_MAX];
    tg_gui_message messages[TG_GUI_MAX_MESSAGES];
    int message_count;
    int chat_scroll;       /* first visible chat row in the sidebar (0 = top) */
    int chat_scroll_to_sel; /* one-shot: next paint scrolls the list to selected_chat */
    int transcript_scroll; /* PIXELS scrolled up from the newest-pinned bottom (0 = newest) */
    int input_h;           /* composer box height (px), cached by the painter for the hit-test */
    /* Scrollbar geometry the painter caches each frame for the event loop's
       knob-drag / track-click (only the painter has the backend to size the
       transcript). *_max == 0 means no bar / nothing to drag. */
    int sb_list_x, sb_list_ty, sb_list_th, sb_list_ky, sb_list_kh, sb_list_max;
    int sb_tr_x, sb_tr_ty, sb_tr_th, sb_tr_ky, sb_tr_kh, sb_tr_max;
    int sb_drag;    /* 0 none, 1 chat list, 2 transcript -- a knob being dragged */
    int sb_grab_dy; /* cursor y within the knob when the drag began */
    /* Drag-and-drop reorder of the sidebar chat list. drag_src = the armed source
       row (chats[] index, >= 0); -1 = idle (doubles as the armed flag). drag_active
       flips on once the gesture passes the click/drag threshold (then the painter
       draws the insertion line + dims the source row). drag_press_y / drag_cur_y
       are window-inner-relative cursor Y (mouse_y - origin_y) at press and now. */
    int drag_src;
    int drag_active;
    int drag_press_y;
    int drag_cur_y;
    char input[TG_GUI_MSG_TEXT_MAX];
    char status[TG_GUI_NAME_MAX];
    int theme;
    int composing;  /* the input field is focused */
    int input_caret; /* caret byte offset into input[] (0..strlen); chat composer */
    int cursor_on;  /* caret blink phase, toggled by the window's tick */
    unsigned long open_read_outbox_max; /* peer read our msgs up to this id */
    char typing[TG_GUI_NAME_MAX]; /* "...sta scrivendo" for the open chat; "" = none */
    int mode;          /* TG_GUI_MODE_* (chat vs a first-login screen) */
    int input_masked;  /* render `input` as '*' (the 2FA password field) */
    /* Sent-message history for UP/DOWN recall in the composer (like the TUI). */
    char history[TG_GUI_HISTORY_MAX][TG_GUI_MSG_TEXT_MAX];
    int history_count;
    int history_pos;   /* -1 = not recalling; else index into history[] */
    char history_draft[TG_GUI_MSG_TEXT_MAX]; /* live text stashed on first UP */
    /* Chat search (the sidebar "Search chats..." box). search_active = the box is
       focused (typing); in_search = the sidebar currently shows online search
       results instead of the cached chat list (ESC restores it). */
    int search_active;
    int search_caret;
    char search_query[TG_GUI_SEARCH_MAX];
    int in_search;
    int search_dirty;     /* query changed since the last online search (debounce) */
    /* The open chat has older history beyond what is loaded (server total > shown).
       When the loaded rows fit the window (no real scrollbar), the painter still
       draws a short scrollbar so the user has a handle to drag up / wheel up and
       pull the previous page -- needed for media-heavy chats the server returns a
       few messages at a time. Cleared once the chat start is reached. */
    int more_above;

    /* Scroll-to-bottom button (Telegram's floating down-arrow), Amiga-adapted.
       newest_dropped: set by the driver when a load-older prepend EVICTS the
       true-newest tail from the full ring -- transcript_scroll==0 is then NOT the
       real newest, so a jump must RELOAD. Cleared centrally in
       tg_gui_session_open_chat (every newest-reload path funnels through it).
       unread_below: live messages appended while NOT at the true bottom (the
       button's badge). jb_*: button rect the painter caches each frame for the
       event-loop hit-test (jb_w == 0 means no button this frame). */
    int newest_dropped;
    int unread_below;
    int jb_x, jb_y, jb_w, jb_h;

    /* Reply compose: which message the next send threads to (0 = no reply).
       Invariant: reply_to_id==0 <=> reply_sender[0]=='\0' && reply_snippet[0]=='\0'.
       msg_top[] caches each rendered row's y-top (newest-first hit-test), valid
       only when msg_cached == message_count for the frame just painted. */
    unsigned long reply_to_id;
    char reply_sender[TG_GUI_NAME_MAX];
    char reply_snippet[TG_GUI_REPLY_MAX];
    int msg_top[TG_GUI_MAX_MESSAGES];
    int msg_cached;
    /* transcript area of the last paint (for the char-level hit test) */
    int tr_area_x;
    int tr_area_w;
    /* mouse text selection inside ONE transcript message (issue #5 part two):
       press latched on SELECTDOWN, becomes a selection when the pointer drags
       past a small threshold, click-without-drag keeps the old reply gesture
       (executed on SELECTUP). sel_a/sel_b are unordered char indexes. */
    int sel_active;
    int sel_msg;
    long sel_a;
    long sel_b;
    unsigned long sel_gen_snap;  /* msg_gen when made: any shift invalidates */
    int sel_press_armed;
    int sel_press_msg;
    long sel_press_char;
    unsigned long sel_press_id;  /* the message ID latched at press: the
                                    deferred reply verifies it at release */
    unsigned long sel_press_gen;
    int sel_press_x;
    int sel_press_y;
    /* Transcript GENERATION: bumped on EVERY mutation of messages[] (append,
       own echo, load-older prepend, reload, chat switch). A count snapshot is
       NOT enough: a full ring shifts every index at constant count. */
    unsigned long msg_gen;

    /* Right-click context menu: a small popup at the pointer over a message
       bubble. ctx_visible gates the paint + hit-test; ctx_msg is the target
       message index captured when the menu opened; ctx_x/ctx_y is the pointer
       (window-inner coords) the box anchors to (clamped into the window).
       ctx_hover is the 0-based item index under the pointer (-1 = none), so the
       popup highlights the entry the click will pick. */
    int ctx_visible;
    int ctx_msg;
    int ctx_x, ctx_y;
    int ctx_hover;
    int selected_msg; /* transcript row highlighted by a click (-1 = none) */

    /* '@' mention autocomplete over the composer: while the caret sits in an
       "@prefix" token, mention_items holds up to TG_GUI_MENTION_MAX candidate
       usernames (no leading '@') from the open group's member cache.
       mention_active gates paint + key routing (Up/Down pick, ENTER/TAB
       complete, ESC closes); mention_start is the byte offset of the '@'. */
    int mention_active;
    int mention_count;
    int mention_sel;
    int mention_start;
    char mention_items[TG_GUI_MENTION_MAX][TG_GUI_MENTION_LEN];

    /* Composer is editing this own message in place (0 = composing a new one).
       Set when "Edit" is picked from the context menu; the next send routes to
       messages.editMessage instead of sendMessage. */
    unsigned long edit_to_id;
} tg_gui_state;

/* Fills state with the demo conversation the GUI design was signed off on; used
   by --gui-test (real window, later) and by the self-test. */
void tg_gui_demo_state(tg_gui_state *state);

/* The shared renderer: paints the whole window for the backend's current
   geometry by issuing backend calls. Amiga backends draw to a RastPort; the
   host backend records the calls for the self-test. */
void tg_gui_paint(const tg_gui_state *state, tg_gui_backend *backend);

/* Repaints ONLY the active caret region (composer input row in chat mode, login
   input box otherwise). Lets the ~2 Hz caret blink avoid a full-window repaint,
   which was visible as a constant refresh on slow OS3 planar displays. */
void tg_gui_paint_caret(const tg_gui_state *state, tg_gui_backend *backend);

/* Maps a click at renderer-space (x, y) to an actionable region, so a mouse
   can drive the same things the keyboard does. Returns a chat-row index
   (0..chat_count-1) to open that chat, or one of the negative codes below. */
#define TG_GUI_HIT_NONE (-1)
#define TG_GUI_HIT_INPUT (-2) /* the message input field: start composing */
#define TG_GUI_HIT_SEND (-3)  /* the Send button */
#define TG_GUI_HIT_SEARCH (-4) /* the sidebar search box: focus it */
#define TG_GUI_HIT_JUMP_BOTTOM (-5) /* the floating scroll-to-bottom button */
#define TG_GUI_HIT_REPLY_CANCEL (-6) /* the "Replying to ..." composer header */
/* Transcript message pick: message i -> (TG_GUI_HIT_MESSAGE_BASE - i). */
#define TG_GUI_HIT_MESSAGE_BASE (-100)
int tg_gui_hit_test(const tg_gui_state *state, int width, int height, int lh,
                    int x, int y);

/* Right-click context-menu geometry/items. Fixed width keeps the hit-test
   backend-free (no text measuring). One item for now; the list is laid out so
   more (Copy/Edit/Delete) can follow the roadmap. */
#define TG_GUI_CTX_W 108
/* Item ids returned by tg_gui_context_menu_hit. Which items the popup shows
   depends on the target message: Reply always; Edit + Delete only on an own
   message that has a server id (you can only edit/delete your own). Send file is
   chat-level (not tied to the clicked message) and mirrors the menubar item. */
#define TG_GUI_CTX_REPLY 0
#define TG_GUI_CTX_EDIT 1
#define TG_GUI_CTX_DELETE 2
#define TG_GUI_CTX_DOWNLOAD 3
#define TG_GUI_CTX_SENDFILE 4
#define TG_GUI_CTX_COPY 5
#define TG_GUI_CTX_ITEMS_MAX 6

/* 1 when the currently selected sidebar row is the pinned Saved Messages
   (self) chat -- the row whose index carries TG_GUI_SAVED_PEER_INDEX. There
   the server clears the out flag on every message (nothing in Saved Messages
   is "outgoing"), yet everything is yours: Edit/Delete gate on this too. */
int tg_gui_open_chat_is_self(const tg_gui_state *state);
/* Maps a click at renderer-space (x, y) to a context-menu item id
   (TG_GUI_CTX_REPLY/EDIT/DELETE) when the popup is open, or -1 when the click is
   outside it (the caller then dismisses the menu). */
int tg_gui_context_menu_hit(const tg_gui_state *state, int width, int height,
                            int lh, int x, int y);
/* Maps a pointer at renderer-space (x, y) to the 0-based context-menu item INDEX
   under it (for hover highlighting), or -1 when the pointer is outside the popup.
   Same geometry as tg_gui_context_menu_hit, which returns the item id instead. */
int tg_gui_context_menu_index(const tg_gui_state *state, int width, int height,
                              int lh, int x, int y);

/* Locates the '@'-token the caret sits in: scans back from `caret` for an '@'
   that starts the line or follows whitespace, with no whitespace between it and
   the caret. Returns the prefix length (bytes after the '@', may be 0) and sets
   *start to the '@' byte offset; returns -1 when the caret is in no such token.
   Pure text logic (self-tested); the window layer feeds the popup with it. */
int tg_gui_mention_token(const char *input, int caret, int *start);

/* F8 click-to-caret: map a click at renderer coords to a byte offset in the
   composer input / the sidebar search query. -1 = outside the field. */
/* Char index inside message `msg_index` for a click at inner (x,y), clamped
   to the message's text (above/left -> 0 side, below/right -> line/text end);
   -1 when the cached layout is stale or the message has no text geometry.
   Uses the SAME wrap + bubble geometry as the painter. */
long tg_gui_transcript_char_at(const tg_gui_state *state,
                               tg_gui_backend *backend, int lh, int msg_index,
                               int x, int y);

/* Copies the selected substring into out (NUL-terminated). 1 = a non-empty
   selection was copied, 0 = no live selection. */
int tg_gui_selection_get(const tg_gui_state *state, char *out,
                         unsigned long out_size);

int tg_gui_input_click_caret(const tg_gui_state *state,
                             tg_gui_backend *backend, int x, int y);
int tg_gui_search_click_caret(const tg_gui_state *state,
                              tg_gui_backend *backend, int x, int y);

/* Click over the '@' mention popup -> candidate index, or -1 if outside/absent.
   Lets the mouse pick a mention the way the cursor keys already do. */
int tg_gui_mention_click(const tg_gui_state *state, tg_gui_backend *backend,
                         int x, int y);

/* Window layer: drop the cached avatar pen grid for one peer (after a fresh
   download) so the next repaint rebuilds it from the disk JPEG. */
void tg_gui_window_avatar_invalidate(unsigned long id_hi, unsigned long id_lo);

/* Maps a cursor Y (window-inner-relative) to a drag-and-drop INSERT-BEFORE target
   in [0, chat_count] for the sidebar list (chat_count == drop at the end). Uses
   the same search_h/row_h/chat_scroll geometry the painter uses, rounded to the
   nearest gap. Shared by the event loop (on drop) and the painter (insertion line)
   so they never disagree. */
int tg_gui_chat_drop_target(const tg_gui_state *state, int lh, int y);

/* The sidebar (chat-list) width for a window width -- shared with the event
   loop so a mouse-wheel can tell which panel the pointer is over. */
int tg_gui_sidebar_w(int width);

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
