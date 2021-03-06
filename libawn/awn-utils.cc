/*
 *  Copyright (C) 2009 Michal Hruby <michal.mhr@gmail.com>
 *  Copyright (C) 2009 Mark Lee <avant-wn@lazymalevolence.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Michal Hruby (awn_utils_ensure_transparent_bg,
 *                        awn_utils_make_transparent_bg)
 *          Mark Lee (awn_utils_gslist_to_gvaluearray)
 */
#include <math.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "awn-utils.h"
#include "awn-icon.h"
#include "awn-themed-icon.h"
#include "awn-applet.h"
#include "gseal-transition.h"

/*yes this is evil.  so sue me */
struct _GtkIconThemePrivate {
    guint custom_theme        : 1;
    guint is_screen_singleton : 1;
    guint pixbuf_supports_svg : 1;
    guint themes_valid        : 1;
    guint check_reload        : 1;

    char* current_theme;
    char* fallback_theme;
    char** search_path;
    int search_path_len;

    /* A list of all the themes needed to look up icons.
     * In search order, without duplicates
     */
    GList* themes;
    GHashTable* unthemed_icons;

    /* Note: The keys of this hashtable are owned by the
     * themedir and unthemed hashtables.
     */
    GHashTable* all_icons;

    /* GdkScreen for the icon theme (may be NULL)
     */
    GdkScreen* screen;

    /* time when we last stat:ed for theme changes */
    long last_stat_time;
    GList* dir_mtimes;

    gulong reset_styles_idle;
};

void
awn_utils_make_transparent_bg(GtkWidget* widget)
{
    static GdkPixmap* pixmap = NULL;
    GdkWindow* win;

    win = gtk_widget_get_window(widget);

    /* This can happen on Window manager restarts/changes */
    if (!win) {
        return;
    }

    if (gtk_widget_is_composited(widget)) {
        if (pixmap == NULL) {
            pixmap = gdk_pixmap_new(win, 1, 1, -1);
            cairo_t* cr = gdk_cairo_create(pixmap);
            cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
            cairo_paint(cr);
            cairo_destroy(cr);
        }
        gdk_window_set_back_pixmap(win, pixmap, FALSE);

        if (GTK_IS_VIEWPORT(widget)) {
            win = GTK_VIEWPORT(widget)->bin_window;
            gdk_window_set_back_pixmap(win, pixmap, FALSE);
        }
    }
}

static void
on_style_set(GtkWidget* widget, GtkStyle* prev_style)
{
    if (gtk_widget_get_realized(GTK_WIDGET(widget))) {
        awn_utils_make_transparent_bg(widget);
    }
}

static void
on_composited_change(GtkWidget* widget, gpointer data)
{
    if (gtk_widget_is_composited(widget)) {
        awn_utils_make_transparent_bg(widget);
    } else {
        // use standard background
        gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, NULL);
    }
}

void
awn_utils_ensure_transparent_bg(GtkWidget* widget)
{
    if (gtk_widget_get_realized(GTK_WIDGET(widget))) {
        awn_utils_make_transparent_bg(widget);
    }

    // make sure we don't connect the handler multiple times
    g_signal_handlers_disconnect_by_func(widget,
                                         G_CALLBACK(awn_utils_make_transparent_bg), NULL);
    g_signal_handlers_disconnect_by_func(widget,
                                         G_CALLBACK(on_style_set), NULL);
    g_signal_handlers_disconnect_by_func(widget,
                                         G_CALLBACK(on_composited_change), NULL);

    g_signal_connect(widget, "realize",
                     G_CALLBACK(awn_utils_make_transparent_bg), NULL);
    g_signal_connect(widget, "style-set",
                     G_CALLBACK(on_style_set), NULL);
    g_signal_connect(widget, "composited-changed",
                     G_CALLBACK(on_composited_change), NULL);
}

GValueArray*
awn_utils_gslist_to_gvaluearray(GSList* list)
{
    GValueArray* array;

    array = g_value_array_new(g_slist_length(list));
    for (GSList* n = list; n != NULL; n = n->next) {
        GValue val = {0};

        g_value_init(&val, G_TYPE_STRING);
        g_value_set_string(&val, (gchar*)n->data);
        g_value_array_append(array, &val);
        g_value_unset(&val);
    }

    return array;
}

gfloat
awn_utils_get_offset_modifier_by_path_type(AwnPathType path_type,
        GtkPositionType position,
        gint offset,
        gfloat offset_modifier,
        gint pos_x, gint pos_y,
        gint width, gint height)
{
    gfloat result, relative_pos;

    if (width == 0 || height == 0) {
        return offset;
    }

    switch (path_type) {
    case AWN_PATH_ELLIPSE:
        switch (position) {
        case GTK_POS_LEFT:
        case GTK_POS_RIGHT:
            relative_pos = pos_y * 1.0 / height;
            break;
        default:
            relative_pos = pos_x * 1.0 / width;
            break;
        }
        offset_modifier += sqrt(offset);  // let the max raise with higher offset
        result = sinf(M_PI * relative_pos);
        return result * offset_modifier + offset;
        break;
    default:
        return offset;
    }
}

void awn_utils_show_menu_images(GtkMenu* menu)
{
#if GTK_CHECK_VERSION (2,16,0)
    GList* i;
    GList* children = gtk_container_get_children(GTK_CONTAINER(menu));
    GtkWidget* submenu;

    for (i = children; i; i = g_list_next(i)) {
        if (GTK_IS_IMAGE_MENU_ITEM(i->data)) {
            g_object_set(i->data, "always-show-image", TRUE, NULL);
        }
        submenu = gtk_menu_item_get_submenu(i->data);
        if (submenu) {
            awn_utils_show_menu_images(GTK_MENU(submenu));
        }
    }
    g_list_free(children);
#endif
}

const gchar*
awn_utils_get_gtk_icon_theme_name(GtkIconTheme* theme)
{
    return theme->priv->current_theme;
}

void awn_utils_menu_set_position_widget_relative(GtkMenu* menu,
        gint* px,
        gint* py,
        gboolean* push_in,
        gpointer data)
{
    *push_in = TRUE;
#define PANEL_PADDING 5

    gint w, h, mw, mh, icon_size = 0;
    GtkWidget* widget = GTK_WIDGET(data);
    GtkPositionType pos = GTK_POS_BOTTOM;
    g_return_if_fail(widget);

    GtkRequisition r;
    gtk_widget_size_request(GTK_WIDGET(menu), &r);

    mw = r.width;
    mh = r.height;
    gint extra_offset = 0;

    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);

    w = alloc.width;
    h = alloc.height;

    gdk_window_get_origin(widget->window, px, py);

    if (AWN_IS_ICON(widget)) {
        AwnIcon* ico = AWN_ICON(widget);
        GtkWidget* app = gtk_widget_get_toplevel(widget);
        if (AWN_IS_APPLET(app)) {
            icon_size = awn_applet_get_size(AWN_APPLET(app));
        } else {
            g_warning("popup_gtk_menu: Cannot retrive icon size. AwnIcon is not a child of AwnApplet.");
            icon_size = (w + h) / 2;
        }
        pos = awn_icon_get_pos_type(ico);
        extra_offset = awn_icon_get_offset(ico);
    } else if (AWN_IS_APPLET(widget)) {
        AwnApplet* app = AWN_APPLET(widget);
        icon_size = awn_applet_get_size(app);
        pos = awn_applet_get_pos_type(app);
        extra_offset = awn_applet_get_offset_at(app,
                                                alloc.x + w / 2,
                                                alloc.y + h / 2);
    }

    extra_offset += PANEL_PADDING;
    switch (pos) {
    case GTK_POS_BOTTOM:
        *py = *py + h - icon_size - extra_offset - mh;
        break;
    case GTK_POS_TOP:
        *py = *py + icon_size + extra_offset;
        break;
    case GTK_POS_LEFT:
        *px = *px + icon_size + extra_offset;
        break;
    case GTK_POS_RIGHT:
        *px = *px + w - icon_size - extra_offset - mw;
        break;
    }
    /* fits to screen? */
    GdkScreen* screen = NULL;
    if (gtk_widget_has_screen(GTK_WIDGET(menu))) {
        screen = gtk_widget_get_screen(GTK_WIDGET(menu));
    } else {
        screen = gdk_screen_get_default();
    }
    if (screen) {
        gint screen_w = gdk_screen_get_width(screen);
        gint screen_h = gdk_screen_get_height(screen);
        *px = MIN(*px, screen_w - mw);
        *py = MIN(*py, screen_h - mh);
        if (*px < 0) {
            *px = 0;
        }
        if (*py < 0) {
            *py = 0;
        }
    }
}

