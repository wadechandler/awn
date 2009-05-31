/*
 * Copyright (C) 2008 Neil Jagdish Patel <njpatel@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as 
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by Neil Jagdish Patel <njpatel@gmail.com>
 *
 */

#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "taskmanager-marshal.h"
#include "task-drag-indicator.h"
#include "task-settings.h"

G_DEFINE_TYPE (TaskDragIndicator, task_drag_indicator, AWN_TYPE_ICON)

enum {
        TARGET_TASK_ICON
};

static const GtkTargetEntry drop_types[] = 
{
  { (gchar*)"awn/task-icon", 0, TARGET_TASK_ICON }
};
static const gint n_drop_types = G_N_ELEMENTS (drop_types);

/* FORWARDS */
static gboolean task_drag_indicator_expose (GtkWidget *widget, GdkEventExpose *event);
static void     task_drag_indicator_drag_data_get (GtkWidget *widget, 
                                                   GdkDragContext *context, 
                                                   GtkSelectionData *selection_data,
                                                   guint target_type, 
                                                   guint time_);
/* DnD 'destination' forwards */
static gboolean  task_drag_indicator_dest_drag_motion         (GtkWidget      *widget,
                                                               GdkDragContext *context,
                                                               gint            x,
                                                               gint            y,
                                                               guint           t);
static void      task_drag_indicator_dest_drag_leave          (GtkWidget      *widget,
                                                               GdkDragContext *context,
                                                               guint           time_);
static void      task_drag_indicator_dest_drag_data_received  (GtkWidget      *widget,
                                                               GdkDragContext *context,
                                                               gint            x,
                                                               gint            y,
                                                               GtkSelectionData *data,
                                                               guint           info,
                                                               guint           time_);

enum
{
  PROP_0,
};

enum
{
  DEST_DRAG_MOVE,
  DEST_DRAG_LEAVE,

  LAST_SIGNAL
};
static guint32 _icon_signals[LAST_SIGNAL] = { 0 };

/* GObject stuff */
static void
task_drag_indicator_finalize (GObject *object)
{
  G_OBJECT_CLASS (task_drag_indicator_parent_class)->finalize (object);
}

static void
task_drag_indicator_constructed (GObject *object)
{

}

static void
task_drag_indicator_class_init (TaskDragIndicatorClass *klass)
{
  GObjectClass   *obj_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *wid_class = GTK_WIDGET_CLASS (klass);

  obj_class->constructed  = task_drag_indicator_constructed;
  obj_class->finalize     = task_drag_indicator_finalize;

  wid_class->expose_event         = task_drag_indicator_expose;
  wid_class->drag_data_get        = task_drag_indicator_drag_data_get;
  wid_class->drag_data_received   = task_drag_indicator_dest_drag_data_received;
  wid_class->drag_motion          = task_drag_indicator_dest_drag_motion;
  wid_class->drag_leave           = task_drag_indicator_dest_drag_leave;

  /* Install signals */
  _icon_signals[DEST_DRAG_MOVE] =
		g_signal_new ("dest-drag-motion",
			      G_OBJECT_CLASS_TYPE (obj_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TaskDragIndicatorClass, dest_drag_motion),
			      NULL, NULL,
			      taskmanager_marshal_VOID__INT_INT, 
			      G_TYPE_NONE, 2,
            G_TYPE_INT, G_TYPE_INT);
  _icon_signals[DEST_DRAG_LEAVE] =
		g_signal_new ("dest-drag-leave",
			      G_OBJECT_CLASS_TYPE (obj_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TaskDragIndicatorClass, dest_drag_leave),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID, 
			      G_TYPE_NONE, 0);
}

static void
task_drag_indicator_init (TaskDragIndicator *drag_indicator)
{
  TaskSettings *settings;

  settings = task_settings_get_default ();

  awn_icon_set_orientation (AWN_ICON (drag_indicator), AWN_ORIENTATION_BOTTOM);
  awn_icon_set_custom_paint (AWN_ICON (drag_indicator), settings->panel_size, settings->panel_size);

  /* D&D accept dragged objs */
  gtk_widget_add_events (GTK_WIDGET (drag_indicator), GDK_ALL_EVENTS_MASK);
  gtk_drag_dest_set (GTK_WIDGET (drag_indicator), 
                     GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
                     drop_types, n_drop_types,
                     GDK_ACTION_MOVE);
  /*gtk_drag_dest_add_uri_targets (GTK_WIDGET (icon));
  gtk_drag_dest_add_text_targets (GTK_WIDGET (icon));*/
}

GtkWidget *
task_drag_indicator_new ()
{
  GtkWidget *drag_indicator = NULL;

  drag_indicator = g_object_new (TASK_TYPE_DRAG_INDICATOR, NULL);

  return drag_indicator;
}

/**
 * task_drag_indicator_expose
 * 
 * This function draws nothing atm,
 * but it will draw the AwnIcon with an opacity of 0.5.
 *
 * @return: TRUE to stop other handlers from being invoked for the event. 
 *          FALSE to propagate the event further. 
 */
static gboolean
task_drag_indicator_expose (GtkWidget *widget, GdkEventExpose *event)
{
  cairo_t      *cr;
  AwnEffects   *effects;

  effects = awn_icon_get_effects (AWN_ICON (widget));
  cr = awn_effects_cairo_create (effects);

  cairo_destroy (cr);

  return FALSE;
}

void
task_drag_indicator_refresh (TaskDragIndicator      *drag_indicator)
{
  TaskSettings *settings;

  g_return_if_fail (TASK_IS_DRAG_INDICATOR (drag_indicator));

  settings = task_settings_get_default ();

  awn_icon_set_custom_paint (AWN_ICON (drag_indicator), settings->panel_size, settings->panel_size);
}

static void
task_drag_indicator_drag_data_get (GtkWidget *widget, 
                                   GdkDragContext *context, 
                                   GtkSelectionData *selection_data,
                                   guint target_type, 
                                   guint time_)
{
  switch(target_type)
  {
    case TARGET_TASK_ICON:
      gtk_selection_data_set (selection_data, GDK_TARGET_STRING, 8, NULL, 0);
      break;
    default:
      /* Default to some a safe target instead of fail. */
      g_assert_not_reached ();
  }
}

/* DnD 'destination' forwards */

static gboolean
task_drag_indicator_dest_drag_motion (GtkWidget      *widget,
                                      GdkDragContext *context,
                                      gint            x,
                                      gint            y,
                                      guint           t)
{
  GdkAtom target;
  gchar *target_name;

  g_return_val_if_fail (TASK_IS_DRAG_INDICATOR (widget), FALSE);

  target = gtk_drag_dest_find_target (widget, context, NULL);
  target_name = gdk_atom_name (target);

  if (g_strcmp0("awn/task-icon", target_name) == 0)
  {
    gdk_drag_status (context, GDK_ACTION_MOVE, t);

    g_signal_emit (TASK_DRAG_INDICATOR (widget), _icon_signals[DEST_DRAG_MOVE], 0, x, y);
    return TRUE;
  }
  return FALSE;
}

static void   
task_drag_indicator_dest_drag_leave (GtkWidget      *widget,
                           GdkDragContext *context,
                           guint           time_)
{
  g_return_if_fail (TASK_IS_DRAG_INDICATOR (widget));

  g_signal_emit (TASK_DRAG_INDICATOR (widget), _icon_signals[DEST_DRAG_LEAVE], 0);
}

static void
task_drag_indicator_dest_drag_data_received (GtkWidget      *widget,
                                             GdkDragContext *context,
                                             gint            x,
                                             gint            y,
                                             GtkSelectionData *sdata,
                                             guint           info,
                                             guint           time_)
{
  gtk_drag_finish (context, TRUE, TRUE, time_);
}