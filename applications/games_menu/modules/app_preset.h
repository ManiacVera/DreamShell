/* DreamShell ##version##

   app_preset.h
   Copyright (C) 2024-2025 Maniac Vera

*/

#ifndef __APP_PRESET_H
#define __APP_PRESET_H

#include <ds.h>
#include "tsunami/tsudefinition.h"
#include "tsunami/drawable.h"
#include "tsunami/dsapp.h"
#include "tsunami/drawables/scene.h"
#include "tsunami/drawables/form.h"
#include "tsunami/texture.h"

#include "app_utils.h"
#include "app_definition.h"

void CreatePresetMenu(DSApp *dsapp_ptr, Scene *scene_ptr, Font *menu_font, Font *message_font);
void DestroyPresetMenu();
void PresetMenuRemoveAll();
void OnViewIndexChangedEvent(Drawable *drawable, int view_index);
void CreateGeneralView(Form *form_ptr);
void CreateCDDAView(Form *form_ptr);
void CreatePatchView(Form *form_ptr);
void CreateExtensionsView(Form *form_ptr);
void CreateShortcutView(Form *form_ptr);

void PresetMenuInputEvent(int type, int key);
int StatePresetMenu();
void ShowPresetMenu(int game_index);
void HidePresetMenu();
void PresetMenuSelectedEvent(Drawable *drawable, uint bottom_index, uint column, uint row);

void ExitPresetMenuClick(Drawable *drawable);
void SavePresetOptionClick(Drawable *drawable);
void DMAOptionClick(Drawable *drawable);
void AsyncOptionClick(Drawable *drawable);
void ByPassOptionClick(Drawable *drawable);
void CDDAOptionClick(Drawable *drawable);
void IRQOptionClick(Drawable *drawable);
void DefaultPresetOptionClick(Drawable *drawable);
void OSOptionClick(Drawable *drawable);

void LoaderOptionClick(Drawable *drawable);
void BootOptionClick(Drawable *drawable);
void FastOptionClick(Drawable *drawable);
void LowLevelOptionClick(Drawable *drawable);
void MemoryOptionClick(Drawable *drawable);
void CustomMemoryOptionClick(Drawable *drawable);
void HeapOptionClick(Drawable *drawable);

void CDDASourceOptionClick(Drawable *drawable);
void CDDADestinationOptionClick(Drawable *drawable);
void CDDAPositionOptionClick(Drawable *drawable);
void CDDAChannelOptionClick(Drawable *drawable);

void PatchAddress1OptionClick(Drawable *drawable);
void PatchValue1OptionClick(Drawable *drawable);
void PatchAddress2OptionClick(Drawable *drawable);
void PatchValue2OptionClick(Drawable *drawable);

void AlterBootOptionClick(Drawable *drawable);
void SetModeScreenshot();
int GetModeScreenshot();
bool AllButtonsDisabled();
void CheckScreenshot();
void ScreenshotOptionClick(Drawable *drawable);
void ButtonStartOptionClick(Drawable *drawable);
void ButtonXOptionClick(Drawable *drawable);
void ButtonYOptionClick(Drawable *drawable);
void ButtonZOptionClick(Drawable *drawable);
void ButtonLTOptionClick(Drawable *drawable);
void ButtonAOptionClick(Drawable *drawable);
void ButtonBOptionClick(Drawable *drawable);
void ButtonCOptionClick(Drawable *drawable);
void ButtonRTOptionClick(Drawable *drawable);
void VMUOptionClick(Drawable *drawable);
void VMUSelectorOptionClick(Drawable *drawable);

void ShortcutSizeOptionClick(Drawable *drawable);
void ShortcutRotateOptionClick(Drawable *drawable);
void ShortcutNameOptionClick(Drawable *drawable);
void ShortcutDontShowNameOptionClick(Drawable *drawable);
void ShortcutCreateOptionClick(Drawable *drawable);

void SavePresetInputEvent(int type, int key);
void DMAInputEvent(int type, int key);
void AsyncInputEvent(int type, int key);
void ByPassInputEvent(int type, int key);
void CDDAInputEvent(int type, int key);
void IRQInputEvent(int type, int key);
void OSInputEvent(int type, int key);

void LoaderInputEvent(int type, int key);
void BootInputEvent(int type, int key);
void FastInputEvent(int type, int key);
void LowLevelInputEvent(int type, int key);
void MemoryInputEvent(int type, int key);
void CustomMemoryInputEvent(int type, int key);
void HeapInputEvent(int type, int key);

void CDDASourceInputEvent(int type, int key);
void CDDADestinationInputEvent(int type, int key);
void CDDAPositionInputEvent(int type, int key);
void CDDAChannelInputEvent(int type, int key);

void SetPatchByType(TextBox *patch_textbox, int control_type, char *patch_text_variable_ptr, uint32 *patch_variable_ptr);
void PatchAddress1InputEvent(int type, int key);
void PatchValue1InputEvent(int type, int key);
void PatchAddress2InputEvent(int type, int key);
void PatchValue2InputEvent(int type, int key);

void AlterBootInputEvent(int type, int key);
void ScreenshotInputEvent(int type, int key);
void VMUInputEvent(int type, int key);
void VMUSelectorInputEvent(int type, int key);
void ShortcutSizeInputEvent(int type, int key);
void ShortcutRotateInputEvent(int type, int key);
void ShortcutNameInputEvent(int type, int key);
void ShortcutDontShowNameInputEvent(int type, int key);
void ShortcutCreateInputEvent(int type, int key);
void ShortcutLoadTextureCover();
void ShortcutChangeCoverSize();

void CheckIfShorcutExits();

#endif // __APP_PRESET_H
