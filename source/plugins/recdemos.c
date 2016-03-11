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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "../plugins.h"
#include "../client.h"
#include "../utils.h"

#define RECBUFFER 5
#define POSTRUN_TIME 8000
#define MAX_TIME 1200000

typedef struct recdemo_s {
    int id;
    qboolean stopped;
    qboolean finished;
    qboolean record;
    unsigned int record_time;
    signed int start_time;
    signed int stop_time;
} recdemo_t;

typedef struct manager_s {
    recdemo_t demos[RECBUFFER];
    int current;
} manager_t;

plugin_interface_t *trap;

static int external_index;
static int pr_index;

static manager_t demos[CLIENTS][MAX_CLIENTS];

static void save_demo(int id, int c, int t, qboolean terminated) {
    manager_t *manager = &demos[c][t];
    int i;
    for (i = 0; i < RECBUFFER; i++) {
        recdemo_t *demo = manager->demos + i;
        if (demo->id == id) {
            if (demo->record && !terminated) {
                static char old[1024];
                strcpy(old, trap->path("demos/runs/%d_%d_%d.wdz%d", c, t, i, DEMO_PROTOCOL));
                rename(old, trap->path("demos/records/%s.wdz%d", trap->get_level(c), DEMO_PROTOCOL));
            } else {
                unlink(trap->path("demos/runs/%d_%d_%d.wdz%d", c, t, i, DEMO_PROTOCOL));
            }
            demo->id = -1;
        }
    }
}

static void stop(int c, int t, int d) {
    recdemo_t *demo = demos[c][t].demos + d;
    if (demo->finished)
        return;
    trap->client_stop_record(c, demo->id);
    demo->finished = qtrue;
}

static void terminate(int c, int t, int d) {
    recdemo_t *demo = demos[c][t].demos + d;
    if (demo->id == -1)
        return;
    trap->client_terminate_record(c, demo->id);
}

static void schedule_stop(int c, int t) {
    manager_t *manager = &demos[c][t];
    if (manager->current == -1)
        return;
    recdemo_t *demo = manager->demos + manager->current;
    if (demo->id == -1 || demo->stopped || demo->finished)
        return;
    demo->stopped = qtrue;
    demo->stop_time = millis();
}

static void start(int c, int t) {
    manager_t *manager = &demos[c][t];
    schedule_stop(c, t);
    int i;
    recdemo_t *demo = NULL;
    if (manager->current != -1 && manager->demos[manager->current].id == -1) {
        demo = manager->demos + manager->current;
    } else {
        for (i = 0; i < RECBUFFER; i++) {
            if (manager->demos[i].id == -1) {
                manager->current = i;
                demo = manager->demos + i;
                break;
            }
        }
        if (demo == NULL) {
            int min = -1;
            for (i = 0; i < RECBUFFER; i++) {
                if (!manager->demos[i].record && (min == -1
                            || manager->demos[i].stop_time - manager->demos[i].start_time
                        < manager->demos[min].stop_time - manager->demos[min].start_time))
                    min = i;
            }
            if (min == -1) {
                for (i = 0; i < RECBUFFER; i++) {
                    if (min == -1 || manager->demos[i].stop_time - manager->demos[i].start_time
                            < manager->demos[min].stop_time - manager->demos[min].start_time)
                        min = i;
                }
            }
            terminate(c, t, min);
            manager->current = min;
            demo = manager->demos + min;
        }
    }
    demo->start_time = millis();
    demo->stopped = qfalse;
    demo->finished = qfalse;
    demo->record = qfalse;
    FILE *fp = fopen(trap->path("demos/runs/%d_%d_%d.wd%d", c, t, manager->current, PROTOCOL), "w");
    demo->id = trap->client_record(trap->cmd_client(), fp, t, save_demo);
}

void frame() {
    signed int time = millis();
    int i;
    for (i = 0; i < CLIENTS; i++) {
        int j;
        for (j = 0; j < MAX_CLIENTS; j++) {
            manager_t *manager = &demos[i][j];
            int k;
            for (k = 0; k < RECBUFFER; k++) {
                recdemo_t *demo = manager->demos + k;
                if (demo->id >= 0 && (time - demo->start_time >= MAX_TIME
                            || (demo->stopped && time >= demo->stop_time + POSTRUN_TIME)))
                    stop(i, j, k);
            }
        }
    }
}

static void cmd_external() {
    int target_bytes = atoi(trap->cmd_argv(1));
    int target_bits = target_bytes * 8;
    if (!strcmp(trap->cmd_argv(2 + target_bytes), "dstart")) {
        int i;
        for (i = 0; i < target_bits; i++) {
            if ((atoi(trap->cmd_argv(2 + (i >> 3))) & (1 << (i & 7))))
                start(trap->cmd_client(), i);
        }
    }
}

static void cmd_pr() {
    char *message = trap->cmd_argv(1);
    if (partial_match("made a new server record", message)) {
        unsigned int time = 0;
        char *p;
        int multiplier = 1;
        int position = 1;
        for (p = message + strlen(message); *p != ' '; p--) {
            switch (*p) {
                case ':':
                    multiplier *= 60;
                    position = 1;
                    break;
                case '.':
                    multiplier *= 1000;
                    position = 1;
                    break;
                default:
                    if (*p >= '0' && *p <= '9') {
                        time += multiplier * position * (*p - '0');
                        position *= 10;
                    }
                    break;
            }
        }
        int c = trap->cmd_client();
        unsigned int old_time = 0;
        char *level = trap->get_level(c);
        FILE *fp = fopen(trap->path("demos/records/%s.txt", level), "r");
        if (fp) {
            if (!fscanf(fp, "%u", &old_time))
                old_time = time + 1;
            fclose(fp);
        } else {
            old_time = time + 1;
        }
        if (time < old_time) {
            cs_t *cs = trap->client_cs(c);
            int i;
            for (i = 0; i < MAX_CLIENTS; i++) {
                char *name = player_name(cs, i + 1);
                if (name && *name) {
                    if (starts_with(message, name)) {
                        manager_t *manager = &demos[c][i];
                        int min = -1;
                        int j;
                        for (j = 0; j < RECBUFFER; j++) {
                            if (manager->demos[j].id != -1) {
                                if (min == -1 || manager->demos[j].start_time < manager->demos[min].start_time)
                                    min = j;
                            }
                        }
                        if (min >= 0) {
                            for (j = 0; j < MAX_CLIENTS; j++) {
                                manager_t *other = &demos[c][j];
                                int k;
                                for (k = 0; k < RECBUFFER; k++) {
                                    if (other->demos[k].id != -1)
                                        other->demos[k].record = qfalse;
                                }
                            }
                            manager->demos[min].record = qtrue;
                            manager->demos[min].record_time = time;
                            fp = fopen(trap->path("demos/records/%s.txt", trap->get_level(c)), "w");
                            fprintf(fp, "%u\n%s\n%s\n%u\n", time, level, name, unixtime());
                            fclose(fp);
                            schedule_stop(c, i);
                            break;
                        }
                    }
                }
            }
        }
    }
}

void init(plugin_interface_t *new_trap) {
    trap = new_trap;
    int i;
    for (i = 0; i < CLIENTS; i++) {
        int j;
        for (j = 0; j < MAX_CLIENTS; j++) {
            manager_t *manager = &demos[i][j];
            manager->current = -1;
            int k;
            for (k = 0; k < RECBUFFER; k++)
                manager->demos[k].id = -1;
        }
    }
    external_index = trap->cmd_add_event("external", cmd_external);
    pr_index = trap->cmd_add_from_server("pr", cmd_pr);
}

void shutdown() {
    int i;
    for (i = 0; i < CLIENTS; i++) {
        int j;
        for (j = 0; j < MAX_CLIENTS; j++) {
            manager_t *manager = &demos[i][j];
            int k;
            for (k = 0; k < RECBUFFER; k++) {
                if (manager->demos[k].id != -1)
                    terminate(i, j, k);
            }
        }
    }
    trap->cmd_remove(external_index);
    trap->cmd_remove(pr_index);
}
