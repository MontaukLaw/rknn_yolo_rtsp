#include "comm.h"

int bind_rga_for_vi(struct Session session) {

    MPP_CHN_S stViChn;
    MPP_CHN_S stRgaChn;
    MPP_CHN_S stVencChn;
    RGA_ATTR_S stRgaAttr;
    int ret = 0;

    memset(&stViChn, 0, sizeof(MPP_CHN_S));
    memset(&stRgaChn, 0, sizeof(MPP_CHN_S));
    memset(&stVencChn, 0, sizeof(MPP_CHN_S));
    memset(&stRgaAttr, 0, sizeof(RGA_ATTR_S));

    printf("session: width=%d, height=%d,\n", session.u32Width, session.u32Height);

    stRgaAttr.bEnBufPool = RK_TRUE;
    stRgaAttr.u16BufPoolCnt = 4;
    stRgaAttr.u16Rotaion = 0;

    stRgaAttr.stImgIn.u32X = (session.u32Width - MODEL_INPUT_SIZE) / 2;
    stRgaAttr.stImgIn.u32Y = (session.u32Height - MODEL_INPUT_SIZE) / 2;
    stRgaAttr.stImgIn.imgType = IMAGE_TYPE_NV12;
    stRgaAttr.stImgIn.u32Width = MODEL_INPUT_SIZE;
    stRgaAttr.stImgIn.u32Height = MODEL_INPUT_SIZE;
    stRgaAttr.stImgIn.u32HorStride = session.u32Width;
    stRgaAttr.stImgIn.u32VirStride = session.u32Height;

    stRgaAttr.stImgOut.u32X = 0;
    stRgaAttr.stImgOut.u32Y = 0;
    stRgaAttr.stImgOut.imgType = IMAGE_TYPE_NV12;

    stRgaAttr.stImgOut.u32Width = MODEL_INPUT_SIZE;
    stRgaAttr.stImgOut.u32Height = MODEL_INPUT_SIZE;
    stRgaAttr.stImgOut.u32HorStride = MODEL_INPUT_SIZE;
    stRgaAttr.stImgOut.u32VirStride = MODEL_INPUT_SIZE;

    ret = RK_MPI_RGA_CreateChn(session.stRgaChn.s32ChnId, &stRgaAttr);
    if (ret) {
        printf("ERROR: Create rga[%d] falied! ret=%d\n", session.stRgaChn.s32ChnId, ret);
        return -1;
    } else {
        printf("Create rga[%d] success!\n", session.stRgaChn.s32ChnId);
    }

    stViChn.enModId = RK_ID_VI;
    stViChn.s32DevId = session.stViChn.s32DevId;
    stViChn.s32ChnId = session.stViChn.s32ChnId;

    // bind vi to rga to make resize
    stRgaChn.enModId = RK_ID_RGA;
    stRgaChn.s32ChnId = session.stRgaChn.s32ChnId;

    // printf("stSrcChn.s32DevId: %d stSrcChn.s32ChnId: %d\n", stSrcChn.s32DevId, stSrcChn.s32ChnId);
    // printf("stDestChn.s32ChnId: %d\n", stDestChn.s32ChnId);

    ret = RK_MPI_SYS_Bind(&stViChn, &stRgaChn);
    if (ret) {
        printf("ERROR: Bind vi and rga failed! ret=%d\n", ret);
        return -1;
    }

    stVencChn.enModId = RK_ID_VENC;
    stVencChn.s32ChnId = session.stVenChn.s32ChnId;

    ret = RK_MPI_SYS_Bind(&stRgaChn, &stVencChn);
    if (ret) {
        printf("ERROR: Bind vga and venc failed! ret=%d\n", ret);
        return -1;
    }
    return 0;

}

void common_vi_setup(struct Session *session, VI_CHN_WORK_MODE mode, RK_S32 vi_pipe) {
    VI_CHN_ATTR_S vi_chn_attr;

    printf("vi session->videopath: %s\n", session->videopath);
    printf("vi session->u32Width: %d\n", session->u32Width);
    printf("vi session->u32Height: %d\n", session->u32Height);

    vi_chn_attr.u32BufCnt = 3;
    vi_chn_attr.u32Width = session->u32Width;
    vi_chn_attr.u32Height = session->u32Height;
    vi_chn_attr.enWorkMode = mode;
    vi_chn_attr.pcVideoNode = session->videopath;
    vi_chn_attr.enPixFmt = session->enImageType;

    RK_MPI_VI_SetChnAttr(vi_pipe, session->stViChn.s32ChnId, &vi_chn_attr);
    RK_MPI_VI_EnableChn(vi_pipe, session->stViChn.s32ChnId);
}

void common_venc_setup(struct Session *session, bool ifSubStream) {
    VENC_CHN_ATTR_S venc_chn_attr;
    memset(&venc_chn_attr, 0, sizeof(venc_chn_attr));

    venc_chn_attr.stVencAttr.enType = session->video_type;
    venc_chn_attr.stVencAttr.imageType = session->enImageType;

    printf("venc session->u32Width: %d\n", session->u32Width);
    printf("venc session->u32Height: %d\n", session->u32Height);
    printf("isSubStream: %d\n", ifSubStream);

    if (ifSubStream) {
        venc_chn_attr.stVencAttr.u32PicWidth = session->u32Width; // MODEL_INPUT_SIZE;
        venc_chn_attr.stVencAttr.u32PicHeight = session->u32Height; // MODEL_INPUT_SIZE;
        venc_chn_attr.stVencAttr.u32VirWidth = session->u32Width; // MODEL_INPUT_SIZE;
        venc_chn_attr.stVencAttr.u32VirHeight = session->u32Height;
        venc_chn_attr.stVencAttr.enRotation = VENC_ROTATION_0;
    } else {
        venc_chn_attr.stVencAttr.u32PicWidth = MODEL_INPUT_SIZE; //session->u32Width; //MODEL_INPUT_SIZE;
        venc_chn_attr.stVencAttr.u32PicHeight = MODEL_INPUT_SIZE; //session->u32Height; //MODEL_INPUT_SIZE;
        venc_chn_attr.stVencAttr.u32VirWidth = MODEL_INPUT_SIZE; //session->u32Width; //MODEL_INPUT_SIZE;
        venc_chn_attr.stVencAttr.u32VirHeight = MODEL_INPUT_SIZE; // session->u32Height;
        venc_chn_attr.stVencAttr.u32Profile = 77;  // MP
    }

    venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
    venc_chn_attr.stRcAttr.stH264Cbr.u32Gop = FPS;
    venc_chn_attr.stRcAttr.stH264Cbr.u32BitRate = session->u32Width * session->u32Height * FPS / 14;

    venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = FPS;

    venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = FPS;
    RK_MPI_VENC_CreateChn(session->stVenChn.s32ChnId, &venc_chn_attr);
}

long get_current_time_ms(void) {
    long msec = 0;
    char str[20] = {0};
    struct timeval stuCurrentTime;

    gettimeofday(&stuCurrentTime, NULL);
    sprintf(str, "%ld%03ld", stuCurrentTime.tv_sec,
            (stuCurrentTime.tv_usec) / 1000);
    for (size_t i = 0; i < strlen(str); i++) {
        msec = msec * 10 + (str[i] - '0');
    }

    return msec;
}

