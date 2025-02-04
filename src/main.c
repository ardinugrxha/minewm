
/*
 * This file is part of minewm.
 *
 * minewm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * minewm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with minewm.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2025 Ardi Nugraha
 */

#include "mine.h"
#include <stdio.h>

int main(int argc, char **argv) {
  gtk_init(&argc, &argv);

  WindowArranger arranger;

  arranger.xcb_conn = xcb_connect(NULL, NULL);
  if (xcb_connection_has_error(arranger.xcb_conn)) {
    fprintf(stderr, "Failed to connect to X server\n");
    return 1;
  }

  arranger.xcb_screen =
      xcb_setup_roots_iterator(xcb_get_setup(arranger.xcb_conn)).data;

  arranger.handle = wnck_handle_new(WNCK_CLIENT_TYPE_APPLICATION);
  arranger.screen = wnck_handle_get_default_screen(arranger.handle);
  wnck_screen_force_update(arranger.screen);
  arranger.tree = NULL;

  g_timeout_add(POLLING_INTERVAL, on_window_opened, &arranger);

  gtk_main();

  g_object_unref(arranger.handle);
  xcb_disconnect(arranger.xcb_conn);

  return 0;
}
