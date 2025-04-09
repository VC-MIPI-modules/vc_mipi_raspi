#include "vc_mipi_camera_extras.h"


#define IMX335_REG_TPG_TESTCLKEN	0x3148
#define IMX335_REG_TPG_DIG_CLP_MODE	0x3280
#define IMX335_REG_TPG_EN_DUOUT		0x329c
#define IMX335_REG_TPG			    0x329e
#define IMX335_REG_TPG_COLORWIDTH	0x32a0
#define IMX335_REG_WRJ_OPEN		    0x336c


#define IMX335_TPG_ALL_000		0
#define IMX335_TPG_ALL_FFF		1
#define IMX335_TPG_ALL_555		2
#define IMX335_TPG_ALL_AAA		3
#define IMX335_TPG_TOG_555_AAA		4
#define IMX335_TPG_TOG_AAA_555		5
#define IMX335_TPG_TOG_000_555		6
#define IMX335_TPG_TOG_555_000		7
#define IMX335_TPG_TOG_000_FFF		8
#define IMX335_TPG_TOG_FFF_000		9
#define IMX335_TPG_H_COLOR_BARS		10
#define IMX335_TPG_V_COLOR_BARS		11


static const char * const imx335_tpg_menu[] = {
	"Disabled",
	"All 000h",
	"All FFFh",
	"All 555h",
	"All AAAh",
	"Toggle 555/AAAh",
	"Toggle AAA/555h",
	"Toggle 000/555h",
	"Toggle 555/000h",
	"Toggle 000/FFFh",
	"Toggle FFF/000h",
	// "Horizontal color bars",
	// "Vertical color bars",
};

static const int imx335_tpg_val[] = {
	IMX335_TPG_ALL_000,
	IMX335_TPG_ALL_000,
	IMX335_TPG_ALL_FFF,
	IMX335_TPG_ALL_555,
	IMX335_TPG_ALL_AAA,
	IMX335_TPG_TOG_555_AAA,
	IMX335_TPG_TOG_AAA_555,
	IMX335_TPG_TOG_000_555,
	IMX335_TPG_TOG_555_000,
	IMX335_TPG_TOG_000_FFF,
	IMX335_TPG_TOG_FFF_000,
	// IMX335_TPG_H_COLOR_BARS,
	// IMX335_TPG_V_COLOR_BARS,
};

int imx335_init_control(struct vc_device *device, struct v4l2_ctrl_handler *hdl, struct v4l2_ctrl_ops *ctrl_ops)
{
    struct v4l2_ctrl *ctrl;
    struct device *dev = &device->cam.ctrl.client_sen->dev;

    ctrl = v4l2_ctrl_new_std_menu_items(hdl,
        ctrl_ops,
        V4L2_CID_TEST_PATTERN,
        ARRAY_SIZE(imx335_tpg_menu) - 1,
        0, 0, imx335_tpg_menu);

    if (ctrl == NULL)
    {
            vc_err(dev, "%s(): Failed to init 0x%08x ctrl\n", __func__, V4L2_CID_TEST_PATTERN);
            return -EIO;
    }
    ctrl->flags |= V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;


    return 0;
}

int imx335_update_test_pattern(struct vc_cam *cam, u32 pattern_index)
{
	int ret = 0;

	if (pattern_index >= ARRAY_SIZE(imx335_tpg_val))
		return -EINVAL;

	if (pattern_index) {	
                ret |= vc_write_i2c_reg(cam->ctrl.client_sen, IMX335_REG_TPG,
                        imx335_tpg_val[pattern_index]);	

                ret |= vc_write_i2c_reg(cam->ctrl.client_sen, IMX335_REG_TPG_TESTCLKEN, 0x10);
                ret |= vc_write_i2c_reg(cam->ctrl.client_sen, IMX335_REG_TPG_DIG_CLP_MODE, 0x00);
                ret |= vc_write_i2c_reg(cam->ctrl.client_sen, IMX335_REG_TPG_EN_DUOUT, 0x01);
                ret |= vc_write_i2c_reg(cam->ctrl.client_sen, IMX335_REG_TPG_COLORWIDTH, 0x11);
                ret |= vc_write_i2c_reg2(cam->ctrl.client_sen, &cam->ctrl.csr.sen.blacklevel, 0x00);
                ret |= vc_write_i2c_reg(cam->ctrl.client_sen, IMX335_REG_WRJ_OPEN, 0x00);

        // Set the test pattern value
     

		
	} else {
		

        // Disable the test pattern
        ret |= vc_write_i2c_reg(cam->ctrl.client_sen, IMX335_REG_TPG_TESTCLKEN, 0x00);
        ret |= vc_write_i2c_reg(cam->ctrl.client_sen, IMX335_REG_TPG_DIG_CLP_MODE, 0x01);
        ret |= vc_write_i2c_reg(cam->ctrl.client_sen, IMX335_REG_TPG_EN_DUOUT, 0x00);
        ret |= vc_write_i2c_reg(cam->ctrl.client_sen, IMX335_REG_TPG_COLORWIDTH, 0x10);
        ret |= vc_write_i2c_reg2(cam->ctrl.client_sen, &cam->ctrl.csr.sen.blacklevel, cam->state.blacklevel);
        ret |= vc_write_i2c_reg(cam->ctrl.client_sen, IMX335_REG_WRJ_OPEN, 0x01);

	
	}

	return ret;
}

int vc_ctrl_init_ctrl_std_menu(struct vc_device *device, struct v4l2_ctrl_handler *hdl, struct v4l2_ctrl_ops *ctrl_ops, int id, const char * const items[], size_t items_count)
{
        struct i2c_client *client = device->cam.ctrl.client_sen;
        struct device *dev = &client->dev;
        struct v4l2_ctrl *ctrl;

        for (size_t i = 0; i < items_count; i++) {
            }
        ctrl = v4l2_ctrl_new_std_menu_items(&device->ctrl_handler, ctrl_ops, id, items_count - 1, 0, 0, items);
        if (ctrl == NULL)
        {
                vc_err(dev, "%s(): Failed to init 0x%08x ctrl\n", __func__, id);
                return -EIO;
        }

        return 0;
}