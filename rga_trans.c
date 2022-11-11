#include "include/comm.h"

static long int crv_tab[256];
static long int cbu_tab[256];
static long int cgu_tab[256];
static long int cgv_tab[256];
static long int tab_76309[256];
static unsigned char clp[1024]; // for clip in CCIR601

void init_yuv420p_table() {
    long int crv, cbu, cgu, cgv;
    int i, ind;
    static int init = 0;

    if (init == 1)
        return;

    crv = 104597;
    cbu = 132201; /* fra matrise i global.h */
    cgu = 25675;
    cgv = 53279;

    for (i = 0; i < 256; i++) {
        crv_tab[i] = (i - 128) * crv;
        cbu_tab[i] = (i - 128) * cbu;
        cgu_tab[i] = (i - 128) * cgu;
        cgv_tab[i] = (i - 128) * cgv;
        tab_76309[i] = 76309 * (i - 16);
    }

    for (i = 0; i < 384; i++)
        clp[i] = 0;
    ind = 384;
    for (i = 0; i < 256; i++)
        clp[ind++] = i;
    ind = 640;
    for (i = 0; i < 384; i++)
        clp[ind++] = 255;

    init = 1;
}

void nv12_to_rgb24(unsigned char* yuvbuffer, unsigned char* rga_buffer,
    int width, int height) {
    int y1, y2, u, v;
    unsigned char* py1, * py2;
    int i, j, c1, c2, c3, c4;
    unsigned char* d1, * d2;
    unsigned char* src_u;

    src_u = yuvbuffer + width * height; // u

    py1 = yuvbuffer; // y
    py2 = py1 + width;
    d1 = rga_buffer;
    d2 = d1 + 3 * width;

    init_yuv420p_table();

    for (j = 0; j < height; j += 2) {
        for (i = 0; i < width; i += 2) {
            u = *src_u++;
            v = *src_u++; // v immediately follows u, in the next position of u

            c4 = crv_tab[v];
            c2 = cgu_tab[u];
            c3 = cgv_tab[v];
            c1 = cbu_tab[u];

            // up-left
            y1 = tab_76309[*py1++];
            *d1++ = clp[384 + ((y1 + c1) >> 16)];
            *d1++ = clp[384 + ((y1 - c2 - c3) >> 16)];
            *d1++ = clp[384 + ((y1 + c4) >> 16)];

            // down-left
            y2 = tab_76309[*py2++];
            *d2++ = clp[384 + ((y2 + c1) >> 16)];
            *d2++ = clp[384 + ((y2 - c2 - c3) >> 16)];
            *d2++ = clp[384 + ((y2 + c4) >> 16)];

            // up-right
            y1 = tab_76309[*py1++];
            *d1++ = clp[384 + ((y1 + c1) >> 16)];
            *d1++ = clp[384 + ((y1 - c2 - c3) >> 16)];
            *d1++ = clp[384 + ((y1 + c4) >> 16)];

            // down-right
            y2 = tab_76309[*py2++];
            *d2++ = clp[384 + ((y2 + c1) >> 16)];
            *d2++ = clp[384 + ((y2 - c2 - c3) >> 16)];
            *d2++ = clp[384 + ((y2 + c4) >> 16)];
        }
        d1 += 3 * width;
        d2 += 3 * width;
        py1 += width;
        py2 += width;
    }

    // save bmp
    // int filesize = 54 + 3 * width * height;
    // FILE *f;
    // unsigned char bmpfileheader[14] = {'B', 'M', 0, 0,  0, 0, 0,
    //                                    0,   0,   0, 54, 0, 0, 0};
    // unsigned char bmpinfoheader[40] = {40, 0, 0, 0, 0, 0, 0,  0,
    //                                    0,  0, 0, 0, 1, 0, 24, 0};
    // unsigned char bmppad[3] = {0, 0, 0};

    // bmpfileheader[2] = (unsigned char)(filesize);
    // bmpfileheader[3] = (unsigned char)(filesize >> 8);
    // bmpfileheader[4] = (unsigned char)(filesize >> 16);
    // bmpfileheader[5] = (unsigned char)(filesize >> 24);

    // bmpinfoheader[4] = (unsigned char)(width);
    // bmpinfoheader[5] = (unsigned char)(width >> 8);
    // bmpinfoheader[6] = (unsigned char)(width >> 16);
    // bmpinfoheader[7] = (unsigned char)(width >> 24);
    // bmpinfoheader[8] = (unsigned char)(height);
    // bmpinfoheader[9] = (unsigned char)(height >> 8);
    // bmpinfoheader[10] = (unsigned char)(height >> 16);
    // bmpinfoheader[11] = (unsigned char)(height >> 24);

    // f = fopen("/tmp/tmp.bmp", "wb");
    // fwrite(bmpfileheader, 1, 14, f);
    // fwrite(bmpinfoheader, 1, 40, f);
    // for (int k = 0; k < height; k++) {
    //   fwrite(rga_buffer + (width * (height - k - 1) * 3), 3, width, f);
    //   fwrite(bmppad, 1, (4 - (width * 3) % 4) % 4, f);
    // }
    // fclose(f);
}

int nv12_border(char* pic, int pic_w, int pic_h, int rect_x, int rect_y,
    int rect_w, int rect_h, int R, int G, int B) {
    /* Set up the rectangle border size */
    const int border = 5;

    /* RGB convert YUV */
    int Y, U, V;
    Y = 0.299 * R + 0.587 * G + 0.114 * B;
    U = -0.1687 * R + 0.3313 * G + 0.5 * B + 128;
    V = 0.5 * R - 0.4187 * G - 0.0813 * B + 128;
    /* Locking the scope of rectangle border range */
    int j, k;
    for (j = rect_y; j < rect_y + rect_h; j++) {
        for (k = rect_x; k < rect_x + rect_w; k++) {
            if (k < (rect_x + border) || k >(rect_x + rect_w - border) ||
                j < (rect_y + border) || j >(rect_y + rect_h - border)) {
                /* Components of YUV's storage address index */
                int y_index = j * pic_w + k;
                int u_index =
                    (y_index / 2 - pic_w / 2 * ((j + 1) / 2)) * 2 + pic_w * pic_h;
                int v_index = u_index + 1;
                /* set up YUV's conponents value of rectangle border */
                pic[y_index] = Y;
                pic[u_index] = U;
                pic[v_index] = V;
            }
        }
    }

    return 0;
}

int rgb24_resize(unsigned char* input_rgb, unsigned char* output_rgb, int width,
    int height, int outwidth, int outheight) {
    rga_buffer_t src =
        wrapbuffer_virtualaddr(input_rgb, width, height, RK_FORMAT_RGB_888);
    rga_buffer_t dst = wrapbuffer_virtualaddr(output_rgb, outwidth, outheight,
        RK_FORMAT_RGB_888);
    rga_buffer_t pat = { 0 };
    im_rect src_rect = { 0, 0, width, height };
    im_rect dst_rect = { 0, 0, outwidth, outheight };
    im_rect pat_rect = { 0 };
    IM_STATUS STATUS = improcess(src, dst, pat, src_rect, dst_rect, pat_rect, 0);
    if (STATUS != IM_STATUS_SUCCESS) {
        printf("imcrop failed: %s\n", imStrError(STATUS));
        return -1;
    }
    return 0;
}

void trans_data_for_yolo_input(unsigned char* rga_buffer_model_input,
    struct demo_cfg cfg,
    MEDIA_BUFFER buffer) {

    // nv12 to rgb24 and resize
    int rga_buffer_size = cfg.session_cfg[RK_NN_RGA_CHN_INDEX].u32Width *
        cfg.session_cfg[RK_NN_RGA_CHN_INDEX].u32Height * 3; // nv12 3/2, rgb 3

    unsigned char* rga_buffer = (unsigned char*)malloc(rga_buffer_size);

    nv12_to_rgb24((unsigned char*)RK_MPI_MB_GetPtr(buffer), rga_buffer,
        cfg.session_cfg[RK_NN_RGA_CHN_INDEX].u32Width,
        cfg.session_cfg[RK_NN_RGA_CHN_INDEX].u32Height);

    rgb24_resize(rga_buffer, rga_buffer_model_input,
        cfg.session_cfg[RK_NN_RGA_CHN_INDEX].u32Width,
        cfg.session_cfg[RK_NN_RGA_CHN_INDEX].u32Height, MODEL_INPUT_SIZE,
        MODEL_INPUT_SIZE);
}
