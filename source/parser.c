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
#include <time.h>
#include <math.h>

#include "import.h"
#include "utils.h"
#include "client.h"
#include "ui.h"
#include "parser.h"

static void end_previous_demo(demo_t *demo) {
    long int backup = ftell(demo->fp);
    fseek(demo->fp, demo->start, SEEK_SET);
    int length = LittleLong(backup - demo->start - 4);
    fwrite(&length, 4, 1, demo->fp);
    fseek(demo->fp, demo->pos_duration, SEEK_SET);
    fprintf(demo->fp, "%u", (unsigned int)time(NULL) - demo->start_time + 1);
    fseek(demo->fp, backup, SEEK_SET);
}

static void parser_terminate_record_real(parser_t *parser, int id, qboolean terminated) {
    demo_t *demo = parser->demos + id;
    if (demo->fp) {
        end_previous_demo(demo);
        fclose(demo->fp);
        if (demo->save)
            demo->save(id, parser->client, demo->target, terminated);
        demo->fp = NULL;
    }
}

static void end_finishing_demos(parser_t *parser) {
    int i;
    for (i = 0; i < MAX_DEMOS; i++) {
        demo_t *demo = parser->demos + i;
        if (demo->fp && demo->finishing)
            parser_terminate_record_real(parser, i, qfalse);
    }
}

void parser_stop_record(parser_t *parser, int id) {
    demo_t *demo = parser->demos + id;
    if (demo->fp != NULL)
        demo->finishing = qtrue;
}

void parser_terminate_record(parser_t *parser, int id) {
    parser_terminate_record_real(parser, id, qtrue);
}

static void terminate_demos(parser_t *parser) {
    int i;
    for (i = 0; i < MAX_DEMOS; i++)
        parser_terminate_record_real(parser, i, qtrue);
}

void parser_reset(parser_t *parser) {
    static qboolean first = qtrue;
    parser->last_frame = -1;
    parser->server_time = 0;
    parser->last_cmd_num = 0;
    parser->last_cmd_ack = -1;
    if (!first)
        terminate_demos(parser);
    int i;
    for (i = 0; i < MAX_DEMOS; i++)
        parser->demos[i].fp = NULL;
    parser->initial.cursize = 0;
    for (i = 0; i < MAX_CLIENTS; i++)
        parser->playernums[i] = i;
    first = qfalse;
}

static void start_new_demo(demo_t *demo) {
    demo->start = ftell(demo->fp);
    int length = 0;
    fwrite(&length, 4, 1, demo->fp);
}

static void start_new(parser_t *parser) {
    int i;
    for (i = 0; i < MAX_DEMOS; i++) {
        demo_t *demo = parser->demos + i;
        if (demo->fp && !demo->waiting)
            start_new_demo(demo);
    }
}

static void end_previous(parser_t *parser) {
    int i;
    for (i = 0; i < MAX_DEMOS; i++) {
        demo_t *demo = parser->demos + i;
        if (demo->fp && !demo->waiting)
            end_previous_demo(demo);
    }
}

int parser_record(parser_t *parser, FILE *fp, int target, void (*save)(int id, int client, int target, qboolean terminated)) {
    int i;
    for (i = 0; i < MAX_DEMOS; i++) {
        demo_t *demo = parser->demos + i;
        if (demo->fp == NULL) {
            demo->fp = fp;
            demo->target = target;
            demo->waiting = qfalse;
            demo->finishing = qfalse;
            demo->save = save;
            int x = 0;
            start_new_demo(demo);
            qbyte c = svc_demoinfo;
            fwrite(&c, 1, 1, fp);
            long pos_meta_length = ftell(fp);
            x = 0;
            fwrite(&x, 4, 1, fp); // demoinfo length
            long pos_meta_start = ftell(fp);
            x = LittleLong(4);
            fwrite(&x, 4, 1, fp); // metadata offset
            x = LittleLong(parser->server_time);
            fwrite(&x, 4, 1, fp); // time
            long pos_meta_keys_length = ftell(fp);
            x = 0;
            fwrite(&x, 4, 1, fp); // size
            fwrite(&x, 4, 1, fp); // max size
            long pos_meta_keys_start = ftell(fp);
            cs_t *cs = client_cs(parser->client);
            fprintf(fp, "hostname");
            fwrite(&x, 1, 1, fp);
            fprintf(fp, "%s", cs_get(cs, CS_HOSTNAME));
            fwrite(&x, 1, 1, fp);
            fprintf(fp, "localtime");
            fwrite(&x, 1, 1, fp);
            demo->start_time = time(NULL);
            fprintf(fp, "%u", demo->start_time);
            fwrite(&x, 1, 1, fp);
            fprintf(fp, "multipov");
            fwrite(&x, 1, 1, fp);
            fprintf(fp, "%d", target < 0);
            fwrite(&x, 1, 1, fp);
            fprintf(fp, "mapname");
            fwrite(&x, 1, 1, fp);
            fprintf(fp, "%s", cs_get(cs, CS_MAPNAME));
            fwrite(&x, 1, 1, fp);
            fprintf(fp, "gametype");
            fwrite(&x, 1, 1, fp);
            fprintf(fp, "%s", cs_get(cs, CS_GAMETYPENAME));
            fwrite(&x, 1, 1, fp);
            fprintf(fp, "levelname");
            fwrite(&x, 1, 1, fp);
            fprintf(fp, "%s", cs_get(cs, CS_MESSAGE));
            fwrite(&x, 1, 1, fp);
            fprintf(fp, "duration");
            fwrite(&x, 1, 1, fp);
            demo->pos_duration = ftell(fp);
            int j;
            for (j = 0; j < 32; j++)
                fwrite(&x, 1, 1, fp);
            long pos_meta_end = ftell(fp);
            fseek(fp, pos_meta_length, SEEK_SET);
            x = LittleLong(pos_meta_end - pos_meta_start);
            fwrite(&x, 4, 1, fp);
            fseek(fp, pos_meta_keys_length, SEEK_SET);
            x = LittleLong(pos_meta_end - pos_meta_keys_start);
            fwrite(&x, 4, 1, fp);
            fseek(fp, pos_meta_keys_length + 4, SEEK_SET);
            fwrite(&x, 4, 1, fp);
            fseek(fp, pos_meta_end, SEEK_SET);
            end_previous_demo(demo);
            start_new_demo(demo);
            fwrite(parser->initial.data, 1, parser->initial.cursize, fp);
            for (j = 0; j < MAX_CONFIGSTRINGS; j++) {
                char *string = cs_get(cs, j);
                if (string && string[0]) {
                    c = svc_servercs;
                    fwrite(&c, 1, 1, fp);
                    fprintf(fp, "cs %i \"%s\"", j, string);
                    c = 0;
                    fwrite(&c, 1, 1, fp);
                }
            }
            c = svc_servercs;
            fwrite(&c, 1, 1, fp);
            fprintf(fp, "precache");
            c = 0;
            fwrite(&c, 1, 1, fp);
            client_command(parser->client, "nodelta");
            demo->waiting = qtrue;
            return i;
        }
    }
    return -1;
}

static void start_demos(parser_t *parser) {
    int i;
    for (i = 0; i < MAX_DEMOS; i++) {
        if (parser->demos[i].fp)
            parser->demos[i].waiting = qfalse;
    }
}

static void record_initial(parser_t *parser, msg_t *source, int size) {
    write_data(&parser->initial, source->data + source->readcount, size);
}

static qboolean target_match(parser_t *parser, int target, qbyte *targets) {
    if (target == -1 || targets == NULL)
        return qtrue;
    return (targets[target >> 3] & (1 << (target & 7))) > 0;
}

static qboolean target_wrap_match(parser_t *parser, int target, int actual) {
    if (target == -1 || actual == -1)
        return qtrue;
    return (target <= actual ? parser->playernums[target] : target) == parser->playernums[actual];
}

static void record(parser_t *parser, msg_t *msg, int size, qbyte *targets) {
    int i;
    for (i = 0; i < MAX_DEMOS; i++) {
        if (parser->demos[i].fp && !parser->demos[i].waiting && target_match(parser, parser->demos[i].target, targets))
            fwrite(msg->data + msg->readcount, 1, size, parser->demos[i].fp);
    }
}

static void record_wrapped(parser_t *parser, msg_t *msg, int size, int target) {
    int i;
    for (i = 0; i < MAX_DEMOS; i++) {
        if (parser->demos[i].fp && !parser->demos[i].waiting && target_wrap_match(parser, parser->demos[i].target, target))
            fwrite(msg->data + msg->readcount, 1, size, parser->demos[i].fp);
    }
}

static void record_multipov(parser_t *parser, msg_t *msg, int size) {
    int i;
    for (i = 0; i < MAX_DEMOS; i++) {
        if (parser->demos[i].fp && !parser->demos[i].waiting && parser->demos[i].target < 0)
            fwrite(msg->data + msg->readcount, 1, size, parser->demos[i].fp);
    }
}

static void record_frameflags(parser_t *parser, msg_t *msg) {
    int i;
    qbyte b = msg->data[msg->readcount];
    for (i = 0; i < MAX_DEMOS; i++) {
        if (parser->demos[i].fp && !parser->demos[i].waiting) {
            if (parser->demos[i].target < 0)
                b |= FRAMESNAP_FLAG_MULTIPOV;
            else
                b &= ~FRAMESNAP_FLAG_MULTIPOV;
            fwrite(&b, 1, 1, parser->demos[i].fp);
        }
    }
}

static void record_last(parser_t *parser, msg_t *msg, qbyte *targets) {
    msg->readcount--;
    record(parser, msg, 1, targets);
    msg->readcount++;
}

static void prepare_fragment(parser_t *parser, msg_t *msg) {
    end_previous(parser);
    start_new(parser);
    record_last(parser, msg, NULL);
}

static void record_string(parser_t *parser, msg_t *msg, qbyte *targets) {
    int i;
    for (i = msg->readcount; msg->data[i]; i++)
        ;
    record(parser, msg, i - msg->readcount + 1, targets);
}

static void parse_player_state(parser_t *parser, msg_t *msg, short *old_stats, int index) {
	int i, b;

	int flags = read_byte(msg);
	if(flags & PS_MOREBITS1) {
		b = read_byte(msg);
		flags |= b<<8;
	}
	if( flags & PS_MOREBITS2 ) {
		b = read_byte( msg );
		flags |= b<<16;
	}
	if (flags & PS_MOREBITS3) {
		b = read_byte( msg );
		flags |= b<<24;
	}

	if( flags & PS_M_TYPE )
		read_byte(msg);

	if( flags & PS_M_ORIGIN0 )
        read_int3(msg);
	if( flags & PS_M_ORIGIN1 )
        read_int3(msg);
	if( flags & PS_M_ORIGIN2 )
        read_int3(msg);

	if( flags & PS_M_VELOCITY0 )
        read_int3(msg);
	if( flags & PS_M_VELOCITY1 )
        read_int3(msg);
	if( flags & PS_M_VELOCITY2 )
        read_int3(msg);

	if( flags & PS_M_TIME )
        read_byte(msg);

	if( flags & PS_M_FLAGS )
        read_short(msg);

	if( flags & PS_M_DELTA_ANGLES0 )
        read_short(msg);
	if( flags & PS_M_DELTA_ANGLES1 )
        read_short(msg);
	if( flags & PS_M_DELTA_ANGLES2 )
        read_short(msg);

	if( flags & PS_EVENT ) {
		int x = read_byte( msg );
		if( x & EV_INVERSE )
			read_byte( msg );
	}

	if( flags & PS_EVENT2 ) {
		int x = read_byte( msg );
		if( x & EV_INVERSE )
			read_byte( msg );
	}

	if( flags & PS_VIEWANGLES ) {
        read_short(msg);
        read_short(msg);
        read_short(msg);
	}

	if( flags & PS_M_GRAVITY )
        read_short(msg);

	if( flags & PS_WEAPONSTATE )
        read_byte(msg);

	if( flags & PS_FOV )
        read_byte(msg);

	if( flags & PS_POVNUM )
        read_byte(msg);

	if( flags & PS_PLAYERNUM )
		parser->playernums[index] = read_byte(msg);

	if( flags & PS_VIEWHEIGHT )
        read_char(msg);

	if( flags & PS_PMOVESTATS ) {
		int pmstatbits = read_short( msg );
		for( i = 0; i < PM_STAT_SIZE; i++ ) {
			if( pmstatbits & ( 1<<i ) )
				read_short(msg);
		}
	}

	if( flags & PS_INVENTORY ) {
		int invstatbits[SNAP_INVENTORY_LONGS];

		// parse inventory
		for( i = 0; i < SNAP_INVENTORY_LONGS; i++ )
			invstatbits[i] = read_long( msg );

		for( i = 0; i < MAX_ITEMS; i++ ) {
			if( invstatbits[i>>5] & ( 1<<(i&31) ) )
                read_byte(msg);
		}
	}

	if( flags & PS_PLRKEYS )
        read_byte(msg);

	int statbits[SNAP_STATS_LONGS];

	// parse stats
	for( i = 0; i < SNAP_STATS_LONGS; i++ )
		statbits[i] = read_long( msg );

	for( i = 0; i < PS_MAX_STATS; i++ ) {
		if( statbits[i>>5] & ( 1<<(i&31) ) )
			set_stat(parser->client, parser->playernums[index + 1], i, read_short(msg));
        else
			set_stat(parser->client, parser->playernums[index + 1], i, old_stats[i]);
	}
}

static void parse_delta_gamestate(msg_t *msg) {
	qbyte bits = read_byte( msg );
	short statbits = read_short( msg );

    if( bits ) {
        int i;
        for( i = 0; i < MAX_GAME_LONGSTATS; i++ ) {
            if( bits & ( 1<<i ) )
                read_long(msg);
        }
    }

    if( statbits ) {
        int i;
        for( i = 0; i < MAX_GAME_STATS; i++ ) {
            if( statbits & ( 1<<i ) )
                read_short(msg);
        }
    }
}

static void parse_frame(parser_t *parser, msg_t *msg) {
    int length = read_short(msg); // length
    int pos = msg->readcount;
    parser->server_time = read_long(msg);
    int frame = read_long(msg);
    read_long(msg); // delta frame number
    read_long(msg); // ucmd executed
    int flags = read_byte(msg);
    if (!(flags & FRAMESNAP_FLAG_DELTA))
        start_demos(parser);
    msg->readcount -= 1;
    msg->readcount -= 18;
    prepare_fragment(parser, msg);
    record(parser, msg, 18, NULL);
    msg->readcount += 18;
    record_frameflags(parser, msg);
    msg->readcount += 1;
    record(parser, msg, 2, NULL);

    read_byte(msg); // suppresscount

    read_byte(msg); // svc_gamecommands
    int framediff;
    static qbyte targets[MAX_CLIENTS / 8];
    while ((framediff = read_short(msg)) != -1) {
        qboolean valid = frame > parser->last_frame + framediff;
        qboolean record_valid = valid;
        int pos = msg->readcount;
        char *cmd = read_string(msg);
        if (partial_match("private message", cmd) || partial_match(">>>", cmd) || partial_match("<<<", cmd))
            record_valid = qfalse;
        int numtargets = 0;
        int i;
        for (i = 0; i < MAX_CLIENTS / 8; i++)
            targets[i] = 0;
        if (flags & FRAMESNAP_FLAG_MULTIPOV) {
            int a = msg->readcount;
            numtargets = read_byte(msg);
            int b = msg->readcount;
            read_data(msg, targets, numtargets);
            if (record_valid) {
                int real = msg->readcount;
                msg->readcount = pos - 2;
                record(parser, msg, 2, numtargets ? targets : NULL);
                msg->readcount = pos;
                record_string(parser, msg, numtargets ? targets : NULL);
                msg->readcount = a;
                record_multipov(parser, msg, 1);
                msg->readcount = b;
                record_multipov(parser, msg, numtargets);
                msg->readcount = real;
            }
        } else if (record_valid) {
            int real = msg->readcount;
            msg->readcount = pos - 2;
            record(parser, msg, 2, NULL);
            msg->readcount = pos;
            record_string(parser, msg, NULL);
            qbyte temp = msg->data[msg->readcount];
            msg->data[msg->readcount] = 0;
            record_multipov(parser, msg, 1);
            msg->data[msg->readcount] = temp;
            msg->readcount = real;
        }
        if (valid)
            execute(parser->client, cmd, numtargets ? targets : NULL);
    }
    msg->readcount -= 2;
    record(parser, msg, 2, NULL);
    msg->readcount += 2;

    record(parser, msg, 1, NULL);
    int bytes = read_byte(msg);
    record(parser, msg, bytes, NULL);
    skip_data(msg, bytes);

    int start = msg->readcount;
    read_byte(msg); // svc_match
    parse_delta_gamestate(msg);
    int backup = msg->readcount;
    msg->readcount = start;
    record(parser, msg, backup - start, NULL);
    msg->readcount = backup;

    short *old_stats = get_stats(parser->client);
    int cmd;
    int players = 0;
    while ((cmd = read_byte(msg)) != 0) { // svc_playerinfo
        start = msg->readcount - 1;
        parse_player_state(parser, msg, old_stats + (parser->playernums[players + 1]) * PS_MAX_STATS, players);
        backup = msg->readcount;
        msg->readcount = start;
        record_wrapped(parser, msg, backup - start, players);
        msg->readcount = backup;
        players++;
    }
    while (players < MAX_CLIENTS - 1) {
        set_stat(parser->client, parser->playernums[players + 1], STAT_TEAM, 0);
        players++;
    }
    msg->readcount -= 1;
    record(parser, msg, 1, NULL);
    msg->readcount += 1;
    record(parser, msg, length - (msg->readcount - pos), NULL);
    skip_data(msg, length - (msg->readcount - pos));

    if (frame > parser->last_frame)
        client_ack_frame(parser->client, frame, parser->server_time);

    parser->last_frame = frame;
}

void parse_message(parser_t *parser, msg_t *msg) {
    int cmd;
    int ack;
    int size;
    int start;
    int backup;
    while (1) {
        cmd = read_byte(msg);
        switch (cmd) {
            case svc_demoinfo:
                read_long(msg); // length
                read_long(msg); // meta data offset
                read_long(msg); // basetime
                size_t meta_data_realsize = read_long(msg);
                size_t meta_data_maxsize = read_long(msg);
                size_t end = msg->readcount + meta_data_realsize;
                while (msg->readcount < end) {
                    demoinfo_key(parser->client, read_string(msg));
                    demoinfo_value(parser->client, read_string(msg));
                }
                skip_data(msg, meta_data_maxsize - meta_data_realsize);
                break;
            case svc_clcack:
                ack = read_long(msg); // reliable ack
                if (ack > parser->last_cmd_ack) {
                    client_get_ack(parser->client, ack);
                    parser->last_cmd_ack = ack;
                }
                read_long(msg); // ucmd acknowledged
                client_activate(parser->client);
                break;
            case svc_servercmd:
                start = msg->readcount;
                if (!(get_bitflags(parser->client) & SV_BITFLAGS_RELIABLE)) {
                    int cmd_num = read_long(msg);
                    if (cmd_num != parser->last_cmd_num + 1) {
                        read_string(msg);
                        break;
                    }
                    parser->last_cmd_num = cmd_num;
                    client_ack(parser->client, cmd_num);
                }
            case svc_servercs:
                if (cmd == svc_servercs)
                    start = msg->readcount;
                backup = msg->readcount;
                msg->readcount = start;
                prepare_fragment(parser, msg);
                msg->readcount = backup;
                record_string(parser, msg, NULL);
                execute(parser->client, read_string(msg), NULL);
                break;
            case svc_serverdata:
                size = msg->readcount - 1;
                set_protocol(parser->client, read_long(msg));
                set_spawn_count(parser->client, read_long(msg));
                read_short(msg); // snap frametime
                read_string(msg); // base game
                set_game(parser->client, read_string(msg));
                set_playernum(parser->client, read_short(msg) + 1);
                set_level(parser->client, read_string(msg)); // level name
                int flag_offset = msg->readcount;
                set_bitflags(parser->client, read_byte(msg));
                if (get_bitflags(parser->client) & SV_BITFLAGS_HTTP_BASEURL)
                    set_baseurl(parser->client, read_string(msg));
                int pure = read_short(msg);
                while (pure > 0) {
                    read_string(msg); // pure pk3 name
                    read_long(msg); // checksum
                    pure--;
                }
                size = msg->readcount - size;
                msg->readcount -= size;
                msg->data[flag_offset] |= SV_BITFLAGS_RELIABLE;
                record_initial(parser, msg, size);
                msg->readcount += size;
                break;
            case svc_spawnbaseline:
                size = msg->readcount - 1;
                read_delta_entity(msg, read_entity_bits(msg));
                size = msg->readcount - size;
                msg->readcount -= size;
                record_initial(parser, msg, size);
                msg->readcount += size;
                break;
            case svc_frame:
                parse_frame(parser, msg);
                break;
            case -1:
                return;
            default:
                ui_output(parser->client, "Unknown command: %d\n", cmd);
                return;
        }
        end_finishing_demos(parser);
    }
}

static qboolean read_demo_message(parser_t *parser, FILE *fp) {
    int length;
    if (!fread(&length, 4, 1, fp))
        return qfalse;
    length = LittleLong(length);
    if (length < 0)
        return qfalse;
    static msg_t msg;
    if (!fread(msg.data, length, 1, fp))
        return qfalse;
    msg.readcount = 0;
    msg.cursize = length;
    msg.maxsize = sizeof(msg.data);
    msg.compressed = qfalse;
    parse_message(parser, &msg);
    return qtrue;
}

void parse_demo(parser_t *parser, FILE *fp) {
    while (read_demo_message(parser, fp))
        ;
}
