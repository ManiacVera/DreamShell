/* DreamShell ##version##
   utils.c
   Copyright (C) 2024 Maniac Vera
*/

#include "ds.h"
#include "img/SegaPVRImage.h"

#if defined(__DREAMCAST__)
#include <malloc.h>
#endif

bool pvr_is_alpha(const char *filename)
{
	file_t pFile = fs_open(filename, O_RDONLY);
	if (pFile == FILEHND_INVALID)
	{
		return false;
	}

	size_t fsize = 512;
	uint8 *data = (uint8 *)memalign(32, fsize);	
	if (data == NULL)
	{
		fs_close(pFile);
		return false;
	}

	if (fs_read(pFile, data, fsize) == -1)
	{
		fs_close(pFile);
		return false;
	}

	fs_close(pFile);
	pFile = -1;

	struct PVRTHeader pvrtHeader;
	unsigned int offset = ReadPVRHeader(data, &pvrtHeader);
	if (offset == 0)
	{
		free(data);
		dbglog(DBG_INFO, "pvr_is_alpha: wrong header");		
		return false;
	}
	free(data);

	enum TextureFormatMasks srcFormat = (enum TextureFormatMasks)(pvrtHeader.textureAttributes & 0xFF);

	if (srcFormat != TFM_ARGB1555 && srcFormat != TFM_RGB565 && srcFormat != TFM_ARGB4444)
	{	
		dbglog(DBG_INFO, "pvr_is_alpha: unsupported format");
		return false;
	}
	
	return srcFormat != TFM_RGB565;	
}
