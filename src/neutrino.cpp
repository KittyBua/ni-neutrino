/*
	Neutrino-GUI   -  DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
							 and some other guys

	Copyright (C) 2006-2017 Stefan Seyfried

	Copyright (C) 2011 CoolStream International Ltd

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define NEUTRINO_CPP

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/mount.h>
#include <dirent.h>

#include <fstream>

#include "global.h"
#include "neutrino.h"
#include "neutrino_menue.h"
#include "version_pseudo.h"

#include <daemonc/remotecontrol.h>

#include <driver/abstime.h>
#include <driver/fontrenderer.h>
#include <driver/framebuffer.h>
#include <driver/neutrinofonts.h>
#include <driver/rcinput.h>
#include <driver/shutdown_count.h>
#include <driver/record.h>
#include <driver/screenshot.h>
#include <driver/volume.h>
#include <driver/streamts.h>
#include <driver/display.h>
#include <driver/radiotext.h>
#include <driver/scanepg.h>

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
#include "gui/psisetup.h"
#endif
#include "gui/adzap.h"
#include "gui/audiomute.h"
#include "gui/audioplayer.h"
#include "gui/bouquetlist.h"
#include "gui/cam_menu.h"
#include "gui/cec_setup.h"
#include "gui/epgview.h"
#include "gui/eventlist.h"
#include "gui/favorites.h"
#include "gui/filebrowser.h"
#include "gui/followscreenings.h"
#ifdef ENABLE_GRAPHLCD
#include "gui/glcdthemes.h"
#endif
#include "gui/hdd_menu.h"
#include "gui/infoviewer.h"
#include "gui/mediaplayer.h"
#include "gui/movieplayer.h"
#include "gui/osd_helpers.h"
#include "gui/osd_setup.h"
#include "gui/osdlang_setup.h"
#include "gui/pictureviewer.h"
#include "gui/plugins.h"
#include "gui/rc_lock.h"
#include "gui/scan_setup.h"
#include "gui/screensaver.h"
#include "gui/sleeptimer.h"
#include "gui/start_wizard.h"
#include "gui/update_ext.h"
#include "gui/update.h"
#include "gui/update_check.h"
#if ENABLE_PKG_MANAGEMENT
#include "gui/update_check_packages.h"
#endif
#include "gui/videosettings.h"
#include "gui/audio_select.h"
#include "gui/weather.h"
#include "gui/weather_setup.h"
#include "gui/webchannels_setup.h"

#include "gui/widget/hintbox.h"
#include "gui/widget/icons.h"
#include "gui/widget/menue.h"
#include "gui/widget/msgbox.h"
#include "gui/infoclock.h"
#include "gui/timeosd.h"
#include "gui/parentallock_setup.h"
#if ENABLE_PIP
#include "gui/pipsetup.h"
#endif
#if ENABLE_QUADPIP
#include <gui/quadpip_setup.h>
#endif
#include "gui/themes.h"
#include "gui/timerlist.h"
#include "gui/components/cc_item_progressbar.h"

#include <hardware/audio.h>
#include <hardware/ca.h>
#include <hardware/video.h>
#include <cs_api.h>
#include <pwrmngr.h>

#include <system/debug.h>
#include <system/fsmounter.h>
#include <system/hddstat.h>
#include <system/setting_helpers.h>
#include <system/settings.h>
#include <system/helpers.h>
#include <system/proc_tools.h>
#include <system/sysload.h>

#include <timerdclient/timerdclient.h>
#include <timerd/timermanager.h>

#include <zapit/debug.h>
#include <zapit/zapit.h>
#include <zapit/getservices.h>
#include <zapit/satconfig.h>
#include <zapit/scan.h>
#include <zapit/capmt.h>
#include <zapit/client/zapitclient.h>

#include <linux/reboot.h>
#include <sys/reboot.h>

#ifdef __sh__
/* the sh4 gcc seems to dislike someting about openthreads... */
#define exit _exit
#endif

#include <compatibility.h>

#include <lib/libdvbsub/dvbsub.h>
#include <lib/libtuxtxt/teletext.h>
#include <eitd/sectionsd.h>
#ifdef ENABLE_LUA
#include <system/luaserver.h>
#endif
int old_b_id = -1;

CInfoClock      *InfoClock;
CTimeOSD	*FileTimeOSD;

//NI InfoIcons
#include "gui/infoicons.h"
CInfoIcons *InfoIcons;

#ifdef ENABLE_LCD4LINUX
#include "driver/lcd4l.h"
#endif

Zapit_config zapitCfg;
char zapit_lat[21]="#";
char zapit_long[21]="#";
bool autoshift = false;
uint32_t scrambled_timer;
uint32_t fst_timer;
t_channel_id standby_channel_id = 0;

//NEW
static pthread_t timer_thread;
static bool timerd_thread_started = false;
extern void *timerd_main_thread(void *data);
extern void *nhttpd_main_thread(void *data);

//#define DISABLE_SECTIONSD

extern cVideo *videoDecoder;
#if ENABLE_PIP
extern cVideo *pipVideoDecoder[3];
extern cDemux *pipVideoDemux[3];
#endif
extern cDemux *videoDemux;
extern cAudio *audioDecoder;
cPowerManager *powerManager;
cCpuFreqManager *cpuFreq;

void stop_daemons(bool stopall = true, bool for_flash = false);
void stop_video(void);

CAudioSetupNotifier	* audioSetupNotifier;
CBouquetList   * bouquetList; // current list

CBouquetList   * TVbouquetList;
CBouquetList   * TVsatList;
CBouquetList   * TVfavList;
CBouquetList   * TVallList;
CBouquetList   * TVwebList;

CBouquetList   * RADIObouquetList;
CBouquetList   * RADIOsatList;
CBouquetList   * RADIOfavList;
CBouquetList   * RADIOallList;
CBouquetList   * RADIOwebList;

CBouquetList   * AllFavBouquetList;

CPlugins       * g_Plugins;
CRemoteControl * g_RemoteControl;
CPictureViewer * g_PicViewer;
CCAMMenuHandler * g_CamHandler;
CVolume        * g_volume;
CAudioMute     * g_audioMute;
CNeutrinoFonts * neutrinoFonts = NULL;

// Globale Variablen - to use import global.h

// I don't like globals, I would have hidden them in classes,
// but if you wanna do it so... ;)
bool parentallocked = false;

static char **global_argv;

extern const char * locale_real_names[]; /* #include <system/locals_intern.h> */

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
+          CNeutrinoApp - Constructor, initialize g_fontRenderer                      +
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
CNeutrinoApp::CNeutrinoApp()
: configfile('\t')
{
	standby_pressed_at.tv_sec = 0;
#if HAVE_LIBSTB_HAL
	cCA *CA = NULL;
	if (!CA)
	{
		printf("[neutrino] create CA Instance\n");
		CA = cCA::GetInstance();
	}

	/* this needs to happen before the framebuffer is set up */
	hal_api_init();
#endif
	osd_resolution_tmp        = -1;
	frameBufferInitialized    = false;

	frameBuffer = CFrameBuffer::getInstance();
	frameBuffer->setIconBasePath(ICONSDIR);
	SetupFrameBuffer();

	mode 			= NeutrinoModes::mode_unknown;
	lastMode		= NeutrinoModes::mode_unknown;
	channelList		= NULL;
	TVchannelList		= NULL;
	RADIOchannelList	= NULL;
	skipShutdownTimer	= false;
	skipSleepTimer		= false;
	lockStandbyCall         = false;
	current_muted		= 0;
	recordingstatus		= 0;
	channels_changed	= false;
	favorites_changed	= false;
	bouquets_changed	= false;
	channels_init		= false;
	channelList_allowed	= true;
	channelList_painted	= false;
	blank_screen		= false; //NI

#if ENABLE_PIP
#if !HAVE_CST_HARDWARE && !HAVE_GENERIC_HARDWARE
	avinput_pip = false;
#endif
#endif

	// remove standby flag
	if (access("/tmp/.standby", F_OK) == 0)
		unlink("/tmp/.standby");
}

/*-------------------------------------------------------------------------------------
-           CNeutrinoApp - Destructor                                                 -
-------------------------------------------------------------------------------------*/
CNeutrinoApp::~CNeutrinoApp()
{
	if (channelList)
		delete channelList;
	if (neutrinoFonts)
		delete neutrinoFonts;
	neutrinoFonts = NULL;
}

CNeutrinoApp* CNeutrinoApp::getInstance()
{
	static CNeutrinoApp* neutrinoApp = NULL;

	if(!neutrinoApp) {
		neutrinoApp = new CNeutrinoApp();
		dprintf(DEBUG_DEBUG, "NeutrinoApp Instance created\n");
	}
	return neutrinoApp;
}

typedef struct lcd_setting_t
{
	const char * const name;
	const unsigned int default_value;
} lcd_setting_struct_t;

const lcd_setting_struct_t lcd_setting[SNeutrinoSettings::LCD_SETTING_COUNT] =
{
	{"lcd_brightness"       , DEFAULT_VFD_BRIGHTNESS       },
	{"lcd_standbybrightness", DEFAULT_VFD_STANDBYBRIGHTNESS},
	{"lcd_contrast"         , DEFAULT_LCD_CONTRAST         },
	{"lcd_power"            , DEFAULT_LCD_POWER            },
	{"lcd_inverse"          , DEFAULT_LCD_INVERSE          },
	{"lcd_show_volume"      , DEFAULT_LCD_SHOW_VOLUME      },
	{"lcd_autodimm"         , DEFAULT_LCD_AUTODIMM         },
	{"lcd_deepbrightness"   , DEFAULT_VFD_STANDBYBRIGHTNESS }
#if HAVE_LIBSTB_HAL
	,{ "lcd_epgmode"        , 0 /*DEFAULT_LCD_EPGMODE*/ }
#endif
};

static SNeutrinoSettings::usermenu_t usermenu_default[] = {
	{ CRCInput::RC_red,             "2,3,4,13,1,23,22",                     "",     "red"           },
	{ CRCInput::RC_green,           "6",                                    "",     "green"         },
	{ CRCInput::RC_yellow,          "7,35,freeze",                          "",     "yellow"        },
	{ CRCInput::RC_blue,            "27,28,21,20,1,39,10,11,24,19,14",      "",     "blue"          },
#if BOXMODEL_HD51 || BOXMODEL_BRE2ZE4K || BOXMODEL_H7 || BOXMODEL_PROTEK4K || BOXMODEL_HD60 || BOXMODEL_HD61 || BOXMODEL_MULTIBOX || BOXMODEL_MULTIBOXSE
	{ CRCInput::RC_playpause,       "9",                                    "",     "5"             },
#else
	{ CRCInput::RC_play,            "9",                                    "",     "5"             },
#endif
	{ CRCInput::RC_audio,           "6",                                    "",     "6"             },
	{ CRCInput::RC_nokey,           "",                                     "",     ""              },
};

/**************************************************************************************
*          CNeutrinoApp -  loadSetup, load the application-settings                   *
**************************************************************************************/

std::string font_file_monospace = "";

int CNeutrinoApp::loadSetup(const char *fname)
{
	char cfg_key[81];
	char cfg_value[20]; // mode_icons_flag
	int erg = 0;

	// execute migration script before loading configfile
	std::string migration_sh = find_executable("migration.sh");
	if (!migration_sh.empty())
		my_system(migration_sh.c_str());

	configfile.clear();
	// load settings; setup defaults
	if (!configfile.loadConfig(fname))
		erg = 1; // file doesn't exist
	else
		migrateConfig(fname);

	parentallocked = !access(NEUTRINO_PARENTALLOCKED_FILE, R_OK);

	// theme/color options
	g_settings.theme_name = configfile.getString("theme_name", !access(NEUTRINO_SETTINGS_FILE, F_OK) ? MIGRATE_THEME_NAME : "");
	CThemes::getInstance()->getTheme(configfile);

#ifdef ENABLE_GRAPHLCD
	g_settings.glcd_theme_name = configfile.getString("glcd_theme_name", !access(NEUTRINO_SETTINGS_FILE, F_OK) ? MIGRATE_THEME_OLED_NAME : "");
	CGLCDThemes::getInstance()->getTheme(configfile);

	g_settings.glcd_enable = configfile.getInt32("glcd_enable", g_info.hw_caps->display_type == HW_DISPLAY_GFX);
	g_settings.glcd_logodir = configfile.getString("glcd_logodir", TARGET_ROOT "/media/sda1/logos");

	g_settings.glcd_brightness = configfile.getInt32("glcd_brightness", GLCD_DEFAULT_BRIGHTNESS);
	g_settings.glcd_brightness_dim = configfile.getInt32("glcd_brightness_dim", GLCD_DEFAULT_BRIGHTNESS_DIM);
	g_settings.glcd_brightness_dim_time = configfile.getString("glcd_brightness_dim_time", GLCD_DEFAULT_BRIGHTNESS_DIM_TIME);
	g_settings.glcd_brightness_standby = configfile.getInt32("glcd_brightness_standby", GLCD_DEFAULT_BRIGHTNESS_STANDBY);
	g_settings.glcd_mirror_osd = configfile.getInt32("glcd_mirror_osd", 0);
	g_settings.glcd_mirror_video = configfile.getInt32("glcd_mirror_video", 0);
	g_settings.glcd_scroll = configfile.getInt32("glcd_scroll", 1);
#if BOXMODEL_VUUNO4KSE
	g_settings.glcd_scroll_speed = configfile.getInt32("glcd_scroll_speed", 1);
#elif BOXMODEL_VUSOLO4K || BOXMODEL_VUDUO4K || BOXMODEL_VUDUO4KSE || BOXMODEL_VUULTIMO4K
	g_settings.glcd_scroll_speed = configfile.getInt32("glcd_scroll_speed", 2);
#else
	g_settings.glcd_scroll_speed = configfile.getInt32("glcd_scroll_speed", 5);
#endif
	g_settings.glcd_selected_config = configfile.getInt32("glcd_selected_config", 0);
#endif

#ifdef ENABLE_LCD4LINUX
	g_settings.lcd4l_support = configfile.getInt32("lcd4l_support", 0);
	g_settings.lcd4l_logodir = configfile.getString("lcd4l_logodir", TARGET_ROOT "/media/sda1/logos");
	g_settings.lcd4l_display_type = configfile.getInt32("lcd4l_display_type", CLCD4l::DPF320x240);
	g_settings.lcd4l_skin = configfile.getInt32("lcd4l_skin", 0);
	g_settings.lcd4l_skin_radio = configfile.getInt32("lcd4l_skin_radio", 0);
	g_settings.lcd4l_brightness = configfile.getInt32("lcd4l_brightness", 7);
	g_settings.lcd4l_brightness_standby = configfile.getInt32("lcd4l_brightness_standby", 3);
	g_settings.lcd4l_convert = configfile.getInt32("lcd4l_convert", 1);
	g_settings.lcd4l_screenshots = configfile.getInt32("lcd4l_screenshots", 0);
#endif

	g_settings.mode_icons = configfile.getInt32("mode_icons", 0);
	g_settings.mode_icons_background = configfile.getInt32("mode_icons_background", 0);
	g_settings.mode_icons_skin = configfile.getInt32("mode_icons_skin", 0);
	for (int i = 0 ; i < MODE_ICONS_NR_OF_ENTRIES ; i++)
	{
		sprintf(cfg_key, "mode_icons_flag%d", i);
		if (i == 0)
			sprintf(cfg_value, "/tmp/tuxmail.new");
		else if	(i == 1)
			sprintf(cfg_value, "/var/etc/.call");
		else if	(i == 2)
			sprintf(cfg_value, "/var/etc/.srv");
		else if	(i == 3)
			sprintf(cfg_value, "/var/etc/.card");
		else if	(i == 4)
			sprintf(cfg_value, "/var/etc/.update");
		else
			strcpy(cfg_value, "");
		g_settings.mode_icons_flag[i] = configfile.getString(cfg_key, cfg_value);
	}

	g_settings.show_ecm = configfile.getInt32("show_ecm", 0);
	g_settings.show_ecm_pos = configfile.getInt32("show_ecm_pos", 0);

	// video
	int video_Mode_default = VIDEO_STD_720P50;
#if HAVE_ARM_HARDWARE
	video_Mode_default = VIDEO_STD_1080P50;
#elif HAVE_CST_HARDWARE && defined(BOXMODEL_CST_HD2)
	video_Mode_default = VIDEO_STD_1080P24;
#endif
	if (getenv("NEUTRINO_DEFAULT_SCART") != NULL)
		video_Mode_default = VIDEO_STD_PAL;

	g_settings.video_Mode = configfile.getInt32("video_Mode", video_Mode_default);
#ifdef ANALOG_MODE
	g_settings.analog_mode1 = configfile.getInt32("analog_mode1", (int)ANALOG_MODE(BOTH, SD, RGB)); // default RGB
	g_settings.analog_mode2 = configfile.getInt32("analog_mode2", (int)ANALOG_MODE(CINCH, SD, YPRPB)); // default YPBPR
#else
	g_settings.analog_mode1 = configfile.getInt32("analog_mode1", (int)ANALOG_SD_RGB_SCART); // default RGB
	g_settings.analog_mode2 = configfile.getInt32("analog_mode2", (int)ANALOG_SD_YPRPB_CINCH); // default YPBPR
#endif
	g_settings.video_Format = configfile.getInt32("video_Format", DISPLAY_AR_16_9);
	g_settings.video_43mode = configfile.getInt32("video_43mode", DISPLAY_AR_MODE_LETTERBOX);

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	g_settings.psi_brightness = configfile.getInt32("video_psi_brightness", 128);
	g_settings.psi_contrast = configfile.getInt32("video_psi_contrast", 128);
	g_settings.psi_saturation = configfile.getInt32("video_psi_saturation", 128);
	g_settings.psi_step = configfile.getInt32("video_psi_step", 2);
	g_settings.psi_tint = configfile.getInt32("video_psi_tint", 128);
#endif

	// hdmi cec
	g_settings.hdmi_cec_mode = configfile.getInt32("hdmi_cec_mode", 0);
	g_settings.hdmi_cec_view_on = configfile.getInt32("hdmi_cec_view_on", 0);
	g_settings.hdmi_cec_standby = configfile.getInt32("hdmi_cec_standby", 0);
	g_settings.hdmi_cec_volume = configfile.getInt32("hdmi_cec_volume", 0);

	// volume
	g_settings.current_volume = configfile.getInt32("current_volume", 75);
	g_settings.current_volume_step = configfile.getInt32("current_volume_step", 5);
	g_settings.start_volume = configfile.getInt32("start_volume", -1);
	if (g_settings.start_volume >= 0)
		g_settings.current_volume = g_settings.start_volume;
	if (g_settings.hdmi_cec_volume)
		g_settings.current_volume = 100;
	g_settings.audio_volume_percent_ac3 = configfile.getInt32("audio_volume_percent_ac3", 100);
	g_settings.audio_volume_percent_pcm = configfile.getInt32("audio_volume_percent_pcm", 100);

	g_settings.channel_mode = configfile.getInt32("channel_mode", LIST_MODE_FAV);
	g_settings.channel_mode_radio = configfile.getInt32("channel_mode_radio", LIST_MODE_FAV);
	g_settings.channel_mode_initial = configfile.getInt32("channel_mode_initial", LIST_MODE_FAV);
	g_settings.channel_mode_initial_radio = configfile.getInt32("channel_mode_initial_radio", LIST_MODE_FAV);
	if (g_settings.channel_mode_initial > -1)
		g_settings.channel_mode = g_settings.channel_mode_initial;
	if (g_settings.channel_mode_initial_radio > -1)
		g_settings.channel_mode_radio = g_settings.channel_mode_initial_radio;

	g_settings.fan_speed = configfile.getInt32("fan_speed", 1);
	if (g_settings.fan_speed < 1)
		g_settings.fan_speed = 1;

	g_settings.srs_enable = configfile.getInt32("srs_enable", 0);
	g_settings.srs_algo = configfile.getInt32("srs_algo", 1);
	g_settings.srs_ref_volume = configfile.getInt32("srs_ref_volume", 75);
	g_settings.srs_nmgr_enable = configfile.getInt32("srs_nmgr_enable", 0);
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	g_settings.ac3_pass = configfile.getInt32("ac3_pass", 0);
	g_settings.dts_pass = configfile.getInt32("dts_pass", 0);
#else
	g_settings.hdmi_dd = configfile.getInt32("hdmi_dd", 0);
	g_settings.spdif_dd = configfile.getInt32("spdif_dd", 1);
#endif // HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	g_settings.analog_out = configfile.getInt32("analog_out", 1);

	g_settings.avsync = configfile.getInt32("avsync", 1);
	g_settings.video_dbdr = configfile.getInt32("video_dbdr", 0);

	for (int i = 0; i < VIDEOMENU_VIDEOMODE_OPTION_COUNT; i++)
	{
		sprintf(cfg_key, "enabled_video_mode_%d", i);
		g_settings.enabled_video_modes[i] = configfile.getInt32(cfg_key, 0);
	}

	if (erg)
	{
		/*
		   enable some defaults for g_settings.key_switchformat
		*/
		g_settings.enabled_video_modes[5] = 1; // VIDEO_STD_720P50
		g_settings.enabled_video_modes[7] = 1; // VIDEO_STD_1080I50
#if HAVE_CST_HARDWARE && defined(BOXMODEL_CST_HD2)
		g_settings.enabled_video_modes[10] = 1; // VIDEO_STD_1080P24
#elif HAVE_ARM_HARDWARE
		g_settings.enabled_video_modes[13] = 1; // VIDEO_STD_1080P50
		g_settings.enabled_video_modes[18] = 1; // VIDEO_STD_2160P50
#endif
	}

	for (int i = 0; i < VIDEOMENU_VIDEOMODE_OPTION_COUNT; i++)
	{
		sprintf(cfg_key, "enabled_auto_mode_%d", i);
		g_settings.enabled_auto_modes[i] = configfile.getInt32(cfg_key, 1);
	}

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	g_settings.zappingmode = configfile.getInt32("zappingmode", (strcmp(g_info.hw_caps->boxmodel, "e4hdultra") == 0) ? 2 : 0);
	g_settings.hdmi_colorimetry = configfile.getInt32("hdmi_colorimetry", 0);
#endif

	g_settings.cpufreq = g_info.hw_caps->can_cpufreq ? configfile.getInt32("cpufreq", 0) : 0;
	g_settings.standby_cpufreq = g_info.hw_caps->can_cpufreq ? configfile.getInt32("standby_cpufreq", 100) : 50;

	// ci-settings
	g_settings.ci_standby_reset = configfile.getInt32("ci_standby_reset", 0);
	g_settings.ci_check_live = configfile.getInt32("ci_check_live", 0);
	g_settings.ci_tuner = configfile.getInt32("ci_tuner", -1);
	g_settings.ci_rec_zapto = configfile.getInt32("ci_rec_zapto", 0);
	g_settings.ci_mode = configfile.getInt32("ci_mode", 0);
#if BOXMODEL_VUPLUS_ALL
	g_settings.ci_delay = configfile.getInt32("ci_delay", 128);
#endif
	// ci-settings for each slot
	unsigned int ci_slots = cCA::GetInstance()->GetNumberCISlots();
	if (strcmp(g_info.hw_caps->boxvendor, "Coolstream") == 0)
	{
		/*
		   CST hardware isn't initialized here,
		   so we assume one ci-slot (two for HD1 BSE).
		*/
		ci_slots = (strcmp(g_info.hw_caps->boxname, "HD1") == 0) ? 2 : 1;
	}
	for (unsigned int i = 0; i < ci_slots; i++)
	{
		sprintf(cfg_key, "ci_ignore_messages_%d", i);
		g_settings.ci_ignore_messages[i] = configfile.getInt32(cfg_key, 0);
		sprintf(cfg_key, "ci_save_pincode_%d", i);
		g_settings.ci_save_pincode[i] = configfile.getInt32(cfg_key, 0);
		sprintf(cfg_key, "ci_pincode_%d", i);
		g_settings.ci_pincode[i] = configfile.getString(cfg_key, "");
		sprintf(cfg_key, "ci_op_%d", i);
		g_settings.ci_op[i] = configfile.getInt32(cfg_key, 0);
		sprintf(cfg_key, "ci_clock_%d", i);
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
		g_settings.ci_clock[i] = configfile.getInt32(cfg_key, 6);
#else
		g_settings.ci_clock[i] = configfile.getInt32(cfg_key, 9);
#endif
#if BOXMODEL_VUPLUS_ALL
		sprintf(cfg_key, "ci_rpr_%d", i);
		g_settings.ci_rpr[i] = configfile.getInt32(cfg_key, 9);
#endif
	}

	g_settings.make_hd_list = configfile.getInt32("make_hd_list", 0);
	g_settings.make_new_list = configfile.getInt32("make_new_list", 1);
	g_settings.make_removed_list = configfile.getInt32("make_removed_list", 1);
	g_settings.make_webradio_list = configfile.getInt32("make_webradio_list", 1);
	g_settings.make_webtv_list = configfile.getInt32("make_webtv_list", 1);

	g_settings.keep_channel_numbers = configfile.getInt32("keep_channel_numbers", 0);
	g_settings.show_empty_favorites = configfile.getInt32("show_empty_favorites", 0);

	// lcd/led
	for (int i = 0; i < SNeutrinoSettings::LCD_SETTING_COUNT; i++)
		g_settings.lcd_setting[i] = configfile.getInt32(lcd_setting[i].name, lcd_setting[i].default_value);
	g_settings.lcd_setting_dim_time = configfile.getString("lcd_dim_time", "0");
	g_settings.lcd_setting_dim_brightness = configfile.getInt32("lcd_dim_brightness", 0);
	g_settings.lcd_info_line = configfile.getInt32("lcd_info_line", 0); //channel name or clock
	g_settings.lcd_scroll = configfile.getInt32("lcd_scroll", 1);
	g_settings.lcd_notify_rclock = configfile.getInt32("lcd_notify_rclock", 1);

	g_settings.backlight_tv = configfile.getInt32("backlight_tv", 1);
	g_settings.backlight_standby = configfile.getInt32("backlight_standby", 0);
	g_settings.backlight_deepstandby = configfile.getInt32("backlight_deepstandby", 0);

	g_settings.led_tv_mode = configfile.getInt32("led_tv_mode", 2);
	g_settings.led_standby_mode = configfile.getInt32("led_standby_mode", 3);
	g_settings.led_deep_mode = configfile.getInt32("led_deep_mode", 3);
	g_settings.led_rec_mode = configfile.getInt32("led_rec_mode", 1);
	g_settings.led_blink = configfile.getInt32("led_blink", 1);

#ifdef BOXMODEL_CST_HD2
	g_settings.brightness = configfile.getInt32("brightness", 0);
	g_settings.contrast = configfile.getInt32("contrast", 0);
	g_settings.saturation = configfile.getInt32("saturation", 0);
#endif

	// hdd
	g_settings.hdd_fs = configfile.getInt32("hdd_fs", 0);
	g_settings.hdd_sleep = configfile.getInt32("hdd_sleep", 60);
	g_settings.hdd_noise = configfile.getInt32("hdd_noise", 254);
	g_settings.hdd_statfs_mode = configfile.getInt32("hdd_statfs_mode", SNeutrinoSettings::HDD_STATFS_RECORDING);
	g_settings.hdd_format_on_mount_failed = configfile.getInt32("hdd_format_on_mount_failed", 0);
	g_settings.hdd_wakeup = configfile.getInt32("hdd_wakeup", 1);
	g_settings.hdd_wakeup_msg = configfile.getInt32("hdd_wakeup_msg", 1);
	g_settings.hdd_allow_set_recdir = configfile.getInt32("hdd_allow_set_recdir", 1);

	// timer
	g_settings.timer_remotebox_ip.clear();
	int timer_remotebox_itemcount = configfile.getInt32("timer_remotebox_ip_count", 0);
	if (timer_remotebox_itemcount)
	{
		for (int i = 0; i < timer_remotebox_itemcount; i++)
		{
			timer_remotebox_item timer_rb;
			timer_rb.online = false;
			timer_rb.enabled = configfile.getBool("timer_remotebox_enabled_" + to_string(i), true);
			timer_rb.port = 0;
			std::string k;
			k = "timer_remotebox_ip_" + to_string(i);
			timer_rb.rbaddress = configfile.getString(k, "");
			if (timer_rb.rbaddress.empty())
				continue;
			k = "timer_remotebox_rbname_" + to_string(i);
			timer_rb.rbname = configfile.getString(k, "");
			if (timer_rb.rbname.empty())
				timer_rb.rbname = timer_rb.rbaddress;
			k = "timer_remotebox_user_" + to_string(i);
			timer_rb.user = configfile.getString(k, "");
			k = "timer_remotebox_pass_" + to_string(i);
			timer_rb.pass = configfile.getString(k, "");
			k = "timer_remotebox_port_" + to_string(i);
			timer_rb.port = configfile.getInt32(k, 80);

			g_settings.timer_remotebox_ip.push_back(timer_rb);
		}
	}
	g_settings.timer_followscreenings = configfile.getInt32("timer_followscreenings", CFollowScreenings::FOLLOWSCREENINGS_ON);

	// misc
	g_settings.zap_cycle = configfile.getInt32("zap_cycle", 1);
	g_settings.remote_control_hardware = configfile.getInt32("remote_control_hardware", CRCInput::RC_HW_COOLSTREAM);
	g_settings.enable_sdt = configfile.getInt32("enable_sdt", 1);
	g_settings.radiotext_enable = configfile.getBool("radiotext_enable", true);
	g_settings.cacheTXT = configfile.getInt32("cacheTXT", 1);
	g_settings.zapto_pre_time = configfile.getInt32("zapto_pre_time", 0);
	g_settings.minimode = configfile.getInt32("minimode", 0);

	g_settings.filesystem_is_utf8 = configfile.getBool("filesystem_is_utf8", true);

	// shutdown
	g_settings.shutdown_count = configfile.getInt32("shutdown_count", 0);
	if (g_info.hw_caps->can_shutdown)
	{
		g_settings.shutdown_min = configfile.getInt32("shutdown_min", 0);
		g_settings.shutdown_real = configfile.getBool("shutdown_real", false);
	}
	else
	{
		g_settings.shutdown_min = 0;
		g_settings.shutdown_real = false;
	}
	g_settings.shutdown_real_rcdelay = configfile.getBool("shutdown_real_rcdelay", false);
	g_settings.sleeptimer_min = configfile.getInt32("sleeptimer_min", 0);
	g_settings.power_standby = configfile.getInt32("power_standby", 0);

	// screen saver
	g_settings.screensaver_delay = configfile.getInt32("screensaver_delay", 1);
	g_settings.screensaver_dir = configfile.getString("screensaver_dir", ICONSDIR "/screensaver");
	g_settings.screensaver_mode = configfile.getInt32("screensaver_mode", CScreenSaver::SCR_MODE_CLOCK);
	g_settings.screensaver_mode_text = configfile.getInt32("screensaver_mode_text", CScreenSaver::SCR_MODE_TEXT_OFF);
	g_settings.screensaver_random = configfile.getInt32("screensaver_random", 0);
	g_settings.screensaver_timeout = configfile.getInt32("screensaver_timeout", 10);

	// vcr
	g_settings.vcr_AutoSwitch = configfile.getBool("vcr_AutoSwitch", true);

	// audio
	g_settings.audio_AnalogMode = configfile.getInt32("audio_AnalogMode", 0);
	g_settings.audio_DolbyDigital = configfile.getBool("audio_DolbyDigital", true);

	g_settings.audiochannel_up_down_enable = configfile.getBool("audiochannel_up_down_enable", false);

	// language
	g_settings.auto_lang = configfile.getInt32("auto_lang", 1);
	g_settings.auto_subs = configfile.getInt32("auto_subs", 0);

	for (int i = 0; i < 3; i++)
	{
		std::string _lang = "none";
		switch (i)
		{
			case 0:
				_lang = "German" ;
				break;
			case 1:
				_lang = "English";
				break;
			case 2:
				_lang = "French" ;
				break;
		}
		sprintf(cfg_key, "pref_lang_%d", i);
		g_settings.pref_lang[i] = configfile.getString(cfg_key, _lang);
		sprintf(cfg_key, "pref_subs_%d", i);
		g_settings.pref_subs[i] = configfile.getString(cfg_key, _lang);
	}
	g_settings.subs_charset = configfile.getString("subs_charset", "CP1252");

	g_settings.language = configfile.getString("language", "");
	g_settings.timezone = configfile.getString("timezone", "(GMT+01:00) Amsterdam, Berlin, Bern, Rome, Vienna");

	// epg
	g_settings.epg_dir = configfile.getString("epg_dir", TARGET_ROOT "/media/sda1/epg");
	g_settings.epg_cache = configfile.getInt32("epg_cache_time", 7);
	g_settings.epg_extendedcache = configfile.getInt32("epg_extendedcache_time", 168);
	g_settings.epg_max_events = configfile.getInt32("epg_max_events", 30000);
	g_settings.epg_old_events = configfile.getInt32("epg_old_events", 1);
	g_settings.epg_read = configfile.getBool("epg_read", true);
	g_settings.epg_read_frequently = configfile.getInt32("epg_read_frequently", 1);
	g_settings.epg_save = configfile.getBool("epg_save", false);
	g_settings.epg_save_frequently = configfile.getInt32("epg_save_frequently", 0);
	g_settings.epg_save_mode = configfile.getInt32("epg_save_mode", 0);
	g_settings.epg_save_standby = configfile.getBool("epg_save_standby", true);
	g_settings.epg_scan = configfile.getInt32("epg_scan", CEpgScan::SCAN_FAV);
	g_settings.epg_scan_mode = configfile.getInt32("epg_scan_mode", CEpgScan::MODE_STANDBY);
	// backward-compatible check
	if (g_settings.epg_scan == 0)
	{
		g_settings.epg_scan = CEpgScan::SCAN_CURRENT;
		g_settings.epg_scan_mode = CEpgScan::MODE_OFF;
	}
	g_settings.epg_scan_rescan = configfile.getInt32("epg_scan_rescan", 24);

	// epg search
	g_settings.epg_search_history_max = configfile.getInt32("epg_search_history_max", 10);
	g_settings.epg_search_history_size = configfile.getInt32("epg_search_history_size", 0);
	if (g_settings.epg_search_history_size > g_settings.epg_search_history_max)
		g_settings.epg_search_history_size = g_settings.epg_search_history_max;
	g_settings.epg_search_history.clear();
	for (int i = 0; i < g_settings.epg_search_history_size; i++)
	{
		std::string s = configfile.getString("epg_search_history_" + to_string(i));
		if (!s.empty())
			g_settings.epg_search_history.push_back(configfile.getString("epg_search_history_" + to_string(i), ""));
	}
	g_settings.epg_search_history_size = g_settings.epg_search_history.size();

	// network
	g_settings.ifname = configfile.getString("ifname", "eth0");

	for (int i = 0 ; i < NETWORK_NFS_NR_OF_ENTRIES ; i++)
	{
		std::string i_str(to_string(i));
		g_settings.network_nfs[i].ip = configfile.getString("network_nfs_ip_" + i_str, "");
		g_settings.network_nfs[i].mac = configfile.getString("network_nfs_mac_" + i_str, "11:22:33:44:55:66");
		g_settings.network_nfs[i].local_dir = configfile.getString("network_nfs_local_dir_" + i_str, "");
		if (g_settings.network_nfs[i].local_dir.empty())
			g_settings.network_nfs[i].local_dir = "/mnt/mnt" + i_str;
		g_settings.network_nfs[i].dir = configfile.getString("network_nfs_dir_" + i_str, "");
		g_settings.network_nfs[i].automount = configfile.getInt32("network_nfs_automount_" + i_str, 0);
		g_settings.network_nfs[i].mount_options1 = configfile.getString("network_nfs_mount_options1_" + i_str, "soft");
		g_settings.network_nfs[i].mount_options2 = configfile.getString("network_nfs_mount_options2_" + i_str, "nolock");
		g_settings.network_nfs[i].type = configfile.getInt32("network_nfs_type_" + i_str, 0);
		g_settings.network_nfs[i].username = configfile.getString("network_nfs_username_" + i_str, "");
		g_settings.network_nfs[i].password = configfile.getString("network_nfs_password_" + i_str, "");
	}

	g_settings.network_nfs_audioplayerdir = configfile.getString("network_nfs_audioplayerdir", TARGET_ROOT "/media/sda1/music");
	g_settings.network_nfs_moviedir = configfile.getString("network_nfs_moviedir", TARGET_ROOT "/media/sda1/movies");
	g_settings.network_nfs_picturedir = configfile.getString("network_nfs_picturedir", TARGET_ROOT "/media/sda1/pictures");
	g_settings.network_nfs_recordingdir = configfile.getString("network_nfs_recordingdir", TARGET_ROOT "/media/sda1/movies");
	g_settings.network_nfs_streamripperdir = configfile.getString("network_nfs_streamripperdir", TARGET_ROOT "/media/sda1/music/streamripper");

	g_settings.downloadcache_dir = configfile.getString("downloadcache_dir", g_settings.network_nfs_recordingdir.c_str());
	g_settings.logo_hdd_dir = configfile.getString("logo_hdd_dir", TARGET_ROOT "/media/sda1/logos");

	// recording
	g_settings.record_hours = configfile.getInt32("record_hours", 4);
	g_settings.recording_already_found_check = configfile.getBool("recording_already_found_check", false);
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	g_settings.recording_bufsize = configfile.getInt32("recording_bufsize", 4);
	g_settings.recording_bufsize_dmx = configfile.getInt32("recording_bufsize_dmx", 2);
#endif
	g_settings.recording_choose_direct_rec_dir = configfile.getInt32("recording_choose_direct_rec_dir", 0);
	g_settings.recording_epg_for_end = configfile.getBool("recording_epg_for_end", true);
	g_settings.recording_epg_for_filename = configfile.getBool("recording_epg_for_filename", true);
	g_settings.recording_filename_template = configfile.getString("recordingmenu.filename_template", "%C_%T_%d_%t");
	g_settings.recording_save_in_channeldir = configfile.getBool("recording_save_in_channeldir", false);
	g_settings.recording_fill_warning = configfile.getInt32("recording_fill_warning", 95);
	g_settings.recording_slow_warning = configfile.getBool("recording_slow_warning", false);
	g_settings.recording_startstop_msg = configfile.getBool("recording_startstop_msg", true);
	g_settings.recording_stopsectionsd = configfile.getBool("recording_stopsectionsd", false);
	g_settings.recording_stream_pmt_pid = configfile.getBool("recordingmenu.stream_pmt_pid", false);
	g_settings.recording_stream_subtitle_pids = configfile.getBool("recordingmenu.stream_subtitle_pids", true);
	g_settings.recording_stream_vtxt_pid = configfile.getBool("recordingmenu.stream_vtxt_pid", true);
	g_settings.recording_type = configfile.getInt32("recording_type", CNeutrinoApp::RECORDING_FILE);
	g_settings.recording_zap_on_announce = configfile.getBool("recording_zap_on_announce", false);
	g_settings.shutdown_timer_record_type = configfile.getBool("shutdown_timer_record_type", false);

	g_settings.recording_audio_pids_default = configfile.getInt32("recording_audio_pids_default", TIMERD_APIDS_STD | TIMERD_APIDS_AC3);

	// streaming
	g_settings.streaming_ecmmode = configfile.getInt32("streaming_ecmmode", 0);
	g_settings.streaming_decryptmode = configfile.getInt32("streaming_decryptmode", 1);
	g_settings.streaming_port = configfile.getInt32("streaming_port", 31339);

	// timeshift
	g_settings.timeshiftdir = configfile.getString("timeshiftdir", "");
	g_settings.timeshift_auto = configfile.getInt32("timeshift_auto", 0);
	g_settings.timeshift_delete = configfile.getInt32("timeshift_delete", 1);
	g_settings.timeshift_hours = configfile.getInt32("timeshift_hours", 4);
	g_settings.timeshift_pause = configfile.getInt32("timeshift_pause", 1);
	g_settings.timeshift_temp = configfile.getInt32("timeshift_temp", 1);

	std::string timeshiftdir;
	if (g_settings.timeshiftdir.empty())
	{
		timeshiftdir = g_settings.network_nfs_recordingdir + "/.timeshift";
		safe_mkdir(timeshiftdir.c_str());
	}
	else
	{
		if (g_settings.timeshiftdir != g_settings.network_nfs_recordingdir)
			timeshiftdir = g_settings.timeshiftdir;
		else
			timeshiftdir = g_settings.network_nfs_recordingdir + "/.timeshift";
	}
	dprintf(DEBUG_NORMAL, "recording dir: %s\n", g_settings.network_nfs_recordingdir.c_str());
	dprintf(DEBUG_NORMAL, "timeshift dir: %s\n", timeshiftdir.c_str());

	CRecordManager::getInstance()->SetTimeshiftDirectory(timeshiftdir.c_str());

	// remove old timeshift recordings
	if (g_settings.timeshift_delete)
	{
		/*
		   Why only remove old timeshift recordings
		   if timeshift-dir == recording-dir?
		*/
		//if (g_settings.timeshiftdir == g_settings.network_nfs_recordingdir)
		{
			DIR *d = opendir(timeshiftdir.c_str());
			if (d)
			{
				while (struct dirent *e = readdir(d))
				{
					std::string filename = e->d_name;
					if ((filename.find("_temp.ts") == filename.size() - 8) || (filename.find("_temp.xml") == filename.size() - 9))
					{
						std::string timeshiftdir_filename = timeshiftdir;
						timeshiftdir_filename += "/" + filename;
						remove(timeshiftdir_filename.c_str());
					}
				}
				closedir(d);
			}
		}
	}

	// ntp server for sectionsd
	g_settings.network_ntpenable = configfile.getBool("network_ntpenable", true);
	g_settings.network_ntpserver = configfile.getString("network_ntpserver", "0.de.pool.ntp.org");
	g_settings.network_ntprefresh = configfile.getString("network_ntprefresh", "30");
	g_settings.network_ntpatboot = configfile.getBool("network_ntpatboot", false);

	// personalize
	for (int i = 0; i < SNeutrinoSettings::P_SETTINGS_MAX; i++) // settings.h, settings.cpp
		g_settings.personalize[i] = configfile.getInt32(personalize_settings[i].personalize_settings_name, personalize_settings[i].personalize_default_val);
	g_settings.personalize_pincode = configfile.getString("personalize_pincode", "0000");

	// widget settings
	g_settings.widget_fade = false;
	g_settings.widget_fade = configfile.getBool("widget_fade", false);

	// webtv
	g_settings.webtv_xml_auto = configfile.getInt32("webtv_xml_auto", 1);
	g_settings.webtv_xml.clear();
	int webtv_count = configfile.getInt32("webtv_xml_count", 0);
	if (webtv_count)
	{
		for (int i = 0; i < webtv_count; i++)
		{
			std::string k = "webtv_xml_" + to_string(i);
			std::string webtv_xml = configfile.getString(k, "");
			if (webtv_xml.empty())
				continue;
			g_settings.webtv_xml.push_back(webtv_xml);
		}
	}
	else
	{
		std::string webtv_xml = configfile.getString("webtv_xml", WEBTV_XML);
		if (file_size(webtv_xml.c_str()))
			g_settings.webtv_xml.push_back(webtv_xml);
	}
	g_settings.last_webtv_dir = configfile.getString("last_webtv_dir", WEBTVDIR_VAR);

	CWebChannelsSetup webchannelssetup;
	webchannelssetup.webtv_xml_auto();

	// webradio
	g_settings.webradio_xml_auto = configfile.getInt32("webradio_xml_auto", 1);
	g_settings.webradio_xml.clear();
#ifndef BOXMODEL_CST_HD1
	/*
	   Coolstream's HD1 generation can't play audiostreams via movieplayer
	   because of driver- or firmware-issues or so. Not sure.
	   So let's avoid loading webradio_xml to get an empty webradio bouquet.
	*/
	int webradio_count = configfile.getInt32("webradio_xml_count", 0);
	if (webradio_count)
	{
		for (int i = 0; i < webradio_count; i++)
		{
			std::string k = "webradio_xml_" + to_string(i);
			std::string webradio_xml = configfile.getString(k, "");
			if (webradio_xml.empty())
				continue;
			g_settings.webradio_xml.push_back(webradio_xml);
		}
	}
	else
	{
		std::string webradio_xml = configfile.getString("webradio_xml", WEBRADIO_XML);
		if (file_size(webradio_xml.c_str()))
			g_settings.webradio_xml.push_back(webradio_xml);
	}
	g_settings.last_webradio_dir = configfile.getString("last_webradio_dir", WEBRADIODIR_VAR);

	webchannelssetup.webradio_xml_auto();
#endif

	// xmltv
	g_settings.xmltv_xml.clear();
	int xmltv_count = configfile.getInt32("xmltv_xml_count", 0);
	if (xmltv_count)
	{
		for (int i = 0; i < xmltv_count; i++)
		{
			std::string k = "xmltv_xml_" + to_string(i);
			std::string xmltv_xml = configfile.getString(k, "");
			if (xmltv_xml.empty())
				continue;
			g_settings.xmltv_xml.push_back(xmltv_xml);
		}
	}

	g_settings.xmltv_xml_auto.clear();

	g_settings.livestreamResolution = configfile.getInt32("livestreamResolution", 1920);
	g_settings.livestreamScriptPath = configfile.getString("livestreamScriptPath", WEBTVDIR);

	// plugins
	g_settings.plugin_hdd_dir = configfile.getString("plugin_hdd_dir", TARGET_ROOT "/media/sda1/plugins");
	g_settings.plugins_disabled = configfile.getString("plugins_disabled", "");
	g_settings.plugins_game = configfile.getString("plugins_game", "");
	g_settings.plugins_lua = configfile.getString("plugins_lua", "");
	g_settings.plugins_script = configfile.getString("plugins_script", "");
	g_settings.plugins_tool = configfile.getString("plugins_tool", "");

	// default plugin for movieplayer
	g_settings.movieplayer_plugin = configfile.getString("movieplayer_plugin", "---");

	// screenshot
	g_settings.screenshot_dir = configfile.getString("screenshot_dir", TARGET_ROOT "/media/sda1/movies");
	g_settings.screenshot_count = configfile.getInt32("screenshot_count", 1);
	g_settings.screenshot_cover = configfile.getInt32("screenshot_cover", 0);
	g_settings.screenshot_format = configfile.getInt32("screenshot_format", 1);
	g_settings.screenshot_mode = configfile.getInt32("screenshot_mode", 0);
	g_settings.screenshot_scale = configfile.getInt32("screenshot_scale", 0);
	g_settings.screenshot_video = configfile.getInt32("screenshot_video", 1);
	g_settings.auto_cover = configfile.getInt32("auto_cover", 0);

	// screen configuration
	int osd_res = OSDMODE_720;
#if HAVE_ARM_HARDWARE || (HAVE_CST_HARDWARE && defined(BOXMODEL_CST_HD2))
	osd_res = OSDMODE_1080;
#endif
	g_settings.osd_resolution = (osd_resolution_tmp == -1) ? configfile.getInt32("osd_resolution", osd_res) : osd_resolution_tmp;
	COsdHelpers::getInstance()->g_settings_osd_resolution_save = g_settings.osd_resolution;

	// default for fullpixel
	g_settings.screen_StartX_a_0 = configfile.getInt32("screen_StartX_a_0", 0);
	g_settings.screen_StartY_a_0 = configfile.getInt32("screen_StartY_a_0", 0);
	g_settings.screen_EndX_a_0 = configfile.getInt32("screen_EndX_a_0", 1280 - g_settings.screen_StartX_a_0 - 1);
	g_settings.screen_EndY_a_0 = configfile.getInt32("screen_EndY_a_0",  720 - g_settings.screen_StartY_a_0 - 1);
	g_settings.screen_StartX_a_1 = configfile.getInt32("screen_StartX_a_1", 0);
	g_settings.screen_StartY_a_1 = configfile.getInt32("screen_StartY_a_1", 0);
	g_settings.screen_EndX_a_1 = configfile.getInt32("screen_EndX_a_1", 1920 - g_settings.screen_StartX_a_1 - 1);
	g_settings.screen_EndY_a_1 = configfile.getInt32("screen_EndY_a_1", 1080 - g_settings.screen_StartY_a_1 - 1);

	// default for non fullpixel
	g_settings.screen_StartX_b_0 = configfile.getInt32("screen_StartX_b_0", 22);
	g_settings.screen_StartY_b_0 = configfile.getInt32("screen_StartY_b_0", 12);
	g_settings.screen_EndX_b_0 = configfile.getInt32("screen_EndX_b_0", 1259 - g_settings.screen_StartX_b_0 - 1);
	g_settings.screen_EndY_b_0 = configfile.getInt32("screen_EndY_b_0",  708 - g_settings.screen_StartY_b_0 - 1);
	g_settings.screen_StartX_b_1 = configfile.getInt32("screen_StartX_b_1", 33);
	g_settings.screen_StartY_b_1 = configfile.getInt32("screen_StartY_b_1", 18);
	g_settings.screen_EndX_b_1 = configfile.getInt32("screen_EndX_b_1", 1888 - g_settings.screen_StartX_b_1 - 1);
	g_settings.screen_EndY_b_1 = configfile.getInt32("screen_EndY_b_1", 1062 - g_settings.screen_StartY_b_1 - 1);

	g_settings.screen_preset = configfile.getInt32("screen_preset", COsdSetup::PRESET_SCREEN_A);

	setScreenSettings();

	// avoid configuration mismatch
	if (g_settings.screen_EndX >= g_settings.screen_width)
		g_settings.screen_EndX = g_settings.screen_width - 1;
	if (g_settings.screen_EndY >= g_settings.screen_height)
		g_settings.screen_EndY = g_settings.screen_height - 1;

	// software update
	g_settings.softupdate_autocheck = configfile.getBool("softupdate_autocheck", false);
#if ENABLE_PKG_MANAGEMENT
	g_settings.softupdate_autocheck_packages = configfile.getInt32("softupdate_autocheck_packages", false);
#endif
	g_settings.softupdate_mode = configfile.getInt32("softupdate_mode", 1);
	g_settings.apply_kernel = configfile.getBool("apply_kernel", false);
	g_settings.apply_settings = configfile.getBool("apply_settings", false);
	g_settings.softupdate_url_file = configfile.getString("softupdate_url_file", "/var/etc/update.urls");
#if ENABLE_EXTUPDATE
	g_settings.softupdate_name_mode_apply = configfile.getInt32("softupdate_name_mode_apply", CExtUpdate::SOFTUPDATE_NAME_DEFAULT);
	g_settings.softupdate_name_mode_backup = configfile.getInt32("softupdate_name_mode_backup", CExtUpdate::SOFTUPDATE_NAME_DEFAULT);
#else
	g_settings.softupdate_name_mode_apply = 0;
	g_settings.softupdate_name_mode_backup = 0;
#endif
	g_settings.softupdate_proxyserver = configfile.getString("softupdate_proxyserver", "");
	g_settings.softupdate_proxyusername = configfile.getString("softupdate_proxyusername", "");
	g_settings.softupdate_proxypassword = configfile.getString("softupdate_proxypassword", "");

	if (g_settings.softupdate_proxyserver.empty())
		unsetenv("http_proxy");
	else
	{
		std::string proxy = "http://";
		if (!g_settings.softupdate_proxyusername.empty())
			proxy += g_settings.softupdate_proxyusername + ":" + g_settings.softupdate_proxypassword + "@";
		proxy += g_settings.softupdate_proxyserver;
		setenv("http_proxy", proxy.c_str(), 1);
	}

	g_settings.flashupdate_createimage_add_env = configfile.getInt32("flashupdate_createimage_add_env", 0);
	g_settings.flashupdate_createimage_add_kernel = configfile.getInt32("flashupdate_createimage_add_kernel", 1);
	g_settings.flashupdate_createimage_add_root1 = configfile.getInt32("flashupdate_createimage_add_root1", 0);
	g_settings.flashupdate_createimage_add_spare = configfile.getInt32("flashupdate_createimage_add_spare", 0);
	g_settings.flashupdate_createimage_add_u_boot = configfile.getInt32("flashupdate_createimage_add_u_boot", 0);
	g_settings.flashupdate_createimage_add_uldr = configfile.getInt32("flashupdate_createimage_add_uldr", 1);
	g_settings.flashupdate_createimage_add_var = configfile.getInt32("flashupdate_createimage_add_var", 1);

	g_settings.backup_dir = configfile.getString("backup_dir", TARGET_ROOT "/media");
	g_settings.update_dir = configfile.getString("update_dir", "/tmp");
	g_settings.update_dir_opkg = configfile.getString("update_dir_opkg", g_settings.update_dir);

	// parentallock
	if (!parentallocked)
	{
		g_settings.parentallock_prompt = configfile.getInt32("parentallock_prompt", 0);
		g_settings.parentallock_lockage = configfile.getInt32("parentallock_lockage", 12);
	}
	else
	{
		g_settings.parentallock_prompt = 3;
		g_settings.parentallock_lockage = 18;
	}
	g_settings.parentallock_defaultlocked = configfile.getInt32("parentallock_defaultlocked", 0);
	g_settings.parentallock_pincode = configfile.getString("parentallock_pincode", "0000");
	g_settings.parentallock_zaptime = configfile.getInt32("parentallock_zaptime", 60);

	// fonts
	g_settings.font_file = configfile.getString("font_file", FONTDIR"/neutrino.ttf");
	g_settings.font_file_monospace = configfile.getString("font_file_monospace", FONTDIR"/tuxtxt.ttf");
	if (access(g_settings.font_file_monospace, F_OK) != 0)
	{
		g_settings.font_file_monospace = FONTDIR "/tuxtxt.ttf";
		configfile.setUnknownKeyQueryedFlag(true); // force saving config
	}
	font_file_monospace = g_settings.font_file_monospace.c_str();

	g_settings.font_scaling_x = configfile.getInt32("font_scaling_x", 105);
	g_settings.font_scaling_y = configfile.getInt32("font_scaling_y", 105);

	// online services
	g_settings.weather_api_key = WEATHER_API_KEY;
	g_settings.weather_api_version = WEATHER_API_VERSION;
#if ENABLE_WEATHER_KEY_MANAGE
	g_settings.weather_api_key = configfile.getString("weather_api_key", g_settings.weather_api_key.empty() ? "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX" : g_settings.weather_api_key);
	g_settings.weather_api_version = configfile.getString("weather_api_version", g_settings.weather_api_version.empty() ? "3.0" : g_settings.weather_api_version);
#endif
	g_settings.weather_enabled = configfile.getInt32("weather_enabled", 1);
	g_settings.weather_enabled = g_settings.weather_enabled && CApiKey::check_weather_api_key();

	g_settings.weather_city = configfile.getString("weather_city", WEATHER_DEFAULT_CITY);
	g_settings.weather_location = configfile.getString("weather_location", WEATHER_DEFAULT_LOCATION);
	g_settings.weather_postalcode = configfile.getString("weather_postalcode", WEATHER_DEFAULT_POSTALCODE);

	g_settings.youtube_api_key = YOUTUBE_API_KEY;
#if ENABLE_YOUTUBE_KEY_MANAGE
	g_settings.youtube_api_key = configfile.getString("youtube_api_key", g_settings.youtube_api_key.empty() ? "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX" : g_settings.youtube_api_key);
#endif
	g_settings.youtube_enabled = configfile.getInt32("youtube_enabled", 1);
	g_settings.youtube_enabled = g_settings.youtube_enabled && CApiKey::check_youtube_api_key();

	g_settings.tmdb_api_key = TMDB_API_KEY;
#if ENABLE_TMDB_KEY_MANAGE
	g_settings.tmdb_api_key = configfile.getString("tmdb_api_key", g_settings.tmdb_api_key.empty() ? "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX" : g_settings.tmdb_api_key);
#endif
	g_settings.tmdb_enabled = configfile.getInt32("tmdb_enabled", 1);
	g_settings.tmdb_enabled = g_settings.tmdb_enabled && CApiKey::check_tmdb_api_key();

	g_settings.omdb_api_key = OMDB_API_KEY;
#if ENABLE_OMDB_KEY_MANAGE
	g_settings.omdb_api_key = configfile.getString("omdb_api_key", g_settings.omdb_api_key.empty() ? "XXXXXXXX" : g_settings.omdb_api_key);
#endif
	g_settings.omdb_enabled = configfile.getInt32("omdb_enabled", 1);
	g_settings.omdb_enabled = g_settings.omdb_enabled && CApiKey::check_omdb_api_key();

	g_settings.shoutcast_dev_id = SHOUTCAST_DEV_ID;
#if ENABLE_SHOUTCAST_ID_MANAGE
	g_settings.shoutcast_dev_id = configfile.getString("shoutcast_dev_id", g_settings.shoutcast_dev_id.empty() ? "XXXXXXXXXXXXXXXX" : g_settings.shoutcast_dev_id);
#endif
	g_settings.shoutcast_enabled = configfile.getInt32("shoutcast_enabled", 1);
	g_settings.shoutcast_enabled = g_settings.shoutcast_enabled && CApiKey::check_shoutcast_dev_id();

	// zapit setup
	g_settings.StartChannelTV = configfile.getString("startchanneltv", "");
	g_settings.StartChannelRadio = configfile.getString("startchannelradio", "");
	g_settings.startchanneltv_id = configfile.getInt64("startchanneltv_id", 0);
	g_settings.startchannelradio_id = configfile.getInt64("startchannelradio_id", 0);
	g_settings.uselastchannel = configfile.getInt32("uselastchannel", 1);

	// adzap
	g_settings.adzap_zapBackPeriod = configfile.getInt32("adzap_zapBackPeriod", 180);
	g_settings.adzap_zapOnActivation = configfile.getInt32("adzap_zapOnActivation", 0);
	g_settings.adzap_writeData = configfile.getInt32("adzap_writeData", 0);

	// pip
#if ENABLE_PIP
	g_settings.pip_x = configfile.getInt32("pip_x", 50);
	g_settings.pip_y = configfile.getInt32("pip_y", 50);
	g_settings.pip_width = configfile.getInt32("pip_width", 365);
	g_settings.pip_height = configfile.getInt32("pip_height", 200);

	g_settings.pip_radio_x = configfile.getInt32("pip_radio_x", g_settings.pip_x);
	g_settings.pip_radio_y = configfile.getInt32("pip_radio_y", g_settings.pip_y);
	g_settings.pip_radio_width = configfile.getInt32("pip_radio_width", g_settings.pip_width);
	g_settings.pip_radio_height = configfile.getInt32("pip_radio_height", g_settings.pip_height);

	g_settings.pip_rotate_lastpos = configfile.getInt32("pip_rotate_lastpos", PIP_NO_POS);
#endif

#if ENABLE_QUADPIP
	for (unsigned int i = 0; i < 4; i++)
	{
		sprintf(cfg_key, "quadpip_channel_window_%d", i);
		g_settings.quadpip_channel_window[i] = configfile.getString(cfg_key, "-");
		sprintf(cfg_key, "quadpip_channel_id_window_%d", i);
		g_settings.quadpip_channel_id_window[i] = configfile.getInt64(cfg_key, 0);
	}
#endif

	// pictureviewer
	g_settings.picviewer_scaling = configfile.getInt32("picviewer_scaling", 1 /*(int)CPictureViewer::SIMPLE*/);
	g_settings.picviewer_slide_time = configfile.getInt32("picviewer_slide_time", 10);

	// audioplayer
	g_settings.audioplayer_cover_as_screensaver = configfile.getInt32("audioplayer_cover_as_screensaver", 1);
	g_settings.audioplayer_display = configfile.getInt32("audioplayer_display", (int)CAudioPlayerGui::ARTIST_TITLE);
	g_settings.audioplayer_enable_sc_metadata = configfile.getInt32("audioplayer_enable_sc_metadata", 1);
	g_settings.audioplayer_follow = configfile.getInt32("audioplayer_follow", 0);
	g_settings.audioplayer_highprio = configfile.getInt32("audioplayer_highprio", 0);
	g_settings.audioplayer_repeat_on = configfile.getInt32("audioplayer_repeat_on", 0);
	g_settings.audioplayer_select_title_by_name = configfile.getInt32("audioplayer_select_title_by_name", 0);
	g_settings.audioplayer_show_playlist = configfile.getInt32("audioplayer_show_playlist", 1);
	g_settings.inetradio_autostart = configfile.getInt32("inetradio_autostart", 0);
	g_settings.spectrum = configfile.getBool("spectrum", false);

	// movieplayer
	g_settings.movieplayer_bisection_jump = configfile.getInt32("movieplayer_bisection_jump", 5);
	g_settings.movieplayer_display_playtime = configfile.getInt32("movieplayer_display_playtime", g_info.hw_caps->display_type == HW_DISPLAY_LED_NUM);
	g_settings.movieplayer_eof_cnt = configfile.getInt32("movieplayer_eof_cnt", 1);
	g_settings.movieplayer_repeat_on = configfile.getInt32("movieplayer_repeat_on", CMoviePlayerGui::REPEAT_OFF);
#if HAVE_CST_HARDWARE
	g_settings.movieplayer_select_ac3_atype0 = configfile.getBool("movieplayer_select_ac3_atype0", false);
#endif
	g_settings.movieplayer_timeosd_while_searching = configfile.getInt32("movieplayer_timeosd_while_searching", 1);

	// filebrowser
	g_settings.filebrowser_denydirectoryleave = configfile.getBool("filebrowser_denydirectoryleave", false);
	g_settings.filebrowser_showrights = configfile.getInt32("filebrowser_showrights", 1);
	g_settings.filebrowser_sortmethod = configfile.getInt32("filebrowser_sortmethod", 1);
	if ((g_settings.filebrowser_sortmethod < 0) || (g_settings.filebrowser_sortmethod >= FILEBROWSER_NUMBER_OF_SORT_VARIANTS))
		g_settings.filebrowser_sortmethod = 0;

	// infoclock
	g_settings.mode_clock = configfile.getInt32("mode_clock", 0);
	g_settings.infoClockBackground = configfile.getInt32("infoClockBackground", 0);
	g_settings.infoClockFontSize = configfile.getInt32("infoClockFontSize", 30);
	g_settings.infoClockSeconds = configfile.getInt32("infoClockSeconds", 0);

	// volume gui
	g_settings.show_mute_icon = configfile.getInt32("show_mute_icon", 0);
	g_settings.volume_digits = configfile.getBool("volume_digits", true);
	g_settings.volume_pos = configfile.getInt32("volume_pos", CVolumeBar::VOLUMEBAR_POS_BOTTOM_CENTER);
	g_settings.volume_size = configfile.getInt32("volume_size", 26);

	// menu
	g_settings.menu_pos = configfile.getInt32("menu_pos", CMenuWidget::MENU_POS_CENTER);
	g_settings.show_menu_hints = configfile.getBool("show_menu_hints", true);
	g_settings.show_menu_hints_line = configfile.getBool("show_menu_hints_line", false);

	// epgview
	g_settings.bigFonts = configfile.getInt32("bigFonts", 1);

	// eventlist
	g_settings.eventlist_additional = configfile.getInt32("eventlist_additional", 1);
	g_settings.eventlist_epgplus = configfile.getInt32("eventlist_epgplus", 1);

	// channellist
	g_settings.channellist_descmode = false;
	g_settings.channellist_displaymode = DISPLAY_MODE_NOW;

	g_settings.channellist_additional = configfile.getInt32("channellist_additional", 1); // default no minitv
	g_settings.channellist_epgtext_alignment = configfile.getInt32("channellist_epgtext_alignment", EPGTEXT_ALIGN_LEFT_MIDDLE);
	g_settings.channellist_foot = configfile.getInt32("channellist_foot", 1); // default next Event
	g_settings.channellist_new_zap_mode = configfile.getInt32("channellist_new_zap_mode", 0);
	g_settings.channellist_numeric_adjust = configfile.getInt32("channellist_numeric_adjust", 0);
	g_settings.channellist_show_channellogo = configfile.getInt32("channellist_show_channellogo", CCHeaderTypes::CC_LOGO_RIGHT);
	g_settings.channellist_show_eventlogo = configfile.getInt32("channellist_show_eventlogo", 1);
	g_settings.channellist_show_infobox = configfile.getInt32("channellist_show_infobox", 1);
	g_settings.channellist_show_numbers = configfile.getInt32("channellist_show_numbers", 1);
	g_settings.channellist_show_res_icon = configfile.getInt32("channellist_show_res_icon", 0);
	g_settings.channellist_sort_mode = configfile.getInt32("channellist_sort_mode", 0); // sort mode: alpha, freq, sat

	// infobar
	g_settings.infobar_analogclock = configfile.getInt32("infobar_analogclock", 0);
	g_settings.infobar_buttons_usertitle = configfile.getBool("infobar_buttons_usertitle", false);
	g_settings.infobar_casystem_display = configfile.getInt32("infobar_casystem_display", 0);
	g_settings.infobar_casystem_dotmatrix = configfile.getInt32("infobar_casystem_dotmatrix", 0);
	g_settings.infobar_casystem_frame = configfile.getInt32("infobar_casystem_frame", 0);
	g_settings.infobar_progressbar = configfile.getInt32("infobar_progressbar", 2);
	g_settings.infobar_sat_display = configfile.getBool("infobar_sat_display", true);
	g_settings.infobar_show = configfile.getInt32("infobar_show", 0);
	g_settings.infobar_show_channeldesc = configfile.getBool("infobar_show_channeldesc", false);
	g_settings.infobar_show_channellogo = configfile.getInt32("infobar_show_channellogo", 5);
	g_settings.infobar_show_dd_available = configfile.getInt32("infobar_show_dd_available", 1);
	g_settings.infobar_show_res = configfile.getInt32("infobar_show_res", 0);
	g_settings.infobar_show_sysfs_hdd = configfile.getBool("infobar_show_sysfs_hdd", false);
	g_settings.infobar_show_tuner = configfile.getInt32("infobar_show_tuner", 1);
	g_settings.infobar_subchan_disp_pos = configfile.getInt32("infobar_subchan_disp_pos", 4);
	g_settings.infobar_weather = configfile.getInt32("infobar_weather", 0);

	g_settings.scrambled_message = configfile.getBool("scrambled_message", false);

	// windows
	g_settings.window_size = configfile.getInt32("window_size", 100);
	g_settings.window_width = configfile.getInt32("window_width", g_settings.window_size);
	g_settings.window_height = configfile.getInt32("window_height", g_settings.window_size);

	// osd
	g_settings.osd_colorsettings_advanced_mode = configfile.getBool("osd_colorsettings_advanced_mode", false);
#ifdef BOXMODEL_CST_HD2
	g_settings.enable_sd_osd = configfile.getInt32("enable_sd_osd", 1);
#endif

	// timing
	for (int i = 0; i < SNeutrinoSettings::TIMING_SETTING_COUNT; i++)
		g_settings.timing[i] = configfile.getInt32(locale_real_names[timing_setting[i].name], timing_setting[i].default_timing);

	// timing/handling infobar
	for (int i = 0; i < SNeutrinoSettings::HANDLING_INFOBAR_SETTING_COUNT; i++)
		g_settings.handling_infobar[i] = configfile.getInt32(locale_real_names[handling_infobar_setting[i].name], handling_infobar_setting[i].default_timing);

	// usermenu -> in system/settings.h
	for (unsigned int i = 0; i < g_settings.usermenu.size(); ++i)
	{
		delete g_settings.usermenu[i];
		g_settings.usermenu[i] = NULL;
	}
	g_settings.usermenu.clear();
	if (configfile.getString("usermenu_key_red", "").empty() ||
		configfile.getString("usermenu_key_green", "").empty() ||
		configfile.getString("usermenu_key_yellow", "").empty() ||
		configfile.getString("usermenu_key_blue", "").empty())
	{
		for (SNeutrinoSettings::usermenu_t *um = usermenu_default; um->key != CRCInput::RC_nokey; um++)
		{
			SNeutrinoSettings::usermenu_t *u = new SNeutrinoSettings::usermenu_t;
			*u = *um;
			g_settings.usermenu.push_back(u);
		}
	}
	else
	{
		bool unknown = configfile.getUnknownKeyQueryedFlag();
		for (unsigned int i = 0; ; i++)
		{
			std::string name = (i < 4) ? usermenu_default[i].name : to_string(i);
			std::string usermenu_key("usermenu_key_");
			usermenu_key += name;
			int uk = configfile.getInt32(usermenu_key, CRCInput::RC_nokey);
			if (!uk || uk == (int)CRCInput::RC_nokey)
			{
				if (i > 3)
				{
					configfile.setUnknownKeyQueryedFlag(unknown);
					break;
				}
				continue;
			}
			SNeutrinoSettings::usermenu_t *u = new SNeutrinoSettings::usermenu_t; //FIXME: possible leak
			u->key = uk;

			std::string txt1("usermenu_tv_");
			txt1 += name;
			u->items = configfile.getString(txt1, "");
			txt1 += "_text";
			u->title = configfile.getString(txt1, "");

			g_settings.usermenu.push_back(u);
		}
	}

	g_settings.version_pseudo = configfile.getString("version_pseudo", "19700101000000");

	loadKeys();

	if (!erg)
	{
		if (g_settings.version_pseudo < NEUTRINO_VERSION_PSEUDO)
			upgradeSetup(fname);

		if (configfile.getUnknownKeyQueryedFlag())
			erg = 2;
	}

	if (erg)
		configfile.setModifiedFlag(true);

	return erg;
}

void CNeutrinoApp::setScreenSettings()
{
	g_settings.screen_width = frameBuffer->getScreenWidth(true);
	g_settings.screen_height = frameBuffer->getScreenHeight(true);

	switch (g_settings.osd_resolution)
	{
#ifdef ENABLE_CHANGE_OSD_RESOLUTION
		case 1:
		{
			switch (g_settings.screen_preset)
			{
				case COsdSetup::PRESET_SCREEN_B:
					g_settings.screen_StartX = g_settings.screen_StartX_b_1;
					g_settings.screen_StartY = g_settings.screen_StartY_b_1;
					g_settings.screen_EndX   = g_settings.screen_EndX_b_1;
					g_settings.screen_EndY   = g_settings.screen_EndY_b_1;
					break;
				case COsdSetup::PRESET_SCREEN_A:
				default:
					g_settings.screen_StartX = g_settings.screen_StartX_a_1;
					g_settings.screen_StartY = g_settings.screen_StartY_a_1;
					g_settings.screen_EndX   = g_settings.screen_EndX_a_1;
					g_settings.screen_EndY   = g_settings.screen_EndY_a_1;
					break;
			}
		}
		break;
#endif
		case 0:
		default:
		{
			switch (g_settings.screen_preset)
			{
				case COsdSetup::PRESET_SCREEN_B:
					g_settings.screen_StartX = g_settings.screen_StartX_b_0;
					g_settings.screen_StartY = g_settings.screen_StartY_b_0;
					g_settings.screen_EndX   = g_settings.screen_EndX_b_0;
					g_settings.screen_EndY   = g_settings.screen_EndY_b_0;
					break;
				case COsdSetup::PRESET_SCREEN_A:
				default:
					g_settings.screen_StartX = g_settings.screen_StartX_a_0;
					g_settings.screen_StartY = g_settings.screen_StartY_a_0;
					g_settings.screen_EndX   = g_settings.screen_EndX_a_0;
					g_settings.screen_EndY   = g_settings.screen_EndY_a_0;
					break;
			}
		}
		break;
	}
}

void CNeutrinoApp::upgradeSetup(const char * fname)
{
	dprintf(DEBUG_NORMAL, "upgrade/cleanup %s\n", fname);

	if (g_settings.version_pseudo < "20210331000000")
	{
		// switch g_settings.screen_preset
		/*
		   old: 0 = CRT, 1 = LCD
		   new: 0 = Screen 1, 1 = Screen 2
		*/
		g_settings.screen_preset = !g_settings.screen_preset;

		configfile.deleteKey("screen_EndX_crt_0");
		configfile.deleteKey("screen_EndX_crt_1");
		configfile.deleteKey("screen_EndX_lcd_0");
		configfile.deleteKey("screen_EndX_lcd_1");
		configfile.deleteKey("screen_EndY_crt_0");
		configfile.deleteKey("screen_EndY_crt_1");
		configfile.deleteKey("screen_EndY_lcd_0");
		configfile.deleteKey("screen_EndY_lcd_1");
		configfile.deleteKey("screen_StartX_crt_0");
		configfile.deleteKey("screen_StartX_crt_1");
		configfile.deleteKey("screen_StartX_lcd_0");
		configfile.deleteKey("screen_StartX_lcd_1");
		configfile.deleteKey("screen_StartY_crt_0");
		configfile.deleteKey("screen_StartY_crt_1");
		configfile.deleteKey("screen_StartY_lcd_0");
		configfile.deleteKey("screen_StartY_lcd_1");
	}
	if (g_settings.version_pseudo < "20210428200000")
	{
		g_settings.recording_type = CNeutrinoApp::RECORDING_FILE;
	}
	if (g_settings.version_pseudo < "20210702230000")
	{
		configfile.deleteKey("infobar_cn");
	}
	if (g_settings.version_pseudo < "20210801210000")
	{
		if (g_settings.usermenu[SNeutrinoSettings::BUTTON_YELLOW]->items == "7,35")
		{
			g_settings.usermenu[SNeutrinoSettings::BUTTON_YELLOW]->items = "7,35,freeze";
			configfile.setString("usermenu_tv_yellow", g_settings.usermenu[SNeutrinoSettings::BUTTON_YELLOW]->items);
		}
	}
	if (g_settings.version_pseudo < "20211209230000")
	{
		g_settings.bouquetlist_mode = SNeutrinoSettings::CHANNELLIST;
	}
	if (g_settings.version_pseudo < "20220114220000")
	{
		configfile.deleteKey("weather_country");
	}
	if (g_settings.version_pseudo < "20220130200000")
	{
		g_settings.font_file_monospace = configfile.getString("ttx_font_file", FONTDIR"/tuxtxt.ttf");
		if (access(g_settings.font_file_monospace, F_OK) != 0)
			g_settings.font_file_monospace = FONTDIR "/tuxtxt.ttf";
		configfile.deleteKey("ttx_font_file");
	}
	if (g_settings.version_pseudo < "20220506230000")
	{
		g_settings.channellist_epgtext_alignment = configfile.getBool("channellist_epgtext_align_right", false);
		if (g_settings.channellist_epgtext_alignment == 1) // old bool
			g_settings.channellist_epgtext_alignment = EPGTEXT_ALIGN_RIGHT_MIDDLE;
		configfile.deleteKey("channellist_epgtext_align_right");
	}
	if (g_settings.version_pseudo < "20221129210000")
	{
		configfile.deleteKey("glcd_weather_curr_x_position");
		configfile.deleteKey("glcd_weather_next_x_position");
	}
	if (g_settings.version_pseudo < "20240405210000")
	{
		g_settings.youtube_api_key = configfile.getString("youtube_dev_id", g_settings.youtube_api_key);
		configfile.deleteKey("youtube_dev_id");
	}
	if (g_settings.version_pseudo < "20240922210000")
	{
		g_settings.weather_api_version = "3.0";
	}

	g_settings.version_pseudo = NEUTRINO_VERSION_PSEUDO;
	configfile.setString("version_pseudo", g_settings.version_pseudo);
}

/**************************************************************************************
*          CNeutrinoApp -  saveSetup, save the application-settings                   *
**************************************************************************************/
void CNeutrinoApp::saveSetup(const char *fname)
{
	char cfg_key[81];

	// scansettings
	if (!scansettings.saveSettings(NEUTRINO_SCAN_SETTINGS_FILE))
	{
		dprintf(DEBUG_NORMAL, "error while saving scan-settings!\n");
	}

	// theme/color options
	CThemes::getInstance()->setTheme(configfile);
	configfile.setString("theme_name", g_settings.theme_name);

#ifdef ENABLE_GRAPHLCD
	CGLCDThemes::getInstance()->setTheme(configfile);
	configfile.setString("glcd_theme_name", g_settings.glcd_theme_name);

	configfile.setInt32("glcd_enable", g_settings.glcd_enable);
	configfile.setString("glcd_logodir", g_settings.glcd_logodir);

	configfile.setInt32("glcd_brightness", g_settings.glcd_brightness);
	configfile.setInt32("glcd_brightness_dim", g_settings.glcd_brightness_dim);
	configfile.setString("glcd_brightness_dim_time", g_settings.glcd_brightness_dim_time);
	configfile.setInt32("glcd_brightness_standby", g_settings.glcd_brightness_standby);
	configfile.setInt32("glcd_mirror_osd", g_settings.glcd_mirror_osd);
	configfile.setInt32("glcd_mirror_video", g_settings.glcd_mirror_video);
	configfile.setInt32("glcd_scroll", g_settings.glcd_scroll);
	configfile.setInt32("glcd_scroll_speed", g_settings.glcd_scroll_speed);
	configfile.setInt32("glcd_selected_config", g_settings.glcd_selected_config);
#endif

#ifdef ENABLE_LCD4LINUX
	configfile.setInt32("lcd4l_support", g_settings.lcd4l_support);
	configfile.setString("lcd4l_logodir", g_settings.lcd4l_logodir);
	configfile.setInt32("lcd4l_display_type", g_settings.lcd4l_display_type);
	configfile.setInt32("lcd4l_skin", g_settings.lcd4l_skin);
	configfile.setInt32("lcd4l_skin_radio", g_settings.lcd4l_skin_radio);
	configfile.setInt32("lcd4l_brightness", g_settings.lcd4l_brightness);
	configfile.setInt32("lcd4l_brightness_standby", g_settings.lcd4l_brightness_standby);
	configfile.setInt32("lcd4l_convert", g_settings.lcd4l_convert);
	configfile.setInt32("lcd4l_screenshots", g_settings.lcd4l_screenshots);
#endif

	configfile.setInt32("mode_icons", g_settings.mode_icons);
	configfile.setInt32("mode_icons_background", g_settings.mode_icons_background);
	configfile.setInt32("mode_icons_skin", g_settings.mode_icons_skin);
	for (int i = 0 ; i < MODE_ICONS_NR_OF_ENTRIES ; i++)
	{
		sprintf(cfg_key, "mode_icons_flag%d", i);
		configfile.setString(cfg_key, g_settings.mode_icons_flag[i]);
	}

	configfile.setInt32("show_ecm", g_settings.show_ecm);
	configfile.setInt32("show_ecm_pos", g_settings.show_ecm_pos);

	// video
	configfile.setInt32("video_Mode", g_settings.video_Mode);
	configfile.setInt32("analog_mode1", g_settings.analog_mode1);
	configfile.setInt32("analog_mode2", g_settings.analog_mode2);
	configfile.setInt32("video_Format", g_settings.video_Format);
	configfile.setInt32("video_43mode", g_settings.video_43mode);

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	configfile.setInt32("video_psi_brightness", g_settings.psi_brightness);
	configfile.setInt32("video_psi_contrast", g_settings.psi_contrast);
	configfile.setInt32("video_psi_saturation", g_settings.psi_saturation);
	configfile.setInt32("video_psi_step", g_settings.psi_step);
	configfile.setInt32("video_psi_tint", g_settings.psi_tint);
#endif

	// hdmi cec
	configfile.setInt32("hdmi_cec_mode", g_settings.hdmi_cec_mode);
	configfile.setInt32("hdmi_cec_view_on", g_settings.hdmi_cec_view_on);
	configfile.setInt32("hdmi_cec_standby", g_settings.hdmi_cec_standby);
	configfile.setInt32("hdmi_cec_volume", g_settings.hdmi_cec_volume);

	// volume
	configfile.setInt32( "current_volume", g_settings.current_volume );
	configfile.setInt32("current_volume_step", g_settings.current_volume_step);
	configfile.setInt32( "start_volume", g_settings.start_volume );
	configfile.setInt32("audio_volume_percent_ac3", g_settings.audio_volume_percent_ac3);
	configfile.setInt32("audio_volume_percent_pcm", g_settings.audio_volume_percent_pcm);

	configfile.setInt32("channel_mode", g_settings.channel_mode);
	configfile.setInt32("channel_mode_radio", g_settings.channel_mode_radio);
	configfile.setInt32("channel_mode_initial", g_settings.channel_mode_initial);
	configfile.setInt32("channel_mode_initial_radio", g_settings.channel_mode_initial_radio);

	configfile.setInt32("fan_speed", g_settings.fan_speed);

	configfile.setInt32("srs_enable", g_settings.srs_enable);
	configfile.setInt32("srs_algo", g_settings.srs_algo);
	configfile.setInt32("srs_ref_volume", g_settings.srs_ref_volume);
	configfile.setInt32("srs_nmgr_enable", g_settings.srs_nmgr_enable);
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	configfile.setInt32("ac3_pass", g_settings.ac3_pass);
	configfile.setInt32("dts_pass", g_settings.dts_pass);
#else
	configfile.setInt32("hdmi_dd", g_settings.hdmi_dd);
	configfile.setInt32("spdif_dd", g_settings.spdif_dd);
#endif
	configfile.setInt32("analog_out", g_settings.analog_out);

	configfile.setInt32("avsync", g_settings.avsync);
	configfile.setInt32("video_dbdr", g_settings.video_dbdr);

	for (int i = 0; i < VIDEOMENU_VIDEOMODE_OPTION_COUNT; i++)
	{
		sprintf(cfg_key, "enabled_video_mode_%d", i);
		configfile.setInt32(cfg_key, g_settings.enabled_video_modes[i]);
	}
	for (int i = 0; i < VIDEOMENU_VIDEOMODE_OPTION_COUNT; i++)
	{
		sprintf(cfg_key, "enabled_auto_mode_%d", i);
		configfile.setInt32(cfg_key, g_settings.enabled_auto_modes[i]);
	}

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	configfile.setInt32("zappingmode", g_settings.zappingmode);
	configfile.setInt32("hdmi_colorimetry", g_settings.hdmi_colorimetry);
#endif

	configfile.setInt32("cpufreq", g_settings.cpufreq);
	configfile.setInt32("standby_cpufreq", g_settings.standby_cpufreq);

	// ci-settings
	configfile.setInt32("ci_standby_reset", g_settings.ci_standby_reset);
	configfile.setInt32("ci_check_live", g_settings.ci_check_live);
	configfile.setInt32("ci_tuner", g_settings.ci_tuner);
	configfile.setInt32("ci_rec_zapto", g_settings.ci_rec_zapto);
	configfile.setInt32("ci_mode", g_settings.ci_mode);
#if BOXMODEL_VUPLUS_ALL
	configfile.setInt32("ci_delay", g_settings.ci_delay);
#endif
	// ci-settings for each slot
	for (unsigned int i = 0; i < cCA::GetInstance()->GetNumberCISlots(); i++)
	{
		sprintf(cfg_key, "ci_ignore_messages_%d", i);
		configfile.setInt32(cfg_key, g_settings.ci_ignore_messages[i]);
		sprintf(cfg_key, "ci_save_pincode_%d", i);
		configfile.setInt32(cfg_key, g_settings.ci_save_pincode[i]);
		sprintf(cfg_key, "ci_pincode_%d", i);
		configfile.setString(cfg_key, g_settings.ci_pincode[i]);
		sprintf(cfg_key, "ci_op_%d", i);
		configfile.setInt32(cfg_key, g_settings.ci_op[i]);
		sprintf(cfg_key, "ci_clock_%d", i);
		configfile.setInt32(cfg_key, g_settings.ci_clock[i]);
#if BOXMODEL_VUPLUS_ALL
		sprintf(cfg_key, "ci_rpr_%d", i);
		configfile.setInt32(cfg_key, g_settings.ci_rpr[i]);
#endif
	}

	configfile.setInt32("make_hd_list", g_settings.make_hd_list);
	configfile.setInt32("make_new_list", g_settings.make_new_list);
	configfile.setInt32("make_removed_list", g_settings.make_removed_list);
	configfile.setInt32("make_webradio_list", g_settings.make_webradio_list);
	configfile.setInt32("make_webtv_list", g_settings.make_webtv_list);

	configfile.setInt32("keep_channel_numbers", g_settings.keep_channel_numbers);
	configfile.setInt32("show_empty_favorites", g_settings.show_empty_favorites);

	// lcd/led
	for (int i = 0; i < SNeutrinoSettings::LCD_SETTING_COUNT; i++)
		configfile.setInt32(lcd_setting[i].name, g_settings.lcd_setting[i]);
	configfile.setString("lcd_dim_time", g_settings.lcd_setting_dim_time);
	configfile.setInt32("lcd_dim_brightness", g_settings.lcd_setting_dim_brightness);
	configfile.setInt32("lcd_info_line", g_settings.lcd_info_line);
	configfile.setInt32("lcd_scroll", g_settings.lcd_scroll);
	configfile.setInt32("lcd_notify_rclock", g_settings.lcd_notify_rclock);

	configfile.setInt32("backlight_tv", g_settings.backlight_tv);
	configfile.setInt32("backlight_standby", g_settings.backlight_standby);
	configfile.setInt32("backlight_deepstandby", g_settings.backlight_deepstandby);

	configfile.setInt32("led_tv_mode", g_settings.led_tv_mode);
	configfile.setInt32("led_standby_mode", g_settings.led_standby_mode);
	configfile.setInt32("led_deep_mode", g_settings.led_deep_mode);
	configfile.setInt32("led_rec_mode", g_settings.led_rec_mode);
	configfile.setInt32("led_blink", g_settings.led_blink);

#ifdef BOXMODEL_CST_HD2
	configfile.setInt32("brightness", g_settings.brightness);
	configfile.setInt32("contrast", g_settings.contrast);
	configfile.setInt32("saturation", g_settings.saturation);
#endif

	// hdd
	configfile.setInt32("hdd_fs", g_settings.hdd_fs);
	configfile.setInt32("hdd_sleep", g_settings.hdd_sleep);
	configfile.setInt32("hdd_noise", g_settings.hdd_noise);
	configfile.setInt32("hdd_statfs_mode", g_settings.hdd_statfs_mode);
	configfile.setInt32("hdd_format_on_mount_failed", g_settings.hdd_format_on_mount_failed);
	configfile.setInt32("hdd_wakeup", g_settings.hdd_wakeup);
	configfile.setInt32("hdd_wakeup_msg", g_settings.hdd_wakeup_msg);
	configfile.setInt32("hdd_allow_set_recdir", g_settings.hdd_allow_set_recdir);

	// timer
	configfile.setInt32("timer_remotebox_ip_count", g_settings.timer_remotebox_ip.size());
	int timer_remotebox_itemcount = 0;
	for (std::vector<timer_remotebox_item>::iterator it = g_settings.timer_remotebox_ip.begin(); it != g_settings.timer_remotebox_ip.end(); ++it)
	{
		configfile.setBool("timer_remotebox_enabled_" + to_string(timer_remotebox_itemcount), it->enabled);
		std::string k;
		k = "timer_remotebox_ip_" + to_string(timer_remotebox_itemcount);
		configfile.setString(k, it->rbaddress);
		k = "timer_remotebox_rbname_" + to_string(timer_remotebox_itemcount);
		configfile.setString(k, it->rbname);
		k = "timer_remotebox_user_" + to_string(timer_remotebox_itemcount);
		configfile.setString(k, it->user);
		k = "timer_remotebox_pass_" + to_string(timer_remotebox_itemcount);
		configfile.setString(k, it->pass);
		k = "timer_remotebox_port_" + to_string(timer_remotebox_itemcount);
		configfile.setInt32(k, it->port);
		timer_remotebox_itemcount++;
	}
	configfile.setInt32("timer_followscreenings", g_settings.timer_followscreenings);

	// misc
	configfile.setInt32("zap_cycle", g_settings.zap_cycle);
	configfile.setInt32("remote_control_hardware", g_settings.remote_control_hardware);
	configfile.setInt32("enable_sdt", g_settings.enable_sdt);
	configfile.setBool("radiotext_enable", g_settings.radiotext_enable);
	configfile.setInt32("cacheTXT", g_settings.cacheTXT);
	configfile.setInt32("zapto_pre_time", g_settings.zapto_pre_time);
	configfile.setInt32("minimode", g_settings.minimode);

	configfile.setBool("filesystem_is_utf8", g_settings.filesystem_is_utf8);

	// shutdown
	configfile.setInt32("shutdown_count", g_settings.shutdown_count);
	configfile.setInt32("shutdown_min", g_settings.shutdown_min);
	configfile.setBool("shutdown_real", g_settings.shutdown_real);
	configfile.setBool("shutdown_real_rcdelay", g_settings.shutdown_real_rcdelay);
	configfile.setInt32("sleeptimer_min", g_settings.sleeptimer_min);
	configfile.setInt32("power_standby", g_settings.power_standby);

	// screen saver
	configfile.setInt32("screensaver_delay", g_settings.screensaver_delay);
	configfile.setString("screensaver_dir", g_settings.screensaver_dir);
	configfile.setInt32("screensaver_mode", g_settings.screensaver_mode);
	configfile.setInt32("screensaver_mode_text", g_settings.screensaver_mode_text);
	configfile.setInt32("screensaver_random", g_settings.screensaver_random);
	configfile.setInt32("screensaver_timeout", g_settings.screensaver_timeout);

	// vcr
	configfile.setBool("vcr_AutoSwitch", g_settings.vcr_AutoSwitch);

	// audio
	configfile.setInt32("audio_AnalogMode", g_settings.audio_AnalogMode);
	configfile.setBool("audio_DolbyDigital", g_settings.audio_DolbyDigital);

	configfile.setBool("audiochannel_up_down_enable", g_settings.audiochannel_up_down_enable);

	// language
	configfile.setInt32("auto_lang", g_settings.auto_lang);
	configfile.setInt32("auto_subs", g_settings.auto_subs);
	for (int i = 0; i < 3; i++)
	{
		sprintf(cfg_key, "pref_lang_%d", i);
		configfile.setString(cfg_key, g_settings.pref_lang[i]);
		sprintf(cfg_key, "pref_subs_%d", i);
		configfile.setString(cfg_key, g_settings.pref_subs[i]);
	}
	configfile.setString("subs_charset", g_settings.subs_charset);

	configfile.setString("language", g_settings.language);
	configfile.setString("timezone", g_settings.timezone);

	// epg
	configfile.setString("epg_dir", g_settings.epg_dir);
	configfile.setInt32("epg_cache_time", g_settings.epg_cache);
	configfile.setInt32("epg_extendedcache_time", g_settings.epg_extendedcache);
	configfile.setInt32("epg_max_events", g_settings.epg_max_events);
	configfile.setInt32("epg_old_events", g_settings.epg_old_events);
	configfile.setBool("epg_read", g_settings.epg_read);
	configfile.setInt32("epg_read_frequently", g_settings.epg_read_frequently);
	configfile.setBool("epg_save", g_settings.epg_save);
	configfile.setInt32("epg_save_frequently", g_settings.epg_save_frequently);
	configfile.setInt32("epg_save_mode", g_settings.epg_save_mode);
	configfile.setBool("epg_save_standby", g_settings.epg_save_standby);
	configfile.setInt32("epg_scan", g_settings.epg_scan);
	configfile.setInt32("epg_scan_mode", g_settings.epg_scan_mode);
	configfile.setInt32("epg_scan_rescan", g_settings.epg_scan_rescan);

	// epg search
	g_settings.epg_search_history_size = g_settings.epg_search_history.size();
	if (g_settings.epg_search_history_size > g_settings.epg_search_history_max)
		g_settings.epg_search_history_size = g_settings.epg_search_history_max;
	configfile.setInt32("epg_search_history_max", g_settings.epg_search_history_max);
	configfile.setInt32("epg_search_history_size", g_settings.epg_search_history_size);
	std::list<std::string>:: iterator _it = g_settings.epg_search_history.begin();
	for (int i = 0; i < g_settings.epg_search_history_size; i++, ++_it)
		configfile.setString("epg_search_history_" + to_string(i), *_it);

	// network
	configfile.setString("ifname", g_settings.ifname);

	for (int i = 0 ; i < NETWORK_NFS_NR_OF_ENTRIES ; i++)
	{
		sprintf(cfg_key, "network_nfs_ip_%d", i);
		configfile.setString(cfg_key, g_settings.network_nfs[i].ip);
		sprintf(cfg_key, "network_nfs_mac_%d", i);
		configfile.setString(cfg_key, g_settings.network_nfs[i].mac);
		sprintf(cfg_key, "network_nfs_local_dir_%d", i);
		configfile.setString(cfg_key, g_settings.network_nfs[i].local_dir);
		sprintf(cfg_key, "network_nfs_dir_%d", i);
		configfile.setString(cfg_key, g_settings.network_nfs[i].dir);
		sprintf(cfg_key, "network_nfs_automount_%d", i);
		configfile.setInt32(cfg_key, g_settings.network_nfs[i].automount);
		sprintf(cfg_key, "network_nfs_mount_options1_%d", i);
		configfile.setString(cfg_key, g_settings.network_nfs[i].mount_options1);
		sprintf(cfg_key, "network_nfs_mount_options2_%d", i);
		configfile.setString(cfg_key, g_settings.network_nfs[i].mount_options2);
		sprintf(cfg_key, "network_nfs_type_%d", i);
		configfile.setInt32(cfg_key, g_settings.network_nfs[i].type);
		sprintf(cfg_key, "network_nfs_username_%d", i);
		configfile.setString(cfg_key, g_settings.network_nfs[i].username);
		sprintf(cfg_key, "network_nfs_password_%d", i);
		configfile.setString(cfg_key, g_settings.network_nfs[i].password);
	}

	configfile.setString("network_nfs_audioplayerdir", g_settings.network_nfs_audioplayerdir);
	configfile.setString("network_nfs_moviedir", g_settings.network_nfs_moviedir);
	configfile.setString("network_nfs_picturedir", g_settings.network_nfs_picturedir);
	configfile.setString("network_nfs_recordingdir", g_settings.network_nfs_recordingdir);
	configfile.setString("network_nfs_streamripperdir", g_settings.network_nfs_streamripperdir);

	configfile.setString("downloadcache_dir", g_settings.downloadcache_dir);
	configfile.setString("logo_hdd_dir", g_settings.logo_hdd_dir);

	// recording
	configfile.setInt32("record_hours", g_settings.record_hours);
	configfile.setBool("recording_already_found_check", g_settings.recording_already_found_check);
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	configfile.setInt32("recording_bufsize", g_settings.recording_bufsize);
	configfile.setInt32("recording_bufsize_dmx", g_settings.recording_bufsize_dmx);
#endif
	configfile.setInt32("recording_choose_direct_rec_dir", g_settings.recording_choose_direct_rec_dir);
	configfile.setBool("recording_epg_for_end", g_settings.recording_epg_for_end);
	configfile.setBool("recording_epg_for_filename", g_settings.recording_epg_for_filename);
	configfile.setString("recordingmenu.filename_template", g_settings.recording_filename_template);
	configfile.setBool("recording_save_in_channeldir", g_settings.recording_save_in_channeldir);
	configfile.setInt32("recording_fill_warning", g_settings.recording_fill_warning);
	configfile.setBool("recording_slow_warning", g_settings.recording_slow_warning);
	configfile.setBool("recording_startstop_msg", g_settings.recording_startstop_msg);
	configfile.setBool("recording_stopsectionsd", g_settings.recording_stopsectionsd);
	configfile.setBool("recordingmenu.stream_pmt_pid", g_settings.recording_stream_pmt_pid);
	configfile.setBool("recordingmenu.stream_subtitle_pids", g_settings.recording_stream_subtitle_pids);
	configfile.setBool("recordingmenu.stream_vtxt_pid", g_settings.recording_stream_vtxt_pid);
	configfile.setInt32("recording_type", g_settings.recording_type);
	configfile.setBool("recording_zap_on_announce", g_settings.recording_zap_on_announce);
	configfile.setBool("shutdown_timer_record_type", g_settings.shutdown_timer_record_type);

	configfile.setInt32("recording_audio_pids_default", g_settings.recording_audio_pids_default);

	// streaming
	configfile.setInt32("streaming_ecmmode", g_settings.streaming_ecmmode);
	configfile.setInt32("streaming_decryptmode", g_settings.streaming_decryptmode);
	configfile.setInt32("streaming_port", g_settings.streaming_port);

	// timeshift
	configfile.setString("timeshiftdir", g_settings.timeshiftdir);
	configfile.setInt32("timeshift_auto", g_settings.timeshift_auto);
	configfile.setInt32("timeshift_delete", g_settings.timeshift_delete);
	configfile.setInt32("timeshift_hours", g_settings.timeshift_hours);
	configfile.setInt32("timeshift_pause", g_settings.timeshift_pause);
	configfile.setInt32("timeshift_temp", g_settings.timeshift_temp);

	// ntp server for sectionsd
	configfile.setBool("network_ntpenable", g_settings.network_ntpenable);
	configfile.setString("network_ntpserver", g_settings.network_ntpserver);
	configfile.setString("network_ntprefresh", g_settings.network_ntprefresh);
	configfile.setBool("network_ntpatboot", g_settings.network_ntpatboot);

	// personalize
	for (int i = 0; i < SNeutrinoSettings::P_SETTINGS_MAX; i++) // settings.h, settings.cpp
		configfile.setInt32(personalize_settings[i].personalize_settings_name, g_settings.personalize[i]);
	configfile.setString("personalize_pincode", g_settings.personalize_pincode);

	// widget settings
	configfile.setBool("widget_fade", g_settings.widget_fade);

	// webtv
	CWebChannelsSetup webchannelssetup;
	configfile.setInt32("webtv_xml_auto", g_settings.webtv_xml_auto);
	int webtv_count = 0;
	for (std::list<std::string>::iterator it = g_settings.webtv_xml.begin(); it != g_settings.webtv_xml.end(); ++it)
	{
		std::string k = "webtv_xml_" + to_string(webtv_count);
		if (webchannelssetup.webtv_xml_autodir((*it)))
			continue;
		configfile.setString(k, *it);
		webtv_count++;
	}
	configfile.setInt32("webtv_xml_count", webtv_count);
	configfile.setString("last_webtv_dir", g_settings.last_webtv_dir);

	// webradio
	configfile.setInt32("webradio_xml_auto", g_settings.webradio_xml_auto);
	int webradio_count = 0;
	for (std::list<std::string>::iterator it = g_settings.webradio_xml.begin(); it != g_settings.webradio_xml.end(); ++it)
	{
		std::string k = "webradio_xml_" + to_string(webradio_count);
		if (webchannelssetup.webradio_xml_autodir((*it)))
			continue;
		configfile.setString(k, *it);
		webradio_count++;
	}
	configfile.setInt32("webradio_xml_count", webradio_count);
	configfile.setString("last_webradio_dir", g_settings.last_webradio_dir);

	// xmltv
	int xmltv_count = 0;
	for (std::list<std::string>::iterator it = g_settings.xmltv_xml.begin(); it != g_settings.xmltv_xml.end(); ++it)
	{
		std::string k = "xmltv_xml_" + to_string(xmltv_count);
		configfile.setString(k, *it);
		xmltv_count++;
	}
	configfile.setInt32("xmltv_xml_count", g_settings.xmltv_xml.size());

	configfile.setInt32("livestreamResolution", g_settings.livestreamResolution);
	configfile.setString("livestreamScriptPath", g_settings.livestreamScriptPath);

	// plugins
	configfile.setString("plugin_hdd_dir", g_settings.plugin_hdd_dir);
	configfile.setString("plugins_disabled", g_settings.plugins_disabled);
	configfile.setString("plugins_game", g_settings.plugins_game);
	configfile.setString("plugins_lua", g_settings.plugins_lua);
	configfile.setString("plugins_script", g_settings.plugins_script);
	configfile.setString("plugins_tool", g_settings.plugins_tool);

	// default plugin for movieplayer
	configfile.setString("movieplayer_plugin", g_settings.movieplayer_plugin);

	// screenshot
	configfile.setString("screenshot_dir", g_settings.screenshot_dir);
	configfile.setInt32("screenshot_count", g_settings.screenshot_count);
	configfile.setInt32("screenshot_cover", g_settings.screenshot_cover);
	configfile.setInt32("screenshot_format", g_settings.screenshot_format);
	configfile.setInt32("screenshot_mode", g_settings.screenshot_mode);
	configfile.setInt32("screenshot_scale", g_settings.screenshot_scale);
	configfile.setInt32("screenshot_video", g_settings.screenshot_video);
	configfile.setInt32("auto_cover", g_settings.auto_cover);

	// screen configuration
	configfile.setInt32("osd_resolution", COsdHelpers::getInstance()->g_settings_osd_resolution_save);

	configfile.setInt32("screen_StartX_a_0", g_settings.screen_StartX_a_0);
	configfile.setInt32("screen_StartY_a_0", g_settings.screen_StartY_a_0);
	configfile.setInt32("screen_EndX_a_0", g_settings.screen_EndX_a_0);
	configfile.setInt32("screen_EndY_a_0", g_settings.screen_EndY_a_0);
	configfile.setInt32("screen_StartX_a_1", g_settings.screen_StartX_a_1);
	configfile.setInt32("screen_StartY_a_1", g_settings.screen_StartY_a_1);
	configfile.setInt32("screen_EndX_a_1", g_settings.screen_EndX_a_1);
	configfile.setInt32("screen_EndY_a_1", g_settings.screen_EndY_a_1);

	configfile.setInt32("screen_StartX_b_0", g_settings.screen_StartX_b_0);
	configfile.setInt32("screen_StartY_b_0", g_settings.screen_StartY_b_0);
	configfile.setInt32("screen_EndX_b_0", g_settings.screen_EndX_b_0);
	configfile.setInt32("screen_EndY_b_0", g_settings.screen_EndY_b_0);
	configfile.setInt32("screen_StartX_b_1", g_settings.screen_StartX_b_1);
	configfile.setInt32("screen_StartY_b_1", g_settings.screen_StartY_b_1);
	configfile.setInt32("screen_EndX_b_1", g_settings.screen_EndX_b_1);
	configfile.setInt32("screen_EndY_b_1", g_settings.screen_EndY_b_1);

	configfile.setInt32("screen_preset", g_settings.screen_preset);

	// software update
	configfile.setBool("softupdate_autocheck", g_settings.softupdate_autocheck);
#if ENABLE_PKG_MANAGEMENT
	configfile.setInt32("softupdate_autocheck_packages", g_settings.softupdate_autocheck_packages);
#endif
	configfile.setInt32("softupdate_mode", g_settings.softupdate_mode);
	configfile.setBool("apply_kernel", g_settings.apply_kernel);
	configfile.setBool("apply_settings", g_settings.apply_settings);
	configfile.setString("softupdate_url_file", g_settings.softupdate_url_file);
	configfile.setInt32("softupdate_name_mode_apply", g_settings.softupdate_name_mode_apply);
	configfile.setInt32("softupdate_name_mode_backup", g_settings.softupdate_name_mode_backup);
	configfile.setString("softupdate_proxyserver", g_settings.softupdate_proxyserver);
	configfile.setString("softupdate_proxyusername", g_settings.softupdate_proxyusername);
	configfile.setString("softupdate_proxypassword", g_settings.softupdate_proxypassword);

	configfile.setInt32("flashupdate_createimage_add_env", g_settings.flashupdate_createimage_add_env);
	configfile.setInt32("flashupdate_createimage_add_kernel", g_settings.flashupdate_createimage_add_kernel);
	configfile.setInt32("flashupdate_createimage_add_root1", g_settings.flashupdate_createimage_add_root1);
	configfile.setInt32("flashupdate_createimage_add_spare", g_settings.flashupdate_createimage_add_spare);
	configfile.setInt32("flashupdate_createimage_add_u_boot", g_settings.flashupdate_createimage_add_u_boot);
	configfile.setInt32("flashupdate_createimage_add_uldr", g_settings.flashupdate_createimage_add_uldr);
	configfile.setInt32("flashupdate_createimage_add_var", g_settings.flashupdate_createimage_add_var);

	configfile.setString("backup_dir", g_settings.backup_dir);
	configfile.setString("update_dir", g_settings.update_dir);
	configfile.setString("update_dir_opkg", g_settings.update_dir_opkg);

	// parentallock
	configfile.setInt32("parentallock_prompt", g_settings.parentallock_prompt);
	configfile.setInt32("parentallock_lockage", g_settings.parentallock_lockage);
	configfile.setInt32("parentallock_defaultlocked", g_settings.parentallock_defaultlocked);
	configfile.setString("parentallock_pincode", g_settings.parentallock_pincode);
	configfile.setInt32("parentallock_zaptime", g_settings.parentallock_zaptime);

	// fonts
	configfile.setString("font_file", g_settings.font_file);
	configfile.setString("font_file_monospace", g_settings.font_file_monospace);

	configfile.setInt32("font_scaling_x", g_settings.font_scaling_x);
	configfile.setInt32("font_scaling_y", g_settings.font_scaling_y);

	// online services
#if ENABLE_WEATHER_KEY_MANAGE
	configfile.setString("weather_api_key", g_settings.weather_api_key);
	configfile.setString( "weather_api_version", g_settings.weather_api_version);
#endif
	configfile.setInt32("weather_enabled", g_settings.weather_enabled);

	configfile.setString("weather_city", g_settings.weather_city);
	configfile.setString("weather_location", g_settings.weather_location);
	configfile.setString("weather_postalcode", g_settings.weather_postalcode);

#if ENABLE_YOUTUBE_KEY_MANAGE
	configfile.setString("youtube_api_key", g_settings.youtube_api_key);
#endif
	configfile.setInt32("youtube_enabled", g_settings.youtube_enabled);

#if ENABLE_TMDB_KEY_MANAGE
	configfile.setString("tmdb_api_key", g_settings.tmdb_api_key);
#endif
	configfile.setInt32("tmdb_enabled", g_settings.tmdb_enabled);

#if ENABLE_OMDB_KEY_MANAGE
	configfile.setString("omdb_api_key", g_settings.omdb_api_key);
#endif
	configfile.setInt32("omdb_enabled", g_settings.omdb_enabled);

#if ENABLE_SHOUTCAST_ID_MANAGE
	configfile.setString("shoutcast_dev_id", g_settings.shoutcast_dev_id);
#endif
	configfile.setInt32("shoutcast_enabled", g_settings.shoutcast_enabled);

	// zapit setup
	configfile.setString("startchanneltv", g_settings.StartChannelTV);
	configfile.setString("startchannelradio", g_settings.StartChannelRadio);
	configfile.setInt64("startchanneltv_id", g_settings.startchanneltv_id);
	configfile.setInt64("startchannelradio_id", g_settings.startchannelradio_id);
	configfile.setInt32("uselastchannel", g_settings.uselastchannel);

	// adzap
	configfile.setInt32("adzap_zapBackPeriod", g_settings.adzap_zapBackPeriod);
	configfile.setInt32("adzap_zapOnActivation", g_settings.adzap_zapOnActivation);
	configfile.setInt32("adzap_writeData", g_settings.adzap_writeData);

	// pip
#if ENABLE_PIP
	configfile.setInt32("pip_x", g_settings.pip_x);
	configfile.setInt32("pip_y", g_settings.pip_y);
	configfile.setInt32("pip_width", g_settings.pip_width);
	configfile.setInt32("pip_height", g_settings.pip_height);

	configfile.setInt32("pip_radio_x", g_settings.pip_radio_x);
	configfile.setInt32("pip_radio_y", g_settings.pip_radio_y);
	configfile.setInt32("pip_radio_width", g_settings.pip_radio_width);
	configfile.setInt32("pip_radio_height", g_settings.pip_radio_height);

	configfile.setInt32("pip_rotate_lastpos", g_settings.pip_rotate_lastpos);
#endif

#if ENABLE_QUADPIP
	for (unsigned int i = 0; i < 4; i++)
	{
		std::string qp = "quadpip_channel_window_" + to_string(i);
		configfile.setString(qp, g_settings.quadpip_channel_window[i]);
		qp = "quadpip_channel_id_window_" + to_string(i);
		configfile.setInt64(qp, g_settings.quadpip_channel_id_window[i]);
	}
#endif

	// pictureviewer
	configfile.setInt32("picviewer_scaling", g_settings.picviewer_scaling);
	configfile.setInt32("picviewer_slide_time", g_settings.picviewer_slide_time);

	// audioplayer
	configfile.setInt32("audioplayer_cover_as_screensaver", g_settings.audioplayer_cover_as_screensaver);
	configfile.setInt32("audioplayer_display", g_settings.audioplayer_display);
	configfile.setInt32("audioplayer_enable_sc_metadata", g_settings.audioplayer_enable_sc_metadata);
	configfile.setInt32("audioplayer_follow", g_settings.audioplayer_follow);
	configfile.setInt32("audioplayer_highprio", g_settings.audioplayer_highprio);
	configfile.setInt32("audioplayer_repeat_on", g_settings.audioplayer_repeat_on);
	configfile.setInt32("audioplayer_select_title_by_name", g_settings.audioplayer_select_title_by_name);
	configfile.setInt32("audioplayer_show_playlist", g_settings.audioplayer_show_playlist);
	configfile.setInt32("inetradio_autostart", g_settings.inetradio_autostart);
	configfile.setBool("spectrum", g_settings.spectrum);

	// movieplayer
	configfile.setInt32("movieplayer_bisection_jump", g_settings.movieplayer_bisection_jump);
	configfile.setInt32("movieplayer_display_playtime", g_settings.movieplayer_display_playtime);
	configfile.setInt32("movieplayer_eof_cnt", g_settings.movieplayer_eof_cnt);
	configfile.setInt32("movieplayer_repeat_on", g_settings.movieplayer_repeat_on);
#if HAVE_CST_HARDWARE
	configfile.setBool("movieplayer_select_ac3_atype0", g_settings.movieplayer_select_ac3_atype0);
#endif
	configfile.setInt32("movieplayer_timeosd_while_searching", g_settings.movieplayer_timeosd_while_searching);

	// filebrowser
	configfile.setBool("filebrowser_denydirectoryleave", g_settings.filebrowser_denydirectoryleave);
	configfile.setInt32("filebrowser_showrights", g_settings.filebrowser_showrights);
	configfile.setInt32("filebrowser_sortmethod", g_settings.filebrowser_sortmethod);

	// infoclock
	configfile.setInt32("mode_clock", g_settings.mode_clock);
	configfile.setInt32("infoClockBackground", g_settings.infoClockBackground);
	configfile.setInt32("infoClockFontSize", g_settings.infoClockFontSize);
	configfile.setInt32("infoClockSeconds", g_settings.infoClockSeconds);

	// volume gui
	configfile.setInt32("show_mute_icon", g_settings.show_mute_icon);
	configfile.setBool("volume_digits", g_settings.volume_digits);
	configfile.setInt32("volume_pos", g_settings.volume_pos);
	configfile.setInt32("volume_size", g_settings.volume_size);

	// menu
	configfile.setInt32("menu_pos", g_settings.menu_pos);
	configfile.setBool("show_menu_hints", g_settings.show_menu_hints);
	configfile.setBool("show_menu_hints_line", g_settings.show_menu_hints_line);

	// epgview
	configfile.setInt32("bigFonts", g_settings.bigFonts);

	// eventlist
	configfile.setInt32("eventlist_additional", g_settings.eventlist_additional);
	configfile.setInt32("eventlist_epgplus", g_settings.eventlist_epgplus);

	// channellist
	configfile.setInt32("channellist_additional", g_settings.channellist_additional);
	configfile.setInt32("channellist_epgtext_alignment", g_settings.channellist_epgtext_alignment);
	configfile.setInt32("channellist_foot", g_settings.channellist_foot);
	configfile.setInt32("channellist_new_zap_mode", g_settings.channellist_new_zap_mode);
	configfile.setInt32("channellist_numeric_adjust", g_settings.channellist_numeric_adjust);
	configfile.setInt32("channellist_show_channellogo", g_settings.channellist_show_channellogo);
	configfile.setInt32("channellist_show_eventlogo", g_settings.channellist_show_eventlogo);
	configfile.setInt32("channellist_show_infobox", g_settings.channellist_show_infobox);
	configfile.setInt32("channellist_show_numbers", g_settings.channellist_show_numbers);
	configfile.setInt32("channellist_show_res_icon", g_settings.channellist_show_res_icon);
	configfile.setInt32("channellist_sort_mode", g_settings.channellist_sort_mode);

	// infobar
	configfile.setInt32("infobar_analogclock", g_settings.infobar_analogclock);
	configfile.setBool("infobar_buttons_usertitle", g_settings.infobar_buttons_usertitle);
	configfile.setInt32("infobar_casystem_display", g_settings.infobar_casystem_display);
	configfile.setInt32("infobar_casystem_dotmatrix", g_settings.infobar_casystem_dotmatrix);
	configfile.setInt32("infobar_casystem_frame", g_settings.infobar_casystem_frame);
	configfile.setInt32("infobar_progressbar", g_settings.infobar_progressbar);
	configfile.setBool("infobar_sat_display", g_settings.infobar_sat_display);
	configfile.setInt32("infobar_show", g_settings.infobar_show);
	configfile.setBool("infobar_show_channeldesc", g_settings.infobar_show_channeldesc);
	configfile.setInt32("infobar_show_channellogo", g_settings.infobar_show_channellogo);
	configfile.setInt32("infobar_show_dd_available", g_settings.infobar_show_dd_available);
	configfile.setInt32("infobar_show_res", g_settings.infobar_show_res);
	configfile.setBool("infobar_show_sysfs_hdd", g_settings.infobar_show_sysfs_hdd);
	configfile.setInt32("infobar_show_tuner", g_settings.infobar_show_tuner);
	configfile.setInt32("infobar_subchan_disp_pos", g_settings.infobar_subchan_disp_pos);
	configfile.setInt32("infobar_weather", g_settings.infobar_weather);

	configfile.setBool("scrambled_message", g_settings.scrambled_message);

	// windows
	configfile.setInt32("window_size", g_settings.window_size);
	configfile.setInt32("window_width", g_settings.window_width);
	configfile.setInt32("window_height", g_settings.window_height);

	// osd
	configfile.setBool("osd_colorsettings_advanced_mode", g_settings.osd_colorsettings_advanced_mode);
#ifdef BOXMODEL_CST_HD2
	configfile.setInt32("enable_sd_osd", g_settings.enable_sd_osd);
#endif

	// timing
	for (int i = 0; i < SNeutrinoSettings::TIMING_SETTING_COUNT; i++)
		configfile.setInt32(locale_real_names[timing_setting[i].name], g_settings.timing[i]);

	// timing/handling infobar
	for (int i = 0; i < SNeutrinoSettings::HANDLING_INFOBAR_SETTING_COUNT; i++)
		configfile.setInt32(locale_real_names[handling_infobar_setting[i].name], g_settings.handling_infobar[i]);

	// usermenu
	for (unsigned int i = 0, count = 4; i < g_settings.usermenu.size(); i++)
	{
		if (g_settings.usermenu[i]->key != CRCInput::RC_nokey)
		{
			std::string name;
			if (i < 4)
				name = usermenu_default[i].name;
			else
				name = to_string(count++);
			std::string usermenu_key("usermenu_key_");
			usermenu_key += name;
			configfile.setInt32(usermenu_key, g_settings.usermenu[i]->key);
			std::string txt1("usermenu_tv_");
			txt1 += name;
			configfile.setString(txt1, g_settings.usermenu[i]->items);
			txt1 += "_text";
			configfile.setString(txt1, g_settings.usermenu[i]->title);
		}
	}

	configfile.setString("version_pseudo", g_settings.version_pseudo);

	saveKeys();

	if (strcmp(fname, NEUTRINO_SETTINGS_FILE) || configfile.getModifiedFlag())
		configfile.saveConfig(fname);
}

/**************************************************************************************
*          CNeutrinoApp -  channelsInit, get the Channellist from daemon              *
**************************************************************************************/
extern CBouquetManager *g_bouquetManager;

void CNeutrinoApp::channelsInit(bool bOnly)
{
	CBouquet* tmp;

	printf("[neutrino] Creating channels lists...\n");
	TIMER_START();

	memset(tvsort, -1, sizeof(tvsort));
	memset(radiosort, -1, sizeof(tvsort));

	if(g_bouquetManager && g_bouquetManager->existsUBouquet(DEFAULT_BQ_NAME_FAV, true) == -1)
		g_bouquetManager->addBouquet(DEFAULT_BQ_NAME_FAV, true, true);

	if(TVbouquetList) delete TVbouquetList;
	if(RADIObouquetList) delete RADIObouquetList;

	if(TVfavList) delete TVfavList;
	if(RADIOfavList) delete RADIOfavList;

	if(TVchannelList) delete TVchannelList;
	if(RADIOchannelList) delete RADIOchannelList;

	if(TVwebList) delete TVwebList;
	if(RADIOwebList) delete RADIOwebList;

	TVchannelList = new CChannelList(g_Locale->getText(LOCALE_CHANNELLIST_HEAD), false, true);
	TVbouquetList = new CBouquetList(g_Locale->getText(LOCALE_CHANNELLIST_PROVS));
	TVfavList = new CBouquetList(g_Locale->getText(LOCALE_CHANNELLIST_FAVS));
	TVwebList = new CBouquetList(g_Locale->getText(LOCALE_BOUQUETNAME_WEBTV));

	RADIOchannelList = new CChannelList(g_Locale->getText(LOCALE_CHANNELLIST_HEAD), false, true);
	RADIObouquetList = new CBouquetList(g_Locale->getText(LOCALE_CHANNELLIST_PROVS));
	RADIOfavList = new CBouquetList(g_Locale->getText(LOCALE_CHANNELLIST_FAVS));
	RADIOwebList = new CBouquetList(g_Locale->getText(LOCALE_BOUQUETNAME_WEBRADIO));

	int tvi = 0, ri = 0;

	ZapitChannelList zapitList, webtvList, webradioList;

	/* all TV channels */
	CServiceManager::getInstance()->GetAllTvChannels(zapitList);
	tvi = zapitList.size();
	TVchannelList->SetChannelList(&zapitList);

	/* all RADIO channels */
	CServiceManager::getInstance()->GetAllRadioChannels(zapitList);
	ri = zapitList.size();

	RADIOchannelList->SetChannelList(&zapitList);

	printf("[neutrino] got %d TV and %d RADIO channels\n", tvi, ri); fflush(stdout);
	TIMER_STOP("[neutrino] all channels took");

	/* unless we will do real channel delete from allchans, needed once ? */
	if(!bOnly) {
		if(TVallList) delete TVallList;
		if(RADIOallList) delete RADIOallList;

		TVallList = new CBouquetList(g_Locale->getText(LOCALE_CHANNELLIST_HEAD));
		tmp = TVallList->addBouquet(g_Locale->getText(LOCALE_CHANNELLIST_HEAD));
		tmp->channelList->SetChannelList(&TVchannelList->getChannels());

		RADIOallList = new CBouquetList(g_Locale->getText(LOCALE_CHANNELLIST_HEAD));
		tmp = RADIOallList->addBouquet(g_Locale->getText(LOCALE_CHANNELLIST_HEAD));
		tmp->channelList->SetChannelList(&RADIOchannelList->getChannels());

		if(TVsatList) delete TVsatList;
		TVsatList = new CBouquetList(g_Locale->getText(LOCALE_CHANNELLIST_SATS));
		if(RADIOsatList) delete RADIOsatList;
		RADIOsatList = new CBouquetList(g_Locale->getText(LOCALE_CHANNELLIST_SATS));

		/* all TV / RADIO channels per satellite */
		sat_iterator_t sit;
		satellite_map_t satlist = CServiceManager::getInstance()->SatelliteList();
		for(sit = satlist.begin(); sit != satlist.end(); sit++) {
			if (!CServiceManager::getInstance()->GetAllSatelliteChannels(zapitList, sit->first))
				continue;

			tvi = 0, ri = 0;
			CBouquet* tmp1 = TVsatList->addBouquet(sit->second.name.c_str());
			CBouquet* tmp2 = RADIOsatList->addBouquet(sit->second.name.c_str());
			tmp1->satellitePosition = tmp2->satellitePosition = sit->first;

			for(zapit_list_it_t it = zapitList.begin(); it != zapitList.end(); it++) {
				if ((*it)->getServiceType() == ST_DIGITAL_TELEVISION_SERVICE) {
					tmp1->channelList->addChannel(*it);
					tvi++;
				}
				else if ((*it)->getServiceType() == ST_DIGITAL_RADIO_SOUND_SERVICE) {
					tmp2->channelList->addChannel(*it);
					ri++;
				}
			}
			printf("[neutrino] created %s (%d) bouquet with %d TV and %d RADIO channels\n", sit->second.name.c_str(), sit->first, tvi, ri);
			if(!tvi)
				TVsatList->deleteBouquet(tmp1);
			if(!ri)
				RADIOsatList->deleteBouquet(tmp2);

			TIMER_STOP("[neutrino] sat took");
		}
		/* all WebTV channels */
		if (g_settings.make_webtv_list) {
			if (CServiceManager::getInstance()->GetAllWebTVChannels(webtvList)) {
				/* all channels */
				CBouquet* webtvBouquet = new CBouquet(0, g_Locale->getText(LOCALE_BOUQUETNAME_WEBTV), false, true);
				webtvBouquet->channelList->SetChannelList(&webtvList);
				TVallList->Bouquets.push_back(webtvBouquet);
#if 0
				/* "satellite" */
				webtvBouquet = new CBouquet(0, g_Locale->getText(LOCALE_BOUQUETNAME_WEBTV), false, true);
				webtvBouquet->channelList->SetChannelList(&webtvList);
				TVsatList->Bouquets.push_back(webtvBouquet);
#endif
				printf("[neutrino] got %d WebTV channels\n", (int)webtvList.size()); fflush(stdout);
			}
		}
		/* all WebRadio channels */
		if (g_settings.make_webradio_list) {
			if (CServiceManager::getInstance()->GetAllWebRadioChannels(webradioList)) {
				/* all channels */
				CBouquet* webradioBouquet = new CBouquet(0, g_Locale->getText(LOCALE_BOUQUETNAME_WEBRADIO), false, true);
				webradioBouquet->channelList->SetChannelList(&webradioList);
				RADIOallList->Bouquets.push_back(webradioBouquet);
#if 0
				/* "satellite" */
				webradioBouquet = new CBouquet(0, g_Locale->getText(LOCALE_BOUQUETNAME_WEBRADIO), false, true);
				webradioBouquet->channelList->SetChannelList(&webradioList);
				RADIOsatList->Bouquets.push_back(webradioBouquet);
#endif
				printf("[neutrino] got %d WebRadio channels\n", (int)webradioList.size()); fflush(stdout);
			}
		}
		/* all HD channels */
		if (g_settings.make_hd_list) {
			if (CServiceManager::getInstance()->GetAllHDChannels(zapitList)) {
				CBouquet* hdBouquet = new CBouquet(0, g_Locale->getText(LOCALE_BOUQUETNAME_HDTV), false, true);
				hdBouquet->channelList->SetChannelList(&zapitList);
				TVallList->Bouquets.push_back(hdBouquet);
				printf("[neutrino] got %d HD channels\n", (int)zapitList.size()); fflush(stdout);
			}
			if (CServiceManager::getInstance()->GetAllUHDChannels(zapitList)) {
				CBouquet* uhdBouquet = new CBouquet(0, g_Locale->getText(LOCALE_BOUQUETNAME_UHDTV), false, true);
				uhdBouquet->channelList->SetChannelList(&zapitList);
				TVallList->Bouquets.push_back(uhdBouquet);
				printf("[neutrino] got %d UHD channels\n", (int)zapitList.size()); fflush(stdout);
			}
		}
		/* new channels */
		if (g_settings.make_new_list) {
			if (CServiceManager::getInstance()->GetAllTvChannels(zapitList, CZapitChannel::NEW)) {
				CBouquet* newBouquet = new CBouquet(0, g_Locale->getText(LOCALE_BOUQUETNAME_NEW), false, true);
				newBouquet->channelList->SetChannelList(&zapitList);
				TVallList->Bouquets.push_back(newBouquet);
				printf("[neutrino] got %d new TV channels\n", (int)zapitList.size()); fflush(stdout);
			}
			if (CServiceManager::getInstance()->GetAllRadioChannels(zapitList, CZapitChannel::NEW)) {
				CBouquet* newBouquet = new CBouquet(0, g_Locale->getText(LOCALE_BOUQUETNAME_NEW), false, true);
				newBouquet->channelList->SetChannelList(&zapitList);
				RADIOallList->Bouquets.push_back(newBouquet);
				printf("[neutrino] got %d new RADIO channels\n", (int)zapitList.size()); fflush(stdout);
			}
		}
		/* removed channels */
		if (g_settings.make_removed_list) {
			if (CServiceManager::getInstance()->GetAllTvChannels(zapitList, CZapitChannel::REMOVED)) {
				CBouquet* newBouquet = new CBouquet(0, g_Locale->getText(LOCALE_BOUQUETNAME_REMOVED), false, true);
				newBouquet->channelList->SetChannelList(&zapitList);
				TVallList->Bouquets.push_back(newBouquet);
				printf("[neutrino] got %d removed TV channels\n", (int)zapitList.size()); fflush(stdout);
			}
			if (CServiceManager::getInstance()->GetAllRadioChannels(zapitList, CZapitChannel::REMOVED)) {
				CBouquet* newBouquet = new CBouquet(0, g_Locale->getText(LOCALE_BOUQUETNAME_REMOVED), false, true);
				newBouquet->channelList->SetChannelList(&zapitList);
				RADIOallList->Bouquets.push_back(newBouquet);
				printf("[neutrino] got %d removed RADIO channels\n", (int)zapitList.size()); fflush(stdout);
			}
		}
		TIMER_STOP("[neutrino] sats took");
	}

	delete AllFavBouquetList;
	AllFavBouquetList = new CBouquetList(g_Locale->getText(LOCALE_CHANNELLIST_FAVS));
	/* Favorites and providers bouquets */
	tvi = ri = 0;
	if(g_bouquetManager){
		for (uint32_t i = 0; i < g_bouquetManager->Bouquets.size(); i++) {
			CZapitBouquet *b = g_bouquetManager->Bouquets[i];
			if (!b->bHidden) {
				if (b->getTvChannels(zapitList) || (g_settings.show_empty_favorites && b->bUser)) {
					if(b->bUser)
						tmp = TVfavList->addBouquet(b);
					else if(b->bWebtv)
						tmp = TVwebList->addBouquet(b);
					else
						tmp = TVbouquetList->addBouquet(b);

					tmp->channelList->SetChannelList(&zapitList);
					tvi++;
				}
				if (b->getRadioChannels(zapitList) || (g_settings.show_empty_favorites && b->bUser)) {
					if(b->bUser)
						tmp = RADIOfavList->addBouquet(b);
					else if(b->bWebradio)
						tmp = RADIOwebList->addBouquet(b);
					else
						tmp = RADIObouquetList->addBouquet(b);

					tmp->channelList->SetChannelList(&zapitList);
					ri++;
				}
				if(b->bUser)
					AllFavBouquetList->addBouquet(b);
			}
		}
	}
#if 0
	if (!webtvList.empty()) {
		/* provider */
		CBouquet* webtvBouquet = new CBouquet(0, g_Locale->getText(LOCALE_BOUQUETNAME_WEBTV), false, true);
		webtvBouquet->channelList->SetChannelList(&webtvList);
		TVbouquetList->Bouquets.push_back(webtvBouquet);
	}
#endif
	printf("[neutrino] got %d TV and %d RADIO bouquets\n", tvi, ri); fflush(stdout);
	TIMER_STOP("[neutrino] took");

	SetChannelMode(lastChannelMode);
	CEpgScan::getInstance()->ConfigureEIT();

	dprintf(DEBUG_DEBUG, "\nAll bouquets-channels received\n");
}

void CNeutrinoApp::SetChannelMode(int newmode)
{
	bool isRadioMode = (mode == NeutrinoModes::mode_radio || mode == NeutrinoModes::mode_webradio);

	printf("CNeutrinoApp::SetChannelMode %d [%s]\n", newmode, isRadioMode ? "radio" : "tv");
	int *sortmode;

	if (isRadioMode) {
		channelList = RADIOchannelList;
		g_settings.channel_mode_radio = newmode;
		sortmode = radiosort;
	} else {
		channelList = TVchannelList;
		g_settings.channel_mode = newmode;
		sortmode = tvsort;
	}

	switch(newmode) {
		case LIST_MODE_FAV:
			if (isRadioMode) {
#ifdef ENABLE_GRAPHLCD
				cGLCD::MirrorOSD(false);
#endif
				bouquetList = RADIOfavList;
			} else
				bouquetList = TVfavList;
			break;
		case LIST_MODE_SAT:
			if (isRadioMode) {
#ifdef ENABLE_GRAPHLCD
				cGLCD::MirrorOSD(false);
#endif
				bouquetList = RADIOsatList;
			} else
				bouquetList = TVsatList;
			break;
		case LIST_MODE_WEB:
			if (isRadioMode) {
#ifdef ENABLE_GRAPHLCD
				cGLCD::MirrorOSD(false);
#endif
				bouquetList = RADIOwebList;
			} else
				bouquetList = TVwebList;
			break;
		case LIST_MODE_ALL:
			if (isRadioMode) {
#ifdef ENABLE_GRAPHLCD
				cGLCD::MirrorOSD(false);
#endif
				bouquetList = RADIOallList;
			} else
				bouquetList = TVallList;
			break;
		default:
			newmode = LIST_MODE_PROV;
			/* fall through */
		case LIST_MODE_PROV:
			if (isRadioMode) {
#ifdef ENABLE_GRAPHLCD
				cGLCD::MirrorOSD(false);
#endif
				bouquetList = RADIObouquetList;
			} else
				bouquetList = TVbouquetList;
			break;
	}
	INFO("newmode %d sort old %d new %d", newmode, sortmode[newmode], g_settings.channellist_sort_mode);
	if(newmode != LIST_MODE_FAV && sortmode[newmode] != g_settings.channellist_sort_mode && g_settings.channellist_sort_mode < CChannelList::SORT_MAX) {
		sortmode[newmode] = g_settings.channellist_sort_mode;
		INFO("sorting, mode %d, %d bouquets", g_settings.channellist_sort_mode, (int)bouquetList->Bouquets.size());
		for (uint32_t i = 0; i < bouquetList->Bouquets.size(); i++) {
			if(g_settings.channellist_sort_mode == CChannelList::SORT_ALPHA)
				bouquetList->Bouquets[i]->channelList->SortAlpha();
			if(g_settings.channellist_sort_mode == CChannelList::SORT_TP)
				bouquetList->Bouquets[i]->channelList->SortTP();
			if(g_settings.channellist_sort_mode == CChannelList::SORT_SAT)
				bouquetList->Bouquets[i]->channelList->SortSat();
			if(g_settings.channellist_sort_mode == CChannelList::SORT_CH_NUMBER)
				bouquetList->Bouquets[i]->channelList->SortChNumber();
		}
		adjustToChannelID(CZapit::getInstance()->GetCurrentChannelID());
	}
	lastChannelMode = newmode;
}

/**************************************************************************************
*          CNeutrinoApp -  run, the main runloop                                      *
**************************************************************************************/
extern int cnxt_debug;
extern int sections_debug;
extern int zapit_debug;

void CNeutrinoApp::CmdParser(int argc, char **argv)
{
	global_argv = new char *[argc + 1];
	for (int i = 0; i < argc; i++)
		global_argv[i] = strdup(argv[i]);
	global_argv[argc] = NULL;

	// set default sections debuglevel
	sections_debug = 1; // DEBUG_NORMAL; FIXME: sections_debug is different to neutrino debug

	for (int x = 1; x < argc; x++)
	{
		if (((!strcmp(argv[x], "-v")) || (!strcmp(argv[x], "--verbose"))) && (x + 1 < argc))
		{
			int dl = atoi(argv[x + 1]);
			dprintf(DEBUG_NORMAL, "set debuglevel: %d\n", dl);
			setDebugLevel(dl);
			x++;
		}
		else if ((!strcmp(argv[x], "-xd")))
		{
			cnxt_debug = 1;
		}
		else if ((!strcmp(argv[x], "-sd")))
		{
			int dl = 2; // DEBUG_INFO; FIXME: sections_debug is different to neutrino debug
			if (x + 1 < argc)
			{
				if (!strcmp(argv[x + 1], "0"))
				{
					dl = 0; // DEBUG_ERROR; FIXME: sections_debug is different to neutrino debug
					x++;
				}
				else
				{
					int tmp = atoi(argv[x + 1]);
					if (tmp)
					{
						dl = tmp;
						x++;
					}
				}
			}
			dprintf(DEBUG_NORMAL, "set sections debuglevel: %d\n", dl);
			sections_debug = dl;
		}
		else if ((!strcmp(argv[x], "-zd")))
		{
			zapit_debug = 1;
		}
		else
		{
			dprintf(DEBUG_NORMAL, "Usage: neutrino [-v | --verbose 0..3]\n");
			exit(CNeutrinoApp::EXIT_ERROR);
		}
	}
}

/**************************************************************************************
*          CNeutrinoApp -  setup the framebuffer                                      *
**************************************************************************************/

void CNeutrinoApp::SetupFrameBuffer()
{
	frameBuffer->init();
	int setFbMode = 0;
	osd_resolution_tmp = -1;
#ifdef ENABLE_CHANGE_OSD_RESOLUTION
	frameBuffer->setOsdResolutions();
	if (frameBuffer->osd_resolutions.empty()) {
		dprintf(DEBUG_NORMAL, "Error while setting framebuffer mode\n");
		exit(CNeutrinoApp::EXIT_ERROR);
	}

	uint32_t ort;
	configfile.loadConfig(NEUTRINO_SETTINGS_FILE);
	ort = configfile.getInt32("osd_resolution", 0);

	size_t resCount = frameBuffer->osd_resolutions.size();

	if (ort > (resCount - 1))
		osd_resolution_tmp = ort = 0;

	if (resCount == 1)
		ort = 0;

	setFbMode = frameBuffer->setMode(frameBuffer->osd_resolutions[ort].xRes,
					 frameBuffer->osd_resolutions[ort].yRes,
					 frameBuffer->osd_resolutions[ort].bpp);

/*
	setFbMode = 0;
	COsdHelpers::getInstance()->changeOsdResolution(0, true);
*/
#else
	/* all other hardware ignores setMode parameters */
	setFbMode = frameBuffer->setMode(0, 0, 0);
#endif

	if (setFbMode == -1) {
		dprintf(DEBUG_NORMAL, "Error while setting framebuffer mode\n");
		exit(CNeutrinoApp::EXIT_ERROR);
	}
	frameBuffer->Clear();
	frameBufferInitialized = true;
}

/**************************************************************************************
*          CNeutrinoApp -  setup fonts                                                *
**************************************************************************************/

void CNeutrinoApp::SetupFonts(int fmode)
{
	OnBeforeSetupFonts();
	if (neutrinoFonts == NULL)
		neutrinoFonts = CNeutrinoFonts::getInstance();

	if ((fmode & CNeutrinoFonts::FONTSETUP_NEUTRINO_FONT) == CNeutrinoFonts::FONTSETUP_NEUTRINO_FONT)
		neutrinoFonts->SetupNeutrinoFonts(((fmode & CNeutrinoFonts::FONTSETUP_NEUTRINO_FONT_INST) == CNeutrinoFonts::FONTSETUP_NEUTRINO_FONT_INST));

	if ((fmode & CNeutrinoFonts::FONTSETUP_DYN_FONT) == CNeutrinoFonts::FONTSETUP_DYN_FONT) {
		neutrinoFonts->SetupDynamicFonts(((fmode & CNeutrinoFonts::FONTSETUP_DYN_FONT_INST) == CNeutrinoFonts::FONTSETUP_DYN_FONT_INST));
		neutrinoFonts->refreshDynFonts();
	}

	neutrinoFonts->SetupIconFont();

	/* recalculate infobar position */
	if (g_InfoViewer)
		g_InfoViewer->start();
	OnAfterSetupFonts();
}

/**************************************************************************************
*          CNeutrinoApp -  setup the menu timouts                                     *
**************************************************************************************/

#define LCD_UPDATE_TIME_RADIO_MODE (6 * 1000 * 1000)
#define LCD_UPDATE_TIME_TV_MODE (60 * 1000 * 1000)

void CNeutrinoApp::MakeSectionsdConfig(CSectionsdClient::epg_config& config)
{
	config.epg_cache                = g_settings.epg_cache;
	config.epg_old_events           = g_settings.epg_old_events;
	config.epg_max_events           = g_settings.epg_max_events;
	config.epg_extendedcache        = g_settings.epg_extendedcache;
	config.epg_save_frequently      = g_settings.epg_save ? g_settings.epg_save_frequently : 0;
	config.epg_read_frequently      = g_settings.epg_read ? g_settings.epg_read_frequently : 0;
	config.epg_dir                  = g_settings.epg_dir;
	config.network_ntpserver        = g_settings.network_ntpserver;
	config.network_ntprefresh       = atoi(g_settings.network_ntprefresh.c_str());
	config.network_ntpenable        = g_settings.network_ntpenable;
}

void CNeutrinoApp::SendSectionsdConfig(void)
{
	CSectionsdClient::epg_config config;
	MakeSectionsdConfig(config);
	g_Sectionsd->setConfig(config);
}

void CNeutrinoApp::InitZapper()
{
	struct stat my_stat;

	g_InfoViewer->start();
	SendSectionsdConfig();
	if (g_settings.epg_read) {
		if(stat(g_settings.epg_dir.c_str(), &my_stat) == 0)
			g_Sectionsd->readSIfromXML(g_settings.epg_dir.c_str());
	}
	int tvmode = CZapit::getInstance()->getMode() & CZapitClient::MODE_TV;
	lastChannelMode = tvmode ? g_settings.channel_mode : g_settings.channel_mode_radio;
	mode = tvmode ? NeutrinoModes::mode_tv : NeutrinoModes::mode_radio;
	lastMode = mode;

	SDTreloadChannels = false;
	channelsInit();

	if(tvmode)
		tvMode(true);
	else
		radioMode(true);

	if(g_settings.cacheTXT)
		tuxtxt_init();

	t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
	if(channelList->getSize() && live_channel_id  && !IS_WEBCHAN(live_channel_id))
		g_Sectionsd->setServiceChanged(live_channel_id, false);
}

void CNeutrinoApp::setupRecordingDevice(void)
{
	CRecordManager::getInstance()->SetDirectory(g_settings.network_nfs_recordingdir);
	CRecordManager::getInstance()->Config(g_settings.recording_stopsectionsd, g_settings.recording_stream_vtxt_pid, g_settings.recording_stream_pmt_pid, g_settings.recording_stream_subtitle_pids);
}

static void CSSendMessage(uint32_t msg, uint32_t data)
{
	if (g_RCInput)
		g_RCInput->postMsg(msg, data);
}

void CNeutrinoApp::InitTimerdClient()
{
	g_Timerd = new CTimerdClient;
	g_Timerd->registerEvent(CTimerdClient::EVT_ANNOUNCE_SHUTDOWN, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_SHUTDOWN, 222, NEUTRINO_UDS_NAME);
#if 0
	g_Timerd->registerEvent(CTimerdClient::EVT_ANNOUNCE_NEXTPROGRAM, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_NEXTPROGRAM, 222, NEUTRINO_UDS_NAME);
#endif
	g_Timerd->registerEvent(CTimerdClient::EVT_STANDBY_ON, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_STANDBY_OFF, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_ANNOUNCE_RECORD, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_RECORD_START, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_RECORD_STOP, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_ANNOUNCE_ZAPTO, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_ZAPTO, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_SLEEPTIMER, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_ANNOUNCE_SLEEPTIMER, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_REMIND, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_EXEC_PLUGIN, 222, NEUTRINO_UDS_NAME);
}

void CNeutrinoApp::InitZapitClient()
{
	g_Zapit         = new CZapitClient;
#define ZAPIT_EVENT_COUNT 29
	const CZapitClient::events zapit_event[ZAPIT_EVENT_COUNT] =
	{
		CZapitClient::EVT_ZAP_COMPLETE,
		CZapitClient::EVT_ZAP_COMPLETE_IS_NVOD,
		CZapitClient::EVT_ZAP_FAILED,
		CZapitClient::EVT_ZAP_SUB_COMPLETE,
		CZapitClient::EVT_ZAP_SUB_FAILED,
		CZapitClient::EVT_ZAP_MOTOR,
		CZapitClient::EVT_ZAP_CA_ID,
		CZapitClient::EVT_RECORDMODE_ACTIVATED,
		CZapitClient::EVT_RECORDMODE_DEACTIVATED,
		CZapitClient::EVT_SCAN_COMPLETE,
		CZapitClient::EVT_SCAN_FAILED,
		CZapitClient::EVT_SCAN_NUM_TRANSPONDERS,
		CZapitClient::EVT_SCAN_REPORT_NUM_SCANNED_TRANSPONDERS,
		CZapitClient::EVT_SCAN_REPORT_FREQUENCYP,
		CZapitClient::EVT_SCAN_SATELLITE,
		CZapitClient::EVT_SCAN_NUM_CHANNELS,
		CZapitClient::EVT_SCAN_PROVIDER,
		CZapitClient::EVT_BOUQUETS_CHANGED,
		CZapitClient::EVT_SERVICES_CHANGED,
		CZapitClient::EVT_SCAN_SERVICENAME,
		CZapitClient::EVT_SCAN_FOUND_TV_CHAN,
		CZapitClient::EVT_SCAN_FOUND_RADIO_CHAN,
		CZapitClient::EVT_SCAN_FOUND_DATA_CHAN,
		CZapitClient::EVT_SDT_CHANGED,
		CZapitClient::EVT_PMT_CHANGED,
		CZapitClient::EVT_TUNE_COMPLETE,
		CZapitClient::EVT_BACK_ZAP_COMPLETE,
		CZapitClient::EVT_WEBTV_ZAP_COMPLETE,
	};

	for (int i = 0; i < ZAPIT_EVENT_COUNT; i++)
		g_Zapit->registerEvent(zapit_event[i], 222, NEUTRINO_UDS_NAME);
}

void CNeutrinoApp::InitSectiondClient()
{
#if 0
	/* TODO: check if still needed */
	/* wait for sectionsd to be able to process our registration */
	int64_t t = time_monotonic_ms();
	while (! sectionsd_isReady())
		sleep(0);
	dprintf(DEBUG_NORMAL, "had to wait %" PRId64 " ms for sectionsd to start up\n", time_monotonic_ms() - t);
#endif
	g_Sectionsd = new CSectionsdClient;
	struct timespec t;
	if (clock_gettime(CLOCK_MONOTONIC, &t)) {
		dprintf(DEBUG_NORMAL, "CLOCK_MONOTONIC not supported? (%m), falling back to EVT_TIMESET\n");
		g_Sectionsd->registerEvent(CSectionsdClient::EVT_TIMESET, 222, NEUTRINO_UDS_NAME);
	}
	g_Sectionsd->registerEvent(CSectionsdClient::EVT_GOT_CN_EPG, 222, NEUTRINO_UDS_NAME);
	g_Sectionsd->registerEvent(CSectionsdClient::EVT_EIT_COMPLETE, 222, NEUTRINO_UDS_NAME);
	g_Sectionsd->registerEvent(CSectionsdClient::EVT_WRITE_SI_FINISHED, 222, NEUTRINO_UDS_NAME);
	g_Sectionsd->registerEvent(CSectionsdClient::EVT_RELOAD_XMLTV, 222, NEUTRINO_UDS_NAME);
}

#if HAVE_CST_HARDWARE
#include <cs_frontpanel.h>
#endif

bool is_wakeup()
{
	bool wakeup = false;

#if HAVE_CST_HARDWARE
#ifndef FP_IOCTL_CLEAR_WAKEUP_TIMER
#define FP_IOCTL_CLEAR_WAKEUP_TIMER 10
#endif

#define FP_IOCTL_SET_RTC         0x101
#define FP_IOCTL_GET_RTC         0x102

	int fd = open("/dev/display", O_RDONLY);
	if (fd < 0) {
		perror("/dev/display");
	} else {
		fp_wakeup_data_t wk;
		memset(&wk, 0, sizeof(wk));
		int ret = ioctl(fd, IOC_FP_GET_WAKEUP, &wk);
		if(ret >= 0)
			wakeup = ((wk.source == FP_WAKEUP_SOURCE_TIMER) /* || (wk.source == WAKEUP_SOURCE_PWLOST)*/);
		close(fd);
	}
#endif

	/* prioritize proc filesystem */
	if (access("/proc/stb/fp/was_timer_wakeup", F_OK) == 0)
	{
		FILE *f = fopen("/proc/stb/fp/was_timer_wakeup", "r");
		if (f)
		{
			unsigned int tmp;
			if (fscanf(f, "%u", &tmp) != 1)
				printf("[neutrino] read /proc/stb/fp/was_timer_wakeup failed: %m\n");
			else
				wakeup = (tmp > 0);
			fclose(f);
		}
	}
	/* not platform specific - this is created by the init process */
	else if (access("/tmp/.timer_wakeup", F_OK) == 0)
	{
		wakeup = 1;
		unlink("/tmp/.timer_wakeup");
	}
	printf("[timerd] wakeup from standby: %s\n", wakeup ? "yes" : "no");

	if (!wakeup)
		exec_controlscript(NEUTRINO_LEAVE_DEEPSTANDBY_SCRIPT);

	return wakeup;
}

int CNeutrinoApp::run(int argc, char **argv)
{
	neutrino_start_time = time_monotonic();

	exec_controlscript(NEUTRINO_APP_START_SCRIPT);

	CmdParser(argc, argv);

TIMER_START();
	cs_api_init();
	cs_register_messenger(CSSendMessage);
#if defined(HAVE_CST_HARDWARE) && defined(ENABLE_CHANGE_OSD_RESOLUTION)
	cs_new_auto_videosystem();
#endif

	g_info.hw_caps = get_hwcaps();

	g_Locale        = new CLocaleManager;

	int loadSettingsErg = loadSetup(NEUTRINO_SETTINGS_FILE);

	initialize_iso639_map();

	bool show_startwizard = false;
	CLocaleManager::loadLocale_ret_t loadLocale_ret = g_Locale->loadLocale(g_settings.language.c_str());
	if (loadLocale_ret == CLocaleManager::NO_SUCH_LOCALE)
	{
		g_settings.language = "deutsch"; //NI
		loadLocale_ret = g_Locale->loadLocale(g_settings.language.c_str());
		show_startwizard = true;
	}

	// default usermenu titles correspond to gui/user_menue_setup.h:struct usermenu_props_t usermenu
	if (g_settings.usermenu[0]->title.empty() && !g_settings.usermenu[0]->items.empty())
		g_settings.usermenu[0]->title = g_Locale->getText(LOCALE_USERMENU_TITLE_RED);
	if (g_settings.usermenu[1]->title.empty() && !g_settings.usermenu[1]->items.empty())
		g_settings.usermenu[1]->title = g_Locale->getText(LOCALE_USERMENU_TITLE_GREEN);
	if (g_settings.usermenu[2]->title.empty() && !g_settings.usermenu[2]->items.empty())
		g_settings.usermenu[2]->title = g_Locale->getText(LOCALE_USERMENU_TITLE_YELLOW);
	if (g_settings.usermenu[3]->title.empty() && !g_settings.usermenu[3]->items.empty())
		g_settings.usermenu[3]->title = g_Locale->getText(LOCALE_USERMENU_TITLE_BLUE);

	/* setup GUI */
	neutrinoFonts = CNeutrinoFonts::getInstance();
	SetupFonts();
	g_PicViewer = new CPictureViewer();
	CColorSetupNotifier::setPalette();

	char start_text [100];
	snprintf(start_text, sizeof(start_text), g_Locale->getText(LOCALE_NEUTRINO_STARTING), PACKAGE_NAME, PACKAGE_VERSION );
	start_text[99] = '\0';
	CHintBox * hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, start_text);

	// show startlogo
	bool startlogo = false;
	if (!show_startwizard)
		startlogo = true;
	else
		hintBox->paint();

	CVFD::getInstance()->init(neutrinoFonts->fontDescr.filename.c_str(), neutrinoFonts->fontDescr.name.c_str());
	CVFD::getInstance()->Clear();
	CVFD::getInstance()->ShowText(g_Locale->getText(LOCALE_NI)); //NI
	CVFD::getInstance()->setBacklight(g_settings.backlight_tv);
	CVFD::getInstance()->setScrollMode(g_settings.lcd_scroll);

#ifdef ENABLE_GRAPHLCD
	cGLCD::getInstance();
#endif

	if (!scanSettings.loadSettings(NEUTRINO_SCAN_SETTINGS_FILE))
		dprintf(DEBUG_NORMAL, "Loading of scan settings failed. Using defaults.\n");

	CFileHelpers::getInstance()->removeDir(COVERDIR_TMP);
	CFileHelpers::getInstance()->removeDir(LOGODIR_TMP);

	/* set service manager options before starting zapit */
	CServiceManager::getInstance()->KeepNumbers(g_settings.keep_channel_numbers);
	//zapit start parameters
	Z_start_arg ZapStart_arg;
	ZapStart_arg.startchanneltv_id = g_settings.startchanneltv_id;
	ZapStart_arg.startchannelradio_id = g_settings.startchannelradio_id;
	ZapStart_arg.uselastchannel = g_settings.uselastchannel;
	ZapStart_arg.video_mode = g_settings.video_Mode;
	memcpy(ZapStart_arg.ci_clock, g_settings.ci_clock, sizeof(g_settings.ci_clock));
#if BOXMODEL_VUPLUS_ALL
	ZapStart_arg.ci_delay = g_settings.ci_delay;
	memcpy(ZapStart_arg.ci_rpr, g_settings.ci_rpr, sizeof(g_settings.ci_rpr));
#endif
	memcpy(ZapStart_arg.ci_op, g_settings.ci_op, sizeof(g_settings.ci_op));
	ZapStart_arg.volume = g_settings.current_volume;
	ZapStart_arg.webtv_xml = &g_settings.webtv_xml;
	ZapStart_arg.webradio_xml = &g_settings.webradio_xml;

	ZapStart_arg.osd_resolution = g_settings.osd_resolution;

	CCamManager::getInstance()->SetCITuner(g_settings.ci_tuner);
	/* create decoders, read channels */
	bool zapit_init = CZapit::getInstance()->Start(&ZapStart_arg);
	//get zapit config for writeChannelsNames
	CZapit::getInstance()->GetConfig(zapitCfg);

	// init audio settings
	audioDecoder->SetSRS(g_settings.srs_enable, g_settings.srs_nmgr_enable, g_settings.srs_algo, g_settings.srs_ref_volume);
	//audioDecoder->setVolume(g_settings.current_volume, g_settings.current_volume);
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	audioDecoder->SetHdmiDD(g_settings.ac3_pass ? true : false);
	audioDecoder->SetSpdifDD(g_settings.dts_pass ? true : false);
#else
	audioDecoder->SetHdmiDD((HDMI_ENCODED_MODE)g_settings.hdmi_dd);
	audioDecoder->SetSpdifDD(g_settings.spdif_dd ? true : false);
#endif
	audioDecoder->EnableAnalogOut(g_settings.analog_out ? true : false);
	audioSetupNotifier        = new CAudioSetupNotifier;
	// trigger a change
	if(g_settings.avsync != (AVSYNC_TYPE) AVSYNC_ENABLED)
		audioSetupNotifier->changeNotify(LOCALE_AUDIOMENU_AVSYNC, NULL);

	//init video settings
	g_videoSettings = new CVideoSettings;
	g_videoSettings->initVideoSettings();

	// reset videodecoder to fullscreen
	videoDecoder->Pig(-1, -1, -1, -1);

	// show startlogo
	if (startlogo)
		startlogo = frameBuffer->showFrame("start.jpg");

	g_RCInput = new CRCInput();

	InitZapitClient();
	g_Zapit->setStandby(false);

	CheckFastScan();

	// dirty part of hw_caps - specify some details after zapit start
	if (strcmp(g_info.hw_caps->boxname, "HD1") == 0)
	{
		// only SAT-HD1 has fan
		if (!CFEManager::getInstance()->getFE(0)->hasSat())
			g_info.hw_caps->has_fan = 0;
	}
	if (strcmp(g_info.hw_caps->boxname, "Neo") == 0)
	{
		// detecting Neo Twin by counting frontends
		if (CFEManager::getInstance()->getFrontendCount() > 1)
			strcpy(g_info.hw_caps->boxname, "Neo Twin");
	}

	//timer start
	timer_wakeup = (is_wakeup() && g_settings.shutdown_timer_record_type);
	g_settings.shutdown_timer_record_type = false;

	init_cec_setting = true;
	if(!(timer_wakeup && g_settings.hdmi_cec_mode))
	{
		//init cec settings
		CCECSetup cecsetup;
		cecsetup.setCECSettings();
		init_cec_setting = false;
	}

	long timerd_signal = timer_wakeup;
	pthread_create (&timer_thread, NULL, timerd_main_thread, (void *)&timerd_signal);
	timerd_thread_started = true;

	powerManager = new cPowerManager;
	powerManager->Open();

	cpuFreq = g_info.hw_caps->can_cpufreq ? new cCpuFreqManager() : NULL;
	if (cpuFreq)
		cpuFreq->SetCpuFreq(g_settings.cpufreq * 1000 * 1000);

	//fan speed
	if (g_info.hw_caps->has_fan)
		CFanControlNotifier::setSpeed(g_settings.fan_speed);

	dvbsub_init();

	pthread_t nhttpd_thread;
	if (!pthread_create (&nhttpd_thread, NULL, nhttpd_main_thread, (void *) NULL))
		pthread_detach (nhttpd_thread);

	CStreamManager::getInstance()->Start();

#ifndef DISABLE_SECTIONSD
	CSectionsdClient::epg_config config;
	MakeSectionsdConfig(config);
	CEitManager::getInstance()->SetConfig(config);
	CEitManager::getInstance()->Start();
#endif

	g_RemoteControl = new CRemoteControl;
	g_EpgData = new CEpgData;
	InfoClock = CInfoClock::getInstance();
	//NI InfoIcons
	InfoIcons = CInfoIcons::getInstance();
	if(g_settings.mode_icons)
		InfoIcons->StartIcons();
	FileTimeOSD = CTimeOSD::getInstance();
	g_InfoViewer = new CInfoViewer;
	g_EventList = new CEventList;

	g_CamHandler = new CCAMMenuHandler();
	g_CamHandler->init();

	/* later on, we'll crash anyway, so tell about it. */
	if (! zapit_init)
		DisplayErrorMessage("Zapit initialization failed. This is a fatal error, sorry.");

#ifndef ASSUME_MDEV
	mkdir("/media/sda1", 0755);
	mkdir("/media/sdb1", 0755);
	my_system(3, "mount", "/dev/sda1", "/media/sda1");
	my_system(3, "mount", "/dev/sdb1", "/media/sdb1");
#endif

	CFSMounter::automount();
	g_Plugins = new CPlugins;
	g_Plugins->setPluginDir(PLUGINDIR);
	//load Pluginlist before main menu (only show script menu if at least one script is available
	g_Plugins->loadPlugins();

	// setup recording device
	setupRecordingDevice();

	dprintf( DEBUG_NORMAL, "menue setup\n");
	//init Menues
	InitMenu();

	dprintf( DEBUG_NORMAL, "registering as event client\n");

	InitSectiondClient();

	/* wait until timerd is ready... */
	int64_t timerd_wait = time_monotonic_ms();
	while (timerd_signal >= 0) {
		usleep(100);
	}
	dprintf(DEBUG_NORMAL, "had to wait %" PRId64 " ms for timerd start...\n", time_monotonic_ms() - timerd_wait);
	InitTimerdClient();

	// volume
	if (g_settings.show_mute_icon && g_settings.current_volume == 0)
		current_muted = true;

	g_volume = CVolume::getInstance();
	g_audioMute = CAudioMute::getInstance();

	g_audioMute->AudioMute(current_muted, true);
	CZapit::getInstance()->SetVolumePercent(g_settings.audio_volume_percent_ac3, g_settings.audio_volume_percent_pcm);
	CVFD::getInstance()->setMuted(current_muted);
	if (g_info.hw_caps->display_has_statusline)
		CVFD::getInstance()->showVolume(g_settings.current_volume, false);
	else
		CVFD::getInstance()->setVolume(g_settings.current_volume);


#ifdef ENABLE_GRAPHLCD
	if (current_muted)
		cGLCD::lockIcon(cGLCD::MUTE);
	else
		cGLCD::unlockIcon(cGLCD::MUTE);
#endif

	if (show_startwizard) {
		hintBox->hide();
		CStartUpWizard startwizard;
		startwizard.exec(NULL, "");
	}

	// clean up startlogo
	if (startlogo)
	{
		startlogo = false;
		sleep(3);
		frameBuffer->stopFrame();
	}

	if(loadSettingsErg) {
		hintBox->hide();
		dprintf(DEBUG_INFO, "config file or options missing\n");
		ShowHint(LOCALE_MESSAGEBOX_INFO, loadSettingsErg ==  1 ? g_Locale->getText(LOCALE_SETTINGS_NOCONFFILE)
				: g_Locale->getText(LOCALE_SETTINGS_MISSINGOPTIONSCONFFILE));
		configfile.setModifiedFlag(true);
		saveSetup(NEUTRINO_SETTINGS_FILE);
	}

	InitZapper();

	CHDDDestExec * hdd = new CHDDDestExec();
	hdd->exec(NULL, "");
	delete hdd;

	hintBox->hide(); // InitZapper also displays a hintbox
	delete hintBox;

	cCA::GetInstance()->Ready(true);
#if HAVE_LIBSTB_HAL
	cCA::GetInstance()->setCheckLiveSlot(g_settings.ci_check_live);
#endif
	//InitZapper();

#if HAVE_ARM_HARDWARE
	CPSISetup::getInstance()->blankScreen(false);
#endif
	SHTDCNT::getInstance()->init();

#ifdef ENABLE_LCD4LINUX
	if (g_settings.lcd4l_support)
		CLCD4l::getInstance()->StartLCD4l();
#endif

	CZapit::getInstance()->SetScanSDT(g_settings.enable_sdt);

	cSysLoad::getInstance();
	cHddStat::getInstance();

	//NI create userdirs
	CFileHelpers::createDir(CONTROLDIR_VAR);
	CFileHelpers::createDir(FONTDIR_VAR);
	CFileHelpers::createDir(ICONSDIR_VAR);
	CFileHelpers::createDir(LOGODIR_VAR);
	CFileHelpers::createDir(LOGODIR_VAR "/events");
	CFileHelpers::createDir(LOCALEDIR_VAR);
	CFileHelpers::createDir(THEMESDIR_VAR);
	CFileHelpers::createDir(PLUGINDIR_VAR);
	CFileHelpers::createDir(LUAPLUGINDIR_VAR);
	CFileHelpers::createDir(WEBRADIODIR_VAR);
	CFileHelpers::createDir(WEBTVDIR_VAR);
	CFileHelpers::createDir(PUBLIC_HTTPDDIR);
	CFileHelpers::createDir(PUBLIC_HTTPDDIR "/logo");

	CWeather::getInstance()->setCoords(g_settings.weather_location, g_settings.weather_city);

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	videoDecoder->SetControl(VIDEO_CONTROL_ZAPPING_MODE, g_settings.zappingmode);
	videoDecoder->SetHDMIColorimetry((HDMI_COLORIMETRY) g_settings.hdmi_colorimetry);
#endif

TIMER_STOP("################################## after all ##################################");

	if (g_settings.softupdate_autocheck)
		CFlashUpdateCheck::getInstance()->startThread();
#if ENABLE_PKG_MANAGEMENT
	if (g_settings.softupdate_autocheck_packages)
		CUpdateCheckPackages::getInstance()->startThread();
#endif

	xmltv_xml_readepg();
	xmltv_xml_auto_readepg();

#if ENABLE_PIP && (BOXMODEL_HD51 || BOXMODEL_BRE2ZE4K || BOXMODEL_H7 || BOXMODEL_E4HDULTRA || BOXMODEL_PROTEK4K)
	if (g_info.hw_caps->can_pip)
	{
		CZapit::getInstance()->OpenPip(0);
		if (pipVideoDecoder[0])
			pipVideoDecoder[0]->Pig(pip_recalc_pos_x(g_settings.pip_x), pip_recalc_pos_y(g_settings.pip_y), g_settings.pip_width, g_settings.pip_height, frameBuffer->getScreenWidth(true), frameBuffer->getScreenHeight(true));
		usleep(100);
		CZapit::getInstance()->StopPip(0);
	}
#endif

	RealRun();
	ExitRun(g_info.hw_caps->can_shutdown);

	return 0;
}

void CNeutrinoApp::quickZap(int msg)
{
	int res;

	StopSubtitles();
	bool ret;
	if(!bouquetList->Bouquets.empty())
		ret = bouquetList->Bouquets[bouquetList->getActiveBouquetNumber()]->channelList->quickZap(msg, g_settings.zap_cycle);
	else
		ret = channelList->quickZap(msg);
	if (!ret) {
		res = channelList->showLiveBouquet(g_settings.key_zaphistory);
		StartSubtitles(res < 0);
	}
}

void CNeutrinoApp::numericZap(int msg)
{
	StopSubtitles();
	int res = channelList->numericZap(msg);
	StartSubtitles(res < 0);
	if (res >= 0 && CRCInput::isNumeric(msg)) {
		if (g_settings.channellist_numeric_adjust && first_mode_found >= 0) {
			SetChannelMode(first_mode_found);
			channelList->getLastChannels().set_mode(channelList->getActiveChannel_ChannelID());
		}
	}
}

void CNeutrinoApp::showInfo()
{
	StopSubtitles();
	g_InfoViewer->showTitle(channelList->getActiveChannel());
	StartSubtitles();
}

void CNeutrinoApp::showMainMenu()
{
	StopSubtitles();
	InfoClock->enableInfoClock(false);
	InfoIcons->enableInfoIcons(false); //NI InfoIcons
	int old_ttx = g_settings.cacheTXT;
	int old_epg = g_settings.epg_scan;
	int old_mode = g_settings.epg_scan_mode;
	int old_save_mode = g_settings.epg_save_mode;
	mainMenu->exec(NULL, "");
	CVFD::getInstance()->UpdateIcons();
	InfoClock->enableInfoClock(true);
	InfoIcons->enableInfoIcons(true); //NI InfoIcons
	StartSubtitles();
	saveSetup(NEUTRINO_SETTINGS_FILE);

	if (old_save_mode != g_settings.epg_save_mode)
		CEpgScan::getInstance()->ConfigureEIT();
	if (old_epg != g_settings.epg_scan || old_mode != g_settings.epg_scan_mode) {
		if (g_settings.epg_scan_mode != CEpgScan::MODE_OFF)
			CEpgScan::getInstance()->Start();
		else
			CEpgScan::getInstance()->Clear();
	}
	if (old_ttx != g_settings.cacheTXT) {
		if(g_settings.cacheTXT) {
			tuxtxt_init();
		} else
			tuxtxt_close();
	}
}

void CNeutrinoApp::RealRun()
{
	mainMenu = &personalize.getWidget(MENU_MAIN);

	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	dprintf(DEBUG_NORMAL, "initialized everything\n");

	if (g_settings.power_standby || init_cec_setting || file_exists(FLAGDIR "/.power_standby"))
	{
		unlink(FLAGDIR "/.power_standby");
		standbyMode(true, true);
	}

	//cCA::GetInstance()->Ready(true);
#ifdef ENABLE_LUA
	CLuaServer *luaServer = CLuaServer::getInstance();
#endif
	g_Plugins->startPlugin("startup");
	if (!g_Plugins->getScriptOutput().empty()) {
		ShowMsg(LOCALE_PLUGINS_RESULT, g_Plugins->getScriptOutput(), CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_SHELL);
	}
	g_RCInput->clearRCMsg();

	CScreenSaver::getInstance()->resetIdleTime();

	while( true ) {
#ifdef ENABLE_LUA
		luaServer->UnBlock();
#endif
		g_RCInput->getMsg(&msg, &data, 100, ((g_settings.mode_left_right_key_tv == SNeutrinoSettings::VOLUME) && (g_RemoteControl->subChannels.size() < 1)) ? true : false);	// 10 secs..
#ifdef ENABLE_LUA
		if (luaServer->Block(msg, data))
			continue;
#endif

		if (msg <= CRCInput::RC_MaxRC)
			CScreenSaver::getInstance()->resetIdleTime();

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
		if (mode == NeutrinoModes::mode_radio)
#else
		if (mode == NeutrinoModes::mode_radio || mode == NeutrinoModes::mode_webradio)
#endif
		{
			if (msg == CRCInput::RC_timeout || msg == NeutrinoMessages::EVT_TIMER)
			{
				if (!blank_screen && CScreenSaver::getInstance()->canStart() && !CScreenSaver::getInstance()->isActive())
				{
					CScreenSaver::getInstance()->Start();
				}
			}
			else if (!CScreenSaver::getInstance()->ignoredMsg(msg))
			{
				if (CScreenSaver::getInstance()->isActive())
				{
					printf("[neutrino] CScreenSaver stop; msg: %lX\n", msg);
					CScreenSaver::getInstance()->Stop();

					frameBuffer->stopFrame();
					frameBuffer->showFrame("radiomode.jpg");

					if (msg <= CRCInput::RC_MaxRC)
					{
						// ignore first keypress - just quit the screensaver
						g_RCInput->clearRCMsg();
						continue;
					}
				}
			}
		}

		if( ( mode == NeutrinoModes::mode_tv ) || ( mode == NeutrinoModes::mode_radio ) || ( mode == NeutrinoModes::mode_webtv ) || ( mode == NeutrinoModes::mode_webradio ) ) {
			//NI
			if (blank_screen) {
				if (!videoDecoder->getBlank()) {
					INFO("blank_screen auto off");
					blank_screen = false;
					if (mode == NeutrinoModes::mode_radio || mode == NeutrinoModes::mode_webradio)
						frameBuffer->showFrame("radiomode.jpg");
				}
				else if (msg <= CRCInput::RC_MaxRC) {
					INFO("blank_screen manual off");
					blank_screen = false;
					videoDecoder->setBlank(blank_screen);
					if (mode == NeutrinoModes::mode_radio || mode == NeutrinoModes::mode_webradio)
						frameBuffer->showFrame("radiomode.jpg");
					//eat key - just leave blank screen
					g_RCInput->clearRCMsg();
					continue;
				}
			}
			if (msg == NeutrinoMessages::SHOW_EPG) {
				InfoClock->enableInfoClock(false);
				InfoIcons->enableInfoIcons(false); //NI InfoIcons
				StopSubtitles();
				t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
				g_EpgData->show(live_channel_id);
				InfoClock->enableInfoClock(true);
				InfoIcons->enableInfoIcons(true); //NI InfoIcons
				StartSubtitles();
			}
			/* the only hardcoded key to check before key bindings */
			else if( msg == CRCInput::RC_setup ) {
				if(!g_settings.minimode) {
					showMainMenu();
				}
			}
			else if (msg == (neutrino_msg_t) g_settings.key_favorites)
			{
				showChannelList(msg);
			}
			else if( ( msg == (neutrino_msg_t) g_settings.key_quickzap_up ) || ( msg == (neutrino_msg_t) g_settings.key_quickzap_down ) )
			{
				quickZap(msg);
			}
			else if( msg == (neutrino_msg_t) g_settings.key_tvradio_mode ) {
				switchTvRadioMode();
			}
			else if( msg == (neutrino_msg_t) g_settings.key_subchannel_up || msg == (neutrino_msg_t) g_settings.key_subchannel_down) {
				if( !g_RemoteControl->subChannels.empty() ) {
					StopSubtitles();
					if( msg == (neutrino_msg_t) g_settings.key_subchannel_up )
						g_RemoteControl->subChannelUp();
					else if( msg == (neutrino_msg_t) g_settings.key_subchannel_down )
						g_RemoteControl->subChannelDown();
					g_InfoViewer->showSubchan();
				}
				else if ( msg == CRCInput::RC_left || msg == CRCInput::RC_right) {
					switch (g_settings.mode_left_right_key_tv)
					{
						case SNeutrinoSettings::INFOBAR:
						case SNeutrinoSettings::VZAP:
							if (channelList->getSize())
								showInfo();
							break;
						case SNeutrinoSettings::VOLUME:
							g_volume->setVolume(msg);
							break;
						default: /* SNeutrinoSettings::ZAP */
							quickZap(msg);
							break;
					}
				}
				else
					quickZap( msg );
			}
			else if (msg == (neutrino_msg_t) g_settings.key_lastchannel) {
				numericZap(msg);
			}
			else if (msg == (neutrino_msg_t) g_settings.key_zaphistory || msg == (neutrino_msg_t) g_settings.key_current_transponder) {
				//NI InfoIcons; hide, if one of the keys above is assigned to RC_home or RC_back
				if (backKey(msg))
				{
					if (g_settings.mode_icons && g_settings.mode_icons_skin == INFOICONS_POPUP)
						InfoIcons->hideIcons();
				}
				showChannelList(msg);
			}
#ifdef SCREENSHOT
			else if (msg == (neutrino_msg_t) g_settings.key_screenshot) {
				for(int i = 0; i < g_settings.screenshot_count; i++) {
					CScreenShot * sc = new CScreenShot("", (CScreenShot::screenshot_format_t)g_settings.screenshot_format);
					sc->MakeFileName(CZapit::getInstance()->GetCurrentChannelID());
					sc->Start();
				}
			}
#endif
			else if(msg == (neutrino_msg_t) g_settings.key_timeshift) {
#if 0
				if (mode == NeutrinoModes::mode_webtv) {
					CMoviePlayerGui::getInstance().Pause();
				} else
#endif
					CRecordManager::getInstance()->StartTimeshift();
			}
#if ENABLE_PIP
			else if ((msg == (neutrino_msg_t) g_settings.key_pip_close) && g_info.hw_caps->can_pip) {
				int boxmode = getBoxMode();
				if (boxmode > -1 && boxmode != 12)
					ShowMsg(LOCALE_MESSAGEBOX_ERROR, LOCALE_BOXMODE12_NOT_ACTIVATED, CMsgBox::mbrOk, CMsgBox::mbOk, NEUTRINO_ICON_ERROR);
				else
				{
					t_channel_id pip_channel_id = CZapit::getInstance()->GetPipChannelID();
					if (pip_channel_id)
						g_Zapit->stopPip();
					else
						StartPip(CZapit::getInstance()->GetCurrentChannelID());
				}
			}
#if !HAVE_CST_HARDWARE && !HAVE_GENERIC_HARDWARE
			else if ((msg == (neutrino_msg_t) g_settings.key_pip_close_avinput) && ((g_info.hw_caps->has_SCART_input) || (g_info.hw_caps->has_HDMI_input)) && g_info.hw_caps->can_pip) {
				int boxmode = getBoxMode();
				if (boxmode > -1 && boxmode != 12)
					ShowMsg(LOCALE_MESSAGEBOX_ERROR, LOCALE_BOXMODE12_NOT_ACTIVATED, CMsgBox::mbrOk, CMsgBox::mbOk, NEUTRINO_ICON_ERROR);
				else
				{
					if (CZapit::getInstance()->GetPipChannelID())
						CZapit::getInstance()->StopPip();
					if (!avinput_pip)
						StartAVInputPiP();
					else
						StopAVInputPiP();
				}
			}
#endif
			else if ((msg == (neutrino_msg_t) g_settings.key_pip_setup) && g_info.hw_caps->can_pip) {
				CPipSetup pipsetup;
				pipsetup.exec(NULL, "");
			}
			else if ((msg == (neutrino_msg_t) g_settings.key_pip_swap) && g_info.hw_caps->can_pip) {
				t_channel_id pip_channel_id = CZapit::getInstance()->GetPipChannelID();
				t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
				if (pip_channel_id && (pip_channel_id != live_channel_id)) {
					g_Zapit->stopPip();
					channelList->zapTo_ChannelID(pip_channel_id);
					StartPip(live_channel_id);
				}
			}
			else if (((msg == (neutrino_msg_t) g_settings.key_pip_rotate_cw) || (msg == (neutrino_msg_t) g_settings.key_pip_rotate_ccw)) && g_info.hw_caps->can_pip) {
				(msg == (neutrino_msg_t) g_settings.key_pip_rotate_cw) ? pip_rotate(1) : pip_rotate(-1);
			}
#endif
			else if( msg == (neutrino_msg_t) g_settings.key_record /* && (mode != NeutrinoModes::mode_webtv) */) {
				if (g_settings.recording_type != CNeutrinoApp::RECORDING_OFF)
					CRecordManager::getInstance()->exec(NULL, "Record");
			}
			else if ((mode == NeutrinoModes::mode_webtv) && msg == (neutrino_msg_t) g_settings.mpkey_subtitle) {
				CMoviePlayerGui::getInstance(true).selectSubtitle();
			}
			/* after sensitive key bind, check user menu */
			else if (usermenu.showUserMenu(msg)) {
			}
			/* hardcoded key values, if not redefined in keybind or user menu */
			else if( msg == CRCInput::RC_text) {
				g_RCInput->clearRCMsg();
				InfoClock->enableInfoClock(false);
				InfoIcons->enableInfoIcons(false); //NI InfoIcons
				StopSubtitles();
				tuxtx_stop_subtitle();

				tuxtx_main(g_RemoteControl->current_PIDs.PIDs.vtxtpid);

				frameBuffer->paintBackground();
				//if(!g_settings.cacheTXT)
				//	tuxtxt_stop();
				g_RCInput->clearRCMsg();
				InfoClock->enableInfoClock(true);
				InfoIcons->enableInfoIcons(true); //NI InfoIcons
				StartSubtitles();
			}
			else if (((msg == CRCInput::RC_tv) || (msg == CRCInput::RC_radio)) && (g_settings.key_tvradio_mode == (int)CRCInput::RC_nokey)) {
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
				if (msg == CRCInput::RC_tv)
				{
					if (mode == NeutrinoModes::mode_radio || mode == NeutrinoModes::mode_webradio)
						tvMode();
					else if (!g_InfoViewer->is_visible)
						g_RCInput->postMsg(CRCInput::RC_info, 0);
				}
				else if (msg == CRCInput::RC_radio)
				{
					if (mode == NeutrinoModes::mode_tv || mode == NeutrinoModes::mode_webtv)
						radioMode();
					else if (!g_InfoViewer->is_visible)
						g_RCInput->postMsg(CRCInput::RC_info, 0);
				}
				else
#endif
					switchTvRadioMode(); //used with defined default tv/radio rc key
			}
			/* in case key_subchannel_up/down redefined */
			else if( msg == CRCInput::RC_left || msg == CRCInput::RC_right) {
				switch (g_settings.mode_left_right_key_tv)
				{
					case SNeutrinoSettings::INFOBAR:
					case SNeutrinoSettings::VZAP:
						if (channelList->getSize())
							showInfo();
						break;
					case SNeutrinoSettings::VOLUME:
						g_volume->setVolume(msg);
						break;
					default: /* SNeutrinoSettings::ZAP */
						quickZap(msg);
						break;
				}
			}
			else if( msg == CRCInput::RC_epg ) {
				InfoClock->enableInfoClock(false);
				InfoIcons->enableInfoIcons(false); //NI InfoIcons
				StopSubtitles();
				t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
				g_EventList->exec(live_channel_id, channelList->getActiveChannelName());
				InfoClock->enableInfoClock(true);
				InfoIcons->enableInfoIcons(true); //NI InfoIcons
				StartSubtitles();
			}
			else if (CRCInput::isNumeric(msg)) {
				numericZap(msg);
			}
			/* FIXME ??? */
			else if (CRCInput::isNumeric(msg) && g_RemoteControl->director_mode ) {
				g_RemoteControl->setSubChannel(CRCInput::getNumericValue(msg));
				g_InfoViewer->showSubchan();
			}
			else if( msg == CRCInput::RC_page_up || msg == CRCInput::RC_page_down) {
				quickZap(msg == CRCInput::RC_page_up ? CRCInput::RC_right : CRCInput::RC_left);
			}
			else if(msg == CRCInput::RC_rewind /* && (mode != NeutrinoModes::mode_webtv) */) {
				if(g_RemoteControl->is_video_started) {
					t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
					if(CRecordManager::getInstance()->RecordingStatus(live_channel_id))
						CMoviePlayerGui::getInstance().exec(NULL, "timeshift_rewind");
				}
			}
			else if( msg == CRCInput::RC_stop) {
				StopSubtitles();
				CRecordManager::getInstance()->exec(NULL, "Stop_record");
				StartSubtitles();
			}
			else if (msg == CRCInput::RC_audio)
			{
				StopSubtitles();
				CAudioSelectMenuHandler as;
				as.exec(NULL, "-1");
				StartSubtitles();
			}
			else if( msg == CRCInput::RC_video || msg == CRCInput::RC_play || msg == CRCInput::RC_playpause || msg == CRCInput::RC_pvr)
			{
				//open moviebrowser via media player menu object
				if (g_settings.recording_type != CNeutrinoApp::RECORDING_OFF)
					CMediaPlayerMenu::getInstance()->exec(NULL, "moviebrowser");
				CVFD::getInstance()->UpdateIcons();
			}
			else if( ( msg == CRCInput::RC_help ) || ( msg == CRCInput::RC_info) ||
						( msg == NeutrinoMessages::SHOW_INFOBAR ) )
			{
				bool show_info = ((msg != NeutrinoMessages::SHOW_INFOBAR) || (g_InfoViewer->is_visible || g_InfoViewer->hasTimeout()));

			         // turn on LCD display
				CVFD::getInstance()->wake_up();

				// show Infoviewer
				if(show_info && channelList->getSize()) {
					showInfo();
				}
#ifdef ENABLE_GRAPHLCD
				if (msg == NeutrinoMessages::EVT_CURRENTNEXT_EPG) {
					cGLCD::Update();
				}
#endif
			}
			else if (msg == CRCInput::RC_timer || msg == CRCInput::RC_program)
			{
				CTimerList Timerlist;
				Timerlist.exec(NULL, "");
			}
			else if (msg == CRCInput::RC_aux)
				AVInputMode(true);
			else {
				if (CNeutrinoApp::getInstance()->backKey(msg))
				{
					//NI InfoIcons
					if(g_settings.mode_icons && g_settings.mode_icons_skin == INFOICONS_POPUP) {
						InfoIcons->hideIcons();
					}
					CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
				}

				if (msg != CRCInput::RC_timeout)
					handleMsg(msg, data);
			}
		}
		else {
			// mode == NeutrinoModes::mode_avinput
			if (backKey(msg) || msg == CRCInput::RC_aux) {
				if( mode == NeutrinoModes::mode_avinput ) {
					// AVInput-Mode verlassen
					AVInputMode(false);
				}
			}
			else {
				if (msg != CRCInput::RC_timeout)
					handleMsg(msg, data);
			}
		}
	}
}

int CNeutrinoApp::showChannelList(const neutrino_msg_t _msg, bool from_menu)
{
	/* Exit here if paint of channlellist is not allowed, disallow could be possible, eg: if
	 * RC_ok or other stuff is shared with other window handlers and
	 * it's easy here to disable channellist paint if required!
	*/
	if (!channelList_allowed){
		channelList_allowed = true;
		return menu_return::RETURN_NONE;
	}
	channelList_painted = true;

	neutrino_msg_t msg = _msg;
	InfoClock->enableInfoClock(false);//TODO: use callback in channel list class
	InfoIcons->enableInfoIcons(false); //NI InfoIcons
	StopSubtitles();

	int nNewChannel = -1;
	int old_b = bouquetList->getActiveBouquetNumber();
	t_channel_id old_id = 0;
	if(!bouquetList->Bouquets.empty())
		old_id = bouquetList->Bouquets[bouquetList->getActiveBouquetNumber()]->channelList->getActiveChannel_ChannelID();

	int old_mode = GetChannelMode();
	printf("CNeutrinoApp::showChannelList: bouquetList %p size %d old_b %d\n", bouquetList, (int)bouquetList->Bouquets.size(), old_b);fflush(stdout);

	// reset display mode and description mode of channellist
	g_settings.channellist_displaymode = DISPLAY_MODE_NOW;
	g_settings.channellist_descmode = false;

	if(msg == CRCInput::RC_ok)
	{
		switch (g_settings.bouquetlist_mode)
		{
			case SNeutrinoSettings::FAVORITES:
			{
				SetChannelMode(LIST_MODE_FAV);
				if (bouquetList->Bouquets.empty())
					SetChannelMode(LIST_MODE_PROV);
				nNewChannel = bouquetList->exec(true);
				break;
			}
			case SNeutrinoSettings::CHANNELLIST:
			default:
			{
				if( !bouquetList->Bouquets.empty() && bouquetList->Bouquets[old_b]->channelList->getSize() > 0)
					nNewChannel = bouquetList->Bouquets[old_b]->channelList->exec();//with ZAP!
				else
					nNewChannel = bouquetList->exec(true);
				break;
			}
		}
	} else if(msg == CRCInput::RC_sat) {
		SetChannelMode(LIST_MODE_SAT);
		nNewChannel = bouquetList->exec(true);
	} else if(msg == CRCInput::RC_www) {
		SetChannelMode(LIST_MODE_WEB);
		if (bouquetList->Bouquets.empty())
			SetChannelMode(LIST_MODE_PROV);
		nNewChannel = bouquetList->exec(true);
	} else if(msg == (neutrino_msg_t) g_settings.key_favorites) {
		SetChannelMode(LIST_MODE_FAV);
		if (bouquetList->Bouquets.empty())
			SetChannelMode(LIST_MODE_PROV);
		nNewChannel = bouquetList->exec(true);
	} else if (msg == (neutrino_msg_t) g_settings.key_zaphistory || msg == (neutrino_msg_t) g_settings.key_current_transponder) {
		nNewChannel = channelList->showLiveBouquet(msg);
	}
_repeat:
	printf("CNeutrinoApp::showChannelList: nNewChannel %d\n", nNewChannel);fflush(stdout);
	//CVFD::getInstance ()->showServicename(channelList->getActiveChannelName());
	CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
	if(nNewChannel == CHANLIST_CANCEL) { // restore orig. bouquet and selected channel on cancel
		/* FIXME if mode was changed while browsing,
		 * other modes selected bouquet not restored */
		SetChannelMode(old_mode);
		bouquetList->activateBouquet(old_b, false);

		if(!bouquetList->Bouquets.empty())
			bouquetList->Bouquets[bouquetList->getActiveBouquetNumber()]->channelList->adjustToChannelID(old_id);

		StartSubtitles(mode == NeutrinoModes::mode_tv);
	}
	else if(nNewChannel == CHANLIST_CHANGE_MODE) { // list mode changed
		printf("CNeutrinoApp::showChannelList: newmode: bouquetList %p size %d\n", bouquetList, (int)bouquetList->Bouquets.size());fflush(stdout);
		nNewChannel = bouquetList->exec(true);
		goto _repeat;
	}
	if (channels_changed || favorites_changed || bouquets_changed || channels_init) {
		neutrino_locale_t loc = channels_init ? LOCALE_SERVICEMENU_RELOAD_HINT : LOCALE_BOUQUETEDITOR_SAVINGCHANGES;
		CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(loc));
		hintBox.paint();

		if (favorites_changed) {
			g_bouquetManager->saveUBouquets();
			if (!channels_init)
				CEpgScan::getInstance()->ConfigureEIT();
		}

		if (channels_changed)
			CServiceManager::getInstance()->SaveServices(true);

		if (bouquets_changed)
			g_bouquetManager->saveBouquets();

		if (channels_init) {
			g_bouquetManager->renumServices();
			channelsInit(/*true*/);
		}

		favorites_changed = false;
		channels_changed = false;
		bouquets_changed = false;
		channels_init = false;

		t_channel_id live_channel_id = channelList->getActiveChannel_ChannelID();
		if(!live_channel_id)
			live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
		adjustToChannelID(live_channel_id);//FIXME what if deleted ?
		hintBox.hide();
	}

	channelList_painted = false;

	if (!from_menu)
		InfoClock->enableInfoClock(true);
	if (!from_menu)
		InfoIcons->enableInfoIcons(true); //NI InfoIcons

	return ((nNewChannel >= 0) ? menu_return::RETURN_EXIT_ALL : menu_return::RETURN_REPAINT);
}

void CNeutrinoApp::zapTo(t_channel_id channel_id)
{
	bool recordingStatus = CRecordManager::getInstance()->RecordingStatus(channel_id);
	if (!recordingStatus || (recordingStatus && CRecordManager::getInstance()->TimeshiftOnly()) ||
			(recordingStatus && channelList && channelList->SameTP(channel_id))) {

		dvbsub_stop();
		g_Zapit->zapTo_serviceID_NOWAIT(channel_id);
	}
}

bool CNeutrinoApp::wakeupFromStandby(void)
{
	bool alive = recordingstatus || CEpgScan::getInstance()->Running() ||
		CStreamManager::getInstance()->StreamStatus();

	if ((mode == NeutrinoModes::mode_standby) && !alive) {
		if (cpuFreq)
			cpuFreq->SetCpuFreq(g_settings.cpufreq * 1000 * 1000);
		if(g_settings.ci_standby_reset) {
			g_CamHandler->exec(NULL, "ca_ci_reset0");
			g_CamHandler->exec(NULL, "ca_ci_reset1");
		}
		g_Zapit->setStandby(false);
		g_Zapit->getMode();
		return true;
	}
	return false;
}

void CNeutrinoApp::standbyToStandby(void)
{
	bool alive = recordingstatus || CEpgScan::getInstance()->Running() ||
		CStreamManager::getInstance()->StreamStatus();

	if ((mode == NeutrinoModes::mode_standby) && !alive) {
		// zap back to pre-recording channel if necessary
		t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
		if (standby_channel_id && (live_channel_id != standby_channel_id)) {
			live_channel_id = standby_channel_id;
			if(channelList)
				channelList->zapTo_ChannelID(live_channel_id);
		}
		g_Zapit->setStandby(true);
		g_Sectionsd->setPauseScanning(true);
		if (cpuFreq)
			cpuFreq->SetCpuFreq(g_settings.standby_cpufreq * 1000 * 1000);
	}
}

void CNeutrinoApp::stopPlayBack(bool lock)
{
	CMoviePlayerGui::getInstance().stopPlayBack();
	g_Zapit->stopPlayBack();
	if (lock)
		CZapit::getInstance()->EnablePlayback(false);
}

void CNeutrinoApp::lockPlayBack(bool blank)
{
	CMoviePlayerGui::getInstance().stopPlayBack();
	g_Zapit->lockPlayBack();
	if (blank)
		videoDecoder->setBlank(true);
}

bool CNeutrinoApp::listModeKey(const neutrino_msg_t msg)
{
	if (
		   msg == CRCInput::RC_sat
		|| msg == CRCInput::RC_www
		|| msg == (neutrino_msg_t) g_settings.key_favorites
	)
	{
		printf("CNeutrinoApp::listModeKey: true\n");
		return true;
	}
	return false;
}

bool CNeutrinoApp::backKey(const neutrino_msg_t msg)
{
	if (
		   msg == CRCInput::RC_home
		|| msg == CRCInput::RC_back
	)
	{
		printf("CNeutrinoApp::backKey: true\n");
		return true;
	}
	return false;
}

int CNeutrinoApp::handleMsg(const neutrino_msg_t _msg, neutrino_msg_data_t data)
{
	int res = 0;
	neutrino_msg_t msg = _msg;

	if(msg == NeutrinoMessages::EVT_WEBTV_ZAP_COMPLETE) {
		t_channel_id chid = *(t_channel_id *) data;
		printf("EVT_WEBTV_ZAP_COMPLETE: %" PRIx64 "\n", chid);
		if (mode == NeutrinoModes::mode_standby) {
			delete [] (unsigned char*) data;
		} else {
			CZapitChannel * cc = CZapit::getInstance()->GetCurrentChannel();
			if (cc && (chid == cc->getChannelID())) {
				CMoviePlayerGui::getInstance().stopPlayBack();
				if (CMoviePlayerGui::getInstance().PlayBackgroundStart(cc->getUrl(), cc->getName(), cc->getChannelID(), cc->getScriptName()))
					delete [] (unsigned char*) data;
				else
				{
					if (mode == NeutrinoModes::mode_webtv || mode == NeutrinoModes::mode_webradio)
						videoDecoder->setBlank(true);
					g_RCInput->postMsg(NeutrinoMessages::EVT_ZAP_FAILED, data);
				}
			} else
				delete [] (unsigned char*) data;
		}
		return messages_return::handled;
	}
	if (mode == NeutrinoModes::mode_webtv && msg == NeutrinoMessages::EVT_SUBT_MESSAGE) {
		CMoviePlayerGui::getInstance(true).showSubtitle(data);
		return messages_return::handled;
	}
	if (msg == NeutrinoMessages::EVT_AUTO_SET_VIDEOSYSTEM) {
		printf(">>>>>[CNeutrinoApp::%s:%d] Receive EVT_AUTO_SET_VIDEOSYSTEM message\n", __func__, __LINE__);
		COsdHelpers *coh = COsdHelpers::getInstance();
		int videoSystem = (int)data;
		if ((videoSystem != -1) /* -1 => not enabled for automode */ &&
		    (coh->getVideoSystem() != videoSystem)) {
			coh->setVideoSystem(videoSystem, false);
			if (frameBufferInitialized)
				coh->changeOsdResolution(0, true, false);
		}
		return messages_return::handled;
	}
	if(msg == NeutrinoMessages::EVT_ZAP_COMPLETE) {
		CZapit::getInstance()->GetAudioMode(g_settings.audio_AnalogMode);
		if(g_settings.audio_AnalogMode < 0 || g_settings.audio_AnalogMode > 2)
			g_settings.audio_AnalogMode = 0;

		CVFD::getInstance()->UpdateIcons();
#ifdef ENABLE_GRAPHLCD
		cGLCD::Update();
#endif
		g_RCInput->killTimer(scrambled_timer);
		if (mode != NeutrinoModes::mode_webtv) {
			g_Zapit->setMode43(g_settings.video_43mode);
			videoDecoder->setAspectRatio(g_settings.video_Format,-1);

			scrambled_timer = g_RCInput->addTimer(10*1000*1000, true);
			SelectSubtitles();
			//StartSubtitles(!g_InfoViewer->is_visible);

			/* update scan settings for manual scan to current channel */
			CScanSetup::getInstance()->updateManualSettings();
		}
	}
	if ((msg == NeutrinoMessages::EVT_TIMER)) {
		if(data == scrambled_timer) {
			scrambled_timer = 0;
			if(g_settings.scrambled_message && videoDecoder->getBlank() && videoDecoder->getPlayState()) {
				const char * text = g_Locale->getText(LOCALE_SCRAMBLED_CHANNEL);
				ShowHint (LOCALE_MESSAGEBOX_INFO, text, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(text) + 10, 5);
			}
			return messages_return::handled;
		}
		if(data == fst_timer) {
			g_RCInput->killTimer(fst_timer);
			if (wakeupFromStandby()) {
				CheckFastScan(true);
				standbyToStandby();
			} else if (mode == NeutrinoModes::mode_standby) {
				fst_timer = g_RCInput->addTimer(30*1000*1000, true);
			}
			return messages_return::handled;
		}
	}
	if (msg == NeutrinoMessages::SHOW_MAINMENU) {
		showMainMenu();
		return messages_return::handled;
	}


	res = res | g_RemoteControl->handleMsg(msg, data);
	res = res | g_InfoViewer->handleMsg(msg, data);
	if (channelList) /* can be not yet ready during startup */
		res = res | channelList->handleMsg(msg, data);
	// else fprintf(stderr, "channelList = NULL!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	res = res | CRecordManager::getInstance()->handleMsg(msg, data);
	res = res | CEpgScan::getInstance()->handleMsg(msg, data);
	res = res | CHDDMenuHandler::getInstance()->handleMsg(msg, data);

	if( res != messages_return::unhandled ) {
		if( ( msg>= CRCInput::RC_WithData ) && ( msg< CRCInput::RC_WithData+ 0x10000000 ) ) {
			delete[] (unsigned char*) data;
		}
		return( res & ( 0xFFFFFFFF - messages_return::unhandled ) );
	}

	/* we assume g_CamHandler free/delete data if needed */
	res = g_CamHandler->handleMsg(msg, data);
	if( res != messages_return::unhandled ) {
		return(res & (0xFFFFFFFF - messages_return::unhandled));
	}

	/* ================================== KEYS ================================================ */
	if (msg == (neutrino_msg_t) g_settings.key_standby_off_add && mode == NeutrinoModes::mode_standby)
		msg = CRCInput::RC_standby_off;

	if( msg == CRCInput::RC_ok || msg == (neutrino_msg_t) g_settings.key_zaphistory || msg == (neutrino_msg_t) g_settings.key_current_transponder
			|| (!g_InfoViewer->getSwitchMode() && CNeutrinoApp::getInstance()->listModeKey(msg))) {
		if( (mode == NeutrinoModes::mode_tv) || (mode == NeutrinoModes::mode_radio) || (mode == NeutrinoModes::mode_ts) || (mode == NeutrinoModes::mode_webtv) || (mode == NeutrinoModes::mode_webradio)) {
			// reset displaymode and descmode of channellist
			g_settings.channellist_displaymode = DISPLAY_MODE_NOW;
			g_settings.channellist_descmode = false;

			showChannelList(msg);
			return messages_return::handled;
		}
	}
	else if (msg == CRCInput::RC_standby_on) {
		if (data == 0)
			g_RCInput->postMsg(NeutrinoMessages::STANDBY_ON, 0);
		return messages_return::cancel_all | messages_return::handled;
	}
	else if ((msg == CRCInput::RC_standby_off) || (msg == CRCInput::RC_power_on)) {
		if (data == 0)
			g_RCInput->postMsg(NeutrinoMessages::STANDBY_OFF, 0);
		return messages_return::handled;
	}
	else if (msg == CRCInput::RC_power_off) {
		g_RCInput->postMsg(NeutrinoMessages::SHUTDOWN, 0);
		return messages_return::cancel_all | messages_return::handled;
	}
	else if ((msg == CRCInput::RC_tv) || (msg == CRCInput::RC_radio)) {
		if (data == 0)
			g_RCInput->postMsg(NeutrinoMessages::LEAVE_ALL, 0);
		return messages_return::cancel_all | messages_return::handled;
	}
	else if (msg == (neutrino_msg_t) g_settings.key_power_off /*CRCInput::RC_standby*/) {
		if (data == 0) {
			neutrino_msg_t new_msg;

			/* Note: pressing the power button on the dbox (not the remote control) over 1 second */
			/*       shuts down the system even if !g_settings.shutdown_real_rcdelay (see below)  */
			gettimeofday(&standby_pressed_at, NULL);

			if ((mode != NeutrinoModes::mode_standby) && (g_settings.shutdown_real)) {
				CRecordManager::getInstance()->StopAutoRecord();
				if(CRecordManager::getInstance()->RecordingStatus()) {
					new_msg = NeutrinoMessages::STANDBY_ON;
					CTimerManager::getInstance()->wakeup = true;
					g_RCInput->firstKey = false;
				} else
					new_msg = NeutrinoMessages::SHUTDOWN;
			}
			else {
				new_msg = (mode == NeutrinoModes::mode_standby) ? NeutrinoMessages::STANDBY_OFF : NeutrinoMessages::STANDBY_ON;
				//printf("standby: new msg %X\n", new_msg);
				if ((g_settings.shutdown_real_rcdelay)) {
					neutrino_msg_t      _msg_;
					neutrino_msg_data_t mdata;
					struct timeval      endtime;
					time_t              seconds;

					int timeout = g_settings.repeat_blocker;
					int timeout1 = g_settings.repeat_genericblocker;

					if (timeout1 > timeout)
						timeout = timeout1;

					timeout += 500;
					//printf("standby: timeout %d\n", timeout);

					while(true) {
						g_RCInput->getMsg_ms(&_msg_, &mdata, timeout);

						//printf("standby: input msg %X\n", msg);
						if (_msg_ == CRCInput::RC_timeout)
							break;

						gettimeofday(&endtime, NULL);
						seconds = endtime.tv_sec - standby_pressed_at.tv_sec;
						if (endtime.tv_usec < standby_pressed_at.tv_usec)
							seconds--;
						//printf("standby: input seconds %d\n", seconds);
						if (seconds >= 1) {
							if (_msg_ == CRCInput::RC_standby)
								new_msg = NeutrinoMessages::SHUTDOWN;
							break;
						}
					}
				}
			}
			g_RCInput->postMsg(new_msg, 0);
			return messages_return::cancel_all | messages_return::handled;
		}
		return messages_return::handled;
#if 0
		else  /* data == 1: KEY_POWER released                         */
			if (standby_pressed_at.tv_sec != 0) /* check if we received a KEY_POWER pressed event before */
			{                                   /* required for correct handling of KEY_POWER events of  */
				/* the power button on the dbox (not the remote control) */
				struct timeval endtime;
				gettimeofday(&endtime, NULL);
				time_t seconds = endtime.tv_sec - standby_pressed_at.tv_sec;
				if (endtime.tv_usec < standby_pressed_at.tv_usec)
					seconds--;
				if (seconds >= 1) {
					g_RCInput->postMsg(NeutrinoMessages::SHUTDOWN, 0);
					return messages_return::cancel_all | messages_return::handled;
				}
			}
#endif
	}
	else if ((msg == CRCInput::RC_plus) || (msg == CRCInput::RC_minus))
	{
		g_volume->setVolume(msg);
		return messages_return::handled;
	}
	else if( msg == CRCInput::RC_spkr ) {
		if( mode == NeutrinoModes::mode_standby ) {
			//switch lcd off/on
			CVFD::getInstance()->togglePower();
#ifdef ENABLE_GRAPHLCD
			if (g_settings.glcd_enable)
				cGLCD::TogglePower();
#endif
		}
		else {
			//mute
			g_audioMute->AudioMute(!current_muted, true);
		}
		return messages_return::handled;
	}
	else if( msg == CRCInput::RC_mute_on ) {
		g_audioMute->AudioMute(true, true);
		return messages_return::handled;
	}
	else if( msg == CRCInput::RC_mute_off ) {
		g_audioMute->AudioMute(false, true);
		return messages_return::handled;
	}
	else if( msg == CRCInput::RC_analog_on ) {
		g_settings.analog_out = 1;
		audioDecoder->EnableAnalogOut(true);
		return messages_return::handled;
	}
	else if( msg == CRCInput::RC_analog_off ) {
		g_settings.analog_out = 0;
		audioDecoder->EnableAnalogOut(false);
		return messages_return::handled;
	}
	else if(( msg == CRCInput::RC_mode ) && g_settings.key_format_mode_active ) {
		g_videoSettings->nextMode();
		return messages_return::handled;
	}
	else if(( msg == (neutrino_msg_t) g_settings.key_next43mode ) && g_settings.key_pic_size_active ) {
		g_videoSettings->next43Mode();
		return messages_return::handled;
	}
	else if(( msg == (neutrino_msg_t) g_settings.key_switchformat) && g_settings.key_pic_mode_active ) {
		g_videoSettings->SwitchFormat();
		return messages_return::handled;
	}
	else if( msg == (neutrino_msg_t) g_settings.key_sleep ) {
		CSleepTimerWidget *sleepTimer = new CSleepTimerWidget;
		sleepTimer->exec(NULL, "");
		delete sleepTimer;
		return messages_return::handled;
	}
#ifdef SCREENSHOT
	else if (msg == (neutrino_msg_t) g_settings.key_screenshot) {
		//video+osd scaled to osd size
		CScreenShot * sc = new CScreenShot("", (CScreenShot::screenshot_format_t)g_settings.screenshot_format);
		sc->EnableOSD(true);
		sc->MakeFileName(CZapit::getInstance()->GetCurrentChannelID());
		sc->Start();
	}
#endif

	/* ================================== MESSAGES ================================================ */
	else if (msg == NeutrinoMessages::EVT_VOLCHANGED) {
		//setVolume(msg, false, true);
		return messages_return::handled;
	}
#ifdef HAVE_CONTROLD
	else if( msg == NeutrinoMessages::EVT_VCRCHANGED ) {
		if (g_settings.vcr_AutoSwitch) {
			if( data != VCR_STATUS_OFF )
				g_RCInput->postMsg( NeutrinoMessages::AVINPUT_ON, 0 );
			else
				g_RCInput->postMsg( NeutrinoMessages::AVINPUT_OFF, 0 );
		}
		return messages_return::handled | messages_return::cancel_info;
	}
#endif
	else if( msg == NeutrinoMessages::EVT_MUTECHANGED ) {
		//FIXME unused ?
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::EVT_SERVICESCHANGED ) {
		printf("NeutrinoMessages::EVT_SERVICESCHANGED\n");fflush(stdout);
		channelsInit();
		t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
		adjustToChannelID(live_channel_id);//FIXME what if deleted ?
		if(old_b_id >= 0) {
			bouquetList->activateBouquet(old_b_id, false);
			old_b_id = -1;
			g_RCInput->postMsg(CRCInput::RC_ok, 0);
		}
	}
	else if( msg == NeutrinoMessages::EVT_BOUQUETSCHANGED ) {
		printf("NeutrinoMessages::EVT_BOUQUETSCHANGED\n");fflush(stdout);
		channelsInit();
		t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
		adjustToChannelID(live_channel_id);//FIXME what if deleted ?
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::EVT_RECORDMODE ) {
		/* sent by rcinput, when got msg from zapit about record activated/deactivated */
		/* should be sent when no record running */
		printf("NeutrinoMessages::EVT_RECORDMODE: %s\n", ( data ) ? "on" : "off");
		recordingstatus = data;
		CEpgScan::getInstance()->Next();
		standbyToStandby();
		autoshift = CRecordManager::getInstance()->TimeshiftOnly();
		CVFD::getInstance()->ShowIcon(FP_ICON_CAM1, recordingstatus != 0);

		if( ( !g_InfoViewer->is_visible ) && data && !autoshift)
			g_RCInput->postMsg( NeutrinoMessages::SHOW_INFOBAR, 0 );

		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::RECORD_START) {
		//FIXME better at announce ?
		wakeupFromStandby();
#if 0
		//zap to rec channel if box start from deepstandby
		if(timer_wakeup){
			timer_wakeup=false;
			dvbsub_stop();
			CTimerd::RecordingInfo * eventinfo = (CTimerd::RecordingInfo *) data;
			t_channel_id channel_id=eventinfo->channel_id;
			g_Zapit->zapTo_serviceID_NOWAIT(channel_id);
		}
#endif
		//zap to rec channel in standby-mode
		CTimerd::RecordingInfo * eventinfo = (CTimerd::RecordingInfo *) data;
		t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
		/* special case for nhttpd: start direct record, if no eventID */
		if (eventinfo->eventID == 0) {
			int rec_mode = CRecordManager::getInstance()->GetRecordMode(live_channel_id);
			/* start only if not recorded yet */
			if (rec_mode == CRecordManager::RECMODE_OFF || rec_mode == CRecordManager::RECMODE_TSHIFT)
				CRecordManager::getInstance()->Record(live_channel_id);
			delete[] (unsigned char*) data;
			return messages_return::handled;
		}
		if(mode == NeutrinoModes::mode_standby){
			if((eventinfo->channel_id != live_channel_id) && !(SAME_TRANSPONDER(live_channel_id, eventinfo->channel_id)))
				zapTo(eventinfo->channel_id);
		}
		//NI zap to CI Channel
		if(g_settings.ci_rec_zapto){
			CZapitChannel * ch = CServiceManager::getInstance()->FindChannel(eventinfo->channel_id);
			if (ch && ch->bUseCI && (eventinfo->channel_id != live_channel_id))
			{
				zapTo(eventinfo->channel_id);

				CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, LOCALE_CI_TRY_DECODE);
				hintBox.paint();

				int t = time_monotonic();
				while (videoDecoder->getBlank())
				{
					usleep(100);
					if (time_monotonic() - t > 15)
					{
						// can't decode channel within 15 seconds
						break;
					}
				}
				g_RCInput->clearRCMsg();

				hintBox.hide();
			}
		}
		if (g_settings.recording_type != CNeutrinoApp::RECORDING_OFF) {
			CRecordManager::getInstance()->Record(eventinfo);
			autoshift = CRecordManager::getInstance()->TimeshiftOnly();
		}

		delete[] (unsigned char*) data;
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::RECORD_STOP) {
		CTimerd::RecordingStopInfo* recinfo = (CTimerd::RecordingStopInfo*)data;
		printf("NeutrinoMessages::RECORD_STOP: eventID %d channel_id %" PRIx64 "\n", recinfo->eventID, recinfo->channel_id);
		CRecordManager::getInstance()->Stop(recinfo);
		autoshift = CRecordManager::getInstance()->TimeshiftOnly();

		delete[] (unsigned char*) data;
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_STREAM_START) {
		printf("NeutrinoMessages::EVT_STREAM_START\n");
		wakeupFromStandby();
		if (g_Radiotext)
			g_Radiotext->setPid(0);

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
		if (!CRecordManager::getInstance()->GetRecordCount()) {
			CVFD::getInstance()->ShowIcon(FP_ICON_CAM1, false);
		}
#endif
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_STREAM_STOP) {
		printf("NeutrinoMessages::EVT_STREAM_STOP\n");
		CEpgScan::getInstance()->Next();
		standbyToStandby();
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::EVT_PMT_CHANGED) {
		t_channel_id channel_id = *(t_channel_id*) data;
		CRecordManager::getInstance()->Update(channel_id);
		delete[] (unsigned char*) data;
		return messages_return::handled;
	}

	else if( msg == NeutrinoMessages::ZAPTO) {
		CTimerd::EventInfo * eventinfo = (CTimerd::EventInfo *) data;
		if (eventinfo->channel_id != CZapit::getInstance()->GetCurrentChannelID()){
			if( (recordingstatus == 0) || (recordingstatus && CRecordManager::getInstance()->TimeshiftOnly()) ||
					(recordingstatus && channelList && channelList->SameTP(eventinfo->channel_id)) ) {
				bool isTVMode = CServiceManager::getInstance()->IsChannelTVChannel(eventinfo->channel_id);

				dvbsub_stop();

				if ((!isTVMode) && (mode != NeutrinoModes::mode_radio) && (mode != NeutrinoModes::mode_webradio)) {
					radioMode(true);
				}
				else if (isTVMode && (mode != NeutrinoModes::mode_tv) && (mode != NeutrinoModes::mode_webtv)) {
					tvMode(true);
				}

				if(channelList)
					channelList->zapTo_ChannelID(eventinfo->channel_id);
			}
		}
		delete[] (unsigned char*) data;
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::ANNOUNCE_ZAPTO) {
		if( mode == NeutrinoModes::mode_standby ) {
			standbyMode( false );
		}
		if( mode != NeutrinoModes::mode_avinput ) {
			CTimerd::RecordingInfo * eventinfo = (CTimerd::RecordingInfo *) data;
			std::string name = g_Locale->getText(LOCALE_ZAPTOTIMER_ANNOUNCE);
			getAnnounceEpgName( eventinfo, name);
			ShowHint( LOCALE_MESSAGEBOX_INFO, name.c_str() );
		}
		delete [] (unsigned char*) data;
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::ANNOUNCE_RECORD) {
		exec_controlscript(NEUTRINO_RECORDING_TIMER_SCRIPT);
		CTimerd::RecordingInfo * eventinfo = (CTimerd::RecordingInfo *) data;
		if (g_settings.recording_type == RECORDING_FILE) {
			char * recordingDir = eventinfo->recordingDir;
			for(int i=0 ; i < NETWORK_NFS_NR_OF_ENTRIES ; i++) {
				if (g_settings.network_nfs[i].local_dir == recordingDir) {
					printf("[neutrino] waking up %s (%s)\n", g_settings.network_nfs[i].ip.c_str(), recordingDir);
					if (my_system(2, "ether-wake", g_settings.network_nfs[i].mac.c_str()) != 0)
						perror("ether-wake failed");
					break;
				}
			}
			printf("CNeutrinoApp::%s: wakeup hdd...\n", __func__); //NI
			wakeup_hdd(recordingDir);
		}

		if( g_settings.recording_zap_on_announce && (mode != NeutrinoModes::mode_standby) && (eventinfo->channel_id != CZapit::getInstance()->GetCurrentChannelID())) {
			CRecordManager::getInstance()->StopAutoRecord();
			zapTo(eventinfo->channel_id);
		}
		if(( mode != NeutrinoModes::mode_avinput ) && ( mode != NeutrinoModes::mode_standby ) && g_settings.recording_startstop_msg) {
			std::string name = g_Locale->getText(LOCALE_RECORDTIMER_ANNOUNCE);
			getAnnounceEpgName(eventinfo, name);
			ShowHint(LOCALE_MESSAGEBOX_INFO, name.c_str());
		}
		delete[] (unsigned char*) data;
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::ANNOUNCE_SLEEPTIMER) {
		if( mode != NeutrinoModes::mode_avinput && mode != NeutrinoModes::mode_standby)
			skipSleepTimer = (ShowMsg(LOCALE_MESSAGEBOX_INFO, g_settings.shutdown_real ? LOCALE_SHUTDOWNTIMER_ANNOUNCE:LOCALE_SLEEPTIMERBOX_ANNOUNCE,CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo, NULL, 450, 30, true) == CMsgBox::mbrYes);
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::SLEEPTIMER) {
		if( data && mode == NeutrinoModes::mode_standby ) { //SKIP INACTIVITY SLEEPTIMER
			printf("NeutrinoMessages::INACTIVITY SLEEPTIMER: skipping, already in standby mode\n");
			return messages_return::handled;
		}
		else if(data) {//INACTIVITY SLEEPTIMER
			int msgbox = ShowMsg(LOCALE_MESSAGEBOX_INFO, g_settings.shutdown_real ? LOCALE_SHUTDOWNTIMER_ANNOUNCE:LOCALE_SLEEPTIMERBOX_ANNOUNCE,
				      CMsgBox::mbrCancel, CMsgBox::mbCancel, NULL, 450, 60);
			skipShutdownTimer = !(msgbox & CMsgBox::mbrTimeout);
			if(skipShutdownTimer) {
				printf("NeutrinoMessages::INACTIVITY SLEEPTIMER: skipping\n");
				skipShutdownTimer = false;
				return messages_return::handled;
			}
		}else{ //MAIN-MENU SLEEPTIMER
			if(skipSleepTimer) {
				printf("NeutrinoMessages::SLEEPTIMER: skipping\n");
				skipSleepTimer = false;
				return messages_return::handled;
			}
		}
		if (g_settings.shutdown_real)
			g_RCInput->postMsg(NeutrinoMessages::SHUTDOWN, 0);
		else
			g_RCInput->postMsg(NeutrinoMessages::STANDBY_ON, 0);
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::RELOAD_SETUP ) {
		bool tmp1 = g_settings.make_hd_list;
		bool tmp2 = g_settings.make_webtv_list;
		bool tmp3 = g_settings.make_webradio_list;
		loadSetup(NEUTRINO_SETTINGS_FILE);
		if(tmp1 != g_settings.make_hd_list || tmp2 != g_settings.make_webtv_list || tmp3 != g_settings.make_webradio_list)
			g_Zapit->reinitChannels();

		SendSectionsdConfig();
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::STANDBY_TOGGLE ) {
		standbyMode( !(mode & NeutrinoModes::mode_standby) );
		g_RCInput->clearRCMsg();
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::LEAVE_ALL ) {
		g_RCInput->clearRCMsg();
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::STANDBY_ON ) {
		if( mode != NeutrinoModes::mode_standby ) {
			standbyMode( true );
		}
		g_RCInput->clearRCMsg();
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::STANDBY_OFF ) {
		if( mode == NeutrinoModes::mode_standby ) {
			standbyMode( false );
		}
		g_RCInput->clearRCMsg();
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::ANNOUNCE_SHUTDOWN) {
		if( mode != NeutrinoModes::mode_avinput )
			skipShutdownTimer = (ShowMsg(LOCALE_MESSAGEBOX_INFO, LOCALE_SHUTDOWNTIMER_ANNOUNCE, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo, NULL, 450, 30) == CMsgBox::mbrYes);
	}
	else if( msg == NeutrinoMessages::SHUTDOWN ) {
		if(CStreamManager::getInstance()->StreamStatus())
			skipShutdownTimer = true;
		if(!skipShutdownTimer) {
			ExitRun(g_info.hw_caps->can_shutdown);
		}
		else {
			skipShutdownTimer=false;
		}
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::RESTART ) {
		ExitRun(CNeutrinoApp::EXIT_RESTART);
	}
	else if( msg == NeutrinoMessages::REBOOT ) {
		ExitRun(CNeutrinoApp::EXIT_REBOOT);
	}
	else if (msg == NeutrinoMessages::EVT_POPUP || msg == NeutrinoMessages::EVT_EXTMSG) {
		if (mode != NeutrinoModes::mode_avinput && mode != NeutrinoModes::mode_standby) {
			int timeout = DEFAULT_TIMEOUT;
			std::string text = (char*)data;
			std::string::size_type pos;

			pos = text.find("&timeout=", 0);
			if (pos != std::string::npos) {
				std::string tmp = text.substr( pos+9, text.length()+1 );
				text[pos] = '\0';
				timeout = atoi(tmp.c_str());
			}

			if (msg == NeutrinoMessages::EVT_POPUP)
				ShowHint(LOCALE_MESSAGEBOX_INFO, text.c_str(), 0, timeout);
			else if (msg == NeutrinoMessages::EVT_EXTMSG)
				ShowMsg(LOCALE_MESSAGEBOX_INFO, text, CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_INFO, 500, timeout);
		}
		delete[] (unsigned char*) data;
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_RECORDING_ENDED) {
		/* FIXME TODO, when/if needed, monitor record status somewhere
		 * and report possible error to user if any with this message ?
		 * not used/not supported for now */
		//delete[] (unsigned char*) data;

		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::REMIND) {
		std::string text = (char*)data;
		std::string::size_type pos;
		while((pos=text.find('/'))!= std::string::npos)
		{
			text[pos] = '\n';
		}
		if( mode != NeutrinoModes::mode_avinput )
			ShowMsg(LOCALE_TIMERLIST_TYPE_REMIND, text, CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_INFO); // UTF-8
		delete[] (unsigned char*) data;
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::LOCK_RC)
	{
		CRCLock::getInstance()->exec(NULL, CRCLock::NO_USER_INPUT);
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::LOCK_RC_EXTERN || msg == NeutrinoMessages::UNLOCK_RC_EXTERN)
	{
		printf("CNeutrinoApp::handleMsg: RC is %s now\n", msg == NeutrinoMessages::LOCK_RC_EXTERN ? "LOCKED" : "UNLOCKED");
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::CHANGEMODE ) {
		printf("CNeutrinoApp::handleMsg: CHANGEMODE to %s rezap %d\n", neutrinoMode_to_string(data & NeutrinoModes::mode_mask), (data & NeutrinoModes::norezap) != NeutrinoModes::norezap);
		if((data & NeutrinoModes::mode_mask)== NeutrinoModes::mode_radio) {
			if( mode != NeutrinoModes::mode_radio ) {
				radioMode((data & NeutrinoModes::norezap) != NeutrinoModes::norezap);
			}
		}
		if((data & NeutrinoModes::mode_mask)== NeutrinoModes::mode_tv) {
			if( mode != NeutrinoModes::mode_tv ) {
				tvMode((data & NeutrinoModes::norezap) != NeutrinoModes::norezap);
			}
		}
		if((data & NeutrinoModes::mode_mask)== NeutrinoModes::mode_standby) {
			if(mode != NeutrinoModes::mode_standby)
				standbyMode( true );
		}
		if((data & NeutrinoModes::mode_mask)== NeutrinoModes::mode_upnp) {
			lastMode=mode;
			mode=NeutrinoModes::mode_upnp;
		}
		if((data & NeutrinoModes::mode_mask)== NeutrinoModes::mode_audio) {
			lastMode=mode;
			mode=NeutrinoModes::mode_audio;
		}
		if((data & NeutrinoModes::mode_mask)== NeutrinoModes::mode_pic) {
			lastMode=mode;
			mode=NeutrinoModes::mode_pic;
		}
		if((data & NeutrinoModes::mode_mask)== NeutrinoModes::mode_ts) {
			if(mode == NeutrinoModes::mode_radio)
				frameBuffer->stopFrame();
			lastMode=mode;
			mode=NeutrinoModes::mode_ts;
		}
		if((data & NeutrinoModes::mode_mask)== NeutrinoModes::mode_webtv || (data & NeutrinoModes::mode_mask)== NeutrinoModes::mode_webradio) {
			lastMode=mode;
			if ((data & NeutrinoModes::mode_mask) == NeutrinoModes::mode_webtv)
				mode=NeutrinoModes::mode_webtv;
			else
				mode=NeutrinoModes::mode_webradio;
			if ((data & NeutrinoModes::norezap) != NeutrinoModes::norezap) {
				CZapitChannel * cc = CZapit::getInstance()->GetCurrentChannel();
				if (cc && IS_WEBCHAN(cc->getChannelID())) {
					CMoviePlayerGui::getInstance().stopPlayBack();
					if (!CMoviePlayerGui::getInstance().PlayBackgroundStart(cc->getUrl(), cc->getName(), cc->getChannelID(), cc->getScriptName()))
						g_RCInput->postMsg(NeutrinoMessages::EVT_ZAP_FAILED, data);
				}
			}
		}
	}
	else if( msg == NeutrinoMessages::AVINPUT_ON ) {
		if( mode != NeutrinoModes::mode_avinput ) {
			AVInputMode( true );
		}
		else
			CVFD::getInstance()->setMode(CVFD::MODE_AVINPUT);
	}

	else if( msg == NeutrinoMessages::AVINPUT_OFF ) {
		if( mode == NeutrinoModes::mode_avinput ) {
			AVInputMode( false );
		}
	}
	else if (msg == NeutrinoMessages::EVT_START_PLUGIN) {
		g_Plugins->startPlugin((const char *)data);
		if (!g_Plugins->getScriptOutput().empty()) {
			ShowMsg(LOCALE_PLUGINS_RESULT, g_Plugins->getScriptOutput(), CMsgBox::mbrBack,CMsgBox::mbBack,NEUTRINO_ICON_SHELL);
		}

		delete[] (unsigned char*) data;
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_SERVICES_UPD) {
		SDTreloadChannels = true;
		g_InfoViewer->SDT_freq_update = true;
		if( !g_InfoViewer->is_visible && !autoshift){
			g_RCInput->postMsg(NeutrinoMessages::SHOW_INFOBAR , 0);
		}
		return messages_return::handled;
//		ShowHint(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_EXTRA_ZAPIT_SDT_CHANGED),
//				CMsgBox::mbrBack,CMsgBox::mbBack, NEUTRINO_ICON_INFO);
	}
	else if (msg == NeutrinoMessages::EVT_HDMI_CEC_VIEW_ON) {
		if(g_settings.hdmi_cec_view_on)
			videoDecoder->SetCECAutoView(g_settings.hdmi_cec_view_on);

		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_HDMI_CEC_STANDBY) {
		if(g_settings.hdmi_cec_standby)
			videoDecoder->SetCECAutoStandby(g_settings.hdmi_cec_standby);

		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_SET_MUTE) {
		g_audioMute->AudioMute((int)data, true);
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_SET_VOLUME) {
		g_volume->setVolumeExt((int)data);
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_RELOAD_XMLTV) {
		printf("CNeutrinoApp::handleMsg: reload xmltv epg\n");
		xmltv_xml_readepg();
		xmltv_xml_auto_readepg();
		return messages_return::handled;
	}
	if ((msg >= CRCInput::RC_WithData) && (msg < CRCInput::RC_WithData + 0x10000000)) {
		INFO("###################################### DELETED msg %lx data %lx\n", msg, data);
		delete [] (unsigned char*) data;
		return messages_return::handled;
	}
	return messages_return::unhandled;
}

extern time_t timer_minutes;//timermanager.cpp
extern bool timer_is_rec;//timermanager.cpp

void CNeutrinoApp::ExitRun(int exit_code)
{
	bool do_exiting = true;
	CRecordManager::getInstance()->StopAutoRecord();
	if(CRecordManager::getInstance()->RecordingStatus())
	{
		do_exiting = (ShowMsg(LOCALE_MESSAGEBOX_INFO, LOCALE_SHUTDOWN_RECORDING_QUERY, CMsgBox::mbrNo,
					CMsgBox::mbYes | CMsgBox::mbNo, NULL, 450, DEFAULT_TIMEOUT, true) == CMsgBox::mbrYes);
	}
	if (!do_exiting)
		return;

#if 0
	/*
	   For compatibility: /tmp/.reboot is not really needed anymore
	   if we use the defined exit code 2 instead of this flagfile.
	   Next block is just to avoid force changes in start scripts.
	*/
	if (exit_code == CNeutrinoApp::EXIT_REBOOT)
	{
		exit_code = CNeutrinoApp::EXIT_NORMAL;
		FILE *f = fopen("/tmp/.reboot", "w");
		fclose(f);
	}
	else
		unlink("/tmp/.reboot");
#endif

	printf("[neutrino] %s(int %d)\n", __func__, exit_code);
	printf("[neutrino] hw_caps->can_shutdown: %d\n", g_info.hw_caps->can_shutdown);

#ifdef ENABLE_LCD4LINUX
	if (g_settings.lcd4l_support)
		CLCD4l::getInstance()->StopLCD4l();
#endif

	//NI InfoIcons
	if(g_settings.mode_icons && g_settings.mode_icons_skin == INFOICONS_POPUP)
		InfoIcons->saveIconstate();

	if (SDTreloadChannels)
		SDT_ReloadChannels();

	dprintf(DEBUG_INFO, "exit\n");
	OnShutDown();

	//cleanup progress bar cache
	CProgressBarCache::pbcClear();

	StopSubtitles();
	stopPlayBack();

	frameBuffer->paintBackground();
	frameBuffer->showFrame("shutdown.jpg");

	delete cHddStat::getInstance();
	delete CRecordManager::getInstance();

	CEpgScan::getInstance()->Stop();
	if (g_settings.epg_save)
	{
		g_Sectionsd->setPauseScanning(true);
		saveEpg(NeutrinoModes::mode_off);
	}

	/* on shutdown force load new fst */
	if (exit_code == CNeutrinoApp::EXIT_SHUTDOWN)
		CheckFastScan(true, false);

	CVFD::getInstance()->setMode(CVFD::MODE_SHUTDOWN);

	stop_daemons(true); // need here for timer_is_rec before saveSetup
	g_settings.shutdown_timer_record_type = timer_is_rec;
	saveSetup(NEUTRINO_SETTINGS_FILE);

	exec_controlscript(NEUTRINO_ENTER_DEEPSTANDBY_SCRIPT);

	printf("entering off state\n");
	mode = NeutrinoModes::mode_off;

	printf("timer_minutes: %" PRId64 "\n", (int64_t)timer_minutes);
	int leds = 0;
	int bright = 0;
#if HAVE_CST_HARDWARE
	if (exit_code == CNeutrinoApp::EXIT_SHUTDOWN)
	{
		leds = 0x40;
		switch (g_settings.led_deep_mode)
		{
			case 0:
				leds = 0x0; // leds off
				break;
			case 1:
				leds = 0x60; // led1 on, led2 on
				break;
			case 2:
				leds = 0x20; // led1 on, led2 off
				break;
			case 3:
				leds = 0x40; // led1 off, led2 on
				break;
			default:
				break;
		}
		if (leds && g_settings.led_blink && timer_minutes)
			leds |= 0x80;
	}
	if (cs_get_revision() != 10)
		bright = g_settings.lcd_setting[SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS];
#endif
	if (exit_code == EXIT_SHUTDOWN)
	{
		if (timer_minutes)
		{
			/* prioritize proc filesystem */

			time_t t = timer_minutes * 60;
			struct tm *lt = localtime(&t);
			char date[30];
			strftime(date, sizeof(date), "%c", lt);
			printf("wakeup time : %s (%" PRId64 ")\n", date, (int64_t)(timer_minutes * 60));

			proc_put("/proc/stb/fp/wakeup_time", timer_minutes * 60);

			t = time(NULL);
			lt = localtime(&t);
			strftime(date, sizeof(date), "%c", lt);
			printf("current time: %s (%" PRId64 ")\n", date, (int64_t)t);

			proc_put("/proc/stb/fp/rtc", t);

			struct tm *gt = gmtime(&t);
			int offset = (lt->tm_hour - gt->tm_hour) * 3600;
			printf("rtc_offset  : %d\n", offset);

			proc_put("/proc/stb/fp/rtc_offset", offset);
		}

		/* not platform specific */
		FILE *f = fopen("/tmp/.timer", "w");
		if (f)
		{
			fprintf(f, "%" PRId64 "\n", (int64_t)(timer_minutes ? timer_minutes * 60 : 0));
			fprintf(f, "%d\n", leds);
			fprintf(f, "%d\n", bright);
			fclose(f);
		}
		else
			perror("fopen /tmp/.timer");
	}

	delete g_RCInput;
	g_RCInput = NULL;

	if (g_info.hw_caps->has_fan)
		CFanControlNotifier::setSpeed(0);

	delete CVFD::getInstance();
	delete SHTDCNT::getInstance();
	stop_video();

#ifdef ENABLE_GRAPHLCD
	if (exit_code == CNeutrinoApp::EXIT_SHUTDOWN)
		cGLCD::SetBrightness(0);
#endif

	Cleanup();

	printf("[neutrino] This is the end. Exiting with code %d\n", exit_code);
	exit(exit_code);
}

void CNeutrinoApp::saveEpg(int _mode)
{
	struct stat my_stat;
	if (stat(g_settings.epg_dir.c_str(), &my_stat) == 0)
	{
		if (_mode == NeutrinoModes::mode_standby)
		{
			// skip save epg in standby mode, if last saveepg time < 15 minutes
			std::string index_xml = g_settings.epg_dir.c_str();
			index_xml += "/index.xml";
			time_t t=0;
			if (stat(index_xml.c_str(), &my_stat) == 0)
			{
				if (difftime(time(&t), my_stat.st_ctime) < 900)
					return;
			}
		}

		CVFD::getInstance()->Clear();
		//NI CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
		CVFD::getInstance()->ShowText(g_Locale->getText(LOCALE_EPG_SAVING));

		printf("[neutrino] Saving EPG to %s...\n", g_settings.epg_dir.c_str());
		g_Sectionsd->writeSI2XML(g_settings.epg_dir.c_str());

		neutrino_msg_t msg;
		neutrino_msg_data_t data;
		while(true)
		{
			g_RCInput->getMsg(&msg, &data, 1200); // 120 secs..
			if ((msg == CRCInput::RC_timeout) || (msg == NeutrinoMessages::EVT_SI_FINISHED))
			{
				//printf("Msg %x timeout %d EVT_SI_FINISHED %x\n", msg, CRCInput::RC_timeout, NeutrinoMessages::EVT_SI_FINISHED);
				CVFD::getInstance()->Clear();
				// do we really have to change VFD-mode here again?
				CVFD::getInstance()->setMode((_mode == NeutrinoModes::mode_standby) ? CVFD::MODE_STANDBY : CVFD::MODE_SHUTDOWN);
				delete [] (unsigned char*) data;
				break;
			}
			else if (_mode == NeutrinoModes::mode_standby)
			{
				printf("wait for epg saving, msg %x \n", (int) msg);
				handleMsg(msg, data);
			}
		}
	}
}

void CNeutrinoApp::tvMode(bool rezap)
{
	if (mode == NeutrinoModes::mode_webradio) {
		CMoviePlayerGui::getInstance().setLastMode(NeutrinoModes::mode_unknown);
		CMoviePlayerGui::getInstance().stopPlayBack();
		CVFD::getInstance()->ShowIcon(FP_ICON_TV, false);
		rezap = true;
	}
	INFO("rezap %d current mode %s", rezap, neutrinoMode_to_string(mode));
	if (mode == NeutrinoModes::mode_radio || mode == NeutrinoModes::mode_webradio) {
		if (g_settings.radiotext_enable && g_Radiotext) {
			delete g_Radiotext;
			g_Radiotext = NULL;
		}

		frameBuffer->stopFrame();
		CVFD::getInstance()->ShowIcon(FP_ICON_RADIO, false);
		StartSubtitles(!rezap);
	}
	g_InfoViewer->setUpdateTimer(LCD_UPDATE_TIME_TV_MODE);

	CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
	CVFD::getInstance()->ShowIcon(FP_ICON_TV, true);

	if( mode == NeutrinoModes::mode_standby ) {
		CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
		videoDecoder->Standby(false);
	}

#ifdef ENABLE_PIP
	if (g_info.hw_caps->can_pip)
	{
		if (pipVideoDecoder[0])
			pipVideoDecoder[0]->Pig(pip_recalc_pos_x(g_settings.pip_x), pip_recalc_pos_y(g_settings.pip_y), g_settings.pip_width, g_settings.pip_height, frameBuffer->getScreenWidth(true), frameBuffer->getScreenHeight(true));
	}
#endif

#if 0
	if(mode != NeutrinoModes::mode_ts /*&& autoshift*/) {
		//printf("standby on: autoshift ! stopping ...\n");
		CRecordManager::getInstance()->StopAutoRecord();
	}
#endif
	if (mode != NeutrinoModes::mode_webtv) {
		frameBuffer->useBackground(false);
		frameBuffer->paintBackground();
	}
	mode = NeutrinoModes::mode_tv;

	g_RemoteControl->tvMode();
	SetChannelMode(g_settings.channel_mode);

	if( rezap )
		channelRezap();

	videoDecoder->SetSyncMode((AVSYNC_TYPE)g_settings.avsync);
	audioDecoder->SetSyncMode((AVSYNC_TYPE)g_settings.avsync);
}

void CNeutrinoApp::AVInputMode(bool bOnOff)
{
	//printf( (bOnOff) ? "mode: avinput on\n" : "mode: avinput off\n" );

#if !HAVE_CST_HARDWARE && !HAVE_GENERIC_HARDWARE
	if (bOnOff) {
		// AVInput AN
		frameBuffer->useBackground(false);
		frameBuffer->paintBackground();

		CVFD::getInstance()->setMode(CVFD::MODE_AVINPUT);
		lastMode = mode;
		mode = NeutrinoModes::mode_avinput;

		videoDecoder->setAVInput(1);
		audioDecoder->setAVInput(1);
#ifdef ENABLE_GRAPHLCD
		cGLCD::AVInputMode(true);
#endif
	} else {
		// AVInput AUS
		mode = NeutrinoModes::mode_unknown;
		//re-set mode
		if( lastMode == NeutrinoModes::mode_radio || lastMode == NeutrinoModes::mode_webradio) {
			radioMode(false);
		}
		else if( lastMode == NeutrinoModes::mode_tv || lastMode == NeutrinoModes::mode_webtv) {
			tvMode(false);
		}
		else if( lastMode == NeutrinoModes::mode_standby ) {
			standbyMode(true);
		}

		videoDecoder->setAVInput(0);
		audioDecoder->setAVInput(0);
		tvMode(true);
#ifdef ENABLE_GRAPHLCD
		cGLCD::AVInputMode(false);
#endif
	}
#else
	(void)bOnOff; // avoid compiler warning
#endif // !HAVE_CST_HARDWARE && !HAVE_GENERIC_HARDWARE
}

void CNeutrinoApp::standbyMode(bool bOnOff, bool fromDeepStandby)
{
	//static bool wasshift = false;
	INFO("%s", bOnOff ? "ON" : "OFF" );

	if(lockStandbyCall)
		return;

	lockStandbyCall = true;

	if( bOnOff ) {
		// set standby flag
		if (FILE *f = fopen("/tmp/.standby", "w"))
			fclose(f);

#if BOXMODEL_E4HDULTRA
		// ensure a blank screen in standby mode
		videoDecoder->SetControl(VIDEO_CONTROL_ZAPPING_MODE, 2); // force mutetilllock
#endif

#ifdef ENABLE_GRAPHLCD
		cGLCD::StandbyMode(true);
#endif
		CVFD::getInstance()->ShowText("Standby ...");
		if( mode == NeutrinoModes::mode_avinput ) {
		}
		g_InfoViewer->setUpdateTimer(0); // delete timer
		StopSubtitles();
		if(SDTreloadChannels && !CRecordManager::getInstance()->RecordingStatus()){
			SDT_ReloadChannels();
			//SDTreloadChannels = false;
		}

		/* wasshift = */ CRecordManager::getInstance()->StopAutoRecord();

		if(mode == NeutrinoModes::mode_radio && g_Radiotext)
			g_Radiotext->radiotext_stop();

#if ENABLE_PIP
		g_Zapit->stopPip();
#endif
		CMoviePlayerGui::getInstance().stopPlayBack();
		bool stream_status = CStreamManager::getInstance()->StreamStatus();
		if((g_settings.epg_scan_mode == CEpgScan::MODE_OFF) && !fromDeepStandby &&
				!CRecordManager::getInstance()->RecordingStatus() && !stream_status) {
			g_Zapit->setStandby(true);
		} else {
			//g_Zapit->stopPlayBack();
			g_Zapit->lockPlayBack();
		}

		videoDecoder->Standby(true);

		g_Sectionsd->setServiceChanged(0, false);
		g_Sectionsd->setPauseScanning(!fromDeepStandby);

		lastMode = mode;
		mode = NeutrinoModes::mode_standby;

		if(!CRecordManager::getInstance()->RecordingStatus() ) {
			//only save epg when not recording
			if(g_settings.epg_save && !fromDeepStandby && g_settings.epg_save_standby) {
				saveEpg(NeutrinoModes::mode_standby);
			}
		}

		CVFD::getInstance()->Clear();
		CVFD::getInstance()->setMode(CVFD::MODE_STANDBY);
		CVFD::getInstance()->setBacklight(g_settings.backlight_standby);

		InfoClock->enableInfoClock(false);
		InfoIcons->enableInfoIcons(false); //NI InfoIcons

		//remember tuned channel-id
		standby_channel_id = CZapit::getInstance()->GetCurrentChannelID();

		exec_controlscript(NEUTRINO_ENTER_STANDBY_SCRIPT);

		//NI InfoIcons
                if(g_settings.mode_icons && g_settings.mode_icons_skin == INFOICONS_POPUP)
			InfoIcons->saveIconstate();

		CEpgScan::getInstance()->Start(true);

		bool alive = recordingstatus || CEpgScan::getInstance()->Running() || CStreamManager::getInstance()->StreamStatus();
		if (!alive && cpuFreq)
			cpuFreq->SetCpuFreq(g_settings.standby_cpufreq * 1000 * 1000);

		//fan speed
		if (g_info.hw_caps->has_fan)
			CFanControlNotifier::setSpeed(1);

		if (g_InfoViewer->is_visible)
			g_InfoViewer->killTitle();
		frameBuffer->useBackground(false);
		frameBuffer->paintBackground();
		frameBuffer->setActive(false);

		// Active standby on
		powerManager->SetStandby(false, false);
		if (scansettings.fst_update)
			fst_timer = g_RCInput->addTimer(30*1000*1000, true);
	} else {
		// Active standby off
		powerManager->SetStandby(false, false);
		CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
		CVFD::getInstance()->ShowText("Resume ...");
		if (cpuFreq)
			cpuFreq->SetCpuFreq(g_settings.cpufreq * 1000 * 1000);

#if BOXMODEL_E4HDULTRA
		// reset to users choice
		videoDecoder->SetControl(VIDEO_CONTROL_ZAPPING_MODE, g_settings.zappingmode);
#endif

		videoDecoder->Standby(false);
		CEpgScan::getInstance()->Stop();
		CSectionsdClient::CurrentNextInfo dummy;
		g_InfoViewer->getEPG(0, dummy);
		g_RCInput->killTimer(fst_timer);

#ifdef ENABLE_GRAPHLCD
		cGLCD::Resume();
		cGLCD::StandbyMode(false);
#endif

		if(init_cec_setting){
			//init cec settings
			CCECSetup cecsetup;
			cecsetup.setCECSettings();
			init_cec_setting = false;
		}

		if(!recordingstatus && g_settings.ci_standby_reset) {
			g_CamHandler->exec(NULL, "ca_ci_reset0");
			g_CamHandler->exec(NULL, "ca_ci_reset1");
		}

		frameBuffer->setActive(true);

		//fan speed
		if (g_info.hw_caps->has_fan)
			CFanControlNotifier::setSpeed(g_settings.fan_speed);

		exec_controlscript(NEUTRINO_LEAVE_STANDBY_SCRIPT);

		CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
		CVFD::getInstance()->setBacklight(g_settings.backlight_tv);
		if (g_info.hw_caps->display_has_statusline)
			CVFD::getInstance()->showVolume(g_settings.current_volume, false);

		CZapit::getInstance()->EnablePlayback(true);
		g_Zapit->setStandby(false);
		/* the old code did:
		   if(was_record) g_Zapit->startPlayBack()
		   unfortunately this bypasses the parental PIN code check if a record timer
		   was set on a locked channel, then the box put in standby and after the
		   recording started, the box was woken up.
		   The channelList->setSelected(); channelList->zapTo_ChannelID() sequence
		   does trigger the PIN check
		   If the channel is the same (as during a recording), then it will only
		   check PIN and not zap, so we should be fine here
		 */
		mode = NeutrinoModes::mode_unknown;
		if (lastMode == NeutrinoModes::mode_radio || lastMode == NeutrinoModes::mode_webradio) {
			radioMode( false );
		} else {
			/* for standby -> tv mode from radio mode in case of record */
			frameBuffer->stopFrame();
			tvMode( false );
		}
		t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
		if(!recordingstatus) { //only switch to standby_channel_id when not recording
			live_channel_id = standby_channel_id;
		}
#if 0
		/* todo: check parental pin */
		videoDecoder->Standby(false);
		channelList->setSelected(0xfffffff); /* make sure that zapTo_ChannelID will zap */
		channelList->zapTo_ChannelID(live_channel_id);
#endif
		channelList->zapTo_ChannelID(live_channel_id, true); /* force re-zap */

		g_Sectionsd->setPauseScanning(false);

		InfoClock->enableInfoClock(true);
		InfoIcons->enableInfoIcons(true); //NI InfoIcons

		// remove standby flag
		if (access("/tmp/.standby", F_OK) == 0)
			unlink("/tmp/.standby");

		g_audioMute->AudioMute(current_muted, true);
		StartSubtitles();
	}
	lockStandbyCall = false;
}

void CNeutrinoApp::radioMode(bool rezap)
{
	//printf("radioMode: rezap %s\n", rezap ? "yes" : "no");
	INFO("rezap %d current mode %s", rezap, neutrinoMode_to_string(mode));
	if (mode == NeutrinoModes::mode_webtv) {
		CMoviePlayerGui::getInstance().setLastMode(NeutrinoModes::mode_unknown);
		CMoviePlayerGui::getInstance().stopPlayBack();
		CVFD::getInstance()->ShowIcon(FP_ICON_TV, false);
	}
	if (mode == NeutrinoModes::mode_tv) {
		CVFD::getInstance()->ShowIcon(FP_ICON_TV, false);
		StopSubtitles();
	}
	g_InfoViewer->setUpdateTimer(LCD_UPDATE_TIME_RADIO_MODE);
	CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
	CVFD::getInstance()->ShowIcon(FP_ICON_RADIO, true);

	if( mode == NeutrinoModes::mode_standby ) {
		CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
		videoDecoder->Standby(false);
	}

#ifdef ENABLE_PIP
	if (g_info.hw_caps->can_pip)
	{
		if (pipVideoDecoder[0])
			pipVideoDecoder[0]->Pig(pip_recalc_pos_x(g_settings.pip_radio_x), pip_recalc_pos_y(g_settings.pip_radio_y), g_settings.pip_radio_width, g_settings.pip_radio_height, frameBuffer->getScreenWidth(true), frameBuffer->getScreenHeight(true));
	}
#endif

	CRecordManager::getInstance()->StopAutoRecord();

	if (mode != NeutrinoModes::mode_webtv && mode != NeutrinoModes::mode_webradio) {
		/*
		  FIXME:
		  frameBuffer->paintBackground() is clearing display.
		  What if any gui-element (e.g. infoviewer) is active?
		*/
		frameBuffer->useBackground(false);
		frameBuffer->paintBackground();
	}
	mode = NeutrinoModes::mode_radio;

	g_RemoteControl->radioMode();
	SetChannelMode(g_settings.channel_mode_radio);

	if (g_settings.radiotext_enable && !g_Radiotext)
		g_Radiotext = new CRadioText;

	if( rezap )
		channelRezap();
	frameBuffer->showFrame("radiomode.jpg");

	videoDecoder->SetSyncMode((AVSYNC_TYPE)AVSYNC_DISABLED);
	audioDecoder->SetSyncMode((AVSYNC_TYPE)AVSYNC_DISABLED);
}

void CNeutrinoApp::channelRezap()
{
	t_channel_id last_chid = 0;
	if (mode == NeutrinoModes::mode_tv)
		last_chid = CZapit::getInstance()->GetLastTVChannel();
	else if (mode == NeutrinoModes::mode_radio)
		last_chid = CZapit::getInstance()->GetLastRADIOChannel();
	else
		return;

	if(CServiceManager::getInstance()->FindChannel(last_chid))
		channelList->zapTo_ChannelID(last_chid, true);
	else
		channelList->zapTo(0, true);
}

//switching from current mode to tv or radio mode or to optional parameter prev_mode
void CNeutrinoApp::switchTvRadioMode(const int prev_mode)
{
	if (prev_mode != NeutrinoModes::mode_unknown){
		if (prev_mode == NeutrinoModes::mode_tv && mode != NeutrinoModes::mode_tv )
			tvMode();
		else if(prev_mode == NeutrinoModes::mode_radio && mode != NeutrinoModes::mode_radio)
			radioMode();
	} else {
		if (mode == NeutrinoModes::mode_radio || mode == NeutrinoModes::mode_webradio)
			tvMode();
		else if(mode == NeutrinoModes::mode_tv || mode == NeutrinoModes::mode_webtv)
			radioMode();
	}
}

#if ENABLE_PIP
#if !HAVE_CST_HARDWARE && !HAVE_GENERIC_HARDWARE
void CNeutrinoApp::StartAVInputPiP()
{
	if (!g_info.hw_caps->can_pip)
		return;

	if (!pipVideoDemux[0])
	{
		pipVideoDemux[0] = new cDemux(1);
		pipVideoDemux[0]->Open(DMX_VIDEO_CHANNEL);
	}

	if (!pipVideoDecoder[0])
		pipVideoDecoder[0] = new cVideo(0, NULL, NULL, 1);

	if (pipVideoDemux[0] && pipVideoDecoder[0])
	{
		pipVideoDemux[0]->SetSource(1, 2);
		pipVideoDecoder[0]->SetStreamType((VIDEO_FORMAT) 1);
		pipVideoDemux[0]->Start();
		pipVideoDecoder[0]->Start(0, 0, 0);
		pipVideoDecoder[0]->open_AVInput_Device();
		pipVideoDecoder[0]->Pig(pip_recalc_pos_x(g_settings.pip_x), pip_recalc_pos_y(g_settings.pip_y), g_settings.pip_width, g_settings.pip_height, g_settings.screen_width, g_settings.screen_height);
		pipVideoDecoder[0]->ShowPig(1);
	}
	avinput_pip = true;
}

void CNeutrinoApp::StopAVInputPiP()
{
	if (!g_info.hw_caps->can_pip || !avinput_pip)
		return;

	if (pipVideoDemux[0])
	{
		pipVideoDemux[0]->Stop();
		delete pipVideoDemux[0];
		pipVideoDemux[0] = NULL;
	}
	if (pipVideoDecoder[0])
	{
		pipVideoDecoder[0]->ShowPig(0);
		pipVideoDecoder[0]->Stop();
		pipVideoDecoder[0]->close_AVInput_Device();
		pipVideoDecoder[0]->closeDevice();
		delete pipVideoDecoder[0];
		pipVideoDecoder[0] = NULL;
	}
	avinput_pip = false;
}
#endif
#endif

/**************************************************************************************
*          CNeutrinoApp -  exec, menuitem callback (shutdown)                         *
**************************************************************************************/

int CNeutrinoApp::exec(CMenuTarget* parent, const std::string & actionKey)
{
	//	printf("ac: %s\n", actionKey.c_str());
	int returnval = menu_return::RETURN_REPAINT;

	if (actionKey == "help_recording") {
		ShowMsg(LOCALE_SETTINGS_HELP, LOCALE_RECORDINGMENU_HELP, CMsgBox::mbrBack, CMsgBox::mbBack);
	}
	else if (actionKey=="shutdown")
	{
		ExitRun(g_info.hw_caps->can_shutdown);
	}
	else if(actionKey=="need_reboot")
	{
		int reboot_now = ShowMsg(LOCALE_MESSAGEBOX_INFO, LOCALE_MESSAGEBOX_NEED_REBOOT, CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo, NEUTRINO_ICON_SETTINGS);
		if (reboot_now == CMsgBox::mbrYes)
			CNeutrinoApp::getInstance()->exec(NULL, "reboot");
	}
	else if (actionKey=="reboot")
	{
		ExitRun(CNeutrinoApp::EXIT_REBOOT);
		returnval = menu_return::RETURN_NONE;
	}
	else if (actionKey=="restart")
	{
		videoDecoder->SetCECMode((VIDEO_HDMI_CEC_MODE)0);
		ExitRun(CNeutrinoApp::EXIT_RESTART);
		returnval = menu_return::RETURN_NONE;
	}
	else if (actionKey=="clock_switch")
	{
		InfoClock->switchClockOnOff();
		returnval = menu_return::RETURN_EXIT_ALL;
	}
	else if (actionKey=="tv_radio_switch")//used in mainmenu
	{
		switchTvRadioMode();
		returnval = menu_return::RETURN_EXIT_ALL;
	}
	else if (actionKey=="tv")//used in mainmenu
	{
		switchTvRadioMode(NeutrinoModes::mode_tv);
		returnval = menu_return::RETURN_EXIT_ALL;
	}
	else if (actionKey=="radio")//used in mainmenu
	{
		switchTvRadioMode(NeutrinoModes::mode_radio);
		returnval = menu_return::RETURN_EXIT_ALL;
	}
	else if (actionKey=="avinput") {
		g_RCInput->postMsg( NeutrinoMessages::AVINPUT_ON, 0 );
		returnval = menu_return::RETURN_EXIT_ALL;
	}

#if ENABLE_PIP
#if !HAVE_CST_HARDWARE && !HAVE_GENERIC_HARDWARE
	else if (actionKey=="avinput_pip") {
		if (CZapit::getInstance()->GetPipChannelID())
			CZapit::getInstance()->StopPip();

		if (!avinput_pip)
			StartAVInputPiP();
		else
			StopAVInputPiP();

		returnval = menu_return::RETURN_EXIT_ALL;
	}
#endif
#endif

	else if (actionKey=="savesettings")
	{
		CLoaderHint *lh = new CLoaderHint(LOCALE_MAINSETTINGS_SAVESETTINGSNOW_HINT);
		lh->paint();

		saveSetup(NEUTRINO_SETTINGS_FILE);

		if(g_settings.cacheTXT) {
			tuxtxt_init();
		} else
			tuxtxt_close();

		//g_Sectionsd->setEventsAreOldInMinutes((unsigned short) (g_settings.epg_old_hours*60));
		//g_Sectionsd->setHoursToCache((unsigned short) (g_settings.epg_cache_days*24));

		delete lh;
	}
	else if (actionKey=="recording")
	{
		setupRecordingDevice();
	}
	else if (actionKey=="reloadplugins")
	{
		CLoaderHint *lh = new CLoaderHint(LOCALE_SERVICEMENU_GETPLUGINS_HINT);
		lh->paint();

		g_Plugins->loadPlugins();

		delete lh;
	}
	else if (actionKey=="restarttuner")
	{
		CLoaderHint *lh = new CLoaderHint(LOCALE_SERVICEMENU_RESTARTING_TUNER);
		lh->paint();

		g_Zapit->setStandby(true);
		sleep(2);
		g_Zapit->setStandby(false);
		sleep(2);
		g_Zapit->Rezap();

		delete lh;
	}
	else if (actionKey == "camd_reset")
	{
		CLoaderHint *lh = new CLoaderHint(LOCALE_CAMD_MSG_RESET);
		lh->paint();

		printf("[neutrino.cpp] executing \"service camd restart\"\n");
		if (my_system(3, "service", "camd", "restart") != 0)
			printf("[neutrino.cpp] executing failed\n");

		delete lh;

		return menu_return::RETURN_EXIT_ALL;
	}
	else if (actionKey == "ecmInfo")
	{
		char *buffer = NULL;
		ssize_t read;
		size_t len;
		FILE *fh;

		CFileHelpers fhlp;
		if (fhlp.copyFile("/tmp/ecm.info", "/tmp/ecm.info.tmp", 0644))
		{
			if ((fh = fopen("/tmp/ecm.info.tmp", "r")))
			{
				std::string str = "";
				while ((read = getline(&buffer, &len, fh)) != -1)
				{
					str += buffer;
				}
				fclose(fh);
				remove("/tmp/ecm.info.tmp");
				if(buffer)
					free(buffer);
				ShowHint(LOCALE_ECMINFO, str.c_str(), 450, 20);
			}
		}
		else
			ShowHint(LOCALE_ECMINFO, LOCALE_ECMINFO_NULL, 450, 20);

		return menu_return::RETURN_EXIT_ALL;
	}
	else if (actionKey=="tsmoviebrowser" || actionKey=="fileplayback_video" || actionKey=="fileplayback_audio")
	{
		frameBuffer->Clear();
		if (mode == NeutrinoModes::NeutrinoModes::mode_radio || mode == NeutrinoModes::NeutrinoModes::mode_webradio)
			frameBuffer->stopFrame();
		int prev_mode = mode;
		// FIXME CMediaPlayerMenu::getInstance()->exec(NULL, actionKey); ??
		CMoviePlayerGui::getInstance().exec(NULL, actionKey);
		if (prev_mode == NeutrinoModes::NeutrinoModes::mode_radio || prev_mode == NeutrinoModes::NeutrinoModes::mode_webradio)
			frameBuffer->showFrame("radiomode.jpg");
#if 0
		else if (prev_mode == NeutrinoModes::mode_webtv)
			tvMode(true);
#endif
		return menu_return::RETURN_EXIT_ALL;
	}
	else if (actionKey=="audioplayer" || actionKey == "inetplayer") {
		frameBuffer->Clear();
		CMediaPlayerMenu * media = CMediaPlayerMenu::getInstance();
		media->exec(NULL, actionKey);
		return menu_return::RETURN_EXIT_ALL;
	}
	else if (actionKey=="restart") {
		//usage of slots from any classes
		OnBeforeRestart();

		//cleanup progress bar cache
		CProgressBarCache::pbcClear();

		if (recordingstatus)
			DisplayErrorMessage(g_Locale->getText(LOCALE_SERVICEMENU_RESTART_REFUSED_RECORDING));
		else {
			CHint * hint = new CHint(LOCALE_SERVICEMENU_RESTART_HINT);
			hint->paint();

#ifdef ENABLE_LCD4LINUX
			if (g_settings.lcd4l_support)
				CLCD4l::getInstance()->StopLCD4l();
#endif

			saveSetup(NEUTRINO_SETTINGS_FILE);

			/* this is an ugly mess :-( */
			delete g_RCInput;
			delete g_Sectionsd;
			delete g_RemoteControl;
			delete g_fontRenderer;
			delete g_fixedFontRenderer;
			delete g_dynFontRenderer;
			delete g_shellFontRenderer;
			delete g_iconFontRenderer;

			delete hint;

			stop_daemons(true);
			delete CVFD::getInstance();
			delete SHTDCNT::getInstance();
			stop_video();
			/* g_Timerd, g_Zapit and CVFD are used in stop_daemons */
			delete g_Timerd;
			delete g_Zapit; //do we really need this?

			for(int i = 3; i < 256; i++)
				close(i);

			execvp(global_argv[0], global_argv); // no return if successful

			exit(CNeutrinoApp::EXIT_REBOOT); // should never be reached
		}
	}
	else if (actionKey == "moviedir") {
		parent->hide();

		chooserDir(g_settings.network_nfs_moviedir, false, NULL);

		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "clearSectionsd")
	{
		g_Sectionsd->freeMemory();
	}
	else if (actionKey == "channels")
		return showChannelList(CRCInput::RC_ok, true);
	else if (actionKey == "standby")
	{
		g_RCInput->postMsg(NeutrinoMessages::STANDBY_ON, 0);
		return menu_return::RETURN_EXIT_ALL;
	}
	//NI
	else if (actionKey == "blank_screen") {
		INFO("blank_screen on");
		blank_screen = true;
		frameBuffer->paintBackground(); //clear entire screen
#if HAVE_ARM_HARDWARE
		/*
		   Hack to get sure we have a blank screen.
		   stopFrame()-function seems not work correctly on ARM_HARDWARE
		*/
		frameBuffer->showFrame("blackscreen.jpg");
#endif
		frameBuffer->stopFrame();
		videoDecoder->setBlank(blank_screen);
		returnval = menu_return::RETURN_EXIT_ALL;
	}

	return returnval;
}

/**************************************************************************************
*          changeNotify - features menu recording start / stop                        *
**************************************************************************************/
bool CNeutrinoApp::changeNotify(const neutrino_locale_t OptionName, void * /*data*/)
{
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LANGUAGESETUP_SELECT))
	{
		g_Locale->loadLocale(g_settings.language.c_str());
		return true;
	}
	return false;
}

void CNeutrinoApp::stopDaemonsForFlash()
{
	stop_daemons(false, true);
}

/**************************************************************************************
*          Main programm - no function here                                           *
**************************************************************************************/
void stop_daemons(bool stopall, bool for_flash)
{
	CMoviePlayerGui::getInstance().stopPlayBack();
	if (for_flash)
	{
		if ( FILE *f = fopen("/tmp/.flash.start", "w") )
			fclose(f);

		CVFD::getInstance()->Clear();
		CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
		CVFD::getInstance()->ShowText("Stop daemons...");
		g_settings.epg_scan_mode = CEpgScan::MODE_OFF;
//NI
#ifdef BOXMODEL_CST_HD2
		std::string backup_flash_sh = find_executable("backup_flash.sh");
		if (!backup_flash_sh.empty())
			system(backup_flash_sh.c_str()); //don't fork
#endif
		std::string start_flash_sh = find_executable("start_flash.sh");
		if (!start_flash_sh.empty())
			my_system(start_flash_sh.c_str());
	}

	InfoClock->enableInfoClock(false);
	InfoIcons->enableInfoIcons(false); //NI InfoIcons
	dvbsub_close();
	tuxtxt_stop();
	tuxtxt_close();

#ifdef ENABLE_LCD4LINUX
	if (g_settings.lcd4l_support)
		CLCD4l::getInstance()->StopLCD4l();
	delete CLCD4l::getInstance();
#endif
#ifdef ENABLE_GRAPHLCD
	cGLCD::Exit();
#endif

	if (g_Radiotext) {
		delete g_Radiotext;
		g_Radiotext = NULL;
	}
	printf("streaming shutdown\n");
	CStreamManager::getInstance()->Stop();
	printf("streaming shutdown done\n");
	if(stopall || for_flash) {
		printf("timerd shutdown\n");
		if (g_Timerd)
			g_Timerd->shutdown();
		if (timerd_thread_started)
			pthread_join(timer_thread, NULL);
		printf("timerd shutdown done\n");
	}
#ifndef DISABLE_SECTIONSD
	printf("sectionsd shutdown\n");
	CEitManager::getInstance()->Stop();
	printf("sectionsd shutdown done\n");
#endif
	tuxtx_stop_subtitle();
	printf("zapit shutdown\n");
	if(!for_flash && !stopall && g_settings.hdmi_cec_mode && g_settings.hdmi_cec_standby)
	{
		videoDecoder->SetCECMode((VIDEO_HDMI_CEC_MODE)0);
	}
	if(InfoClock)
		delete InfoClock;
	if(FileTimeOSD)
		delete FileTimeOSD;
	delete &CMoviePlayerGui::getInstance();

	CZapit::getInstance()->Stop();
	printf("zapit shutdown done\n");
	if (!for_flash) {
		CVFD::getInstance()->Clear();
		CVFD::getInstance()->setBacklight(g_settings.backlight_deepstandby);
	}
	if(stopall && !for_flash) {
		if (cpuFreq) {
			cpuFreq->SetCpuFreq(g_settings.cpufreq * 1000 * 1000);
			delete cpuFreq;
		}

		if (powerManager) {
			/* if we were in standby, leave it otherwise, the next
			   start of neutrino will fail in "_write_gxa" in
			   framebuffer.cpp
			   => this is needed because the drivers are crap :( */
			powerManager->SetStandby(false, false);
			powerManager->Close();
			delete powerManager;
		}
		cs_deregister_messenger();
	}

	delete CFlashUpdateCheck::getInstance();

	if (for_flash) {
		delete cHddStat::getInstance();
		delete CRecordManager::getInstance();
		delete videoDemux;
		int ret = my_system(4, "mount", "-no", "remount,ro", "/");
		printf("remount rootfs readonly %s.\n", (ret == 0)?"successful":"failed"); fflush(stdout);
	}
}

void stop_video()
{
	CFrameBuffer::getInstance()->paintBackground(); // clear osd
	delete videoDecoder;
	delete videoDemux;
	delete CFrameBuffer::getInstance();
	cs_api_exit();
}

void sighandler (int signum)
{
	signal (signum, SIG_IGN);
	switch (signum) {
	case SIGTERM:
	case SIGINT:
		CVFD::getInstance()->ShowText("Exiting ...");
		delete cHddStat::getInstance();
		delete CRecordManager::getInstance();
		//CNeutrinoApp::getInstance()->saveSetup(NEUTRINO_SETTINGS_FILE);
		stop_daemons();
		CVFD::getInstance()->setMode(CVFD::MODE_SHUTDOWN);
		delete CVFD::getInstance();
		delete SHTDCNT::getInstance();
		stop_video();
		exit(CNeutrinoApp::EXIT_NORMAL);
	default:
		break;
	}
}

int main(int argc, char **argv)
{
	g_Timerd = NULL;
	g_Radiotext = NULL;
	g_Zapit = NULL;
	setDebugLevel(DEBUG_NORMAL);
	signal(SIGTERM, sighandler);	// TODO: consider the following
	signal(SIGINT, sighandler);	// NOTES: The effects of signal() in a multithreaded
	signal(SIGHUP, SIG_IGN);	//        process are unspecified (signal(2))
	/* don't die in streamts.cpp from a SIGPIPE if client disconnects */
	signal(SIGPIPE, SIG_IGN);

	tzset();

	return CNeutrinoApp::getInstance()->run(argc, argv);
}

void CNeutrinoApp::loadKeys(const char *fname)
{
	CConfigFile *tconfig;

	if (fname)
	{
		tconfig = new CConfigFile(',');
		if (!tconfig->loadConfig(fname))
			return;
	}
	else
		tconfig = &configfile;

	// key configuration; alphabetical order
	g_settings.key_bouquet_down = tconfig->getInt32("key_bouquet_down", CRCInput::RC_left);
	g_settings.key_bouquet_up = tconfig->getInt32("key_bouquet_up", CRCInput::RC_right);
	g_settings.key_channelList_addrecord = tconfig->getInt32("key_channelList_addrecord", CRCInput::RC_red);
	g_settings.key_channelList_addremind = tconfig->getInt32("key_channelList_addremind", CRCInput::RC_yellow);
	g_settings.key_channelList_cancel = tconfig->getInt32("key_channelList_cancel", CRCInput::RC_home);
	g_settings.key_channelList_sort = tconfig->getInt32("key_channelList_sort", CRCInput::RC_blue);
	g_settings.key_current_transponder = tconfig->getInt32("key_current_transponder", CRCInput::RC_nokey);
#if BOXMODEL_HD51
	g_settings.key_favorites = tconfig->getInt32("key_favorites", CRCInput::RC_video);
#else
	g_settings.key_favorites = tconfig->getInt32("key_favorites", CRCInput::RC_favorites);
#endif

	g_settings.key_format_mode_active = tconfig->getInt32("key_format_mode_active", 1);
	g_settings.key_help = tconfig->getInt32("key_help", CRCInput::RC_help);
	g_settings.key_lastchannel = tconfig->getInt32("key_lastchannel", CRCInput::RC_0);
	g_settings.key_list_end = tconfig->getInt32("key_list_end", CRCInput::RC_nokey);
	g_settings.key_list_start = tconfig->getInt32("key_list_start", CRCInput::RC_nokey);
	g_settings.key_next43mode = tconfig->getInt32("key_next43mode", CRCInput::RC_nokey);
	g_settings.key_pagedown = tconfig->getInt32("key_channelList_pagedown", CRCInput::RC_page_down);
	g_settings.key_pageup = tconfig->getInt32("key_channelList_pageup", CRCInput::RC_page_up);
	g_settings.key_pic_mode_active = tconfig->getInt32("key_pic_mode_active", 1);
	g_settings.key_pic_size_active = tconfig->getInt32("key_pic_size_active", 1);
#if ENABLE_PIP
	g_settings.key_pip_close = tconfig->getInt32("key_pip_close", CRCInput::RC_prev);
	g_settings.key_pip_close_avinput = tconfig->getInt32("key_pip_close_avinput", CRCInput::RC_nokey);
	g_settings.key_pip_rotate_cw = tconfig->getInt32("key_pip_rotate_cw", CRCInput::RC_nokey);
	g_settings.key_pip_rotate_ccw = tconfig->getInt32("key_pip_rotate_ccw", CRCInput::RC_nokey);
	g_settings.key_pip_setup = tconfig->getInt32("key_pip_setup", CRCInput::RC_nokey);
	g_settings.key_pip_swap = tconfig->getInt32("key_pip_swap", CRCInput::RC_next);
#endif
	g_settings.key_power_off = tconfig->getInt32("key_power_off", CRCInput::RC_standby);
	g_settings.key_quickzap_down = tconfig->getInt32("key_quickzap_down", CRCInput::RC_down);
	g_settings.key_quickzap_up = tconfig->getInt32("key_quickzap_up", CRCInput::RC_up);
	g_settings.key_record = tconfig->getInt32("key_record", CRCInput::RC_record);
	g_settings.key_screenshot = tconfig->getInt32("key_screenshot", CRCInput::RC_games);
	g_settings.key_sleep = tconfig->getInt32("key_sleep", CRCInput::RC_sleep);
	g_settings.key_standby_off_add = tconfig->getInt32("key_standby_off_add", CRCInput::RC_ok);
	g_settings.key_subchannel_down = tconfig->getInt32("key_subchannel_down", CRCInput::RC_left);
	g_settings.key_subchannel_up = tconfig->getInt32("key_subchannel_up", CRCInput::RC_right);
	g_settings.key_switchformat = tconfig->getInt32("key_switchformat", CRCInput::RC_nokey);
#if BOXMODEL_HD51 || BOXMODEL_BRE2ZE4K || BOXMODEL_H7 || BOXMODEL_E4HDULTRA || BOXMODEL_PROTEK4K || BOXMODEL_HD60 || BOXMODEL_HD61 || BOXMODEL_MULTIBOX || BOXMODEL_MULTIBOXSE || BOXMODEL_OSMIO4K || BOXMODEL_OSMIO4KPLUS
	g_settings.key_timeshift = tconfig->getInt32("key_timeshift", CRCInput::RC_nokey); // FIXME
#elif BOXMODEL_VUPLUS_ALL
	g_settings.key_timeshift = tconfig->getInt32("key_timeshift", CRCInput::RC_playpause);
#else
	g_settings.key_timeshift = tconfig->getInt32("key_timeshift", CRCInput::RC_pause);
#endif
#if BOXMODEL_E4HDULTRA || BOXMODEL_PROTEK4K || BOXMODEL_HD61
	g_settings.key_tvradio_mode = tconfig->getInt32("key_tvradio_mode", CRCInput::RC_tv);
#else
	g_settings.key_tvradio_mode = tconfig->getInt32("key_tvradio_mode", CRCInput::RC_nokey);
#endif
	g_settings.key_unlock = tconfig->getInt32("key_unlock", CRCInput::RC_setup);
	g_settings.key_volumedown = tconfig->getInt32("key_volumedown", CRCInput::RC_minus);
	g_settings.key_volumeup = tconfig->getInt32("key_volumeup", CRCInput::RC_plus);
	g_settings.key_zaphistory = tconfig->getInt32("key_zaphistory", CRCInput::RC_home);

	g_settings.mbkey_copy_onefile = tconfig->getInt32("mbkey.copy_onefile", CRCInput::RC_nokey);
	g_settings.mbkey_copy_several = tconfig->getInt32("mbkey.copy_several", CRCInput::RC_nokey);
	g_settings.mbkey_cover = tconfig->getInt32("mbkey.cover", g_settings.key_favorites);
	g_settings.mbkey_cut = tconfig->getInt32("mbkey.cut", CRCInput::RC_nokey);
	g_settings.mbkey_truncate = tconfig->getInt32("mbkey.truncate", CRCInput::RC_nokey);
	g_settings.mbkey_toggle_view_cw = tconfig->getInt32("mbkey.toggle_view_cw", CRCInput::RC_right);
	g_settings.mbkey_toggle_view_ccw = tconfig->getInt32("mbkey.toggle_view_ccw", CRCInput::RC_left);

	g_settings.mpkey_audio = tconfig->getInt32("mpkey.audio", CRCInput::RC_green);
	g_settings.mpkey_bookmark = tconfig->getInt32("mpkey.bookmark", CRCInput::RC_yellow);
	g_settings.mpkey_forward = tconfig->getInt32("mpkey.forward", CRCInput::RC_forward);
	g_settings.mpkey_goto = tconfig->getInt32("mpkey.goto", CRCInput::RC_nokey);
	g_settings.mpkey_next_repeat_mode = tconfig->getInt32("mpkey.next_repeat_mode", CRCInput::RC_nokey);
#if BOXMODEL_HD51 || BOXMODEL_BRE2ZE4K || BOXMODEL_H7 || BOXMODEL_PROTEK4K || BOXMODEL_HD60 || BOXMODEL_HD61 || BOXMODEL_MULTIBOX || BOXMODEL_MULTIBOXSE
	g_settings.mpkey_pause = tconfig->getInt32("mpkey.pause", CRCInput::RC_playpause);
	g_settings.mpkey_play = tconfig->getInt32("mpkey.play", CRCInput::RC_playpause);
#elif BOXMODEL_VUPLUS_ALL
	g_settings.mpkey_pause = tconfig->getInt32("mpkey.pause", CRCInput::RC_playpause);
	g_settings.mpkey_play = tconfig->getInt32("mpkey.play", CRCInput::RC_play);
#else
	g_settings.mpkey_pause = tconfig->getInt32("mpkey.pause", CRCInput::RC_pause);
	g_settings.mpkey_play = tconfig->getInt32("mpkey.play", CRCInput::RC_play);
#endif
	g_settings.mpkey_plugin = tconfig->getInt32("mpkey.plugin", CRCInput::RC_nokey);
	g_settings.mpkey_rewind = tconfig->getInt32("mpkey.rewind", CRCInput::RC_rewind);
	g_settings.mpkey_stop = tconfig->getInt32("mpkey.stop", CRCInput::RC_stop);
	g_settings.mpkey_subtitle = tconfig->getInt32("mpkey.subtitle", CRCInput::RC_sub);
	g_settings.mpkey_time = tconfig->getInt32("mpkey.time", CRCInput::RC_timeshift);

	// key options
	g_settings.bouquetlist_mode = tconfig->getInt32("bouquetlist_mode", SNeutrinoSettings::CHANNELLIST);
	g_settings.menu_left_exit = tconfig->getInt32("menu_left_exit", 0);
	g_settings.repeat_blocker = tconfig->getInt32("repeat_blocker", 450);
	g_settings.repeat_genericblocker = tconfig->getInt32("repeat_genericblocker", 100);
	g_settings.sms_channel = tconfig->getInt32("sms_channel", 0);
	g_settings.sms_movie = tconfig->getInt32("sms_movie", 1);

	g_settings.longkeypress_duration = tconfig->getInt32("longkeypress_duration", LONGKEYPRESS_OFF);

	g_settings.mode_left_right_key_tv = tconfig->getInt32("mode_left_right_key_tv", SNeutrinoSettings::VZAP);

	if (fname)
		delete tconfig;
}

void CNeutrinoApp::saveKeys(const char *fname)
{
	CConfigFile *tconfig;

	if (fname)
		tconfig = new CConfigFile(',');
	else
		tconfig = &configfile;

	// key configuration; alphabetical order
	tconfig->setInt32("key_bouquet_down", g_settings.key_bouquet_down);
	tconfig->setInt32("key_bouquet_up", g_settings.key_bouquet_up);
	tconfig->setInt32("key_channelList_addrecord", g_settings.key_channelList_addrecord);
	tconfig->setInt32("key_channelList_addremind", g_settings.key_channelList_addremind);
	tconfig->setInt32("key_channelList_cancel", g_settings.key_channelList_cancel);
	tconfig->setInt32("key_channelList_pagedown", g_settings.key_pagedown);
	tconfig->setInt32("key_channelList_pageup", g_settings.key_pageup);
	tconfig->setInt32("key_channelList_sort", g_settings.key_channelList_sort);
	tconfig->setInt32("key_current_transponder", g_settings.key_current_transponder);
	tconfig->setInt32("key_favorites", g_settings.key_favorites);
	tconfig->setInt32("key_format_mode_active", g_settings.key_format_mode_active);
	tconfig->setInt32("key_help", g_settings.key_help);
	tconfig->setInt32("key_lastchannel", g_settings.key_lastchannel);
	tconfig->setInt32("key_list_end", g_settings.key_list_end);
	tconfig->setInt32("key_list_start", g_settings.key_list_start);
	tconfig->setInt32("key_next43mode", g_settings.key_next43mode);
	tconfig->setInt32("key_pic_mode_active", g_settings.key_pic_mode_active);
	tconfig->setInt32("key_pic_size_active", g_settings.key_pic_size_active);
#if ENABLE_PIP
	tconfig->setInt32("key_pip_close", g_settings.key_pip_close);
	tconfig->setInt32("key_pip_close_avinput", g_settings.key_pip_close_avinput);
	tconfig->setInt32("key_pip_rotate_cw", g_settings.key_pip_rotate_cw);
	tconfig->setInt32("key_pip_rotate_ccw", g_settings.key_pip_rotate_ccw);
	tconfig->setInt32("key_pip_setup", g_settings.key_pip_setup);
	tconfig->setInt32("key_pip_swap", g_settings.key_pip_swap);
#endif
	tconfig->setInt32("key_power_off", g_settings.key_power_off);
	tconfig->setInt32("key_quickzap_down", g_settings.key_quickzap_down);
	tconfig->setInt32("key_quickzap_up", g_settings.key_quickzap_up);
	tconfig->setInt32("key_record", g_settings.key_record);
	tconfig->setInt32("key_screenshot", g_settings.key_screenshot);
	tconfig->setInt32("key_sleep", g_settings.key_sleep);
	tconfig->setInt32("key_standby_off_add", g_settings.key_standby_off_add);
	tconfig->setInt32("key_subchannel_down", g_settings.key_subchannel_down);
	tconfig->setInt32("key_subchannel_up", g_settings.key_subchannel_up);
	tconfig->setInt32("key_switchformat", g_settings.key_switchformat);
	tconfig->setInt32("key_timeshift", g_settings.key_timeshift);
	tconfig->setInt32("key_tvradio_mode", g_settings.key_tvradio_mode);
	tconfig->setInt32("key_unlock", g_settings.key_unlock);
	tconfig->setInt32("key_volumedown", g_settings.key_volumedown);
	tconfig->setInt32("key_volumeup", g_settings.key_volumeup);
	tconfig->setInt32("key_zaphistory", g_settings.key_zaphistory);

	tconfig->setInt32("mbkey.copy_onefile", g_settings.mbkey_copy_onefile);
	tconfig->setInt32("mbkey.copy_several", g_settings.mbkey_copy_several);
	tconfig->setInt32("mbkey.cover", g_settings.mbkey_cover);
	tconfig->setInt32("mbkey.cut", g_settings.mbkey_cut);
	tconfig->setInt32("mbkey.truncate", g_settings.mbkey_truncate);
	tconfig->setInt32("mbkey.toggle_view_cw", g_settings.mbkey_toggle_view_cw);
	tconfig->setInt32("mbkey.toggle_view_ccw", g_settings.mbkey_toggle_view_ccw);

	tconfig->setInt32("mpkey.audio", g_settings.mpkey_audio);
	tconfig->setInt32("mpkey.bookmark", g_settings.mpkey_bookmark);
	tconfig->setInt32("mpkey.forward", g_settings.mpkey_forward);
	tconfig->setInt32("mpkey.goto", g_settings.mpkey_goto);
	tconfig->setInt32("mpkey.next_repeat_mode", g_settings.mpkey_next_repeat_mode);
	tconfig->setInt32("mpkey.pause", g_settings.mpkey_pause);
	tconfig->setInt32("mpkey.play", g_settings.mpkey_play);
	tconfig->setInt32("mpkey.plugin", g_settings.mpkey_plugin);
	tconfig->setInt32("mpkey.rewind", g_settings.mpkey_rewind);
	tconfig->setInt32("mpkey.stop", g_settings.mpkey_stop);
	tconfig->setInt32("mpkey.subtitle", g_settings.mpkey_subtitle);
	tconfig->setInt32("mpkey.time", g_settings.mpkey_time);

	// key options
	tconfig->setInt32("bouquetlist_mode", g_settings.bouquetlist_mode);
	tconfig->setInt32("menu_left_exit", g_settings.menu_left_exit);
	tconfig->setInt32("repeat_blocker", g_settings.repeat_blocker);
	tconfig->setInt32("repeat_genericblocker", g_settings.repeat_genericblocker);
	tconfig->setInt32("sms_channel", g_settings.sms_channel);
	tconfig->setInt32("sms_movie", g_settings.sms_movie);

	tconfig->setInt32("longkeypress_duration", g_settings.longkeypress_duration);

	tconfig->setInt32("mode_left_right_key_tv", g_settings.mode_left_right_key_tv);

	if (fname)
	{
		tconfig->saveConfig(fname);
		delete tconfig;
	}
}

void CNeutrinoApp::StopSubtitles(bool enable_glcd_mirroring)
{
	//printf("[neutrino] %s\n", __FUNCTION__);
	int ttx, dvbpid, ttxpid, ttxpage;

	dvbpid = dvbsub_getpid();
	tuxtx_subtitle_running(&ttxpid, &ttxpage, &ttx);

	if(dvbpid)
		dvbsub_pause();
	if(ttx) {
		tuxtx_pause_subtitle(true);
		frameBuffer->paintBackground();
	}
#ifdef ENABLE_GRAPHLCD
	if (enable_glcd_mirroring)
		cGLCD::MirrorOSD(g_settings.glcd_mirror_osd);
#else
	(void) enable_glcd_mirroring; // avoid compiler warning
#endif
	if (mode == NeutrinoModes::mode_webtv)
		CMoviePlayerGui::getInstance(true).clearSubtitle(true);
}

void CNeutrinoApp::StartSubtitles(bool show)
{
	//printf("%s: %s\n", __FUNCTION__, show ? "Show" : "Not show");
#ifdef ENABLE_GRAPHLCD
	cGLCD::MirrorOSD(false);
#endif
	if(!show)
		return;
	dvbsub_start(0);
	tuxtx_pause_subtitle(false);
	if (mode == NeutrinoModes::mode_webtv)
		CMoviePlayerGui::getInstance(true).clearSubtitle(false);
}

void CNeutrinoApp::SelectSubtitles()
{
	/* called on NeutrinoMessages::EVT_ZAP_COMPLETE, should be safe to use zapit current channel */
	CZapitChannel * cc = CZapit::getInstance()->GetCurrentChannel();

	if(!g_settings.auto_subs || cc == NULL)
		return;

	for(int i = 0; i < 3; i++) {
		if(g_settings.pref_subs[i].empty() || g_settings.pref_subs[i] == "none")
			continue;

		std::string temp(g_settings.pref_subs[i]);

		for(int j = 0 ; j < (int)cc->getSubtitleCount() ; j++) {
			CZapitAbsSub* s = cc->getChannelSub(j);
			if (s->thisSubType == CZapitAbsSub::DVB) {
				CZapitDVBSub* sd = reinterpret_cast<CZapitDVBSub*>(s);
				std::map<std::string, std::string>::const_iterator it;
				for(it = iso639.begin(); it != iso639.end(); ++it) {
					if(temp == it->second && sd->ISO639_language_code == it->first) {
						printf("CNeutrinoApp::SelectSubtitles: found DVB %s, pid %x\n", sd->ISO639_language_code.c_str(), sd->pId);
						dvbsub_stop();
						dvbsub_setpid(sd->pId);
						return;
					}
				}
			}
		}
		for(int j = 0 ; j < (int)cc->getSubtitleCount() ; j++) {
			CZapitAbsSub* s = cc->getChannelSub(j);
			if (s->thisSubType == CZapitAbsSub::TTX) {
				CZapitTTXSub* sd = reinterpret_cast<CZapitTTXSub*>(s);
				std::map<std::string, std::string>::const_iterator it;
				for(it = iso639.begin(); it != iso639.end(); ++it) {
					if(temp == it->second && sd->ISO639_language_code == it->first) {
						int page = ((sd->teletext_magazine_number & 0xFF) << 8) | sd->teletext_page_number;
						printf("CNeutrinoApp::SelectSubtitles: found TTX %s, pid %x page %03X\n", sd->ISO639_language_code.c_str(), sd->pId, page);
						tuxtx_stop_subtitle();
						tuxtx_set_pid(sd->pId, page, (char *) sd->ISO639_language_code.c_str());
						return;
					}
				}
			}
		}
	}
}

void CNeutrinoApp::SDT_ReloadChannels()
{
	SDTreloadChannels = false;
	//g_Zapit->reinitChannels();
	channelsInit();
	t_channel_id live_channel_id = CZapit::getInstance()->GetCurrentChannelID();
	adjustToChannelID(live_channel_id);//FIXME what if deleted ?
	if(old_b_id >= 0) {
		bouquetList->activateBouquet(old_b_id, false);
		old_b_id = -1;
		g_RCInput->postMsg(CRCInput::RC_ok, 0);
	}
}

void CNeutrinoApp::getAnnounceEpgName(CTimerd::RecordingInfo * eventinfo, std::string &name)
{

	name += "\n";

	std::string zAddData = CServiceManager::getInstance()->GetServiceName(eventinfo->channel_id);
	if( zAddData.empty()) {
		zAddData = g_Locale->getText(LOCALE_TIMERLIST_PROGRAM_UNKNOWN);
	}

	if(eventinfo->epg_id!=0) {
		CEPGData epgdata;
		zAddData += " :\n";
		if (CEitManager::getInstance()->getEPGid(eventinfo->epg_id, eventinfo->epg_starttime, &epgdata)) {
			zAddData += epgdata.title;
		}
		else if(strlen(eventinfo->epgTitle)!=0) {
			zAddData += eventinfo->epgTitle;
		}
	}
	else if(strlen(eventinfo->epgTitle)!=0) {
		zAddData += " :\n";
		zAddData += eventinfo->epgTitle;
	}

	name += zAddData;
}

#if ENABLE_PIP
bool CNeutrinoApp::StartPip(const t_channel_id channel_id, int pip)
{
	bool ret = false;
	if (!g_info.hw_caps->can_pip)
		return ret;

	CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(channel_id);
	if (!channel)
		return ret;

	if (channel->getRecordDemux() == channel->getPipDemux())
		CStreamManager::getInstance()->StopStream(channel_id);

	int recmode = CRecordManager::getInstance()->GetRecordMode(channel_id);
	if ((recmode == CRecordManager::RECMODE_OFF) || (channel->getRecordDemux() != channel->getPipDemux()))
	{
		if (!g_Zapit->zapTo_pip(channel_id, pip))
		{
#if BOXMODEL_HISILICON
			if (pipVideoDecoder[0])
				pipVideoDecoder[0]->ShowPig(0);
#endif
			DisplayErrorMessage(g_Locale->getText(LOCALE_VIDEOMENU_PIP_ERROR));
		}
		else
			ret = true;
	}
	return ret;
}

int CNeutrinoApp::pip_recalc_pos_x(int x)
{
	int new_x = x;
	switch(g_settings.pip_rotate_lastpos)
	{
		case PIP_UP_LEFT:
		case PIP_DOWN_LEFT:
			new_x = x;
			break;
		case PIP_UP_RIGHT:
		case PIP_DOWN_RIGHT:
			new_x = g_settings.screen_width - g_settings.pip_width - x;
			break;
		case PIP_NO_POS:
		default:
			break;
	}
	return new_x;
}

int CNeutrinoApp::pip_recalc_pos_y(int y)
{
	int new_y = y;
	switch(g_settings.pip_rotate_lastpos)
	{
		case PIP_UP_LEFT:
		case PIP_UP_RIGHT:
			new_y = y;
			break;
		case PIP_DOWN_RIGHT:
		case PIP_DOWN_LEFT:
			new_y = g_settings.screen_height - g_settings.pip_height - y;
			break;
		case PIP_NO_POS:
		default:
			break;
	}
	return new_y;
}

void CNeutrinoApp::pip_rotate(int cw)
{
	if (pipVideoDecoder[0] && !pipVideoDecoder[0]->getBlank())
	{
		g_settings.pip_rotate_lastpos += cw;
		if (g_settings.pip_rotate_lastpos < PIP_UP_LEFT)
			g_settings.pip_rotate_lastpos = PIP_DOWN_LEFT;
		if (g_settings.pip_rotate_lastpos > PIP_DOWN_LEFT)
			g_settings.pip_rotate_lastpos = PIP_UP_LEFT;

		pipVideoDecoder[0]->Pig(pip_recalc_pos_x(g_settings.pip_x), pip_recalc_pos_y(g_settings.pip_y), g_settings.pip_width, g_settings.pip_height, g_settings.screen_width, g_settings.screen_height);
	}
}
#endif

void CNeutrinoApp::Cleanup()
{
	//CLuaServer::destroyInstance();

#ifdef EXIT_CLEANUP
	INFO("cleanup ...");

	printf("cleanup g_Sectionsd\n"); fflush(stdout);
	delete g_Sectionsd; g_Sectionsd = NULL;

	printf("cleanup g_Timerd\n"); fflush(stdout);
	delete g_Timerd; g_Timerd = NULL;

	printf("cleanup g_Zapit\n"); fflush(stdout);
	delete g_Zapit; g_Zapit = NULL;

	printf("cleanup g_Remotecontrol\n"); fflush(stdout);
	delete g_RemoteControl; g_RemoteControl = NULL;

	printf("cleanup g_fontRenderer\n"); fflush(stdout);
	delete g_fontRenderer; g_fontRenderer = NULL;

	printf("cleanup g_fixedFontRenderer\n"); fflush(stdout);
	delete g_fixedFontRenderer; g_fixedFontRenderer = NULL;

	printf("cleanup g_dynFontRenderer\n"); fflush(stdout);
	delete g_dynFontRenderer; g_dynFontRenderer = NULL;

	printf("cleanup g_shellFontRenderer\n"); fflush(stdout);
	delete g_shellFontRenderer; g_shellFontRenderer = NULL;

	printf("cleanup g_iconFontRenderer\n"); fflush(stdout);
	delete g_iconFontRenderer; g_iconFontRenderer = NULL;

	printf("cleanup g_PicViewer\n"); fflush(stdout);
	delete g_PicViewer; g_PicViewer = NULL;

	printf("cleanup g_Plugins\n"); fflush(stdout);
	delete g_Plugins; g_Plugins = NULL;

	printf("cleanup g_CamHandler\n"); fflush(stdout);
	delete g_CamHandler; g_CamHandler = NULL;

	printf("cleanup g_volume\n"); fflush(stdout);
	delete g_volume; g_volume = NULL;

	printf("cleanup g_audioMute\n"); fflush(stdout);
	delete g_audioMute; g_audioMute = NULL;

	printf("cleanup g_EpgData\n"); fflush(stdout);
	delete g_EpgData; g_EpgData = NULL;

	printf("cleanup g_InfoViewer\n"); fflush(stdout);
	delete g_InfoViewer; g_InfoViewer = NULL;

	printf("cleanup g_EventList\n"); fflush(stdout);
	delete g_EventList; g_EventList = NULL;

	printf("cleanup g_Locale\n"); fflush(stdout);
	delete g_Locale; g_Locale = NULL;

	printf("cleanup g_videoSettings\n"); fflush(stdout);
	delete g_videoSettings; g_videoSettings = NULL;

	printf("cleanup g_Radiotext\n"); fflush(stdout);
	delete g_Radiotext; g_Radiotext = NULL;

	printf("cleanup audioSetupNotifier\n"); fflush(stdout);
	delete audioSetupNotifier; audioSetupNotifier = NULL;

	printf("cleanup TVbouquetList\n"); fflush(stdout);
	delete TVbouquetList; TVbouquetList = NULL;

	printf("cleanup RADIObouquetList\n"); fflush(stdout);
	delete RADIObouquetList; RADIObouquetList = NULL;

	printf("cleanup TVfavList\n"); fflush(stdout);
	delete TVfavList; TVfavList = NULL;

	printf("cleanup RADIOfavList\n"); fflush(stdout);
	delete RADIOfavList; RADIOfavList = NULL;

	printf("cleanup TVchannelList\n"); fflush(stdout);
	delete TVchannelList; TVchannelList = NULL;

	printf("cleanup RADIOchannelList\n"); fflush(stdout);
	delete RADIOchannelList; RADIOchannelList = NULL;

	printf("cleanup TVallList\n"); fflush(stdout);
	delete TVallList; TVallList = NULL;

	printf("cleanup RADIOallList\n"); fflush(stdout);
	delete RADIOallList; RADIOallList = NULL;

	printf("cleanup TVsatList\n"); fflush(stdout);
	delete TVsatList; TVsatList = NULL;

	printf("cleanup RADIOsatList\n"); fflush(stdout);
	delete RADIOsatList; RADIOsatList = NULL;

	printf("cleanup g_Font[]\n"); fflush(stdout);
	for (int i = 0; i < SNeutrinoSettings::FONT_TYPE_COUNT; i++)
	{
		delete g_Font[i];
		g_Font[i] = NULL;
	}

	printf("cleanup g_FixedFont[]\n"); fflush(stdout);
	for (int i = 0; i < SNeutrinoSettings::FONT_TYPE_FIXED_COUNT; i++)
	{
		delete g_FixedFont[i];
		g_FixedFont[i] = NULL;
	}

	printf("cleanup g_SignalFont\n"); fflush(stdout);
	delete g_SignalFont; g_SignalFont = NULL;

	printf("cleanup g_ShellFont\n"); fflush(stdout);
	delete g_ShellFont; g_ShellFont = NULL;

	printf("cleanup g_IconFont\n"); fflush(stdout);
	delete g_IconFont; g_IconFont = NULL;

	printf("cleanup g_settings.usermenu[]\n"); fflush(stdout);
	for (unsigned int i = 0; i < g_settings.usermenu.size(); ++i)
	{
		delete g_settings.usermenu[i];
		g_settings.usermenu[i] = NULL;
	}

	printf("cleanup configfile\n"); fflush(stdout);
	configfile.clear();

	printf("cleanup CZapit::getInstance()\n"); fflush(stdout);
	delete CZapit::getInstance();

	printf("cleanup CEitManager::getInstance()\n"); fflush(stdout);
	delete CEitManager::getInstance();

	/*
	printf("cleanup CVFD::getInstance()\n"); fflush(stdout);
	delete CVFD::getInstance();
	*/

	comp_malloc_stats(NULL);
#endif
}

#if ENABLE_FASTSCAN
void CNeutrinoApp::CheckFastScan(bool standby, bool reload)
{
	if (scansettings.fst_update) {
		g_Zapit->getMode();
		INFO("fst version %02x (%s)", scansettings.fst_version, standby ? "force" : "check");
		CServiceScan::getInstance()->QuietFastScan(true);
		int new_fst = scansettings.fst_version;
		if (!standby) {
			if (CServiceScan::getInstance()->ReadFstVersion(scansettings.fast_op))
				new_fst = CServiceScan::getInstance()->GetFstVersion();
		}
		if (standby || (new_fst != scansettings.fst_version)) {
			CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
			CVFD::getInstance()->ShowText(g_Locale->getText(LOCALE_SATSETUP_FASTSCAN_HEAD));
			CHintBox * fhintbox = NULL;
			if (!standby) {
				fhintbox = new CHintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_SATSETUP_FASTSCAN_HEAD));
				fhintbox->paint();
			}
			if (CServiceScan::getInstance()->ScanFast(scansettings.fast_op, reload)) {
				scanSettings.fst_version = CServiceScan::getInstance()->GetFstVersion();
				scanSettings.saveSettings(NEUTRINO_SCAN_SETTINGS_FILE);
			}
			if (fhintbox){
				fhintbox->hide(); delete fhintbox;
			}
			if (standby)
				CVFD::getInstance()->setMode(CVFD::MODE_STANDBY);
		}
	}
}
#else
void CNeutrinoApp::CheckFastScan(bool, bool)
{
}
#endif

bool CNeutrinoApp::adjustToChannelID(const t_channel_id channel_id)
{
	int old_mode = lastChannelMode;
	int new_mode = old_mode;
	bool has_channel = false;
	first_mode_found = -1;

	if (!channelList->adjustToChannelID(channel_id))
		return false;

	channelList->getLastChannels().store (channel_id);
	if(CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_tv
			|| CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_webtv) {
		has_channel = TVfavList->adjustToChannelID(channel_id);
		if (has_channel && first_mode_found < 0)
			first_mode_found = LIST_MODE_FAV;
		if(!has_channel && old_mode == LIST_MODE_FAV)
			new_mode = LIST_MODE_PROV;

		has_channel = TVbouquetList->adjustToChannelID(channel_id);
		if (has_channel && first_mode_found < 0)
			first_mode_found = LIST_MODE_PROV;
		if(!has_channel && old_mode == LIST_MODE_PROV)
			new_mode = LIST_MODE_WEB;

		has_channel = TVwebList->adjustToChannelID(channel_id);
		if (has_channel && first_mode_found < 0)
			first_mode_found = LIST_MODE_WEB;
		if(!has_channel && old_mode == LIST_MODE_WEB)
			new_mode = LIST_MODE_SAT;

		has_channel = TVsatList->adjustToChannelID(channel_id);
		if (has_channel && first_mode_found < 0)
			first_mode_found = LIST_MODE_SAT;
		if(!has_channel && old_mode == LIST_MODE_SAT)
			new_mode = LIST_MODE_ALL;

		TVallList->adjustToChannelID(channel_id);
	}
	else if(CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_radio
			|| CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_webradio) {
		has_channel = RADIOfavList->adjustToChannelID(channel_id);
		if (has_channel && first_mode_found < 0)
			first_mode_found = LIST_MODE_FAV;
		if(!has_channel && old_mode == LIST_MODE_FAV)
			new_mode = LIST_MODE_PROV;

		has_channel = RADIObouquetList->adjustToChannelID(channel_id);
		if (has_channel && first_mode_found < 0)
			first_mode_found = LIST_MODE_PROV;
		if(!has_channel && old_mode == LIST_MODE_PROV)
			new_mode = LIST_MODE_WEB;

		has_channel = RADIOwebList->adjustToChannelID(channel_id);
		if (has_channel && first_mode_found < 0)
			first_mode_found = LIST_MODE_WEB;
		if(!has_channel && old_mode == LIST_MODE_WEB)
			new_mode = LIST_MODE_SAT;

		has_channel = RADIOsatList->adjustToChannelID(channel_id);
		if (has_channel && first_mode_found < 0)
			first_mode_found = LIST_MODE_SAT;
		if(!has_channel && old_mode == LIST_MODE_SAT)
			new_mode = LIST_MODE_ALL;

		RADIOallList->adjustToChannelID(channel_id);
	}
	if(old_mode != new_mode)
		CNeutrinoApp::getInstance()->SetChannelMode(new_mode);

	return true;
}

/*
 * convert config keys, so that users do not need to set up their system again
*/
struct __key_rename {
	const char *from;
	const char *to;
};

static struct __key_rename key_rename[] = {
	{ "casystem_display",	"infobar_casystem_display" },
	{ "casystem_dotmatrix",	"infobar_casystem_dotmatrix"},
	{ "casystem_frame",	"infobar_casystem_frame" },
	{ "screen_StartX_lcd",	"screen_StartX_a_0" },
	{ "screen_StartY_lcd",	"screen_StartY_a_0" },
	{ "screen_EndX_lcd",	"screen_EndX_a_0" },
	{ "screen_EndY_lcd",	"screen_EndY_a_0" },
	{ "screen_StartX_crt",	"screen_StartX_b_0" },
	{ "screen_StartY_crt",	"screen_StartY_b_0" },
	{ "screen_EndX_crt",	"screen_EndX_b_0" },
	{ "screen_EndY_crt",	"screen_EndY_b_0" },
	{ "timing.infobar_movieplayer",	"timing.infobar_media_video" },
	{ "ci_clock", "ci_clock_0" },
	{ "ci_save_pincode", "ci_save_pincode_0" },
	{ "ci_pincode", "ci_pincode_0" },
	{ "ci_ignore_messages", "ci_ignore_messages_0" },
#if BOXMODEL_VUPLUS_ALL
	{ "ci_rpr", "ci_rpr_0" },
#endif
	{ NULL, NULL }
};


/* actually do the migration of the config entries */
void CNeutrinoApp::migrateConfig(const char *fname)
{
	/* we need a second configfile to not create new entries and trigger the
	 * "new entry created" flag */
	CConfigFile migconf('\t', false);
	migconf.loadConfig(fname);
	/* here we do a simple rename of config file keys */
	int magic = -424242; /* obviously a value that does not appear in real cases */
	int i;
	for (i = 0; key_rename[i].from != NULL; i++) {
		const char *from = key_rename[i].from;
		const char *to   = key_rename[i].to;
		int tmp = migconf.getInt32(from, magic);
		if (tmp == magic)	/* old key does not exist */
			continue;
		/* only set new key to old value if the new key does not yet exist */
		if (configfile.getInt32(to, magic) == magic)
			configfile.setInt32(to, tmp);
		/* always remove old key */
		configfile.deleteKey(from);
	}
	/* more complex migration, including converting values etc. could be done here */
}
