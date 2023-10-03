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

#include <libnotify/notify.h>
#include <curses.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>

#include "global.h"
#include "utils.h"
#include "columnifier.h"
#include "serverlist.h"
#include "client.h"
#include "irc.h"
#include "ui.h"

#define MAX_BUFFER_SIZE 2048
#define MAX_OUTPUT_LENGTH 256
#define OUTPUT_PREFIX_LENGTH 6
#define MAX_INPUT_LENGTH 256
#define MAX_HISTORY 128

#define SCROLL_SPEED 10

#define INPUT_TIME 10

#define BRIGHT_WHITE_PAIR 0
#define COLOR_DEFAULT (BRIGHT_WHITE_PAIR - color_base)

typedef enum cmd_mode_e {
    CM_EXECUTE,
    CM_SAY,
    CM_SAY_TEAM,
    CM_CUSTOM
} cmd_mode_t;

typedef enum color_e {
    NORMAL_BASE,
    NORMAL_BLACK,
    NORMAL_RED,
    NORMAL_GREEN,
    NORMAL_YELLOW,
    NORMAL_BLUE,
    NORMAL_CYAN,
    NORMAL_MAGENTA,
    NORMAL_WHITE,
    NORMAL_ORANGE,
    NORMAL_GREY,
    STATUS_BASE,
    STATUS_BLACK,
    STATUS_RED,
    STATUS_GREEN,
    STATUS_YELLOW,
    STATUS_BLUE,
    STATUS_CYAN,
    STATUS_MAGENTA,
    STATUS_WHITE,
    STATUS_ORANGE,
    STATUS_GREY
} color_t;

static color_t color_base;

static WINDOW *mainwin = NULL;
static WINDOW *titlewin = NULL;
static WINDOW *outwin = NULL;
static WINDOW *statuswin = NULL;
static WINDOW *inwin = NULL;

typedef struct screen_s {
    char *motd;
    char *level;
    char *game;
    char *host;
    char *port;

    char *last_name;
    char *last_server;

    char buffer[MAX_BUFFER_SIZE][MAX_OUTPUT_LENGTH];
    int buffer_count;
    qboolean ghost_line;
    int buffer_index;
    int output_length;
    int visual_output_length;
    int scroll_up;
    qboolean next_line;
    qboolean allow_time;

    cmd_mode_t command_mode;
    char custom_command_mode[MAX_INPUT_LENGTH];

    char commandline[MAX_INPUT_LENGTH];
    char backup[MAX_INPUT_LENGTH];
    int commandline_length;
    int commandline_cursor;
    char history[MAX_HISTORY][MAX_INPUT_LENGTH];
    int history_count;
    int history_index;
    int history_lookup;

    qboolean updated;
    qboolean important;

    qboolean redraw_outwin;
} screen_t;

static screen_t screens[SCREENS];
static int screen = 0;

static bool stopped = FALSE;

void ui_stop() {
    int i;
    irc_disconnect();
    for (i = 0; i < CLIENT_SCREENS; i++)
        disconnect(i);
    if (mainwin)
        endwin();
    mainwin = NULL;
    stopped = TRUE;
}

static void interrupt(int sig) {
    quit(sig);
}

static void init_colors() {
    if (has_colors()) {
        start_color();
        use_default_colors();

        init_pair(NORMAL_BLACK, COLOR_BLACK, COLOR_WHITE);
        init_pair(NORMAL_RED, COLOR_RED, COLOR_BLACK);
        init_pair(NORMAL_GREEN, COLOR_GREEN, COLOR_BLACK);
        init_pair(NORMAL_YELLOW, COLOR_YELLOW, COLOR_BLACK);
        init_pair(NORMAL_BLUE, COLOR_WHITE, COLOR_BLUE);
        init_pair(NORMAL_CYAN, COLOR_CYAN, COLOR_BLACK);
        init_pair(NORMAL_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(NORMAL_WHITE, COLOR_WHITE, COLOR_BLACK);
        init_pair(NORMAL_ORANGE, COLOR_YELLOW, COLOR_BLACK);
        init_pair(NORMAL_GREY, COLOR_WHITE, COLOR_BLACK);
        init_pair(STATUS_BLACK, COLOR_BLACK, COLOR_CYAN);
        init_pair(STATUS_RED, COLOR_RED, COLOR_BLUE);
        init_pair(STATUS_GREEN, COLOR_GREEN, COLOR_BLUE);
        init_pair(STATUS_YELLOW, COLOR_YELLOW, COLOR_BLUE);
        init_pair(STATUS_BLUE, COLOR_BLUE, COLOR_WHITE);
        init_pair(STATUS_CYAN, COLOR_CYAN, COLOR_BLUE);
        init_pair(STATUS_MAGENTA, COLOR_MAGENTA, COLOR_BLUE);
        init_pair(STATUS_WHITE, COLOR_WHITE, COLOR_BLUE);
        init_pair(STATUS_ORANGE, COLOR_YELLOW, COLOR_BLUE);
        init_pair(STATUS_GREY, COLOR_WHITE, COLOR_BLUE);
    }
}

static void set_color(WINDOW *win, int color) {
    if (color_base + color == NORMAL_WHITE)
        color = COLOR_DEFAULT;
    wattrset(win, COLOR_PAIR(color_base + color));
}

static WINDOW *draw_win;
static int draw_len;
static int draw_cursor;
static int draw_skip;
static int draw_max;
static int draw_total_len;

static void draw_colored_char(char c) {
    if (draw_max < 0 || draw_len < draw_skip + draw_max) {
        if (draw_len >= draw_skip) {
            if (draw_len == draw_cursor)
                wattron(draw_win, A_REVERSE);
            waddch(draw_win, c);
            if (draw_len == draw_cursor)
                wattroff(draw_win, A_REVERSE);
        }
        draw_len++;
    }
    draw_total_len++;
}

static void draw_colored_color(int color) {
    set_color(draw_win, color);
}

static int draw_colored_cursored_scroll(WINDOW *win, char *string, int cursor, int max_width) {
    draw_len = 0;
    draw_total_len = 0;
    draw_win = win;
    draw_cursor = cursor;
    if (max_width > 0) {
        draw_skip = cursor >= max_width ? cursor / max_width * max_width : 0;
        draw_max = max(cursor - draw_skip, max_width);
    } else {
        draw_skip = 0;
        draw_max = -1;
    }
    parse(string, draw_colored_char, NULL, draw_colored_color);
    if (draw_len == cursor)
        draw_colored_char(' ');
    return draw_len;
}

static int draw_colored(WINDOW *win, char *string) {
    return draw_colored_cursored_scroll(win, string, -1, 0);
}

static void draw_titlewin() {
    color_base = STATUS_BASE + 1;
    werase(titlewin);
    set_color(titlewin, 7);
    int i = 1;
    waddstr(titlewin, " ");
    if (screens[screen].motd && screens[screen].motd[0]) {
        i += draw_colored(titlewin, screens[screen].motd);
        i += draw_colored(titlewin, " ");
    }
    if (screens[screen].level && screens[screen].level[0]) {
        i += draw_colored(titlewin, "^5[^7");
        i += draw_colored(titlewin, screens[screen].level);
        i += draw_colored(titlewin, "^5] ");
    }
    if (screens[screen].game && screens[screen].game[0]) {
        i += draw_colored(titlewin, "[^7");
        i += draw_colored(titlewin, screens[screen].game);
        i += draw_colored(titlewin, "^5] ");
    }
    if ((screens[screen].host && screens[screen].host[0]) || (screens[screen].port && screens[screen].port[0]))
        i += draw_colored(titlewin, "@ ^7");
    if (screens[screen].host && screens[screen].host[0])
        i += draw_colored(titlewin, screens[screen].host);
    if (screens[screen].port && screens[screen].port[0]) {
        i += draw_colored(titlewin, "^5:^7");
        i += draw_colored(titlewin, screens[screen].port);
    }
    for (; i < COLS; i++)
        waddch(titlewin, ' ');
    wrefresh(titlewin);
}

void set_title(int client, char *new_motd, char *new_level, char *new_game, char *new_host, char *new_port) {
    screens[client + 1].motd = new_motd;
    screens[client + 1].level = new_level;
    screens[client + 1].game = new_game;
    screens[client + 1].host = new_host;
    screens[client + 1].port = new_port;
    if (titlewin != NULL)
        draw_titlewin(client + 1);
}

static void draw_outwin() {
    color_base = NORMAL_BASE + 1;
    werase(outwin);
    int outheight = LINES - 3;
    int i;
    set_color(outwin, 7);
    int actual_buffer_count = screens[screen].buffer_count - (screens[screen].ghost_line ? 1 : 0);
    if (screens[screen].scroll_up > actual_buffer_count - outheight)
        screens[screen].scroll_up = actual_buffer_count - outheight;
    if (screens[screen].scroll_up < 0)
        screens[screen].scroll_up = 0;
    int start = max(0, actual_buffer_count - outheight);
    int displayable = actual_buffer_count - start;
    for (i = start; i < actual_buffer_count; i++) {
        wmove(outwin, i - start + outheight - displayable, 0);
        int index = i;
        index -= screens[screen].buffer_count - 1; // replace the last buffer with
        index += screens[screen].buffer_index; // the selected one
        index -= screens[screen].scroll_up;
        index += MAX_BUFFER_SIZE * 2; // negative modulo prevention
        index %= MAX_BUFFER_SIZE;
        draw_colored(outwin, screens[screen].buffer[index]);
    }
    wrefresh(outwin);
    screens[screen].redraw_outwin = qfalse;
}

void draw_statuswin() {
    color_base = STATUS_BASE + 1;
    werase(statuswin);
    set_color(statuswin, 7);
    static char string[32];
    int i = 0;
    i += draw_colored(statuswin, " ^5[^7");
    timestring(string);
    i += draw_colored(statuswin, string);
    i += draw_colored(statuswin, "^5]^7 ");
    if (screens[screen].last_name != NULL) {
        i += draw_colored(statuswin, "^5[^7");
        i += draw_colored(statuswin, screens[screen].last_name);
        i += draw_colored(statuswin, "^5]^7 ");
    }
    char number[3];
    qboolean first = qtrue;
    int j;
    for (j = 0; j < SCREENS; j++) {
        if (screens[j].updated) {
            if (first) {
                i += draw_colored(statuswin, "^5[^7Act: ^5");
                first = qfalse;
            } else {
                i += draw_colored(statuswin, "^5,");
            }
            sprintf(number, "%d", j);
            if (screens[j].important) {
                i += draw_colored(statuswin, "^7");
                wattron(statuswin, A_BOLD);
            }
            i += draw_colored(statuswin, number);
            wattroff(statuswin, A_BOLD);
        }
    }
    if (!first)
        i += draw_colored(statuswin, "^5] ");
    i += draw_colored(statuswin, "^5[^7");
    sprintf(number, "%d", screen);
    i += draw_colored(statuswin, number);
    if (screen == 0) {
        i += draw_colored(statuswin, ":STATUS");
    } else if (screens[screen].last_server != NULL && screens[screen].last_server[0]) {
        i += draw_colored(statuswin, ":^7");
        i += draw_colored(statuswin, screens[screen].last_server);
    }
    i += draw_colored(statuswin, "^5] ");
    for (; i < COLS; i++)
        waddch(statuswin, ' ');
    wrefresh(statuswin);
}

void set_status(int client, char *name, char *server) {
    screens[client + 1].last_name = name;
    screens[client + 1].last_server = server;
    draw_statuswin();
}

static int command_mode_prefix_length() {
    return screen == 0 ? 0 : 1;
}

static qboolean explicit_command_mode() {
    return screens[screen].commandline_length >= command_mode_prefix_length() && screens[screen].commandline[0] == '/';
}

static cmd_mode_t get_command_mode() {
    return explicit_command_mode() ? CM_EXECUTE : screens[screen].command_mode;
}

static int command_mode_actual_prefix_length() {
    return explicit_command_mode() ? 1 : 0;
}

int ui_client() {
    return screen - 1;
}

static void draw_inwin() {
    color_base = NORMAL_BASE + 1;
    werase(inwin);
    static int prefix_length = 4;
    wmove(inwin, 0, prefix_length);
    switch (get_command_mode()) {
        case CM_SAY:
            set_color(inwin, 2);
            break;
        case CM_SAY_TEAM:
            set_color(inwin, 3);
            break;
        default:
            set_color(inwin, 7);
            break;
    }
    draw_colored_cursored_scroll(inwin, screens[screen].commandline, screens[screen].commandline_cursor, COLS - prefix_length);
    qboolean skipped = draw_skip > 0;
    qboolean hidden = draw_skip + draw_max < draw_total_len;
    wmove(inwin, 0, 0);
    set_color(inwin, 7);
    wattron(inwin, A_BOLD);
    if (skipped)
        draw_colored(inwin, "<");
    else
        draw_colored(inwin, "[");
    switch (screens[screen].command_mode) {
        case CM_EXECUTE:
            draw_colored(inwin, "X");
            break;
        case CM_SAY:
            draw_colored(inwin, "S");
            break;
        case CM_SAY_TEAM:
            draw_colored(inwin, "T");
            break;
        case CM_CUSTOM:
            draw_colored(inwin, "C");
            break;
    }
    if (hidden)
        draw_colored(inwin, ">");
    else
        draw_colored(inwin, "]");
    draw_colored(inwin, " ");
    wattroff(inwin, A_BOLD);
    wrefresh(inwin);
}

static void redraw() {
    draw_titlewin();
    draw_outwin();
    draw_statuswin();
    draw_inwin();
}

void set_screen(int new_screen) {
    screen = new_screen;
    if (screens[screen].scroll_up == 0) {
        screens[screen].updated = qfalse;
        screens[screen].important = qfalse;
    }
    redraw();
}

static screen_t *ui_output_screen;

static void ui_output_char(char c);

static void check_next_line() {
    if (ui_output_screen->next_line) {
        if (ui_output_screen->buffer_count < MAX_BUFFER_SIZE)
            ui_output_screen->buffer_count++;
        ui_output_screen->buffer_index = (ui_output_screen->buffer_index + 1) % MAX_BUFFER_SIZE;
        if (ui_output_screen->scroll_up)
            ui_output_screen->scroll_up++;
        ui_output_screen->output_length = 0;
        ui_output_screen->visual_output_length = 0;
        ui_output_screen->next_line = qfalse;
        ui_output_screen->ghost_line = qtrue;
        if (!ui_output_screen->allow_time) {
            int i;
            for (i = 0; i < OUTPUT_PREFIX_LENGTH; i++)
                ui_output_char(' ');
        }
    }
}

static void schedule_next_line() {
    check_next_line();
    ui_output_screen->next_line = qtrue;
}

static void reserve_space(int places, int visual_places) {
    check_next_line();
    if (ui_output_screen->output_length + places >= MAX_OUTPUT_LENGTH - 1 || ui_output_screen->visual_output_length + visual_places > COLS) {
        schedule_next_line();
        if (visual_places > 0)
            check_next_line();
    }
}

static void add_char(char c) {
    ui_output_screen->buffer[ui_output_screen->buffer_index][ui_output_screen->output_length++] = c;
    ui_output_screen->buffer[ui_output_screen->buffer_index][ui_output_screen->output_length] = '\0';
}

static void ui_output_char(char c) {
    reserve_space(1, 1);
    add_char(c);
    ui_output_screen->visual_output_length++;
    ui_output_screen->allow_time = qfalse;
    if (c != ' ')
        ui_output_screen->ghost_line = qfalse;
}

static void ui_output_color(int color) {
    reserve_space(2, 0);
    add_char('^');
    add_char('0' + color);
}

static void ui_output_real(int client, char *string) {
    ui_output_screen = screens + client + 1;
    if (ui_output_screen->buffer_count == 0)
        ui_output_screen->next_line = qtrue;
    parse_state_t state;
    parse_init(&state, ui_output_char, ui_output_char, ui_output_color, '\n');
    char *s = string;
    do {
        s = parse_interleaved(s, &state);
        parse_finish(&state);
        if (s[-1] != '\0') {
            if (s - 1 != string && ghosted(s - 2))
                add_char('^');
            ui_output_color(7);
            schedule_next_line();
            ui_output_screen->allow_time = parse_empty_last(s);
        }
    } while(s[-1] != '\0');
}

void ui_output(int client, char *format, ...) {
    if (client == -2)
        client = screen - 1;
    static char string[65536];
    int len = 0;
    if (screens[client + 1].allow_time) {
        string[len++] = '^';
        string[len++] = '7';
        len += timestring(string + len);
        string[len++] = ' ';
    }

	va_list	argptr;
	va_start(argptr, format);
    vsprintf(string + len, format, argptr);
	va_end(argptr);
    ui_output_real(client, string);
    screens[client + 1].redraw_outwin = qtrue;
    if (client + 1 != screen || screens[client + 1].scroll_up != 0) {
        screens[client + 1].updated = qtrue;
        draw_statuswin();
    }
}

void ui_output_important(int client, char *format, ...) {
    if (client == -2)
        client = screen - 1;
    static char string[65536];
    string[0] = '^';
    string[1] = '7';
    int len = 2;
    if (screens[client + 1].allow_time) {
        len += timestring(string + len);
        string[len++] = ' ';
    }

	va_list	argptr;
	va_start(argptr, format);
    vsprintf(string + len, format, argptr);
	va_end(argptr);
    ui_output_real(client, string);
    screens[client + 1].redraw_outwin = qtrue;
    if (client + 1 != screen || screens[client + 1].scroll_up != 0) {
        screens[client + 1].updated = qtrue;
        screens[client + 1].important = qtrue;
        draw_statuswin();
    }
    notify_init("chatsow");
    static char title[] = "chatsow(x)";
    title[strlen(title) - 2] = '1' + client;
    char *message = uncolor(string + len);
    char *p;
    for (p = message + strlen(message) - 1; p >= message && *p == '\n'; p--)
        *p = '\0';
    NotifyNotification* notification = notify_notification_new(title, message, NULL);
    notify_notification_set_timeout(notification, 5000);
    notify_notification_show(notification, NULL);
    g_object_unref(G_OBJECT(notification));
}

static void screen_init(screen_t *s) {
    s->last_name = NULL;

    s->buffer_count = 0;
    s->ghost_line = qfalse;
    s->buffer_index = -1;
    s->output_length = 0;
    s->visual_output_length = 0;
    s->scroll_up = 0;
    s->next_line = qfalse;
    s->allow_time = qtrue;

    s->command_mode = s == screens ? CM_EXECUTE : CM_SAY;
    s->custom_command_mode[0] = '\0';

    s->commandline_length = 0;
    s->commandline_cursor = 0;

    s->history_count = 0;
    s->history_index = -1;
    s->history_lookup = -1;

    s->updated = qfalse;
    s->important = qfalse;

    s->redraw_outwin = qfalse;
}

static void add_history() {
    screen_t *s = screens + screen;
    s->history_lookup = -1;

    if (!strcmp(s->history[s->history_index], s->commandline))
        return;

    if (s->history_count < MAX_HISTORY)
        s->history_count++;
    s->history_index = (s->history_index + 1) % MAX_HISTORY;

    strcpy(s->history[s->history_index], s->commandline);
}

static void update_history() {
    screen_t *s = screens + screen;
    if (s->history_lookup == -1)
        return;
    strcpy(s->history[(s->history_index - s->history_lookup + MAX_HISTORY) % MAX_HISTORY], s->commandline);
}

static void move_history(int d) {
    if (d == 0)
        return;

    screen_t *s = screens + screen;
    if (s->history_lookup == -1 && d < 0)
        return;
    if (s->history_lookup == s->history_count - 1 && d > 0)
        return;

    if (s->history_lookup == -1)
        strcpy(s->backup, s->commandline);

    s->history_lookup += d;
    if (s->history_lookup > s->history_count - 1)
        s->history_lookup = s->history_count - 1;
    if (s->history_lookup < -1)
        s->history_lookup = -1;

    if (s->history_lookup == -1)
        strcpy(s->commandline, s->backup);
    else
        strcpy(s->commandline, s->history[(s->history_index - s->history_lookup + MAX_HISTORY) % MAX_HISTORY]);
    s->commandline_length = strlen(s->commandline);
    s->commandline_cursor = uncolored_length(s->commandline);
}

static void move_cursor(int d) {
    int len = uncolored_length(screens[screen].commandline);
    screens[screen].commandline_cursor += d;
    if (screens[screen].commandline_cursor > len)
        screens[screen].commandline_cursor = len;
    if (screens[screen].commandline_cursor < 0)
        screens[screen].commandline_cursor = 0;
}

static void delete(qboolean before) {
    int offset = before ? 1 : 0;
    int index = real_index(screens[screen].commandline, screens[screen].commandline_cursor);
    if (index >= offset && (before || index < screens[screen].commandline_length)) {
        int i;
        for (i = index - offset; screens[screen].commandline[i]; i++)
            screens[screen].commandline[i] = screens[screen].commandline[i + 1];
        screens[screen].commandline_length--;
        screens[screen].commandline_cursor = uncolored_index(screens[screen].commandline, index - offset);
    }
}

static void insert(char c) {
    if (screens[screen].commandline_length == MAX_INPUT_LENGTH - 1)
        return;

    int index = real_index(screens[screen].commandline, screens[screen].commandline_cursor);
    if (index >= 0) {
        int i;
        for (i = screens[screen].commandline_length - 1; i >= index; i--)
            screens[screen].commandline[i + 1] = screens[screen].commandline[i];
        screens[screen].commandline[index] = c;
        screens[screen].commandline_length++;
        screens[screen].commandline[screens[screen].commandline_length] = '\0';
        screens[screen].commandline_cursor = uncolored_index(screens[screen].commandline, index + 1);
    }
}

static char suggestions[MAX_CMDS][MAX_SUGGESTION_SIZE];
static int suggestion_count;
static int suggesting_offset;

static qboolean suggestion_remove_test(void *x) {
    char *suggestion = (char *)x;
    int i;
    for (i = 0; suggestions[i] != suggestion; i++) {
        if (!strcmp(suggestion, suggestions[i]))
            return qtrue;
    }
    return qfalse;
}

static qboolean apply_suggestions(qboolean complete_partial) {
    qsort(suggestions, suggestion_count, MAX_SUGGESTION_SIZE, insensitive_cmp);
    rm(suggestions, sizeof(suggestions[0]), &suggestion_count, suggestion_remove_test);

    if (suggestion_count == 0)
        return qfalse;

    int minlen = -1;
    int i;
    for (i = 0; i < suggestion_count; i++) {
        int len = strlen(suggestions[i]);
        if (minlen < 0 || len < minlen)
            minlen = len;
    }

    int existing = real_index(screens[screen].commandline, screens[screen].commandline_cursor) - suggesting_offset;
    if (suggestion_count == 1 || (complete_partial && minlen >= -1 && minlen > existing)) {
        for (i = 0; i < existing; i++)
            delete(qtrue);
        qboolean valid = qtrue;
        for (i = 0; valid; i++) {
            char c = '\0';
            int j;
            for (j = 0; j < suggestion_count; j++) {
                if (i >= strlen(suggestions[j]) || (c != '\0' && tolower(suggestions[j][i]) != tolower(c)))
                    valid = qfalse;
                else
                    c = suggestions[j][i];
            }
            if (c == '\0')
                valid = qfalse;
            if (valid)
                insert(c);
        }
    }
    if (suggestion_count > 1) {
        ui_output(screen - 1, "^5Possibilities:\n");
        columnifier_t c;
        columnifier_init(&c, COLS - OUTPUT_PREFIX_LENGTH, MAX_OUTPUT_LENGTH - OUTPUT_PREFIX_LENGTH);
        for (i = 0; i < suggestion_count; i++)
            columnifier_preprocess(&c, suggestions[i]);
        static char temp[MAX_SUGGESTION_SIZE + 1];
        for (i = 0; i < suggestion_count; i++) {
            columnifier_process(&c, temp, suggestions[i]);
            ui_output(screen - 1, "%s", temp);
        }
        columnifier_finish(&c, temp);
        ui_output(screen - 1, "%s", temp);
    }
    screens[screen].commandline[screens[screen].commandline_length] = '\0';
    return suggestion_count == 1;
}

static void complete_command() {
    suggesting_offset = command_mode_actual_prefix_length();
    int c = real_index(screens[screen].commandline, screens[screen].commandline_cursor);
    char backup = screens[screen].commandline[c];
    screens[screen].commandline[screens[screen].commandline_cursor] = '\0';
    suggestion_count = cmd_suggest(screen - 1, screens[screen].commandline + suggesting_offset, c, suggestions, qfalse);
    screens[screen].commandline[screens[screen].commandline_cursor] = backup;
    if (apply_suggestions(qtrue) && screens[screen].commandline[real_index(screens[screen].commandline, screens[screen].commandline_cursor)] != ' ')
        insert(' ');
}

static void complete_chat() {
    if (screen == 0)
        return;

    int c = real_index(screens[screen].commandline, screens[screen].commandline_cursor);
    for (suggesting_offset = c - 1; suggesting_offset >= 0; suggesting_offset--) {
        if (screens[screen].commandline[suggesting_offset] == ' ')
            break;
    }
    suggesting_offset++;
    char backup = screens[screen].commandline[c];
    if (suggesting_offset == 0 && screens[screen].commandline[suggesting_offset] == '!') {
        suggesting_offset++;
        screens[screen].commandline[screens[screen].commandline_cursor] = '\0';
        suggestion_count = cmd_suggest(screen - 1, screens[screen].commandline + suggesting_offset, c, suggestions, qtrue);
        screens[screen].commandline[screens[screen].commandline_cursor] = backup;
        if (apply_suggestions(qtrue))
            insert(' ');
    } else {
        char backup = screens[screen].commandline[c];
        screens[screen].commandline[c] = '\0';
        suggestion_count = player_suggest(screen - 1, screens[screen].commandline + suggesting_offset, suggestions);
        screens[screen].commandline[c] = backup;
        if (apply_suggestions(qfalse)) {
            insert('^');
            insert('2');
        }
    }
}

static void remove_subwindows() {
    if (titlewin)
        delwin(titlewin);
    if (outwin)
        delwin(outwin);
    if (statuswin)
        delwin(statuswin);
    if (inwin)
        delwin(inwin);
}

static void redesign() {
    if (!mainwin)
        return;

    remove_subwindows();

    titlewin = subwin(mainwin, 1, COLS, 0, 0);

    outwin = subwin(mainwin, LINES - 3, COLS, 1, 0);
    scrollok(outwin, TRUE);

    statuswin = subwin(mainwin, 1, COLS, LINES - 2, 0);

    inwin = subwin(mainwin, 1, COLS, LINES - 1, 0);
    keypad(inwin, TRUE);
    wtimeout(inwin, INPUT_TIME);

    redraw();
}

static void ui_execute(int client, qboolean output, char *format, ...) {
    static char string[MAX_STRING_CHARS];
	va_list	argptr;
	va_start(argptr, format);
    vsprintf(string, format, argptr);
	va_end(argptr);

    if (output)
        ui_output(client, "/%s\n", string);
    cmd_execute(client, string);
}

void ui_run() {
    signal(SIGINT, interrupt);
    signal(SIGSEGV, interrupt);

    mainwin = initscr();
    init_colors();
    nonl();
    cbreak();
    noecho();
    curs_set(0);

    set_title(-1, NULL, NULL, NULL, NULL, NULL);

    register_general_commands();
    client_register_commands();
    int i;
    for (i = 0; i < SCREENS; i++) {
        screen_init(screens + i);
        if (i > 0)
            client_start(i - 1);
    }

    serverlist_init();
    irc_init();

    qboolean alt = qfalse;
    int last_cols = 0;
    int last_lines = 0;
    int last_status = 0;
    qboolean first = qtrue;
    for (;;) {
        if (last_cols != COLS || last_lines != LINES) {
            redesign();
            last_cols = COLS;
            last_lines = LINES;
        }
        if (first) {
            cmd_execute(screen - 1, "exec config.cfg");
            first = qfalse;
        }
        if (stopped)
            break;
        int m = millis();
        if (last_status == 0 || m / 60000 >= last_status / 60000 + 1) {
            draw_statuswin();
            last_status = m;
        }
        if (screens[screen].redraw_outwin)
            draw_outwin();
        serverlist_frame();
        for (i = 0; i < CLIENT_SCREENS; i++)
            client_frame(i);
        irc_recv();
        plugin_frame();
        int c = wgetch(inwin);
        if (c == -1)
            continue;
        if (alt) {
            if (c >= '0' && c <= '9')
                set_screen(c - '0');
            else if (c == '!')
                set_screen(10);
            else if (c == '@')
                set_screen(11);
            else if (c == '#')
                set_screen(12);
            else if (c == '$')
                set_screen(13);
            else if (c == '%')
                set_screen(14);
            else if (c == '^')
                set_screen(15);
            qboolean refresh_input = qtrue;
            if (c == 120) {
                screens[screen].command_mode = CM_EXECUTE;
            } else if (c == 115) {
                screens[screen].command_mode = CM_SAY;
            } else if (c == 116) {
                screens[screen].command_mode = CM_SAY_TEAM;
            } else if (c == 99) {
                screens[screen].command_mode = CM_CUSTOM;
                if (screens[screen].commandline_length > 0)
                    strcpy(screens[screen].custom_command_mode, screens[screen].commandline + command_mode_actual_prefix_length());
            } else {
                refresh_input = qfalse;
            }
            if (refresh_input) {
                screens[screen].commandline_length = 0;
                screens[screen].commandline[screens[screen].commandline_length] = '\0';
                screens[screen].commandline_cursor = uncolored_length(screens[screen].commandline);
                draw_inwin();
            }
            alt = qfalse;
            continue;
        }
        bool handled = TRUE;
        switch (c) {
            case 11:
                screens[screen].scroll_up++;
                break;
            case 10:
                screens[screen].scroll_up--;
                break;
            case KEY_PPAGE:
                screens[screen].scroll_up += SCROLL_SPEED;
                break;
            case KEY_NPAGE:
                screens[screen].scroll_up -= SCROLL_SPEED;
                if (screens[screen].scroll_up <= 0) {
                    screens[screen].updated = qfalse;
                    screens[screen].important = qfalse;
                    draw_statuswin();
                }
                break;
            default:
                handled = FALSE;
                break;
        }
        if (handled) {
            draw_outwin();
            continue;
        }
        switch (c) {
            case 27:
                alt = qtrue;
                continue;
            case 263:
            case 127:
            case 8:
                delete(qtrue);
                break;
            case 330:
                delete(qfalse);
                break;
            case 258:
                move_history(-1);
                break;
            case 259:
                move_history(1);
                break;
            case 5:
                update_history();
                break;
            case 260:
                move_cursor(-1);
                break;
            case 261:
            case 12:
                move_cursor(1);
                break;
            case 262:
                screens[screen].commandline_cursor = 0;
                break;
            case 360:
                screens[screen].commandline_cursor = uncolored_length(screens[screen].commandline);
                break;
            case 21:
                screens[screen].commandline_length = 0;
                screens[screen].commandline[screens[screen].commandline_length] = '\0';
                screens[screen].commandline_cursor = uncolored_length(screens[screen].commandline);
                break;
            case 9:
                switch (get_command_mode()) {
                    case CM_EXECUTE:
                        if (screens[screen].commandline_cursor >= command_mode_prefix_length())
                            complete_command();
                        break;
                    case CM_SAY:
                    case CM_SAY_TEAM:
                        complete_chat();
                        break;
                    default:
                        break;
                }
                break;
            case 13:
                screens[screen].commandline[screens[screen].commandline_length] = '\0';
                screens[screen].scroll_up = 0;
                int old_screen = screen;
                if (screens[screen].commandline_length > (get_command_mode() == CM_EXECUTE ? command_mode_prefix_length() : 0)) {
                    add_history();
                    switch (get_command_mode()) {
                        case CM_EXECUTE:
                            ui_execute(screen - 1, true, "%s", screens[screen].commandline + command_mode_actual_prefix_length());
                            break;
                        case CM_SAY:
                            client_say(screen - 1, "%s", screens[screen].commandline);
                            break;
                        case CM_SAY_TEAM:
                            client_say_team(screen - 1, "%s", screens[screen].commandline);
                            break;
                        case CM_CUSTOM:
                            ui_execute(screen - 1, false, "%s %s", screens[screen].custom_command_mode, screens[screen].commandline);
                            break;
                    }
                }
                screens[old_screen].commandline_length = 0;
                screens[old_screen].commandline[screens[old_screen].commandline_length] = '\0';
                screens[old_screen].commandline_cursor = uncolored_length(screens[old_screen].commandline);
                break;
            case 410:
                break;
            default:
                insert(c);
                break;
        }
        draw_inwin();
    }
}
