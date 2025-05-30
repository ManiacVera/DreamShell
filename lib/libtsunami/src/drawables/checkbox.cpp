/*
   Tsunami for KallistiOS ##version##

   checkbox.cpp

   Copyright (C) 2024-2025 Maniac Vera

*/

#include "drawables/checkbox.h"
#include "genmenu.h"
#include "tsudefinition.h"
#include <algorithm>
#include <cstring>

CheckBox::CheckBox(Font *display_font, uint text_size, float width, float height, const Color &body_color)
	: CheckBox(display_font, text_size, width, height, body_color, nullptr, nullptr) {
}

CheckBox::CheckBox(Font *display_font, uint text_size, float width, float height, const Color &body_color, const char *on_text, const char *off_text) {
	setObjectType(ObjectTypeEnum::CHECKBOX_TYPE);
	m_z_index = 0;
	m_border_width = 2;
	m_padding_width = 32;
	m_padding_height = 6;

	m_display_font = display_font;
	m_width = width + m_padding_width + m_border_width;
	m_height = height + m_padding_height + m_border_width;
	m_option_selected = 0;
	m_display_label = nullptr;
	m_on_text = "ON";
	m_off_text = "OFF";
	m_body_color = body_color.a ? body_color : Color(DEFAULT_BODY_COLOR);

	if (on_text != nullptr) {
		m_on_text = on_text;
	}

	if (off_text != nullptr) {
		m_off_text = off_text;
	}

	if (text_size == 0) {
		text_size = 20;
	}

	if (display_font) {
		Color border_color = {1, 1.0f, 1.0f, 1.0f};		
		m_z_index = 0;
		float radius = 0;

		Vector position = this->getTranslate();
		m_control_rectangle = new Rectangle (PVR_LIST_OP_POLY, position.x - width/2 - m_padding_width/2 + 1, position.y + height/2 + m_padding_height, width + m_padding_width, height + m_padding_height, m_body_color, m_z_index, m_border_width, border_color, radius);

		Color background_color = { 1, 1.0f, 1.0f, 1.0f };
		m_rectangle = new Rectangle (PVR_LIST_OP_POLY, (position.x - width/2) - 10, position.y + height/2 + 2, 10, height -2, background_color, m_z_index, m_border_width, border_color, radius);

		this->subAdd(m_control_rectangle);
		this->subAdd(m_rectangle);

		m_display_label = new Label(display_font, m_off_text, text_size, true, false);
		this->subAdd(m_display_label);

		Vector rectangle_vector = m_rectangle->getTranslate();
		Vector control_rectangle_vector = m_control_rectangle->getTranslate();
		Vector display_label_control = m_display_label->getTranslate();

		rectangle_vector.x += (12 + width/2 + m_border_width*2);
		rectangle_vector.y -= m_border_width*2;
		rectangle_vector.z = m_z_index + 2;
		m_rectangle->setTranslate(rectangle_vector);

		control_rectangle_vector.x += (12 + width/2 + m_border_width*2);
		control_rectangle_vector.y -= m_border_width*2;
		control_rectangle_vector.z = m_z_index + 1;
		m_control_rectangle->setTranslate(control_rectangle_vector);

		display_label_control.x += (12 + width/2 + m_border_width*2);
		display_label_control.y -= 3;
		display_label_control.z = m_z_index + 2;
		m_display_label->setTranslate(display_label_control);

		setOff();
	}
}

CheckBox::~CheckBox() {
	
	if (m_display_label != nullptr) {
		m_display_label->setFinished();
	}

	if (m_control_rectangle != nullptr) {
		m_control_rectangle->setFinished();
	}

	if (m_rectangle != nullptr) {
		m_rectangle->setFinished();
	}

	this->subRemoveFinished();

	if (m_display_label != nullptr) {
		delete m_display_label;
		m_display_label = nullptr;
	}

	if (m_control_rectangle != nullptr) {
		delete m_control_rectangle;
		m_control_rectangle = nullptr;
	}

	if (m_rectangle != nullptr) {
		delete m_rectangle;
		m_rectangle = nullptr;
	}

	m_option_selected = 0;
	m_display_font = nullptr;
}

void CheckBox::inputEvent(int event_type, int key) {
	if (event_type != GenericMenu::Event::EvtKeypress)
		return;

	switch (key)
	{
		case GenericMenu::Event::KeySelect:
		{
			if (getValue() == 1) {
				setOff();
			}
			else {
				setOn();
			}
		}
		break;
	}
}

void CheckBox::setSize(float width, float height){

}

void CheckBox::setPosition(float x, float y) {

}

const std::string CheckBox::getText() {
	return m_display_label->getText();
}

int CheckBox::getValue() {
	return m_option_selected;
}

void CheckBox::setOn() {
	m_option_selected = 1;
	m_display_label->setText(m_on_text);

	Vector position = m_control_rectangle->getTranslate();

	float w, h;
	m_control_rectangle->getSize(&w, &h);

	float rw, rh;
	m_rectangle->getSize(&rw, &rh);

	position.x = (position.x + w) - (rw + 5);
	position.y -= 4;
	position.z = m_z_index + 2;
	m_rectangle->setTranslate(position);

	Color color = {1.0f, .49f, 1.0f, 0.0f};
	m_rectangle->setTint(color);
}

void CheckBox::setOff() {
	m_option_selected = 0;
	m_display_label->setText(m_off_text);

	Vector position = m_control_rectangle->getTranslate();
	float rw, rh;
	m_rectangle->getSize(&rw, &rh);

	position.x = position.x + rw/2;
	position.y -= 4;
	position.z = m_z_index + 2;
	m_rectangle->setTranslate(position);

	Color color = {1.0f, 0.50f, 0.50f, 0.50f};
	m_rectangle->setTint(color);
}

extern "C"
{
	CheckBox* TSU_CheckBoxCreate(Font *display_font, uint text_size, float width, float height, Color *body_color)
	{
		return new CheckBox(display_font, text_size, width, height, *body_color);
	}

	CheckBox* TSU_CheckBoxCreateWithCustomText(Font *display_font, uint text_size, float width, float height, Color *body_color, const char *on_text, const char *off_text)
	{
		return new CheckBox(display_font, text_size, width, height, *body_color, on_text, off_text);
	}

	void TSU_CheckBoxDestroy(CheckBox **checkbox_ptr)
	{
		if (*checkbox_ptr != NULL)
		{
			delete *checkbox_ptr;
			*checkbox_ptr = NULL;
		}
	}

	void TSU_CheckBoxInputEvent(CheckBox *checkbox_ptr, int event_type, int key)
	{
		if (checkbox_ptr != NULL)
		{
			checkbox_ptr->inputEvent(event_type, key);
		}
	}

	const char* TSU_CheckBoxGetText(CheckBox *checkbox_ptr)
	{
		if (checkbox_ptr != NULL) {
			static char text[51] = {0};
			std::string tmp = checkbox_ptr->getText();
			strncpy(text, tmp.c_str(), sizeof(text) - 1);
			text[sizeof(text) - 1] = '\0';

			return text;
		}
		else {
			return NULL;
		}
	}
	
	int TSU_CheckBoxGetValue(CheckBox *checkbox_ptr)
	{
		if (checkbox_ptr != NULL) {
			return checkbox_ptr->getValue();
		}
		else {
			return 0;
		}
	}

	void TSU_CheckBoxSetOn(CheckBox *checkbox_ptr)
	{
		if (checkbox_ptr != NULL) {
			return checkbox_ptr->setOn();
		}
	}

	void TSU_CheckBoxSetOff(CheckBox *checkbox_ptr)
	{
		if (checkbox_ptr != NULL) {
			return checkbox_ptr->setOff();
		}
	}
}