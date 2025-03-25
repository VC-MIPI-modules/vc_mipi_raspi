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

#define VERSION "0.5.3"

// --- Prototypes --------------------------------------------------------------
static int vc_sd_s_power(struct v4l2_subdev *sd, int on);
static int vc_sd_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *control);
static int vc_suspend(struct device *dev);
static int vc_resume(struct device *dev);
int vc_sd_enum_mbus_code(struct v4l2_subdev *sd, struct v4l2_subdev_state *state, struct v4l2_subdev_mbus_code_enum *code);
int vc_sd_enum_frame_size(struct v4l2_subdev *sd, struct v4l2_subdev_state *state, struct v4l2_subdev_frame_size_enum *fse);
static int vc_sd_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_state *state, struct v4l2_subdev_format *fmt);
static int vc_sd_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_state *state, struct v4l2_subdev_format *fmt);
static int vc_sd_get_selection(struct v4l2_subdev *sd, struct v4l2_subdev_state *state, struct v4l2_subdev_selection *sel);
static int vc_sd_set_selection(struct v4l2_subdev *sd, struct v4l2_subdev_state *state, struct v4l2_subdev_selection *sel);
int vc_sd_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh);
void vc_sd_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh);
int vc_sd_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc);
int vc_sd_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *control);
int vc_sd_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *control);
int vc_sd_querymenu(struct v4l2_subdev *sd, struct v4l2_querymenu *qm);
int vc_sd_g_mbus_config(struct v4l2_subdev *sd, struct v4l2_mbus_config *cfg);
int vc_sd_s_mbus_config(struct v4l2_subdev *sd, struct v4l2_mbus_config *cfg);
int vc_sd_g_frame_interval(struct v4l2_subdev *sd, struct v4l2_subdev_frame_interval *fi);
int vc_sd_s_frame_interval(struct v4l2_subdev *sd, struct v4l2_subdev_frame_interval *fi);
int vc_ctrl_s_ctrl(struct v4l2_ctrl *ctrl);

// --- Structures --------------------------------------------------------------


enum pad_types {
	IMAGE_PAD,
	METADATA_PAD,
	NUM_PADS
};

struct vc_device
{
        struct v4l2_subdev sd;
        struct v4l2_ctrl_handler ctrl_handler;
        struct media_pad pad;
        // struct gpio_desc *power_gpio;
        int power_on;
        struct mutex mutex;

        struct v4l2_rect crop_rect;
        struct v4l2_mbus_framefmt format;

        struct vc_cam cam;
};

static inline struct vc_device *to_vc_device(struct v4l2_subdev *sd)
{
        return container_of(sd, struct vc_device, sd);
}

static inline struct vc_cam *to_vc_cam(struct v4l2_subdev *sd)
{
        struct vc_device *device = to_vc_device(sd);
        return &device->cam;
}

static struct vc_control64 linkfreq  = {
    .min = 0,
    .max = 0,
    .def = 0,
};
static        struct vc_control hblank; //TODO Check implementation
static        struct vc_control vblank; //TODO Check implementation
static        struct vc_control pixel_rate; //TODO Check implementation
static void update_frame_rate_ctrl(struct vc_cam *cam, struct vc_device *device);

// --- v4l2_subdev_core_ops ---------------------------------------------------

static void vc_set_power(struct vc_device *device, int on)
{
        struct device *dev = &device->cam.ctrl.client_sen->dev;

        if (device->power_on == on)
                return;

        vc_dbg(dev, "%s(): Set power: %s\n", __func__, on ? "on" : "off");

        // if (device->power_gpio)
        // 	gpiod_set_value_cansleep(device->power_gpio, on);

        // if (on == 1) {
        //         vc_core_wait_until_device_is_ready(&device->cam, 1000);
        // }
        device->power_on = on;
}

static int vc_sd_s_power(struct v4l2_subdev *sd, int on)
{
        struct vc_device *device = to_vc_device(sd);

        mutex_lock(&device->mutex);

        vc_set_power(to_vc_device(sd), on);

        mutex_unlock(&device->mutex);

        return 0;
}

static int __maybe_unused vc_suspend(struct device *dev)
{
        struct i2c_client *client = to_i2c_client(dev);
        struct v4l2_subdev *sd = i2c_get_clientdata(client);
        struct vc_device *device = to_vc_device(sd);
        struct vc_state *state = &device->cam.state;

        vc_dbg(dev, "%s()\n", __func__);

        mutex_lock(&device->mutex);

        if (state->streaming)
                vc_sen_stop_stream(&device->cam);

        vc_set_power(device, 0);

        mutex_unlock(&device->mutex);

        return 0;
}

static int __maybe_unused vc_resume(struct device *dev)
{
        struct i2c_client *client = to_i2c_client(dev);
        struct v4l2_subdev *sd = i2c_get_clientdata(client);
        struct vc_device *device = to_vc_device(sd);

        vc_dbg(dev, "%s()\n", __func__);

        mutex_lock(&device->mutex);

        vc_set_power(device, 1);

        mutex_unlock(&device->mutex);

        return 0;
}

// MS TODO move in header
enum private_cids
{
        V4L2_CID_VC_TRIGGER_MODE = V4L2_CID_USER_BASE | 0xfff0, // TODO FOR NOW USE 0xfff0 offset
        V4L2_CID_VC_IO_MODE,
        V4L2_CID_VC_FRAME_RATE,
        V4L2_CID_VC_SINGLE_TRIGGER,
        V4L2_CID_VC_BINNING_MODE,
        V4L2_CID_VC_ROI_POSITION,
};

static int vc_sd_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *control)
{
        struct vc_cam *cam = to_vc_cam(sd);
        struct device *dev = vc_core_get_sen_device(cam);
        // __u32 left, top;

        switch (control->id)
        {

        // MS add some CIDs libcamera needs
        case V4L2_CID_HBLANK:
        case V4L2_CID_VBLANK:
        case V4L2_CID_HFLIP:
        case V4L2_CID_VFLIP:
                return 0; // TODO

        case V4L2_CID_EXPOSURE:
                return vc_sen_set_exposure(cam, control->value);

        case V4L2_CID_ANALOGUE_GAIN: // MS
        case V4L2_CID_GAIN:
                return vc_sen_set_gain(cam, control->value, false);

        case V4L2_CID_BLACK_LEVEL:
                return vc_sen_set_blacklevel(cam, control->value);
#if 1
        case V4L2_CID_VC_TRIGGER_MODE:
                return vc_mod_set_trigger_mode(cam, control->value);

        case V4L2_CID_VC_IO_MODE:
                return vc_mod_set_io_mode(cam, control->value);

        case V4L2_CID_VC_FRAME_RATE:
                return vc_core_set_framerate(cam, control->value);

        case V4L2_CID_VC_SINGLE_TRIGGER:
                return vc_mod_set_single_trigger(cam);

        case V4L2_CID_VC_BINNING_MODE:
                return vc_core_set_binning_mode(cam, control->value);

        case V4L2_CID_VC_ROI_POSITION:
                return vc_core_live_roi(cam, control->value);

#endif
        default:
                vc_warn(dev, "%s(): Unkown control 0x%08x\n", __func__, control->id);
                return -EINVAL;
        }

        return 0;
}

// --- v4l2_subdev_video_ops ---------------------------------------------------

static int vc_sd_s_stream(struct v4l2_subdev *sd, int enable)
{
        struct vc_device *device = to_vc_device(sd);
        struct vc_cam *cam = to_vc_cam(sd);
        struct vc_state *state = &cam->state;
        struct device *dev = sd->dev;
        int reset = 0;
        int ret = 0;

        vc_dbg(dev, "%s(): Set streaming: %s\n", __func__, enable ? "on" : "off");

        if (state->streaming == enable)
                return 0;

        mutex_lock(&device->mutex);
        if (enable)
        {
                ret = pm_runtime_get_sync(dev);
                if (ret < 0)
                {
                        pm_runtime_put_noidle(dev);
                        mutex_unlock(&device->mutex);
                        return ret;
                }

                ret = vc_mod_set_mode(cam, &reset);
                ret |= vc_sen_set_roi(cam);
                ret |= vc_sen_set_exposure(cam, cam->state.exposure);
                update_frame_rate_ctrl(cam,device);
                if (!ret && reset)
                {
                        ret |= vc_sen_set_gain(cam, cam->state.gain, false);
                        ret |= vc_sen_set_blacklevel(cam, cam->state.blacklevel);
                }

                ret = vc_sen_start_stream(cam);
                if (ret)
                {
                        enable = 0;
                        vc_sen_stop_stream(cam);
                        pm_runtime_mark_last_busy(dev);
                        pm_runtime_put_autosuspend(dev);
                }
        }
        else
        {
                vc_sen_stop_stream(cam);
                pm_runtime_mark_last_busy(dev);
                pm_runtime_put_autosuspend(dev);
        }

        state->streaming = enable;
        mutex_unlock(&device->mutex);

        return ret;
}

// --- v4l2_subdev_pad_ops ---------------------------------------------------

static int vc_sd_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_state *state, struct v4l2_subdev_format *format)
{
        struct vc_device *device = to_vc_device(sd);
        struct vc_cam *cam = to_vc_cam(sd);
        struct v4l2_mbus_framefmt *mf = &format->format;
        struct vc_frame *frame = NULL;

        mutex_lock(&device->mutex);

        mf->code = vc_core_get_format(cam);
        frame = vc_core_get_frame(cam);
        mf->width = frame->width;
        mf->height = frame->height;
        mf->field = V4L2_FIELD_NONE;
        mf->colorspace = V4L2_COLORSPACE_SRGB;

        mutex_unlock(&device->mutex);

        return 0;
}

static int vc_sd_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_state *state, struct v4l2_subdev_format *format)
{
        struct vc_device *device = to_vc_device(sd);
        struct vc_cam *cam = to_vc_cam(sd);
        struct v4l2_mbus_framefmt *mf = &format->format;

        mutex_lock(&device->mutex);

        vc_core_set_format(cam, mf->code);
        // TODO vc_core_set_frame(cam, mf->top, mf->left, mf->width, mf->height);
        vc_core_set_frame(cam, 0, 0, mf->width, mf->height);
        mf->field = V4L2_FIELD_NONE;
        mf->colorspace = V4L2_COLORSPACE_SRGB;

        mutex_unlock(&device->mutex);

        return 0;
}

int vc_sd_enum_mbus_code(struct v4l2_subdev *sd, struct v4l2_subdev_state *state, struct v4l2_subdev_mbus_code_enum *code)
{
        struct vc_cam *cam = to_vc_cam(sd);
        int i;
        for(i = 0; i < MAX_MBUS_CODES; i++)
        {
               if(cam->ctrl.mbus_codes[i] == 0)
               break;
        }

        if (code->pad >= NUM_PADS)
		return -EINVAL;
        if (code->pad == IMAGE_PAD) {
		if (code->index >= i)
			return -EINVAL;

		code->code = cam->ctrl.mbus_codes[code->index];
	} else {
		if (code->index > 0)
			return -EINVAL;

		code->code = MEDIA_BUS_FMT_SENSOR_DATA;
	}

        return 0;

        

}

int vc_sd_enum_frame_size(struct v4l2_subdev *sd, struct v4l2_subdev_state *cfg, struct v4l2_subdev_frame_size_enum *fse)
{
        struct vc_device *device = to_vc_device(sd);
        struct vc_cam *cam = to_vc_cam(sd);

        if (fse->index != 0)
                return -EINVAL;

        mutex_lock(&device->mutex);

        if (fse->code != vc_core_get_format(cam))
        {
                mutex_unlock(&device->mutex);
                return -EINVAL;
        }

        fse->min_width = cam->ctrl.frame.width;
        fse->max_width = fse->min_width;
        fse->min_height = cam->ctrl.frame.height;
        fse->max_height = fse->min_height;

        mutex_unlock(&device->mutex);

        return 0;
}

static struct v4l2_mbus_framefmt *__cam_get_pad_format(struct v4l2_subdev *sd,
                                                       struct v4l2_subdev_state *cfg,
                                                       unsigned int pad, enum v4l2_subdev_format_whence which)
{
        struct vc_device *device = to_vc_device(sd);
        struct vc_cam *cam __maybe_unused = to_vc_cam(sd);
        switch (which)
        {
        case V4L2_SUBDEV_FORMAT_TRY:
#if KERNEL_VERSION(6, 0, 0) <= LINUX_VERSION_CODE
                return v4l2_subdev_get_try_format(sd, cfg, pad);

#else
                return v4l2_subdev_get_try_format(sd, cfg, pad);

#endif
        case V4L2_SUBDEV_FORMAT_ACTIVE:
                return &device->format;
        default:
                return NULL;
        }
}

static struct v4l2_rect *__maybe_unused __cam_get_pad_crop(struct v4l2_subdev *sd,
                                                           struct v4l2_subdev_state *cfg,
                                                           unsigned int pad, enum v4l2_subdev_format_whence which)
{
        struct vc_device *device = to_vc_device(sd);
        struct vc_cam *cam __maybe_unused = to_vc_cam(sd);
        switch (which)
        {
        case V4L2_SUBDEV_FORMAT_TRY:
#if KERNEL_VERSION(6, 0, 0) <= LINUX_VERSION_CODE
                return v4l2_subdev_get_try_crop(sd, cfg, pad);

#else
                return v4l2_subdev_get_try_crop(sd, cfg, pad);

#endif
        case V4L2_SUBDEV_FORMAT_ACTIVE:
                return &device->crop_rect;
        default:
                return NULL;
        }
}

static int vc_sd_get_selection(struct v4l2_subdev *sd,
                               struct v4l2_subdev_state *cfg,
                               struct v4l2_subdev_selection *sel)
{
    struct vc_device *device = to_vc_device(sd);
    struct vc_cam *cam = to_vc_cam(sd);
    struct v4l2_rect r;
    if (sel->target == V4L2_SEL_TGT_CROP_DEFAULT || sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
        r.left = cam->ctrl.frame.left;
        r.top = cam->ctrl.frame.top;
        r.width = cam->ctrl.frame.width;
        r.height = cam->ctrl.frame.height;
        sel->r = r;
        return 0;
    }

    if (sel->target != V4L2_SEL_TGT_CROP)
        return -EINVAL;

    sel->r = device->crop_rect;
    return 0;
}

static int vc_sd_set_selection(struct v4l2_subdev *sd,
                               struct v4l2_subdev_state *cfg,
                               struct v4l2_subdev_selection *sel)
{
    struct vc_device *device = to_vc_device(sd);
    struct vc_cam *cam = to_vc_cam(sd);
    struct v4l2_rect rect;

    if (sel->target != V4L2_SEL_TGT_CROP)
        return -EINVAL;

    rect.left = clamp_t(s32, ALIGN(sel->r.left, 2), 0, cam->ctrl.frame.width - 2);
    rect.top = clamp_t(s32, ALIGN(sel->r.top, 2), 0, cam->ctrl.frame.height - 2);
    rect.width = clamp_t(s32, ALIGN(sel->r.width, 2), 2, cam->ctrl.frame.width - rect.left);
    rect.height = clamp_t(s32, ALIGN(sel->r.height, 2), 2, cam->ctrl.frame.height - rect.top);

    device->crop_rect = rect;
    sel->r = rect;

    return 0;
}

// --- v4l2_ctrl_ops ---------------------------------------------------

int vc_ctrl_s_ctrl(struct v4l2_ctrl *ctrl)
{
        struct vc_device *device = container_of(ctrl->handler, struct vc_device, ctrl_handler);
        // struct i2c_client *client = device->cam.ctrl.client_sen;
        struct v4l2_control control;

        // V4L2 controls values will be applied only when power is already up
        // if (!pm_runtime_get_if_in_use(&client->dev))
        // 	return 0;

        mutex_lock(&device->mutex);

        control.id = ctrl->id;
        control.value = ctrl->val;
        vc_sd_s_ctrl(&device->sd, &control);

        mutex_unlock(&device->mutex);

        return 0;
}

// MS
static int vc_get_bit_depth(__u8 mipi_format)
{

        switch (mipi_format)
        {
        case FORMAT_RAW08:
                return 8;
                break;
        case FORMAT_RAW10:
                return 10;
                break;
        case FORMAT_RAW12:
                return 12;
                break;
        case FORMAT_RAW14:
                return 14;
                break;
        default:;
                break;
        }
        return 0;
}



// *** Initialisation *********************************************************



static int __maybe_unused vc_check_hwcfg(struct vc_cam *cam, struct device *dev)
{
        struct fwnode_handle *endpoint;
        struct v4l2_fwnode_endpoint ep_cfg = {
            .bus_type = V4L2_MBUS_CSI2_DPHY};
        int ret = -EINVAL;

        endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(dev), NULL);
        if (!endpoint)
        {
                dev_err(dev, "Endpoint node not found!\n");
                return -EINVAL;
        }

        if (v4l2_fwnode_endpoint_alloc_parse(endpoint, &ep_cfg))
        {
                dev_err(dev, "Could not parse endpoint!\n");
                goto error_out;
        }

        /* Set and check the number of MIPI CSI2 data lanes */
        ret = vc_core_set_num_lanes(cam, ep_cfg.bus.mipi_csi2.num_data_lanes);

error_out:
        v4l2_fwnode_endpoint_free(&ep_cfg);
        fwnode_handle_put(endpoint);

        return ret;
}

static const struct v4l2_subdev_core_ops vc_core_ops = {
    .s_power = vc_sd_s_power,
    .subscribe_event = v4l2_ctrl_subdev_subscribe_event,
    .unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops vc_video_ops = {
    .s_stream = vc_sd_s_stream,
};

static const struct v4l2_subdev_pad_ops vc_pad_ops = {
    .get_fmt = vc_sd_get_fmt,
    .set_fmt = vc_sd_set_fmt,
    .enum_mbus_code = vc_sd_enum_mbus_code,
    .enum_frame_size = vc_sd_enum_frame_size,
    .get_selection = vc_sd_get_selection,
    .set_selection = vc_sd_set_selection,
};

static const struct v4l2_subdev_ops vc_subdev_ops = {
    .core = &vc_core_ops,
    .video = &vc_video_ops,
    .pad = &vc_pad_ops,
};

static const struct v4l2_ctrl_ops vc_ctrl_ops = {
    .s_ctrl = vc_ctrl_s_ctrl,
};

static int vc_ctrl_init_ctrl(struct vc_device *device, struct v4l2_ctrl_handler *hdl, int id, struct vc_control *control)
{
        struct i2c_client *client = device->cam.ctrl.client_sen;
        struct device *dev = &client->dev;
        struct v4l2_ctrl *ctrl;

        ctrl = v4l2_ctrl_new_std(&device->ctrl_handler, &vc_ctrl_ops, id, control->min, control->max, 1, control->def);
        if (ctrl == NULL)
        {
                vc_err(dev, "%s(): Failed to init 0x%08x ctrl\n", __func__, id);
                return -EIO;
        }

        return 0;
}
static int vc_ctrl_init_ctrl_special(struct vc_device *device, struct v4l2_ctrl_handler *hdl, int id, int min, int max, int def)
{
        struct i2c_client *client = device->cam.ctrl.client_sen;
        struct device *dev = &client->dev;
        struct v4l2_ctrl *ctrl;

        ctrl = v4l2_ctrl_new_std(&device->ctrl_handler, &vc_ctrl_ops, id, min, max, 1, def);
        if (ctrl == NULL) {
                vc_err(dev, "%s(): Failed to init 0x%08x ctrl\n", __func__, id);
                return -EIO;
        }

        return 0;
}


static int vc_ctrl_init_ctrl_lfreq(struct vc_device *device, struct v4l2_ctrl_handler *hdl, int id, struct vc_control64 *control)
{
        struct i2c_client *client = device->cam.ctrl.client_sen;
        struct device *dev = &client->dev;
        struct v4l2_ctrl *ctrl;

        // CAUTION: only ONE element in linkfreq array used !
        ctrl = v4l2_ctrl_new_int_menu(&device->ctrl_handler, &vc_ctrl_ops, id, 0, 0, &control->def);
        if (ctrl == NULL)
        {
                vc_err(dev, "%s(): Failed to init 0x%08x ctrl\n", __func__, id);
                return -EIO;
        }

        if (ctrl)
                ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

        return 0;
}

static int vc_ctrl_init_ctrl_lc(struct vc_device *device, struct v4l2_ctrl_handler *hdl)
{
        struct i2c_client *client = device->cam.ctrl.client_sen;
        struct device *dev = &client->dev;
        struct v4l2_fwnode_device_properties props;
        struct v4l2_ctrl *ctrl;
        int ret;

        ret = v4l2_fwnode_device_parse(dev, &props);
        if (ret < 0)
                return ret;

        vc_info(dev, "%s(): orientation=%d rotation=%d", __func__, props.orientation, props.rotation);

        ret = v4l2_ctrl_new_fwnode_properties(hdl, &vc_ctrl_ops, &props);
        if (ret < 0)
                vc_err(dev, "%s(): Failed to init fwnode ctrls ... will continue anyway\n", __func__);

        ctrl = v4l2_ctrl_new_std(hdl, &vc_ctrl_ops, V4L2_CID_HFLIP, 0, 1, 1, 0);
        if (ctrl == NULL)
                goto ctrl_err;

        ctrl = v4l2_ctrl_new_std(hdl, &vc_ctrl_ops, V4L2_CID_VFLIP, 0, 1, 1, 0);

ctrl_err:
        if (ctrl == NULL)
        {
                vc_err(dev, "%s(): Failed to init lc ctrls\n", __func__);
                return -EIO;
        }

        return 0;
}
#if 1
static int vc_ctrl_init_custom_ctrl(struct vc_device *device, struct v4l2_ctrl_handler *hdl, const struct v4l2_ctrl_config *config)
{
        struct i2c_client *client = device->cam.ctrl.client_sen;
        struct device *dev = &client->dev;
        struct v4l2_ctrl *ctrl;

        ctrl = v4l2_ctrl_new_custom(&device->ctrl_handler, config, NULL);
        if (ctrl == NULL)
        {
                vc_err(dev, "%s(): Failed to init 0x%08x ctrl\n", __func__, config->id);
                return -EIO;
        }

        return 0;
}
static const struct v4l2_ctrl_config ctrl_rotation = {
    .ops = &vc_ctrl_ops,
    .id = V4L2_CID_CAMERA_SENSOR_ROTATION,
    .name = "Sensor rotation",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE & V4L2_CTRL_FLAG_MODIFY_LAYOUT ,
    .min = 0,
    .max = 360,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config ctrl_orientation = {
    .ops = &vc_ctrl_ops,
    .id = V4L2_CID_CAMERA_ORIENTATION,
    .name = "Sensor orientation",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
    .min = 0,
    .max = 2,
    .step = 1,
    .def = V4L2_CAMERA_ORIENTATION_FRONT,
};

static const struct v4l2_ctrl_config ctrl_trigger_mode = {
    .ops = &vc_ctrl_ops,
    .id = V4L2_CID_VC_TRIGGER_MODE,
    .name = "Trigger Mode",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
    .min = 0,
    .max = 7,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config ctrl_flash_mode = {
    .ops = &vc_ctrl_ops,
    .id = V4L2_CID_VC_IO_MODE,
    .name = "IO Mode",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
    .min = 0,
    .max = 5,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config ctrl_frame_rate = {
    .ops = &vc_ctrl_ops,
    .id = V4L2_CID_VC_FRAME_RATE,
    .name = "Frame Rate",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
    .min = 0,
    .max = 1000000,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config ctrl_single_trigger = {
    .ops = &vc_ctrl_ops,
    .id = V4L2_CID_VC_SINGLE_TRIGGER,
    .name = "Single Trigger",
    .type = V4L2_CTRL_TYPE_BUTTON,
    .flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
    .min = 0,
    .max = 0,
    .step = 0,
    .def = 0,
};

static const struct v4l2_ctrl_config ctrl_binning_mode = {
    .ops = &vc_ctrl_ops,
    .id = V4L2_CID_VC_BINNING_MODE,
    .name = "Binning Mode",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE & V4L2_CTRL_FLAG_MODIFY_LAYOUT ,
    .min = 0,
    .max = 4,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config ctrl_roi_position = {
    .ops = &vc_ctrl_ops,
    .id = V4L2_CID_VC_ROI_POSITION,
    .name = "Roi Position",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
    .min = 0,
    .max = 199999999,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config ctrl_blacklevel = {
    .ops = &vc_ctrl_ops,
    .id = V4L2_CID_BLACK_LEVEL, // See https://github.com/VC-MIPI-modules/vc_mipi_nvidia/blob/master/doc/BLACK_LEVEL.md
    .name = "Black Level",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
    .min = 0,
    .max = 100000,
    .step = 1,
    .def = 0,
};


static struct v4l2_ctrl_config ctrl_hblank = {
        .ops = &vc_ctrl_ops,
        .id = V4L2_CID_HBLANK, 
        .name = "Horizontal blanking",
        .type = V4L2_CTRL_TYPE_INTEGER,
        .flags = V4L2_CTRL_FLAG_READ_ONLY,
        .min = 0,
        .max = 100000,
        .step = 1,
        .def = 0,
    };
static struct v4l2_ctrl_config ctrl_vblank = {
        .ops = &vc_ctrl_ops,
        .id = V4L2_CID_VBLANK, 
        .name = "Vertical blanking",
        .type = V4L2_CTRL_TYPE_INTEGER,
        .flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
        .min = 0,
        .max = 100000,
        .step = 1,
        .def = 0,
    };

#endif


static void vc_update_clk_rates(struct vc_cam *cam)
{
        struct vc_desc_mode *mode_desc = &cam->desc.modes[cam->state.mode];

        vc_mode *mode;

        for(int i = 0; i < MAX_VC_DESC_MODES; i++) //TODO Replace 24 by define
        {
                if(mode_desc->format == cam->ctrl.mode[i].format && 
                        mode_desc->num_lanes == cam->ctrl.mode[i].num_lanes && 
                        mode_desc->binning == cam->ctrl.mode[i].binning)
                {
                        mode = &cam->ctrl.mode[i];
                        break;
                }
        }
        int num_lanes = mode->num_lanes;
        int bit_depth = vc_get_bit_depth(mode->format);

        linkfreq.max = (unsigned long long)(cam->ctrl.clk_pixel) * bit_depth;
        linkfreq.def = linkfreq.max;
        linkfreq.min = linkfreq.max;
        pixel_rate.max = cam->ctrl.clk_pixel * 2 * num_lanes; //DDR Double Data Rate
        pixel_rate.def = pixel_rate.max;


        vblank.min = mode->vmax.min ;
        vblank.max = mode->vmax.max;
        vblank.def = mode->vmax.min;

       

        ctrl_vblank.min = vblank.min;
        ctrl_vblank.max = vblank.max;
        ctrl_vblank.def = vblank.def;

        hblank.min = mode->hmax * num_lanes * 2 - cam->state.frame.width ;
        hblank.max = hblank.min;
        hblank.def = hblank.min;


        ctrl_hblank.max = hblank.min;
        ctrl_hblank.min = hblank.min;
        ctrl_hblank.def = hblank.min;
}
static void update_frame_rate_ctrl(struct vc_cam *cam, struct vc_device *device)
{
        struct v4l2_ctrl *ctrl = v4l2_ctrl_find(&device->ctrl_handler, V4L2_CID_VC_FRAME_RATE);
        if (ctrl)
        {              
                ctrl->maximum = cam->ctrl.framerate.max;
                ctrl->minimum = cam->ctrl.framerate.min;
                ctrl->default_value = cam->ctrl.framerate.def;
                ctrl->val = cam->state.framerate;                
        }
}
static int vc_sd_init(struct vc_device *device)
{
        struct i2c_client *client = device->cam.ctrl.client_sen;
        struct device *dev = &client->dev;
        int ret;

        // Initializes the subdevice
        v4l2_i2c_subdev_init(&device->sd, client, &vc_subdev_ops);

        // Initialize the handler
        ret = v4l2_ctrl_handler_init(&device->ctrl_handler, 3);
        if (ret)
        {
                vc_err(dev, "%s(): Failed to init control handler\n", __func__);
                return ret;
        }
        // Hook the control handler into the driver
        device->sd.ctrl_handler = &device->ctrl_handler;

        vc_update_clk_rates(&device->cam);


        // Add controls
        ret |= vc_ctrl_init_ctrl(device, &device->ctrl_handler, V4L2_CID_EXPOSURE, &device->cam.ctrl.exposure);
        ret |= vc_ctrl_init_ctrl_special(device, &device->ctrl_handler, V4L2_CID_GAIN, 
                0, device->cam.ctrl.again.max_mdB + device->cam.ctrl.dgain.max_mdB, 0);;
        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_blacklevel);
        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_orientation);

        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_trigger_mode);
        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_rotation);
        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_flash_mode);
        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_frame_rate);
        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_single_trigger);
        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_binning_mode);
        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_roi_position);

        ret |= vc_ctrl_init_ctrl(device, &device->ctrl_handler, V4L2_CID_PIXEL_RATE, &pixel_rate);
        ret |= vc_ctrl_init_ctrl_lfreq(device, &device->ctrl_handler, V4L2_CID_LINK_FREQ, &linkfreq);
        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_hblank);
        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_vblank);
        // ret |= vc_ctrl_init_ctrl(device, &device->ctrl_handler, V4L2_CID_ANALOGUE_GAIN, &device->cam.ctrl.gain);
        ret |= vc_ctrl_init_ctrl_lc(device, &device->ctrl_handler);
                // Set the standard format
        struct v4l2_subdev_format fmt = {
                .which = V4L2_SUBDEV_FORMAT_ACTIVE,
                .format = {
                .width = device->cam.ctrl.frame.width,
                .height = device->cam.ctrl.frame.height,
                .code = device->cam.state.format_code,
                .field = V4L2_FIELD_NONE,
                .colorspace = V4L2_COLORSPACE_SRGB,
                },
        };

        ret = v4l2_subdev_call(&device->sd, pad, set_fmt, NULL, &fmt);
        if (ret)
        {
                vc_err(dev, "%s(): Failed to set format\n", __func__);
                return ret;
        }

        return 0;
}


static int vc_link_setup(struct media_entity *entity, const struct media_pad *local, const struct media_pad *remote,
                         __u32 flags)
{
        return 0;
}

static const struct media_entity_operations vc_sd_media_ops = {
    .link_setup = vc_link_setup,
};

static int vc_probe(struct i2c_client *client)
{
        struct device *dev = &client->dev;
        struct vc_device *device;
        struct vc_cam *cam;
        int ret;

        vc_notice(dev, "%s(): Probing UNIVERSAL VC MIPI Driver (v%s)\n", __func__, VERSION);

        device = devm_kzalloc(dev, sizeof(*device), GFP_KERNEL);
        if (!device)
                return -ENOMEM;

        cam = &device->cam;
        cam->ctrl.client_sen = client;


        vc_set_power(device, 1);

        ret = vc_core_init(cam, client);
        if (ret)
                goto error_power_off;



        ret = vc_check_hwcfg(cam, dev); 

        if (ret)
                goto error_power_off;

        vc_mod_set_mode(cam, &ret); 
        ret = vc_sd_init(device);
        if (ret)
                goto error_handler_free;

        device->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
        device->pad.flags = MEDIA_PAD_FL_SOURCE;
        device->sd.entity.ops = &vc_sd_media_ops;
        device->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
        ret = media_entity_pads_init(&device->sd.entity, 1, &device->pad);
        if (ret)
                goto error_handler_free;

        ret = v4l2_async_register_subdev_sensor(&device->sd);
        if (ret)
                goto error_media_entity;

        pm_runtime_get_noresume(dev);
        pm_runtime_set_active(dev);
        pm_runtime_enable(dev);
        pm_runtime_set_autosuspend_delay(dev, 2000);
        pm_runtime_use_autosuspend(dev);
        pm_runtime_mark_last_busy(dev);
        pm_runtime_put_autosuspend(dev);

        return 0;

error_media_entity:
        media_entity_cleanup(&device->sd.entity);

error_handler_free:
        v4l2_ctrl_handler_free(&device->ctrl_handler);
        mutex_destroy(&device->mutex);

error_power_off:
        pm_runtime_disable(dev);
        pm_runtime_set_suspended(dev);
        pm_runtime_put_noidle(dev);
        vc_set_power(device, 0);
        return ret;
}

static void vc_remove(struct i2c_client *client)
{
        struct v4l2_subdev *sd = i2c_get_clientdata(client);
        struct vc_device *device = to_vc_device(sd);
        struct vc_cam *cam = to_vc_cam(sd);

        v4l2_async_unregister_subdev(&device->sd);
        media_entity_cleanup(&device->sd.entity);
        v4l2_ctrl_handler_free(&device->ctrl_handler);
        pm_runtime_disable(&client->dev);
        mutex_destroy(&device->mutex);

        pm_runtime_get_sync(&client->dev);
        pm_runtime_disable(&client->dev);
        pm_runtime_set_suspended(&client->dev);
        pm_runtime_put_noidle(&client->dev);

        if (cam->ctrl.client_mod)
                i2c_unregister_device(cam->ctrl.client_mod);

        vc_set_power(device, 0);
}

static const struct dev_pm_ops vc_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(vc_suspend, vc_resume)};

static const struct i2c_device_id vc_id[] = {
    {"vc_mipi_camera", 0},
    {/* sentinel */},
};
MODULE_DEVICE_TABLE(i2c, vc_id);

static const struct of_device_id vc_dt_ids[] = {
    {.compatible = "vc,vc_mipi_modules"},
    {/* sentinel */}};
MODULE_DEVICE_TABLE(of, vc_dt_ids);

static struct i2c_driver vc_i2c_driver = {
    .driver = {
        .name = "vc_mipi_camera",
        .pm = &vc_pm_ops,
        .of_match_table = vc_dt_ids,
    },
    .id_table = vc_id,
    .probe = vc_probe,
    .remove = vc_remove,
};

module_i2c_driver(vc_i2c_driver);

MODULE_VERSION(VERSION);
MODULE_DESCRIPTION("Vision Components GmbH - VC MIPI CSI-2 driver");
MODULE_AUTHOR("Peter Martienssen, Liquify Consulting <peter.martienssen@liquify-consulting.de>");
MODULE_AUTHOR("Michael Steinel, Vision Components GmbH <mipi-tech@vision-components.com>");
MODULE_LICENSE("GPL v2");

module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-6)");