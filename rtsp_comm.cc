#include "./include/rtsp_comm.h"

rtsp_demo_handle g_rtsplive = NULL;
rtsp_session_handle g_rtsp_session;

extern int g_flag_run;

void init_rtsp(void) {
    g_rtsplive = create_rtsp_demo(554);
    g_rtsp_session = rtsp_new_session(g_rtsplive, "/live/main_stream");
    rtsp_set_video(g_rtsp_session, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
    rtsp_sync_video_ts(g_rtsp_session, rtsp_get_reltime(), rtsp_get_ntptime());
}

void video_packet_cb(MEDIA_BUFFER mb) {
    static RK_S32 packet_cnt = 0;
    if (g_flag_run == 0)
        return;

    printf("#Get packet-%d, size %zu\n", packet_cnt, RK_MPI_MB_GetSize(mb));

    if (g_rtsplive && g_rtsp_session) {
        rtsp_tx_video(g_rtsp_session, RK_MPI_MB_GetPtr(mb), RK_MPI_MB_GetSize(mb),
                      RK_MPI_MB_GetTimestamp(mb));
        rtsp_do_event(g_rtsplive);
    }

    RK_MPI_MB_ReleaseBuffer(mb);
    packet_cnt++;
}

void clean_rtsp(void) {
    if (g_rtsplive) {
        rtsp_del_demo(g_rtsplive);
    }
}