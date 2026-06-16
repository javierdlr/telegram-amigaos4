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

static unsigned long tg_gui_amiga_ticks(void)
{
    struct DateStamp now;

    DateStamp(&now);
    return (unsigned long)now.ds_Days * 24UL * 60UL * 50UL +
           (unsigned long)now.ds_Minute * 60UL * 50UL +
           (unsigned long)now.ds_Tick;
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

/* Switch to chat `sel`: show its header + an empty transcript at once, then
   fetch the history. The instant first paint keeps the switch responsive on a
   slow link instead of the window appearing frozen on the old chat until the
   load finishes. */
static void tg_gui_window_open_selection(tg_gui_state *state, int sel,
                                         tg_gui_backend *backend)
{
    state->selected_chat = sel;
    tg_gui_window_apply_selection(state);
    if (tg_gui_session_is_open()) {
        state->message_count = 0;
        tg_gui_paint(state, backend);
        (void)tg_gui_session_open_chat(state->chats[sel].index, stdout);
    }
    tg_gui_paint(state, backend);
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
            tg_gui_paint(state, backend);
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
                tg_gui_paint(state, backend);
            }
        }
        return;
    }

    /* RETURN: submit the current field. Show progress first -- the DH/RPC round
       trip blocks the window for several seconds on a slow link. */
    tg_gui_window_copy(state->status, sizeof(state->status),
                       "Connecting to Telegram...");
    state->cursor_on = 0;
    tg_gui_paint(state, backend);

    if (state->mode == TG_GUI_MODE_LOGIN_PHONE) {
        int rc;

        rc = tg_gui_session_login_send_code(state->input, stdout);
        state->input[0] = '\0';
        if (rc == TG_GUI_LOGIN_OK) {
            state->mode = TG_GUI_MODE_LOGIN_CODE;
            tg_gui_window_copy(state->status, sizeof(state->status),
                               "Enter the code you received");
        } else {
            tg_gui_window_copy(state->status, sizeof(state->status),
                               "Invalid number - try again");
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
            tg_gui_window_copy(state->status, sizeof(state->status),
                               "Error - try the code again");
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
    tg_gui_paint(state, backend);
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
    "MorphOS and AROS -- no MUI, no ixemul.\n\n"
    "by Michele Dipace";

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

int tg_gui_run_window(tg_gui_state *state)
{
    tg_gui_amiga_ctx ctx;
    tg_gui_backend backend;
    struct TagItem tags[18];
    struct ColorMap *cmap;
    struct TextFont *font;
    APTR vi;
    struct Menu *menu;
    unsigned long mem_before;
    unsigned long mem_after;
    unsigned long footprint;
    unsigned long t0;
    unsigned long t1;
    int repaints;
    int i;
    int done;
    int caret_ticks;
    unsigned long watch_seconds;
    time_t last_session_poll;

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
    i = 0;
    tags[i].ti_Tag = WA_Title;
    tags[i++].ti_Data = TG_GUI_TAG("Telegram Amiga - GUI test");
    tags[i].ti_Tag = WA_InnerWidth;
    tags[i++].ti_Data = 600;
    tags[i].ti_Tag = WA_InnerHeight;
    tags[i++].ti_Data = 380;
    tags[i].ti_Tag = WA_DragBar;
    tags[i++].ti_Data = TRUE;
    tags[i].ti_Tag = WA_DepthGadget;
    tags[i++].ti_Data = TRUE;
    tags[i].ti_Tag = WA_CloseGadget;
    tags[i++].ti_Data = TRUE;
    tags[i].ti_Tag = WA_SizeGadget;
    tags[i++].ti_Data = TRUE;
    tags[i].ti_Tag = WA_Activate;
    tags[i++].ti_Data = TRUE;
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
                        IDCMP_MOUSEBUTTONS | IDCMP_MENUPICK;
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

    tg_gui_paint(state, &backend);

    /* Milestone 0 measurement: time a batch of full repaints so a slow 68k
       reports a real number. The first paint above cleared the background;
       these redraw the same opaque content in place with the clear suppressed,
       so the window does not flash during the timing burst. */
    repaints = 60;
    tg_gui_set_background_clear(0);
    t0 = tg_gui_amiga_ticks();
    for (i = 0; i < repaints; ++i) {
        tg_gui_paint(state, &backend);
    }
    t1 = tg_gui_amiga_ticks();
    tg_gui_set_background_clear(1);
    {
        unsigned long ticks;
        unsigned long ms_total;

        ticks = (t1 >= t0) ? (t1 - t0) : 0UL;
        ms_total = ticks * 20UL; /* DateStamp tick = 1/50 s = 20 ms */
        printf("gui window: open %dx%d, font %dpx, %lu pens; "
               "%d full repaints in %lu ms (%lu ms/paint); window footprint "
               "~%lu KB\n",
               ctx.inner_w, ctx.inner_h, ctx.line_h,
               (unsigned long)(TG_GUI_PEN_COUNT + TG_GUI_AVATAR_COLORS),
               repaints, ms_total,
               repaints > 0 ? ms_total / (unsigned long)repaints : 0UL,
               footprint / 1024UL);
        fflush(stdout);
    }
    tg_gui_paint(state, &backend);

    puts("gui window: close gadget or Q to quit.");
    fflush(stdout);
    tg_gui_log("window: opened");

    /* When a live session is attached (--gui-live), IDCMP_INTUITICKS (~10/s
       while the window is active) drives the network poll: throttle the actual
       tick to the per-platform watch interval so a slow link is not hammered
       (MorphOS especially), and coalesce into a single repaint per wake-up. The
       tick is a no-op when no session is open (demo/--gui-chats). */
#if defined(__MORPHOS__) || defined(__MORPHOS)
    watch_seconds = 12UL;
#else
    watch_seconds = 2UL;
#endif
    last_session_poll = time(0);
    done = 0;
    state->composing = 0;
    /* A login screen shows its caret from the first frame. */
    state->cursor_on = (state->mode != TG_GUI_MODE_CHAT) ? 1 : 0;
    caret_ticks = 0;
    while (!done) {
        struct IntuiMessage *msg;
        int session_dirty;

        session_dirty = 0;
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

            if (msg_class == IDCMP_CLOSEWINDOW) {
                tg_gui_log("window: close gadget");
                done = 1;
            } else if (msg_class == IDCMP_VANILLAKEY &&
                       state->mode != TG_GUI_MODE_CHAT) {
                /* A login screen owns the keyboard until the session opens. */
                tg_gui_window_login_key(state, msg_code, &backend, &done,
                                        &caret_ticks);
            } else if (msg_class == IDCMP_VANILLAKEY && state->composing) {
                /* Composing: keys edit the input line; RETURN sends, ESC
                   cancels, BACKSPACE deletes. */
                if (msg_code == 13 || msg_code == 10) {
                    if (state->input[0] != '\0') {
                        (void)tg_gui_session_send(state->input, stdout);
                        state->input[0] = '\0';
                    }
                    state->composing = 0;
                    tg_gui_window_copy(state->status, sizeof(state->status),
                                       "Live - F1-F10 chats, Q quits");
                    tg_gui_paint(state, &backend);
                } else if (msg_code == 27) {
                    state->input[0] = '\0';
                    state->composing = 0;
                    tg_gui_window_copy(state->status, sizeof(state->status),
                                       "Live - F1-F10 chats, Q quits");
                    tg_gui_paint(state, &backend);
                } else if (msg_code == 8 || msg_code == 127) {
                    unsigned long n;

                    n = (unsigned long)strlen(state->input);
                    if (n > 0UL) {
                        state->input[n - 1UL] = '\0';
                        tg_gui_paint(state, &backend);
                    }
                } else if (msg_code >= 32 && msg_code < 256) {
                    unsigned long n;

                    n = (unsigned long)strlen(state->input);
                    if (n + 1UL < (unsigned long)sizeof(state->input)) {
                        state->input[n] = (char)msg_code;
                        state->input[n + 1UL] = '\0';
                        tg_gui_paint(state, &backend);
                    }
                }
            } else if (msg_class == IDCMP_VANILLAKEY) {
                if (msg_code == 'q' || msg_code == 'Q' || msg_code == 27) {
                    done = 1;
                } else if ((msg_code == 13 || msg_code == 10) &&
                           tg_gui_session_is_open()) {
                    /* RETURN starts composing a message for the open chat. */
                    state->composing = 1;
                    state->cursor_on = 1;
                    caret_ticks = 0;
                    tg_gui_window_copy(state->status, sizeof(state->status),
                                       "Type - ENTER sends, ESC cancels");
                    tg_gui_paint(state, &backend);
                }
                /* Chat selection is on the function keys now (IDCMP_RAWKEY). */
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
                tg_gui_paint(state, &backend);
            } else if (msg_class == IDCMP_REFRESHWINDOW) {
                BeginRefresh(ctx.window);
                tg_gui_amiga_measure_geometry(&ctx);
                tg_gui_paint(state, &backend);
                EndRefresh(ctx.window, TRUE);
            } else if (msg_class == IDCMP_INTUITICKS) {
                if (state->composing || state->mode != TG_GUI_MODE_CHAT) {
                    /* Blink the input/login caret (~2 Hz); the network poll is
                       paused while composing or logging in. */
                    if (++caret_ticks >= 5) {
                        caret_ticks = 0;
                        state->cursor_on = !state->cursor_on;
                        /* Opaque in-place repaint (no full-window clear) so the
                           blink does not flash the background; the input field
                           fill still erases the previous caret. */
                        tg_gui_set_background_clear(0);
                        tg_gui_paint(state, &backend);
                        tg_gui_set_background_clear(1);
                    }
                } else {
                    time_t now;

                    now = time(0);
                    if (now != (time_t)-1 &&
                        (unsigned long)(now - last_session_poll) >=
                            watch_seconds) {
                        last_session_poll = now;
                        if (tg_gui_session_tick(stdout)) {
                            session_dirty = 1;
                        }
                    }
                }
            } else if (msg_class == IDCMP_MOUSEBUTTONS &&
                       state->mode == TG_GUI_MODE_CHAT) {
                if (msg_code == SELECTDOWN) {
                    int hx;
                    int hy;
                    int hit;

                    hx = (int)mouse_x - ctx.origin_x;
                    hy = (int)mouse_y - ctx.origin_y;
                    hit = tg_gui_hit_test(state, ctx.inner_w, ctx.inner_h,
                                          ctx.line_h, hx, hy);
                    if (hit >= 0) {
                        /* Click a chat row: select + open it (drop any draft). */
                        if (state->composing) {
                            state->composing = 0;
                            state->input[0] = '\0';
                            tg_gui_window_copy(state->status, sizeof(state->status),
                                       "Live - F1-F10 chats, Q quits");
                        }
                        if (hit != state->selected_chat) {
                            tg_gui_window_open_selection(state, hit, &backend);
                        } else {
                            tg_gui_paint(state, &backend);
                        }
                    } else if (hit == TG_GUI_HIT_SEND && state->composing) {
                        if (state->input[0] != '\0') {
                            (void)tg_gui_session_send(state->input, stdout);
                            state->input[0] = '\0';
                        }
                        state->composing = 0;
                        tg_gui_window_copy(state->status, sizeof(state->status),
                                       "Live - F1-F10 chats, Q quits");
                        tg_gui_paint(state, &backend);
                    } else if ((hit == TG_GUI_HIT_INPUT ||
                                hit == TG_GUI_HIT_SEND) &&
                               !state->composing && tg_gui_session_is_open()) {
                        /* Click the input field (or Send) to start composing. */
                        state->composing = 1;
                        state->cursor_on = 1;
                        caret_ticks = 0;
                        tg_gui_window_copy(state->status, sizeof(state->status),
                               "Type - ENTER sends, ESC cancels");
                        tg_gui_paint(state, &backend);
                    }
                }
            }
        }
        if (session_dirty) {
            tg_gui_paint(state, &backend);
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
