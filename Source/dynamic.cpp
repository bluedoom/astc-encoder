
#include <stdio.h>
#include "astcenc.h"

#undef ASTCENC_PUBLIC

#if defined(_MSC_VER)
#define PUBLIC_API extern "C" __declspec(dllexport)
#else
#define PUBLIC_API extern "C" __attribute__ ((visibility ("default")))
#endif
static const astcenc_profile profile = ASTCENC_PRF_LDR;
static const float quality = ASTCENC_PRE_MEDIUM;
static const astcenc_swizzle swizzle{
  ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A
};

PUBLIC_API astcenc_context* CreateContext(int block, astcenc_profile profile, float quality, int thread_count)
{
    astcenc_config config;
    config.block_x = block;
    config.block_y = block;
    config.profile = profile;
    
    astcenc_error status;
    status = astcenc_config_init(profile, block, block, 1, quality, 0, &config);
    if (status != ASTCENC_SUCCESS)
    {
        printf("ERROR: Codec config init failed: %s\n", astcenc_get_error_string(status));
        return NULL;
    }


    astcenc_context* context;
    status = astcenc_context_alloc(&config, thread_count, &context);
    if (status != ASTCENC_SUCCESS)
    {
        printf("ERROR: Codec context alloc failed: %s\n", astcenc_get_error_string(status));
        return NULL;
    }
}

PUBLIC_API size_t GetOutputLen(int w, int h, int block)
{
    // Compute the number of ASTC blocks in each dimension
    unsigned int block_count_x = (w + block - 1) / block;
    unsigned int block_count_y = (h + block - 1) / block;
    size_t comp_len = block_count_x * block_count_y * 16;
    return comp_len;
}

PUBLIC_API void FreeContext(astcenc_context* context)
{
    astcenc_context_free(context);
}

PUBLIC_API int Compress(uint8_t* rgbaImg, uint8_t* output, int w, int h, int block, astcenc_context* ctx)
{
    if (!rgbaImg)
    {
        printf("Error: input is null");
        return 1;
    }
    if (!output)
    {
        printf("Error: output is null");
        return 1;
    }

    // ------------------------------------------------------------------------
    // Compress the image
    astcenc_image image;
    image.dim_x = w;
    image.dim_y = h;
    image.dim_z = 1;
    image.data_type = ASTCENC_TYPE_U8;
    uint8_t* slices = rgbaImg;
    image.data = reinterpret_cast<void**>(&slices);
    
    // Space needed for 16 bytes of output per compressed block
    auto comp_len = GetOutputLen(w, h, block);

    astcenc_error status = astcenc_compress_image(ctx, &image, &swizzle, output, comp_len, 0);
    if (status != ASTCENC_SUCCESS)
    {
        printf("ERROR: Codec compress failed: %s\n", astcenc_get_error_string(status));
        return 1;
    }

    return 0;
}
