#ifndef _RASPISTILL_H
#define _RASPISTILL_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <errno.h>
#include <sysexits.h>

#include "bcm_host.h"
#include "interface/vcos/vcos.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"
#include "interface/mmal/mmal_parameters_camera.h"

#include "RaspiCommonSettings.h"
#include "RaspiCamControl.h"
#include "RaspiPreview.h"
#include "RaspiCLI.h"
#include "RaspiTex.h"
#include "RaspiHelpers.h"

#include "RaspiGPS.h"

#include <semaphore.h>
#include <math.h>
#include <pthread.h>
#include <time.h>

// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2

// Stills format information
// 0 implies variable
#define STILLS_FRAME_RATE_NUM 0
#define STILLS_FRAME_RATE_DEN 1

/// Video render needs at least 2 buffers.
#define VIDEO_OUTPUT_BUFFERS_NUM 3

#define MAX_USER_EXIF_TAGS      32
#define MAX_EXIF_PAYLOAD_LENGTH 128

/// Frame advance method
enum
{
   FRAME_NEXT_SINGLE,
   FRAME_NEXT_TIMELAPSE,
   FRAME_NEXT_KEYPRESS,
   FRAME_NEXT_FOREVER,
   FRAME_NEXT_GPIO,
   FRAME_NEXT_SIGNAL,
   FRAME_NEXT_IMMEDIATELY
};

/// Amount of time before first image taken to allow settling of
/// exposure etc. in milliseconds.
#define CAMERA_SETTLE_TIME       1000

/** Structure containing all state information for the current run
 */
typedef struct
{
   RASPICOMMONSETTINGS_PARAMETERS common_settings;     /// Common settings
   int timeout;                        /// Time taken before frame is grabbed and app then shuts down. Units are milliseconds
   int quality;                        /// JPEG quality setting (1-100)
   int wantRAW;                        /// Flag for whether the JPEG metadata also contains the RAW bayer image
   char *linkname;                     /// filename of output file
   int frameStart;                     /// First number of frame output counter
   MMAL_PARAM_THUMBNAIL_CONFIG_T thumbnailConfig;
   int demoMode;                       /// Run app in demo mode
   int demoInterval;                   /// Interval between camera settings changes
   MMAL_FOURCC_T encoding;             /// Encoding to use for the output file.
   const char *exifTags[MAX_USER_EXIF_TAGS]; /// Array of pointers to tags supplied from the command line
   int numExifTags;                    /// Number of supplied tags
   int enableExifTags;                 /// Enable/Disable EXIF tags in output
   int timelapse;                      /// Delay between each picture in timelapse mode. If 0, disable timelapse
   int fullResPreview;                 /// If set, the camera preview port runs at capture resolution. Reduces fps.
   int frameNextMethod;                /// Which method to use to advance to next frame
   int useGL;                          /// Render preview using OpenGL
   int glCapture;                      /// Save the GL frame-buffer instead of camera output
   int burstCaptureMode;               /// Enable burst mode
   int datetime;                       /// Use DateTime instead of frame#
   int timestamp;                      /// Use timestamp instead of frame#
   int restart_interval;               /// JPEG restart interval. 0 for none.

   RASPIPREVIEW_PARAMETERS preview_parameters;    /// Preview setup parameters
   RASPICAM_CAMERA_PARAMETERS camera_parameters; /// Camera setup parameters

   MMAL_COMPONENT_T *camera_component;    /// Pointer to the camera component
   MMAL_COMPONENT_T *encoder_component;   /// Pointer to the encoder component
   MMAL_COMPONENT_T *null_sink_component; /// Pointer to the null sink component
   MMAL_CONNECTION_T *preview_connection; /// Pointer to the connection from camera to preview
   MMAL_CONNECTION_T *encoder_connection; /// Pointer to the connection from camera to encoder

   MMAL_POOL_T *encoder_pool; /// Pointer to the pool of buffers used by encoder output port

   RASPITEX_STATE raspitex_state; /// GL renderer state and parameters

} RASPISTILL_STATE;

/** Struct used to pass information in encoder port userdata to callback
 */
typedef struct
{
   FILE *file_handle;                   /// File handle to write buffer data to.
   VCOS_SEMAPHORE_T complete_semaphore; /// semaphore which is posted when we reach end of frame (indicates end of capture or fault)
   RASPISTILL_STATE *pstate;            /// pointer to our state in case required in callback
} PORT_USERDATA;

static void store_exif_tag(RASPISTILL_STATE *state, const char *exif_tag);

/// Command ID's and Structure defining our command line options
enum
{
   CommandQuality,
   CommandRaw,
   CommandTimeout,
   CommandThumbnail,
   CommandDemoMode,
   CommandEncoding,
   CommandExifTag,
   CommandTimelapse,
   CommandFullResPreview,
   CommandLink,
   CommandKeypress,
   CommandSignal,
   CommandGL,
   CommandGLCapture,
   CommandBurstMode,
   CommandDateTime,
   CommandTimeStamp,
   CommandFrameStart,
   CommandRestartInterval,
};

static COMMAND_LIST cmdline_commands[] =
{
   { CommandQuality, "-quality",    "q",  "Set jpeg quality <0 to 100>", 1 },
   { CommandRaw,     "-raw",        "r",  "Add raw bayer data to jpeg metadata", 0 },
   { CommandLink,    "-latest",     "l",  "Link latest complete image to filename <filename>", 1},
   { CommandTimeout, "-timeout",    "t",  "Time (in ms) before takes picture and shuts down (if not specified, set to 5s)", 1 },
   { CommandThumbnail,"-thumb",     "th", "Set thumbnail parameters (x:y:quality) or none", 1},
   { CommandDemoMode,"-demo",       "d",  "Run a demo mode (cycle through range of camera options, no capture)", 0},
   { CommandEncoding,"-encoding",   "e",  "Encoding to use for output file (jpg, bmp, gif, png)", 1},
   { CommandExifTag, "-exif",       "x",  "EXIF tag to apply to captures (format as 'key=value') or none", 1},
   { CommandTimelapse,"-timelapse", "tl", "Timelapse mode. Takes a picture every <t>ms. %d == frame number (Try: -o img_%04d.jpg)", 1},
   { CommandFullResPreview,"-fullpreview","fp", "Run the preview using the still capture resolution (may reduce preview fps)", 0},
   { CommandKeypress,"-keypress",   "k",  "Wait between captures for a ENTER, X then ENTER to exit", 0},
   { CommandSignal,  "-signal",     "s",  "Wait between captures for a SIGUSR1 or SIGUSR2 from another process", 0},
   { CommandGL,      "-gl",         "g",  "Draw preview to texture instead of using video render component", 0},
   { CommandGLCapture, "-glcapture","gc", "Capture the GL frame-buffer instead of the camera image", 0},
   { CommandBurstMode, "-burst",    "bm", "Enable 'burst capture mode'", 0},
   { CommandDateTime,  "-datetime",  "dt", "Replace output pattern (%d) with DateTime (MonthDayHourMinSec)", 0},
   { CommandTimeStamp, "-timestamp", "ts", "Replace output pattern (%d) with unix timestamp (seconds since 1970)", 0},
   { CommandFrameStart,"-framestart","fs",  "Starting frame number in output pattern(%d)", 1},
   { CommandRestartInterval, "-restart","rs","JPEG Restart interval (default of 0 for none)", 1},
};

static int cmdline_commands_size = sizeof(cmdline_commands) / sizeof(cmdline_commands[0]);

static struct
{
   char *format;
   MMAL_FOURCC_T encoding;
} encoding_xref[] =
{
   {"jpg", MMAL_ENCODING_JPEG},
   {"bmp", MMAL_ENCODING_BMP},
   {"gif", MMAL_ENCODING_GIF},
   {"png", MMAL_ENCODING_PNG},
   {"ppm", MMAL_ENCODING_PPM},
   {"tga", MMAL_ENCODING_TGA}
};

static int encoding_xref_size = sizeof(encoding_xref) / sizeof(encoding_xref[0]);


static struct
{
   char *description;
   int nextFrameMethod;
} next_frame_description[] =
{
   {"Single capture",         FRAME_NEXT_SINGLE},
   {"Capture on timelapse",   FRAME_NEXT_TIMELAPSE},
   {"Capture on keypress",    FRAME_NEXT_KEYPRESS},
   {"Run forever",            FRAME_NEXT_FOREVER},
   {"Capture on GPIO",        FRAME_NEXT_GPIO},
   {"Capture on signal",      FRAME_NEXT_SIGNAL},
};

static int next_frame_description_size = sizeof(next_frame_description) / sizeof(next_frame_description[0]);


extern int cmdline_commands_size = sizeof(cmdline_commands) / sizeof(cmdline_commands[0]);
extern int encoding_xref_size = sizeof(encoding_xref) / sizeof(encoding_xref[0]);
extern int next_frame_description_size = sizeof(next_frame_description) / sizeof(next_frame_description[0]);
extern int parse_cmdline(int argc, const char **argv, RASPISTILL_STATE *state);
extern int wait_for_next_frame(RASPISTILL_STATE *state, int *frame);
extern MMAL_STATUS_T add_exif_tag(RASPISTILL_STATE *state, const char *exif_tag);
extern MMAL_STATUS_T create_camera_component(RASPISTILL_STATE *state);
extern MMAL_STATUS_T create_encoder_component(RASPISTILL_STATE *state);
extern void add_exif_tags(RASPISTILL_STATE *state, struct gps_data_t *gpsdata);
extern void application_help_message(char *app_name);
extern void default_status(RASPISTILL_STATE *state);
extern void destroy_camera_component(RASPISTILL_STATE *state);
extern void destroy_encoder_component(RASPISTILL_STATE *state);
extern void dump_status(RASPISTILL_STATE *state);
extern void encoder_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
extern void rename_file(RASPISTILL_STATE *state, FILE *output_file, const char *final_filename, const char *use_filename, int frame);
extern void store_exif_tag(RASPISTILL_STATE *state, const char *exif_tag);

#endif // _RASPISTILL_H
