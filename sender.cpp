#include <gst/gst.h>
#include <iostream>
#include <json.hpp>
using json = nlohmann::json;

using namespace std;

#ifdef SIMPLE_METADATA
typedef struct _metadata
{
    guint a;
    guint b;
    guint c;
    guint d;
} metadata;

void to_json(json& j, const metadata& m)
{
    j = json
    {
            {"a", m.a},
            {"b", m.b},
            {"c", m.c},
            {"d", m.d}
    };
}

void from_json(const json& j, metadata& m)
{
    j.at("a").get_to(m.a);
    j.at("b").get_to(m.b);
    j.at("c").get_to(m.c);
    j.at("d").get_to(m.d);
}

#else

typedef struct _SourceVideoPackage
{
    guint clientId;
    gfloat gazeAngle[2];
    gfloat headPoseQuaternion[4];
    gfloat headPoseTranslation[3];
    gfloat headScale;
    gfloat rawTrackingPoseEuler[3];
} SourceVideoPackage;


void to_json(json& j, const SourceVideoPackage& m)
{
    j = json
    {
            {"clientId", m.clientId},
            {"gazeAngle", m.gazeAngle},
            {"headPoseQuaternion", m.headPoseQuaternion},
            {"headPoseTranslation", m.headPoseTranslation},
            {"headScale", m.headScale},
            {"rawTrackingPoseEuler", m.rawTrackingPoseEuler}
    };
}

void from_json(const json& j, SourceVideoPackage& m)
{
    j.at("clientId").get_to(m.clientId);
    j.at("gazeAngle").get_to(m.gazeAngle);
    j.at("headPoseQuaternion").get_to(m.headPoseQuaternion);
    j.at("headPoseTranslation").get_to(m.headPoseTranslation);
    j.at("headScale").get_to(m.headScale);
    j.at("rawTrackingPoseEuler").get_to(m.rawTrackingPoseEuler);
}

#endif


static GstPadProbeReturn
rtpgstpay_sink_pad_probe(GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
    GstBuffer *buf = (GstBuffer *) info->data;
    GstMapInfo map;
    gst_buffer_map (buf, &map, GST_MAP_READ);
    g_print ("map.size = %lu\n", map.size);
    unsigned char *ptr = (unsigned char *) map.data;
    g_print ("first 8 bytes = %x %x %x %x %x %x %x %x \n", *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5), *(ptr+6), *(ptr +7));

    gsize size, maxsize, offset;
    size = gst_buffer_get_sizes (buf, &offset, &maxsize);
    g_print ("offset = %lu size = %lu maxsize=%lu\n", offset, size, maxsize);

#if 0
#define JSON_PAYLOAD_SIZE 32
    gst_buffer_resize (buf, 0, size+JSON_PAYLOAD_SIZE);

    memmove (ptr+JSON_PAYLOAD_SIZE, ptr, size);
    memset (ptr, 0xAB, JSON_PAYLOAD_SIZE);

    g_print ("after memmove offset JSON_PAYLOAD_SIZE first 8 bytes = %x %x %x %x %x %x %x %x \n", *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5), *(ptr+6), *(ptr +7));
#else
#ifdef SIMPLE_METADATA
    metadata md{100,200,300,400};

    json mdata(md);
    cout << mdata.dump() << " number of items = " << mdata.size() << endl;

    string tstr = mdata.dump();
    cout << "Total metadata size = " << tstr.size() << endl;

#else
    SourceVideoPackage svp{
        1,
        {0.0, 0.0},
        {-0.12370438128709793, 0.01204279437661171, -0.0015013896627351642, 0.9922448992729187},
        {-0.022162416949868202, -0.22363272309303284, -1.0241384506225586},
        0.5,
        {-0.11148426681756973, -0.039327509701251984, -0.018131190910935402}
    };

    json mdata(svp);
    cout << mdata.dump() << " number of items = " << mdata.size() << endl;

    string tstr = mdata.dump();
    cout << "Total metadata size = " << tstr.size() << endl;
#endif

    gst_buffer_resize (buf, 0, size + tstr.size());

    memmove (ptr+tstr.size(), ptr, size);
    memcpy (ptr, tstr.c_str(), tstr.size());

    g_print ("after memmove offset JSON_PAYLOAD_SIZE first 8 bytes = %x %x %x %x %x %x %x %x \n", *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5), *(ptr+6), *(ptr +7));
#endif

    return GST_PAD_PROBE_OK;
}

int main(int argc, char *argv[])
{
    GstElement *pipeline, *source, *parser, *decoder, *encoder, *rtpgstpay, *rtpstreampay, *tcpserversink;
    GstBus *bus;
    GstMessage *msg;
    GMainLoop *loop;

    /* Initialize GStreamer */
    gst_init (&argc, &argv);

    /* Create the elements */
    source = gst_element_factory_make("filesrc", NULL);
    parser = gst_element_factory_make("h264parse", NULL);
    decoder = gst_element_factory_make("nvv4l2decoder", NULL);
    encoder = gst_element_factory_make("nvv4l2h264enc", NULL);
    rtpgstpay = gst_element_factory_make("rtpgstpay", NULL);
    rtpstreampay = gst_element_factory_make("rtpstreampay", NULL);
    tcpserversink = gst_element_factory_make("tcpserversink", NULL);

    /* Create the empty pipeline */
    pipeline = gst_pipeline_new("sender-pipeline");

    if (!pipeline || !source || !parser || !decoder || !encoder || !rtpgstpay || !rtpstreampay || !tcpserversink) {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    g_object_set (tcpserversink, "host", "10.24.216.249", NULL);
    g_object_set (tcpserversink, "port", 8000, NULL);
    g_object_set (rtpgstpay, "config-interval", 1, NULL);

    /* Set the source's properties */
    g_object_set(source, "location", "/home/vpagar/sample_720p.h264", NULL);

    /* Build the pipeline */
    gst_bin_add_many(GST_BIN(pipeline), source, parser, decoder, encoder, rtpgstpay, rtpstreampay, tcpserversink, NULL);
    if (gst_element_link_many(source, parser, decoder, encoder, rtpgstpay, rtpstreampay, tcpserversink, NULL) != TRUE) {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    GstPad *rtpgstpay_sink_pad = gst_element_get_static_pad (rtpgstpay, "sink");
    gst_pad_add_probe (rtpgstpay_sink_pad, GST_PAD_PROBE_TYPE_BUFFER, rtpgstpay_sink_pad_probe, NULL, NULL);

    /* Start playing */
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    /* Wait until error or EOS */
    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    /* Free resources */
    gst_object_unref(GST_OBJECT(pipeline));
    g_main_loop_unref(loop);

    return 0;
}
