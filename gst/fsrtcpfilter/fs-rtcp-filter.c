/*
 * Farstream Voice+Video library
 *
 *  Copyright 2008-2012 Collabora Ltd,
 *  Copyright 2008 Nokia Corporation
 *   @author: Olivier Crete <olivier.crete@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

/**
 * SECTION:element-fsrtcpfilter
 * @short_description: Removes the framerate from video caps
 *
 * This element will remove the framerate from video caps, it is a poor man's
 * videorate for live pipelines.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fs-rtcp-filter.h"

#include <gst/rtp/gstrtcpbuffer.h>

#include <string.h>

GST_DEBUG_CATEGORY (rtcp_filter_debug);
#define GST_CAT_DEFAULT (rtcp_filter_debug)

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtcp"));

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtcp"));

/* signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SENDING
};

static void fs_rtcp_filter_get_property (GObject *object,
    guint prop_id,
    GValue *value,
    GParamSpec *pspec);
static void fs_rtcp_filter_set_property (GObject *object,
    guint prop_id,
    const GValue *value,
    GParamSpec *pspec);

static GstFlowReturn
fs_rtcp_filter_transform_ip (GstBaseTransform *transform, GstBuffer *buf);

G_DEFINE_TYPE (FsRtcpFilter, fs_rtcp_filter, GST_TYPE_BASE_TRANSFORM);

static void
fs_rtcp_filter_class_init (FsRtcpFilterClass *klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *gstbasetransform_class;

  gobject_class = (GObjectClass *) klass;
  gstbasetransform_class = (GstBaseTransformClass *) klass;

  GST_DEBUG_CATEGORY_INIT
    (rtcp_filter_debug, "fsrtcpfilter", 0, "fsrtcpfilter");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&srctemplate));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sinktemplate));

  gst_element_class_set_metadata (gstelement_class,
      "RTCP Filter element",
      "Filter",
      "This element removes unneeded parts of rtcp buffers",
      "Olivier Crete <olivier.crete@collabora.com>");

  gobject_class->set_property = fs_rtcp_filter_set_property;
  gobject_class->get_property = fs_rtcp_filter_get_property;

  gstbasetransform_class->transform_ip = fs_rtcp_filter_transform_ip;

  g_object_class_install_property (gobject_class,
      PROP_SENDING,
      g_param_spec_boolean ("sending",
          "Sending RTP?",
          "If set to FALSE, it assumes that all RTP has been dropped",
          FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
fs_rtcp_filter_init (FsRtcpFilter *rtcpfilter)
{
  rtcpfilter->sending = FALSE;
}

static void
fs_rtcp_filter_get_property (GObject *object,
    guint prop_id,
    GValue *value,
    GParamSpec *pspec)
{
  FsRtcpFilter *filter = FS_RTCP_FILTER (object);

  switch (prop_id)
  {
    case PROP_SENDING:
      GST_OBJECT_LOCK (filter);
      g_value_set_boolean (value, filter->sending);
      GST_OBJECT_UNLOCK (filter);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
static void
fs_rtcp_filter_set_property (GObject *object,
    guint prop_id,
    const GValue *value,
    GParamSpec *pspec)
{
  FsRtcpFilter *filter = FS_RTCP_FILTER (object);

  switch (prop_id)
  {
    case PROP_SENDING:
      GST_OBJECT_LOCK (filter);
      filter->sending = g_value_get_boolean (value);
      GST_OBJECT_UNLOCK (filter);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
fs_rtcp_filter_transform_ip (GstBaseTransform *transform, GstBuffer *buf)
{
  FsRtcpFilter *filter = FS_RTCP_FILTER (transform);

  if (!gst_rtcp_buffer_validate (buf))
  {
    GST_ERROR_OBJECT (transform, "Invalid RTCP buffer");
    return GST_FLOW_ERROR;
  }

  GST_OBJECT_LOCK (filter);

  if (!filter->sending)
  {
    GstRTCPBuffer rtcpbuffer = GST_RTCP_BUFFER_INIT;
    GstRTCPPacket packet;

    gst_rtcp_buffer_map (buf, GST_MAP_READWRITE, &rtcpbuffer);

    if (gst_rtcp_buffer_get_first_packet (&rtcpbuffer, &packet))
    {
      for (;;)
      {
        if (gst_rtcp_packet_get_type (&packet) == GST_RTCP_TYPE_SR)
        {
          GstRTCPPacket nextpacket = packet;

          if (gst_rtcp_packet_move_to_next (&nextpacket) &&
              gst_rtcp_packet_get_type (&nextpacket) == GST_RTCP_TYPE_RR)
          {
            if (!gst_rtcp_packet_remove (&packet))
              break;
          }
          else
          {
            guchar *data = rtcpbuffer.map.data + packet.offset;

            /* If there is no RR, lets add an empty one */
            data[0] = (GST_RTCP_VERSION << 6);
            data[1] = GST_RTCP_TYPE_RR;
            data[2] = 0;
            data[3] = 1;
            memmove (rtcpbuffer.map.data + packet.offset + 8,
                rtcpbuffer.map.data + nextpacket.offset,
                rtcpbuffer.map.size - nextpacket.offset);

            rtcpbuffer.map.size -= nextpacket.offset - packet.offset - 8 ;

            if (!gst_rtcp_buffer_get_first_packet (&rtcpbuffer, &packet))
              break;
          }

        }
        else
        {
          if (!gst_rtcp_packet_move_to_next (&packet))
            break;
        }
      }
    }
    gst_rtcp_buffer_unmap (&rtcpbuffer);
  }

  GST_OBJECT_UNLOCK (filter);

  return GST_FLOW_OK;
}

gboolean
fs_rtcp_filter_plugin_init (GstPlugin *plugin)
{
  return gst_element_register (plugin, "fsrtcpfilter",
      GST_RANK_MARGINAL, FS_TYPE_RTCP_FILTER);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    fsrtcpfilter,
    "RtcpFilter",
    fs_rtcp_filter_plugin_init, VERSION, "LGPL", "Farstream",
    "http://www.freedesktop.org/wiki/Software/Farstream")
