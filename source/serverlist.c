/*
Copyright (C) 2013 hettoo (Gerco van Heerdt)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <stdio.h>
#include <stdlib.h>

#include "global.h"
#include "utils.h"
#include "ui.h"
#include "net.h"
#include "cmd.h"
#include "serverlist.h"

#define MAX_SERVERS 2048

#define MAX_TOKEN_SIZE 512

#define MAX_PING_RETRIES 4
#define PING_TIMEOUT 1500

typedef struct server_s {
    char address[32];
    int port;

    sock_t sock;
    qboolean received;

    int ping_retries;
    unsigned int ping_start;
    unsigned int ping_end;

    char name[MAX_TOKEN_SIZE];
    char players[MAX_TOKEN_SIZE];
    char map[MAX_TOKEN_SIZE];
    char mod[MAX_TOKEN_SIZE];
    char gametype[MAX_TOKEN_SIZE];
} server_t;

static char filter[MAX_TOKEN_SIZE];

static server_t serverlist[MAX_SERVERS];
static int server_count = 0;
static int output_client;

typedef struct master_s {
    char *address;
    sock_t sock;
} master_t;

static master_t masters[] = {
    {"master1.forbidden.gg"},
    {"master1.icy.gg"},
    {NULL}
};

#define PORT_MASTER 27950

void serverlist_connect() {
    static char cmd[100];
    int i;
    for (i = 1; i < cmd_argc(); i++) {
        char *string = cmd_argv(i);
        int id = atoi(string);
        if (string[0] && id >= 0 && id < server_count) {
            sprintf(cmd, "connect %s %d", serverlist[id].address, serverlist[id].port);
            cmd_execute(cmd_client(), cmd);
        } else {
            ui_output(cmd_client(), "Invalid id: %d.\n", id);
        }
    }
}

int serverlist_connect_complete(int arg, char suggestions[][MAX_SUGGESTION_SIZE]) {
    int count = 0;
    if (arg >= 1) {
        int i;
        for (i = 0; i < server_count; i++) {
            sprintf(suggestions[count], "%d", i);
            if (starts_with(suggestions[count], cmd_argv(1)))
                count++;
        }
    }
    return count;
}

void serverlist_init() {
    master_t *master;
    for (master = masters; master->address; master++) {
        sock_init(&master->sock);
        sock_connect(&master->sock, master->address, PORT_MASTER);
    }
    cmd_add_global("list", serverlist_query);
    int c = cmd_add_global("c", serverlist_connect);
    cmd_complete(c, serverlist_connect_complete);
}

static void ping_server(server_t *server) {
    sock_connect(&server->sock, server->address, server->port);

    msg_t *msg = sock_init_send(&server->sock, qfalse);
    write_string(msg, "info %d full empty", PROTOCOL);
    sock_send(&server->sock);
    server->ping_start = millis();
    server->ping_retries--;
}

void serverlist_query() {
    output_client = cmd_client();
    strcpy(filter, cmd_argv(1));
    int i;
    for (i = 0; i < server_count; i++) {
        serverlist[i].received = qfalse;
        serverlist[i].ping_retries = MAX_PING_RETRIES + 1;
        ping_server(serverlist + i);
    }

    master_t *master;
    for (master = masters; master->address; master++) {
        msg_t *msg = sock_init_send(&master->sock, qfalse);
        write_string(msg, "getservers %s %d full empty", GAME, PROTOCOL);
        sock_send(&master->sock);
    }
}

static server_t *find_server(char *address, int port) {
    int i;
    for (i = 0; i < server_count; i++) {
        if (serverlist[i].port == port && !strcmp(serverlist[i].address, address))
            return serverlist + i;
    }
    return NULL;
}

static void read_server(server_t *server, char *info) {
    int i;
    static char key[MAX_TOKEN_SIZE];
    static char value[MAX_TOKEN_SIZE];
    server->name[0] = '\0';
    server->players[0] = '\0';
    server->map[0] = '\0';
    strcpy(server->mod, BASEMOD);
    server->gametype[0] = '\0';
    qboolean is_key = qtrue;
    key[0] = '\0';
    int len = strlen(info);
    int o = 0;
    for (i = 0; i < len; i++) {
        if (info[i] == '\\' && info[i + 1] == '\\') {
            value[o] = '\0';
            is_key = !is_key;
            if (is_key) {
                strcpy(key, value);
            } else {
                if (!strcmp(key, "n"))
                    strcpy(server->name, value);
                else if (!strcmp(key, "u"))
                    strcpy(server->players, value);
                else if (!strcmp(key, "m"))
                    strcpy(server->map, value);
                else if (!strcmp(key, "mo"))
                    strcpy(server->mod, value);
                else if (!strcmp(key, "g"))
                    strcpy(server->gametype, value);
                key[0] = '\0';
            }
            i++;
            o = 0;
        } else {
            if (o > 0 || info[i] != ' ')
                value[o++] = info[i];
        }
    }
}

void serverlist_frame() {
    int i;
    for (i = 0; i < server_count; i++) {
        msg_t *msg = sock_recv(&serverlist[i].sock);
        if (msg) {
            serverlist[i].ping_end = millis();
            skip_data(msg, strlen("info\n"));
            read_server(serverlist + i, read_string(msg));
            if (partial_match(filter, serverlist[i].name) || partial_match(filter, serverlist[i].map)
                    || partial_match(filter, serverlist[i].mod) || partial_match(filter, serverlist[i].gametype))
                ui_output(output_client, "^5%i ^7(%i) %s %s ^5[^7%s^5] [^7%s:%s^5]\n", i, serverlist[i].ping_end - serverlist[i].ping_start,
                        serverlist[i].players, serverlist[i].name, serverlist[i].map, serverlist[i].mod, serverlist[i].gametype);
            serverlist[i].received = qtrue;
        }
        if (serverlist[i].ping_retries > 0
                && !serverlist[i].received && millis() >= serverlist[i].ping_start + PING_TIMEOUT)
            ping_server(serverlist + i);
    }

    master_t *master;
    for (master = masters; master->address; master++) {
        msg_t *msg = sock_recv(&master->sock);
        if (!msg)
            continue;

        char address_string[32];
        qbyte address[4];
        unsigned short port;

        skip_data(msg, strlen("getserversResponse"));
        while (msg->readcount + 7 <= msg->cursize) {
            char prefix = read_char(msg);
            port = 0;

            if (prefix == '\\') {
                read_data(msg, address, 4);
                port = ShortSwap(read_short(msg));
                sprintf(address_string, "%u.%u.%u.%u", address[0], address[1], address[2], address[3]);
            }

            if (port != 0) {
                server_t *server = find_server(address_string, port);
                if (server != NULL)
                    continue;
                server = serverlist + server_count++;
                sock_init(&server->sock);
                strcpy(server->address, address_string);
                server->port = port;
                server->received = qfalse;
                server->ping_retries = MAX_PING_RETRIES + 1;
                ping_server(server);
            }
        }
    }
}
