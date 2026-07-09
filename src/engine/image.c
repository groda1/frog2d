#include "log.h"

#include "image.h"

// TODO route stb allocations through an arena
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "stb_image.h"
#pragma GCC diagnostic pop

bool Image_Load(const char *path, image_t *image_out)
{
    int width, height, channels;

    u8 *data = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
    if (data == NULL)
    {
        Log(ERROR, "failed to load image %s: %s", path, stbi_failure_reason());
        return false;
    }

    image_out->width = (u32)width;
    image_out->height = (u32)height;
    image_out->data = data;

    return true;
}

void Image_Unload(image_t *image)
{
    stbi_image_free(image->data);
    MemoryZeroItem(image);
}
