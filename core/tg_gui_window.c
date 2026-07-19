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
#include "tg_avatar.h"
#include "tg_mtproto_login.h"
#include "tg_platform.h"
#include "tg_version.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
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
/* While keys are flowing, drain at most one already-queued MTProto frame each
   second. This path sends no RPC and therefore cannot leave a reply pending. */
#define TG_GUI_COMPOSE_RECEIVE_SECONDS 1UL

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
#include <devices/timer.h>
#include <devices/clipboard.h>
#include <libraries/asl.h>
#include <workbench/workbench.h>
#include <workbench/startup.h>
#include <proto/wb.h>
#include <proto/icon.h>
#include <proto/asl.h>

/* asl.library is opened lazily around the file requester (Send file...) so
   startup stays untouched and a system without it just skips the feature. */
struct Library *AslBase = 0;
/* workbench.library + icon.library open lazily around the iconified wait. */
struct Library *WorkbenchBase = 0;
struct Library *IconBase = 0;
#if defined(__amigaos4__)
struct WorkbenchIFace *IWorkbench = 0;
struct IconIFace *IIcon = 0;
#endif
#if defined(__amigaos4__)
struct AslIFace *IAsl = 0;
#endif

/* timer.device request, papering over the OS4 SDK rename (TimeRequest with
   Request/Time.Seconds vs the classic timerequest with tr_node/tr_time). */
#if defined(__amigaos4__)
typedef struct TimeRequest tg_gui_timereq;
#define TG_GUI_TR_NODE(t) ((t)->Request)
#define TG_GUI_TR_SECS(t) ((t)->Time.Seconds)
#define TG_GUI_TR_MICRO(t) ((t)->Time.Microseconds)
#else
typedef struct timerequest tg_gui_timereq;
#define TG_GUI_TR_NODE(t) ((t)->tr_node)
#define TG_GUI_TR_SECS(t) ((t)->tr_time.tv_secs)
#define TG_GUI_TR_MICRO(t) ((t)->tr_time.tv_micro)
#endif
/* Heartbeat period: half the fastest watch cadence so a poll window is never
   missed by more than one beat. */
#define TG_GUI_HEARTBEAT_SECS 2UL
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
#define TG_MENU_ABOUT  1
#define TG_MENU_HELP   2
#define TG_MENU_QUIT   3
#define TG_MENU_REMOVE 4
#define TG_MENU_SENDFILE 5
#define TG_MENU_ICONIFY 6
#define TG_MENU_OWNSCREEN 7
#define TG_MENU_COPY 8
#define TG_MENU_PASTE 9
#define TG_MENU_CUT 10

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
    {0xe6, 0xf1, 0xfb}, /* BADGE_TEXT */
    {0x4d, 0xc2, 0xff}  /* READ - read-receipt double check, pops on the blue bubble */
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
    /* Off-screen double-buffer (flicker-free paint). buf_ok==0 => direct render. */
    struct BitMap *buf_bm;   /* friend of window->RPort->BitMap; 0 if none */
    struct RastPort buf_rp;  /* layerless RastPort over buf_bm */
    int buf_w;               /* allocated buffer width  (== inner_w when valid) */
    int buf_h;               /* allocated buffer height (== inner_h when valid) */
    int buf_ok;              /* 1 iff buf_bm and buf_rp.Font are valid */
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

/* --- real avatars (decoded stripped thumbs) ------------------------------
   Each avatar is decoded ONCE to a TG_GUI_AV_SZ^2 grid of RESOLVED PENS
   (1 byte/pixel) and cached by peer id; repaints replay the pen grid with
   run-length RectFills into the current (buffered) RastPort. Pens come from
   a small pool shared across ALL avatars (ObtainBestPenA, nearest-match once
   full, released with the other pens at teardown). Any failure returns 0 and
   the renderer falls back to the classic initials square. */
static ULONG tg_gui_amiga_rgb32(unsigned char component);

#define TG_GUI_AV_SZ 32
/* Must comfortably exceed the visible sidebar rows: negative slots live here
   too, and an eviction churn would bring back the per-repaint disk probing
   this cache exists to kill. ~1KB per slot. */
#if defined(__m68k__)
#define TG_GUI_AV_SLOTS 24
#else
#define TG_GUI_AV_SLOTS 32
#endif
/* Pen budget for the shared avatar pool, chosen at RUNTIME from the screen
   depth when the pool is armed at window open: paletted screens (<= 8 bit)
   get the lean profile (pens are scarce and shared with Workbench), truecolor
   RTG screens the rich one. This used to be a compile-time m68k gate, which
   kept avatars needlessly dull on truecolor RTG under OS3 (e.g. a Vampire):
   the machine is m68k but its screen affords the fine profile. Arrays are
   sized for the rich cap; the lean profile just uses less of them. */
#define TG_GUI_AV_POOL_MAX 96

typedef struct tg_gui_av_slot {
    unsigned long id_hi;
    unsigned long id_lo;
    unsigned long gen; /* store generation the slot was built at (retry gate) */
    int state; /* 0 free, 1 pens ready, -1 nothing/undecodable (initials) */
    unsigned char pen[TG_GUI_AV_SZ * TG_GUI_AV_SZ];
} tg_gui_av_slot;
static tg_gui_av_slot tg_gui_av_slots[TG_GUI_AV_SLOTS];
static unsigned long tg_gui_av_evict = 0UL;
static struct ColorMap *tg_gui_av_cmap = 0;
static LONG tg_gui_av_pool_pen[TG_GUI_AV_POOL_MAX];
static unsigned char tg_gui_av_pool_rgb[TG_GUI_AV_POOL_MAX][3];
static int tg_gui_av_pool_n = 0;
/* Runtime profile (set where the cmap is armed; lean defaults are the safe
   fallback if the depth probe ever fails). */
static int tg_gui_av_pool_cap = 48;   /* 48 paletted / 96 truecolor */
static long tg_gui_av_share_d = 192L; /* 192 paletted / 48 truecolor */
static int tg_gui_av_rich = 0;        /* seed: cube+greys vs greys only */

static void tg_gui_av_reset(void)
{
    int i;

    for (i = 0; i < TG_GUI_AV_SLOTS; ++i) {
        tg_gui_av_slots[i].state = 0;
    }
    tg_gui_av_pool_n = 0;
    tg_gui_av_evict = 0UL;
}

static void tg_gui_av_release_pool(struct ColorMap *cmap)
{
    int i;

    for (i = 0; i < tg_gui_av_pool_n; ++i) {
        ReleasePen(cmap, tg_gui_av_pool_pen[i]);
    }
    tg_gui_av_pool_n = 0;
    tg_gui_av_cmap = 0;
    tg_gui_av_reset();
}

/* Drops the cached pen grid for one peer so the next paint rebuilds it from
   the freshest source (called after a successful avatar download). */
void tg_gui_window_avatar_invalidate(unsigned long id_hi, unsigned long id_lo)
{
    int i;

    for (i = 0; i < TG_GUI_AV_SLOTS; ++i) {
        if (tg_gui_av_slots[i].state != 0 &&
            tg_gui_av_slots[i].id_hi == id_hi &&
            tg_gui_av_slots[i].id_lo == id_lo) {
            tg_gui_av_slots[i].state = 0;
        }
    }
}

/* Obtain one pool pen for an exact RGB (used by the seeder; the miss path in
   pen_for keeps its own inline copy because it needs the pen value back).
   Skips silently when the pool is full or the obtain fails. */
static void tg_gui_av_pool_add(unsigned char r, unsigned char g,
                               unsigned char b)
{
    LONG p;

    if (tg_gui_av_pool_n >= tg_gui_av_pool_cap || tg_gui_av_cmap == 0) {
        return;
    }
    p = ObtainBestPenA(tg_gui_av_cmap, tg_gui_amiga_rgb32(r),
                       tg_gui_amiga_rgb32(g), tg_gui_amiga_rgb32(b), 0);
    if (p != -1) {
        tg_gui_av_pool_pen[tg_gui_av_pool_n] = p;
        tg_gui_av_pool_rgb[tg_gui_av_pool_n][0] = r;
        tg_gui_av_pool_rgb[tg_gui_av_pool_n][1] = g;
        tg_gui_av_pool_rgb[tg_gui_av_pool_n][2] = b;
        ++tg_gui_av_pool_n;
    }
}

/* Pre-seed the shared pool with a neutral colour lattice. Without this the
   first 2-3 avatars filled the pool with THEIR shades and, once full, every
   later colour snapped to the nearest of those with no bound: whites picked
   up a pink or blue cast depending on which avatars happened to paint first
   (tester reports: pinkish whites on MorphOS, blueish on OS4 -- same code,
   different chat lists). A fixed 4x4x4 RGB cube plus a grey ramp bounds the
   full-pool fallback error and keeps neutrals neutral; the remaining slots
   stay dynamic for frequent exact colours. Paletted screens (lean profile,
   any CPU) seed greys only: pens are scarce there and the coarse share step
   already merges shades. */
static void tg_gui_av_seed_pool(void)
{
    static const unsigned char lv[4] = { 0U, 85U, 170U, 255U };
    int r;
    int g;
    int b;
    int i;

    if (tg_gui_av_rich) { /* truecolor: full cube + grey ramp (76 of 96) */
        for (r = 0; r < 4; ++r) {
            for (g = 0; g < 4; ++g) {
                for (b = 0; b < 4; ++b) {
                    tg_gui_av_pool_add(lv[r], lv[g], lv[b]);
                }
            }
        }
        for (i = 17; i < 255; i += 17) {
            if ((i % 85) != 0) { /* skip the greys already in the cube */
                tg_gui_av_pool_add((unsigned char)i, (unsigned char)i,
                                   (unsigned char)i);
            }
        }
    } else { /* paletted: 6 greys keep whites neutral on a lean budget */
        for (i = 0; i <= 255; i += 51) {
            tg_gui_av_pool_add((unsigned char)i, (unsigned char)i,
                               (unsigned char)i);
        }
    }
}

/* Pool pen for an RGB pixel: reuse a close pool entry, else obtain a new one
   (PRECISION-default like the theme pens), else nearest of what we have. */
static LONG tg_gui_av_pen_for(const unsigned char *rgb)
{
    int i;
    int best = -1;
    long best_d = 0x7fffffffL;

    if (tg_gui_av_pool_n == 0) {
        tg_gui_av_seed_pool(); /* once per screen (reset drops the pool) */
    }
    for (i = 0; i < tg_gui_av_pool_n; ++i) {
        long dr = (long)tg_gui_av_pool_rgb[i][0] - (long)rgb[0];
        long dg = (long)tg_gui_av_pool_rgb[i][1] - (long)rgb[1];
        long db = (long)tg_gui_av_pool_rgb[i][2] - (long)rgb[2];
        long d = dr * dr + dg * dg + db * db;

        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    if (best >= 0 && best_d <= tg_gui_av_share_d) {
        return tg_gui_av_pool_pen[best]; /* close enough: share */
    }
    if (tg_gui_av_pool_n < tg_gui_av_pool_cap && tg_gui_av_cmap != 0) {
        LONG p = ObtainBestPenA(tg_gui_av_cmap, tg_gui_amiga_rgb32(rgb[0]),
                                tg_gui_amiga_rgb32(rgb[1]),
                                tg_gui_amiga_rgb32(rgb[2]), 0);

        if (p != -1) {
            tg_gui_av_pool_pen[tg_gui_av_pool_n] = p;
            tg_gui_av_pool_rgb[tg_gui_av_pool_n][0] = rgb[0];
            tg_gui_av_pool_rgb[tg_gui_av_pool_n][1] = rgb[1];
            tg_gui_av_pool_rgb[tg_gui_av_pool_n][2] = rgb[2];
            ++tg_gui_av_pool_n;
            return p;
        }
    }
    return (best >= 0) ? tg_gui_av_pool_pen[best] : -1;
}

static int tg_gui_amiga_avatar_image(tg_gui_backend *backend,
                                     unsigned long id_hi, unsigned long id_lo,
                                     tg_gui_rect rect)
{
    tg_gui_amiga_ctx *ctx;
    tg_gui_av_slot *slot;
    int i;
    int y;

    ctx = (tg_gui_amiga_ctx *)backend->context;
    if (rect.w <= 0 || rect.h <= 0 || (id_hi == 0UL && id_lo == 0UL) ||
        tg_gui_av_cmap == 0) {
        return 0;
    }
    slot = 0;
    for (i = 0; i < TG_GUI_AV_SLOTS; ++i) {
        if (tg_gui_av_slots[i].state != 0 &&
            tg_gui_av_slots[i].id_hi == id_hi &&
            tg_gui_av_slots[i].id_lo == id_lo) {
            slot = &tg_gui_av_slots[i];
            /* A negative slot only retries when NEW thumbs arrived since it
               was cached; otherwise repaints must stay I/O-free. */
            if (slot->state == -1 &&
                slot->gen != tg_mtproto_avatar_store_generation()) {
                slot->state = 0;
                slot = 0;
            }
            break;
        }
    }
    if (slot == 0) {
        const unsigned char *thumb;
        unsigned long thumb_len;
        static unsigned char rgb[TG_GUI_AV_SZ * TG_GUI_AV_SZ * 3];
        unsigned long px;
        int have_rgb = 0;

        /* Source priority: the downloaded 160px JPEG on disk (v2, crisp),
           else the inline stripped thumb (v1, blurred), else initials. */
        {
            char name[48];
            FILE *f;

            sprintf(name, "avatars/tgav%08lx%08lx.jpg", id_hi, id_lo);
            f = fopen(name, "rb");
            if (f != 0) {
                static unsigned char jpeg[24576];
                unsigned long n = (unsigned long)fread(jpeg, 1, sizeof(jpeg),
                                                       f);

                fclose(f);
                if (n > 0UL && n < sizeof(jpeg) &&
                    tg_avatar_decode_jpeg(jpeg, n, rgb, TG_GUI_AV_SZ,
                                          TG_GUI_AV_SZ) == 0) {
                    have_rgb = 1;
                }
            }
        }
        if (!have_rgb) {
            if (!tg_mtproto_avatar_thumb_lookup(id_hi, id_lo, &thumb,
                                                &thumb_len)) {
                thumb = 0; /* nothing yet: cache the miss as a negative slot
                              below, so the next repaint skips the disk probe */
                thumb_len = 0UL;
            }
        }
        for (i = 0; i < TG_GUI_AV_SLOTS; ++i) {
            if (tg_gui_av_slots[i].state == 0) {
                slot = &tg_gui_av_slots[i];
                break;
            }
        }
        if (slot == 0) {
            slot = &tg_gui_av_slots[tg_gui_av_evict % TG_GUI_AV_SLOTS];
            ++tg_gui_av_evict;
        }
        slot->id_hi = id_hi;
        slot->id_lo = id_lo;
        slot->gen = tg_mtproto_avatar_store_generation();
        if (!have_rgb &&
            (thumb == 0 ||
             tg_avatar_decode_stripped(thumb, thumb_len, rgb, TG_GUI_AV_SZ,
                                       TG_GUI_AV_SZ) != 0)) {
            slot->state = -1; /* nothing/undecodable: initials, no re-probe */
            return 0;
        }
        for (px = 0UL; px < TG_GUI_AV_SZ * TG_GUI_AV_SZ; ++px) {
            LONG p = tg_gui_av_pen_for(rgb + px * 3UL);

            if (p == -1) { /* pen system exhausted: give up cleanly */
                slot->state = -1;
                return 0;
            }
            slot->pen[px] = (unsigned char)p;
        }
        slot->state = 1;
    }
    if (slot->state != 1) {
        return 0;
    }
    /* Replay the pen grid scaled to rect (nearest), row by row with
       run-length RectFills into the current (buffered) RastPort. */
    for (y = 0; y < rect.h; ++y) {
        int sy = (y * TG_GUI_AV_SZ) / rect.h;
        int x = 0;

        while (x < rect.w) {
            int sx = (x * TG_GUI_AV_SZ) / rect.w;
            unsigned char p = slot->pen[sy * TG_GUI_AV_SZ + sx];
            int run = x + 1;

            while (run < rect.w &&
                   slot->pen[sy * TG_GUI_AV_SZ +
                             ((run * TG_GUI_AV_SZ) / rect.w)] == p) {
                ++run;
            }
            SetAPen(ctx->rport, (LONG)p);
            RectFill(ctx->rport, ctx->origin_x + rect.x + x,
                     ctx->origin_y + rect.y + y,
                     ctx->origin_x + rect.x + run - 1,
                     ctx->origin_y + rect.y + y);
            x = run;
        }
    }
    return 1;
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
    tg_gui_av_release_pool(cmap); /* the shared real-avatar pen pool */
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
/* Free the off-screen double-buffer if present. Safe when buf_bm==0. */
static void tg_gui_amiga_buffer_free(tg_gui_amiga_ctx *ctx)
{
    if (ctx->buf_bm != 0) {
        FreeBitMap(ctx->buf_bm);
        ctx->buf_bm = 0;
    }
    ctx->buf_ok = 0;
    ctx->buf_w = 0;
    ctx->buf_h = 0;
}

/* (Re)allocate the off-screen buffer to the CURRENT inner_w/inner_h as a friend
   of the window bitmap, so depth/format/placement match (chunky on a gfx card,
   planar on AGA) and the blit is native. Frees any old buffer first. On any
   failure leaves buf_ok==0 so the paint path falls back to direct rendering.
   MUST run after tg_gui_amiga_measure_geometry() so the geometry is current. */
static void tg_gui_amiga_buffer_alloc(tg_gui_amiga_ctx *ctx)
{
    struct BitMap *src;
    struct BitMap *bm;
    int w;
    int h;

    tg_gui_amiga_buffer_free(ctx);
    if (ctx->window == 0 || ctx->rport == 0 || ctx->rport->BitMap == 0) {
        return;
    }
    src = ctx->rport->BitMap;
    w = ctx->inner_w;
    h = ctx->inner_h;
    if (w < 8 || h < 8) {
        return; /* below the window minimum: skip buffering */
    }
    bm = AllocBitMap((ULONG)w, (ULONG)h,
                     (ULONG)GetBitMapAttr(src, BMA_DEPTH), 0UL, src);
    if (bm == 0) {
        tg_gui_log("window: double-buffer alloc failed, direct render");
        return;
    }
    InitRastPort(&ctx->buf_rp);
    ctx->buf_rp.BitMap = bm;
    /* InitRastPort does NOT inherit a font; text_width()/draw_text() read it from
       ctx->rport (== &buf_rp during the off-screen pass), so set it now. */
    SetFont(&ctx->buf_rp, ctx->rport->Font);
    if (ctx->buf_rp.Font == 0) {
        FreeBitMap(bm);
        tg_gui_log("window: double-buffer has no font, direct render");
        return;
    }
    ctx->buf_bm = bm;
    ctx->buf_w = w;
    ctx->buf_h = h;
    ctx->buf_ok = 1;
}

/* Full-window paint. With the off-screen buffer, render the whole frame INTO it
   (no layer, no lock), then copy it to the window in ONE BltBitMapRastPort under
   the same LockLayerRom discipline the direct path used -- the window only ever
   shows complete frames, so the clear-then-draw flicker is gone. Falls back to
   the direct render when no buffer is available (alloc failed / window too
   small). */
static void tg_gui_window_paint(const tg_gui_state *state,
                                tg_gui_backend *backend)
{
    tg_gui_amiga_ctx *c = (tg_gui_amiga_ctx *)backend->context;
    struct Layer *layer;

    if (c == 0 || c->rport == 0) {
        return;
    }
    layer = c->rport->Layer;
    if (c->buf_ok && c->buf_bm != 0) {
        struct RastPort *saved_rport = c->rport;
        int saved_ox = c->origin_x;
        int saved_oy = c->origin_y;

        c->rport = &c->buf_rp;
        c->origin_x = 0;
        c->origin_y = 0;
        tg_gui_paint(state, backend);
        c->rport = saved_rport;
        c->origin_x = saved_ox;
        c->origin_y = saved_oy;

        if (layer != 0) {
            LockLayerRom(layer);
        }
        BltBitMapRastPort(c->buf_bm, 0, 0, c->rport, saved_ox, saved_oy,
                          c->inner_w, c->inner_h, 0xC0);
        if (layer != 0) {
            UnlockLayerRom(layer);
        }
    } else {
        if (layer != 0) {
            LockLayerRom(layer);
        }
        tg_gui_paint(state, backend);
        if (layer != 0) {
            UnlockLayerRom(layer);
        }
    }
}

/* Caret-only blink repaint. With the buffer, re-render just the focused strip
   into it (tg_gui_paint_caret touches only that strip), then blit the whole
   already-current buffer -- correct and flicker-free; the blink only runs while a
   field is focused, so the 2 Hz full copy is cheap. */
static void tg_gui_window_paint_caret(const tg_gui_state *state,
                                      tg_gui_backend *backend)
{
    tg_gui_amiga_ctx *c = (tg_gui_amiga_ctx *)backend->context;
    struct Layer *layer;

    if (c == 0 || c->rport == 0) {
        return;
    }
    layer = c->rport->Layer;
    if (c->buf_ok && c->buf_bm != 0) {
        struct RastPort *saved_rport = c->rport;
        int saved_ox = c->origin_x;
        int saved_oy = c->origin_y;

        c->rport = &c->buf_rp;
        c->origin_x = 0;
        c->origin_y = 0;
        tg_gui_paint_caret(state, backend);
        c->rport = saved_rport;
        c->origin_x = saved_ox;
        c->origin_y = saved_oy;

        if (layer != 0) {
            LockLayerRom(layer);
        }
        BltBitMapRastPort(c->buf_bm, 0, 0, c->rport, saved_ox, saved_oy,
                          c->inner_w, c->inner_h, 0xC0);
        if (layer != 0) {
            UnlockLayerRom(layer);
        }
    } else {
        if (layer != 0) {
            LockLayerRom(layer);
        }
        tg_gui_paint_caret(state, backend);
        if (layer != 0) {
            UnlockLayerRom(layer);
        }
    }
}

/* How many extra older pages the open may auto-pull to make the backlog overflow
   the window (so a scrollbar appears) when the first page kept only a few text
   rows. Bounded per platform: MorphOS smallest (bsdsocket freeze risk on many
   replies), m68k modest, PPC/AROS a bit more. */
#if defined(__MORPHOS__) || defined(__MORPHOS)
#define TG_GUI_TOPUP_MAX 1
#elif defined(__m68k__)
#define TG_GUI_TOPUP_MAX 2
#else
#define TG_GUI_TOPUP_MAX 3
#endif

/* Switch to chat `sel`: show its header + an empty transcript at once, then
   fetch the history. The instant first paint keeps the switch responsive on a
   slow link instead of the window appearing frozen on the old chat until the
   load finishes. */
static void tg_gui_window_open_selection(tg_gui_state *state, int sel,
                                         tg_gui_backend *backend)
{
    /* Hard bounds guard: with a reprojected (possibly emptied) sidebar a
       stale sel would read garbage from chats[] and open a nonexistent peer
       -- the silent half of the "remove leaves the app stuck" report. */
    if (sel < 0 || sel >= state->chat_count) {
        tg_gui_window_paint(state, backend);
        return;
    }
    state->selected_chat = sel;
    state->selected_msg = -1; /* new chat: no message highlighted yet */
    state->transcript_scroll = 0; /* a freshly opened chat pins to the newest */
    state->chat_scroll_to_sel = 1; /* scroll the sidebar so the row is visible */
    /* Opening a chat clears its unread badge / flash -- you are now reading it. */
    state->chats[sel].unread = 0;
    state->chats[sel].flash = 0;
    tg_gui_window_apply_selection(state);
    if (tg_gui_session_is_open()) {
        state->message_count = 0;
        state->msg_gen++;
        tg_gui_window_paint(state, backend);
        tg_gui_log("open_selection: open_chat begin");
        (void)tg_gui_session_open_chat(state->chats[sel].index, stdout);
    }
    tg_gui_window_paint(state, backend);
    /* Auto-top-up: some chats keep only a few text rows at open (service/empty
       messages dropped, or a media-tail aborting the parse), so the backlog fits
       the window and no scrollbar is drawn -- the user then can't tell there is
       more history. While the content fits (sb_tr_max==0) and the chat start is
       not reached, pull older pages via the proven load_older path (newest stays
       on screen, so allow_drop=0) until it overflows and a scrollbar appears;
       normal scroll/wheel paging takes over from there. Bounded per platform.
       transcript_scroll stays 0 so the newest message remains pinned at bottom. */
    if (tg_gui_session_is_open()) {
        int topup;

        for (topup = 0; topup < TG_GUI_TOPUP_MAX; ++topup) {
            if (state->sb_tr_max > 0) {
                break; /* already overflows -> a scrollbar is present */
            }
            if (tg_gui_session_load_older(stdout, 0) <= 0) {
                break; /* chat start reached (0) or transient fetch failure (<0) */
            }
            tg_gui_window_paint(state, backend);
        }
    }
    /* The unread badge was just cleared (you are reading this chat) -- persist it
       so it does not snap back to the snapshot count after a restart. */
    tg_gui_session_persist_unread();
}

/* Jump the open transcript to the true newest message (Telegram's down-arrow).
   If the ring-bottom is STALE (newest_dropped: a load-older paging evicted the
   true-newest tail), the only way back is to RELOAD via open_selection -- it
   re-fetches the newest history, exactly what re-entering the chat does, and
   tg_gui_session_open_chat then clears the flags centrally. Otherwise the newest
   is already in the ring, so just re-pin (transcript_scroll = 0). */
static void tg_gui_window_jump_to_bottom(tg_gui_state *state,
                                         tg_gui_backend *backend,
                                         int *older_exhausted,
                                         int *older_cooldown)
{
    if (state == 0) {
        return;
    }
    if (state->newest_dropped && state->selected_chat >= 0 &&
        state->selected_chat < state->chat_count && tg_gui_session_is_open()) {
        /* Reload path. open_selection on the SAME chat does not change
           selected_chat, so the loop's open-time re-arm of the paging latches
           would not fire -- reset them here. */
        tg_gui_window_open_selection(state, state->selected_chat, backend);
        if (older_exhausted != 0) {
            *older_exhausted = 0;
        }
        if (older_cooldown != 0) {
            *older_cooldown = 0;
        }
    } else {
        state->transcript_scroll = 0;
        state->unread_below = 0;
        state->newest_dropped = 0;
        tg_gui_window_paint(state, backend);
    }
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

    /* Never submit an EMPTY phone/code field. sendCode/signIn block for several
       seconds, during which a held/auto-repeating RETURN queues a second submit;
       once the step has advanced to the code, that stray RETURN fired signIn with
       an empty code -> a cryptic "query-build-failed" before the user could type
       it (the code had already arrived on the phone). Re-prompt instead.
       LOGIN_2FA is DELIBERATELY excluded: an empty submit there is the legitimate
       way to finish login on an account with no 2FA password -- check_password
       asks the server (account.getPassword) and the "no password required"
       shortcut completes the login. Blocking it traps such users in a re-prompt
       loop if they ever land on the 2FA screen. A real 2FA account just gets a
       harmless "password-invalid" and re-prompts. */
    if ((state->mode == TG_GUI_MODE_LOGIN_PHONE ||
         state->mode == TG_GUI_MODE_LOGIN_CODE) &&
        state->input[0] == '\0') {
        tg_gui_window_copy(
            state->status, sizeof(state->status),
            state->mode == TG_GUI_MODE_LOGIN_PHONE
                ? "Enter your phone (+ country code)"
                : "Enter the code you received");
        state->cursor_on = 1;
        *caret_ticks = 0;
        tg_gui_window_paint(state, backend);
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
                               "2FA password (Enter if you have none)");
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
/* ---- clipboard.device, zero-dep IFF-FTXT ---------------------------------
   Copy/paste for issue #5. The IFF is built and parsed BY HAND (FORM/FTXT/
   CHRS) with explicit big-endian 32-bit sizes: IFF is big-endian on the
   clipboard while the AROS lanes are little-endian CPUs. Unit 0, the one
   every Amiga clipboard tool shares. */
#if defined(TG_GUI_AMIGA)
static void tg_gui_clip_u32be(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static unsigned long tg_gui_clip_u32be_read(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) |
           ((unsigned long)p[2] << 8) | (unsigned long)p[3];
}

static struct IOClipReq *tg_gui_clip_open(struct MsgPort **port_out)
{
    struct MsgPort *port;
    struct IOClipReq *io;

    port = CreateMsgPort();
    if (port == 0) {
        return 0;
    }
    io = (struct IOClipReq *)CreateIORequest(port, sizeof(struct IOClipReq));
    if (io == 0 ||
        OpenDevice((CONST_STRPTR)"clipboard.device", 0,
                   (struct IORequest *)io, 0) != 0) {
        if (io != 0) {
            DeleteIORequest((struct IORequest *)io);
        }
        DeleteMsgPort(port);
        return 0;
    }
    *port_out = port;
    return io;
}

static void tg_gui_clip_close(struct IOClipReq *io, struct MsgPort *port)
{
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);
}

/* Writes `text` to clip unit 0 as FORM FTXT / CHRS. 1 = ok. */
static int tg_gui_clip_write_text(const char *text)
{
    static unsigned char iff[TG_GUI_MSG_TEXT_MAX + 24];
    struct MsgPort *port;
    struct IOClipReq *io;
    unsigned long tlen, even;
    int ok = 0;

    if (text == 0 || text[0] == '\0') {
        return 0;
    }
    tlen = (unsigned long)strlen(text);
    if (tlen > (unsigned long)TG_GUI_MSG_TEXT_MAX) {
        tlen = TG_GUI_MSG_TEXT_MAX;
    }
    even = tlen + (tlen & 1UL);
    memcpy(iff, "FORM", 4);
    tg_gui_clip_u32be(iff + 4, 4UL + 8UL + even); /* FTXT + CHRS hdr + data */
    memcpy(iff + 8, "FTXT", 4);
    memcpy(iff + 12, "CHRS", 4);
    tg_gui_clip_u32be(iff + 16, tlen);
    memcpy(iff + 20, text, tlen);
    if (tlen & 1UL) {
        iff[20 + tlen] = 0; /* IFF pad byte */
    }
    io = tg_gui_clip_open(&port);
    if (io == 0) {
        return 0;
    }
    io->io_Offset = 0;
    io->io_ClipID = 0;
    io->io_Command = CMD_WRITE;
    io->io_Data = (STRPTR)iff;
    io->io_Length = (LONG)(20UL + even);
    if (DoIO((struct IORequest *)io) == 0) {
        io->io_Command = CMD_UPDATE;
        ok = DoIO((struct IORequest *)io) == 0;
    }
    tg_gui_clip_close(io, port);
    return ok;
}

/* One positioned CMD_READ helper; the device advances io_Offset itself. */
static long tg_gui_clip_read(struct IOClipReq *io, void *buf,
                             unsigned long len)
{
    io->io_Command = CMD_READ;
    io->io_Data = (STRPTR)buf;
    io->io_Length = (LONG)len;
    if (DoIO((struct IORequest *)io) != 0) {
        return -1;
    }
    return (long)io->io_Actual;
}

/* Reads the first CHRS chunk of a FORM FTXT clip into out (NUL-terminated).
   Returns the number of bytes copied (0 = empty clip / not text). */
static unsigned long tg_gui_clip_read_text(char *out, unsigned long out_size)
{
    struct MsgPort *port;
    struct IOClipReq *io;
    unsigned char hdr[12];
    unsigned long copied = 0;

    if (out == 0 || out_size == 0UL) {
        return 0;
    }
    out[0] = '\0';
    io = tg_gui_clip_open(&port);
    if (io == 0) {
        return 0;
    }
    io->io_Offset = 0;
    io->io_ClipID = 0;
    if (tg_gui_clip_read(io, hdr, 12UL) == 12L &&
        memcmp(hdr, "FORM", 4) == 0 && memcmp(hdr + 8, "FTXT", 4) == 0) {
        for (;;) {
            unsigned char chdr[8];
            unsigned long clen;

            if (tg_gui_clip_read(io, chdr, 8UL) != 8L) {
                break;
            }
            clen = tg_gui_clip_u32be_read(chdr + 4);
            if (memcmp(chdr, "CHRS", 4) == 0 && copied == 0UL) {
                unsigned long want = clen;

                if (want > out_size - 1UL) {
                    want = out_size - 1UL;
                }
                if (want > 0UL &&
                    tg_gui_clip_read(io, out, want) == (long)want) {
                    copied = want;
                }
                out[copied] = '\0';
                break; /* first text chunk is all we paste */
            }
            /* skip a foreign chunk (+ IFF pad) by dummy reads */
            {
                static unsigned char sink[256];
                unsigned long skip = clen + (clen & 1UL);

                while (skip > 0UL) {
                    unsigned long step = skip > sizeof(sink) ? sizeof(sink)
                                                             : skip;

                    if (tg_gui_clip_read(io, sink, step) <= 0L) {
                        skip = 0UL;
                        break;
                    }
                    skip -= step;
                }
            }
        }
    }
    /* drain to the end so the device releases the clip */
    {
        static unsigned char sink[256];

        while (tg_gui_clip_read(io, sink, sizeof(sink)) > 0L) {
        }
    }
    tg_gui_clip_close(io, port);
    return copied;
}
#endif /* TG_GUI_AMIGA */

static struct NewMenu tg_gui_newmenu[] = {
    { NM_TITLE, (STRPTR)"Telegram", 0, 0, 0, 0 },
    { NM_ITEM,  (STRPTR)"About...", 0, 0, 0, (APTR)TG_MENU_ABOUT },
    { NM_ITEM,  (STRPTR)"Help...",  0, 0, 0, (APTR)TG_MENU_HELP },
    { NM_ITEM,  (STRPTR)NM_BARLABEL, 0, 0, 0, 0 },
    { NM_ITEM,  (STRPTR)"Remove chat from list", (STRPTR)"R", 0, 0,
      (APTR)TG_MENU_REMOVE },
    { NM_ITEM,  (STRPTR)"Send file...", (STRPTR)"F", 0, 0,
      (APTR)TG_MENU_SENDFILE },
    { NM_ITEM,  (STRPTR)"Copy message", (STRPTR)"C", 0, 0,
      (APTR)TG_MENU_COPY },
    { NM_ITEM,  (STRPTR)"Paste", (STRPTR)"V", 0, 0,
      (APTR)TG_MENU_PASTE },
    { NM_ITEM,  (STRPTR)"Cut input", (STRPTR)"X", 0, 0,
      (APTR)TG_MENU_CUT },
    { NM_ITEM,  (STRPTR)"Iconify", (STRPTR)"I", 0, 0,
      (APTR)TG_MENU_ICONIFY },
    { NM_ITEM,  (STRPTR)"Own screen", 0, CHECKIT | MENUTOGGLE, 0,
      (APTR)TG_MENU_OWNSCREEN },
    { NM_ITEM,  (STRPTR)NM_BARLABEL, 0, 0, 0, 0 },
    { NM_ITEM,  (STRPTR)"Quit", (STRPTR)"Q", 0, 0, (APTR)TG_MENU_QUIT },
    { NM_END,   0, 0, 0, 0, 0 }
};

static const char tg_gui_about_text[] =
    "Telegram Amiga\n"
    "alpha " TG_VERSION "  (built " __DATE__ ")\n\n"
    "A native Telegram client for AmigaOS,\n"
    "MorphOS and AROS.\n\n"
    "by Michele Dipace\n"
    "michele.dipace@kaffeine.net";

static const char tg_gui_help_text[] =
    "Chat selection:\n"
    "  F1 - F10          chats 1 to 10\n"
    "  Shift + F1 - F10  chats 11 to 20\n\n"
    "ENTER        write a message to the open chat\n"
    "Del / A+R    remove the selected chat from the list\n"
    "A+F          send a file to the open chat\n"
    "A+I          iconify to an AppIcon (double-click it to return)\n"
    "ESC          cancel\n"
    "Q            quit";

/* EasyRequest runs its OWN modal input loop and never answers our
   IDCMP_MENUVERIFY handshake, so a right-click while it is up would stall every
   app's menus on the whole screen (RKRM: drop the verify bits around a
   requester). Bracket the call with ModifyIDCMP and restore the original flags
   after. */
static LONG tg_gui_amiga_easyreq_args(struct Window *win, struct EasyStruct *es)
{
    ULONG saved = win->IDCMPFlags;
    LONG result;

    if ((saved & IDCMP_MENUVERIFY) != 0UL) {
        ModifyIDCMP(win, saved & ~(ULONG)IDCMP_MENUVERIFY);
    }
    result = EasyRequestArgs(win, es, 0, 0);
    if ((saved & IDCMP_MENUVERIFY) != 0UL) {
        ModifyIDCMP(win, saved);
    }
    return result;
}

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
    (void)tg_gui_amiga_easyreq_args(win, &es);
}

/* Two-button confirm for removing a chat. Returns 1 = Remove, 0 = Cancel. The
   chat name is baked into the body with any '%' dropped, so EasyRequest (which
   treats es_TextFormat as a printf format) never reads phantom args -- safer than
   passing the name as a pointer arg, which would also be size-fragile on 64-bit
   AROS. */
static int tg_gui_amiga_confirm_remove(struct Window *win, const char *name)
{
    struct EasyStruct es;
    char body[TG_GUI_NAME_MAX + 80];
    const char *pre = "Remove this chat from the list?\n\n";
    const char *post = "\n\n(re-add it later via Search)";
    const char *p;
    int n;

    n = 0;
    for (p = pre; *p != '\0' && n < (int)sizeof(body) - 1; ++p) {
        body[n++] = *p;
    }
    if (name != 0) {
        for (p = name; *p != '\0' && n < (int)sizeof(body) - 1; ++p) {
            if (*p != '%') {
                body[n++] = *p;
            }
        }
    }
    for (p = post; *p != '\0' && n < (int)sizeof(body) - 1; ++p) {
        body[n++] = *p;
    }
    body[n] = '\0';

    es.es_StructSize = (ULONG)sizeof(struct EasyStruct);
    es.es_Flags = 0UL;
    es.es_Title = (STRPTR)"Remove chat";
    es.es_TextFormat = (STRPTR)body;
    es.es_GadgetFormat = (STRPTR)"Remove|Cancel";
    return (int)tg_gui_amiga_easyreq_args(win, &es);
}

/* One-line confirm before deleting a message for everyone. 1 = Delete. */
static int tg_gui_amiga_confirm_delete(struct Window *win)
{
    struct EasyStruct es;

    es.es_StructSize = (ULONG)sizeof(struct EasyStruct);
    es.es_Flags = 0UL;
    es.es_Title = (STRPTR)"Delete message";
    es.es_TextFormat = (STRPTR)"Delete this message for everyone?";
    es.es_GadgetFormat = (STRPTR)"Delete|Cancel";
    return (int)tg_gui_amiga_easyreq_args(win, &es);
}

/* Confirm + remove the selected chat from the sidebar, persist it, then land on
   a neighbouring chat (or an empty transcript if the list is now empty). Shared
   by the menu item and the Del key. No-op outside chat mode / with no selection. */
/* Dynamic right-button trap: while the pointer is over a real transcript
   bubble the window claims the right button (WFLG_RMBTRAP -> MENUDOWN comes
   in as a normal MOUSEBUTTONS event for OUR context menu); anywhere else the
   flag is dropped so the right button opens the standard Intuition menu bar.
   Flag poking under Forbid() is the documented classic idiom and works on
   every lane. This replaces IDCMP_MENUVERIFY, whose reply handshake blocked
   input.device system-wide whenever this task was busy in a slow network
   poll -- the "right-click freezes the whole Amiga" report. */
static void tg_gui_amiga_set_rmbtrap(struct Window *win, int on)
{
    if (win == 0) {
        return;
    }
    if (((win->Flags & WFLG_RMBTRAP) != 0UL) == (on != 0)) {
        return; /* already in the wanted state */
    }
    Forbid();
    if (on) {
        win->Flags |= WFLG_RMBTRAP;
    } else {
        win->Flags &= ~(ULONG)WFLG_RMBTRAP;
    }
    Permit();
}

/* "Send file...": ASL file requester -> chunk-5 uploader on the open chat.
   The requester is synchronous and system-rendered (safe while we are the
   caller); the upload itself is the same blocking on-context class as the
   download, with the status row narrating the outcome. */
static void tg_gui_window_send_file(tg_gui_state *state, struct Window *win,
                                    tg_gui_backend *backend)
{
    struct FileRequester *req;
    char path[256];
    int rc;

    if (state->mode != TG_GUI_MODE_CHAT || !tg_gui_session_is_open() ||
        state->chat_count <= 0) {
        return;
    }
    AslBase = OpenLibrary((CONST_STRPTR)"asl.library", 38L);
    if (AslBase == 0) {
        tg_gui_window_copy(state->status, sizeof(state->status),
                           "asl.library V38 not found");
        tg_gui_window_paint(state, backend);
        return;
    }
#if defined(__amigaos4__)
    IAsl = (struct AslIFace *)GetInterface(AslBase, "main", 1L, 0);
    if (IAsl == 0) {
        CloseLibrary(AslBase);
        AslBase = 0;
        return;
    }
#endif
    req = (struct FileRequester *)AllocAslRequestTags(
        ASL_FileRequest, ASLFR_Window, (unsigned long)win, ASLFR_TitleText,
        (unsigned long)"Send file to this chat", TAG_DONE);
    path[0] = '\0';
    if (req != 0 && AslRequestTags(req, TAG_DONE)) {
        unsigned long n = 0UL;
        const char *p;

        for (p = (const char *)req->fr_Drawer; p != 0 && *p != '\0' &&
             n + 2UL < sizeof(path); ++p) {
            path[n++] = *p;
        }
        /* Join drawer and name the AmigaDOS way: add '/' only when the drawer
           does not already end in ':' or '/'. */
        if (n > 0UL && path[n - 1UL] != ':' && path[n - 1UL] != '/') {
            path[n++] = '/';
        }
        for (p = (const char *)req->fr_File; p != 0 && *p != '\0' &&
             n + 1UL < sizeof(path); ++p) {
            path[n++] = *p;
        }
        path[n] = '\0';
    }
    if (req != 0) {
        FreeAslRequest(req);
    }
#if defined(__amigaos4__)
    DropInterface((struct Interface *)IAsl);
    IAsl = 0;
#endif
    CloseLibrary(AslBase);
    AslBase = 0;
    if (path[0] == '\0') {
        return; /* cancelled */
    }
    tg_gui_window_copy(state->status, sizeof(state->status), "Uploading...");
    tg_gui_window_paint(state, backend);
    rc = tg_gui_session_send_document(path, stdout);
    if (rc == 0) {
        tg_gui_window_copy(state->status, sizeof(state->status), "File sent");
    } else if (rc == 2) {
        tg_gui_window_copy(state->status, sizeof(state->status),
                           "File too big (10 MB limit for now)");
    } else if (rc == 3) {
        tg_gui_window_copy(state->status, sizeof(state->status),
                           "Could not read that file");
    } else if (rc == 5) {
        tg_gui_window_copy(state->status, sizeof(state->status),
                           "That file is empty (0 bytes)");
    } else {
        tg_gui_window_copy(state->status, sizeof(state->status),
                           "Upload failed");
    }
    tg_gui_window_paint(state, backend);
}

static void tg_gui_window_remove_selected(tg_gui_state *state,
                                          struct Window *win,
                                          tg_gui_backend *backend)
{
    int sel;
    unsigned long idx;

    if (state->mode != TG_GUI_MODE_CHAT || !tg_gui_session_is_open()) {
        return;
    }
    sel = state->selected_chat;
    if (sel < 0 || sel >= state->chat_count) {
        return;
    }
    idx = state->chats[sel].index;
    if (idx == 0UL) {
        tg_gui_log("remove: idx 0, ignored");
        return;
    }
    if (idx == TG_GUI_SAVED_PEER_INDEX) {
        tg_gui_window_copy(state->status, sizeof(state->status),
                           "Saved Messages is always available");
        tg_gui_window_paint(state, backend);
        return;
    }
    tg_gui_log("remove: begin (showing confirm)");
    if (tg_gui_amiga_confirm_remove(win, state->chats[sel].name) != 1) {
        return; /* cancelled */
    }
    if (tg_gui_session_remove_chat(idx, stdout) != 0) {
        /* Silent no-ops read as "stuck": say it and repaint instead. */
        tg_gui_window_copy(state->status, sizeof(state->status),
                           "Could not remove this chat");
        tg_gui_window_paint(state, backend);
        return;
    }
    /* remove_chat reprojected the sidebar (chat_count updated). Open a neighbour
       so the user is never left on the now-gone chat. */
    if (state->chat_count > 0) {
        int nsel = (sel < state->chat_count) ? sel : (state->chat_count - 1);
        tg_gui_window_open_selection(state, nsel, backend);
    } else {
        state->selected_chat = 0;
        state->message_count = 0;
        state->msg_gen++;
        state->title[0] = '\0';
        state->subtitle[0] = '\0';
        tg_gui_window_paint(state, backend);
    }
}

/* Persist the last window GEOMETRY (size + position) to a small file next to
   the binary (Work:TGh, which survives a reboot) so a reopen restores the
   window exactly where the user left it -- the "pin the window" testers asked
   for. Format: "w h x y"; older files hold just "w h" and still load (position
   stays -1 = let Intuition place it), and older binaries reading a new file
   simply ignore the trailing x y. A roomier default on first run than the old
   600x380. */
static void tg_gui_window_load_geom(int *w, int *h, int *x, int *y, int *own)
{
    FILE *f;
    int rw;
    int rh;
    int rx;
    int ry;
    int got;
    char tok[16];

    *w = 820;
    *h = 560;
    *x = -1; /* -1 = no saved position: Intuition picks the spot */
    *y = -1;
    *own = 0; /* opt-in: append " own" to the geometry line for an own screen */
    rw = 0;
    rh = 0;
    rx = -1;
    ry = -1;
    f = fopen("data/telegram-gui-win.txt", "r");
    if (f != 0) {
        got = fscanf(f, "%d %d %d %d", &rw, &rh, &rx, &ry);
        if (got >= 2 && rw >= 320 && rh >= 200 && rw <= 4096 && rh <= 4096) {
            *w = rw;
            *h = rh;
            if (got == 4 && rx >= 0 && ry >= 0 && rx <= 8192 && ry <= 8192) {
                *x = rx;
                *y = ry;
            }
        }
        /* Optional trailing token: "own" = open on an own (private, cloned
           from Workbench) screen. The save path re-writes it, so the user's
           hand-added toggle survives; old binaries just never read this far. */
        if (fscanf(f, "%15s", tok) == 1 &&
            (strcmp(tok, "own") == 0 || strcmp(tok, "OWN") == 0)) {
            *own = 1;
        }
        fclose(f);
    }
}

static void tg_gui_window_save_geom(int w, int h, int x, int y, int own)
{
    FILE *f;

    if (w < 320 || h < 200) {
        return;
    }
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    (void)mkdir("data", 0777); /* best-effort; normally the launcher made it */
    f = fopen("data/telegram-gui-win.txt", "w");
    if (f != 0) {
        fprintf(f, "%d %d %d %d%s\n", w, h, x, y, own ? " own" : "");
        fclose(f);
    }
}

/* Recompute the '@' mention popup after a composer edit or caret move: token
   under the caret (tg_gui_mention_token) -> candidate usernames from the open
   group's member cache. Selection resets to the top match on every refresh so
   the popup always answers "what will ENTER insert" at a glance. */
static void tg_gui_window_mention_refresh(tg_gui_state *state)
{
    int start = 0;
    int plen;
    char prefix[TG_GUI_MENTION_LEN];

    state->mention_active = 0;
    state->mention_count = 0;
    state->mention_sel = 0;
    if (!state->composing || state->mode != TG_GUI_MODE_CHAT ||
        !tg_gui_session_is_open()) {
        return;
    }
    plen = tg_gui_mention_token(state->input, state->input_caret, &start);
    if (plen < 0 || plen >= (int)sizeof(prefix)) {
        return;
    }
    memcpy(prefix, state->input + start + 1, (unsigned long)plen);
    prefix[plen] = '\0';
    state->mention_count = tg_gui_session_mention_candidates(
        prefix, &state->mention_items[0][0], TG_GUI_MENTION_LEN,
        TG_GUI_MENTION_MAX, stdout);
    if (state->mention_count > 0) {
        state->mention_active = 1;
        state->mention_start = start;
    }
}

/* Replace the '@'-token under the caret with "@<picked username> " and place
   the caret after the space; closes the popup. Bounded by the input buffer. */
static void tg_gui_window_mention_complete(tg_gui_state *state)
{
    char out[TG_GUI_MSG_TEXT_MAX];
    unsigned long o = 0UL;
    unsigned long caret;
    int i;
    const char *u;

    if (!state->mention_active || state->mention_sel < 0 ||
        state->mention_sel >= state->mention_count) {
        return;
    }
    /* head, up to and including the '@' */
    for (i = 0; i <= state->mention_start && o + 1UL < sizeof(out); ++i) {
        out[o++] = state->input[i];
    }
    u = state->mention_items[state->mention_sel];
    while (*u != '\0' && o + 1UL < sizeof(out)) {
        out[o++] = *u++;
    }
    if (o + 1UL < sizeof(out)) {
        out[o++] = ' ';
    }
    caret = o;
    /* tail: whatever followed the caret */
    u = state->input + state->input_caret;
    while (*u != '\0' && o + 1UL < sizeof(out)) {
        out[o++] = *u++;
    }
    out[o] = '\0';
    tg_gui_window_copy(state->input, sizeof(state->input), out);
    if (caret > (unsigned long)strlen(state->input)) {
        caret = (unsigned long)strlen(state->input);
    }
    state->input_caret = (int)caret;
    state->in_sel_active = 0; /* the rebuilt input invalidates a selection */
    state->mention_active = 0;
    state->mention_count = 0;
}

/* Deletes the composer's selected range, moving the caret to its start.
   1 = a selection was consumed (callers repaint / then insert); 0 = none. */
static int tg_gui_window_input_delete_sel(tg_gui_state *state)
{
    unsigned long n;
    long a;
    long b;
    long lo;
    long hi;

    if (!state->in_sel_active) {
        return 0;
    }
    state->in_sel_active = 0;
    n = (unsigned long)strlen(state->input);
    a = (long)state->in_sel_anchor;
    b = (long)state->input_caret;
    lo = a < b ? a : b;
    hi = a > b ? a : b;
    if (lo < 0) {
        lo = 0;
    }
    if (hi > (long)n) {
        hi = (long)n;
    }
    if (hi <= lo) {
        return 0;
    }
    memmove(&state->input[lo], &state->input[hi],
            n - (unsigned long)hi + 1UL);
    state->input_caret = (int)lo;
    return 1;
}

/* Load the last online search's openable results into the sidebar list so the
   existing renderer + click hit-test present them as a picker. chats[] is
   restored from the cache (tg_gui_session_refresh_chats) on cancel/open. */
static void tg_gui_window_load_search_results(tg_gui_state *state)
{
    int n;
    int k;

    n = tg_gui_session_search_count();
    if (n > TG_GUI_MAX_CHATS) {
        n = TG_GUI_MAX_CHATS;
    }
    for (k = 0; k < n; ++k) {
        const char *nm = tg_gui_session_search_name(k);
        char c;

        tg_gui_window_copy(state->chats[k].name, sizeof(state->chats[k].name),
                           nm);
        state->chats[k].preview[0] = '\0';
        state->chats[k].time[0] = '\0';
        c = nm[0];
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 32);
        }
        state->chats[k].initials[0] = (c != '\0') ? c : '?';
        state->chats[k].initials[1] = '\0';
        state->chats[k].avatar_color = k % TG_GUI_AVATAR_COLORS;
        state->chats[k].unread = 0;
        state->chats[k].index = (unsigned long)(k + 1);
        state->chats[k].peer_id_hi = 0UL;
        state->chats[k].peer_id_lo = 0UL;
        state->chats[k].flash = 0;
    }
    state->chat_count = n;
    state->selected_chat = 0;
    state->in_search = 1;
}

/* Run an online search for the current query and show the matches in the sidebar
   as a picker (click one to open). With auto_open_single, a lone match opens
   straight away -- that is what ENTER wants; the as-you-type debounce passes 0 so
   it never opens behind the user's back while typing. Restores the real chat list
   when the query is empty or yields nothing. */
static void tg_gui_window_run_search(tg_gui_state *state, tg_gui_backend *backend,
                                     int auto_open_single)
{
    int cnt;

    state->search_dirty = 0;
    if (state->search_query[0] == '\0') {
        if (state->in_search) {
            state->in_search = 0;
            tg_gui_session_refresh_chats();
            tg_gui_window_paint(state, backend);
        }
        return;
    }
    tg_gui_window_copy(state->status, sizeof(state->status),
                       "Searching Telegram...");
    tg_gui_window_paint(state, backend);
    cnt = tg_gui_session_search_run(state->search_query, stdout);
    if (cnt == 1 && auto_open_single) {
        (void)tg_gui_session_search_open_result(0, stdout);
        state->in_search = 0;
        state->search_active = 0;
        state->search_query[0] = '\0';
        tg_gui_window_copy(state->status, sizeof(state->status),
                           "Live - F1-F10 chats, Q quits");
    } else if (cnt >= 1) {
        /* Show the matches in the sidebar; keep the search box focused so ESC
           cancels and more typing refines. The user clicks a result to open it. */
        tg_gui_window_load_search_results(state);
        tg_gui_window_copy(state->status, sizeof(state->status),
                           "Pick a result - click it (ESC cancels)");
    } else {
        /* None / error: drop any stale picker and restore the real chat list. */
        if (state->in_search) {
            state->in_search = 0;
            tg_gui_session_refresh_chats();
        }
        tg_gui_window_copy(state->status, sizeof(state->status),
                           cnt < 0 ? "Search failed (network?)"
                                   : "No match - try a name or @username");
    }
    tg_gui_window_paint(state, backend);
}

static int tg_gui_run_window_once(tg_gui_state *state)
{
    tg_gui_amiga_ctx ctx;
    tg_gui_backend backend;
    int init_w;
    int init_h;
    int init_x;
    int init_y;
    int init_own;
    int want_own; /* own-screen preference, toggled live by the menu */
    struct Screen *own_scr;
    struct TagItem tags[22];
    struct ColorMap *cmap;
    struct TextFont *font;
    APTR vi;
    struct Menu *menu;
    unsigned long mem_before;
    unsigned long mem_after;
    unsigned long footprint;
    int i;
    int done;
    struct MsgPort *timer_port = 0; /* live-reception heartbeat (timer.device) */
    tg_gui_timereq *timer_req = 0;
    int timer_ok = 0;
    int timer_pending = 0;
    int caret_ticks;
    int search_idle_ticks; /* INTUITICKS since the search query last changed */
    int older_exhausted;   /* load-older confirmed the chat start; re-armed off-top / on open */
    int older_cooldown;    /* wakes to wait before another load-older (slow-link breather) */
    int prev_selected;     /* last selected_chat: a change means a (re)opened chat -> re-arm */
    unsigned long watch_seconds;
    unsigned long watch_boot_seconds;
    unsigned long watch_boot_grace;
    unsigned long effective_watch;
    time_t session_boot;
    time_t last_session_poll;
    time_t last_receive_drain;
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
    own_scr = 0;
    tg_gui_window_load_geom(&init_w, &init_h, &init_x, &init_y, &init_own);
    want_own = init_own;
    /* Own-screen mode (opt-in " own" token in telegram-gui-win.txt): a PRIVATE
       screen cloned from Workbench (SA_LikeWorkbench, V39 on every lane). The
       testers' "move to another page" gadget is MUI-only, so an app screen is
       the one way to give them a dedicated page. Key tags: SA_SharePens=TRUE
       (without it a paletted OS3 clone holds pens exclusive and every
       ObtainBestPen fails), SA_Pens={~0} (full new-look with the user's pen
       preferences), SA_Behind (the screen surfaces only after the first locked
       paint -- same anti-race discipline as WA_Activate=FALSE). Any open
       failure (no chip RAM on a stock A1200, unknown mode...) falls back to
       the normal Workbench-window path -- never a hard failure. */
    if (init_own) {
        ULONG oserr = 0;
        static UWORD own_pens[] = { (UWORD)~0 };
        struct TagItem stags[9];
        int s = 0;

        stags[s].ti_Tag = SA_LikeWorkbench;
        stags[s++].ti_Data = TRUE;
        stags[s].ti_Tag = SA_Title;
        stags[s++].ti_Data = TG_GUI_TAG("Telegram Amiga");
        stags[s].ti_Tag = SA_Pens;
        stags[s++].ti_Data = TG_GUI_TAG(own_pens);
        stags[s].ti_Tag = SA_SharePens;
        stags[s++].ti_Data = TRUE;
        stags[s].ti_Tag = SA_SysFont;
        stags[s++].ti_Data = 1;
        stags[s].ti_Tag = SA_ShowTitle;
        stags[s++].ti_Data = TRUE;
        stags[s].ti_Tag = SA_Behind;
        stags[s++].ti_Data = TRUE;
        stags[s].ti_Tag = SA_ErrorCode;
        stags[s++].ti_Data = TG_GUI_TAG(&oserr);
        stags[s].ti_Tag = TAG_END;
        stags[s++].ti_Data = 0;
        own_scr = OpenScreenTagList(0, stags);
        if (own_scr == 0) {
            printf("gui window: own screen failed (err %lu) - using default\n",
                   (unsigned long)oserr);
        } else {
            /* The saved geometry may exceed the clone (the Workbench mode may
               have shrunk since the save): clamp so the window open cannot
               fail for a stale size. */
            if (init_w > (int)own_scr->Width - 8) {
                init_w = (int)own_scr->Width - 8;
            }
            if (init_h > (int)own_scr->Height - 32) {
                init_h = (int)own_scr->Height - 32;
            }
        }
    }
    i = 0;
    /* Saved position first (when any): if this exact spot no longer fits the
       screen, OpenWindowTagList FAILS -- the open call below retries once with
       these two tags neutralised, falling back to Intuition's own placement. */
    if (init_x >= 0 && init_y >= 0) {
        tags[i].ti_Tag = WA_Left;
        tags[i++].ti_Data = (ULONG)init_x;
        tags[i].ti_Tag = WA_Top;
        tags[i++].ti_Data = (ULONG)init_y;
    }
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
#if defined(__amigaos4__)
    /* OS4 titlebar iconify gadget (like OWB's): a click sends IDCMP_CLOSEWINDOW
       with Code == 1, which we route to the same AppIcon park as the menu. */
    tags[i].ti_Tag = WA_IconifyGadget;
    tags[i++].ti_Data = TRUE;
#endif
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
                        IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_MENUPICK
                        /* NO IDCMP_MENUVERIFY, ever: input.device blocks the
                           WHOLE SYSTEM until the verify is replied, and this
                           task can be tens of seconds deep in a network poll
                           (heavy channels) -- a right-click then froze all of
                           OS3. The context menu uses a dynamic WFLG_RMBTRAP
                           instead (see the MOUSEMOVE handler). */
#if defined(__amigaos4__)
                        /* OS4: the wheel arrives as IDCMP_EXTENDEDMOUSE -- the only
                           form QEMU's emulated mouse emits. Real hardware and the
                           other platforms also send the NewMouse RAWKEY 0x7A/0x7B. */
                        | IDCMP_EXTENDEDMOUSE
#endif
                        ;
    /* MOUSEMOVE is only delivered with REPORTMOUSE (or a follow-mouse gadget),
       so the scrollbar knob-drag needs this. The handler ignores moves unless a
       knob is grabbed, so the extra reports cost nothing when idle. */
    tags[i].ti_Tag = WA_ReportMouse;
    tags[i++].ti_Data = TRUE;
    if (own_scr != 0) {
        /* OWNER window on our private screen (WA_CustomScreen, not a visitor):
           CloseScreen at teardown stays deterministically under our control. */
        tags[i].ti_Tag = WA_CustomScreen;
        tags[i++].ti_Data = TG_GUI_TAG(own_scr);
    }
    tags[i].ti_Tag = TAG_END;
    tags[i++].ti_Data = 0;

    /* Sample free memory now -- after the libraries are open -- so the
       footprint delta isolates the window, RastPort and pens, not the resident
       cost of opening intuition/graphics. */
    mem_before = (unsigned long)AvailMem(MEMF_ANY);

    ctx.window = OpenWindowTagList(0, tags);
    if (ctx.window == 0 && init_x >= 0 && init_y >= 0) {
        /* The remembered position no longer fits (smaller screen/mode since the
           save). Neutralise WA_Left/WA_Top -- they are tags[0]/tags[1] when a
           position was loaded -- and let Intuition place the window instead. */
        tags[0].ti_Tag = TAG_IGNORE;
        tags[1].ti_Tag = TAG_IGNORE;
        ctx.window = OpenWindowTagList(0, tags);
    }
    if (ctx.window == 0 && own_scr != 0) {
        /* Still no window on the own screen: give the screen up and retry on
           the default public screen (the WA_CustomScreen pair is the one right
           before TAG_END). Degraded > dead. */
        tags[i - 2].ti_Tag = TAG_IGNORE;
        CloseScreen(own_scr);
        own_scr = 0;
        ctx.window = OpenWindowTagList(0, tags);
    }
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
    if (own_scr != 0 && own_scr->RastPort.Font != 0) {
        /* SA_SysFont sets the SCREEN font, but a window RastPort still comes up
           with the fixed-width DefaultFont (autodoc caveat) -- adopt the screen
           font so text metrics match the Workbench-window mode exactly. */
        SetFont(ctx.rport, own_scr->RastPort.Font);
    }
    font = ctx.rport->Font;
    ctx.line_h = (font != 0 ? (int)font->tf_YSize : 8) + 2;
    cmap = ctx.window->WScreen->ViewPort.ColorMap;
    tg_gui_amiga_obtain_pens(&ctx, cmap);
    tg_gui_av_reset();          /* pens are per-screen: drop stale avatar pens */
    tg_gui_av_cmap = cmap;      /* arms the real-avatar path */
    {
        /* Avatar pen profile from THIS screen's depth: truecolor RTG affords
           the rich budget even on m68k (Vampire); paletted stays lean. */
        ULONG av_depth = GetBitMapAttr(ctx.window->WScreen->RastPort.BitMap,
                                       BMA_DEPTH);

        tg_gui_av_rich = av_depth > 8UL;
        tg_gui_av_pool_cap = tg_gui_av_rich ? TG_GUI_AV_POOL_MAX : 48;
        tg_gui_av_share_d = tg_gui_av_rich ? 48L : 192L;
    }
    tg_gui_amiga_measure_geometry(&ctx);
    tg_gui_amiga_buffer_alloc(&ctx); /* off-screen double-buffer (flicker-free) */

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
                /* Reflect the current own-screen mode in the toggle's tick. */
                if (want_own && menu->FirstItem != 0) {
                    struct MenuItem *it2 = menu->FirstItem;

                    while (it2 != 0) {
                        if (GTMENUITEM_USERDATA(it2) ==
                            (APTR)TG_MENU_OWNSCREEN) {
                            it2->Flags |= CHECKED;
                            break;
                        }
                        it2 = it2->NextItem;
                    }
                }
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
    backend.avatar_image = tg_gui_amiga_avatar_image;
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
    if (own_scr != 0) {
        /* The screen opened BEHIND (SA_Behind) so nobody saw the pre-paint
           window; surface it only now that the first locked paint settled --
           and never while a layer lock is held. */
        ScreenToFront(own_scr);
    }
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
    last_receive_drain = time(0);
    last_key_time = time(0);
    done = 0;
    state->composing = 0;
    state->history_count = 0;
    state->history_pos = -1;
    state->history_draft[0] = '\0';
    state->chat_scroll = 0;
    state->transcript_scroll = 0;
    state->sb_drag = 0;
    state->drag_src = -1; /* no row-reorder drag armed */
    state->drag_active = 0;
    /* A login screen shows its caret from the first frame. */
    state->cursor_on = (state->mode != TG_GUI_MODE_CHAT) ? 1 : 0;
    caret_ticks = 0;
    search_idle_ticks = 0;
    older_exhausted = 0;
    older_cooldown = 0;
    prev_selected = state->selected_chat;
    /* Live-reception heartbeat. INTUITICKS are delivered ONLY to the ACTIVE
       window, so with the window deactivated the loop slept in Wait() and the
       network poll never ran -- incoming messages stalled until the user came
       back and clicked around (A4000/Roadshow report). A timer.device VBLANK
       request on the same Wait() wakes the loop every TG_GUI_HEARTBEAT_SECS
       regardless of activation; the poll keeps its own cadence/composing
       guards, so an ACTIVE window behaves exactly as before. If any setup step
       fails we simply keep the old active-only behavior. */
    timer_port = CreateMsgPort();
    if (timer_port != 0) {
        timer_req = (tg_gui_timereq *)CreateIORequest(timer_port,
                                                      sizeof(tg_gui_timereq));
    }
    if (timer_req != 0 &&
        OpenDevice((CONST_STRPTR)"timer.device", UNIT_VBLANK,
                   (struct IORequest *)timer_req, 0) == 0) {
        timer_ok = 1;
        TG_GUI_TR_NODE(timer_req).io_Command = TR_ADDREQUEST;
        TG_GUI_TR_SECS(timer_req) = TG_GUI_HEARTBEAT_SECS;
        TG_GUI_TR_MICRO(timer_req) = 0;
        SendIO((struct IORequest *)timer_req);
        timer_pending = 1;
    }
    while (!done) {
        struct IntuiMessage *msg;
        int session_dirty;
        int scroll_dirty;
        int want_older;     /* a transcript scroll-up reached the top this wake */
        int reveal_older;   /* a fits-window load happened: scroll to show it */

        session_dirty = 0;
        scroll_dirty = 0;
        want_older = 0;
        reveal_older = 0;
        if (older_cooldown > 0) {
            older_cooldown -= 1;
        }
        {
            ULONG wait_mask = 1UL << ctx.window->UserPort->mp_SigBit;

            if (timer_ok) {
                wait_mask |= 1UL << timer_port->mp_SigBit;
            }
            (void)Wait(wait_mask);
        }
        while ((msg = (struct IntuiMessage *)GetMsg(ctx.window->UserPort)) !=
               0) {
            ULONG msg_class;
            UWORD msg_code;
            UWORD msg_qual;
            WORD mouse_x;
            WORD mouse_y;
#if defined(__amigaos4__)
            WORD wheel_y = 0;
#endif

            msg_class = msg->Class;
            msg_code = msg->Code;
            msg_qual = msg->Qualifier;
            mouse_x = msg->MouseX;
            mouse_y = msg->MouseY;
            /* Feed REAL user input (keys, clicks, pointer motion) into the
               platform entropy ring -- the DRBG absorbs it on every generate.
               This is what makes the first-run auth-key DH benefit from the
               human at the keyboard even on the GUI-only path (the console
               already fed its stdin bytes). INTUITICKS et al. are skipped so
               the ring stays input-dominated. O(1), no hashing here. */
            if (msg_class == IDCMP_RAWKEY || msg_class == IDCMP_VANILLAKEY ||
                msg_class == IDCMP_MOUSEBUTTONS ||
                msg_class == IDCMP_MOUSEMOVE
#if defined(__amigaos4__)
                || msg_class == IDCMP_EXTENDEDMOUSE
#endif
                ) {
                tg_platform_note_input_event(
                    ((unsigned long)msg_class << 8) ^ (unsigned long)msg_code ^
                        ((unsigned long)msg_qual << 20),
                    ((unsigned long)(unsigned short)mouse_x << 16) |
                        (unsigned long)(unsigned short)mouse_y);
            }
#if defined(__amigaos4__)
            /* Read the wheel delta BEFORE ReplyMsg: the IntuiWheelData behind
               IAddress is only guaranteed valid until the message is replied. */
            if (msg_class == IDCMP_EXTENDEDMOUSE &&
                msg_code == IMSGCODE_INTUIWHEELDATA && msg->IAddress != 0) {
                wheel_y = ((struct IntuiWheelData *)msg->IAddress)->WheelY;
            }
#endif
            ReplyMsg((struct Message *)msg);

            /* Remember the last keystroke so the live poll can defer the
               (blocking) tick while you are actively typing -- see the
               IDCMP_INTUITICKS handler below. */
            if (msg_class == IDCMP_VANILLAKEY || msg_class == IDCMP_RAWKEY) {
                last_key_time = time(0);
            }
#if defined(__amigaos4__)
            /* QEMU's emulated OS4 mouse delivers the wheel only as
               IDCMP_EXTENDEDMOUSE, not the NewMouse RAWKEY 0x7A/0x7B that real
               hardware + the other platforms send (iBrowse handles
               IDCMP_EXTENDEDMOUSE, which is why it scrolls under QEMU and we did
               not). Translate it into those RAWKEY codes so the wheel handler
               below reuses all its panel/scroll logic unchanged. */
            if (msg_class == IDCMP_EXTENDEDMOUSE && wheel_y != 0) {
                msg_class = IDCMP_RAWKEY;
                msg_code = (wheel_y < 0) ? (UWORD)0x7A : (UWORD)0x7B;
            }
#endif

            /* A keystroke (or wheel, already mapped to RAWKEY above) while the
               context menu is open simply closes it and is consumed -- standard
               menu behaviour, and it avoids ESC also quitting the app. */
            if (state->ctx_visible &&
                (msg_class == IDCMP_VANILLAKEY || msg_class == IDCMP_RAWKEY)) {
                state->ctx_visible = 0;
                tg_gui_window_paint(state, &backend);
                continue;
            }

            if (msg_class == IDCMP_CLOSEWINDOW) {
#if defined(__amigaos4__)
                if (msg_code == 1) { /* the iconify gadget, not a real close */
                    tg_gui_log("window: iconify gadget");
                    done = 2; /* park on the AppIcon, same as the menu item */
                } else
#endif
                {
                    tg_gui_log("window: close gadget");
                    done = 1;
                }
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
                    state->search_caret = 0;
                    if (state->in_search) { /* cancel the picker -> restore chats */
                        state->in_search = 0;
                        tg_gui_session_refresh_chats();
                    }
                    tg_gui_window_copy(state->status, sizeof(state->status),
                                       "Live - F1-F10 chats, Q quits");
                    tg_gui_window_paint(state, &backend);
                } else if (msg_code == 8) { /* BACKSPACE: delete BEFORE the caret */
                    unsigned long n;
                    int sc;

                    n = (unsigned long)strlen(state->search_query);
                    sc = state->search_caret;
                    if (sc < 0 || sc > (int)n) {
                        sc = (int)n;
                    }
                    if (sc > 0) { /* delete the char BEFORE the caret */
                        memmove(state->search_query + sc - 1,
                                state->search_query + sc, n - (unsigned long)sc
                                + 1UL);
                        state->search_caret = sc - 1;
                        state->search_dirty = 1; /* re-search after the pause */
                        search_idle_ticks = 0;   /* restart the debounce */
                        last_key_time = time(0);
                        tg_gui_window_copy(state->status, sizeof(state->status),
                                           "Searching when you pause...");
                        tg_gui_window_paint(state, &backend);
                    }
                } else if (msg_code == 127) { /* Canc/Del: delete AT the caret */
                    unsigned long n;
                    int sc;

                    n = (unsigned long)strlen(state->search_query);
                    sc = state->search_caret;
                    if (sc < 0 || sc > (int)n) {
                        sc = (int)n;
                    }
                    if (sc < (int)n) { /* forward-delete: pull the tail one left */
                        memmove(state->search_query + sc,
                                state->search_query + sc + 1,
                                n - (unsigned long)sc);
                        state->search_dirty = 1; /* re-search after the pause */
                        search_idle_ticks = 0;   /* restart the debounce */
                        last_key_time = time(0);
                        tg_gui_window_copy(state->status, sizeof(state->status),
                                           "Searching when you pause...");
                        tg_gui_window_paint(state, &backend);
                    }
                } else if (msg_code == 13 || msg_code == 10) { /* ENTER: search */
                    tg_gui_window_run_search(state, &backend, 1);
                } else if (msg_code >= 32 && msg_code < 256) { /* printable */
                    unsigned long n;
                    int sc;

                    n = (unsigned long)strlen(state->search_query);
                    sc = state->search_caret;
                    if (sc < 0 || sc > (int)n) {
                        sc = (int)n;
                    }
                    if (n + 1UL < sizeof(state->search_query)) {
                        memmove(state->search_query + sc + 1,
                                state->search_query + sc,
                                n - (unsigned long)sc + 1UL);
                        state->search_query[sc] = (char)msg_code;
                        state->search_caret = sc + 1;
                        state->search_dirty = 1; /* re-search after the pause */
                        search_idle_ticks = 0;   /* restart the debounce */
                        last_key_time = time(0);
                        tg_gui_window_copy(state->status, sizeof(state->status),
                                           "Searching when you pause...");
                        tg_gui_window_paint(state, &backend);
                    }
                }
            } else if (msg_class == IDCMP_VANILLAKEY && state->composing) {
                /* Composing: keys edit the input line; RETURN sends, ESC
                   cancels, BACKSPACE deletes. While the '@' mention popup is
                   up, RETURN/TAB insert the highlighted username instead (the
                   NEXT return sends) and ESC only closes the popup. */
                if ((msg_code == 13 || msg_code == 10 || msg_code == 9) &&
                    state->mention_active) {
                    tg_gui_window_mention_complete(state);
                    tg_gui_window_paint(state, &backend);
                } else if (msg_code == 27 && state->mention_active) {
                    state->mention_active = 0;
                    state->mention_count = 0;
                    tg_gui_window_paint(state, &backend);
                } else if (msg_code == 13 || msg_code == 10) {
                    if (state->input[0] != '\0') {
                        if (state->edit_to_id != 0UL) {
                            state->in_sel_active = 0;
                            /* Edit mode: save the edit, then leave edit mode (the
                               bubble is updated in place by the session call). */
                            (void)tg_gui_session_edit(state->input,
                                                      state->edit_to_id, stdout);
                            state->edit_to_id = 0UL;
                        } else if (tg_gui_session_send(state->input,
                                                       state->reply_to_id,
                                                       stdout) == 0) {
                            tg_gui_history_add(state, state->input);
                            state->in_sel_active = 0;
                            state->reply_to_id = 0UL; /* clear only on success */
                            state->reply_sender[0] = '\0';
                            state->reply_snippet[0] = '\0';
                            tg_gui_window_jump_to_bottom(state, &backend,
                                                         &older_exhausted,
                                                         &older_cooldown);
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
                    /* Leaving the composer drops any pending reply/edit too. */
                    state->reply_to_id = 0UL;
                    state->reply_sender[0] = '\0';
                    state->reply_snippet[0] = '\0';
                    state->edit_to_id = 0UL;
                    tg_gui_window_copy(state->status, sizeof(state->status),
                                       "Live - F1-F10 chats, Q quits");
                    tg_gui_window_paint(state, &backend);
                } else if (msg_code == 8) { /* BACKSPACE: delete BEFORE the caret */
                    unsigned long n;
                    unsigned long c;

                    if (tg_gui_window_input_delete_sel(state)) {
                        /* a selection consumes the keypress whole */
                        tg_gui_window_mention_refresh(state);
                        tg_gui_window_paint(state, &backend);
                    } else {
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
                        tg_gui_window_mention_refresh(state);
                        tg_gui_window_paint(state, &backend);
                    }
                    }
                } else if (msg_code == 127) { /* Canc/Del: delete AT the caret */
                    unsigned long n;
                    unsigned long c;

                    if (tg_gui_window_input_delete_sel(state)) {
                        tg_gui_window_mention_refresh(state);
                        tg_gui_window_paint(state, &backend);
                    } else {
                    n = (unsigned long)strlen(state->input);
                    c = (unsigned long)state->input_caret;
                    if (c < n) {
                        /* forward-delete: pull the tail (incl. NUL) one left,
                           the caret stays put. Backspace (0x08) deletes left;
                           this splits the two so Canc is not folded into it. */
                        memmove(&state->input[c], &state->input[c + 1UL],
                                n - c);
                        tg_gui_window_mention_refresh(state);
                        tg_gui_window_paint(state, &backend);
                    }
                    }
                } else if (msg_code >= 32 && msg_code < 256) {
                    unsigned long n;
                    unsigned long c;

                    /* typing REPLACES an active selection (classic field) */
                    (void)tg_gui_window_input_delete_sel(state);
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
                        tg_gui_window_mention_refresh(state);
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
                    if (msg_code == 0x7A) {
                        want_older = 1; /* may have reached the top: paged below */
                    }
                }
                scroll_dirty = 1;
            } else if (msg_class == IDCMP_RAWKEY && state->search_active &&
                       (msg_code == 0x4F || msg_code == 0x4E)) {
                /* F8: arrows move the search caret (insert point). */
                int n = (int)strlen(state->search_query);
                int sc = state->search_caret;

                if (sc < 0 || sc > n) {
                    sc = n;
                }
                if (msg_code == 0x4F && sc > 0) {
                    --sc;
                } else if (msg_code == 0x4E && sc < n) {
                    ++sc;
                }
                state->search_caret = sc;
                state->cursor_on = 1;
                caret_ticks = 0;
                tg_gui_window_paint(state, &backend);
            } else if (msg_class == IDCMP_RAWKEY && state->composing &&
                       state->mode == TG_GUI_MODE_CHAT) {
                /* While composing, LEFT/RIGHT move the caret within the input.
                   The key-up event arrives as code|0x80, so the strict == tests
                   fire exactly once per press. With the '@' mention popup up,
                   UP/DOWN move its highlight instead of recalling history. */
                if ((msg_code == 0x4C || msg_code == 0x4D) &&
                    state->mention_active) {
                    if (msg_code == 0x4C) { /* up */
                        state->mention_sel = (state->mention_sel > 0)
                                                 ? state->mention_sel - 1
                                                 : state->mention_count - 1;
                    } else {                 /* down */
                        state->mention_sel =
                            (state->mention_sel + 1 < state->mention_count)
                                ? state->mention_sel + 1
                                : 0;
                    }
                    tg_gui_window_paint(state, &backend);
                } else if (msg_code == 0x4F || msg_code == 0x4E) {
                    /* cursor left/right; with SHIFT they grow/shrink the
                       composer selection anchored where Shift was first
                       pressed (the classic text-field gesture). */
                    int shifted =
                        (msg_qual &
                         (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0;
                    int changed = 0;

                    if (shifted && !state->in_sel_active) {
                        state->in_sel_active = 1;
                        state->in_sel_anchor = state->input_caret;
                    } else if (!shifted && state->in_sel_active) {
                        state->in_sel_active = 0;
                        changed = 1;
                    }
                    if (msg_code == 0x4F && state->input_caret > 0) {
                        state->input_caret--;
                        changed = 1;
                    } else if (msg_code == 0x4E &&
                               state->input_caret <
                                   (int)strlen(state->input)) {
                        state->input_caret++;
                        changed = 1;
                    }
                    if (shifted &&
                        state->input_caret == state->in_sel_anchor) {
                        state->in_sel_active = 0; /* collapsed back */
                    }
                    if (changed) {
                        tg_gui_window_mention_refresh(state);
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
                        state->in_sel_active = 0;
                        tg_gui_window_paint(state, &backend);
                    }
                } else if (msg_code == 0x4D) { /* cursor down: newer sent line */
                    state->in_sel_active = 0; /* input is about to be rebuilt */
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
                       !state->in_search && state->mode == TG_GUI_MODE_CHAT) {
                /* F1..F10 (rawkey 0x50..0x59) pick chats 1..10; Shift adds 10
                   for 11..20 -- matching the console's F-key selection. (Disabled
                   while the search picker is up: its rows are not real chats, so
                   an F-key would open a bogus chat and strand the picker.) */
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
                    want_older = 1;
                    scroll_dirty = 1;
                } else if (msg_code == 0x4D) { /* cursor down: newer messages */
                    state->transcript_scroll -= 3 * ctx.line_h;
                    if (state->transcript_scroll < 0) {
                        state->transcript_scroll = 0;
                    }
                    scroll_dirty = 1;
                } else if (msg_code == 0x46) { /* Del: remove selected chat (confirm) */
                    tg_gui_window_remove_selected(state, ctx.window, &backend);
                }
            } else if (msg_class == IDCMP_MENUPICK) {
                UWORD mnum;

                tg_gui_log("menu: pick");
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
                        } else if (ud == (APTR)TG_MENU_REMOVE) {
                            tg_gui_window_remove_selected(state, ctx.window,
                                                          &backend);
                        } else if (ud == (APTR)TG_MENU_SENDFILE) {
                            tg_gui_window_send_file(state, ctx.window,
                                                    &backend);
                        } else if (ud == (APTR)TG_MENU_COPY) {
                            /* Copy the highlighted message's text (issue #5).
                               Selection = the row the user last right-clicked
                               or picked; without one, say what to do. */
                            static char selbuf[TG_GUI_MSG_TEXT_MAX];
                            int sel = state->selected_msg;
                            const char *src = 0;

                            if (state->composing && state->in_sel_active) {
                                /* composer selection: copy [anchor..caret] */
                                long a = (long)state->in_sel_anchor;
                                long b = (long)state->input_caret;
                                long lo = a < b ? a : b;
                                long hi = a > b ? a : b;
                                long tl = (long)strlen(state->input);

                                if (lo < 0) {
                                    lo = 0;
                                }
                                if (hi > tl) {
                                    hi = tl;
                                }
                                if (hi > lo &&
                                    (unsigned long)(hi - lo) <
                                        sizeof(selbuf)) {
                                    memcpy(selbuf, state->input + lo,
                                           (unsigned long)(hi - lo));
                                    selbuf[hi - lo] = '\0';
                                    src = selbuf;
                                }
                            }
                            if (src == 0 &&
                                tg_gui_selection_get(state, selbuf,
                                                     sizeof(selbuf))) {
                                src = selbuf; /* the dragged range wins */
                            } else if (src == 0 && sel >= 0 &&
                                       sel < state->message_count &&
                                       state->messages[sel].text[0] !=
                                           '\0') {
                                src = state->messages[sel].text;
                            }
                            if (src != 0 && tg_gui_clip_write_text(src)) {
                                tg_gui_window_copy(state->status,
                                                   sizeof(state->status),
                                                   "Copied to clipboard");
                            } else {
                                tg_gui_window_copy(state->status,
                                                   sizeof(state->status),
                                                   sel < 0
                                                       ? "Right-click a message first"
                                                       : "Nothing to copy");
                            }
                            tg_gui_window_paint(state, &backend);
                        } else if (ud == (APTR)TG_MENU_CUT) {
                            /* Cut the focused input line to the clipboard:
                               the search box when active, else the composer.
                               Completes the cut/copy/paste trio (issue #5). */
                            char *field = state->search_active
                                              ? state->search_query
                                              : state->input;

                            if (!state->search_active && state->composing &&
                                state->in_sel_active) {
                                /* cut ONLY the selected composer range */
                                static char cutbuf[TG_GUI_MSG_TEXT_MAX];
                                long a = (long)state->in_sel_anchor;
                                long b = (long)state->input_caret;
                                long lo = a < b ? a : b;
                                long hi = a > b ? a : b;
                                long tl = (long)strlen(state->input);

                                if (lo < 0) {
                                    lo = 0;
                                }
                                if (hi > tl) {
                                    hi = tl;
                                }
                                if (hi > lo &&
                                    (unsigned long)(hi - lo) <
                                        sizeof(cutbuf)) {
                                    memcpy(cutbuf, state->input + lo,
                                           (unsigned long)(hi - lo));
                                    cutbuf[hi - lo] = '\0';
                                    if (tg_gui_clip_write_text(cutbuf)) {
                                        (void)
                                        tg_gui_window_input_delete_sel(state);
                                        tg_gui_window_mention_refresh(state);
                                        tg_gui_window_copy(
                                            state->status,
                                            sizeof(state->status),
                                            "Cut to clipboard");
                                    } else {
                                        tg_gui_window_copy(
                                            state->status,
                                            sizeof(state->status),
                                            "Copy failed");
                                    }
                                } else {
                                    tg_gui_window_copy(state->status,
                                                       sizeof(state->status),
                                                       "Nothing to cut");
                                }
                                tg_gui_window_paint(state, &backend);
                            } else if (field[0] != '\0' &&
                                tg_gui_clip_write_text(field)) {
                                field[0] = '\0';
                                if (state->search_active) {
                                    state->search_caret = 0;
                                    state->search_dirty = 1;
                                    search_idle_ticks = 0;
                                } else {
                                    state->input_caret = 0;
                                    tg_gui_window_mention_refresh(state);
                                }
                                tg_gui_window_copy(state->status,
                                                   sizeof(state->status),
                                                   "Cut to clipboard");
                            } else {
                                tg_gui_window_copy(state->status,
                                                   sizeof(state->status),
                                                   "Nothing to cut");
                            }
                            tg_gui_window_paint(state, &backend);
                        } else if (ud == (APTR)TG_MENU_PASTE) {
                            /* Paste the clipboard's FTXT at the caret: into
                               the search box when it is active, else into the
                               composer. Newlines/tabs become spaces (the
                               input row is one visual line); other control
                               bytes are dropped. */
                            static char clip[TG_GUI_MSG_TEXT_MAX];
                            unsigned long got =
                                tg_gui_clip_read_text(clip, sizeof(clip));
                            unsigned long src, dst = 0UL;

                            for (src = 0UL; src < got; ++src) {
                                unsigned char cch = (unsigned char)clip[src];

                                if (cch == '\n' || cch == '\r' ||
                                    cch == '\t') {
                                    clip[dst++] = ' ';
                                } else if (cch >= 32) {
                                    clip[dst++] = (char)cch;
                                }
                            }
                            got = dst;
                            clip[got] = '\0';
                            if (got == 0UL) {
                                tg_gui_window_copy(state->status,
                                                   sizeof(state->status),
                                                   "Clipboard is empty");
                                tg_gui_window_paint(state, &backend);
                            } else if (state->search_active) {
                                unsigned long n = (unsigned long)strlen(
                                    state->search_query);
                                unsigned long c =
                                    (unsigned long)state->search_caret;
                                unsigned long room =
                                    sizeof(state->search_query) - 1UL - n;
                                unsigned long p = got > room ? room : got;

                                if (c > n) {
                                    c = n;
                                }
                                memmove(&state->search_query[c + p],
                                        &state->search_query[c], n - c + 1UL);
                                memcpy(&state->search_query[c], clip, p);
                                state->search_caret = (int)(c + p);
                                state->search_dirty = 1;
                                search_idle_ticks = 0;
                                tg_gui_window_copy(
                                    state->status, sizeof(state->status),
                                    "Searching when you pause...");
                                tg_gui_window_paint(state, &backend);
                            } else if (tg_gui_session_is_open() &&
                                       state->mode == TG_GUI_MODE_CHAT) {
                                unsigned long n;
                                unsigned long c;
                                unsigned long room;
                                unsigned long p;

                                (void)tg_gui_window_input_delete_sel(state);
                                n = (unsigned long)strlen(state->input);
                                c = (unsigned long)state->input_caret;
                                room = sizeof(state->input) - 1UL - n;
                                p = got > room ? room : got;

                                if (c > n) {
                                    c = n;
                                }
                                memmove(&state->input[c + p],
                                        &state->input[c], n - c + 1UL);
                                memcpy(&state->input[c], clip, p);
                                state->input_caret = (int)(c + p);
                                state->composing = 1;
                                state->cursor_on = 1;
                                caret_ticks = 0;
                                tg_gui_window_mention_refresh(state);
                                tg_gui_window_copy(state->status,
                                                   sizeof(state->status),
                                                   "Pasted");
                                tg_gui_window_paint(state, &backend);
                            } else {
                                tg_gui_window_copy(state->status,
                                                   sizeof(state->status),
                                                   "Open a chat first");
                                tg_gui_window_paint(state, &backend);
                            }
                        } else if (ud == (APTR)TG_MENU_ICONIFY) {
                            /* Tear down window (and own screen) via the normal
                               path; the outer loop parks on an AppIcon. */
                            done = 2;
                        } else if (ud == (APTR)TG_MENU_OWNSCREEN) {
                            /* Flip own-screen mode and reopen: persist the new
                               flag now so the reopen (which reloads geometry)
                               honours it, then leave via the reopen path. */
                            want_own = !want_own;
                            tg_gui_window_save_geom(ctx.inner_w, ctx.inner_h,
                                                    (int)ctx.window->LeftEdge,
                                                    (int)ctx.window->TopEdge,
                                                    want_own);
                            done = 3; /* reopen (no AppIcon) */
                        } else if (ud == (APTR)TG_MENU_QUIT) {
                            done = 1;
                        }
                    }
                    mnum = item->NextSelect;
                }
            } else if (msg_class == IDCMP_NEWSIZE) {
                tg_gui_amiga_measure_geometry(&ctx);
                tg_gui_amiga_buffer_alloc(&ctx); /* realloc buffer to new size */
                tg_gui_window_paint(state, &backend);
            } else if (msg_class == IDCMP_REFRESHWINDOW) {
                /* BeginRefresh() already holds this window's layer locked for the
                   whole bracket, so no LockLayerRom here. With the buffer -- and
                   only while it still matches the current size -- copy it into the
                   exposed clip region (BltBitMapRastPort honours the ClipRects, so
                   just the damaged area is touched). Otherwise re-render raw. */
                BeginRefresh(ctx.window);
                tg_gui_amiga_measure_geometry(&ctx);
                if (ctx.buf_ok && ctx.buf_bm != 0 &&
                    ctx.buf_w == ctx.inner_w && ctx.buf_h == ctx.inner_h) {
                    BltBitMapRastPort(ctx.buf_bm, 0, 0, ctx.rport,
                                      ctx.origin_x, ctx.origin_y,
                                      ctx.inner_w, ctx.inner_h, 0xC0);
                } else {
                    tg_gui_paint(state, &backend);
                }
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
                    if (!done && state->composing && now != (time_t)-1 &&
                        now >= last_receive_drain &&
                        (unsigned long)(now - last_receive_drain) >=
                            TG_GUI_COMPOSE_RECEIVE_SECONDS) {
                        last_receive_drain = now;
                        if (tg_gui_session_receive_pending(stdout)) {
                            session_dirty = 1;
                        }
                    }
                    /* As-you-type search: count INTUITICKS since the query last
                       changed and fire once the user pauses (~12 ticks ~= 1.2s).
                       Tick-based, NOT wall-clock: stray VM/RustDesk key events kept
                       resetting a time() debounce so it never tripped. The window
                       still ticks (the caret blinks), so the counter advances.
                       auto_open_single=0 -> never opens behind the user mid-type. */
                    if (state->search_active && state->search_dirty) {
                        if (++search_idle_ticks >= 12) {
                            search_idle_ticks = 0;
                            tg_gui_window_run_search(state, &backend, 0);
                        }
                    }
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
                if (msg_code == MENUDOWN) {
                    /* Right press with RMBTRAP held: open OUR context menu on
                       the bubble under the pointer. (Without the trap the
                       press never reaches us -- Intuition runs the menu bar.) */
                    int hit = tg_gui_hit_test(state, ctx.inner_w, ctx.inner_h,
                                              ctx.line_h, hx, hy);

                    if (hit <= TG_GUI_HIT_MESSAGE_BASE) {
                        int mi = TG_GUI_HIT_MESSAGE_BASE - hit;

                        if (mi >= 0 && mi < state->message_count &&
                            !state->messages[mi].is_system &&
                            state->messages[mi].id != 0UL) {
                            state->ctx_visible = 1;
                            state->ctx_msg = mi;
                            state->ctx_x = hx;
                            state->ctx_y = hy;
                            state->ctx_hover = -1;
                            tg_gui_window_paint(state, &backend);
                        }
                    }
                } else if (msg_code == SELECTDOWN &&
                           (state->sel_press_armed = 0, 0)) {
                    /* never taken: clears a stale press latch before ANY other
                       SELECTDOWN branch runs (ctx menu, sidebar, gadgets...);
                       the bubble branch below re-arms it deliberately. */
                } else if (msg_code == SELECTDOWN && state->composing &&
                           state->mention_active &&
                           tg_gui_mention_click(state, &backend, hx, hy) >= 0) {
                    /* Click a candidate in the '@' popup: select it and insert,
                       exactly like ENTER/TAB on the keyboard-highlighted row. */
                    state->mention_sel =
                        tg_gui_mention_click(state, &backend, hx, hy);
                    tg_gui_window_mention_complete(state);
                    tg_gui_window_paint(state, &backend);
                } else if (msg_code == SELECTDOWN && state->ctx_visible) {
                    /* A left click while the context menu is open: run the item
                       under the pointer, or dismiss when the click is outside. */
                    int it = tg_gui_context_menu_hit(state, ctx.inner_w,
                                                     ctx.inner_h, ctx.line_h,
                                                     hx, hy);
                    int mi = state->ctx_msg;
                    const tg_gui_message *m =
                        (mi >= 0 && mi < state->message_count)
                            ? &state->messages[mi]
                            : 0;

                    state->ctx_visible = 0;
                    if (it == TG_GUI_CTX_REPLY && m != 0 && !m->is_system &&
                        m->id != 0UL) {
                        state->in_sel_active = 0;
                        state->reply_to_id = m->id;
                        tg_gui_window_copy(state->reply_sender,
                                           sizeof(state->reply_sender),
                                           m->sender);
                        tg_gui_window_copy(state->reply_snippet,
                                           sizeof(state->reply_snippet),
                                           m->text);
                        state->edit_to_id = 0UL;
                        state->search_active = 0;
                        state->composing = 1;
                        state->input_caret = (int)strlen(state->input);
                        state->cursor_on = 1;
                        caret_ticks = 0;
                        tg_gui_window_copy(state->status, sizeof(state->status),
                                           "Reply - ENTER sends, ESC cancels");
                    } else if (it == TG_GUI_CTX_EDIT && m != 0 &&
                               (state->in_sel_active = 0, 1) &&
                               (m->is_own ||
                                tg_gui_open_chat_is_self(state)) &&
                               m->id != 0UL) {
                        /* Edit mode: pre-fill the composer with the message text;
                           the next ENTER routes to editMessage (edit_to_id). */
                        state->edit_to_id = m->id;
                        state->reply_to_id = 0UL;
                        state->reply_sender[0] = '\0';
                        state->reply_snippet[0] = '\0';
                        tg_gui_window_copy(state->input, sizeof(state->input),
                                           m->text);
                        state->search_active = 0;
                        state->composing = 1;
                        state->input_caret = (int)strlen(state->input);
                        state->cursor_on = 1;
                        caret_ticks = 0;
                        tg_gui_window_copy(state->status, sizeof(state->status),
                                           "Editing - ENTER saves, ESC cancels");
                    } else if (it == TG_GUI_CTX_DELETE && m != 0 &&
                               (m->is_own ||
                                tg_gui_open_chat_is_self(state)) &&
                               m->id != 0UL) {
                        unsigned long del_id = m->id;

                        if (tg_gui_amiga_confirm_delete(ctx.window) != 0) {
                            (void)tg_gui_session_delete(del_id, stdout);
                        }
                    } else if (it == TG_GUI_CTX_DOWNLOAD && m != 0 &&
                               m->has_document && m->id != 0UL) {
                        unsigned long dl_id = m->id;
                        char saved[160];
                        int drc;

                        /* Blocking download: tell the user it is working, then
                           report where it landed (or why not). */
                        tg_gui_window_copy(state->status, sizeof(state->status),
                                           "Downloading...");
                        tg_gui_window_paint(state, &backend);
                        drc = tg_gui_session_download_document(dl_id, saved,
                                                              sizeof(saved),
                                                              stdout);
                        if (drc == 0) {
                            char msg[192];

                            sprintf(msg, "Saved to %s", saved);
                            tg_gui_window_copy(state->status,
                                               sizeof(state->status), msg);
                        } else if (drc == 2) {
                            tg_gui_window_copy(
                                state->status, sizeof(state->status),
                                "File is on another server - not supported yet");
                        } else if (drc == 3) {
                            tg_gui_window_copy(state->status,
                                               sizeof(state->status),
                                               "Could not write to downloads/");
                        } else if (drc == 4) {
                            char tmsg[192];

                            if (saved[0] != '\0') {
                                sprintf(tmsg, "Transfer failed: %.160s", saved);
                            } else {
                                strcpy(tmsg, "Transfer failed (server error)");
                            }
                            tg_gui_window_copy(state->status,
                                               sizeof(state->status), tmsg);
                        } else {
                            char nmsg[192];

                            if (saved[0] != '\0') {
                                sprintf(nmsg, "Not found: %.170s", saved);
                            } else {
                                strcpy(nmsg,
                                       "File not found or reference expired");
                            }
                            tg_gui_window_copy(state->status,
                                               sizeof(state->status), nmsg);
                        }
                    } else if (it == TG_GUI_CTX_COPY && m != 0 &&
                               m->text[0] != '\0') {
                        static char ctxsel[TG_GUI_MSG_TEXT_MAX];
                        const char *src = m->text;

                        /* A dragged selection in THIS message wins over the
                           whole text. */
                        if (state->sel_active && state->sel_msg == mi &&
                            tg_gui_selection_get(state, ctxsel,
                                                 sizeof(ctxsel))) {
                            src = ctxsel;
                        }
                        state->selected_msg = mi; /* keep the row highlighted */
                        tg_gui_window_copy(state->status,
                                           sizeof(state->status),
                                           tg_gui_clip_write_text(src)
                                               ? "Copied to clipboard"
                                               : "Copy failed");
                        tg_gui_window_paint(state, &backend);
                    } else if (it == TG_GUI_CTX_SENDFILE) {
                        /* Chat-level action (not tied to the clicked message):
                           send a file to the open chat, same as the menubar
                           "Send file..." item. */
                        tg_gui_window_send_file(state, ctx.window, &backend);
                    }
                    tg_gui_window_paint(state, &backend);
                } else if (msg_code == SELECTUP) {
                    state->in_drag_armed = 0;
                    if (state->sel_press_armed) {
                        state->sel_press_armed = 0;
                        if (!state->sel_active &&
                            tg_gui_session_is_open() && !state->in_search) {
                            /* Plain click (never dragged): the classic gesture
                               -- reply to the bubble, exactly as before. The
                               press-time ID must still match: a transcript
                               shift between press and release would otherwise
                               aim the reply at the wrong message. */
                            int mi = state->sel_press_msg;

                            if (mi >= 0 && mi < state->message_count) {
                                const tg_gui_message *m = &state->messages[mi];

                                if (!m->is_system && m->id != 0UL &&
                                    m->id == state->sel_press_id &&
                                    mi == state->selected_msg) {
                                    /* Click on the ALREADY-highlighted message
                                       = toggle it off: deselect and cancel a
                                       pending reply to it. Covers the
                                       post-copy flow, where a stray click
                                       must not drag the user into reply
                                       mode -- and gives the missing deselect
                                       gesture. */
                                    state->selected_msg = -1;
                                    if (state->reply_to_id == m->id) {
                                        state->reply_to_id = 0UL;
                                        state->reply_sender[0] = '\0';
                                        state->reply_snippet[0] = '\0';
                                    }
                                    tg_gui_window_copy(state->status,
                                                       sizeof(state->status),
                                                       "");
                                    tg_gui_window_paint(state, &backend);
                                } else if (!m->is_system && m->id != 0UL &&
                                    m->id == state->sel_press_id) {
                                    state->in_sel_active = 0;
                                    state->selected_msg = mi;
                                    state->reply_to_id = m->id;
                                    tg_gui_window_copy(
                                        state->reply_sender,
                                        sizeof(state->reply_sender),
                                        m->sender);
                                    tg_gui_window_copy(
                                        state->reply_snippet,
                                        sizeof(state->reply_snippet),
                                        m->text);
                                    state->search_active = 0;
                                    state->composing = 1;
                                    state->input_caret =
                                        (int)strlen(state->input);
                                    state->cursor_on = 1;
                                    caret_ticks = 0;
                                    tg_gui_window_copy(
                                        state->status, sizeof(state->status),
                                        "Reply - ENTER sends, ESC cancels");
                                    tg_gui_window_paint(state, &backend);
                                }
                            }
                        } else {
                            /* Drag finished: keep the selection, tell how to
                               use it. */
                            tg_gui_window_copy(state->status,
                                               sizeof(state->status),
                                               "Selected - A+C copies");
                            tg_gui_window_paint(state, &backend);
                        }
                    }
                    state->sb_drag = 0;
                    if (state->drag_src >= 0) {
                        if (!state->drag_active) {
                            /* CLICK (never crossed the threshold): open the chat,
                               same logic that used to run on SELECTDOWN. */
                            int hit = state->drag_src;

                            state->search_active = 0;
                            if (state->composing) {
                                state->composing = 0;
                                state->input[0] = '\0';
                                state->input_caret = 0;
                                state->history_pos = -1;
                                tg_gui_window_copy(state->status,
                                                   sizeof(state->status),
                                                   "Live - F1-F10 chats, Q quits");
                            }
                            if (hit >= 0 && hit < state->chat_count &&
                                hit != state->selected_chat) {
                                tg_gui_window_open_selection(state, hit, &backend);
                            } else {
                                tg_gui_window_paint(state, &backend);
                            }
                        } else {
                            /* DRAG: reorder. Map the cursor to an insert-before
                               target, convert to a final 0-based destination row,
                               persist + reproject if it actually moves. */
                            int target = tg_gui_chat_drop_target(
                                state, ctx.line_h, state->drag_cur_y);
                            int dest = (target > state->drag_src) ? (target - 1)
                                                                  : target;

                            if (dest < 0) {
                                dest = 0;
                            }
                            if (dest >= state->chat_count) {
                                dest = state->chat_count - 1;
                            }
                            if (dest != state->drag_src &&
                                state->chats[state->drag_src].index !=
                                    TG_GUI_SAVED_PEER_INDEX &&
                                state->chats[dest].index !=
                                    TG_GUI_SAVED_PEER_INDEX) {
                                /* The pinned Saved Messages row neither moves
                                   nor is displaced: positions above it keep
                                   mapping 1:1 to the file's public indexes. */
                                (void)tg_gui_session_reorder_chat(
                                    (unsigned long)(state->drag_src + 1),
                                    (unsigned long)(dest + 1), stdout);
                            }
                            tg_gui_window_paint(state, &backend);
                        }
                        state->drag_src = -1;
                        state->drag_active = 0;
                    }
                } else if (msg_code == SELECTDOWN && state->jb_w > 0 &&
                           hx >= state->jb_x &&
                           hx < state->jb_x + state->jb_w &&
                           hy >= state->jb_y &&
                           hy < state->jb_y + state->jb_h) {
                    /* Floating scroll-to-bottom button: jump to the true newest.
                       jb_w == 0 when the painter did not draw it this frame, so a
                       click in that area then falls through to the normal hits. */
                    tg_gui_window_jump_to_bottom(state, &backend,
                                                 &older_exhausted, &older_cooldown);
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
                            want_older = 1;
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
                    if (hit >= 0 && state->in_search) {
                        /* Picker: click a search result -> add it to the cache +
                           open it. open_result reloads the real chat list. */
                        state->in_search = 0;
                        state->search_active = 0;
                        state->search_query[0] = '\0';
                        (void)tg_gui_session_search_open_result(hit, stdout);
                        tg_gui_window_copy(state->status, sizeof(state->status),
                                           "Live - F1-F10 chats, Q quits");
                        tg_gui_window_paint(state, &backend);
                    } else if (hit >= 0) {
                        /* Press on a chat row: ARM a reorder drag. The open is
                           deferred to SELECTUP -- a press that never crosses the
                           drag threshold opens the chat (click), a press that does
                           reorders the list (drag). drag_src doubles as the flag. */
                        state->drag_src = hit;
                        state->drag_active = 0;
                        state->drag_press_y = hy;
                        state->drag_cur_y = hy;
                    } else if (hit == TG_GUI_HIT_SEARCH &&
                               tg_gui_session_is_open()) {
                        /* Click the sidebar search box to focus it for typing;
                           F8: the caret lands where you clicked. */
                        int sc;

                        state->composing = 0;
                        state->search_active = 1;
                        state->search_dirty = 0; /* no pending debounce on focus */
                        search_idle_ticks = 0;
                        last_key_time = time(0);
                        state->cursor_on = 1;
                        caret_ticks = 0;
                        sc = tg_gui_search_click_caret(state, &backend, hx, hy);
                        state->search_caret =
                            (sc >= 0) ? sc : (int)strlen(state->search_query);
                        tg_gui_window_copy(
                            state->status, sizeof(state->status),
                            "Search: type then PAUSE to auto-find (or ENTER)");
                        tg_gui_window_paint(state, &backend);
                    } else if (hit == TG_GUI_HIT_SEND && state->composing) {
                        state->in_sel_active = 0; /* input is consumed below */
                        if (state->input[0] != '\0') {
                            if (state->edit_to_id != 0UL) {
                                (void)tg_gui_session_edit(state->input,
                                                          state->edit_to_id,
                                                          stdout);
                                state->edit_to_id = 0UL;
                            } else if (tg_gui_session_send(state->input,
                                                           state->reply_to_id,
                                                           stdout) == 0) {
                                tg_gui_history_add(state, state->input);
                                state->reply_to_id = 0UL; /* clear on success */
                                state->reply_sender[0] = '\0';
                                state->reply_snippet[0] = '\0';
                                tg_gui_window_jump_to_bottom(state, &backend,
                                                             &older_exhausted,
                                                             &older_cooldown);
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
                    } else if (hit == TG_GUI_HIT_INPUT && state->composing) {
                        /* F8: click places the caret in the composer text. */
                        int cc = tg_gui_input_click_caret(state, &backend, hx,
                                                          hy);

                        if (cc >= 0) {
                            state->in_sel_active = 0;
                            state->in_drag_armed = 1;
                            state->in_drag_anchor = cc;
                            state->input_caret = cc;
                            state->cursor_on = 1;
                            caret_ticks = 0;
                            tg_gui_window_mention_refresh(state);
                            tg_gui_window_paint(state, &backend);
                        }
                    } else if ((hit == TG_GUI_HIT_INPUT ||
                                hit == TG_GUI_HIT_SEND) &&
                               !state->composing && tg_gui_session_is_open()) {
                        /* Click the input field (or Send) to start composing --
                           leave the search box so only one caret is focused. */
                        state->search_active = 0;
                        state->search_query[0] = '\0';
                        if (state->in_search) { /* abandon the picker, restore chats */
                            state->in_search = 0;
                            tg_gui_session_refresh_chats();
                        }
                        state->composing = 1;
                        state->in_sel_active = 0; /* fresh focus, no ghosts */
                        state->input_caret = (int)strlen(state->input);
                        if (hit == TG_GUI_HIT_INPUT) { /* F8: caret at click */
                            int cc = tg_gui_input_click_caret(state, &backend,
                                                              hx, hy);

                            if (cc >= 0) {
                                state->in_sel_active = 0;
                                state->in_drag_armed = 1;
                                state->in_drag_anchor = cc;
                                state->input_caret = cc;
                            }
                        }
                        state->cursor_on = 1;
                        caret_ticks = 0;
                        tg_gui_window_copy(state->status, sizeof(state->status),
                               "Type - ENTER sends, ESC cancels");
                        tg_gui_window_paint(state, &backend);
                    } else if (hit == TG_GUI_HIT_REPLY_CANCEL) {
                        /* The composer's reply header "X": drop the reply target
                           (composer shrinks, transcript grows back). */
                        state->reply_to_id = 0UL;
                        state->reply_sender[0] = '\0';
                        state->reply_snippet[0] = '\0';
                        tg_gui_window_paint(state, &backend);
                    } else if (hit <= TG_GUI_HIT_MESSAGE_BASE &&
                               tg_gui_session_is_open() && !state->in_search) {
                        /* Press on a bubble: LATCH it. A drag past the
                           threshold becomes a text selection; a click that
                           never drags keeps the old gesture (reply), executed
                           on SELECTUP -- same defer pattern as the sidebar. */
                        int mi = TG_GUI_HIT_MESSAGE_BASE - hit;

                        if (mi >= 0 && mi < state->message_count) {
                            const tg_gui_message *m = &state->messages[mi];

                            if (!m->is_system && m->id != 0UL) {
                                int repaint = state->sel_active;

                                state->sel_active = 0; /* new press resets */
                                state->sel_press_armed = 1;
                                state->sel_press_msg = mi;
                                state->sel_press_id = m->id;
                                state->sel_press_gen = state->msg_gen;
                                state->sel_press_x = hx;
                                state->sel_press_y = hy;
                                state->sel_press_char =
                                    tg_gui_transcript_char_at(state, &backend,
                                                              ctx.line_h, mi,
                                                              hx, hy);
                                if (repaint) {
                                    tg_gui_window_paint(state, &backend);
                                }
                            }
                        }
                    }
                }
            } else if (msg_class == IDCMP_MOUSEMOVE) {
                if (state->mode == TG_GUI_MODE_CHAT && state->in_drag_armed &&
                    state->composing) {
                    /* Drag inside the input box: grow the composer selection
                       from the press anchor to the char under the pointer. */
                    int hx = (int)mouse_x - ctx.origin_x;
                    int hy = (int)mouse_y - ctx.origin_y;
                    int cc = tg_gui_input_click_caret(state, &backend, hx, hy);

                    if (cc >= 0) {
                        if (!state->in_sel_active &&
                            cc != state->in_drag_anchor) {
                            state->in_sel_active = 1;
                            state->in_sel_anchor = state->in_drag_anchor;
                        }
                        if (state->in_sel_active &&
                            cc == state->in_sel_anchor &&
                            cc == state->input_caret) {
                            state->in_sel_active = 0; /* collapsed on anchor */
                            tg_gui_window_paint(state, &backend);
                        } else if (state->in_sel_active &&
                            cc != state->input_caret) {
                            state->input_caret = cc;
                            if (cc == state->in_sel_anchor) {
                                state->in_sel_active = 0;
                            }
                            tg_gui_window_paint(state, &backend);
                        }
                    }
                }
                if (state->mode == TG_GUI_MODE_CHAT && state->sel_press_armed) {
                    /* Latched press: past a small threshold it becomes a text
                       selection anchored at the press char; every further move
                       extends it (clamped inside the SAME message by the
                       char-at helper). */
                    int hx = (int)mouse_x - ctx.origin_x;
                    int hy = (int)mouse_y - ctx.origin_y;

                    if (!state->sel_active && state->sel_press_char >= 0 &&
                        state->msg_gen == state->sel_press_gen) {
                        int dx = hx - state->sel_press_x;
                        int dy = hy - state->sel_press_y;

                        if (dx > 2 || dx < -2 || dy > 2 || dy < -2) {
                            state->sel_active = 1;
                            state->sel_msg = state->sel_press_msg;
                            state->sel_a = state->sel_press_char;
                            state->sel_b = state->sel_press_char;
                            state->sel_gen_snap = state->msg_gen;
                        }
                    }
                    if (state->sel_active) {
                        long c = tg_gui_transcript_char_at(
                            state, &backend, ctx.line_h, state->sel_msg, hx,
                            hy);

                        if (c >= 0 && c != state->sel_b) {
                            state->sel_b = c;
                            tg_gui_window_paint(state, &backend);
                        }
                    }
                }
                /* Right-button trap follows the pointer: claimed over a real
                   bubble (our context menu), released elsewhere (menu bar). */
                if (state->mode == TG_GUI_MODE_CHAT && !state->ctx_visible) {
                    int hx = (int)mouse_x - ctx.origin_x;
                    int hy = (int)mouse_y - ctx.origin_y;
                    int hit = tg_gui_hit_test(state, ctx.inner_w, ctx.inner_h,
                                              ctx.line_h, hx, hy);
                    int over_msg = 0;

                    if (hit <= TG_GUI_HIT_MESSAGE_BASE) {
                        int mi = TG_GUI_HIT_MESSAGE_BASE - hit;

                        over_msg = (mi >= 0 && mi < state->message_count &&
                                    !state->messages[mi].is_system &&
                                    state->messages[mi].id != 0UL);
                    }
                    tg_gui_amiga_set_rmbtrap(ctx.window, over_msg);
                }
                /* Context-menu hover: highlight the item under the pointer so the
                   user sees which of Reply/Edit/Delete the click will pick.
                   REPORTMOUSE floods moves, so repaint ONLY when the highlighted
                   item actually changes (crossing an item boundary). */
                if (state->ctx_visible) {
                    int hx = (int)mouse_x - ctx.origin_x;
                    int hy = (int)mouse_y - ctx.origin_y;
                    int hv = tg_gui_context_menu_index(state, ctx.inner_w,
                                                       ctx.inner_h, ctx.line_h,
                                                       hx, hy);

                    if (hv != state->ctx_hover) {
                        state->ctx_hover = hv;
                        tg_gui_window_paint(state, &backend);
                    }
                }
                /* Scrollbar knob drag: Intuition reports moves while a button is
                   held. Map the cursor to a scroll offset; the painter re-clamps
                   and redraws the knob to match. */
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
                            /* Dragged the knob to the very top -> page older
                               (the handler gates on transcript_scroll >= max). */
                            want_older = 1;
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
                } else if (state->drag_src >= 0) {
                    /* Row-reorder drag: promote once the gesture passes the
                       click/drag threshold (row_h/2), then track the cursor so the
                       painter redraws the insertion line. Gated on drag_src>=0 so
                       idle pointer motion (REPORTMOUSE is on) never reorders. */
                    int hy = (int)mouse_y - ctx.origin_y;
                    int thresh = ((2 * ctx.line_h) + 12) / 2;

                    state->drag_cur_y = hy;
                    if (!state->drag_active) {
                        int dy = hy - state->drag_press_y;

                        if (dy < 0) {
                            dy = -dy;
                        }
                        if (dy >= thresh) {
                            state->drag_active = 1;
                        }
                    }
                    if (state->drag_active) {
                        scroll_dirty = 1;
                    }
                }
            }
        }
        /* Load-older paging: a scroll-up reached the top of the transcript. "Top"
           INCLUDES the case where the whole backlog FITS the window (sb_tr_max==0,
           no scrollbar drawn): a wheel/cursor up there still means "load older",
           otherwise those chats could never page back (the reported bug -- the
           user had to shrink the window to make a scrollbar appear). The post-drain
           transcript_scroll is used so a drag/wheel that lands at the top fires in
           one gesture; the painter clamps it to sb_tr_max, so the >= test means
           "at the top". Synchronous fetch, like open_chat; a small cooldown keeps a
           fast wheel from hammering a slow link. */
        {
            int was_fits = (state->sb_tr_max == 0);
            int at_top = was_fits || (state->transcript_scroll >= state->sb_tr_max);
            if (want_older && !older_exhausted && older_cooldown == 0 && at_top &&
                tg_gui_session_is_open()) {
                /* Let the ring drop its newest tail ONLY when the newest is
                   off-screen (a scrollable transcript scrolled to the top); when
                   everything fits, the newest rows are visible -- never evict
                   them (would also lose their read-receipt state). */
                int got = tg_gui_session_load_older(stdout, state->sb_tr_max > 0);
                if (got > 0) {
                    scroll_dirty = 1;     /* new rows above -> repaint with them */
                    older_cooldown = 2;   /* breather before the next page */
                    if (was_fits) {
                        reveal_older = 1; /* fits-case: scroll to show the older */
                    }
                } else if (got == 0) {
                    older_exhausted = 1;  /* confirmed chat start / buffer full */
                    state->more_above = 0; /* nothing older -> drop the forced bar */
                }
                /* got < 0: transient fetch failure -> do NOT latch; retry later. */
            }
        }
        /* Heartbeat fired? Run the SAME poll the INTUITICKS path runs (same
           cadence guard via last_session_poll, same composing-idle deferral),
           then re-arm. This is what keeps reception alive while the window is
           inactive; when it is active the shared time guard prevents any
           double-polling. */
        if (timer_ok && timer_pending &&
            CheckIO((struct IORequest *)timer_req) != 0) {
            (void)WaitIO((struct IORequest *)timer_req);
            timer_pending = 0;
            if (!done && state->mode == TG_GUI_MODE_CHAT) {
                time_t hb_now = time(0);
                unsigned long hb_eff = watch_seconds;

                if (watch_boot_grace && session_boot != (time_t)-1 &&
                    hb_now != (time_t)-1 &&
                    (unsigned long)(hb_now - session_boot) < watch_boot_grace) {
                    hb_eff = watch_boot_seconds;
                }
                if (state->composing && hb_now != (time_t)-1 &&
                    hb_now >= last_receive_drain &&
                    (unsigned long)(hb_now - last_receive_drain) >=
                        TG_GUI_COMPOSE_RECEIVE_SECONDS) {
                    last_receive_drain = hb_now;
                    if (tg_gui_session_receive_pending(stdout)) {
                        session_dirty = 1;
                    }
                }
                if (hb_now != (time_t)-1 &&
                    (unsigned long)(hb_now - last_session_poll) >= hb_eff &&
                    (!state->composing ||
                     (unsigned long)(hb_now - last_key_time) >=
                         TG_GUI_COMPOSE_IDLE_POLL_SECONDS)) {
                    last_session_poll = hb_now;
                    if (tg_gui_session_tick(stdout)) {
                        session_dirty = 1;
                    }
                }
            }
            TG_GUI_TR_NODE(timer_req).io_Command = TR_ADDREQUEST;
            TG_GUI_TR_SECS(timer_req) = TG_GUI_HEARTBEAT_SECS;
            TG_GUI_TR_MICRO(timer_req) = 0;
            SendIO((struct IORequest *)timer_req);
            timer_pending = 1;
        }
        if (session_dirty || scroll_dirty) {
            tg_gui_window_paint(state, &backend);
        }
        /* A fits-window load left the older rows above the pinned-newest view: if
           it now overflows, scroll to the top to reveal them (the paint above
           refreshed sb_tr_max). If it still fits, they are already on screen. */
        if (reveal_older && state->sb_tr_max > 0) {
            state->transcript_scroll = state->sb_tr_max;
            tg_gui_window_paint(state, &backend);
        }
        /* Re-arm paging once the user scrolls back off the top of a SCROLLABLE
           transcript. When everything fits (sb_tr_max==0) there is no off-top to
           return to, so the latch persists until the chat is reopened -- correct,
           because with the tri-state return only a real chat-start sets it (a
           transient fetch failure returns < 0 and never latches). */
        if (state->sb_tr_max > 0 &&
            state->transcript_scroll < state->sb_tr_max) {
            older_exhausted = 0;
        }
        /* A (re)opened chat -- every open path moves selected_chat (F-keys, a row
           click, a search result) -- starts paging fresh, including the fits-case
           where the off-top re-arm above can never fire. */
        if (state->selected_chat != prev_selected) {
            older_exhausted = 0;
            older_cooldown = 0;
            prev_selected = state->selected_chat;
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
    /* Heartbeat teardown: abort any in-flight request BEFORE freeing (a
       firing request after DeleteIORequest is a guaranteed crash). */
    if (timer_ok) {
        if (timer_pending) {
            AbortIO((struct IORequest *)timer_req);
            (void)WaitIO((struct IORequest *)timer_req);
            timer_pending = 0;
        }
        CloseDevice((struct IORequest *)timer_req);
        timer_ok = 0;
    }
    if (timer_req != 0) {
        DeleteIORequest((struct IORequest *)timer_req);
        timer_req = 0;
    }
    if (timer_port != 0) {
        DeleteMsgPort(timer_port);
        timer_port = 0;
    }
    tg_gui_amiga_buffer_free(&ctx); /* free the off-screen double-buffer */
    tg_gui_log("window: releasing pens");
    tg_gui_amiga_release_pens(&ctx, cmap);
    tg_gui_window_save_geom(ctx.inner_w, ctx.inner_h,
                            (int)ctx.window->LeftEdge,
                            (int)ctx.window->TopEdge, want_own);
    tg_gui_log("window: CloseWindow");
    CloseWindow(ctx.window);
    ctx.window = 0;
    if (own_scr != 0) {
        /* Private screen, our window was the only one -> CloseScreen succeeds
           deterministically; the bounded retry is purely defensive (leak
           rather than hang if something impossible keeps it open). */
        int scr_tries = 0;

        while (CloseScreen(own_scr) == FALSE && scr_tries < 10) {
            Delay(10);
            ++scr_tries;
        }
        if (scr_tries >= 10) {
            puts("gui window: own screen close blocked (leaked)");
        }
        own_scr = 0;
    }
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
    /* 2 = iconified (park on AppIcon), 3 = reopen (own-screen toggle). */
    return (done == 2) ? 2 : (done == 3) ? 3 : 0;
}

/* Iconified park: an AppIcon on Workbench (our own shipped icon when
   loadable, the default tool icon otherwise); a double-click -- or a
   Ctrl-C break -- wakes us. Returns 1 to reopen the window, 0 to quit.
   Everything is opened lazily and torn down before returning, so the
   iconified footprint is just this task, its stack and the AppIcon. */
static int tg_gui_window_iconify_wait(void)
{
    struct MsgPort *port = 0;
    struct DiskObject *dobj = 0;
    struct AppIcon *icon = 0;
    struct Message *m;
    int reopen = 0;

    WorkbenchBase = OpenLibrary((CONST_STRPTR)"workbench.library", 36L);
    IconBase = OpenLibrary((CONST_STRPTR)"icon.library", 36L);
#if defined(__amigaos4__)
    if (WorkbenchBase != 0) {
        IWorkbench = (struct WorkbenchIFace *)GetInterface(WorkbenchBase,
                                                           "main", 1L, 0);
    }
    if (IconBase != 0) {
        IIcon = (struct IconIFace *)GetInterface(IconBase, "main", 1L, 0);
    }
    if (IWorkbench == 0 || IIcon == 0) {
        goto out;
    }
#endif
    if (WorkbenchBase == 0 || IconBase == 0) {
        goto out;
    }
    port = CreateMsgPort();
    if (port == 0) {
        goto out;
    }
    /* Our own program icon (TelegramAmiga.info) so the AppIcon shows the
       Telegram image, not a generic tool icon. The pre-0.0.6 name was
       "TelegramGUI"; after the rename that file no longer ships, so this used to
       miss and fall through to GetDefDiskObject (the generic icon seen on OS4). */
    dobj = GetDiskObject((STRPTR)"PROGDIR:TelegramAmiga");
    if (dobj == 0) {
        dobj = GetDefDiskObject(WBTOOL);
    }
    if (dobj == 0) {
        goto out;
    }
    dobj->do_CurrentX = NO_ICON_POSITION;
    dobj->do_CurrentY = NO_ICON_POSITION;
    icon = AddAppIcon(0UL, 0UL, (STRPTR)"TelegramAmiga", port, 0, dobj,
                      TAG_DONE);
    if (icon != 0) {
        ULONG sigs = Wait((1UL << port->mp_SigBit) | SIGBREAKF_CTRL_C);

        reopen = (sigs & (1UL << port->mp_SigBit)) != 0UL;
        while ((m = GetMsg(port)) != 0) {
            ReplyMsg(m);
        }
        RemoveAppIcon(icon);
        while ((m = GetMsg(port)) != 0) {
            ReplyMsg(m);
        }
    }
out:
    if (dobj != 0) {
        FreeDiskObject(dobj);
    }
    if (port != 0) {
        DeleteMsgPort(port);
    }
#if defined(__amigaos4__)
    if (IIcon != 0) {
        DropInterface((struct Interface *)IIcon);
        IIcon = 0;
    }
    if (IWorkbench != 0) {
        DropInterface((struct Interface *)IWorkbench);
        IWorkbench = 0;
    }
#endif
    if (IconBase != 0) {
        CloseLibrary(IconBase);
        IconBase = 0;
    }
    if (WorkbenchBase != 0) {
        CloseLibrary(WorkbenchBase);
        WorkbenchBase = 0;
    }
    return reopen;
}

int tg_gui_run_window(tg_gui_state *state)
{
    for (;;) {
        int rc = tg_gui_run_window_once(state);

        if (rc == 3) {
            continue; /* own-screen toggle: reopen immediately, no AppIcon */
        }
        if (rc != 2) {
            return rc;
        }
        if (!tg_gui_window_iconify_wait()) {
            return 0; /* AppIcon failed or Ctrl-C: a clean quit */
        }
        /* Double-click: fall through and reopen the window fresh. */
    }
}

#else /* !TG_GUI_AMIGA: host build */

int tg_gui_run_window(tg_gui_state *state)
{
    (void)state;
    puts("gui window: native window not available on this build; "
         "use --gui-self-test for the layout check.");
    return 0;
}

void tg_gui_window_avatar_invalidate(unsigned long id_hi, unsigned long id_lo)
{
    (void)id_hi; /* no native window: nothing cached to drop */
    (void)id_lo;
}

#endif
