
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
#include "tree.h"
#include <stdio.h>

static void get_screen_size(int *width, int *height) {
  GdkDisplay *display = gdk_display_get_default();
  GdkMonitor *primary_monitor = gdk_display_get_primary_monitor(display);
  GdkRectangle monitor_geometry;
  gdk_monitor_get_geometry(primary_monitor, &monitor_geometry);
  *width = monitor_geometry.width;
  *height = monitor_geometry.height;
}

static int count_windows_in_workspace(WnckScreen *screen,
                                      WnckWorkspace *workspace) {
  GList *windows = wnck_screen_get_windows(screen);
  int count = 0;

  for (GList *l = windows; l != NULL; l = l->next) {
    WnckWindow *window = WNCK_WINDOW(l->data);
    if (!wnck_window_is_minimized(window) &&
        wnck_window_get_window_type(window) == WNCK_WINDOW_NORMAL &&
        wnck_window_get_workspace(window) == workspace) {
      count++;
    }
  }

  return count;
}

static WnckWorkspace *create_new_workspace(WindowArranger *arranger) {
  int current_count = wnck_screen_get_workspace_count(arranger->screen);

  xcb_connection_t *conn = arranger->xcb_conn;
  xcb_screen_t *screen = arranger->xcb_screen;

  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(
      conn, 0, strlen("_NET_NUMBER_OF_DESKTOPS"), "_NET_NUMBER_OF_DESKTOPS");
  xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, cookie, NULL);

  if (reply) {
    xcb_atom_t atom = reply->atom;
    uint32_t new_count = current_count + 1;

    // Send client message to change number of desktops
    xcb_client_message_event_t event = {.response_type = XCB_CLIENT_MESSAGE,
                                        .format = 32,
                                        .window = screen->root,
                                        .type = atom,
                                        .data.data32[0] = new_count};

    xcb_send_event(conn, 0, screen->root,
                   XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                       XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                   (char *)&event);
    xcb_flush(conn);
    free(reply);

    // Wait a bit for the workspace to be created
    g_usleep(500);

    WnckWorkspace *new_workspace =
        wnck_screen_get_workspace(arranger->screen, new_count);
    if (new_workspace) {
      return new_workspace;
    }
  }

  return NULL;
}

static void move_window_to_workspace(WnckWindow *window,
                                     WnckWorkspace *workspace) {
  wnck_window_move_to_workspace(window, workspace);
}

// Updated arrange_windows function using tree
static void arrange_windows(WindowArranger *arranger) {
  WnckWorkspace *active_workspace =
      wnck_screen_get_active_workspace(arranger->screen);
  if (!active_workspace)
    return;

  GList *windows = wnck_screen_get_windows(arranger->screen);
  GList *active_windows = NULL;
  int screen_width, screen_height;

  // Filter windows
  for (GList *l = windows; l != NULL; l = l->next) {
    WnckWindow *window = WNCK_WINDOW(l->data);
    if (!wnck_window_is_minimized(window) &&
        wnck_window_get_window_type(window) == WNCK_WINDOW_NORMAL &&
        wnck_window_get_workspace(window) == active_workspace) {
      active_windows = g_list_append(active_windows, window);
    }
  }

  if (active_windows == NULL)
    return;

  get_screen_size(&screen_width, &screen_height);

  // Create and populate window tree
  WindowTree *tree = init_window_tree(screen_width, screen_height);

  // Insert all windows into tree
  for (GList *l = active_windows; l != NULL; l = l->next) {
    insert_window(tree, WNCK_WINDOW(l->data));
  }

  // Apply the layout
  apply_tree_layout(tree->root);

  // Cleanup
  free_tree(tree->root);
  free(tree);
  g_list_free(active_windows);
}

static gboolean on_window_opened(gpointer data) {
  WindowArranger *arranger = (WindowArranger *)data;
  WnckScreen *screen = arranger->screen;

  if (!screen) {
    g_warning("Failed to retrieve WnckScreen.");
    return FALSE;
  }

  WnckWorkspace *active_workspace = wnck_screen_get_active_workspace(screen);
  if (!active_workspace) {
    g_warning("No active workspace found.");
    return FALSE;
  }

  GList *windows = wnck_screen_get_windows(screen);
  if (!windows) {
    g_warning("No windows found.");
    return FALSE;
  }

  WnckWindow *new_window = (WnckWindow *)g_list_last(windows)->data;
  if (!new_window) {
    g_warning("Failed to retrieve the last opened window.");
    return FALSE;
  }

  GArray *free_workspaces = g_array_new(FALSE, FALSE, sizeof(WnckWorkspace *));
  GArray *overloaded_workspaces =
      g_array_new(FALSE, FALSE, sizeof(WnckWorkspace *));

  int total_workspaces = wnck_screen_get_workspace_count(screen);
  for (int i = 0; i < total_workspaces; i++) {
    WnckWorkspace *workspace = wnck_screen_get_workspace(screen, i);
    if (!workspace)
      continue;

    int count = count_windows_in_workspace(screen, workspace);
    if (count > MAX_WINDOWS_PER_WORKSPACE) {
      g_array_append_val(overloaded_workspaces, workspace);
    } else if (count < MAX_WINDOWS_PER_WORKSPACE) {
      g_array_append_val(free_workspaces, workspace);
    }
  }

  // If no free workspaces exist, create a new one
  if (free_workspaces->len == 0) {
    create_new_workspace(arranger);
    WnckWorkspace *new_workspace =
        wnck_screen_get_workspace(screen, total_workspaces);
    g_array_append_val(free_workspaces, new_workspace);
  }

  // Process overloaded workspaces
  for (int i = 0; i < overloaded_workspaces->len; i++) {
    WnckWorkspace *overloaded =
        g_array_index(overloaded_workspaces, WnckWorkspace *, i);
    int excess_windows = count_windows_in_workspace(screen, overloaded) -
                         MAX_WINDOWS_PER_WORKSPACE;

    if (excess_windows > 0) {
      GList *workspace_windows = wnck_screen_get_windows(screen);
      GList *windows_to_move = NULL;

      for (GList *l = g_list_last(workspace_windows);
           l != NULL && excess_windows > 0; l = l->prev) {
        WnckWindow *window = (WnckWindow *)l->data;
        if (!window)
          continue;

        if (wnck_window_get_workspace(window) == overloaded) {
          windows_to_move = g_list_prepend(windows_to_move, window);
          excess_windows--;
        }
      }

      for (GList *m = windows_to_move; m != NULL; m = m->next) {
        WnckWindow *window_to_move = (WnckWindow *)m->data;
        WnckWorkspace *target_workspace =
            g_array_index(free_workspaces, WnckWorkspace *, 0);

        move_window_to_workspace(window_to_move, target_workspace);

        if (count_windows_in_workspace(screen, target_workspace) >=
            MAX_WINDOWS_PER_WORKSPACE) {
          g_array_remove_index(free_workspaces, 0);
          if (free_workspaces->len == 0) {
            create_new_workspace(arranger);
            target_workspace =
                wnck_screen_get_workspace(screen, total_workspaces);
            g_array_append_val(free_workspaces, target_workspace);
          }
        }
      }

      // Free the list of windows to move
      g_list_free(windows_to_move);
    }
  }

  // Cleanup
  g_array_free(free_workspaces, TRUE);
  g_array_free(overloaded_workspaces, TRUE);

  arrange_windows(arranger);

  return TRUE;
}

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

  g_timeout_add(POLLING_INTERVAL, on_window_opened, &arranger);

  gtk_main();

  g_object_unref(arranger.handle);
  xcb_disconnect(arranger.xcb_conn);

  return 0;
}
