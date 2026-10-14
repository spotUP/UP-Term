/* Build shim (not part of wasabi): the CyberGraphX SDK headers are not in this
 * machine's Bebbo gcc install. wasabid uses the RECTFMT constants, the PIXFMT
 * codes and three LockBitMapTags tags (grab of a deep screen, 0.8); values as in the CyberGraphX SDK. */
#ifndef CYBERGRAPHX_CYBERGRAPHICS_H
#define CYBERGRAPHX_CYBERGRAPHICS_H
#define RECTFMT_RGB   0L
#define RECTFMT_RGBA  1L
#define RECTFMT_ARGB  2L
#define RECTFMT_LUT8  3L
#define RECTFMT_GREY8 4L
#define PIXFMT_LUT8    0L
#define PIXFMT_RGB15   1L
#define PIXFMT_BGR15   2L
#define PIXFMT_RGB15PC 3L
#define PIXFMT_BGR15PC 4L
#define PIXFMT_RGB16   5L
#define PIXFMT_BGR16   6L
#define PIXFMT_RGB16PC 7L
#define PIXFMT_BGR16PC 8L
#define PIXFMT_RGB24   9L
#define PIXFMT_BGR24   10L
#define PIXFMT_ARGB32  11L
#define PIXFMT_BGRA32  12L
#define PIXFMT_RGBA32  13L
#define LBMI_PIXFMT      0x84001004UL
#define LBMI_BYTESPERROW 0x84001006UL
#define LBMI_BASEADDRESS 0x84001007UL
#endif
