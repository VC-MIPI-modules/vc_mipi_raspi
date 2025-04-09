#ifndef VC_MIPI_CAMERA_HELPERS_H
#define VC_MIPI_CAMERA_HELPERS_H


#include "../vc_mipi_core/vc_mipi_core.h"
#include <linux/module.h>
#include <linux/gpio/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/version.h>
#include <linux/of_graph.h> 

#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-event.h>

// Declare debug as extern
extern int debug;

enum private_cids
{
        V4L2_CID_VC_TRIGGER_MODE = V4L2_CID_USER_BASE | 0xfff0, // TODO FOR NOW USE 0xfff0 offset
        V4L2_CID_VC_IO_MODE,
        V4L2_CID_VC_FRAME_RATE,
        V4L2_CID_VC_SINGLE_TRIGGER,
        V4L2_CID_VC_BINNING_MODE,
        V4L2_CID_VC_ROI_POSITION,
};

enum pad_types {
	IMAGE_PAD,
	METADATA_PAD,
	NUM_PADS
};
struct vc_control_int_menu {
        struct v4l2_ctrl *ctrl;
        const struct v4l2_ctrl_ops *ops;
};
struct vc_device
{
        struct v4l2_subdev sd;
        struct v4l2_ctrl_handler ctrl_handler;
        struct media_pad pad;
        int power_on;
        struct mutex mutex;

        struct v4l2_rect crop_rect;
        struct v4l2_mbus_framefmt format;

        struct v4l2_ctrl *ctrl_hblank;
        struct v4l2_ctrl *ctrl_vblank;  
        struct vc_cam cam;
};
static void vc_update_clk_rates(struct vc_device *device, struct vc_cam *cam);

static inline struct vc_device *to_vc_device(struct v4l2_subdev *sd)
{
        return container_of(sd, struct vc_device, sd);
}

static inline struct vc_cam *to_vc_cam(struct v4l2_subdev *sd)
{
        struct vc_device *device = to_vc_device(sd);
        return &device->cam;
}

int imx335_update_test_pattern(struct vc_cam *cam, u32 pattern_index);
int imx335_init_control(struct vc_device *device, struct v4l2_ctrl_handler *hdl, struct v4l2_ctrl_ops *ctrl_ops);

int vc_ctrl_init_ctrl_std_menu(struct vc_device *device, struct v4l2_ctrl_handler *hdl, struct v4l2_ctrl_ops *ctrl_ops, int id, const char * const items[], size_t items_count);

#endif // VC_MIPI_CAMERA_HELPERS_H