#include <vc_mipi_v4l2_helper.h>
#include <vc_mipi_core.h>
#include <cerrno>
// --- Prototypes --------------------------------------------------------------


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
EXPORT_SYMBOL(vc_ctrl_init_ctrl);
static int vc_ctrl_init_ctrl_special(struct v4l2_ctrl_ops *vc_ctrl_ops, struct v4l2_ctrl_handler *hdl, int id, int min, int max, int def)
{
    
        struct v4l2_ctrl *ctrl;

        ctrl = v4l2_ctrl_new_std(hdl, vc_ctrl_ops, id, min, max, 1, def);
        if (ctrl == NULL) {
                vc_err(dev, "%s(): Failed to init 0x%08x ctrl\n", __func__, id);
                return -EIO;
        }

        return 0;
}
EXPORT_SYMBOL(vc_ctrl_init_ctrl_special);

static int vc_ctrl_init_ctrl_lfreq(struct vc_device *device, struct v4l2_ctrl_ops *vc_ctrl_ops,  struct v4l2_ctrl_handler ctrl_handler, int id, struct vc_control64 *control)
{
 
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
EXPORT_SYMBOL(vc_ctrl_init_ctrl_lfreq);