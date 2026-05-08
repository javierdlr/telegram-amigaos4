/*
 * Author: Michele Dipace <michele.dipace@kaffeine.net>
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "tg_platform.h"

const char *tg_platform_name(void)
{
    return "MorphOS";
}

const char *tg_platform_default_data_dir(void)
{
    return "PROGDIR:";
}

void tg_platform_log(const char *level, const char *message)
{
    printf("[morphos:%s] %s\n", level, message);
}

tg_net_status tg_platform_tcp_probe(const char *host, const char *port,
                                    char *error_buffer, unsigned long error_buffer_size)
{
    struct hostent *host_entry;
    struct sockaddr_in address;
    long port_number;
    int rc;
    int sock;

    if (error_buffer != 0 && error_buffer_size > 0) {
        error_buffer[0] = '\0';
    }

    port_number = strtol(port, 0, 10);
    if (port_number <= 0 || port_number > 65535) {
        return TG_NET_INVALID_ARGUMENT;
    }

    host_entry = gethostbyname(host);
    if (host_entry == 0 || host_entry->h_addr_list == 0 || host_entry->h_addr_list[0] == 0) {
        if (error_buffer != 0 && error_buffer_size > 0) {
            strncpy(error_buffer, "host lookup failed", error_buffer_size - 1);
            error_buffer[error_buffer_size - 1] = '\0';
        }
        return TG_NET_RESOLVE_FAILED;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port_number);
    memcpy(&address.sin_addr, host_entry->h_addr_list[0], sizeof(address.sin_addr));

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        if (error_buffer != 0 && error_buffer_size > 0) {
            strncpy(error_buffer, strerror(errno), error_buffer_size - 1);
            error_buffer[error_buffer_size - 1] = '\0';
        }
        return TG_NET_CONNECT_FAILED;
    }

    rc = connect(sock, (struct sockaddr *)&address, sizeof(address));
    close(sock);
    if (rc == 0) {
        return TG_NET_OK;
    }

    if (error_buffer != 0 && error_buffer_size > 0) {
        strncpy(error_buffer, strerror(errno), error_buffer_size - 1);
        error_buffer[error_buffer_size - 1] = '\0';
    }
    return TG_NET_CONNECT_FAILED;
}
