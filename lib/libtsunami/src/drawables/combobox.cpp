#include "drawables/combobox.h"

ComboBox::ComboBox() {
	setObjectType(ObjectTypeEnum::COMBOBOX_TYPE);
}

ComboBox::~ComboBox() {
}

extern "C"
{
	ComboBox* TSU_ComboBoxCreate()
	{
		return new ComboBox();
	}

	void TSU_ComboBoxDestroy(ComboBox **combobox_ptr)
	{
		if (*combobox_ptr != NULL)
		{
			delete *combobox_ptr;
			*combobox_ptr = NULL;
		}
	}
}