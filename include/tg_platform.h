/*
 * Author: Michele Dipace <michele.dipace@kaffeine.net>
 */

#ifndef TG_PLATFORM_H
#define TG_PLATFORM_H

const char *tg_platform_name(void);
const char *tg_platform_default_data_dir(void);
void tg_platform_log(const char *level, const char *message);

#endif
