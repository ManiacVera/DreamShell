/* DreamShell ##version##

   app_system_menu.h - ISO Loader app utils
   Copyright (C) 2024 Maniac Vera

*/

#include "app_system_menu.h"
#include "app_definition.h"
#include "app_menu.h"
#include "app_utils.h"
#include <tsunami/dsapp.h>
#include <tsunami/font.h>
#include <tsunami/drawables/scene.h>
#include <tsunami/drawables/form.h>
#include <tsunami/drawables/rectangle.h>

extern struct menu_structure menu_data;

enum ViewEnum
{
	SYSTEM_MENU_VIEW = 0
};

static struct
{
	bool first_scan_cover;
	DSApp *dsapp_ptr;
	Scene *scene_ptr;
	Form *system_menu_form;
	Form *optimize_cover_popup;	

	Label *optimize_covers_label;
	Label *title_cover_scan;
	Label *note_cover_scan;
	Label *message_cover_scan;
	Rectangle *modal_cover_scan;

	CheckBox *save_preset_option;
	CheckBox *cover_background_option;
	CheckBox *change_page_with_page_option;
	CheckBox *start_in_last_game_option;	

	Font *message_font;
	Font *menu_font;
} self;

void CreateSystemMenu(DSApp *dsapp_ptr, Scene *scene_ptr, Font *menu_font, Font *message_font)
{
	self.dsapp_ptr = dsapp_ptr;
	self.scene_ptr = scene_ptr;
	self.message_font = message_font;
	self.menu_font = menu_font;
	self.system_menu_form = NULL;
	self.optimize_cover_popup = NULL;
	self.optimize_covers_label = NULL;
	self.first_scan_cover = false;
}

void DestroySystemMenu()
{
	HideSystemMenu();
	HideOptimizeCoverPopup();
	HideCoverScan();

	self.dsapp_ptr = NULL;
	self.scene_ptr = NULL;
	self.message_font = NULL;
	self.menu_font = NULL;
}

void SystemMenuRemoveAll()
{
}

void SystemMenuInputEvent(int type, int key)
{
	TSU_FormInputEvent(self.system_menu_form, type, key);
}

int StateSystemMenu()
{
	return (self.system_menu_form != NULL ? 1 : 0);
}

void ShowSystemMenu()
{
	HideSystemMenu();

	if (self.system_menu_form == NULL)
	{
		char font_path[NAME_MAX];
		memset(font_path, 0, sizeof(font_path));
		snprintf(font_path, sizeof(font_path), "%s/%s", GetDefaultDir(menu_data.current_dev), "apps/games_menu/fonts/default.txf");

		Font *form_font = TSU_FontCreate(font_path, PVR_LIST_TR_POLY);

		uint width = 300;
		uint height = 390;
		self.system_menu_form = TSU_FormCreate(640 / 2 - 150, 480 / 2 + (height - 10) / 2, width, height, true, 3, true, false, form_font, &OnSystemViewIndexChangedEvent);
		TSU_DrawableSubAdd((Drawable *)self.scene_ptr, (Drawable *)self.system_menu_form);
		TSU_FormSelectedEvent(self.system_menu_form, &SystemMenuSelectedEvent);		
	}
}

void OnSystemViewIndexChangedEvent(Drawable *drawable, int view_index)
{
	switch (view_index)
	{
		case SYSTEM_MENU_VIEW:
			CreateSystemMenuView((Form *)drawable);
			break;
	}
}

void CreateSystemMenuView(Form *form_ptr)
{
	Font* form_font = TSU_FormGetTitleFont(form_ptr);
	TSU_FormtSetAttributes(form_ptr, 1, 11, 296, 32);		
	TSU_FormSetRowSize(form_ptr, 4, 30);
	TSU_FormSetRowSize(form_ptr, 6, 30);
	TSU_FormSetRowSize(form_ptr, 8, 30);
	TSU_FormSetRowSize(form_ptr, 10, 30);
	TSU_FormSetRowSize(form_ptr, 11, 26);
	TSU_FormSetTitle(form_ptr, "SYSTEM MENU");

	int font_size = 15;

	// THE ALIGN SHOULD BE IN FORM CLASS
	{
		// SCAN MISSING COVERS
		Label *missing_covers_label = TSU_LabelCreate(form_font, "Scan missing covers", font_size, false, false);
		TSU_FormAddBodyLabel(form_ptr, missing_covers_label, 1, 1);
		TSU_DrawableEventSetClick((Drawable *)missing_covers_label, &ScanMissingCoversClick);

		Vector option_vector = TSU_DrawableGetPosition((Drawable *)missing_covers_label);
		option_vector.x += 42;
		TSU_DrawableSetTranslate((Drawable *)missing_covers_label, &option_vector);
		TSU_FormSetCursor(form_ptr, (Drawable *)missing_covers_label);

		missing_covers_label = NULL;
	}

	{
		// OPTIMIZE COVERS
		Label *optimize_covers_label = TSU_LabelCreate(form_font, "Optimize covers", font_size, false, false);
		TSU_FormAddBodyLabel(form_ptr, optimize_covers_label, 1, 2);
		TSU_DrawableEventSetClick((Drawable *)optimize_covers_label, &OptimizeCoversClick);

		Vector option_vector = TSU_DrawableGetPosition((Drawable *)optimize_covers_label);
		option_vector.x += 64;
		TSU_DrawableSetTranslate((Drawable *)optimize_covers_label, &option_vector);

		optimize_covers_label = NULL;
	}		

	{
		// DEFAULT SAVE PRESET
		Label *save_preset_label = TSU_LabelCreate(form_font, "Default save preset:", font_size, false, false);
		TSU_DrawableSetReadOnly((Drawable*)save_preset_label, true);
		TSU_FormAddBodyLabel(form_ptr, save_preset_label, 1, 3);

		self.save_preset_option = TSU_CheckBoxCreate(form_font, font_size, 50, 20);
		TSU_FormAddBodyCheckBox(form_ptr, self.save_preset_option, 1, 4);

		Vector option_vector = TSU_DrawableGetPosition((Drawable *)self.save_preset_option);
		option_vector.y -= 5;
		TSU_DrawableSetTranslate((Drawable *)self.save_preset_option, &option_vector);

		TSU_DrawableEventSetClick((Drawable *)self.save_preset_option, &DefaultSavePresetOptionClick);

		if (menu_data.app_config.save_preset)
		{
			TSU_CheckBoxSetOn(self.save_preset_option);
		}

		save_preset_label = NULL;
	}

	{
		// COVER BACKGROUND
		Label *cover_background_label = TSU_LabelCreate(form_font, "Cover background:", font_size, false, false);
		TSU_DrawableSetReadOnly((Drawable*)cover_background_label, true);
		TSU_FormAddBodyLabel(form_ptr, cover_background_label, 1, 5);

		self.cover_background_option = TSU_CheckBoxCreate(form_font, font_size, 50, 20);
		TSU_FormAddBodyCheckBox(form_ptr, self.cover_background_option, 1, 6);

		Vector option_vector = TSU_DrawableGetPosition((Drawable *)self.cover_background_option);
		option_vector.y -= 5;
		TSU_DrawableSetTranslate((Drawable *)self.cover_background_option, &option_vector);

		TSU_DrawableEventSetClick((Drawable *)self.cover_background_option, &CoverBackgroundOptionClick);

		if (menu_data.app_config.cover_background)
		{
			TSU_CheckBoxSetOn(self.cover_background_option);
		}

		cover_background_label = NULL;
	}

	{
		// CHANGE PAGE WITH PAD
		Label *change_page_with_page_label = TSU_LabelCreate(form_font, "Change page with pad:", font_size, false, false);
		TSU_DrawableSetReadOnly((Drawable*)change_page_with_page_label, true);
		TSU_FormAddBodyLabel(form_ptr, change_page_with_page_label, 1, 7);

		self.change_page_with_page_option = TSU_CheckBoxCreate(form_font, font_size, 50, 20);
		TSU_FormAddBodyCheckBox(form_ptr, self.change_page_with_page_option, 1, 8);

		Vector option_vector = TSU_DrawableGetPosition((Drawable *)self.change_page_with_page_option);
		option_vector.y -= 5;
		TSU_DrawableSetTranslate((Drawable *)self.change_page_with_page_option, &option_vector);

		TSU_DrawableEventSetClick((Drawable *)self.change_page_with_page_option, &ChangePageWithPadOptionClick);

		if (menu_data.app_config.change_page_with_pad)
		{
			TSU_CheckBoxSetOn(self.change_page_with_page_option);
		}

		change_page_with_page_label = NULL;
	}

	{
		// START IN LAST GAME
		Label *start_in_last_game_label = TSU_LabelCreate(form_font, "Start in last game:", font_size, false, false);
		TSU_DrawableSetReadOnly((Drawable*)start_in_last_game_label, true);
		TSU_FormAddBodyLabel(form_ptr, start_in_last_game_label, 1, 9);

		self.start_in_last_game_option = TSU_CheckBoxCreate(form_font, font_size, 50, 20);
		TSU_FormAddBodyCheckBox(form_ptr, self.start_in_last_game_option, 1, 10);

		Vector option_vector = TSU_DrawableGetPosition((Drawable *)self.start_in_last_game_option);
		option_vector.y -= 5;
		TSU_DrawableSetTranslate((Drawable *)self.start_in_last_game_option, &option_vector);

		TSU_DrawableEventSetClick((Drawable *)self.start_in_last_game_option, &StartInLastGameOptionClick);

		if (menu_data.app_config.start_in_last_game)
		{
			TSU_CheckBoxSetOn(self.start_in_last_game_option);
		}

		start_in_last_game_label = NULL;
	}

	{
		// EXIT TO MAIN MENU
		Label *exit_covers_label = TSU_LabelCreate(form_font, "Return", font_size, false, false);
		TSU_FormAddBodyLabel(form_ptr, exit_covers_label, 1, 11);
		TSU_DrawableEventSetClick((Drawable *)exit_covers_label, &ExitSystemMenuClick);

		Vector option_vector = TSU_DrawableGetPosition((Drawable *)exit_covers_label);
		option_vector.x += 110;
		TSU_DrawableSetTranslate((Drawable *)exit_covers_label, &option_vector);

		exit_covers_label = NULL;
	}

	form_font = NULL;
}

void HideSystemMenu()
{
	if (self.system_menu_form != NULL)
	{
		TSU_FormDestroy(&self.system_menu_form);
	}
}

void SystemMenuSelectedEvent(Drawable *drawable, uint bottom_index, uint column, uint row)
{
}

void ScanMissingCoversClick(Drawable *drawable)
{
	HideSystemMenu();

	if (menu_data.current_dev == APP_DEVICE_SD || menu_data.current_dev == APP_DEVICE_IDE)
	{
		if (menu_data.load_pvr_cover_thread == NULL && menu_data.optimize_game_cover_thread == NULL)
		{
			menu_data.stop_load_pvr_cover = false;

			if (!self.first_scan_cover)
			{
				menu_data.rescan_covers = true;
			}

			self.first_scan_cover = true;
			menu_data.load_pvr_cover_thread = thd_create(0, LoadPVRCoverThread, NULL);
			ShowCoverScan();			
		}
	}
	else
	{
		menu_data.state_app = SA_GAMES_MENU;
	}
}

void OptimizeCoversClick(Drawable *drawable)
{
	HideSystemMenu();

	if (menu_data.current_dev == APP_DEVICE_SD || menu_data.current_dev == APP_DEVICE_IDE)
	{
		if (menu_data.load_pvr_cover_thread == NULL && menu_data.optimize_game_cover_thread == NULL)
		{
			menu_data.stop_optimize_game_cover = false;
			menu_data.optimize_game_cover_thread = thd_create(0, OptimizeCoverThread, NULL);
			ShowOptimizeCoverPopup();
			menu_data.state_app = SA_OPTIMIZE_COVER;
		}
	}
	else
	{
		menu_data.state_app = SA_GAMES_MENU;
	}
}

void DefaultSavePresetOptionClick(Drawable *drawable)
{
	TSU_CheckBoxInputEvent(self.save_preset_option, 0, KeySelect);
	menu_data.save_preset = (TSU_CheckBoxGetValue(self.save_preset_option) ? 1 : 0);
}

void CoverBackgroundOptionClick(Drawable *drawable)
{
	TSU_CheckBoxInputEvent(self.cover_background_option, 0, KeySelect);
	menu_data.cover_background = (TSU_CheckBoxGetValue(self.cover_background_option) ? 1 : 0);
}

void ChangePageWithPadOptionClick(Drawable *drawable)
{
	TSU_CheckBoxInputEvent(self.change_page_with_page_option, 0, KeySelect);
	menu_data.change_page_with_pad = (TSU_CheckBoxGetValue(self.change_page_with_page_option) ? 1 : 0);
}

void StartInLastGameOptionClick(Drawable *drawable)
{
	TSU_CheckBoxInputEvent(self.start_in_last_game_option, 0, KeySelect);
	menu_data.start_in_last_game = (TSU_CheckBoxGetValue(self.start_in_last_game_option) ? 1 : 0);
}

void ExitSystemMenuClick(Drawable *drawable)
{
	HideSystemMenu();

	int menu_type = menu_data.menu_type;
	menu_data.menu_type = menu_data.app_config.initial_view;
	SaveMenuConfig();
	menu_data.menu_type = menu_type;

	menu_data.state_app = SA_GAMES_MENU;
}

void ShowOptimizeCoverPopup()
{
	StopCDDA();
	HideOptimizeCoverPopup();

	if (self.optimize_cover_popup == NULL)
	{
		char font_path[NAME_MAX];
		memset(font_path, 0, sizeof(font_path));
		snprintf(font_path, sizeof(font_path), "%s/%s", GetDefaultDir(menu_data.current_dev), "apps/games_menu/fonts/default.txf");

		Font *form_font = TSU_FontCreate(font_path, PVR_LIST_TR_POLY);

		self.optimize_cover_popup = TSU_FormCreate(640 / 2 - 500 / 2, 480 / 2 + 230 / 2 - 30, 500, 230, true, 3, true, false, form_font, NULL);
		TSU_FormtSetAttributes(self.optimize_cover_popup, 1, 2, 500, 32);

		self.optimize_covers_label = TSU_LabelCreate(form_font, "", 16, false, false);
		Label *exit_covers_label = TSU_LabelCreate(form_font, "                Press any key to exit", 16, false, false);

		TSU_DrawableSetReadOnly((Drawable *)self.optimize_covers_label, true);
		TSU_DrawableSetReadOnly((Drawable *)exit_covers_label, true);

		TSU_FormSetRowSize(self.optimize_cover_popup, 1, 50);
		TSU_FormSetRowSize(self.optimize_cover_popup, 2, 100);
		TSU_FormSetTitle(self.optimize_cover_popup, "OPTIMIZING COVERS");

		TSU_FormAddBodyLabel(self.optimize_cover_popup, self.optimize_covers_label, 1, 1);
		TSU_FormAddBodyLabel(self.optimize_cover_popup, exit_covers_label, 1, 2);

		TSU_DrawableSubAdd((Drawable *)self.scene_ptr, (Drawable *)self.optimize_cover_popup);

		form_font = NULL;
		exit_covers_label = NULL;
	}
}

void HideOptimizeCoverPopup()
{
	if (self.optimize_cover_popup != NULL)
	{
		TSU_FormDestroy(&self.optimize_cover_popup);
		self.optimize_covers_label = NULL;
	}
}

void SetMessageOptimizer(const char *fmt, const char *message)
{
	char message_optimizer[NAME_MAX];
	memset(message_optimizer, 0, sizeof(message_optimizer));
	snprintf(message_optimizer, sizeof(message_optimizer), fmt, message);
	message_optimizer[44] = '\0';
		
	if (self.optimize_covers_label != NULL)
	{
		TSU_LabelSetText(self.optimize_covers_label, message_optimizer);
	}
}

int StopOptimizeCovers()
{
	if (menu_data.optimize_game_cover_thread != NULL)
	{
		bool skip_return = self.optimize_cover_popup == NULL;
		menu_data.stop_optimize_game_cover = true;
		thd_join(menu_data.optimize_game_cover_thread, NULL);
		menu_data.optimize_game_cover_thread = NULL;
		HideOptimizeCoverPopup();

		if (!skip_return)
			return 1;
	}

	return (menu_data.optimize_game_cover_thread == NULL ? 0 : -1);
}

void ShowCoverScan()
{
	menu_data.state_app = SA_SCAN_COVER;

	StopCDDA();
	HideCoverScan();

	Vector vector_init_title = {640 / 2, 400 / 2 - 50, ML_CURSOR + 6, 1};
	Color modal_color = {1, 0.0f, 0.0f, 0.0f};
	Color modal_border_color = {1, 1.0f, 1.0f, 1.0f};

	self.modal_cover_scan = TSU_RectangleCreateWithBorder(PVR_LIST_OP_POLY, 40, 350, 560, 230, &modal_color, ML_CURSOR + 5, 3, &modal_border_color, 0);
	TSU_AppSubAddRectangle(self.dsapp_ptr, self.modal_cover_scan);

	static Color color = {1, 1.0f, 1.0f, 1.0f};
	self.title_cover_scan = TSU_LabelCreate(self.menu_font, "SCANNING MISSING COVER", 26, true, true);
	TSU_LabelSetTint(self.title_cover_scan, &color);
	TSU_AppSubAddLabel(self.dsapp_ptr, self.title_cover_scan);
	TSU_LabelSetTranslate(self.title_cover_scan, &vector_init_title);

	vector_init_title.x = 640 / 2;
	vector_init_title.y = 400 / 2 + 100;
	self.note_cover_scan = TSU_LabelCreate(self.menu_font, "\n\nPress any key to exit", 14, true, true);
	TSU_LabelSetTint(self.note_cover_scan, &color);
	TSU_AppSubAddLabel(self.dsapp_ptr, self.note_cover_scan);
	TSU_LabelSetTranslate(self.note_cover_scan, &vector_init_title);
}

void HideCoverScan()
{
	if (self.modal_cover_scan != NULL)
	{
		TSU_AppSubRemoveRectangle(self.dsapp_ptr, self.modal_cover_scan);
		thd_pass();
		TSU_RectangleDestroy(&self.modal_cover_scan);
	}

	if (self.title_cover_scan != NULL)
	{
		TSU_AppSubRemoveLabel(self.dsapp_ptr, self.title_cover_scan);
		thd_pass();
		TSU_LabelDestroy(&self.title_cover_scan);
	}

	if (self.note_cover_scan != NULL)
	{
		TSU_AppSubRemoveLabel(self.dsapp_ptr, self.note_cover_scan);
		thd_pass();
		TSU_LabelDestroy(&self.note_cover_scan);
	}

	if (self.message_cover_scan != NULL)
	{
		TSU_AppSubRemoveLabel(self.dsapp_ptr, self.message_cover_scan);
		thd_pass();
		TSU_LabelDestroy(&self.message_cover_scan);
	}
}

void SetMessageScan(const char *fmt, const char *message)
{
	char message_scan[NAME_MAX];
	memset(message_scan, 0, sizeof(message_scan));
	snprintf(message_scan, sizeof(message_scan), fmt, message);
	message_scan[49] = '\0';
		
	if (self.message_cover_scan != NULL)
	{
		TSU_LabelSetText(self.message_cover_scan, message_scan);
	}
	else
	{
		Color color = {1, 1.0f, 1.0f, 1.0f};
		Vector vector_init_title = {640/2, 400/2, ML_CURSOR + 6, 1};
		vector_init_title.x = 50;
		vector_init_title.y += 40;
		
		self.message_cover_scan = TSU_LabelCreate(self.message_font, message_scan, 16, false, true);
		TSU_LabelSetTint(self.message_cover_scan, &color);
		TSU_AppSubAddLabel(self.dsapp_ptr, self.message_cover_scan);
		TSU_LabelSetTranslate(self.message_cover_scan, &vector_init_title);
	}
}

int StopScanCovers()
{
	if (menu_data.load_pvr_cover_thread != NULL)
	{	
		bool skip_return = self.modal_cover_scan == NULL;
		menu_data.stop_load_pvr_cover = true;		
		thd_join(menu_data.load_pvr_cover_thread, NULL);
		menu_data.load_pvr_cover_thread = NULL;

		if (!skip_return)
			return 1;
	}

	return (menu_data.load_pvr_cover_thread == NULL ? 0 : -1);
}