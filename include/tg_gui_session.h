/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * Live GUI session bridge (Phase 5b, slice 3). A thin, non-interactive driver
 * over the same MTProto chat core the console uses: it holds an authenticated
 * connection for the lifetime of the GUI window and exposes a non-blocking
 * "tick" the window event loop pumps to harvest cross-chat notifications into
 * the sidebar badges. The implementation lives at the bottom of
 * core/tg_mtproto_probe.c so it reaches that file's static network helpers
 * directly -- no de-static of the network core. See docs/GUI_ARCHITECTURE.md.
 *
 * Single session per process (one window): the state is a file-static
 * singleton; these calls are not re-entrant.
 */

#ifndef TG_GUI_SESSION_H
#define TG_GUI_SESSION_H

#include <stdio.h>

#include "tg_gui.h" /* tg_gui_state */

/* Opens a live session over a saved authorization: derives the production
   endpoint from the auth file's DC, loads the api id, holds an authenticated
   connection, binds the GUI driver to `state`, and projects the current peer
   cache into the sidebar. The caller may refresh the cache from the network
   first (tg_mtproto_gui_refresh_peer_cache). Returns 0 on success, non-zero
   when the session could not be opened (the window can still run read-only).
   `state` must outlive the session. */
int tg_gui_session_open(const char *api_file, const char *auth_file,
                        const char *peer_cache_file, tg_gui_state *state,
                        FILE *stream);

/* One non-blocking poll cycle: a cadence-gated getDifference drain harvests
   inbound messages into the notify queue, which is then dispatched to the GUI
   driver (bumping + flashing the matching sidebar badge). Returns 1 when the
   GUI state changed (the caller should repaint), 0 otherwise. Safe to call
   when no session is open (returns 0). */
int tg_gui_session_tick(FILE *stream);

/* Receive-only live drain for the composer path. It never starts an RPC: when
   the held socket already has an encrypted frame queued, consumes at most one,
   ACKs it and applies typing/read-receipt/edit/notification pushes. Returns 1
   when GUI state changed. Safe to call frequently and with no open session. */
int tg_gui_session_receive_pending(FILE *stream);

/* Upload progress hook: `completed`/`total` are parts; percentage is
   completed*100/total. Runs on the calling task after each confirmed part. */
typedef void (*tg_gui_upload_progress_fn)(unsigned long completed_parts,
                                          unsigned long total_parts,
                                          void *user_data);

/* Download progress + CANCEL hook: called after each received chunk with
   (bytes so far, total). Return non-zero to abort the transfer -- the partial
   file is then removed and the call returns 5. Distinct from the void upload
   hook because a big download blocks the event loop, so it must be cancellable
   (a close-gadget click) or the user has to reset the machine. */
typedef int (*tg_gui_download_progress_fn)(unsigned long done_bytes,
                                           unsigned long total_bytes,
                                           void *user_data);

/* F9: download the document attached to message msg_id in the open chat into
   downloads/<name>. 0 ok (path in out_path), 1 fail, 2 foreign DC (unsupported
   yet), 3 disk error, 5 cancelled. Blocking on-context call -- never from the
   tick. `progress` is optional and runs after each received chunk. */
int tg_gui_session_download_document(unsigned long msg_id, char *out_path,
                                     unsigned long out_path_size, FILE *stream,
                                     tg_gui_download_progress_fn progress,
                                     void *progress_data);

/* F9: send the file at `path` to the open chat. Files over 10 MB use
   upload.saveBigFilePart/inputFileBig. The conservative 4000-part bound gives
   a per-build ceiling of about 31 MiB on m68k and 250 MiB elsewhere.
   0 ok, 1 fail, 2 too big for this build, 3 unreadable. Blocking; never from
   the tick. `progress` is optional and runs after each confirmed part. */
int tg_gui_session_send_document(const char *path, FILE *stream,
                                 tg_gui_upload_progress_fn progress,
                                 void *progress_data);

/* F10 Saved Messages: the sidebar row index that opens the self chat (cloud
   archive) is TG_GUI_SAVED_PEER_INDEX, defined in tg_gui.h (the UI layer needs
   it too). Pinned as the LAST row; remove/reorder skip it. */

/* Opens the chat at the given 1-based peer-cache index: clears the transcript,
   fetches its recent history (incoming + outgoing) into tg_gui_state.messages
   through the GUI driver, and marks it the open chat so tg_gui_session_tick
   streams its new messages live. Returns 1 (always repaint) when a session is
   open, 0 otherwise. */
int tg_gui_session_open_chat(unsigned long peer_index, FILE *stream);

/* Pages OLDER history at the top of the open chat: fetches the getHistory page
   just below the oldest message currently shown and PREPENDS it to the transcript.
   allow_drop_newest = 1 lets a full ring evict its newest tail to make room (only
   safe when those rows are off-screen); 0 keeps them (paging then stops at the
   ring's capacity). Tri-state return: > 0 = older messages added; 0 = server
   confirmed the chat start (no older); < 0 = could not page now (fetch failed /
   nothing pageable) -- the caller must NOT treat < 0 as the chat start. */
int tg_gui_session_load_older(FILE *stream, int allow_drop_newest);

/* Sends `text` to the open chat and echoes it into the transcript. When
   reply_to_msg_id != 0 the message is sent as a reply to that message id.
   Returns 0 on success, non-zero on failure or when no chat is open. */
int tg_gui_session_send(const char *text, unsigned long reply_to_msg_id,
                        FILE *stream);

/* Edits an own message (messages.editMessage) to `text`, and on success updates
   the on-screen bubble in place. Returns 0 ok, non-zero on failure / no chat. */
int tg_gui_session_edit(const char *text, unsigned long message_id,
                        FILE *stream);

/* Deletes a message for everyone (messages./channels.deleteMessages by peer
   type), and on success removes it from the transcript. Returns 0 ok. */
int tg_gui_session_delete(unsigned long message_id, FILE *stream);

/* '@' mention autocomplete: fills up to `max` candidate usernames (no leading
   '@', NUL-terminated, `item_size` bytes apart in `items`) for the open GROUP,
   matching `prefix` case-insensitively against username or title (empty prefix
   = first members). Triggers the same one-shot member fetch the group typing
   names use (supergroups; basic groups have no fetch yet -> 0). Returns the
   count; 0 when no group is open or nothing matches. */
int tg_gui_session_mention_candidates(const char *prefix, char *items,
                                      unsigned long item_size, int max,
                                      FILE *stream);

/* Searches Telegram for `query` (contacts.search), adds the first openable
   result to the peer cache and opens it. Small reply, MorphOS-safe. Returns 0 =
   opened, 1 = no result / network issue, 2 = bad args. */
int tg_gui_session_search_open(const char *query, FILE *stream);

/* Run contacts.search for `query` and KEEP the openable results (does not open
   or touch the cache) so the window can show a picker. Returns the openable
   count (>= 0), -1 on failure. */
int tg_gui_session_search_run(const char *query, FILE *stream);
/* Count / display name of the last search's openable results (0-based). */
int tg_gui_session_search_count(void);
const char *tg_gui_session_search_name(int index);
/* Open the index-th openable result of the last search. 0 = opened. */
int tg_gui_session_search_open_result(int index, FILE *stream);

/* Rebuild the sidebar from the cached chat list (no network) -- restores the
   real list after cancelling the search picker. */
void tg_gui_session_refresh_chats(void);

/* Remove the chat at `peer_index` (the 1-based sidebar number) from the cached
   chat list, persist the change, and reproject the sidebar. Re-addable via search.
   Returns 0 on success, non-zero otherwise. */
int tg_gui_session_remove_chat(unsigned long peer_index, FILE *stream);

/* Move the chat at sidebar row src_index to dst_index (both 1-based, == row + 1),
   persist the new order, reproject the sidebar, and keep the open chat selected.
   No network fetch. Returns 0 on success, non-zero otherwise. */
int tg_gui_session_reorder_chat(unsigned long src_index, unsigned long dst_index,
                                FILE *stream);

/* Persist the sidebar unread badges (cleared on open, incremented live) to the
   chat cache so they survive a restart. Writes only when a count changed. */
void tg_gui_session_persist_unread(void);

/* 1 while a live session is held (so the window can enable composing). */
int tg_gui_session_is_open(void);

/* Closes the held connection and unbinds the notification queue. */
void tg_gui_session_close(void);

/* --- First-login flow (no saved session) -------------------------------- *
 * When there is no telegram-auth.bin yet, the window drives a phone -> code
 * -> (optional) 2FA login through these calls, each a single blocking network
 * round-trip the window wraps with a "Connessione..." status. They reuse the
 * console wizard's headless backend (auth.sendCode / auth.signIn / SRP
 * checkPassword) and persist telegram-auth.bin on success. */

/* Result codes for the step calls below. */
#define TG_GUI_LOGIN_OK           0 /* step accepted; advance */
#define TG_GUI_LOGIN_NEED_2FA     1 /* code accepted, a 2FA password is required */
#define TG_GUI_LOGIN_BAD_CODE     2 /* the login code was rejected -- re-prompt */
#define TG_GUI_LOGIN_BAD_PASSWORD 3 /* the 2FA password was wrong -- re-prompt */
#define TG_GUI_LOGIN_BAD_PHONE    4 /* the number was rejected -- re-prompt */
#define TG_GUI_LOGIN_ERROR        5 /* network/other error -- retry */

/* Stores the file paths the login + the eventual session need. Call once before
   entering a login screen (paths must outlive the session). */
void tg_gui_session_login_begin(const char *api_file, const char *auth_file,
                                const char *peer_cache_file);

/* Requests the login code for `phone` (auth.sendCode, handling PHONE_MIGRATE by
   re-deriving the DC). On TG_GUI_LOGIN_OK the next step is the code. */
int tg_gui_session_login_send_code(const char *phone, FILE *stream);

/* Submits the login `code` (auth.signIn). TG_GUI_LOGIN_OK = logged in (call
   activate); TG_GUI_LOGIN_NEED_2FA = ask for the password; TG_GUI_LOGIN_BAD_CODE
   = re-prompt. */
int tg_gui_session_login_sign_in(const char *code, FILE *stream);

/* Submits the 2FA `password` (SRP auth.checkPassword). TG_GUI_LOGIN_OK = logged
   in (call activate); TG_GUI_LOGIN_BAD_PASSWORD = re-prompt. */
int tg_gui_session_login_check_password(const char *password, FILE *stream);

/* The real reason the last send_code/sign_in failed (the GUI has no console:
   stdout is NIL: on a Workbench launch). Empty string if none captured. */
const char *tg_gui_session_login_last_error(void);

/* After a successful login, brings the window live: refreshes the peer cache,
   opens the session into `state`, sets the title/status and opens the first
   chat, flipping `state->mode` to TG_GUI_MODE_CHAT. Returns 0 on success. */
int tg_gui_session_login_activate(tg_gui_state *state, FILE *stream);

/* Crash-safe diagnostic log to a disk file (tg-gui-debug.log in the CWD), to
   pin down where a hard crash happens. tg_gui_log_enable() turns it on
   (--gui-live-debug); tg_gui_log() writes one flushed line when enabled. */
void tg_gui_log_enable(void);
void tg_gui_log(const char *msg);

#endif
