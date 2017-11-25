/*
	WebTV Setup

	(C) 2012-2013 by martii


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
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#define __USE_FILE_OFFSET64 1
#include "filebrowser.h"
#include <stdio.h>
#include <global.h>
#include <libgen.h>
#include <neutrino.h>
#include <driver/screen_max.h>
#include <driver/framebuffer.h>
#include <gui/movieplayer.h>
#include <gui/widget/hintbox.h>
#include <zapit/zapit.h>
#include <neutrino_menue.h>
#include "webtv_setup.h"

//NI
#include <dirent.h>
#include <mymenu.h>
#include <system/helpers.h>
#include <zapit/settings.h>

CWebTVSetup::CWebTVSetup()
{
	width = 55;
	selected = -1;
	item_offset = 0;
	changed = false;
}

const CMenuOptionChooser::keyval_ext LIVESTREAM_RESOLUTION_OPTIONS[] =
{
	{ 1920, NONEXISTANT_LOCALE, "1920x1080" },
	{ 1280, NONEXISTANT_LOCALE, "1280x720"  },
	{ 854,  NONEXISTANT_LOCALE, "854x480"   },
	{ 640,  NONEXISTANT_LOCALE, "640x360"   },
	{ 426,  NONEXISTANT_LOCALE, "426x240"   },
	{ 128,  NONEXISTANT_LOCALE, "128x72"    }
};
#define LIVESTREAM_RESOLUTION_OPTION_COUNT (sizeof(LIVESTREAM_RESOLUTION_OPTIONS)/sizeof(CMenuOptionChooser::keyval_ext))

#define CWebTVSetupFooterButtonCount 3 //NI
static const struct button_label CWebTVSetupFooterButtons[CWebTVSetupFooterButtonCount] = {
	{ NEUTRINO_ICON_BUTTON_RED, LOCALE_WEBTV_XML_DEL },
	{ NEUTRINO_ICON_BUTTON_GREEN, LOCALE_WEBTV_XML_ADD },
	{ NEUTRINO_ICON_BUTTON_BLUE, LOCALE_WEBTV_XML_RELOAD } //NI
};

int CWebTVSetup::exec(CMenuTarget* parent, const std::string & actionKey)
{
	int res = menu_return::RETURN_REPAINT;

	if(actionKey == "d" /* delete */) {
		selected = m->getSelected();
		if (selected >= item_offset) {
			m->removeItem(selected);
		    m->hide();
			selected = m->getSelected();
			changed = true;
		}
		return res;
	}
	if(actionKey == "c" /* change */) {
		CFileBrowser fileBrowser;
		CFileFilter fileFilter;
		fileFilter.addFilter("xml");
		fileBrowser.Filter = &fileFilter;
		selected = m->getSelected();
		CMenuItem* item = m->getItem(selected);
		CMenuForwarder *f = static_cast<CMenuForwarder*>(item);
		std::string dirname(f->getName());
		dirname = dirname.substr(0, dirname.rfind('/'));
		if (fileBrowser.exec(dirname.c_str())) {
			f->setName(fileBrowser.getSelectedFile()->Name);
			g_settings.last_webtv_dir = dirname;
			changed = true;
		}
		return res;
	}
	if(actionKey == "a" /* add */) {
		CFileBrowser fileBrowser;
		CFileFilter fileFilter;
		fileFilter.addFilter("xml");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec(g_settings.last_webtv_dir.c_str()) == true) {
			std::string s = fileBrowser.getSelectedFile()->Name;
			m->addItem(new CMenuForwarder(s, true, NULL, this, "c"));
			g_settings.last_webtv_dir = s.substr(0, s.rfind('/')).c_str();
			changed = true;
		}
		return res;
	}
	//NI
	if(actionKey == "r" /* reload */) {
		changed = true;
		return menu_return::RETURN_EXIT_ALL;
	}
	if (actionKey == "script_path") {
		const char *action_str = "ScriptPath";
		chooserDir(g_settings.livestreamScriptPath, false, action_str);
		return res;
	}

	if(parent)
		parent->hide();

	res = Show();

	return res;
}

int CWebTVSetup::Show()
{
	item_offset = 0;

	m = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_MOVIEPLAYER, width, MN_WIDGET_ID_WEBTVSETUP);
	m->addKey(CRCInput::RC_red, this, "d");
	m->addKey(CRCInput::RC_green, this, "a");
	m->addKey(CRCInput::RC_blue, this, "r"); //NI

	m->addIntroItems(LOCALE_WEBTV_HEAD, LOCALE_LIVESTREAM_HEAD);

	bool _mode_webtv = (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_webtv) &&
				(!CZapit::getInstance()->GetCurrentChannel()->getScriptName().empty());

	CMenuForwarder *mf;
	int shortcut = 1;
	mf = new CMenuForwarder(LOCALE_LIVESTREAM_SCRIPTPATH, !_mode_webtv, g_settings.livestreamScriptPath, this, "script_path", CRCInput::convertDigitToKey(shortcut++));
	m->addItem(mf);
#if 0
	mf = new CMenuForwarder(LOCALE_LIVESTREAM_RESOLUTION, _mode_webtv, NULL, new CWebTVResolution(), NULL, CRCInput::convertDigitToKey(shortcut++));
	m->addItem(mf);
#endif

	m->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_WEBTV_XML));

	//NI
	char hint_text[1024];
	snprintf(hint_text, sizeof(hint_text)-1, g_Locale->getText(LOCALE_MENU_HINT_WEBTV_XML_AUTO), WEBTVDIR, WEBTVDIR_VAR);
	CMenuOptionChooser *oc = new CMenuOptionChooser(LOCALE_WEBTV_XML_AUTO, &g_settings.webtv_xml_auto, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this, CRCInput::convertDigitToKey(shortcut++));
	oc->setHint("", hint_text);
	m->addItem(oc);
	m->addItem(GenericMenuSeparator);

	item_offset = m->getItemsCount();
	for (std::list<std::string>::iterator it = g_settings.webtv_xml.begin(); it != g_settings.webtv_xml.end(); ++it)
	{
		//NI
		if (
			   (*it).empty()
			|| ((*it).find(WEBTVDIR) != std::string::npos)
			|| ((*it).find(WEBTVDIR_VAR) != std::string::npos)
		)
			continue;
		m->addItem(new CMenuForwarder(*it, true, NULL, this, "c"));
	}

	m->setFooter(CWebTVSetupFooterButtons, CWebTVSetupFooterButtonCount); //Why we need here an extra buttonbar?

	int res = m->exec(NULL, "");
	m->hide();

	if (changed)
	{
		g_settings.webtv_xml.clear();
		for (int i = item_offset; i < m->getItemsCount(); i++)
		{
			CMenuItem *item = m->getItem(i);
			CMenuForwarder *f = static_cast<CMenuForwarder*>(item);
			g_settings.webtv_xml.push_back(f->getName());
		}
		webtv_xml_auto(); //NI
		g_Zapit->reinitChannels();
		changed = false;
	}

	delete m;
	return res;
}

//NI
bool CWebTVSetup::changeNotify(const neutrino_locale_t, void */*data*/)
{
	changed = true;
	return false;
}

//NI
int xml_filter(const struct dirent *entry)
{
	int len = strlen(entry->d_name);
	if (len > 3 && entry->d_name[len-3] == 'x' && entry->d_name[len-2] == 'm' && entry->d_name[len-1] == 'l')
		return 1;
	return 0;
}

//NI
void CWebTVSetup::webtv_xml_auto()
{
	if (g_settings.webtv_xml_auto)
	{
		const char *dirs[] = {WEBTVDIR_VAR, WEBTVDIR};
		struct dirent **xml_list;
		char xml_file[1024] = {0};
		for (int i = 0; i < 2; i++)
		{
			int xml_cnt = scandir(dirs[i], &xml_list, xml_filter, alphasort);
			if (xml_cnt > -1)
			{
				for (int count = 0; count < xml_cnt; count++)
				{
					snprintf(xml_file, sizeof(xml_file), "%s/%s", dirs[i], xml_list[count]->d_name);
					if (file_size(xml_file))
					{
						bool found = false;
						for (std::list<std::string>::iterator it = g_settings.webtv_xml.begin(); it != g_settings.webtv_xml.end(); it++)
							found |= ((*it).find(xml_list[count]->d_name) != std::string::npos);

						if (!found)
						{
							printf("[CWebTVSetup] loading: %s\n", xml_file);
							g_settings.webtv_xml.push_back(xml_file);
						}
						else
						{
							printf("[CWebTVSetup] skipping: %s\n", xml_file);
						}
					}
				}
			}
		}
	}
}


/* ## CWebTVResolution ############################################# */

CWebTVResolution::CWebTVResolution()
{
	width = 40;
}

int CWebTVResolution::exec(CMenuTarget* parent, const std::string& /*actionKey*/)
{
	if (parent)
		parent->hide();

	return Show();
}

int CWebTVResolution::Show()
{
	m = new CMenuWidget(LOCALE_WEBTV_HEAD, NEUTRINO_ICON_MOVIEPLAYER, width, MN_WIDGET_ID_LIVESTREAM_RESOLUTION);
	m->addIntroItems(LOCALE_LIVESTREAM_HEAD);

	CMenuOptionChooser *mc;
	mc = new CMenuOptionChooser(LOCALE_LIVESTREAM_RESOLUTION, &g_settings.livestreamResolution,
				    LIVESTREAM_RESOLUTION_OPTIONS, LIVESTREAM_RESOLUTION_OPTION_COUNT,
				    true, NULL, CRCInput::RC_nokey, NULL, true);
	m->addItem(mc);

	int oldRes = g_settings.livestreamResolution;
	int res = m->exec(NULL, "");
	m->hide();
	delete m;

	bool _mode_webtv = (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_webtv) &&
				(!CZapit::getInstance()->GetCurrentChannel()->getScriptName().empty());
	if (oldRes != g_settings.livestreamResolution && _mode_webtv) {
		CZapitChannel * cc = CZapit::getInstance()->GetCurrentChannel();
		if (cc && IS_WEBCHAN(cc->getChannelID())) {
			CMoviePlayerGui::getInstance().stopPlayBack();
			CMoviePlayerGui::getInstance().PlayBackgroundStart(cc->getUrl(), cc->getName(), cc->getChannelID(), cc->getScriptName());
		}
	}

	return res;
}

const char *CWebTVResolution::getResolutionValue()
{
	for (unsigned int i = 0; i < LIVESTREAM_RESOLUTION_OPTION_COUNT; ++i)
	{
		if (g_settings.livestreamResolution == LIVESTREAM_RESOLUTION_OPTIONS[i].key)
			return LIVESTREAM_RESOLUTION_OPTIONS[i].valname;
	}
	return "";
}
