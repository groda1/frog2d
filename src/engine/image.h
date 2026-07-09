#ifndef IMAGE_H
#define IMAGE_H

#include "core.h"

typedef struct _image_t image_t;
struct _image_t
{
    u32 width;
    u32 height;
    u8  *data; /* rgba8, width * height * 4 bytes */
};

bool Image_Load(const char *path, image_t *image_out);
void Image_Unload(image_t *image);

#endif
