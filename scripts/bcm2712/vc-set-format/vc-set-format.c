/*
 * vc-set-format: V4L2 video format setter with validation
 * 
 * This utility sets video format parameters using V4L2 ioctl calls
 * with proper format validation and fallback handling for older kernels.
 * 
 * Usage: vc-set-format <device> <width> <height> <fourcc> [colorspace]
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <syslog.h>

#define DEFAULT_COLORSPACE V4L2_COLORSPACE_SRGB

typedef struct {
    const char *device;
    uint32_t width;
    uint32_t height;
    uint32_t pixelformat;
    enum v4l2_colorspace colorspace;
    int verbose;
} format_config_t;

/* Convert FourCC string to uint32_t */
static uint32_t fourcc_to_uint32(const char *fourcc) {
    if (strlen(fourcc) != 4) {
        return 0;
    }
    return v4l2_fourcc(fourcc[0], fourcc[1], fourcc[2], fourcc[3]);
}

/* Convert uint32_t to FourCC string */
static void uint32_to_fourcc(uint32_t fourcc, char *buf) {
    buf[0] = (fourcc >> 0) & 0xFF;
    buf[1] = (fourcc >> 8) & 0xFF;
    buf[2] = (fourcc >> 16) & 0xFF;
    buf[3] = (fourcc >> 24) & 0xFF;
    buf[4] = '\0';
}

/* Parse colorspace string */
static enum v4l2_colorspace parse_colorspace(const char *str) {
    if (strcasecmp(str, "srgb") == 0)
        return V4L2_COLORSPACE_SRGB;
    if (strcasecmp(str, "rec709") == 0)
        return V4L2_COLORSPACE_REC709;
    if (strcasecmp(str, "bt878") == 0)
        return V4L2_COLORSPACE_BT878;
    if (strcasecmp(str, "470_system_m") == 0)
        return V4L2_COLORSPACE_470_SYSTEM_M;
    if (strcasecmp(str, "470_system_bg") == 0)
        return V4L2_COLORSPACE_470_SYSTEM_BG;
    if (strcasecmp(str, "jpeg") == 0)
        return V4L2_COLORSPACE_JPEG;
    if (strcasecmp(str, "smpte170m") == 0)
        return V4L2_COLORSPACE_SMPTE170M;
    if (strcasecmp(str, "smpte240m") == 0)
        return V4L2_COLORSPACE_SMPTE240M;
    if (strcasecmp(str, "raw") == 0)
        return V4L2_COLORSPACE_RAW;
    
    return V4L2_COLORSPACE_SRGB; /* Default */
}

/* Check if format is supported */
static int is_format_supported(int fd, uint32_t pixelformat) {
    struct v4l2_fmtdesc fmtdesc;
    int found = 0;

    memset(&fmtdesc, 0, sizeof(fmtdesc));
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        if (fmtdesc.pixelformat == pixelformat) {
            found = 1;
            break;
        }
        fmtdesc.index++;
    }

    return found;
}

/* List supported formats */
static void list_supported_formats(int fd) {
    struct v4l2_fmtdesc fmtdesc;
    char fourcc[5];

    memset(&fmtdesc, 0, sizeof(fmtdesc));
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    fprintf(stderr, "Supported formats:\n");
    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        uint32_to_fourcc(fmtdesc.pixelformat, fourcc);
        fprintf(stderr, "  %s - %s\n", fourcc, fmtdesc.description);
        fmtdesc.index++;
    }
}

/* Set video format */
static int set_video_format(int fd, format_config_t *config, int use_colorspace) {
    struct v4l2_format fmt;

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = config->width;
    fmt.fmt.pix.height = config->height;
    fmt.fmt.pix.pixelformat = config->pixelformat;
    
    if (use_colorspace) {
        fmt.fmt.pix.colorspace = config->colorspace;
    }

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        return -1;
    }

    return 0;
}

/* Verify format was set correctly */
static int verify_format(int fd, format_config_t *config) {
    struct v4l2_format fmt;
    char expected_fourcc[5], actual_fourcc[5];

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(fd, VIDIOC_G_FMT, &fmt) < 0) {
        perror("VIDIOC_G_FMT");
        return -1;
    }

    if (fmt.fmt.pix.pixelformat != config->pixelformat) {
        uint32_to_fourcc(config->pixelformat, expected_fourcc);
        uint32_to_fourcc(fmt.fmt.pix.pixelformat, actual_fourcc);
        fprintf(stderr, "Format mismatch: expected '%s', got '%s'\n",
                expected_fourcc, actual_fourcc);
        return -1;
    }

    if (config->verbose) {
        uint32_to_fourcc(fmt.fmt.pix.pixelformat, actual_fourcc);
        printf("Format verified: %s at %ux%u\n",
               actual_fourcc, fmt.fmt.pix.width, fmt.fmt.pix.height);
    }

    return 0;
}

/* Print usage information */
static void print_usage(const char *progname) {
    fprintf(stderr, "Usage: %s [options] <device> <width> <height> <fourcc> [colorspace]\n\n", progname);
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr, "  device      Video device path (e.g., /dev/video0)\n");
    fprintf(stderr, "  width       Video width in pixels\n");
    fprintf(stderr, "  height      Video height in pixels\n");
    fprintf(stderr, "  fourcc      FourCC pixel format code (e.g., pRAA, RG16, Y10P)\n");
    fprintf(stderr, "  colorspace  Optional colorspace (default: srgb)\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -v, --verbose   Enable verbose output\n");
    fprintf(stderr, "  -l, --list      List supported formats\n");
    fprintf(stderr, "  -h, --help      Show this help message\n\n");
    fprintf(stderr, "Exit codes:\n");
    fprintf(stderr, "  0   Success\n");
    fprintf(stderr, "  1   Format not supported by device\n");
    fprintf(stderr, "  2   Failed to set format\n");
    fprintf(stderr, "  3   Invalid arguments\n");
    fprintf(stderr, "  4   Device not found or cannot open\n\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s /dev/video0 1920 1080 pRAA\n", progname);
    fprintf(stderr, "  %s -v /dev/video0 1920 1080 RG16 srgb\n", progname);
}

int main(int argc, char *argv[]) {
    format_config_t config = {
        .device = NULL,
        .width = 0,
        .height = 0,
        .pixelformat = 0,
        .colorspace = DEFAULT_COLORSPACE,
        .verbose = 0
    };
    int fd;
    int list_formats = 0;
    int arg_offset = 1;
    char fourcc_str[5];

    openlog("vc-set-format", LOG_PID, LOG_USER);

    /* Parse options */
    while (arg_offset < argc && argv[arg_offset][0] == '-') {
        if (strcmp(argv[arg_offset], "-v") == 0 || 
            strcmp(argv[arg_offset], "--verbose") == 0) {
            config.verbose = 1;
            arg_offset++;
        } else if (strcmp(argv[arg_offset], "-l") == 0 || 
                   strcmp(argv[arg_offset], "--list") == 0) {
            list_formats = 1;
            arg_offset++;
        } else if (strcmp(argv[arg_offset], "-h") == 0 || 
                   strcmp(argv[arg_offset], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[arg_offset]);
            print_usage(argv[0]);
            return 3;
        }
    }

    /* Check required arguments */
    if (list_formats) {
        if (argc - arg_offset < 1) {
            fprintf(stderr, "Error: Device path required\n");
            print_usage(argv[0]);
            return 3;
        }
        config.device = argv[arg_offset];
    } else {
        if (argc - arg_offset < 4) {
            fprintf(stderr, "Error: Missing required arguments\n");
            print_usage(argv[0]);
            return 3;
        }

        config.device = argv[arg_offset];
        config.width = atoi(argv[arg_offset + 1]);
        config.height = atoi(argv[arg_offset + 2]);
        config.pixelformat = fourcc_to_uint32(argv[arg_offset + 3]);

        if (argc - arg_offset >= 5) {
            config.colorspace = parse_colorspace(argv[arg_offset + 4]);
        }

        if (config.pixelformat == 0) {
            fprintf(stderr, "Error: Invalid FourCC format: '%s'\n", argv[arg_offset + 3]);
            return 3;
        }

        if (config.width == 0 || config.height == 0) {
            fprintf(stderr, "Error: Invalid resolution: %ux%u\n", config.width, config.height);
            return 3;
        }
    }

    /* Open device */
    fd = open(config.device, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Error: Cannot open device '%s': %s\n", 
                config.device, strerror(errno));
        syslog(LOG_ERR, "Cannot open device '%s': %s", config.device, strerror(errno));
        return 4;
    }

    /* Check if it's a V4L2 device */
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        fprintf(stderr, "Error: '%s' is not a V4L2 device\n", config.device);
        close(fd);
        return 4;
    }

    if (config.verbose) {
        printf("Device: %s\n", cap.card);
        printf("Driver: %s\n", cap.driver);
    }

    /* List formats if requested */
    if (list_formats) {
        list_supported_formats(fd);
        close(fd);
        return 0;
    }

    uint32_to_fourcc(config.pixelformat, fourcc_str);

    if (config.verbose) {
        printf("Setting format: %s at %ux%u\n", fourcc_str, config.width, config.height);
    }

    /* Check if format is supported */
    if (!is_format_supported(fd, config.pixelformat)) {
        fprintf(stderr, "Error: Format '%s' is not supported by %s\n",
                fourcc_str, config.device);
        syslog(LOG_ERR, "Format '%s' not supported by %s", fourcc_str, config.device);
        
        fprintf(stderr, "\n");
        list_supported_formats(fd);
        close(fd);
        return 1;
    }

    /* Try to set format with colorspace */
    if (set_video_format(fd, &config, 1) < 0) {
        if (config.verbose) {
            fprintf(stderr, "Failed with colorspace, trying without...\n");
        }
        
        /* Try without colorspace (for older kernels) */
        if (set_video_format(fd, &config, 0) < 0) {
            fprintf(stderr, "Error: Failed to set format '%s': %s\n",
                    fourcc_str, strerror(errno));
            syslog(LOG_ERR, "Failed to set format '%s': %s", fourcc_str, strerror(errno));
            close(fd);
            return 2;
        }
    }

    /* Verify the format was set correctly */
    if (verify_format(fd, &config) < 0) {
        fprintf(stderr, "Error: Format verification failed\n");
        syslog(LOG_ERR, "Format verification failed");
        close(fd);
        return 2;
    }

    printf("Successfully set format: %s at %ux%u\n", 
           fourcc_str, config.width, config.height);
    syslog(LOG_INFO, "Successfully set format: %s at %ux%u", 
           fourcc_str, config.width, config.height);

    close(fd);
    closelog();
    return 0;
}
