// Single translation unit that instantiates the stb single-header libraries.
// Every assignment links this object so the headers stay declaration-only elsewhere.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
