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

#ifndef WRLC_PARSER_H
#define WRLC_PARSER_H

#include <stdio.h>
#include "zlib.h"

#include "import.h"

#define MAX_DEMOS 512

typedef struct demo_s {
    FILE *fp;
    int target;
    long int start;
    unsigned int start_time;
    qboolean waiting;
    qboolean finishing;
    void (*save)(int id, int client, int target, qboolean terminated);

    long pos_duration;
} demo_t;

typedef struct parser_s {
    int client;
    int last_frame;
    unsigned int server_time;
    int last_cmd_num;
    int last_cmd_ack;
    demo_t demos[MAX_DEMOS];
    msg_t initial;
    int playernums[MAX_CLIENTS];
} parser_t;

void parser_reset(parser_t *parser);
void parse_message(parser_t *parser, msg_t *msg);
void parse_demo(parser_t *parser, gzFile fp);
int parser_record(parser_t *parser, FILE *fp, int target, void (*save)(int id, int client, int target, qboolean terminated));
void parser_stop_record(parser_t *parser, int id);
void parser_terminate_record(parser_t *parser, int id);

#endif
