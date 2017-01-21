/* totem-missing-plugins.c

   Copyright (C) 2007 Tim-Philipp Müller <tim centricular net>

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301  USA.

   Author: Tim-Philipp Müller <tim centricular net>
 */

#include "config.h"

#include "bacon-video-widget-gst-missing-plugins.h"

#define GST_USE_UNSTABLE_API 1
#include <gst/gst.h> /* for gst_registry_update and functions in bacon_video_widget_gst_missing_plugins_blacklist */

#ifdef ENABLE_MISSING_PLUGIN_INSTALLATION

#include "bacon-video-widget.h"

#include <gst/pbutils/pbutils.h>
#include <gst/pbutils/install-plugins.h>

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_X11
#include <gtk/gtkx.h>
#endif

#include <string.h>

GST_DEBUG_CATEGORY_EXTERN (_totem_gst_debug_cat);
#define GST_CAT_DEFAULT _totem_gst_debug_cat

/* list of blacklisted detail strings */
static GList *blacklisted_plugins = NULL;

typedef struct
{
	gboolean   playing;
	gchar    **descriptions;
	gchar    **details;
	BaconVideoWidget *bvw;
}
TotemCodecInstallContext;

typedef struct _CodecInfo {
	gchar        *type_name;
	GstStructure *structure;
} CodecInfo;

#ifdef GDK_WINDOWING_X11
/* Adapted from totem-interface.c */
static Window
bacon_video_widget_gtk_plug_get_toplevel (GtkPlug *plug)
{
	Window root, parent, *children;
	guint nchildren;
	Window xid;

	g_return_val_if_fail (GTK_IS_PLUG (plug), 0);

	xid = gtk_plug_get_id (plug);

	do
	{
		/* FIXME: multi-head */
		if (XQueryTree (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), xid, &root,
					&parent, &children, &nchildren) == 0)
		{
			g_warning ("Couldn't find window manager window");
			return 0;
		}

		if (root == parent)
			return xid;

		xid = parent;
	}
	while (TRUE);
}

static Window
bacon_video_widget_gst_get_toplevel (GtkWidget *widget)
{
	GtkWidget *parent;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (widget));
	if (parent == NULL)
		return 0;

	if (GTK_IS_PLUG (parent))
		return bacon_video_widget_gtk_plug_get_toplevel (GTK_PLUG (parent));
	else
		return GDK_WINDOW_XID(gtk_widget_get_window (parent));
}
#endif

static gboolean
bacon_video_widget_gst_codec_install_plugin_is_blacklisted (const gchar * detail)
{
	/* We disable this blacklisting logic on Endless OS, as it's not useful
	 * on our system (as we don't support on-demand installation of plugins
	 * via systems such as PackageKit), and also because keeping it would
	 * complicate the logic to show a "missing codecs" dialog to the user. */
	return FALSE;
}

static void
bacon_video_widget_gst_codec_install_blacklist_plugin (const gchar * detail)
{
	/* We disable this blacklisting logic on Endless OS, as it's not useful
	 * on our system (as we don't support on-demand installation of plugins
	 * via systems such as PackageKit), and also because keeping it would
	 * complicate the logic to show a "missing codecs" dialog to the user. */
}

static void
bacon_video_widget_gst_codec_install_context_free (TotemCodecInstallContext *ctx)
{
	g_strfreev (ctx->descriptions);
	g_strfreev (ctx->details);
	g_free (ctx);
}

static void
on_plugin_installation_done (GstInstallPluginsReturn res, gpointer user_data)
{
	TotemCodecInstallContext *ctx = (TotemCodecInstallContext *) user_data;
	gchar **p;

	GST_INFO ("res = %d (%s)", res, gst_install_plugins_return_get_name (res));

	switch (res)
	{
		/* treat partial success the same as success; in the worst case we'll
		 * just do another round and get NOT_FOUND as result that time */
		case GST_INSTALL_PLUGINS_PARTIAL_SUCCESS:
		case GST_INSTALL_PLUGINS_SUCCESS:
			{
				/* blacklist installed plugins too, so that we don't get
				 * into endless installer loops in case of inconsistencies */
				for (p = ctx->details; p != NULL && *p != NULL; ++p)
					bacon_video_widget_gst_codec_install_blacklist_plugin (*p);

				bacon_video_widget_stop (ctx->bvw);
				g_message ("Missing plugins installed. Updating plugin registry ...");

				/* force GStreamer to re-read its plugin registry */
				if (gst_update_registry ())
				{
					g_message ("Plugin registry updated, trying again.");
					bacon_video_widget_play (ctx->bvw, NULL);
				} else {
					g_warning ("GStreamer registry update failed");
					/* FIXME: should we show an error message here? */
				}
			}
			break;
		case GST_INSTALL_PLUGINS_NOT_FOUND:
			{
				g_message ("No installation candidate for missing plugins found.");

				/* NOT_FOUND should only be returned if not a single one of the
				 * requested plugins was found; if we managed to play something
				 * anyway, we should just continue playing what we have and
				 * blacklist the requested plugins for this session; if we
				 * could not play anything we should blacklist them as well,
				 * so the install wizard isn't called again for nothing */
				for (p = ctx->details; p != NULL && *p != NULL; ++p)
					bacon_video_widget_gst_codec_install_blacklist_plugin (*p);

				if (ctx->playing)
				{
					bacon_video_widget_play (ctx->bvw, NULL);
				} else {
					/* wizard has not shown error, do stop/play again,
					 * so that an error message gets shown */
					bacon_video_widget_stop (ctx->bvw);
					bacon_video_widget_play (ctx->bvw, NULL);
				}
			}
			break;
		case GST_INSTALL_PLUGINS_USER_ABORT:
			{
				/* blacklist on user abort, so we show an error next time (or
				 * just play what we can) instead of calling the installer */
				for (p = ctx->details; p != NULL && *p != NULL; ++p)
					bacon_video_widget_gst_codec_install_blacklist_plugin (*p);

				if (ctx->playing) {
					bacon_video_widget_play (ctx->bvw, NULL);
				} else {
					/* if we couldn't play anything, do stop/play again,
					 * so that an error message gets shown */
					bacon_video_widget_stop (ctx->bvw);
					bacon_video_widget_play (ctx->bvw, NULL);
				}
			}
			break;
		case GST_INSTALL_PLUGINS_INVALID:
		case GST_INSTALL_PLUGINS_ERROR:
		case GST_INSTALL_PLUGINS_CRASHED:
		default:
			{
				g_message ("Missing plugin installation failed: %s",
				           gst_install_plugins_return_get_name (res));

				if (ctx->playing)
					bacon_video_widget_play (ctx->bvw, NULL);
				else
					bacon_video_widget_stop (ctx->bvw);
				break;
			}
		case GST_INSTALL_PLUGINS_STARTED_OK:
		case GST_INSTALL_PLUGINS_INTERNAL_FAILURE:
		case GST_INSTALL_PLUGINS_HELPER_MISSING:
		case GST_INSTALL_PLUGINS_INSTALL_IN_PROGRESS:
			{
				g_assert_not_reached ();
				break;
			}
	}

	bacon_video_widget_gst_codec_install_context_free (ctx);
}

#ifdef GDK_WINDOWING_X11
static void
set_startup_notification_id (GstInstallPluginsContext *install_ctx)
{
	gchar *startup_id;
	guint32 timestamp;

	timestamp = gtk_get_current_event_time ();
	startup_id = g_strdup_printf ("_TIME%u", timestamp);
	gst_install_plugins_context_set_startup_notification_id (install_ctx, startup_id);
	g_free (startup_id);
}
#endif

static gboolean
bacon_video_widget_start_plugin_installation (TotemCodecInstallContext *ctx,
                                              gboolean                  confirm_search)
{
	GstInstallPluginsContext *install_ctx;
	GstInstallPluginsReturn status;
#ifdef GDK_WINDOWING_X11
	GdkDisplay *display;
#endif

	install_ctx = gst_install_plugins_context_new ();
	gst_install_plugins_context_set_desktop_id (install_ctx, "org.gnome.Totem.desktop");
	gst_install_plugins_context_set_confirm_search (install_ctx, confirm_search);

#ifdef GDK_WINDOWING_X11
	display = gdk_display_get_default ();

	if (GDK_IS_X11_DISPLAY (display) &&
	    gtk_widget_get_window (GTK_WIDGET (ctx->bvw)) != NULL &&
	    gtk_widget_get_realized (GTK_WIDGET (ctx->bvw)))
	{
		gulong xid = 0;

		set_startup_notification_id (install_ctx);

		xid = bacon_video_widget_gst_get_toplevel (GTK_WIDGET (ctx->bvw));
		gst_install_plugins_context_set_xid (install_ctx, xid);
	}
#endif /* GDK_WINDOWING_X11 */

	status = gst_install_plugins_async ((const char * const*) ctx->details, install_ctx,
	                                    on_plugin_installation_done,
	                                    ctx);

	gst_install_plugins_context_free (install_ctx);

	GST_INFO ("gst_install_plugins_async() result = %d", status);

	if (status != GST_INSTALL_PLUGINS_STARTED_OK)
	{
		if (status == GST_INSTALL_PLUGINS_HELPER_MISSING)
		{
			g_message ("Automatic missing codec installation not supported "
			           "(helper script missing)");
		} else {
			g_warning ("Failed to start codec installation: %s",
			           gst_install_plugins_return_get_name (status));
		}
		bacon_video_widget_gst_codec_install_context_free (ctx);
		return FALSE;
	}

	return TRUE;
}

static gboolean
is_supported_codec_version (GstStructure *structure, const char *key, GType type, gpointer value)
{
	if (!gst_structure_has_field (structure, key)) {
		/* Could not find the target key */
		return FALSE;
	}

	/* We found the target key, check whether it's the right type */
	if (gst_structure_get_field_type (structure, key) != type)
		return FALSE;

	switch (type) {
	case G_TYPE_BOOLEAN: {
		gboolean value_found;
		gst_structure_get_boolean (structure, key, &value_found);
		return GPOINTER_TO_INT(value) == value_found;
	}
	case G_TYPE_INT: {
		gint value_found;
		gst_structure_get_int (structure, key, &value_found);
		return GPOINTER_TO_INT(value) == value_found;
	}
	case G_TYPE_STRING: {
		const char *value_found = gst_structure_get_string (structure, key);
		return g_strcmp0 ((const char*)value, value_found) == 0;
	}
	default:
		return FALSE;
	}
}

static gboolean
is_supported_codec (GstStructure *structure)
{
	const gchar *codec_name = gst_structure_get_name (structure);

	/* H.263 video decoder */
	if (g_strcmp0 (codec_name, "video/x-h263") == 0)
		return is_supported_codec_version (structure, "variant", G_TYPE_STRING, "itu");

	/* H.264 video decoder */
	if (g_strcmp0 (codec_name, "video/x-h264") == 0) {
		return TRUE;
	}

	/* MPEG-4 Part 2 video decoder */
	if (g_strcmp0 (codec_name, "video/x-divx") == 0) {
		return is_supported_codec_version (structure, "divxversion", G_TYPE_INT, GINT_TO_POINTER(4)) ||
		       is_supported_codec_version (structure, "divxversion", G_TYPE_INT, GINT_TO_POINTER(5));
	}

	/* MPEG-4 Part 2 video decoder */
	if (g_strcmp0 (codec_name, "video/mpeg") == 0) {
		return is_supported_codec_version (structure, "mpegversion", G_TYPE_INT, GINT_TO_POINTER(4)) &&
		       is_supported_codec_version (structure, "systemstream", G_TYPE_BOOLEAN, GINT_TO_POINTER(FALSE));
	}

	/* MPEG-4 audio decoders (AAC) */
	if (g_strcmp0 (codec_name, "audio/mpeg") == 0) {
		return is_supported_codec_version (structure, "mpegversion", G_TYPE_INT, GINT_TO_POINTER(2)) ||
		       is_supported_codec_version (structure, "mpegversion", G_TYPE_INT, GINT_TO_POINTER(4));
	}

	/* AC-3 audio decoder */
	if (g_strcmp0 (codec_name, "audio/x-ac3") == 0)
		return TRUE;

	/* Other codecs not supported by our codecs pack */
	return FALSE;
}

static CodecInfo *
codec_info_new (const gchar *codec)
{
	GstStructure *s;
	CodecInfo *info = NULL;
	g_autofree gchar *caps = NULL;
	g_autofree gchar *type_name = NULL;
	g_auto(GStrv) split = NULL;
	g_auto(GStrv) ss = NULL;

	split = g_strsplit (codec, "|", -1);
	if (split == NULL || g_strv_length (split) != 5) {
		g_warning ("Not a GStreamer codec line");
		return NULL;
	}
	if (g_strcmp0 (split[0], "gstreamer") != 0) {
		g_warning ("Not a GStreamer codec request");
		return NULL;
	}
	if (g_strcmp0 (split[1], "1.0") != 0) {
		g_warning ("Not recognised GStreamer version");
		return NULL;
	}

	if (g_str_has_prefix (split[4], "uri") != FALSE) {
		g_warning ("Couldn't find codec related information");
		return NULL;
	}

	ss = g_strsplit (split[4], "-", 2);
	type_name = g_strdup (ss[0]);
	caps = g_strdup (ss[1]);

	s = gst_structure_from_string (caps, NULL);
	if (s == NULL) {
		g_warning ("totem: failed to parse caps: %s", caps);
		return NULL;
	}

	info = g_new0 (CodecInfo, 1);
	info->type_name = g_steal_pointer (&type_name);
	info->structure = s;
	return info;
}

static void
codec_info_free (CodecInfo *codec)
{
	gst_structure_free (codec->structure);
	g_free (codec->type_name);
	g_free (codec);
}

static gboolean
missing_codecs_supported (const char **codecs)
{
	g_autoptr(GPtrArray) codecs_array = NULL;
	guint len, i;

	/* First of all filter the detailed strings for each missing codec
	 * so that we only consider those we're able to successfully parse. */
	len = g_strv_length ((GStrv)codecs);
	codecs_array = g_ptr_array_new_with_free_func ((GDestroyNotify) codec_info_free);
	for (i = 0; i < len; i++) {
		CodecInfo *info = codec_info_new (codecs[i]);
		if (info == NULL)
			continue;

		g_ptr_array_add (codecs_array, info);
	}

	/* No successfully parsed codecs strings, we can't tell */
	if (codecs_array->len == 0)
		return FALSE;

	/* All the codecs must be supported for us to be able to declare that
	 * we support the missing codecs with our codecs pack */
	for (i = 0; i < codecs_array->len; i++) {
		CodecInfo *info = g_ptr_array_index (codecs_array, i);

		if (info->structure == NULL)
			return FALSE;

		/* No encoders are supported */
		if (g_strcmp0 (info->type_name, "decoder") != 0)
			return FALSE;

		if (!is_supported_codec (info->structure))
			return FALSE;
	}

	/* All codecs supported if reached */
	return TRUE;
}

static void
codec_confirmation_dialog_response_cb (GtkDialog       *dialog,
                                       GtkResponseType  response_type,
                                       gpointer	 user_data)
{
	GError *error = NULL;
	const char * const *languages = g_get_language_names ();
	const char *shop_uri = NULL;

	switch (response_type) {
	case GTK_RESPONSE_ACCEPT:
		/* Different URLs for the three different shops */
		if (g_strv_contains (languages, "es"))
			shop_uri = "https://shop.endlessm.com/products/codecs";
		else if (g_strv_contains (languages, "pt"))
			shop_uri = "https://comprar.endlessm.com/products/codecs";
		else
			shop_uri = "https://endless-global.myshopify.com/products/codecs";

		if (gtk_show_uri (NULL, shop_uri, gtk_get_current_event_time (), &error) == FALSE) {
			g_warning ("Could not open shopify website: %s", error->message);
			g_error_free (error);
		}
		break;
	case GTK_RESPONSE_CANCEL:
	case GTK_RESPONSE_DELETE_EVENT:
		break;
	default:
		g_assert_not_reached ();
	}
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
show_codec_missing_information_dialog (TotemCodecInstallContext *ctx)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;
	const char *message_text;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (ctx->bvw));

	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
	                                 GTK_DIALOG_MODAL |
	                                 GTK_DIALOG_DESTROY_WITH_PARENT,
	                                 GTK_MESSAGE_ERROR,
	                                 GTK_BUTTONS_CANCEL,
	                                 _("Unable to play the file"));

	/* Show additional button pointing to our online store if the
	 * missing codec can be installed via our codecs activation key. */
	if (missing_codecs_supported ((const char**)ctx->details)) {
		GtkWidget *button;
		const char *button_text;

		message_text = _("This file type is currently unsupported. To support this type "
				 "of file, you can purchase our audio/video codec upgrade to add "
				 "support for additional file formats.");

		button_text = _("_Purchase Codec Upgrade");
		button = gtk_dialog_add_button (GTK_DIALOG (dialog),
						button_text,
						GTK_RESPONSE_ACCEPT);
		gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

		gtk_style_context_add_class (gtk_widget_get_style_context (button), "suggested-action");
	} else {
		message_text = _("This file type is unsupported.");
	}

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", message_text);
	g_signal_connect (dialog, "response", G_CALLBACK (codec_confirmation_dialog_response_cb), NULL);

	gtk_window_present (GTK_WINDOW (dialog));
}

static gboolean
bacon_video_widget_gst_on_missing_plugins_event (BaconVideoWidget  *bvw,
                                                 char             **details,
                                                 char             **descriptions,
                                                 gboolean           playing,
                                                 gpointer           user_data)
{
	TotemCodecInstallContext *ctx;
	guint i, num;

	num = g_strv_length (details);
	g_return_val_if_fail (num > 0 && g_strv_length (descriptions) == num, FALSE);

	ctx = g_new0 (TotemCodecInstallContext, 1);
	ctx->descriptions = g_strdupv (descriptions);
	ctx->details = g_strdupv (details);
	ctx->playing = playing;
	ctx->bvw = bvw;

	for (i = 0; i < num; ++i)
	{
		if (bacon_video_widget_gst_codec_install_plugin_is_blacklisted (ctx->details[i]))
		{
			g_message ("Missing plugin: %s (ignoring)", ctx->details[i]);
			g_free (ctx->details[i]);
			g_free (ctx->descriptions[i]);
			ctx->details[i] = ctx->details[num-1];
			ctx->descriptions[i] = ctx->descriptions[num-1];
			ctx->details[num-1] = NULL;
			ctx->descriptions[num-1] = NULL;
			--num;
			--i;
		} else {
			g_message ("Missing plugin: %s (%s)", ctx->details[i], ctx->descriptions[i]);
		}
	}

	if (num == 0)
	{
		g_message ("All missing plugins are blacklisted, doing nothing");
		bacon_video_widget_gst_codec_install_context_free (ctx);
		return FALSE;
	}

	/* if we managed to start playing, pause playback, since some install
	 * wizard should now take over in a second anyway and the user might not
	 * be able to use totem's controls while the wizard is running */
	if (playing)
		bacon_video_widget_pause (bvw);

	/* We don't have PackageKit available in Endless, so we always have to
	 * show an informative information dialog to the user at this point. */
	show_codec_missing_information_dialog (ctx);

	/* Also, we don't have any means to really install codecs automatically on
	 * Endless yet, but we still pretend we do so that we can record the attempt
	 * to play media with a missing codecs from a centralized point. */
	bacon_video_widget_start_plugin_installation (ctx, FALSE);

	return TRUE;
}

#endif /* ENABLE_MISSING_PLUGIN_INSTALLATION */

void
bacon_video_widget_gst_missing_plugins_setup (BaconVideoWidget *bvw)
{
#ifdef ENABLE_MISSING_PLUGIN_INSTALLATION
	g_signal_connect (G_OBJECT (bvw),
			"missing-plugins",
			G_CALLBACK (bacon_video_widget_gst_on_missing_plugins_event),
			bvw);

	gst_pb_utils_init ();

	GST_INFO ("Set up support for automatic missing plugin installation");
#endif
}

void
bacon_video_widget_gst_missing_plugins_blacklist (void)
{
	struct {
		const char *name;
		gboolean remove;
	} blacklisted_elements[] = {
		{ "ffdemux_flv", 0 },
		{ "avdemux_flv", 0 },
		{ "dvdreadsrc" , 1 }
	};
	GstRegistry *registry;
	guint i;

	registry = gst_registry_get ();

	for (i = 0; i < G_N_ELEMENTS (blacklisted_elements); ++i) {
		GstPluginFeature *feature;

		feature = gst_registry_find_feature (registry,
						     blacklisted_elements[i].name,
						     GST_TYPE_ELEMENT_FACTORY);

		if (!feature)
			continue;

		if (blacklisted_elements[i].remove)
			gst_registry_remove_feature (registry, feature);
		else
			gst_plugin_feature_set_rank (feature, GST_RANK_NONE);
	}
}

