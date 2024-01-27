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
rtpgstdepay_src_pad_probe(GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
    GstBuffer *buf = (GstBuffer *) info->data;
    GstMapInfo map;
    gst_buffer_map (buf, &map, GST_MAP_READ);
    g_print ("map.size = %lu \n", map.size);
    unsigned char *ptr = (unsigned char *) map.data;
    g_print ("first 8 bytes = %x %x %x %x %x %x %x %x \n", *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5), *(ptr+6), *(ptr +7));

    json mdata{json::parse(map.data)};
    string stringdata{mdata.dump()};
    cout << stringdata << endl;

    SourceVideoPackage svp;

    svp.clientId = mdata[0]["clientId"].get<float>();
    svp.headScale = mdata[0]["headScale"].get<float>();

    std::vector<float> vec;
    vec = mdata[0]["headPoseQuaternion"].get<std::vector<float>>();
    std::memcpy(svp.headPoseQuaternion, vec.data(), vec.size()*sizeof(float));

    vec = mdata[0]["headPoseTranslation"].get<std::vector<float>>();
    std::memcpy(svp.headPoseTranslation, vec.data(), vec.size()*sizeof(float));

    vec = mdata[0]["gazeAngle"].get<std::vector<float>>();
    std::memcpy(svp.gazeAngle, vec.data(), vec.size()*sizeof(float));

    vec = mdata[0]["rawTrackingPoseEuler"].get<std::vector<float>>();
    std::memcpy(svp.rawTrackingPoseEuler, vec.data(), vec.size()*sizeof(float));


    gsize size, maxsize, offset;
    size = gst_buffer_get_sizes (buf, &offset, &maxsize);
    g_print ("offset = %lu size = %lu maxsize=%lu\n", offset, size, maxsize);

    //metadata *md = (metadata *) map.data;
    //g_print ("a = %u, b = %u, c = %u, d = %u \n", md->a, md->b, md->c, md->d);

    return GST_PAD_PROBE_OK;
}

int main(int argc, char *argv[])
{
    GstElement *pipeline, *capsfilter, *parser, *decoder, *rtpgstdepay, *rtpstreamdepay, *tcpclientsrc, *sink;
    GstBus *bus;
    GstMessage *msg;
    GMainLoop *loop;

    /* Initialize GStreamer */
    gst_init (&argc, &argv);

    /* Create the elements */
    tcpclientsrc = gst_element_factory_make("tcpclientsrc", NULL);
    rtpgstdepay = gst_element_factory_make("rtpgstdepay", NULL);
    capsfilter = gst_element_factory_make("capsfilter", NULL);
    rtpstreamdepay = gst_element_factory_make("rtpstreamdepay", NULL);
    parser = gst_element_factory_make("h264parse", NULL);
    decoder = gst_element_factory_make("nvv4l2decoder", NULL);
    sink = gst_element_factory_make ("nveglglessink", NULL);


    GstCaps *caps = gst_caps_from_string("application/x-rtp-stream, media=(string)application, encoding-name=(string)X-GST");
    g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);
    gst_caps_unref(caps);

    /* Create the empty pipeline */
    pipeline = gst_pipeline_new("receiver-pipeline");

    if (!pipeline || !parser || !decoder || !rtpgstdepay || !rtpstreamdepay || !tcpclientsrc || !sink || !capsfilter) {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    g_object_set (tcpclientsrc, "host", "10.24.216.249", NULL);
    g_object_set (tcpclientsrc, "port", 8000, NULL);


    /* Build the pipeline */
    gst_bin_add_many(GST_BIN(pipeline), tcpclientsrc, capsfilter, rtpstreamdepay, rtpgstdepay, parser, decoder, sink , NULL);
    if (gst_element_link_many(tcpclientsrc, capsfilter, rtpstreamdepay, rtpgstdepay, parser, decoder, sink, NULL) != TRUE) {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    GstPad *rtpgstdepay_src_pad = gst_element_get_static_pad (rtpgstdepay, "src");
    gst_pad_add_probe (rtpgstdepay_src_pad, GST_PAD_PROBE_TYPE_BUFFER, rtpgstdepay_src_pad_probe, NULL, NULL);

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
