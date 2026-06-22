/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * Phase 5b milestone 0: the native window backend behind tg_gui_backend.
 * One file, two branches -- the OS4 interface model and the classic shared
 * base model (OS3 / MorphOS / AROS) differ only in how libraries are opened;
 * every draw call (RectFill, Text, TextLength) is source-identical because the
 * portable renderer in tg_gui.c does all the layout. The host build (no Amiga
 * platform macro) compiles only the stub at the bottom.
 *
 * The window is painted entirely by tg_gui_paint(); this file is the thin
 * backend (window + RastPort + theme pens + event loop) plus a redraw-time and
 * footprint measurement, which is the whole point of milestone 0 on a 68k.
 */

#include "tg_gui.h"
#include "tg_gui_session.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(__amigaos3__) || defined(__amigaos4__) || defined(__MORPHOS__) || \
    defined(__AROS__)
#define TG_GUI_AMIGA 1
#endif

/* While the composer is focused, defer the (blocking) live network poll until
   typing has paused this many seconds, so a quiet recv never stalls active
   keystrokes -- yet live messages and the "is typing" header still arrive as
   soon as you pause. Without this the poll was skipped for the whole time the
   composer was focused, and "keep composer focus after send" leaves it focused,
   so reception + the typing header silently stopped until a chat switch. */
#define TG_GUI_COMPOSE_IDLE_POLL_SECONDS 3UL

#if defined(TG_GUI_AMIGA)

/* AmigaOS4: make the library calls expand to interface inlines (IGraphics->...,
   IIntuition->...) instead of unresolved external symbols, exactly as the OS4
   platform file does. Harmless on the classic-base targets. */
#ifndef __USE_INLINE__
#define __USE_INLINE__
#endif

#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <graphics/gfx.h>
#include <graphics/text.h>
#include <graphics/view.h>
#include <utility/tagitem.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <proto/dos.h>

/* TagItem ti_Data is pointer-sized. Only AROS needs IPTR (64-bit on AROS
   x86_64, where a ULONG cast would truncate the pointer); the 32-bit targets
   (OS3, MorphOS, OS4 PPC) hold a pointer in a ULONG and do not all expose
   IPTR here. */
#if defined(__AROS__)
#define TG_GUI_TAG(p) ((IPTR)(p))
#else
#define TG_GUI_TAG(p) ((ULONG)(p))
#endif

/* graphics.library is not opened by the C startup or by any platform file, so
   this translation unit owns its base (single definition program-wide). The
   OS4 type is a plain Library plus a separate interface. intuition.library's
   base is defined by the platform file; we only borrow it here. */
#if defined(__amigaos4__)
struct Library *GfxBase = 0;
struct GraphicsIFace *IGraphics = 0;
struct Library *GadToolsBase = 0;
struct GadToolsIFace *IGadTools = 0;
#else
struct GfxBase *GfxBase = 0;
struct Library *GadToolsBase = 0;
#endif

/* Menu item ids (GadTools NM_USERDATA), decoded on IDCMP_MENUPICK. */
#define TG_MENU_ABOUT 1
#define TG_MENU_HELP  2
#define TG_MENU_QUIT  3

/* Dark-theme palette: one RGB triplet per pen role and per avatar tint. The
   backend resolves the renderer's pen indices to obtained pens here; a future
   light / AmIRC theme is just another table. */
typedef struct tg_gui_rgb {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} tg_gui_rgb;

static const tg_gui_rgb tg_gui_dark_pens[TG_GUI_PEN_COUNT] = {
    {0x12, 0x14, 0x1a}, /* WINDOW */
    {0x20, 0x24, 0x2e}, /* SURFACE */
    {0xe6, 0xea, 0xf0}, /* TEXT */
    {0x93, 0x9a, 0xa6}, /* TEXT_DIM */
    {0x2a, 0x6e, 0xb4}, /* ACCENT */
    {0xf0, 0xf6, 0xff}, /* ACCENT_TEXT */
    {0x1a, 0x2c, 0x44}, /* SELECT */
    {0x18, 0x5f, 0xa5}, /* BADGE */
    {0xe6, 0xf1, 0xfb}  /* BADGE_TEXT */
};

static const tg_gui_rgb tg_gui_avatar_rgb[TG_GUI_AVATAR_COLORS] = {
    {0x2f, 0x8f, 0x74}, /* teal */
    {0xc0, 0x5a, 0x3c}, /* coral */
    {0x6a, 0x5f, 0xc8}, /* purple */
    {0xbf, 0x52, 0x7e}, /* pink */
    {0xb0, 0x7a, 0x1f}, /* amber */
    {0x2c, 0x7c, 0xb8}  /* blue */
};

typedef struct tg_gui_amiga_ctx {
    struct Window *window;
    struct RastPort *rport;
    int origin_x;
    int origin_y;
    int inner_w;
    int inner_h;
    int line_h;
    LONG pens[TG_GUI_PEN_COUNT];               /* usable draw pen (fallback if obtain failed) */
    LONG pens_obtained[TG_GUI_PEN_COUNT];       /* raw ObtainBestPenA result, -1 if failed */
    LONG avatar_pens[TG_GUI_AVATAR_COLORS];
    LONG avatar_obtained[TG_GUI_AVATAR_COLORS];
    int pens_held;
} tg_gui_amiga_ctx;

static int tg_gui_amiga_width(tg_gui_backend *backend)
{
    return ((tg_gui_amiga_ctx *)backend->context)->inner_w;
}

static int tg_gui_amiga_height(tg_gui_backend *backend)
{
    return ((tg_gui_amiga_ctx *)backend->context)->inner_h;
}

static int tg_gui_amiga_line_height(tg_gui_backend *backend)
{
    return ((tg_gui_amiga_ctx *)backend->context)->line_h;
}

static int tg_gui_amiga_text_width(tg_gui_backend *backend, const char *text,
                                   unsigned long length)
{
    tg_gui_amiga_ctx *ctx;

    ctx = (tg_gui_amiga_ctx *)backend->context;
    if (length == 0UL) {
        return 0;
    }
    if (length > 0x7fffUL) {
        length = 0x7fffUL; /* TextLength count is 16-bit; clamp defensively */
    }
    return (int)TextLength(ctx->rport, (STRPTR)text, (UWORD)length);
}

static LONG tg_gui_amiga_resolve_pen(tg_gui_amiga_ctx *ctx, int pen)
{
    if (pen >= TG_GUI_PEN_COUNT) {
        int avatar;

        avatar = pen - TG_GUI_PEN_COUNT;
        if (avatar < 0 || avatar >= TG_GUI_AVATAR_COLORS) {
            avatar = 0;
        }
        return ctx->avatar_pens[avatar];
    }
    if (pen < 0 || pen >= TG_GUI_PEN_COUNT) {
        pen = TG_GUI_PEN_TEXT;
    }
    return ctx->pens[pen];
}

static void tg_gui_amiga_fill_rect(tg_gui_backend *backend, int pen,
                                   tg_gui_rect rect)
{
    tg_gui_amiga_ctx *ctx;
    int x0;
    int y0;
    int x1;
    int y1;

    ctx = (tg_gui_amiga_ctx *)backend->context;
    if (rect.w <= 0 || rect.h <= 0) {
        return;
    }
    x0 = ctx->origin_x + rect.x;
    y0 = ctx->origin_y + rect.y;
    x1 = x0 + rect.w - 1;
    y1 = y0 + rect.h - 1;
    SetAPen(ctx->rport, tg_gui_amiga_resolve_pen(ctx, pen));
    RectFill(ctx->rport, x0, y0, x1, y1);
}

static void tg_gui_amiga_avatar_fill(tg_gui_backend *backend, int color_index,
                                     tg_gui_rect rect)
{
    tg_gui_amiga_ctx *ctx;
    int x0;
    int y0;

    ctx = (tg_gui_amiga_ctx *)backend->context;
    if (rect.w <= 0 || rect.h <= 0) {
        return;
    }
    if (color_index < 0 || color_index >= TG_GUI_AVATAR_COLORS) {
        color_index = 0;
    }
    x0 = ctx->origin_x + rect.x;
    y0 = ctx->origin_y + rect.y;
    SetAPen(ctx->rport, ctx->avatar_pens[color_index]);
    RectFill(ctx->rport, x0, y0, x0 + rect.w - 1, y0 + rect.h - 1);
}

static void tg_gui_amiga_draw_text(tg_gui_backend *backend, int pen, int x,
                                   int baseline, const char *text,
                                   unsigned long length)
{
    tg_gui_amiga_ctx *ctx;

    ctx = (tg_gui_amiga_ctx *)backend->context;
    if (length == 0UL) {
        return;
    }
    if (length > 0x7fffUL) {
        length = 0x7fffUL; /* Text count is 16-bit; clamp defensively */
    }
    SetAPen(ctx->rport, tg_gui_amiga_resolve_pen(ctx, pen));
    SetDrMd(ctx->rport, JAM1);
    Move(ctx->rport, ctx->origin_x + x, ctx->origin_y + baseline);
    Text(ctx->rport, (STRPTR)text, (UWORD)length);
}

/* Map the renderer's style bitmask to graphics.library soft styles. Bold and
   italic are the algorithmic font styles; code and strike reuse underline
   (no soft strikethrough exists). The OS clamps to what the font supports. */
static void tg_gui_amiga_set_style(tg_gui_backend *backend, int style)
{
    tg_gui_amiga_ctx *ctx;
    ULONG soft;

    ctx = (tg_gui_amiga_ctx *)backend->context;
    soft = 0UL;
    if ((style & TG_GUI_STYLE_BOLD) != 0) {
        soft |= FSF_BOLD;
    }
    if ((style & TG_GUI_STYLE_ITALIC) != 0) {
        soft |= FSF_ITALIC;
    }
    if ((style & (TG_GUI_STYLE_CODE | TG_GUI_STYLE_STRIKE)) != 0) {
        soft |= FSF_UNDERLINED;
    }
    SetSoftStyle(ctx->rport, soft, FSF_BOLD | FSF_ITALIC | FSF_UNDERLINED);
}

static ULONG tg_gui_amiga_rgb32(unsigned char component)
{
    ULONG c;

    c = (ULONG)component;
    return (c << 24) | (c << 16) | (c << 8) | c;
}

static void tg_gui_amiga_obtain_pens(tg_gui_amiga_ctx *ctx,
                                     struct ColorMap *cmap)
{
    int i;

    ctx->pens_held = 0;
    for (i = 0; i < TG_GUI_PEN_COUNT; ++i) {
        /* Keep the raw result so release frees ONLY what was really obtained;
           the drawing value falls back to a stock pen when obtain fails, but
           that fallback must never be passed to ReleasePen. */
        ctx->pens_obtained[i] =
            ObtainBestPenA(cmap, tg_gui_amiga_rgb32(tg_gui_dark_pens[i].r),
                           tg_gui_amiga_rgb32(tg_gui_dark_pens[i].g),
                           tg_gui_amiga_rgb32(tg_gui_dark_pens[i].b), 0);
        ctx->pens[i] = (ctx->pens_obtained[i] == -1)
                           ? ((i == TG_GUI_PEN_WINDOW) ? 0L : 1L)
                           : ctx->pens_obtained[i];
    }
    for (i = 0; i < TG_GUI_AVATAR_COLORS; ++i) {
        ctx->avatar_obtained[i] =
            ObtainBestPenA(cmap, tg_gui_amiga_rgb32(tg_gui_avatar_rgb[i].r),
                           tg_gui_amiga_rgb32(tg_gui_avatar_rgb[i].g),
                           tg_gui_amiga_rgb32(tg_gui_avatar_rgb[i].b), 0);
        ctx->avatar_pens[i] =
            (ctx->avatar_obtained[i] == -1) ? 1L : ctx->avatar_obtained[i];
    }
    ctx->pens_held = 1;
}

static void tg_gui_amiga_release_pens(tg_gui_amiga_ctx *ctx,
                                      struct ColorMap *cmap)
{
    int i;

    if (!ctx->pens_held) {
        return;
    }
    for (i = 0; i < TG_GUI_PEN_COUNT; ++i) {
        if (ctx->pens_obtained[i] != -1) {
            ReleasePen(cmap, ctx->pens_obtained[i]);
        }
    }
    for (i = 0; i < TG_GUI_AVATAR_COLORS; ++i) {
        if (ctx->avatar_obtained[i] != -1) {
            ReleasePen(cmap, ctx->avatar_obtained[i]);
        }
    }
    ctx->pens_held = 0;
}

static void tg_gui_amiga_measure_geometry(tg_gui_amiga_ctx *ctx)
{
    struct Window *w;

    w = ctx->window;
    ctx->origin_x = w->BorderLeft;
    ctx->origin_y = w->BorderTop;
    ctx->inner_w = (int)w->Width - w->BorderLeft - w->BorderRight;
    ctx->inner_h = (int)w->Height - w->BorderTop - w->BorderBottom;
    if (ctx->inner_w < 1) {
        ctx->inner_w = 1;
    }
    if (ctx->inner_h < 1) {
        ctx->inner_h = 1;
    }
}

/* Mirrors the selected chat's name into the header title so the title line
   tracks the highlighted sidebar row as the user navigates. */
static void tg_gui_window_apply_selection(tg_gui_state *state)
{
    const char *name;
    unsigned long i;

    if (state == 0 || state->chat_count <= 0) {
        return;
    }
    if (state->selected_chat < 0) {
        state->selected_chat = 0;
    }
    if (state->selected_chat >= state->chat_count) {
        state->selected_chat = state->chat_count - 1;
    }
    name = state->chats[state->selected_chat].name;
    for (i = 0UL; i + 1UL < (unsigned long)sizeof(state->title) &&
                  name[i] != '\0'; ++i) {
        state->title[i] = name[i];
    }
    state->title[i] = '\0';
}

/* Serialize every direct render against Intuition's layer machinery.
   OpenWindowTagList guarantees only a non-NULL Window -- NOT when the layer is
   safe to draw -- and the input.device/intuition task edits this window's
   ClipRect list at activation, sizing and dragging. An unserialized RastPort
   write while it does so corrupts the cliprect chain and, on MorphOS, freezes
   the whole box inside layers3d (DSI). LockLayerRom()/UnlockLayerRom() (the only
   layers.library pair the autodocs sanction for Intuition windows; reached here
   through graphics.library / GfxBase, already open, so no extra library) blocks
   Intuition from touching the layer while we render. The bracket is short (one
   full renderer paint of pure graphics ops -- no Intuition calls inside, which
   the LockLayer autodoc forbids) and rport->Layer is the window's layer for our
   non-GIMMEZEROZERO window. The IDCMP_REFRESHWINDOW path does NOT use these: it
   already runs inside BeginRefresh()'s own layer lock. */
static void tg_gui_window_paint(const tg_gui_state *state,
                                tg_gui_backend *backend)
{
    tg_gui_amiga_ctx *c = (tg_gui_amiga_ctx *)backend->context;
    struct Layer *layer = (c != 0 && c->rport != 0) ? c->rport->Layer : 0;

    if (layer != 0) {
        LockLayerRom(layer);
    }
    tg_gui_paint(state, backend);
    if (layer != 0) {
        UnlockLayerRom(layer);
    }
}

/* Caret-only blink repaint, same layer-lock discipline as tg_gui_window_paint(). */
static void tg_gui_window_paint_caret(const tg_gui_state *state,
                                      tg_gui_backend *backend)
{
    tg_gui_amiga_ctx *c = (tg_gui_amiga_ctx *)backend->context;
    struct Layer *layer = (c != 0 && c->rport != 0) ? c->rport->Layer : 0;

    if (layer != 0) {
        LockLayerRom(layer);
    }
    tg_gui_paint_caret(state, backend);
    if (layer != 0) {
        UnlockLayerRom(layer);
    }
}

/* Switch to chat `sel`: show its header + an empty transcript at once, then
   fetch the history. The instant first paint keeps the switch responsive on a
   slow link instead of the window appearing frozen on the old chat until the
   load finishes. */
static void tg_gui_window_open_selection(tg_gui_state *state, int sel,
                                         tg_gui_backend *backend)
{
    state->selected_chat = sel;
    state->transcript_scroll = 0; /* a freshly opened chat pins to the newest */
    state->chat_scroll_to_sel = 1; /* scroll the sidebar so the row is visible */
    /* Opening a chat clears its unread badge / flash -- you are now reading it. */
    if (sel >= 0 && sel < state->chat_count) {
        state->chats[sel].unread = 0;
        state->chats[sel].flash = 0;
    }
    tg_gui_window_apply_selection(state);
    if (tg_gui_session_is_open()) {
        state->message_count = 0;
        tg_gui_window_paint(state, backend);
        (void)tg_gui_session_open_chat(state->chats[sel].index, stdout);
    }
    tg_gui_window_paint(state, backend);
}

/* Bounded copy into a fixed UI buffer (status/title) -- never overflows even if
   a future string grows past the field. */
static void tg_gui_window_copy(char *dest, unsigned long size, const char *src)
{
    unsigned long i;

    if (dest == 0 || size == 0UL) {
        return;
    }
    for (i = 0UL; i + 1UL < size && src != 0 && src[i] != '\0'; ++i) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

/* Push a just-sent line into the composer recall ring (UP/DOWN). Skips empty and
   consecutive duplicates, shifts the ring when full, resets the recall cursor.
   Mirrors the TUI's tg_chat_history_add. */
static void tg_gui_history_add(tg_gui_state *state, const char *text)
{
    int i;

    state->history_pos = -1;
    if (text == 0 || text[0] == '\0') {
        return;
    }
    if (state->history_count > 0 &&
        strcmp(state->history[state->history_count - 1], text) == 0) {
        return;
    }
    if (state->history_count >= TG_GUI_HISTORY_MAX) {
        for (i = 1; i < TG_GUI_HISTORY_MAX; ++i) {
            tg_gui_window_copy(state->history[i - 1], TG_GUI_TEXT_MAX,
                               state->history[i]);
        }
        state->history_count = TG_GUI_HISTORY_MAX - 1;
    }
    tg_gui_window_copy(state->history[state->history_count], TG_GUI_TEXT_MAX,
                       text);
    ++state->history_count;
}

/* Brings the window live after a successful login: opens the session over the
   freshly-written auth.bin (activate flips state->mode to chat + opens the first
   chat). If the re-open fails, the login itself still succeeded (auth.bin is
   written), so drop to chat mode offline rather than trapping the user back in
   the login screen -- where login.active is now cleared and every retry would
   just error. A relaunch will connect. */
static void tg_gui_window_login_finish(tg_gui_state *state)
{
    if (tg_gui_session_login_activate(state, stdout) == 0) {
        return;
    }
    state->mode = TG_GUI_MODE_CHAT;
    state->composing = 0;
    state->input_masked = 0;
    state->input[0] = '\0';
    tg_gui_window_copy(state->title, sizeof(state->title), "Telegram Amiga");
    tg_gui_window_copy(state->status, sizeof(state->status),
                       "Logged in - relaunch to connect");
}

/* Handles one key while a login screen is shown (state->mode != CHAT). ESC
   aborts the window; printable keys edit the field; RETURN submits the field
   to the matching auth step and advances the screen (or shows an error). The
   network round-trip blocks, so a "Connessione..." status is painted first. */
static void tg_gui_window_login_key(tg_gui_state *state, UWORD code,
                                    tg_gui_backend *backend, int *done,
                                    int *caret_ticks)
{
    if (code == 27) { /* ESC: give up on logging in */
        memset(state->input, 0, sizeof(state->input)); /* wipe any typed secret */
        state->input_masked = 0;
        *done = 1;
        return;
    }
    if (code == 8 || code == 127) { /* BACKSPACE */
        unsigned long n;

        n = (unsigned long)strlen(state->input);
        if (n > 0UL) {
            state->input[n - 1UL] = '\0';
            tg_gui_window_paint(state, backend);
        }
        return;
    }
    if (code != 13 && code != 10) { /* a printable character */
        if (code >= 32 && code < 256) {
            unsigned long n;

            n = (unsigned long)strlen(state->input);
            if (n + 1UL < (unsigned long)sizeof(state->input)) {
                state->input[n] = (char)code;
                state->input[n + 1UL] = '\0';
                tg_gui_window_paint(state, backend);
            }
        }
        return;
    }

    /* RETURN: submit the current field. Show progress first -- the DH/RPC round
       trip blocks the window for several seconds on a slow link. */
    tg_gui_window_copy(state->status, sizeof(state->status),
                       "Connecting to Telegram...");
    state->cursor_on = 0;
    tg_gui_window_paint(state, backend);

    if (state->mode == TG_GUI_MODE_LOGIN_PHONE) {
        int rc;

        rc = tg_gui_session_login_send_code(state->input, stdout);
        state->input[0] = '\0';
        if (rc == TG_GUI_LOGIN_OK) {
            state->mode = TG_GUI_MODE_LOGIN_CODE;
            tg_gui_window_copy(state->status, sizeof(state->status),
                               "Enter the code you received");
        } else {
            const char *e = tg_gui_session_login_last_error();
            tg_gui_window_copy(state->status, sizeof(state->status),
                               (e != 0 && e[0] != '\0')
                                   ? e : "Invalid number - try again");
        }
    } else if (state->mode == TG_GUI_MODE_LOGIN_CODE) {
        int rc;

        rc = tg_gui_session_login_sign_in(state->input, stdout);
        state->input[0] = '\0';
        if (rc == TG_GUI_LOGIN_OK) {
            tg_gui_window_login_finish(state);
        } else if (rc == TG_GUI_LOGIN_NEED_2FA) {
            state->mode = TG_GUI_MODE_LOGIN_2FA;
            state->input_masked = 1;
            tg_gui_window_copy(state->status, sizeof(state->status),
                               "2FA password");
        } else if (rc == TG_GUI_LOGIN_BAD_CODE) {
            tg_gui_window_copy(state->status, sizeof(state->status),
                               "Wrong code - try again");
        } else {
            const char *e = tg_gui_session_login_last_error();
            tg_gui_window_copy(state->status, sizeof(state->status),
                               (e != 0 && e[0] != '\0')
                                   ? e : "Error - try the code again");
        }
    } else { /* TG_GUI_MODE_LOGIN_2FA */
        int rc;

        rc = tg_gui_session_login_check_password(state->input, stdout);
        memset(state->input, 0, sizeof(state->input)); /* wipe the password */
        if (rc == TG_GUI_LOGIN_OK) {
            state->input_masked = 0;
            tg_gui_window_login_finish(state);
        } else if (rc == TG_GUI_LOGIN_BAD_PASSWORD) {
            tg_gui_window_copy(state->status, sizeof(state->status),
                               "Wrong password - try again");
        } else {
            tg_gui_window_copy(state->status, sizeof(state->status),
                               "Error - try the password again");
        }
    }
    state->cursor_on = 1;
    *caret_ticks = 0;
    tg_gui_window_paint(state, backend);
}

/* The right-button menu strip (laid out by GadTools so the metrics follow the
   screen font). Quit also gets the standard Right-Amiga+Q shortcut. */
static struct NewMenu tg_gui_newmenu[] = {
    { NM_TITLE, (STRPTR)"Telegram", 0, 0, 0, 0 },
    { NM_ITEM,  (STRPTR)"About...", 0, 0, 0, (APTR)TG_MENU_ABOUT },
    { NM_ITEM,  (STRPTR)"Help...",  0, 0, 0, (APTR)TG_MENU_HELP },
    { NM_ITEM,  (STRPTR)NM_BARLABEL, 0, 0, 0, 0 },
    { NM_ITEM,  (STRPTR)"Quit", (STRPTR)"Q", 0, 0, (APTR)TG_MENU_QUIT },
    { NM_END,   0, 0, 0, 0, 0 }
};

static const char tg_gui_about_text[] =
    "Telegram Amiga\n\n"
    "A native Telegram client for AmigaOS,\n"
    "MorphOS and AROS.\n\n"
    "by Michele Dipace\n"
    "michele.dipace@kaffeine.net";

static const char tg_gui_help_text[] =
    "Chat selection:\n"
    "  F1 - F10          chats 1 to 10\n"
    "  Shift + F1 - F10  chats 11 to 20\n\n"
    "ENTER   write a message to the open chat\n"
    "ESC     cancel\n"
    "Q       quit";

/* Shows a one-button info requester (About / Help). No printf args, so the
   text is passed verbatim (it carries no '%'). */
static void tg_gui_amiga_easyreq(struct Window *win, const char *title,
                                 const char *body)
{
    struct EasyStruct es;

    es.es_StructSize = (ULONG)sizeof(struct EasyStruct);
    es.es_Flags = 0UL;
    es.es_Title = (STRPTR)title;
    es.es_TextFormat = (STRPTR)body;
    es.es_GadgetFormat = (STRPTR)"OK";
    (void)EasyRequestArgs(win, &es, 0, 0);
}

/* Persist the last window size to a small file next to the binary (Work:TGh,
   which survives a reboot) so a reopen restores it; a roomier default on first
   run than the old 600x380. */
static void tg_gui_window_load_size(int *w, int *h)
{
    FILE *f;
    int rw;
    int rh;

    *w = 820;
    *h = 560;
    rw = 0;
    rh = 0;
    f = fopen("telegram-gui-win.txt", "r");
    if (f != 0) {
        if (fscanf(f, "%d %d", &rw, &rh) == 2 && rw >= 320 && rh >= 200 &&
            rw <= 4096 && rh <= 4096) {
            *w = rw;
            *h = rh;
        }
        fclose(f);
    }
}

static void tg_gui_window_save_size(int w, int h)
{
    FILE *f;

    if (w < 320 || h < 200) {
        return;
    }
    f = fopen("telegram-gui-win.txt", "w");
    if (f != 0) {
        fprintf(f, "%d %d\n", w, h);
        fclose(f);
    }
}

int tg_gui_run_window(tg_gui_state *state)
{
    tg_gui_amiga_ctx ctx;
    tg_gui_backend backend;
    int init_w;
    int init_h;
    struct TagItem tags[18];
    struct ColorMap *cmap;
    struct TextFont *font;
    APTR vi;
    struct Menu *menu;
    unsigned long mem_before;
    unsigned long mem_after;
    unsigned long footprint;
    int i;
    int done;
    int caret_ticks;
    unsigned long watch_seconds;
    unsigned long watch_boot_seconds;
    unsigned long watch_boot_grace;
    unsigned long effective_watch;
    time_t session_boot;
    time_t last_session_poll;
    time_t last_key_time;

    if (state == 0) {
        return 2;
    }

    /* We borrow the program-global IntuitionBase that the per-platform file
       defines and that tg_platform_display_beep() also opens transiently.
       Invariant for this lane: nothing may issue a DisplayBeep (or anything
       routed through that shared base) between OpenWindowTagList and
       CloseWindow below -- it would close the base out from under the live
       window. --gui-test honours this (no network, no notifications); the real
       client will own a single base centrally. graphics.library's base is
       genuinely owned by this file. */
#if defined(__amigaos4__)
    IntuitionBase = OpenLibrary("intuition.library", 39L); /* struct Library * */
#else
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library",
                                                        39L);
#endif
    if (IntuitionBase == 0) {
        puts("gui window: cannot open intuition.library");
        return 2;
    }
#if defined(__amigaos4__)
    IIntuition = (struct IntuitionIFace *)GetInterface(
        (struct Library *)IntuitionBase, "main", 1L, 0);
    if (IIntuition == 0) {
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = 0;
        puts("gui window: cannot get intuition interface");
        return 2;
    }
    GfxBase = OpenLibrary("graphics.library", 39L);
    if (GfxBase != 0) {
        IGraphics = (struct GraphicsIFace *)GetInterface(GfxBase, "main", 1L, 0);
        if (IGraphics == 0) {
            CloseLibrary(GfxBase);
            GfxBase = 0;
        }
    }
#else
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39L);
#endif
    if (GfxBase == 0) {
        puts("gui window: cannot open graphics.library");
#if defined(__amigaos4__)
        DropInterface((struct Interface *)IIntuition);
        IIntuition = 0;
#endif
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = 0;
        return 2;
    }

    memset(&ctx, 0, sizeof(ctx));
    tg_gui_window_load_size(&init_w, &init_h);
    i = 0;
    tags[i].ti_Tag = WA_Title;
    tags[i++].ti_Data = TG_GUI_TAG("Telegram Amiga - GUI");
    tags[i].ti_Tag = WA_InnerWidth;
    tags[i++].ti_Data = (ULONG)init_w;
    tags[i].ti_Tag = WA_InnerHeight;
    tags[i++].ti_Data = (ULONG)init_h;
    tags[i].ti_Tag = WA_DragBar;
    tags[i++].ti_Data = TRUE;
    tags[i].ti_Tag = WA_DepthGadget;
    tags[i++].ti_Data = TRUE;
    tags[i].ti_Tag = WA_CloseGadget;
    tags[i++].ti_Data = TRUE;
    tags[i].ti_Tag = WA_SizeGadget;
    tags[i++].ti_Data = TRUE;
    /* Open INACTIVE: activation is what makes the input.device/intuition task
       build this window's layer/ClipRect list, and an immediate paint that races
       that build is what freezes MorphOS inside layers3d. We paint the first
       frame under LockLayerRom() and only then ActivateWindow() (see below). */
    tags[i].ti_Tag = WA_Activate;
    tags[i++].ti_Data = FALSE;
    tags[i].ti_Tag = WA_SmartRefresh;
    tags[i++].ti_Data = TRUE;
    tags[i].ti_Tag = WA_MinWidth;
    tags[i++].ti_Data = 320;
    tags[i].ti_Tag = WA_MinHeight;
    tags[i++].ti_Data = 200;
    tags[i].ti_Tag = WA_MaxWidth;
    tags[i++].ti_Data = 0xffff;
    tags[i].ti_Tag = WA_MaxHeight;
    tags[i++].ti_Data = 0xffff;
    tags[i].ti_Tag = WA_IDCMP;
    tags[i++].ti_Data = IDCMP_CLOSEWINDOW | IDCMP_VANILLAKEY | IDCMP_RAWKEY |
                        IDCMP_NEWSIZE | IDCMP_REFRESHWINDOW | IDCMP_INTUITICKS |
                        IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_MENUPICK;
    /* MOUSEMOVE is only delivered with REPORTMOUSE (or a follow-mouse gadget),
       so the scrollbar knob-drag needs this. The handler ignores moves unless a
       knob is grabbed, so the extra reports cost nothing when idle. */
    tags[i].ti_Tag = WA_ReportMouse;
    tags[i++].ti_Data = TRUE;
    tags[i].ti_Tag = TAG_END;
    tags[i++].ti_Data = 0;

    /* Sample free memory now -- after the libraries are open -- so the
       footprint delta isolates the window, RastPort and pens, not the resident
       cost of opening intuition/graphics. */
    mem_before = (unsigned long)AvailMem(MEMF_ANY);

    ctx.window = OpenWindowTagList(0, tags);
    if (ctx.window == 0) {
        puts("gui window: cannot open window");
#if defined(__amigaos4__)
        DropInterface((struct Interface *)IGraphics);
        IGraphics = 0;
        CloseLibrary(GfxBase);
        GfxBase = 0;
        DropInterface((struct Interface *)IIntuition);
        IIntuition = 0;
#else
        CloseLibrary((struct Library *)GfxBase);
        GfxBase = 0;
#endif
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = 0;
        return 2;
    }

    ctx.rport = ctx.window->RPort;
    font = ctx.rport->Font;
    ctx.line_h = (font != 0 ? (int)font->tf_YSize : 8) + 2;
    cmap = ctx.window->WScreen->ViewPort.ColorMap;
    tg_gui_amiga_obtain_pens(&ctx, cmap);
    tg_gui_amiga_measure_geometry(&ctx);

    /* Right-button menu via GadTools (optional: a missing gadtools.library or a
       layout failure just leaves the window menu-less). */
    vi = 0;
    menu = 0;
#if defined(__amigaos4__)
    GadToolsBase = OpenLibrary("gadtools.library", 39L);
    if (GadToolsBase != 0) {
        IGadTools = (struct GadToolsIFace *)GetInterface(GadToolsBase, "main",
                                                         1L, 0);
        if (IGadTools == 0) {
            CloseLibrary(GadToolsBase);
            GadToolsBase = 0;
        }
    }
#else
    GadToolsBase = OpenLibrary("gadtools.library", 39L);
#endif
    if (GadToolsBase != 0) {
        vi = GetVisualInfoA(ctx.window->WScreen, 0);
        if (vi != 0) {
            menu = CreateMenusA(tg_gui_newmenu, 0);
            if (menu != 0 && LayoutMenusA(menu, vi, 0)) {
                SetMenuStrip(ctx.window, menu);
            }
        }
    }

    tg_gui_log("window: setup done");
    backend.context = &ctx;
    backend.width = tg_gui_amiga_width;
    backend.height = tg_gui_amiga_height;
    backend.line_height = tg_gui_amiga_line_height;
    backend.text_width = tg_gui_amiga_text_width;
    backend.fill_rect = tg_gui_amiga_fill_rect;
    backend.avatar_fill = tg_gui_amiga_avatar_fill;
    backend.draw_text = tg_gui_amiga_draw_text;
    backend.set_style = tg_gui_amiga_set_style;

    mem_after = (unsigned long)AvailMem(MEMF_ANY);
    footprint = (mem_before > mem_after) ? (mem_before - mem_after) : 0UL;

    /* Paint the initial content ONCE, under the layer lock. The window was opened
       INACTIVE (WA_Activate=FALSE) precisely so this first paint does not race the
       input.device/intuition task that builds the layer/ClipRect list at
       activation: a former 60x "benchmark" burst here mutated the same cliprect
       chain that the input task was building, which then walked a corrupted node
       and wrote through it inside layers3d, freezing the whole machine (DSI store
       to protected memory -- the "lists a few chats then the system freezes"
       report). OpenWindowTagList only guarantees a non-NULL Window; it does NOT
       guarantee when the layer is safe to draw, so tg_gui_window_paint() takes
       LockLayerRom() -- the one layers primitive sanctioned for Intuition windows,
       which blocks Intuition's window machinery from touching this layer while we
       render -- around every direct paint. We activate the window only AFTER this
       first locked paint settles (ActivateWindow must NOT be called while a layer
       is locked, so it goes here, after the wrapper has unlocked). Later repaints
       are IDCMP-driven and the IDCMP_REFRESHWINDOW path is bracketed by
       BeginRefresh/EndRefresh, which carries its own layer lock. */
    tg_gui_window_paint(state, &backend);
    ActivateWindow(ctx.window);
    printf("gui window: open %dx%d, font %dpx, %lu pens; window footprint "
           "~%lu KB\n",
           ctx.inner_w, ctx.inner_h, ctx.line_h,
           (unsigned long)(TG_GUI_PEN_COUNT + TG_GUI_AVATAR_COLORS),
           footprint / 1024UL);
    fflush(stdout);

    puts("gui window: close gadget or Q to quit.");
    fflush(stdout);
    tg_gui_log("window: opened");

    /* When a live session is attached (--gui-live), IDCMP_INTUITICKS (~10/s
       while the window is active) drives the network poll: throttle the actual
       tick to the per-platform watch interval so a slow link is not hammered
       (MorphOS especially), and coalesce into a single repaint per wake-up. The
       tick is a no-op when no session is open (demo/--gui-chats). */
#if defined(__MORPHOS__) || defined(__MORPHOS)
    /* ADAPTIVE RAMP on MorphOS: slow for the first WATCH_BOOT_GRACE seconds after
       the window opens, then faster steady-state. The 2026-06-20 boot freeze at a
       flat 1s was the PPC STACK OVERFLOW (Background CLI hit 32756/32756 bytes of
       the libnix default ~32KB task stack) -- now CURED by `__stack = 1MB` in
       platforms/morphos/tg_platform_morphos.c. With the stack fixed, the only
       remaining caution is the startup network burst (session open = DH + first
       connect + push-backlog drain): keep the FIRST few seconds at the proven 6s
       so that settles undisturbed, then drop to 3s steady-state -- halving the
       reception latency (the poll interval IS the latency for the open chat) while
       staying boot-safe. The per-tick getDifference is throttled to a backstop;
       pushes carry the live cross-chat stream. */
    watch_seconds = 3UL;
    watch_boot_seconds = 6UL;
    watch_boot_grace = 12UL;
#else
    watch_seconds = 2UL;
    watch_boot_seconds = 2UL;
    watch_boot_grace = 0UL;
#endif
    session_boot = time(0);
    last_session_poll = time(0);
    last_key_time = time(0);
    done = 0;
    state->composing = 0;
    state->history_count = 0;
    state->history_pos = -1;
    state->history_draft[0] = '\0';
    state->chat_scroll = 0;
    state->transcript_scroll = 0;
    state->sb_drag = 0;
    /* A login screen shows its caret from the first frame. */
    state->cursor_on = (state->mode != TG_GUI_MODE_CHAT) ? 1 : 0;
    caret_ticks = 0;
    while (!done) {
        struct IntuiMessage *msg;
        int session_dirty;
        int scroll_dirty;

        session_dirty = 0;
        scroll_dirty = 0;
        (void)Wait(1L << ctx.window->UserPort->mp_SigBit);
        while ((msg = (struct IntuiMessage *)GetMsg(ctx.window->UserPort)) !=
               0) {
            ULONG msg_class;
            UWORD msg_code;
            UWORD msg_qual;
            WORD mouse_x;
            WORD mouse_y;

            msg_class = msg->Class;
            msg_code = msg->Code;
            msg_qual = msg->Qualifier;
            mouse_x = msg->MouseX;
            mouse_y = msg->MouseY;
            ReplyMsg((struct Message *)msg);

            /* Remember the last keystroke so the live poll can defer the
               (blocking) tick while you are actively typing -- see the
               IDCMP_INTUITICKS handler below. */
            if (msg_class == IDCMP_VANILLAKEY || msg_class == IDCMP_RAWKEY) {
                last_key_time = time(0);
            }

            if (msg_class == IDCMP_CLOSEWINDOW) {
                tg_gui_log("window: close gadget");
                done = 1;
            } else if (msg_class == IDCMP_VANILLAKEY &&
                       state->mode != TG_GUI_MODE_CHAT) {
                /* A login screen owns the keyboard until the session opens. */
                tg_gui_window_login_key(state, msg_code, &backend, &done,
                                        &caret_ticks);
            } else if (msg_class == IDCMP_VANILLAKEY && state->search_active) {
                /* The sidebar search box owns the keyboard while focused: type a
                   name, ENTER runs an online search and opens the top match,
                   ESC cancels, BACKSPACE deletes. */
                if (msg_code == 27) { /* ESC */
                    state->search_active = 0;
                    state->search_query[0] = '\0';
                    tg_gui_window_copy(state->status, sizeof(state->status),
                                       "Live - F1-F10 chats, Q quits");
                    tg_gui_window_paint(state, &backend);
                } else if (msg_code == 8 || msg_code == 127) { /* BACKSPACE */
                    unsigned long n;

                    n = (unsigned long)strlen(state->search_query);
                    if (n > 0UL) {
                        state->search_query[n - 1UL] = '\0';
                        tg_gui_window_paint(state, &backend);
                    }
                } else if (msg_code == 13 || msg_code == 10) { /* ENTER: search */
                    if (state->search_query[0] != '\0') {
                        int src;

                        tg_gui_window_copy(state->status, sizeof(state->status),
                                           "Searching Telegram...");
                        tg_gui_window_paint(state, &backend);
                        src = tg_gui_session_search_open(state->search_query,
                                                         stdout);
                        state->search_active = 0;
                        if (src == 0) {
                            state->search_query[0] = '\0';
                            tg_gui_window_copy(state->status,
                                               sizeof(state->status),
                                               "Live - F1-F10 chats, Q quits");
                        } else {
                            tg_gui_window_copy(
                                state->status, sizeof(state->status),
                                "No match - try a name or @username");
                        }
                        tg_gui_window_paint(state, &backend);
                    }
                } else if (msg_code >= 32 && msg_code < 256) { /* printable */
                    unsigned long n;

                    n = (unsigned long)strlen(state->search_query);
                    if (n + 1UL < sizeof(state->search_query)) {
                        state->search_query[n] = (char)msg_code;
                        state->search_query[n + 1UL] = '\0';
                        tg_gui_window_paint(state, &backend);
                    }
                }
            } else if (msg_class == IDCMP_VANILLAKEY && state->composing) {
                /* Composing: keys edit the input line; RETURN sends, ESC
                   cancels, BACKSPACE deletes. */
                if (msg_code == 13 || msg_code == 10) {
                    if (state->input[0] != '\0') {
                        if (tg_gui_session_send(state->input, stdout) == 0) {
                            tg_gui_history_add(state, state->input);
                        }
                        state->input[0] = '\0';
                    }
                    state->input_caret = 0;
                    state->history_pos = -1;
                    state->history_draft[0] = '\0';
                    /* Keep focus in the composer so the next message can be
                       typed without re-clicking; re-prime the caret blink. */
                    state->composing = 1;
                    state->cursor_on = 1;
                    caret_ticks = 0;
                    tg_gui_window_copy(state->status, sizeof(state->status),
                                       "Type - ENTER sends, ESC cancels");
                    tg_gui_window_paint(state, &backend);
                } else if (msg_code == 27) {
                    state->input[0] = '\0';
                    state->input_caret = 0;
                    state->history_pos = -1;
                    state->composing = 0;
                    tg_gui_window_copy(state->status, sizeof(state->status),
                                       "Live - F1-F10 chats, Q quits");
                    tg_gui_window_paint(state, &backend);
                } else if (msg_code == 8 || msg_code == 127) {
                    unsigned long n;
                    unsigned long c;

                    n = (unsigned long)strlen(state->input);
                    c = (unsigned long)state->input_caret;
                    if (c > n) {
                        c = n;
                    }
                    if (c > 0UL) {
                        /* delete the char before the caret, keeping the NUL */
                        memmove(&state->input[c - 1UL], &state->input[c],
                                n - c + 1UL);
                        state->input_caret = (int)(c - 1UL);
                        tg_gui_window_paint(state, &backend);
                    }
                } else if (msg_code >= 32 && msg_code < 256) {
                    unsigned long n;
                    unsigned long c;

                    n = (unsigned long)strlen(state->input);
                    c = (unsigned long)state->input_caret;
                    if (c > n) {
                        c = n;
                    }
                    if (n + 1UL < (unsigned long)sizeof(state->input)) {
                        /* insert at the caret, shifting the tail (incl. NUL) */
                        memmove(&state->input[c + 1UL], &state->input[c],
                                n - c + 1UL);
                        state->input[c] = (char)msg_code;
                        state->input_caret = (int)(c + 1UL);
                        tg_gui_window_paint(state, &backend);
                    }
                }
            } else if (msg_class == IDCMP_VANILLAKEY) {
                if (msg_code == 'q' || msg_code == 'Q' || msg_code == 27) {
                    done = 1;
                } else if ((msg_code == 13 || msg_code == 10) &&
                           tg_gui_session_is_open()) {
                    /* RETURN starts composing a message for the open chat. */
                    state->composing = 1;
                    state->input_caret = (int)strlen(state->input);
                    state->cursor_on = 1;
                    caret_ticks = 0;
                    tg_gui_window_copy(state->status, sizeof(state->status),
                                       "Type - ENTER sends, ESC cancels");
                    tg_gui_window_paint(state, &backend);
                }
                /* Chat selection is on the function keys now (IDCMP_RAWKEY). */
            } else if (msg_class == IDCMP_RAWKEY &&
                       (msg_code == 0x7A || msg_code == 0x7B) &&
                       state->mode == TG_GUI_MODE_CHAT) {
                /* Mouse wheel (NewMouse RAWKEY 0x7A up / 0x7B down): scroll the
                   panel under the pointer. The up-transition arrives as
                   0xFA/0xFB and is ignored by the strict == tests (fires once).
                   Works while composing too (no gate). */
                int sw;
                int hx;

                sw = tg_gui_sidebar_w(ctx.inner_w);
                hx = (int)mouse_x - ctx.origin_x;
                if (hx < sw) {
                    state->chat_scroll += (msg_code == 0x7A) ? -3 : 3;
                    if (state->chat_scroll < 0) {
                        state->chat_scroll = 0;
                    }
                } else {
                    /* 0x7A reveals older history (scroll up), 0x7B newer. */
                    state->transcript_scroll += (msg_code == 0x7A) ? (3 * ctx.line_h) : (-3 * ctx.line_h);
                    if (state->transcript_scroll < 0) {
                        state->transcript_scroll = 0;
                    }
                }
                scroll_dirty = 1;
            } else if (msg_class == IDCMP_RAWKEY && state->composing &&
                       state->mode == TG_GUI_MODE_CHAT) {
                /* While composing, LEFT/RIGHT move the caret within the input.
                   The key-up event arrives as code|0x80, so the strict == tests
                   fire exactly once per press. */
                if (msg_code == 0x4F) { /* cursor left */
                    if (state->input_caret > 0) {
                        state->input_caret--;
                        tg_gui_window_paint(state, &backend);
                    }
                } else if (msg_code == 0x4E) { /* cursor right */
                    if (state->input_caret < (int)strlen(state->input)) {
                        state->input_caret++;
                        tg_gui_window_paint(state, &backend);
                    }
                } else if (msg_code == 0x4C) { /* cursor up: older sent line */
                    if (state->history_count > 0) {
                        int hidx;

                        if (state->history_pos < 0) {
                            /* entering recall: stash the live draft */
                            tg_gui_window_copy(state->history_draft,
                                               sizeof(state->history_draft),
                                               state->input);
                            hidx = state->history_count - 1;
                        } else if (state->history_pos > 0) {
                            hidx = state->history_pos - 1;
                        } else {
                            hidx = 0;
                        }
                        state->history_pos = hidx;
                        tg_gui_window_copy(state->input, sizeof(state->input),
                                           state->history[hidx]);
                        state->input_caret = (int)strlen(state->input);
                        tg_gui_window_paint(state, &backend);
                    }
                } else if (msg_code == 0x4D) { /* cursor down: newer sent line */
                    if (state->history_pos >= 0) {
                        int hidx;

                        hidx = state->history_pos + 1;
                        if (hidx >= state->history_count) {
                            /* past the newest: restore the live draft */
                            tg_gui_window_copy(state->input,
                                               sizeof(state->input),
                                               state->history_draft);
                            state->history_pos = -1;
                        } else {
                            state->history_pos = hidx;
                            tg_gui_window_copy(state->input,
                                               sizeof(state->input),
                                               state->history[hidx]);
                        }
                        state->input_caret = (int)strlen(state->input);
                        tg_gui_window_paint(state, &backend);
                    }
                }
            } else if (msg_class == IDCMP_RAWKEY && !state->composing &&
                       state->mode == TG_GUI_MODE_CHAT) {
                /* F1..F10 (rawkey 0x50..0x59) pick chats 1..10; Shift adds 10
                   for 11..20 -- matching the console's F-key selection. */
                if (msg_code >= 0x50 && msg_code <= 0x59 &&
                    state->chat_count > 0) {
                    int idx;

                    idx = (int)(msg_code - 0x50);
                    if ((msg_qual &
                         (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0) {
                        idx += 10;
                    }
                    if (idx < state->chat_count &&
                        idx != state->selected_chat) {
                        tg_gui_window_open_selection(state, idx, &backend);
                    }
                } else if (msg_code == 0x4C) { /* cursor up: older messages */
                    state->transcript_scroll += 3 * ctx.line_h;
                    scroll_dirty = 1;
                } else if (msg_code == 0x4D) { /* cursor down: newer messages */
                    state->transcript_scroll -= 3 * ctx.line_h;
                    if (state->transcript_scroll < 0) {
                        state->transcript_scroll = 0;
                    }
                    scroll_dirty = 1;
                }
            } else if (msg_class == IDCMP_MENUPICK) {
                UWORD mnum;

                mnum = msg_code;
                while (mnum != MENUNULL) {
                    struct MenuItem *item;

                    item = ItemAddress(menu, mnum);
                    if (item == 0) {
                        break;
                    }
                    {
                        APTR ud;

                        ud = GTMENUITEM_USERDATA(item);
                        if (ud == (APTR)TG_MENU_ABOUT) {
                            tg_gui_amiga_easyreq(ctx.window,
                                                 "About Telegram Amiga",
                                                 tg_gui_about_text);
                        } else if (ud == (APTR)TG_MENU_HELP) {
                            tg_gui_amiga_easyreq(ctx.window, "Telegram Amiga Help",
                                                 tg_gui_help_text);
                        } else if (ud == (APTR)TG_MENU_QUIT) {
                            done = 1;
                        }
                    }
                    mnum = item->NextSelect;
                }
            } else if (msg_class == IDCMP_NEWSIZE) {
                tg_gui_amiga_measure_geometry(&ctx);
                tg_gui_window_paint(state, &backend);
            } else if (msg_class == IDCMP_REFRESHWINDOW) {
                /* BeginRefresh() already holds this window's layer locked for the
                   whole bracket (it wraps the Layers BeginUpdate() locking
                   protocol), so paint RAW here -- tg_gui_window_paint() would take
                   a second, redundant LockLayerRom on top of it. */
                BeginRefresh(ctx.window);
                tg_gui_amiga_measure_geometry(&ctx);
                tg_gui_paint(state, &backend);
                EndRefresh(ctx.window, TRUE);
            } else if (msg_class == IDCMP_INTUITICKS) {
                if (state->mode != TG_GUI_MODE_CHAT) {
                    /* A login screen owns the keyboard: blink the caret (~2 Hz);
                       no network poll until the session opens. */
                    if (++caret_ticks >= 5) {
                        caret_ticks = 0;
                        state->cursor_on = !state->cursor_on;
                        /* Repaint ONLY the login input box, not the whole window
                           -- a full repaint twice a second was a visible refresh
                           on OS3. */
                        tg_gui_window_paint_caret(state, &backend);
                    }
                } else {
                    time_t now;

                    /* Blink the composer caret (~2 Hz) while typing. */
                    if ((state->composing || state->search_active) &&
                        ++caret_ticks >= 5) {
                        caret_ticks = 0;
                        state->cursor_on = !state->cursor_on;
                        /* Repaint ONLY the focused input strip (composer row or
                           the sidebar search box), not the whole window -- a full
                           repaint twice a second flickered on slow OS3 displays. */
                        tg_gui_window_paint_caret(state, &backend);
                    }

                    /* Live poll on the watch interval -- now runs even while the
                       composer is focused (keep-focus-after-send leaves
                       composing=1, and pausing the poll there silently stopped
                       reception + the "is typing" header). To keep typing smooth,
                       defer the (blocking) tick until the composer has been idle
                       for TG_GUI_COMPOSE_IDLE_POLL_SECONDS; skip it entirely when
                       a close/quit is already queued this drain. */
                    now = time(0);
                    /* Effective interval: hold the conservative boot cadence until
                       the startup network burst has had WATCH_BOOT_GRACE seconds to
                       settle, then use the faster steady-state interval. */
                    {
                        unsigned long eff = watch_seconds;
                        if (watch_boot_grace &&
                            session_boot != (time_t)-1 && now != (time_t)-1 &&
                            (unsigned long)(now - session_boot) < watch_boot_grace) {
                            eff = watch_boot_seconds;
                        }
                        effective_watch = eff;
                    }
                    if (!done && now != (time_t)-1 &&
                        (unsigned long)(now - last_session_poll) >=
                            effective_watch &&
                        (!state->composing ||
                         (unsigned long)(now - last_key_time) >=
                             TG_GUI_COMPOSE_IDLE_POLL_SECONDS)) {
                        last_session_poll = now;
                        if (tg_gui_session_tick(stdout)) {
                            session_dirty = 1;
                        }
                    }
                }
            } else if (msg_class == IDCMP_MOUSEBUTTONS &&
                       state->mode == TG_GUI_MODE_CHAT) {
                int hx;
                int hy;

                hx = (int)mouse_x - ctx.origin_x;
                hy = (int)mouse_y - ctx.origin_y;
                if (msg_code == SELECTUP) {
                    state->sb_drag = 0;
                } else if (msg_code == SELECTDOWN && state->sb_tr_max > 0 &&
                           hx >= state->sb_tr_x &&
                           hx < state->sb_tr_x + TG_GUI_SCROLLBAR_W &&
                           hy >= state->sb_tr_ty &&
                           hy < state->sb_tr_ty + state->sb_tr_th) {
                    if (hy >= state->sb_tr_ky &&
                        hy < state->sb_tr_ky + state->sb_tr_kh) {
                        state->sb_drag = 2;
                        state->sb_grab_dy = hy - state->sb_tr_ky;
                    } else {
                        int page = state->sb_tr_th;

                        if (page < 1) {
                            page = 1;
                        }
                        if (hy < state->sb_tr_ky) {
                            state->transcript_scroll += page;
                        } else {
                            state->transcript_scroll -= page;
                            if (state->transcript_scroll < 0) {
                                state->transcript_scroll = 0;
                            }
                        }
                        scroll_dirty = 1;
                    }
                } else if (msg_code == SELECTDOWN && state->sb_list_max > 0 &&
                           hx >= state->sb_list_x &&
                           hx < state->sb_list_x + TG_GUI_SCROLLBAR_W &&
                           hy >= state->sb_list_ty &&
                           hy < state->sb_list_ty + state->sb_list_th) {
                    if (hy >= state->sb_list_ky &&
                        hy < state->sb_list_ky + state->sb_list_kh) {
                        state->sb_drag = 1;
                        state->sb_grab_dy = hy - state->sb_list_ky;
                    } else {
                        int page = state->chat_count - state->sb_list_max;

                        if (page < 1) {
                            page = 1;
                        }
                        if (hy < state->sb_list_ky) {
                            state->chat_scroll -= page;
                            if (state->chat_scroll < 0) {
                                state->chat_scroll = 0;
                            }
                        } else {
                            state->chat_scroll += page;
                        }
                        scroll_dirty = 1;
                    }
                } else if (msg_code == SELECTDOWN) {
                    int hit;

                    hit = tg_gui_hit_test(state, ctx.inner_w, ctx.inner_h,
                                          ctx.line_h, hx, hy);
                    if (hit >= 0) {
                        /* Click a chat row: select + open it (drop any draft). */
                        state->search_active = 0; /* leaving the search box */
                        if (state->composing) {
                            state->composing = 0;
                            state->input[0] = '\0';
                            state->input_caret = 0;
                            state->history_pos = -1;
                            tg_gui_window_copy(state->status, sizeof(state->status),
                                       "Live - F1-F10 chats, Q quits");
                        }
                        if (hit != state->selected_chat) {
                            tg_gui_window_open_selection(state, hit, &backend);
                        } else {
                            tg_gui_window_paint(state, &backend);
                        }
                    } else if (hit == TG_GUI_HIT_SEARCH &&
                               tg_gui_session_is_open()) {
                        /* Click the sidebar search box to focus it for typing. */
                        state->composing = 0;
                        state->search_active = 1;
                        state->cursor_on = 1;
                        caret_ticks = 0;
                        tg_gui_window_copy(
                            state->status, sizeof(state->status),
                            "Search: type a name, ENTER to find, ESC cancels");
                        tg_gui_window_paint(state, &backend);
                    } else if (hit == TG_GUI_HIT_SEND && state->composing) {
                        if (state->input[0] != '\0') {
                            if (tg_gui_session_send(state->input, stdout) == 0) {
                                tg_gui_history_add(state, state->input);
                            }
                            state->input[0] = '\0';
                        }
                        state->input_caret = 0;
                        state->history_pos = -1;
                        state->history_draft[0] = '\0';
                        /* Keep focus in the composer so the next message can be
                           typed without re-clicking; re-prime the caret blink. */
                        state->composing = 1;
                        state->cursor_on = 1;
                        caret_ticks = 0;
                        tg_gui_window_copy(state->status, sizeof(state->status),
                                       "Type - ENTER sends, ESC cancels");
                        tg_gui_window_paint(state, &backend);
                    } else if ((hit == TG_GUI_HIT_INPUT ||
                                hit == TG_GUI_HIT_SEND) &&
                               !state->composing && tg_gui_session_is_open()) {
                        /* Click the input field (or Send) to start composing --
                           leave the search box so only one caret is focused. */
                        state->search_active = 0;
                        state->search_query[0] = '\0';
                        state->composing = 1;
                        state->input_caret = (int)strlen(state->input);
                        state->cursor_on = 1;
                        caret_ticks = 0;
                        tg_gui_window_copy(state->status, sizeof(state->status),
                               "Type - ENTER sends, ESC cancels");
                        tg_gui_window_paint(state, &backend);
                    }
                }
            } else if (msg_class == IDCMP_MOUSEMOVE) {
                /* Intuition reports moves while a button is held -- so this only
                   fires during a knob drag. Map the cursor to a scroll offset;
                   the painter re-clamps and redraws the knob to match. */
                if (state->sb_drag != 0) {
                    int hy;
                    int nky;
                    int span;

                    hy = (int)mouse_y - ctx.origin_y;
                    if (state->sb_drag == 2) {
                        span = state->sb_tr_th - state->sb_tr_kh;
                        nky = hy - state->sb_grab_dy;
                        if (nky < state->sb_tr_ty) {
                            nky = state->sb_tr_ty;
                        }
                        if (nky > state->sb_tr_ty + span) {
                            nky = state->sb_tr_ty + span;
                        }
                        if (span > 0) {
                            int off_top;

                            off_top = (nky - state->sb_tr_ty) *
                                      state->sb_tr_max / span;
                            state->transcript_scroll =
                                state->sb_tr_max - off_top;
                            if (state->transcript_scroll < 0) {
                                state->transcript_scroll = 0;
                            }
                        }
                    } else {
                        span = state->sb_list_th - state->sb_list_kh;
                        nky = hy - state->sb_grab_dy;
                        if (nky < state->sb_list_ty) {
                            nky = state->sb_list_ty;
                        }
                        if (nky > state->sb_list_ty + span) {
                            nky = state->sb_list_ty + span;
                        }
                        if (span > 0) {
                            state->chat_scroll = (nky - state->sb_list_ty) *
                                                 state->sb_list_max / span;
                        }
                    }
                    scroll_dirty = 1;
                }
            }
        }
        if (session_dirty || scroll_dirty) {
            tg_gui_window_paint(state, &backend);
        }
    }

    /* Detach + free the menu strip before the window goes away. */
    if (menu != 0) {
        ClearMenuStrip(ctx.window);
        FreeMenus(menu);
        menu = 0;
    }
    if (vi != 0) {
        FreeVisualInfo(vi);
        vi = 0;
    }
#if defined(__amigaos4__)
    if (IGadTools != 0) {
        DropInterface((struct Interface *)IGadTools);
        IGadTools = 0;
    }
#endif
    if (GadToolsBase != 0) {
        CloseLibrary(GadToolsBase);
        GadToolsBase = 0;
    }
    tg_gui_log("window: releasing pens");
    tg_gui_amiga_release_pens(&ctx, cmap);
    tg_gui_window_save_size(ctx.inner_w, ctx.inner_h);
    tg_gui_log("window: CloseWindow");
    CloseWindow(ctx.window);
    ctx.window = 0;
#if defined(__amigaos4__)
    DropInterface((struct Interface *)IGraphics);
    IGraphics = 0;
    CloseLibrary(GfxBase);
    GfxBase = 0;
    DropInterface((struct Interface *)IIntuition);
    IIntuition = 0;
#else
    CloseLibrary((struct Library *)GfxBase);
    GfxBase = 0;
#endif
    CloseLibrary((struct Library *)IntuitionBase);
    IntuitionBase = 0;
    tg_gui_log("window: libraries closed");
    return 0;
}

#else /* !TG_GUI_AMIGA: host build */

int tg_gui_run_window(tg_gui_state *state)
{
    (void)state;
    puts("gui window: native window not available on this build; "
         "use --gui-self-test for the layout check.");
    return 0;
}

#endif
