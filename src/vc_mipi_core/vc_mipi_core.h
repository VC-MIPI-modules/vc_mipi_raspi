#ifndef _VC_MIPI_CORE_H
#define _VC_MIPI_CORE_H

#define ENABLE_ADVANCED_CONTROL

#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/videodev2.h>

// extern int debug;
int debug = 3; // FS Workaround
#define level(level) if (debug >= level)
#define vc_reg(dev, fmt, ...) level(6) dev_info(dev, fmt, ##__VA_ARGS__)
#define vc_dbg(dev, fmt, ...) level(5) dev_info(dev, fmt, ##__VA_ARGS__)
#define vc_info(dev, fmt, ...) level(4) dev_info(dev, fmt, ##__VA_ARGS__)
#define vc_notice(dev, fmt, ...) level(3) dev_notice(dev, fmt, ##__VA_ARGS__)
#define vc_warn(dev, fmt, ...) level(2) dev_warn(dev, fmt, ##__VA_ARGS__)
#define vc_err(dev, fmt, ...) level(1) dev_err(dev, fmt, ##__VA_ARGS__)

#define FLAG_RESET_ALWAYS               (1 <<  0)
#define FLAG_EXPOSURE_SONY              (1 <<  1)
#define FLAG_EXPOSURE_NORMAL            (1 <<  2)
#define FLAG_EXPOSURE_OMNIVISION        (1 <<  3)

#define FLAG_IO_ENABLED                 (1 <<  4)
#define FLAG_FORMAT_GBRG                (1 <<  5)
#define FLAG_DOUBLE_HEIGHT              (1 <<  6)
#define FLAG_INCREASE_FRAME_RATE        (1 <<  7)

#define FLAG_TRIGGER_DISABLE            (1 <<  8)
#define FLAG_TRIGGER_EXTERNAL           (1 <<  9)
#define FLAG_TRIGGER_PULSEWIDTH         (1 << 10)
#define FLAG_TRIGGER_SELF               (1 << 11)
#define FLAG_TRIGGER_SELF_V2            (1 << 12)
#define FLAG_TRIGGER_SINGLE             (1 << 13)
#define FLAG_TRIGGER_SYNC               (1 << 14)
#define FLAG_TRIGGER_STREAM_EDGE        (1 << 15)
#define FLAG_TRIGGER_STREAM_LEVEL       (1 << 16)
#define FLAG_TRIGGER_SLAVE              (1 << 17)

#define FLAG_PREGIUS_S                  (1 << 18)
#define FLAG_USE_BINNING_INDEX          (1 << 19)

#define GAIN_DISABLED                   0
#define GAIN_LINEAR                     1
#define GAIN_LOGARITHMIC                2
#define GAIN_RECIPROCAL                 3
#define GAIN_FRACTIONAL                 4

#define FORMAT_RAW08                    0x2a
#define FORMAT_RAW10                    0x2b
#define FORMAT_RAW12                    0x2c
#define FORMAT_RAW14                    0x2d

#define MAX_VC_MODES                    16
#define MAX_BINNING_MODE_REGS           16

#define MAX_MBUS_CODES                  5
#define MAX_VC_DESC_MODES               24

struct vc_desc_mode {
        __u8 data_rate[4];
        __u8 num_lanes;
        __u8 format;
        __u8 type;
        __u8 binning;
        __u8 reserved2[8];
};

struct vc_desc {
        // Module description
        __u8 magic[12];
        __u8 manuf[32];
        __u16 manuf_id;
        __u8 sen_manuf[8];
        __u8 sen_type[16];
        __u16 mod_id;
        __u16 mod_rev;
        __u16 chip_id_high;
        __u16 chip_id_low;
        __u16 chip_rev;
        // Sensor registers
        __u16 csr_mode;
        __u16 csr_h_start_h;
        __u16 csr_h_start_l;
        __u16 csr_v_start_h;
        __u16 csr_v_start_l;
        __u16 csr_h_end_h;
        __u16 csr_h_end_l;
        __u16 csr_v_end_h;
        __u16 csr_v_end_l;
        __u16 csr_o_width_h;
        __u16 csr_o_width_l;
        __u16 csr_o_height_h;
        __u16 csr_o_height_l;
        __u16 csr_exposure_h;
        __u16 csr_exposure_m;
        __u16 csr_exposure_l;
        __u16 csr_gain_h;
        __u16 csr_gain_l;
        // Exposure Settings
        __u32 clk_ext_trigger;
        __u32 clk_pixel;
        __u16 shutter_offset;
        // Reserved
        __u8 reserved[4];
        // Modes
        __u16 num_modes;
        __u16 bytes_per_mode;
        struct vc_desc_mode modes[MAX_VC_DESC_MODES];
};

typedef struct vc_control {
        __u32 min;
        __u32 max;
        __u32 def;
} vc_control;

typedef struct vc_control64 { 
        __u64 min;
        __u64 max;
        __u64 def;
} vc_control64;

typedef struct vc_gain {
        __u32 max;
        __u32 max_mdB;
        __u32 type;
        __u32 c0;
        __u32 c1;
} vc_gain;

typedef struct vc_frame {
        __u32 left;
        __u32 top;
        __u32 width;
        __u32 height;
} vc_frame;

typedef struct vc_csr2 {
        __u32 l;
        __u32 m;
} vc_csr2;

typedef struct vc_csr4 {
        __u32 l;
        __u32 m;
        __u32 h;
        __u32 u;
} vc_csr4;

struct vc_sen_csr {
        struct vc_csr2 mode;
        __u8 mode_standby;
        __u8 mode_operating;
        struct vc_csr4 vmax;
        struct vc_csr4 hmax;
        struct vc_csr4 shs;
        struct vc_csr2 again;
        struct vc_csr2 dgain;
        struct vc_csr2 blacklevel;
        struct vc_csr2 h_start;
        struct vc_csr2 v_start;
        struct vc_csr2 h_end;
        struct vc_csr2 v_end;
        struct vc_csr2 w_width;
        struct vc_csr2 w_height;
        struct vc_csr2 o_width;
        struct vc_csr2 o_height;
        struct vc_csr4 flash_duration;
        struct vc_csr4 flash_offset;
};

struct vc_csr {
        struct vc_sen_csr sen;
};

typedef struct vc_reg {
        __u16 address;
        __u8 value;
} vc_reg;

typedef struct vc_mode {
        __u8       num_lanes;
        __u8       format;
        __u8       binning;
        __u32      hmax;
        vc_control vmax;
        vc_control blacklevel;
        __u32      retrigger_min;
        struct vc_reg binning_mode_regs[MAX_BINNING_MODE_REGS];
} vc_mode;

typedef struct vc_binning {
        __u8 h_factor;
        __u8 v_factor;
        struct vc_reg regs[8];
} vc_binning;

#define BINNING_START(binning, h, v) \
        binning = (vc_binning) { .h_factor = h, .v_factor = v }; \
        { const struct vc_reg regs [] = {
#define BINNING_END(binning) \
        , {0, 0} }; memcpy(&binning.regs, regs, sizeof(regs)); }

typedef struct dt_binning_mode {
        __u32 binning_mode;
        bool  mode_set;
} dt_binning_mode;

struct vc_ctrl {
        // Communication
        int mod_i2c_addr;
        struct i2c_client *client_sen;
        struct i2c_client *client_mod;
        // Controls
        struct vc_mode mode[MAX_VC_MODES];
        struct vc_control exposure;
        struct vc_gain again;
        struct vc_gain dgain;
        struct vc_control framerate;
        // Modes & Frame Formats
        struct vc_frame frame;          // Pixel
        struct vc_binning binnings[8];
        __u8 max_binning_modes_used;
        
        // Control and status registers
        struct vc_csr csr;
        // Exposure
        __u32 clk_ext_trigger;          // Hz
        __u32 clk_pixel;                // Hz
        // Flash
        __u32 flash_factor;
        __s32 flash_toffset;
        // Special features
        __u32 flags;
        __u32 mbus_codes[MAX_MBUS_CODES];
        bool  packed_outputs;
};

struct vc_state {
        __u8 mode;
        __u32 vmax;
        __u32 shs;
        __u32 exposure;                 // µs
        __u32 gain;                     // mdB
        __u32 blacklevel;
        __u32 exposure_cnt;
        __u32 retrigger_cnt;
        __u32 framerate;
        __u32 format_code;
        struct vc_frame frame;          // Pixel
        __u8 num_lanes;
        __u8 io_mode;
        __u8 trigger_mode;
        __u8 binning_mode;
        __u8 former_binning_mode;
        int power_on;
        int streaming;
        __u8 flags;
#ifdef ENABLE_ADVANCED_CONTROL
        __s32 hmax_overwrite;
        __s32 vmax_overwrite;
        __s32 height_offset;
#endif
};

struct vc_cam {
        struct vc_desc desc;
        struct vc_ctrl ctrl;
        struct vc_state state;
};

// --- Helper functions to allow i2c communication for customization ----------
int vc_read_i2c_reg(struct i2c_client *client, const __u16 addr);
int vc_write_i2c_reg(struct i2c_client *client, const __u16 addr, const __u8 value);
struct i2c_client *vc_mod_get_client(struct device *dev, struct i2c_adapter *adapter, __u8 i2c_addr);

// --- Helper functions for internal data structures --------------------------
void vc_core_print_debug(struct vc_cam *cam);                                   // Only used by NVIDIA driver
struct device *vc_core_get_sen_device(struct vc_cam *cam);
vc_mode vc_core_get_mode(struct vc_cam *cam);
void vc_core_update_mbus_codes(struct vc_cam *cam);
int vc_core_set_format(struct vc_cam *cam, __u32 code);
__u32 vc_core_get_format(struct vc_cam *cam);
int vc_core_set_frame(struct vc_cam *cam, __u32 left, __u32 top, __u32 width, __u32 height);
struct vc_frame *vc_core_get_frame(struct vc_cam *cam);
int vc_core_set_num_lanes(struct vc_cam *cam, __u32 number);
__u32 vc_core_get_num_lanes(struct vc_cam *cam);
int vc_core_set_framerate(struct vc_cam *cam, __u32 framerate);
__u32 vc_core_get_framerate(struct vc_cam *cam);
__u32 vc_core_get_time_per_line_ns(struct vc_cam *cam);                         // Only used by NXP driver
int vc_core_set_binning_mode(struct vc_cam *cam, int mode);
__u64 vc_core_mdB_to_times(int mdB);                                            // Only used by NXP driver
int vc_core_times_to_mdB(__u64 times);                                          // Only used by NXP driver
int vc_core_live_roi(struct vc_cam *cam, __s32 data);                           // Only used by NXP driver
#ifdef ENABLE_ADVANCED_CONTROL
int vc_core_set_hmax_overwrite(struct vc_cam *cam, __s32 hmax_overwrite);       // Only used by NXP driver
int vc_core_set_vmax_overwrite(struct vc_cam *cam, __s32 vmax_overwrite);       // Only used by NXP driver
int vc_core_set_height_offset(struct vc_cam *cam, __s32 vmax_overwrite);        // Only used by NXP driver
#endif

// --- Function to initialize the vc core --------------------------------------
int vc_core_init(struct vc_cam *cam, struct i2c_client *client);
int vc_core_release(struct vc_cam *cam);
int vc_core_update_controls(struct vc_cam *cam);                                // Only used by NVIDIA driver

// --- Functions for the VC MIPI Controller Module ----------------------------
int vc_mod_set_mode(struct vc_cam *cam, int *reset);
int vc_mod_set_trigger_mode(struct vc_cam *cam, int mode);
int vc_mod_get_trigger_mode(struct vc_cam *cam);                                // Only used by NVIDIA driver
int vc_mod_set_single_trigger(struct vc_cam *cam);
int vc_mod_set_io_mode(struct vc_cam *cam, int mode);

// --- Functions for the VC MIPI Sensors --------------------------------------
int vc_sen_set_roi(struct vc_cam *cam);
struct v4l2_rect vc_sen_get_roi(struct vc_cam *cam);
int vc_sen_set_exposure(struct vc_cam *cam, int exposure);
int vc_sen_set_gain(struct vc_cam *cam, __u64 gain, bool unit_is_mdB);
int vc_sen_set_blacklevel(struct vc_cam *cam, __u32 blacklevel);
int vc_sen_start_stream(struct vc_cam *cam);
int vc_sen_stop_stream(struct vc_cam *cam);
int vc_sen_set_hmax(struct vc_cam *cam);
int vc_sen_write_vmax(struct vc_ctrl *ctrl, __u32 vmax);

#endif // _VC_MIPI_CORE_H