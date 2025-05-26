#include "../vc_mipi_core/vc_mipi_core.h"
#include <linux/module.h>
#include <linux/gpio/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/version.h>
#include <linux/of_graph.h> 
#include <linux/property.h> // For device_property_read_bool()

#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-event.h>

#define VERSION "0.6.2"

int debug = 3;
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
static vc_mode *vc_get_mode(struct vc_cam *cam);

// --- Structures --------------------------------------------------------------

enum private_cids
{
        V4L2_CID_VC_TRIGGER_MODE = V4L2_CID_USER_BASE | 0xfff0, // TODO FOR NOW USE 0xfff0 offset
        V4L2_CID_VC_IO_MODE,
        V4L2_CID_VC_FRAME_RATE,
        V4L2_CID_VC_SINGLE_TRIGGER,
        V4L2_CID_VC_BINNING_MODE,
        V4L2_CID_VC_ROI_POSITION,
        V4L2_CID_VC_NAME,
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
        struct vc_cam cam;
        bool libcamera_enabled;
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


static        struct vc_control hblank; 
static        struct vc_control vblank; 
static        struct vc_control pixel_rate; 

static struct vc_control64 linkfreq  = {
        .min = 0,
        .max = 0,
        .def = 0,
    };

static void update_frame_rate_ctrl(struct vc_cam *cam, struct vc_device *device);
int vc_sd_update_fmt(struct vc_device *device);

static void vc_get_binning_scale(struct vc_cam *cam, __u8 *h_scale, __u8 *v_scale)
{
    struct vc_binning *binning = vc_core_get_binning(cam);

    *h_scale = binning->h_factor == 0 ? 1 : binning->h_factor;
    *v_scale = binning->v_factor == 0 ? 1 : binning->v_factor;
}
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



static int vc_sd_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *control)
{
        struct vc_cam *cam = to_vc_cam(sd);
        struct device *dev = vc_core_get_sen_device(cam);
        struct vc_device *device = to_vc_device(sd);
        int ret = 0;
        vc_mode *mode = vc_get_mode(cam);

        switch (control->id)
        {

        case V4L2_CID_HBLANK:
                vc_core_set_hmax_overwrite(cam, mode->hmax + control->value);
                vc_sen_set_hmax(cam);
                return 0;
                
        case V4L2_CID_VBLANK:
                vc_core_set_vmax_overwrite(cam,mode->vmax.def + control->value);
                vc_sen_write_vmax(&cam->ctrl, cam->state.vmax_overwrite);
                return 0;
        case V4L2_CID_HFLIP:
        case V4L2_CID_VFLIP:
                return 0; // Currently not planned to be implemented

        case V4L2_CID_EXPOSURE:
                if(device->libcamera_enabled)
                {
                        // libcamera's unit for exposure is in lines count
                        vc_sen_set_exposure(cam, control->value * vc_core_get_time_per_line_ns(cam) / 1000);
                        return 0;
                }
                else
                {
                        return vc_sen_set_exposure(cam, control->value );
                }
      

        case V4L2_CID_ANALOGUE_GAIN:
        case V4L2_CID_GAIN:
                return vc_sen_set_gain(cam, control->value, true);

        case V4L2_CID_BLACK_LEVEL:
                return vc_sen_set_blacklevel(cam, control->value);
        case V4L2_CID_VC_TRIGGER_MODE:
                return vc_mod_set_trigger_mode(cam, control->value);

        case V4L2_CID_VC_IO_MODE:
                return vc_mod_set_io_mode(cam, control->value);

        case V4L2_CID_VC_FRAME_RATE:
                ret =  vc_core_set_framerate(cam, control->value);                
                vc_update_clk_rates(device, cam);
                return ret;

        case V4L2_CID_VC_SINGLE_TRIGGER:
                return vc_mod_set_single_trigger(cam);

        case V4L2_CID_VC_BINNING_MODE:
                ret = vc_core_set_binning_mode(cam, control->value);
                mutex_unlock(&device->mutex);                
                        vc_sd_update_fmt(device);
                mutex_lock(&device->mutex);

                return ret;

        case V4L2_CID_VC_ROI_POSITION:
                return vc_core_live_roi(cam, control->value);


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
                        goto err_unlock;

                }

               
                ret |= vc_sen_set_exposure(cam, cam->state.exposure );
                update_frame_rate_ctrl(cam,device);
                if (!ret && reset)
                {
                        ret |= vc_sen_set_gain(cam, cam->state.gain, true);
                        ret |= vc_sen_set_blacklevel(cam, cam->state.blacklevel);
                }

                ret = vc_sen_start_stream(cam);
                if (ret)
                {
                       goto err_rpm_put;
                }
        }
        else
        {
             
                vc_sen_stop_stream(cam);
                pm_runtime_put(dev);
        }

        state->streaming = enable;
        mutex_unlock(&device->mutex);

        return ret;
  err_rpm_put:
                enable = 0;
                vc_sen_stop_stream(cam);
	        pm_runtime_put(dev);
  err_unlock:
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
        __u8 h_scale, v_scale;
        vc_get_binning_scale(cam, &h_scale, &v_scale);
        vc_frame *frame = vc_core_get_frame(cam);
        int codeIx;
        if (fse->index != 0)
                return -EINVAL;

        mutex_lock(&device->mutex);

        bool format_supported = false;

        for (codeIx = 0; codeIx < ARRAY_SIZE(cam->ctrl.mbus_codes); codeIx++) {
                if (cam->ctrl.mbus_codes[codeIx] == fse->code) {
                        format_supported = true;
                        break;         
                }       
        }        

        if (!format_supported)
        {
                mutex_unlock(&device->mutex);
                return -EINVAL;
        }


        // Frame sizes are the same for different formats

        fse->min_width = 32;
        fse->max_width = frame->width / h_scale;
        fse->min_height = 32;
        fse->max_height = frame->height /v_scale;

        mutex_unlock(&device->mutex);

        return 0;
}




static int vc_sd_get_selection(struct v4l2_subdev *sd,
                               struct v4l2_subdev_state *cfg,
                               struct v4l2_subdev_selection *sel)
{
    struct vc_cam *cam = to_vc_cam(sd);
    __u8 h_scale, v_scale;
    vc_get_binning_scale(cam, &h_scale, &v_scale);

    struct v4l2_rect r;
    switch(sel->target)
    {
        case V4L2_SEL_TGT_COMPOSE:
        case V4L2_SEL_TGT_COMPOSE_DEFAULT:
        case V4L2_SEL_TGT_COMPOSE_BOUNDS:
            r.left = cam->ctrl.frame.left / h_scale;
            r.top = cam->ctrl.frame.top / v_scale;
            r.width = cam->ctrl.frame.width / h_scale;
            r.height = cam->ctrl.frame.height / v_scale;
            sel->r = r;
            return 0;
        case V4L2_SEL_TGT_CROP:
        case V4L2_SEL_TGT_CROP_DEFAULT:
        case V4L2_SEL_TGT_CROP_BOUNDS:
        r.left = cam->ctrl.frame.left / h_scale;
        r.top = cam->ctrl.frame.top / v_scale;
        r.width = cam->ctrl.frame.width / h_scale;
        r.height = cam->ctrl.frame.height / v_scale;
        sel->r = r;
        return 0;

        
    }

   
    return -1;
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
        struct i2c_client *client = device->cam.ctrl.client_sen;
        struct v4l2_control control;

        // V4L2 controls values will be applied only when power is already up
        if (!pm_runtime_get_if_in_use(&client->dev))
        	return 0;


        control.id = ctrl->id;
        control.value = ctrl->val;
        vc_sd_s_ctrl(&device->sd, &control);

	pm_runtime_put(&client->dev);

        return 0;
}

static int vc_ctrl_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
        struct vc_device *device = container_of(ctrl->handler, struct vc_device, ctrl_handler);
        if (ctrl->id == V4L2_CID_VC_NAME) {
                strscpy(ctrl->p_new.p_char, device->cam.desc.sen_type, ctrl->maximum);
                return 0;
        }
    return -EINVAL;
}

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



static int vc_check_hwcfg(struct vc_cam *cam, struct device *dev, struct vc_device *device)
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

        if (device_property_read_bool(dev, "libcamera")) {
                device->libcamera_enabled = true;
                dev_info(dev, "libcamera support enabled\n");
        } else {
                dev_info(dev, "libcamera support disabled\n");
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
    .g_volatile_ctrl = vc_ctrl_g_volatile_ctrl,
};

static int vc_ctrl_init_ctrl(struct vc_device *device, struct v4l2_ctrl_handler *hdl, int id, struct vc_control *control, int flags)
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
        if (flags)
                ctrl->flags |= flags;

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

// static int vc_ctrl_init_ctrl_std_menu(struct vc_device *device, struct v4l2_ctrl_handler *hdl, int id, const char * const items[], size_t items_count)
// {
//         struct i2c_client *client = device->cam.ctrl.client_sen;
//         struct device *dev = &client->dev;
//         struct v4l2_ctrl *ctrl;

//         for (size_t i = 0; i < items_count; i++) {
//             }
//         ctrl = v4l2_ctrl_new_std_menu_items(&device->ctrl_handler, &vc_ctrl_ops, id, items_count - 1, 0, 0, items);
//         if (ctrl == NULL)
//         {
//                 vc_err(dev, "%s(): Failed to init 0x%08x ctrl\n", __func__, id);
//                 return -EIO;
//         }

//         return 0;
// }

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
static int vc_ctrl_init_custom_ctrl(struct vc_device *device, struct v4l2_ctrl_handler *hdl, const struct v4l2_ctrl_config *config, struct v4l2_ctrl **ctrl)
{
        struct i2c_client *client = device->cam.ctrl.client_sen;
        struct device *dev = &client->dev;

        *ctrl = v4l2_ctrl_new_custom(&device->ctrl_handler, config, NULL);
        if (*ctrl == NULL)
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

static const char * const trigger_mode_menu[] = {
    "Off",
    "External",
    "Pulse Width",
    "Self",
    "Single",
    "Sync",
    "Stream Edge",
    "Stream Level"
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
    .qmenu = trigger_mode_menu,
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

static const struct v4l2_ctrl_config ctrl_name = {
    .ops = &vc_ctrl_ops,
    .id = V4L2_CID_VC_NAME, // See https://github.com/VC-MIPI-modules/vc_mipi_nvidia/blob/master/doc/BLACK_LEVEL.md
    .name = "Sensor name",
    .type = V4L2_CTRL_TYPE_STRING,
    .flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
    .min = 0,
    .max = 10,
    .step = 1,
    .def = 0,
};



struct v4l2_subdev_format fmt = {
        .which = V4L2_SUBDEV_FORMAT_ACTIVE,
        .format = {
        .width = 0,
        .height = 0,
        .code = 0,
        .field = V4L2_FIELD_NONE,
        .colorspace = V4L2_COLORSPACE_SRGB,
        },
};


static vc_mode *vc_get_mode(struct vc_cam *cam)
{
        struct vc_desc_mode *mode_desc = &cam->desc.modes[cam->state.mode];
        vc_mode *mode = NULL;

        for(int i = 0; i < MAX_VC_DESC_MODES; i++)
        {
                if(mode_desc->format == cam->ctrl.mode[i].format && 
                        mode_desc->num_lanes == cam->ctrl.mode[i].num_lanes && 
                        mode_desc->binning == cam->ctrl.mode[i].binning)
                {
                        mode = &cam->ctrl.mode[i];
                        break;
                }
        }
        return mode;
}

static void vc_update_clk_rates(struct vc_device *device, struct vc_cam *cam)
{
        vc_mode *mode = vc_get_mode(cam);
        int num_lanes = mode->num_lanes;
        int bit_depth = vc_get_bit_depth(mode->format);

        linkfreq.max = (unsigned long long)(cam->ctrl.clk_pixel) * bit_depth;
        linkfreq.def = linkfreq.max;
        linkfreq.min = linkfreq.max;
        pixel_rate.max = cam->ctrl.clk_pixel * 2 * num_lanes; //DDR Double Data Rate
        pixel_rate.def = pixel_rate.max;


        vblank.min = mode->vmax.min;
        vblank.max = mode->vmax.max;
        vblank.def = mode->vmax.min;



        hblank.min = 0 ;
        hblank.max = 0;
        hblank.def = hblank.min;



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
int vc_sd_update_fmt(struct vc_device *device)
{
        __u8 h_scale, v_scale;
        vc_get_binning_scale(&device->cam, &h_scale, &v_scale);

        fmt.format.code = device->cam.state.format_code;
        fmt.format.width = device->cam.ctrl.frame.width / h_scale;
        fmt.format.height = device->cam.ctrl.frame.height / v_scale;

        // return 0;
               

        return v4l2_subdev_call(&device->sd, pad, set_fmt, NULL, &fmt);
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

        vc_update_clk_rates(device,&device->cam);
        struct v4l2_ctrl *ctrl;


        if(device->libcamera_enabled)
        {
                device->cam.ctrl.exposure.min = 1;
                device->cam.ctrl.exposure.max = 1000000;
                device->cam.ctrl.exposure.def = 10;
        }
     


        // Add controls
        ret |= vc_ctrl_init_ctrl(device, &device->ctrl_handler, V4L2_CID_EXPOSURE, &device->cam.ctrl.exposure, 0);
        ret |= vc_ctrl_init_ctrl_special(device, &device->ctrl_handler, V4L2_CID_ANALOGUE_GAIN, 
                0, device->cam.ctrl.again.max_mdB + device->cam.ctrl.dgain.max_mdB, 0);
                
        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_blacklevel, &ctrl);
        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_orientation, &ctrl);

        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_trigger_mode, &ctrl);
        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_rotation, &ctrl);
        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_flash_mode, &ctrl);
        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_frame_rate, &ctrl);
        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_single_trigger, &ctrl);
        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_binning_mode, &ctrl);
        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_roi_position, &ctrl);
        ret |= vc_ctrl_init_custom_ctrl(device, &device->ctrl_handler, &ctrl_name, &ctrl);

        ret |= vc_ctrl_init_ctrl(device, &device->ctrl_handler, V4L2_CID_PIXEL_RATE, &pixel_rate, 0);
        ret |= vc_ctrl_init_ctrl_lfreq(device, &device->ctrl_handler, V4L2_CID_LINK_FREQ, &linkfreq);
        ret |= vc_ctrl_init_ctrl(device, &device->ctrl_handler, V4L2_CID_HBLANK,  &hblank, 0);
        ret |= vc_ctrl_init_ctrl(device, &device->ctrl_handler, V4L2_CID_VBLANK,  &vblank, 0);
        ret |= vc_ctrl_init_ctrl_lc(device, &device->ctrl_handler);
        if (ret)
        {
                vc_err(dev, "%s(): Failed to set format\n", __func__);
                return ret;
        }

        vc_sd_update_fmt(device);

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

    mutex_init(&device->mutex);

    vc_set_power(device, 1);

    ret = vc_core_init(cam, client);
    if (ret)
        goto error_power_off;

    ret = vc_check_hwcfg(cam, dev, device); 

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

    /* Enable runtime PM and take one usage reference */
    pm_runtime_enable(dev);
    pm_runtime_get_sync(dev);

    return 0;

error_media_entity:
    media_entity_cleanup(&device->sd.entity);
error_handler_free:
    v4l2_ctrl_handler_free(&device->ctrl_handler);
    mutex_destroy(&device->mutex);
error_power_off:
    pm_runtime_disable(dev);
    pm_runtime_set_suspended(dev);
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
    mutex_destroy(&device->mutex);

    /* Drop the usage reference taken in probe and disable runtime PM */
    pm_runtime_put_sync(&client->dev);
    pm_runtime_disable(&client->dev);

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