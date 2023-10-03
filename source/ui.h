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

#ifndef WRLC_UI_H
#define WRLC_UI_H

#define SCREENS 16
#define CLIENT_SCREENS (SCREENS - 1)
#define CLIENTS CLIENT_SCREENS

#define MAX_SUGGESTION_SIZE MAX_ARG_SIZE

#include "cmd.h"

void ui_run();
void ui_stop();
int ui_client();
void set_screen(int new_screen);
void ui_output(int client, char *format, ...);
void ui_output_important(int client, char *format, ...);
void set_title(int client, char *new_motd, char *new_level, char *new_game, char *new_host, char *new_port);
void set_status(int client, char *name, char *server);

#endif
