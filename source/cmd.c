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

#include "global.h"
#include "utils.h"
#include "cs.h"
#include "client.h"
#include "cmd.h"

#define CMD_STACK_SIZE 64

typedef enum cmd_type_e {
    CT_NORMAL,
    CT_PERSISTENT,
    CT_PUBLIC,
    CT_PUBLIC_PERSISTENT,
    CT_EVENT,
    CT_FROM_SERVER,
    CT_SERVER,
    CT_GLOBAL,
    CT_FIND_FREE,
    CT_BROADCAST,
    CT_BROADCAST_ALL,
    CT_CVAR,
    CT_TOTAL
} cmd_type_t;

typedef struct cmd_s {
    char *name;
    void (*f)();
    void *(*cvar_get)();
    int (*complete)(int arg, char suggestions[][MAX_SUGGESTION_SIZE]);
    cmd_type_t type;
    qboolean clients[CLIENTS];
    int index;
} cmd_t;

typedef struct cmd_stack_s {
    int client;
    int caller;
    int argc;
    char argv[MAX_ARGC][MAX_ARG_SIZE];
    char args[MAX_ARGS_SIZE];
    int args_index[MAX_ARGC];
    int cursor;
} cmd_stack_t;

cmd_t cmds[MAX_CMDS];
int cmd_count = 0;

static cmd_stack_t cmd_stack[CMD_STACK_SIZE];
static int cmd_stack_count = 0;

static cmd_stack_t *s = NULL;

static void cmd_stack_push() {
    if (cmd_stack_count == CMD_STACK_SIZE)
        die("Command stack overflow");
    s = cmd_stack + cmd_stack_count++;
    s->client = -1;
    s->argc = 0;
    return;
}

static void cmd_stack_pop() {
    cmd_stack_count--;
    if (cmd_stack_count == 0)
        s = NULL;
    else
        s = cmd_stack + cmd_stack_count - 1;
}

void parse_cmd(char *cmd, int cursor) {
    s->argc = 0;
    s->cursor = -1;
    strcpy(s->args, cmd);
    int i;
    qboolean escaped = qfalse;
    char quote = '\0';
    int len = strlen(s->args);
    int o = 0;
    int start = 0;
    for (i = 0; i < len; i++) {
        qboolean normal = qfalse;
        qboolean skip = qfalse;
        char add = '\0';
        if (escaped) {
            if (quote != s->args[i] && s->args[i] != '\\')
                add = '\\';
            escaped = qfalse;
            normal = qtrue;
        } else {
            switch (s->args[i]) {
                case '\\':
                    escaped = qtrue;
                    break;
                case ' ':
                case '\t':
                case '\n':
                    if (quote != '\0')
                        normal = qtrue;
                    else if (o > 0)
                        skip = qtrue;
                    else
                        start = i + 1;
                    break;
                case '\'':
                case '"':
                    if (quote == s->args[i])
                        skip = qtrue;
                    else if (quote == '\0')
                        quote = s->args[i];
                    else
                        normal = qtrue;
                    break;
                default:
                    normal = qtrue;
                    break;
            }
        }
        if (i == cursor)
            s->cursor = s->argc;
        if (skip) {
            s->args_index[s->argc] = start;
            s->argv[s->argc][o] = '\0';
            s->argc++;
            o = 0;
            start = i + 1;
        }
        if (add != '\0' && o < MAX_ARG_SIZE - 1)
            s->argv[s->argc][o++] = add;
        if (normal && o < MAX_ARG_SIZE - 1)
            s->argv[s->argc][o++] = s->args[i];
    }
    if (cursor >= 0 && s->cursor == -1)
        s->cursor = s->argc;
    if (o > 0 || (i >= 1 && (s->args[i - 1] == ' ' || s->args[i - 1] == '\t' || s->args[i - 1] == '\n'))) {
        s->args_index[s->argc] = start;
        s->argv[s->argc][o] = '\0';
        s->argc++;
    }
}

static qboolean cmd_type_extends(int type, int parent) {
    if (type == parent)
        return qtrue;

    if (parent == CT_NORMAL && (type == CT_PERSISTENT || type == CT_SERVER))
        return qtrue;

    if (parent == CT_PUBLIC && type == CT_PUBLIC_PERSISTENT)
        return qtrue;

    if (parent == CT_GLOBAL && (type == CT_FIND_FREE || type == CT_BROADCAST || type == CT_BROADCAST_ALL || type == CT_CVAR))
        return qtrue;

    return qfalse;
}

static qboolean cmd_type_compatible(int type, int parent) {
    if (cmd_type_extends(type, parent))
        return qtrue;

    if (parent == CT_NORMAL && cmd_type_extends(type, CT_GLOBAL))
        return qtrue;

    return qfalse;
}

static qboolean cmd_valid(cmd_t *cmd, int c, qboolean partial) {
    int len = strlen(cmd_argv(0));
    return (partial ? !strncmp(cmd->name, cmd_argv(0), len)
            : !strcmp(cmd->name, cmd_argv(0)))
        && (c < 0 || cmd->clients[c])
        && (cmd->type != CT_NORMAL || cmd->type != CT_PUBLIC || client_active(c));
}

static cmd_t *cmd_find(cmd_t *cmd, int c, int type, qboolean partial) {
    if (!partial && cmd_argc() == 0)
        return NULL;

    int i;
    for (i = cmd ? (cmd - cmds) + 1 : 0; i < cmd_count; i++) {
        if (cmd_type_compatible(cmds[i].type, type) && cmd_valid(cmds + i, c, partial))
            return cmds + i;
    }

    return NULL;
}

static int normal_type(int c) {
    return c >= 0 ? CT_NORMAL : CT_GLOBAL;
}

int cmd_suggest(int c, char *name, int cursor, char suggestions[][MAX_SUGGESTION_SIZE], qboolean public) {
    int type = public ? CT_PUBLIC : normal_type(c);
    int count = 0;
    cmd_stack_push();
    parse_cmd(name, cursor);
    if (cmd_argc() <= 1) {
        cmd_t *cmd = NULL;
        while ((cmd = cmd_find(cmd, c, type, qtrue)) != NULL) {
            if (cmd->name[0])
                strcpy(suggestions[count++], cmd->name);
        }
    } else {
        cmd_t *cmd = NULL;
        while ((cmd = cmd_find(cmd, c, type, qfalse)) != NULL) {
            if (cmd->complete)
                count += cmd->complete(s->cursor, suggestions + count);
        }
        if (count == 1) {
            char *temp = suggestions[1];
            strcpy(temp, suggestions[0]);
            suggestions[0][0] = '\0';
            int j;
            for (j = 0; j < cmd_argc(); j++) {
                if (j == s->cursor)
                    strcat(suggestions[0], temp);
                else
                    strcat(suggestions[0], cmd_argv(j));
                if (j != cmd_argc() - 1)
                    strcat(suggestions[0], " ");
            }
        }
    }
    cmd_stack_pop();
    return count;
}

static void cmd_execute_real(int c, int caller, char *name, int type) {
    cmd_stack_push();
    s->caller = caller;
    parse_cmd(name, -1);

    cmd_t *cmd = NULL;
    int cmds = 0;
    while ((cmd = cmd_find(cmd, c, type, qfalse)) != NULL) {
        cmds++;

        int start = c;
        int end = c;
        qboolean switch_screen = qfalse;
        int old_type = type;
        if (cmd_type_compatible(cmd->type, type))
            type = cmd->type;
        if ((type == CT_BROADCAST || type == CT_CVAR) && c < 0) {
            start = 0;
            end = CLIENTS - 1;
        } else if (type == CT_BROADCAST_ALL) {
            start = -1;
            end = CLIENTS - 1;
        } else if (type == CT_FIND_FREE && c < 0) {
            int i;
            qboolean found = qfalse;
            for (i = 0; i < CLIENTS && !found; i++) {
                if (!client_active(i)) {
                    start = i;
                    end = i;
                    found = qtrue;
                    switch_screen = qtrue;
                    break;
                }
            }
            if (!found) {
                ui_output(c, "No free client found.\n");
                cmd_stack_pop();
                return;
            }
        }
        for (s->client = start; s->client <= end; s->client++) {
            if (type == CT_SERVER)
                client_command(s->client, "%s", cmd_args(0));
            else
                cmd->f();
            if (switch_screen)
                set_screen(s->client + 1);
        }
        type = old_type;
    }
    if (cmds == 0) {
        if (cmd_type_compatible(type, CT_PUBLIC)) {
            if (cmd_argv(0)[0])
                client_say(c, "Unknown command: \"%s\"", cmd_argv(0));
        } else if (!cmd_type_compatible(type, CT_EVENT)) {
            ui_output(c, "Unknown command: \"%s\"\n", cmd_argv(0));
        }
        cmd_stack_pop();
        return;
    }
    cmd_stack_pop();
}

void cmd_execute(int c, char *cmd) {
    cmd_execute_real(c, -1, cmd, normal_type(c));
}

void cmd_execute_public(int c, int caller, char *cmd) {
    cmd_execute_real(c, caller, cmd, CT_PUBLIC);
}

void cmd_execute_event(int c, char *cmd) {
    cmd_execute_real(c, -1, cmd, CT_EVENT);
}

void cmd_execute_from_server(int c, char *cmd) {
    cmd_execute_real(c, -1, cmd, CT_FROM_SERVER);
}

void *cvar_get(int c, char *name) {
    cmd_stack_push();
    parse_cmd(name, -1);
    cmd_t *cmd = cmd_find(NULL, c, CT_CVAR, qfalse);
    if (cmd == NULL || cmd->cvar_get == NULL) {
        cmd_stack_pop();
        return NULL;
    }
    void *result = cmd->cvar_get();
    cmd_stack_pop();
    return result;
}

int cmd_client() {
    return s->client;
}

int cmd_caller() {
    return s->caller;
}

int cmd_argc() {
    return s->argc;
}

char *cmd_argv(int index) {
    if (index >= s->argc)
        return "";
    return s->argv[index];
}

char *cmd_args(int start) {
    if (start >= s->argc)
        return "";
    return s->args + s->args_index[start];
}

static cmd_t *cmd_find_exact(char *name, void(*f)(), int type) {
    int i;
    for (i = 0; i < cmd_count; i++) {
        if (cmds[i].f == f && cmds[i].name == name && cmds[i].type == type)
            return cmds + i;
    }
    return NULL;
}

static cmd_t *cmd_reserve(char *name, void (*f)(), int type) {
    cmd_t *cmd = cmd_find_exact(name, f, type);
    if (cmd)
        return cmd;

    if (cmd_count == MAX_CMDS)
        die("Command count overflow.\n");
    cmd = cmds + cmd_count++;

    int i;
    for (i = 0; i < CLIENTS; i++)
        cmd->clients[i] = qfalse;
    cmd->name = name;
    cmd->f = f;
    cmd->type = type;
    cmd->complete = NULL;

    return cmd;
}

static void cmd_allow_all(cmd_t *cmd) {
    int i;
    for (i = 0; i < CLIENTS; i++)
        cmd->clients[i] = qtrue;
}

static int cmd_index(cmd_t *cmd) {
    static int index = 0;
    cmd->index = index++;
    return cmd->index;
}

int cmd_add(int client, char *name, void (*f)()) {
    cmd_t *cmd = cmd_reserve(name, f, CT_NORMAL);
    cmd->clients[client] = qtrue;
    return cmd_index(cmd);
}

int cmd_add_persistent(int client, char *name, void (*f)()) {
    cmd_t *cmd = cmd_reserve(name, f, CT_PERSISTENT);
    cmd->clients[client] = qtrue;
    return cmd_index(cmd);
}

int cmd_add_generic(char *name, void (*f)()) {
    cmd_t *cmd = cmd_reserve(name, f, CT_NORMAL);
    cmd_allow_all(cmd);
    return cmd_index(cmd);
}

int cmd_add_public(int client, char *name, void (*f)()) {
    cmd_t *cmd = cmd_reserve(name, f, CT_PUBLIC);
    cmd->clients[client] = qtrue;
    return cmd_index(cmd);
}

int cmd_add_public_persistent(int client, char *name, void (*f)()) {
    cmd_t *cmd = cmd_reserve(name, f, CT_PUBLIC_PERSISTENT);
    cmd->clients[client] = qtrue;
    return cmd_index(cmd);
}

int cmd_add_public_generic(char *name, void (*f)()) {
    cmd_t *cmd = cmd_reserve(name, f, CT_PUBLIC);
    cmd_allow_all(cmd);
    return cmd_index(cmd);
}

int cmd_add_event(char *name, void (*f)()) {
    cmd_t *cmd = cmd_reserve(name, f, CT_EVENT);
    cmd_allow_all(cmd);
    return cmd_index(cmd);
}

int cmd_add_from_server(char *name, void (*f)()) {
    cmd_t *cmd = cmd_reserve(name, f, CT_FROM_SERVER);
    cmd_allow_all(cmd);
    return cmd_index(cmd);
}

int cmd_add_server(int client, char *name) {
    cmd_t *cmd = cmd_reserve(name, NULL, CT_SERVER);
    cmd->clients[client] = qtrue;
    return cmd_index(cmd);
}

int cmd_add_global(char *name, void (*f)()) {
    cmd_t *cmd = cmd_reserve(name, f, CT_GLOBAL);
    cmd_allow_all(cmd);
    return cmd_index(cmd);
}

int cmd_add_find_free(char *name, void (*f)()) {
    cmd_t *cmd = cmd_reserve(name, f, CT_FIND_FREE);
    cmd_allow_all(cmd);
    return cmd_index(cmd);
}

int cmd_add_broadcast(char *name, void (*f)()) {
    cmd_t *cmd = cmd_reserve(name, f, CT_BROADCAST);
    cmd_allow_all(cmd);
    return cmd_index(cmd);
}

int cmd_add_broadcast_all(char *name, void (*f)()) {
    cmd_t *cmd = cmd_reserve(name, f, CT_BROADCAST_ALL);
    cmd_allow_all(cmd);
    return cmd_index(cmd);
}

int cmd_add_cvar(char *name, void (*f)(), void *(*cvar_get)()) {
    cmd_t *cmd = cmd_reserve(name, f, CT_CVAR);
    cmd_allow_all(cmd);
    cmd->cvar_get = cvar_get;
    return cmd_index(cmd);
}

void cmd_complete(int index, int (*complete)(int arg, char suggestions[][MAX_SUGGESTION_SIZE])) {
    int i;
    for (i = 0; i < cmd_count; i++) {
        if (cmds[i].index == index)
            cmds[i].complete = complete;
    }
}

static int cmd_remove_index;

static qboolean cmd_remove_test(void *x) {
    cmd_t *cmd = (cmd_t *)x;
    return cmd->index == cmd_remove_index;
}

void cmd_remove(int index) {
    cmd_remove_index = index;
    rm(cmds, sizeof(cmds[0]), &cmd_count, cmd_remove_test);
}
