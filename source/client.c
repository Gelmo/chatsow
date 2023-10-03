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
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "config.h"
#include "global.h"
#include "utils.h"
#include "net.h"
#include "parser.h"
#include "cmd.h"
#include "client.h"

#define TIMEOUT 1800

#define INACTIVE_TIME 4000

#define COMMAND_BUFFER 32

typedef enum client_state_e {
    CA_DISCONNECTED,
    CA_SETUP,
    CA_CHALLENGING,
    CA_CONNECTING,
    CA_CONNECTED,
    CA_LOADING,
    CA_CONFIGURING,
    CA_ENTERING,
    CA_ACTIVE
} client_state_t;

typedef struct command_buffer_s {
    msg_t msg;
    int last_seq;
    unsigned int last_send;
} command_buffer_t;

typedef struct client_s {
    int id;

    client_state_t state;

    parser_t parser;
    cs_t cs;

    sock_t sock;

    char host[512];
    char port[512];
    char name[512];
    char password[512];
    int port_int;
    char challenge[512];
    int tvserver;
    int multiview;
    int auto_reconnect;

    int command_seq;

    unsigned int resend;
    unsigned int last_send;

    int lastframe;
    unsigned int server_time;

    int bitflags;
    char baseurl;
    int protocol;
    int spawn_count;
    int playernum;
    char motd[MAX_STRING_CHARS];
    char game[MAX_STRING_CHARS];
    char level[MAX_STRING_CHARS];

    int demo;

    command_buffer_t buffers[COMMAND_BUFFER];
    int buffer_count;

    short stats[MAX_CLIENTS][PS_MAX_STATS];
} client_t;

static client_t clients[CLIENT_SCREENS];

const char *profanitylist[388] = {
    "arschficker",
    "ass bandit",
    "ass banger",
    "ass cowboy",
    "ass fucker",
    "ass fukka",
    "ass goblin",
    "ass hopper",
    "ass jacker",
    "ass jockey",
    "ass lover",
    "ass packer",
    "ass pirate",
    "ass ranger",
    "ass-bandit",
    "ass-banger",
    "ass-cowboy",
    "ass-fucker",
    "ass-fukka",
    "ass-goblin",
    "ass-hopper",
    "ass-jacker",
    "ass-jockey",
    "ass-lover",
    "ass-packer",
    "ass-pirate",
    "ass-ranger",
    "assbandit",
    "assbanger",
    "asscowboy",
    "assfucker",
    "assfukka",
    "assgoblin",
    "asshopper",
    "assjacker",
    "assjockey",
    "asslover",
    "asspacker",
    "asspirate",
    "assranger",
    "backdoor man",
    "backdoor-man",
    "backdoorman",
    "bum boy",
    "bum-boy",
    "bumboy",
    "butt bandit",
    "butt banger",
    "butt cowboy",
    "butt fucker",
    "butt fukka",
    "butt goblin",
    "butt hopper",
    "butt jacker",
    "butt jockey",
    "butt lover",
    "butt packer",
    "butt pirate",
    "butt ranger",
    "butt-bandit",
    "butt-banger",
    "butt-cowboy",
    "butt-fucker",
    "butt-fukka",
    "butt-goblin",
    "butt-hopper",
    "butt-jacker",
    "butt-jockey",
    "butt-lover",
    "butt-packer",
    "butt-pirate",
    "butt-ranger",
    "buttbandit",
    "buttbanger",
    "buttcowboy",
    "buttfucker",
    "buttfukka",
    "buttgoblin",
    "butthopper",
    "buttjacker",
    "buttjockey",
    "buttlover",
    "buttpacker",
    "buttpirate",
    "buttranger",
    "camel jockey",
    "camel-jockey",
    "cameljockey",
    "cock bandit",
    "cock banger",
    "cock cowboy",
    "cock fucker",
    "cock fukka",
    "cock goblin",
    "cock hopper",
    "cock jacker",
    "cock jockey",
    "cock lover",
    "cock packer",
    "cock pirate",
    "cock ranger",
    "cock-bandit",
    "cock-banger",
    "cock-cowboy",
    "cock-fucker",
    "cock-fukka",
    "cock-goblin",
    "cock-hopper",
    "cock-jacker",
    "cock-jockey",
    "cock-lover",
    "cock-packer",
    "cock-pirate",
    "cock-ranger",
    "cockbandit",
    "cockbanger",
    "cockcowboy",
    "cockfucker",
    "cockfukka",
    "cockgoblin",
    "cockhopper",
    "cockjacker",
    "cockjockey",
    "cocklover",
    "cockpacker",
    "cockpirate",
    "cockranger",
    "cok bandit",
    "cok banger",
    "cok cowboy",
    "cok fucker",
    "cok fukka",
    "cok goblin",
    "cok hopper",
    "cok jacker",
    "cok jockey",
    "cok lover",
    "cok packer",
    "cok pirate",
    "cok ranger",
    "cok-bandit",
    "cok-banger",
    "cok-cowboy",
    "cok-fucker",
    "cok-fukka",
    "cok-goblin",
    "cok-hopper",
    "cok-jacker",
    "cok-jockey",
    "cok-lover",
    "cok-packer",
    "cok-pirate",
    "cok-ranger",
    "cokbandit",
    "cokbanger",
    "cokcowboy",
    "cokfucker",
    "cokfukka",
    "cokgoblin",
    "cokhopper",
    "cokjacker",
    "cokjockey",
    "coklover",
    "cokpacker",
    "cokpirate",
    "cokranger",
    "cunt",
    "dick bandit",
    "dick banger",
    "dick cowboy",
    "dick fucker",
    "dick fukka",
    "dick goblin",
    "dick hopper",
    "dick jacker",
    "dick jockey",
    "dick lover",
    "dick packer",
    "dick pirate",
    "dick ranger",
    "dick-bandit",
    "dick-banger",
    "dick-cowboy",
    "dick-fucker",
    "dick-fukka",
    "dick-goblin",
    "dick-hopper",
    "dick-jacker",
    "dick-jockey",
    "dick-lover",
    "dick-packer",
    "dick-pirate",
    "dick-ranger",
    "dickbandit",
    "dickbanger",
    "dickcowboy",
    "dickfucker",
    "dickfukka",
    "dickgoblin",
    "dickhopper",
    "dickjacker",
    "dickjockey",
    "dicklover",
    "dickpacker",
    "dickpirate",
    "dickranger",
    "dik bandit",
    "dik banger",
    "dik cowboy",
    "dik fucker",
    "dik fukka",
    "dik goblin",
    "dik hopper",
    "dik jacker",
    "dik jockey",
    "dik lover",
    "dik packer",
    "dik pirate",
    "dik ranger",
    "dik-bandit",
    "dik-banger",
    "dik-cowboy",
    "dik-fucker",
    "dik-fukka",
    "dik-goblin",
    "dik-hopper",
    "dik-jacker",
    "dik-jockey",
    "dik-lover",
    "dik-packer",
    "dik-pirate",
    "dik-ranger",
    "dikbandit",
    "dikbanger",
    "dikcowboy",
    "dikfucker",
    "dikfukka",
    "dikgoblin",
    "dikhopper",
    "dikjacker",
    "dikjockey",
    "diklover",
    "dikpacker",
    "dikpirate",
    "dikranger",
    "dyke",
    "fag",
    "faggot",
    "fagot",
    "fairy",
    "fudge packer",
    "fudge-packer",
    "fudgepacker",
    "gayass",
    "gaybob",
    "gaydo",
    "gayfuck",
    "gaytard",
    "gaywad",
    "gender bender",
    "gender-bender",
    "genderbender",
    "ghay",
    "ghey",
    "higg@",
    "higg3r",
    "higga",
    "higger",
    "jigaboo",
    "jigga",
    "jigger",
    "jiggs",
    "jiggyboo",
    "jigro",
    "kigger",
    "kike",
    "kneegrow",
    "kyke",
    "ligger",
    "n1ckker",
    "n1g3r",
    "n1gg@",
    "n1gg3r",
    "n1gga",
    "n1gger",
    "n1ggu",
    "n3gro",
    "negga",
    "negr0",
    "negroes",
    "negroid",
    "niccer",
    "nicka",
    "nicker",
    "nickk3r",
    "nickker",
    "nig lover",
    "nig nog",
    "nig-lover",
    "nig-nog",
    "nigaboo",
    "nigah",
    "nigasses",
    "nigers",
    "nigg@",
    "nigg3r",
    "nigg4h",
    "nigga",
    "nigger",
    "niggir",
    "niggle",
    "niggling",
    "niggor",
    "niggress",
    "nigguh",
    "niggur",
    "niglet",
    "niglover",
    "nignig",
    "nignog",
    "nigra",
    "nigre",
    "nigs",
    "niguh",
    "nikk3r",
    "nikkas",
    "nikker",
    "nlgger",
    "nlggor",
    "nuckas",
    "penis bandit",
    "penis banger",
    "penis cowboy",
    "penis fucker",
    "penis fukka",
    "penis goblin",
    "penis hopper",
    "penis jacker",
    "penis jockey",
    "penis lover",
    "penis packer",
    "penis pirate",
    "penis ranger",
    "penis-bandit",
    "penis-banger",
    "penis-cowboy",
    "penis-fucker",
    "penis-fukka",
    "penis-goblin",
    "penis-hopper",
    "penis-jacker",
    "penis-jockey",
    "penis-lover",
    "penis-packer",
    "penis-pirate",
    "penis-ranger",
    "penisbandit",
    "penisbanger",
    "peniscowboy",
    "penisfucker",
    "penisfukka",
    "penisgoblin",
    "penishopper",
    "penisjacker",
    "penisjockey",
    "penislover",
    "penispacker",
    "penispirate",
    "penisranger",
    "r-tard",
    "reetard",
    "retard",
    "rtard",
    "rump rammer",
    "rump-rammer",
    "rumprammer",
    "she male",
    "she-male",
    "shemale",
    "spick",
    "trannie",
    "tranny",
    "transvestite",
    "whigger",
    "white power",
    "white-power",
    "whitepower",
    "wigger"
};

static void reset(client_t *c) {
    c->id = c - clients;

    c->parser.client = c->id;

    c->command_seq = 1;

    c->resend = 0;
    c->last_send = 0;

    c->lastframe = -1;
    c->server_time = 0;

    c->protocol = 0;
    c->bitflags = 0;
    c->spawn_count = 0;
    c->playernum = 0;
    c->motd[0] = '\0';
    c->game[0] = '\0';
    c->level[0] = '\0';

    c->demo = -1;

    c->buffer_count = 0;

    int i;
    int j;
    for (i = 0; i < MAX_CLIENTS; i++) {
        for (j = 0; j < PS_MAX_STATS; j++)
            c->stats[i][j] = 0;
    }

    parser_reset(&c->parser);
    cs_init(&c->cs);
    sock_init(&c->sock);

    set_status(c->id, c->name, cs_get(&c->cs, 0));
}

void register_configstring_commands(client_t *c) {
    int max = CS_GAMECOMMANDS + MAX_GAMECOMMANDS;
    int i;
    for (i = CS_GAMECOMMANDS; i < max; i++)
        cmd_add_server(c->id, cs_get(&c->cs, i));
}

cs_t *client_cs(int id) {
    return &clients[id].cs;
}

int client_record(int id, FILE *fp, int target, void (*save)(int id, int client, int target, qboolean terminated)) {
    return parser_record(&clients[id].parser, fp, target, save);
}

void client_stop_record(int id, int demo_id) {
    parser_stop_record(&clients[id].parser, demo_id);
}

void client_terminate_record(int id, int demo_id) {
    parser_terminate_record(&clients[id].parser, demo_id);
}

qboolean client_active(int id) {
    return clients[id].state > CA_DISCONNECTED;
}

qboolean client_ready(int id) {
    return clients[id].state == CA_ACTIVE;
}

int get_bitflags(int id) {
    client_t *c = clients + id;
    return c->bitflags;
}

int get_playernum(int id) {
    return clients[id].playernum;
}

void set_spawn_count(int id, int new_spawn_count) {
    clients[id].spawn_count = new_spawn_count;
}

void set_protocol(int id, int new_protocol) {
    clients[id].protocol = new_protocol;
}

void set_game(int id, char *new_game) {
    strcpy(clients[id].game, new_game);
}

void set_playernum(int id, int new_playernum) {
    clients[id].playernum = new_playernum;
}

char *get_level(int id) {
    return clients[id].level;
}

void set_level(int id, char *new_level) {
    strcpy(clients[id].level, new_level);
}

void set_bitflags(int id, int new_bitflags) {
    clients[id].bitflags = new_bitflags;
}

void set_baseurl(int id, char new_baseurl) {
    clients[id].baseurl = new_baseurl;
}

void set_stat(int id, int player, int index, short value) {
    clients[id].stats[player][index] = value;
}

short get_stat(int id, int player, int index) {
    return clients[id].stats[player][index];
}

short *get_stats(int id) {
    return &clients[id].stats[0][0];
}

char *get_host(int id) {
    return clients[id].host;
}

static void client_title(client_t *c) {
    set_title(c->id, c->state == CA_DISCONNECTED ? "Disconnected" : c->motd, c->level, c->game, c->host, c->port);
}

static void set_state(client_t *c, int new_state) {
    c->state = new_state;
    c->resend = 0;
}

static void force_disconnect(client_t *c) {
    if (c->state == CA_DISCONNECTED)
        return;

    sock_disconnect(&c->sock);
    set_state(c, CA_DISCONNECTED);
    reset(c);
    client_title(c);
}

int get_port(int id) {
    return clients[id].port_int;
}

static void socket_connect(client_t *c) {
    if (c->state != CA_DISCONNECTED)
        force_disconnect(c);

    sock_connect(&c->sock, c->host, c->port_int);

    ui_output(c->id, "Connecting to %s:%s...\n", c->host, c->port);
    set_state(c, CA_SETUP);
}

static void challenge(client_t *c);

static void client_connect(client_t *c) {
    socket_connect(c);
    if (c->state == CA_DISCONNECTED)
        return;
    challenge(c);
}

void disconnect(int id) {
    client_t *c = clients + id;
    if (c->state == CA_DISCONNECTED)
        return;

    ui_output(id, "Disconnecting...\n");
    if (c->state >= CA_CONNECTING) {
        int i;
        for (i = 0; i < CERTAINTY; i++)
            client_command(id, "disconnect");
    }
    force_disconnect(c);
}

static void force_reconnect(client_t *c) {
    force_disconnect(c);
    client_connect(c);
}

static void reconnect(client_t *c) {
    disconnect(c->id);
    client_connect(c);
}

static void client_send(client_t *c) {
    sock_send(&c->sock);
    c->last_send = millis();
}

static char string[MAX_MSGLEN];

void client_say(int id, char *format, ...) {
    strcpy(string, "say \"");
    va_list    argptr;
    va_start(argptr, format);
    vsprintf(string + strlen(string), format, argptr);
    va_end(argptr);
    strcat(string, "\"");
    cmd_execute(id, string);
}

void client_say_team(int id, char *format, ...) {
    strcpy(string, "say_team \"");
    va_list    argptr;
    va_start(argptr, format);
    vsprintf(string + strlen(string), format, argptr);
    va_end(argptr);
    strcat(string, "\"");
    cmd_execute(id, string);
}

static void buffer_send(client_t *c, command_buffer_t *buffer) {
    msg_t *msg = sock_init_send(&c->sock, qtrue);
    msg_copy(msg, &buffer->msg);
    client_send(c);
    buffer->last_send = millis();
}

static void client_resend(client_t *c) {
    if (c->buffer_count > 0 && millis() >= c->buffers[0].last_send + TIMEOUT)
        buffer_send(c, c->buffers);
}

static void buffer(client_t *c, msg_t *original_msg, int seq) {
    if (c->bitflags & SV_BITFLAGS_RELIABLE) {
        msg_t *msg = sock_init_send(&c->sock, qtrue);
        msg_copy(msg, original_msg);
        client_send(c);
    } else {
        if (c->buffer_count == COMMAND_BUFFER)
            client_get_ack(c->id, c->buffers[0].last_seq);
        command_buffer_t *buffer = c->buffers + c->buffer_count++;
        buffer->msg = *original_msg;
        buffer->last_seq = seq;
        buffer->last_send = -TIMEOUT;
        if (buffer == c->buffers)
            buffer_send(c, buffer);
    }
}

void client_command(int id, char *format, ...) {
    client_t *c = clients + id;
    if (c->state < CA_SETUP) {
        ui_output(id, "not connected\n");
        return;
    }

    static msg_t msg;
    msg_clear(&msg);
    write_byte(&msg, clc_clientcommand);
    int seq = c->command_seq++;
    if (!(c->bitflags & SV_BITFLAGS_RELIABLE))
        write_long(&msg, seq);
    va_list    argptr;
    va_start(argptr, format);
    vwrite_string(&msg, format, argptr);
    va_end(argptr);
    buffer(c, &msg, seq);
}

void client_ack(int id, int num) {
    client_t *c = clients + id;
    msg_t *msg = sock_init_send(&c->sock, qtrue);
    write_byte(msg, clc_svcack);
    write_long(msg, num);
    client_send(c);
}

static void add_move(client_t *c, msg_t *msg) {
    write_byte(msg, clc_move);
    write_long(msg, c->lastframe);
    write_long(msg, 2);
    write_byte(msg, 1);
    write_byte(msg, 0);
    write_long(msg, c->server_time);
}

void client_ack_frame(int id, int lastframe, unsigned int server_time) {
    client_t *c = clients + id;
    c->lastframe = lastframe;
    c->server_time = server_time;
    msg_t *msg = sock_init_send(&c->sock, qtrue);
    add_move(c, msg);
    client_send(c);
}

void client_get_ack(int id, int ack) {
    client_t *c = clients + id;
    if (c->buffer_count > 0 && c->buffers[0].last_seq == ack) {
        c->buffer_count--;
        int i;
        for (i = 0; i < c->buffer_count; i++)
            c->buffers[i] = c->buffers[i + 1];
        client_resend(c);
    }
}

static void client_recv(client_t *c) {
    msg_t *msg = sock_recv(&c->sock);
    if (msg) {
        if (c->sock.sequenced)
            parse_message(&c->parser, msg);
        else
            cmd_execute_from_server(c->id, read_string(msg));
    }
}

static void connection_request(client_t *c) {
    ui_output(c->id, "Sending connection request...\n");
    msg_t *msg = sock_init_send(&c->sock, qfalse);
    write_string(msg, "connect %d %s %s \"\\name\\%s\\password\\%s\" %d", PROTOCOL, c->port, c->challenge, c->name, c->password, c->tvserver);
    client_send(c);

    set_state(c, CA_CONNECTING);
    c->resend = millis() + TIMEOUT;
}

static void challenge(client_t *c) {
    ui_output(c->id, "Requesting challenge...\n");
    msg_t *msg = sock_init_send(&c->sock, qfalse);
    write_string(msg, "getchallenge");
    client_send(c);

    set_state(c, CA_CHALLENGING);
    c->resend = millis() + TIMEOUT;
}

static void enter(client_t *c) {
    ui_output(c->id, "Entering the game...\n");
    client_command(c->id, "begin %d", c->spawn_count);
    if (c->multiview)
        client_command(c->id, "multiview 1");
    set_state(c, CA_ENTERING);
}

static void request_serverdata(client_t *c) {
    ui_output(c->id, "Sending serverdata request...\n");
    client_command(c->id, "new");
    set_state(c, CA_LOADING);
}

void client_frame(int id) {
    client_t *c = clients + id;

    if (c->state == CA_DISCONNECTED)
        return;

    client_recv(c);
    client_resend(c);
    switch (c->state) {
        case CA_CHALLENGING:
            if (millis() >= c->resend)
                challenge(c);
        break;
        case CA_CONNECTING:
            if (millis() >= c->resend)
                connection_request(c);
        break;
        case CA_LOADING:
            if (c->playernum != 0) {
                cs_init(&c->cs);
                ui_output(c->id, "Requesting configstrings...\n");
                client_command(c->id, "configstrings %d 0", c->spawn_count);
                set_state(c, CA_CONFIGURING);
            }
            break;
        case CA_ACTIVE:
            if (millis() >= c->last_send + INACTIVE_TIME) {
                msg_t *msg = sock_init_send(&c->sock, qtrue);
                add_move(c, msg);
                client_send(c);
            }
            break;
        default:
            break;
    }
}

static void set_server(client_t *c, char *new_host, char *new_port) {
    disconnect(c->id);
    if (new_host == NULL || new_port == NULL) {
        strcpy(c->host, "");
        strcpy(c->port, "");
        return;
    }
    strcpy(c->host, new_host);
    if (!strcmp(c->host, "localhost"))
        strcpy(c->host, "127.0.0.1");
    strcpy(c->port, new_port);
    c->port_int = atoi(c->port);
    client_connect(c);
    client_title(c);
}

void cmd_challenge() {
    client_t *c = clients + cmd_client();
    if (c->state != CA_CHALLENGING)
        return;

    strcpy(c->challenge, cmd_argv(1));
    connection_request(c);
}

void cmd_client_connect() {
    client_t *c = clients + cmd_client();
    request_serverdata(c);
}

void cmd_cs() {
    client_t *c = clients + cmd_client();
    int i;
    for (i = 0; i < cmd_argc(); i++) {
        if (i % 2 == 1)
            cs_set(&c->cs, atoi(cmd_argv(i)), cmd_argv(i + 1));
    }
    if (c->state > CA_DISCONNECTED) {
        strcpy(c->name, player_name(&c->cs, c->playernum));
        set_status(c->id, c->name, cs_get(&c->cs, 0));
    }
}

void cmd_cmd() {
    client_t *c = clients + cmd_client();
    client_command(c->id, "%s", cmd_args(1));
    c->resend = millis() + TIMEOUT;
}

void cmd_clc() {
    client_t *c = clients + cmd_client();
    msg_t *msg = sock_init_send(&c->sock, qfalse);
    write_string(msg, cmd_args(1));
    client_send(c);
}

void cmd_record() {
    client_t *c = clients + cmd_client();
    if (c->demo < 0) {
        char *name = path("demos/%s.wd%d", cmd_argv(1), PROTOCOL);
        ui_output(c->id, "Recording to %s\n", name);
        FILE *fp = fopen(name, "w");
        c->demo = parser_record(&c->parser, fp, cmd_argc() > 2 ? atoi(cmd_argv(2)) : -1, NULL);
    }
}

void cmd_stop() {
    client_t *c = clients + cmd_client();
    if (c->demo >= 0)
        parser_stop_record(&c->parser, c->demo);
    c->demo = -1;
}

void cmd_precache() {
    client_t *c = clients + cmd_client();
    if (c->state != CA_CONFIGURING || cs_get(&c->cs, 0)[0] == '\0')
        return;

    enter(c);
}

void client_activate(int id) {
    client_t *c = clients + id;
    if (c->state != CA_ENTERING)
        return;

    set_state(c, CA_ACTIVE);
    client_title(c);
    cmd_execute(id, "players");
}

static void cmd_disconnect_server() {
    client_t *c = clients + cmd_client();
    if (c->auto_reconnect)
        reconnect(c);
    else
        disconnect(c->id);
}

static void cmd_disconnect() {
    client_t *c = clients + cmd_client();
    disconnect(c->id);
}

static void cmd_reconnect() {
    client_t *c = clients + cmd_client();
    reconnect(c);
}

static void cmd_force_reconnect() {
    client_t *c = clients + cmd_client();
    force_reconnect(c);
}

static void cmd_nop() {
}

char* strlwr(char* s)
{
    char* tmp = s;
    for (;*tmp;++tmp) {
        *tmp = tolower((unsigned char) *tmp);
    }
    return s;
}

void client_mute(int id, int caller, ...) {
    char callerstr[40];
    char cmdstr[40];
    int newcaller = (caller - 1);
    snprintf(callerstr, sizeof(callerstr), "%d", newcaller);
    cmd_execute(id, "op ReplaceThisWithOpPass");
    strcpy(cmdstr, "opcall mute ");
    strcat(cmdstr, callerstr);
    cmd_execute(id, cmdstr);
    strcpy(cmdstr, "opcall vmute ");
    strcat(cmdstr, callerstr);
    cmd_execute(id, cmdstr);
}

static void chat_command(client_t *c, int caller, char *command) {
    for (int i = 0; i < sizeof(profanitylist)/sizeof(profanitylist[0]); ++i) {
        if (strstr(strlwr(command), profanitylist[i])) {
            client_mute(c->id, caller);
            break;
        }
    }
    if (command[0] == '!')
        cmd_execute_public(c->id, caller, command + 1);
}

static void cmd_pr() {
    client_t *c = clients + cmd_client();
    ui_output(c->id, "%s", cmd_argv(1));
}

static void cmd_print() {
    client_t *c = clients + cmd_client();
    ui_output(c->id, "%s", cmd_args(1));
}

static void cmd_ch() {
    client_t *c = clients + cmd_client();
    char *name = player_name(&c->cs, atoi(cmd_argv(1)));
    if (c->state > CA_DISCONNECTED)
        ui_output_important(c->id, "%s^7: ^2%s\n", name, cmd_argv(2));
    else
        ui_output(c->id, "%s^7: ^2%s\n", name, cmd_argv(2));
    chat_command(c, atoi(cmd_argv(1)), cmd_argv(2));
}

static void cmd_tch() {
    client_t *c = clients + cmd_client();
    char *name = player_name(&c->cs, atoi(cmd_argv(1)));
    if (c->state > CA_DISCONNECTED)
        ui_output_important(c->id, "%s^7: ^3%s\n", name, cmd_argv(2));
    else
        ui_output(c->id, "%s^7: ^3%s\n", name, cmd_argv(2));
    chat_command(c, atoi(cmd_argv(1)), cmd_argv(2));
}

static void cmd_tvch() {
    client_t *c = clients + cmd_client();
    char *name = player_name(&c->cs, atoi(cmd_argv(1)));
    if (c->state > CA_DISCONNECTED)
        ui_output_important(c->id, "[TV]%s^7: ^2%s\n", name, cmd_argv(2));
    else
        ui_output(c->id, "[TV]%s^7: ^2%s\n", name, cmd_argv(2));
    chat_command(c, atoi(cmd_argv(1)), cmd_argv(2));
}

static void cmd_motd() {
    client_t *c = clients + cmd_client();
    strcpy(c->motd, cmd_argv(2));
}

static void cmd_connect() {
    client_t *c = clients + cmd_client();
    char *new_host = cmd_argv(1);
    char *new_port = "44400";
    if (cmd_argc() > 2)
        new_port = cmd_argv(2);
    int i;
    for (i = 0; i < CLIENTS; i++) {
        if (i != cmd_client() && !strcmp(new_host, clients[i].host) && !strcmp(new_port, clients[i].port)) {
            ui_output(cmd_client(), "Already connected to this server (%d)\n", i + 1);
            return;
        }
    }
    set_server(c, new_host, new_port);
}

static void cmd_replay() {
    client_t *c = clients + cmd_client();
    set_server(c, NULL, NULL);
    FILE *fp = fopen(path("demos/%s.wd%d", cmd_argv(1), PROTOCOL), "r");
    if (fp) {
        force_disconnect(c);
        c->playernum = atoi(cmd_argv(2));
        parse_demo(&c->parser, fp);
        reset(c);
    } else {
        ui_output(c->id, "Demo not found\n");
    }
}

static void cvar_name() {
    client_t *c = clients + cmd_client();
    strcpy(c->name, cmd_argv(1));
    set_status(c->id, c->name, cs_get(&c->cs, 0));
    if (c->state >= CA_SETUP)
        client_command(c->id, "usri \"\\name\\%s\"", c->name);
}

static void *cvar_name_get() {
    client_t *c = clients + cmd_client();
    return c->name;
}

static void cvar_password() {
    client_t *c = clients + cmd_client();
    strcpy(c->password, cmd_argv(1));
}

static void *cvar_password_get() {
    client_t *c = clients + cmd_client();
    return c->password;
}

static void cvar_tvserver() {
    client_t *c = clients + cmd_client();
    c->tvserver = atoi(cmd_argv(1));
}

static void *cvar_tvserver_get() {
    client_t *c = clients + cmd_client();
    return &c->tvserver;
}

static void cvar_multiview() {
    client_t *c = clients + cmd_client();
    c->multiview = atoi(cmd_argv(1));
    if (c->state >= CA_ENTERING)
        client_command(c->id, "multiview %d", c->multiview);
}

static void *cvar_multiview_get() {
    client_t *c = clients + cmd_client();
    return &c->multiview;
}

static void cvar_auto_reconnect() {
    client_t *c = clients + cmd_client();
    c->auto_reconnect = atoi(cmd_argv(1));
}

static void *cvar_auto_reconnect_get() {
    client_t *c = clients + cmd_client();
    return &c->auto_reconnect;
}

static char suggestions[MAX_CMDS][MAX_SUGGESTION_SIZE];

static qboolean suggestion_remove_test(void *x) {
    char *suggestion = (char *)x;
    int i;
    for (i = 0; suggestions[i] != suggestion; i++) {
        if (!strcmp(suggestion, suggestions[i]))
            return qtrue;
    }
    return qfalse;
}

static void cmd_help_public() {
    static char message[MAX_CMDS * MAX_SUGGESTION_SIZE];
    int suggestion_count = cmd_suggest(cmd_client(), "", -1, suggestions, qtrue);
    qsort(suggestions, suggestion_count, MAX_SUGGESTION_SIZE, insensitive_cmp);
    rm(suggestions, sizeof(suggestions[0]), &suggestion_count, suggestion_remove_test);
    message[0] = '\0';
    int line = 0;
    int i;
    for (i = 0; i < suggestion_count; i++) {
        int len = strlen(suggestions[i]);
        if (line + len > 80) {
            strcat(message, "\n");
            line = 0;
        }
        strcat(message, suggestions[i]);
        strcat(message, " ");
        line += len + 1;
    }
    client_say(cmd_client(), "%s", message);
}

static void cmd_reject() {
    client_t *c = clients + cmd_client();
    if (c->state > CA_CONNECTING)
        return;
    ui_output(c->id, "Rejected: %s %s %s %s %s %s\n", cmd_argv(3), cmd_argv(4), cmd_argv(5), cmd_argv(6), cmd_argv(7), cmd_argv(8));
    if (atoi(cmd_argv(2)) & DROP_FLAG_AUTORECONNECT || c->auto_reconnect)
        reconnect(c);
    else
        disconnect(c->id);
}

void client_register_commands() {
    cmd_add_from_server("challenge", cmd_challenge);
    cmd_add_from_server("client_connect", cmd_client_connect);
    cmd_add_from_server("cs", cmd_cs);
    cmd_add_from_server("cmd", cmd_cmd);
    cmd_add_from_server("precache", cmd_precache);
    cmd_add_from_server("disconnect", cmd_disconnect_server);
    cmd_add_from_server("reject", cmd_reject);
    cmd_add_from_server("forcereconnect", cmd_force_reconnect);
    cmd_add_from_server("reconnect", cmd_reconnect);

    cmd_add_from_server("mm", cmd_nop);
    cmd_add_from_server("plstats", cmd_nop);
    cmd_add_from_server("scb", cmd_nop);
    cmd_add_from_server("cvarinfo", cmd_nop);
    cmd_add_from_server("obry", cmd_nop);
    cmd_add_from_server("ti", cmd_nop);
    cmd_add_from_server("changing", cmd_nop);
    cmd_add_from_server("cp", cmd_nop);
    cmd_add_from_server("aw", cmd_nop);

    cmd_add_from_server("qm", cmd_nop);
    cmd_add_from_server("meop", cmd_nop);

    cmd_add_from_server("dstart", cmd_nop);
    cmd_add_from_server("dstop", cmd_nop);
    cmd_add_from_server("dcancel", cmd_nop);
    cmd_add_from_server("cpc", cmd_nop);
    cmd_add_from_server("cpa", cmd_nop);

    cmd_add_from_server("pr", cmd_pr);
    cmd_add_from_server("print", cmd_print);
    cmd_add_from_server("ch", cmd_ch);
    cmd_add_from_server("tch", cmd_tch);
    cmd_add_from_server("tvch", cmd_tvch);
    cmd_add_from_server("motd", cmd_motd);

    cmd_add_find_free("connect", cmd_connect);
    cmd_add_find_free("replay", cmd_replay);
    cmd_add_cvar("name", cvar_name, cvar_name_get);
    cmd_add_cvar("password", cvar_password, cvar_password_get);
    cmd_add_cvar("tvserver", cvar_tvserver, cvar_tvserver_get);
    cmd_add_cvar("multiview", cvar_multiview, cvar_multiview_get);
    cmd_add_cvar("auto_reconnect", cvar_auto_reconnect, cvar_auto_reconnect_get);
    cmd_add_public_generic("help", cmd_help_public);
}

int player_suggest(int id, char *partial, char suggestions[][MAX_SUGGESTION_SIZE]) {
    int count = 0;
    int i;
    for (i = 1; i <= MAX_CLIENTS; i++) {
        char *name = player_name(&clients[id].cs, i);
        if (name && *name && partial_match(partial, name))
            strcpy(suggestions[count++], name);
    }
    return count;
}

void client_start(int id) {
    client_t *c = clients + id;

    reset(c);
    c->tvserver = 0;
    c->multiview = 0;
    c->auto_reconnect = 0;

    register_configstring_commands(c);
    cmd_add_persistent(id, "reconnect", cmd_reconnect);
    cmd_add(id, "disconnect", cmd_disconnect);
    cmd_add(id, "cmd", cmd_cmd);
    cmd_add(id, "clc", cmd_clc);
    cmd_add(id, "record", cmd_record);
    cmd_add(id, "stop", cmd_stop);

    strcpy(c->name, "chatter");
    strcpy(c->password, "");
    set_state(c, CA_DISCONNECTED);
    set_server(c, NULL, NULL);
    set_status(c->id, c->name, cs_get(&c->cs, 0));
}

void demoinfo_key(int id, char *key) {
    ui_output(id, "demoinfo key %s\n", key);
}

void demoinfo_value(int id, char *value) {
    ui_output(id, "demoinfo value %s\n", value);
}

void execute(int id, char *cmd, qbyte *targets) {
    client_t *c = clients + id;
    if (targets && !(targets[(c->playernum - 1) >> 3] & (1 << ((c->playernum - 1) & 7)))) {
        static char final[MAX_ARGS_SIZE];
        int length = 0;
        int i;
        int numtargets = 0;
        for (i = 0; i < MAX_CLIENTS / 8; i++) {
            if (targets[i])
                numtargets = i + 1;
        }
        length += sprintf(final + length, "external %d", numtargets);
        for (i = 0; i < numtargets; i++)
            length += sprintf(final + length, " %d", targets[i]);
        sprintf(final + length, " %s", cmd);
        cmd_execute_event(id, final);
        return;
    }
    cmd_execute_from_server(id, cmd);
}
