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

#include <stdio.h>
#include <string.h>

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
#include <graphics/gfx.h>
#include <graphics/text.h>
#include <graphics/view.h>
#include <utility/tagitem.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
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
#else
struct GfxBase *GfxBase = 0;
#endif

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

int tg_gui_run_window(const tg_gui_state *state)
{
    tg_gui_amiga_ctx ctx;
    tg_gui_backend backend;
    struct TagItem tags[18];
    struct ColorMap *cmap;
    struct TextFont *font;
    unsigned long mem_before;
    unsigned long mem_after;
    unsigned long footprint;
    unsigned long t0;
    unsigned long t1;
    int repaints;
    int i;
    int done;

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
    tags[i++].ti_Data = IDCMP_CLOSEWINDOW | IDCMP_VANILLAKEY | IDCMP_NEWSIZE |
                        IDCMP_REFRESHWINDOW;
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

    backend.context = &ctx;
    backend.width = tg_gui_amiga_width;
    backend.height = tg_gui_amiga_height;
    backend.line_height = tg_gui_amiga_line_height;
    backend.text_width = tg_gui_amiga_text_width;
    backend.fill_rect = tg_gui_amiga_fill_rect;
    backend.avatar_fill = tg_gui_amiga_avatar_fill;
    backend.draw_text = tg_gui_amiga_draw_text;

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

    done = 0;
    while (!done) {
        struct IntuiMessage *msg;

        (void)Wait(1L << ctx.window->UserPort->mp_SigBit);
        while ((msg = (struct IntuiMessage *)GetMsg(ctx.window->UserPort)) !=
               0) {
            ULONG msg_class;
            UWORD msg_code;

            msg_class = msg->Class;
            msg_code = msg->Code;
            ReplyMsg((struct Message *)msg);

            if (msg_class == IDCMP_CLOSEWINDOW) {
                done = 1;
            } else if (msg_class == IDCMP_VANILLAKEY) {
                if (msg_code == 'q' || msg_code == 'Q' || msg_code == 27) {
                    done = 1;
                }
            } else if (msg_class == IDCMP_NEWSIZE) {
                tg_gui_amiga_measure_geometry(&ctx);
                tg_gui_paint(state, &backend);
            } else if (msg_class == IDCMP_REFRESHWINDOW) {
                BeginRefresh(ctx.window);
                tg_gui_amiga_measure_geometry(&ctx);
                tg_gui_paint(state, &backend);
                EndRefresh(ctx.window, TRUE);
            }
        }
    }

    tg_gui_amiga_release_pens(&ctx, cmap);
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
    return 0;
}

#else /* !TG_GUI_AMIGA: host build */

int tg_gui_run_window(const tg_gui_state *state)
{
    (void)state;
    puts("gui window: native window not available on this build; "
         "use --gui-self-test for the layout check.");
    return 0;
}

#endif
