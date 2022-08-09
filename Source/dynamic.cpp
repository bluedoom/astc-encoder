
#include <stdio.h>
#include "astcenc.h"
#include "astcenccli_internal.h"
#undef ASTCENC_PUBLIC

#if defined(_MSC_VER)
#define PUBLIC_API extern "C" __declspec(dllexport)
#else
#define PUBLIC_API extern "C" __attribute__ ((visibility ("default")))
#endif

struct compress_context_wrap
{
    astcenc_context* context;
    int32_t block;
    int32_t thread_count;
};

struct compression_workload
{
    astcenc_context* context;
    astcenc_image* image;
    astcenc_swizzle swizzle;
    uint8_t* data_out;
    size_t data_len;
    astcenc_error error;
};

/**
 * @brief Runner callback function for a compression worker thread.
 *
 * @param thread_count   The number of threads in the worker pool.
 * @param thread_id      The index of this thread in the worker pool.
 * @param payload        The parameters for this thread.
 */
static void compression_workload_runner(
    int thread_count,
    int thread_id,
    void* payload
) {
    (void)thread_count;

    compression_workload* work = static_cast<compression_workload*>(payload);
    astcenc_error error = astcenc_compress_image(
        work->context, work->image, &work->swizzle,
        work->data_out, work->data_len, thread_id);

    // This is a racy update, so which error gets returned is a random, but it
    // will reliably report an error if an error occurs
    if (error != ASTCENC_SUCCESS)
    {
        work->error = error;
    }
}

PUBLIC_API compress_context_wrap* CreateContext(int block,
    astcenc_profile profile, float quality, unsigned int flags,
    float w_r, float w_g, float w_b, float w_a,
    int thread_count)
{
    astcenc_config config;
    config.block_x = block;
    config.block_y = block;
    config.profile = profile;
    config.cw_a_weight = w_a;
    config.cw_r_weight = w_r;
    config.cw_g_weight = w_g;
    config.cw_b_weight = w_b;

    config.flags = flags;

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
    return new compress_context_wrap{ context,block,thread_count };
}

PUBLIC_API size_t GetOutputLen(int w, int h, int block)
{
    // Compute the number of ASTC blocks in each dimension
    unsigned int block_count_x = (w + block - 1) / block;
    unsigned int block_count_y = (h + block - 1) / block;
    size_t comp_len = block_count_x * block_count_y * 16;

    return comp_len + 16;
}

PUBLIC_API void FreeContext(compress_context_wrap* wrap)
{
    astcenc_context_free(wrap->context);

    delete wrap;
}

static const uint32_t ASTC_MAGIC_ID = 0x5CA1AB13;


PUBLIC_API int Compress(uint8_t* rgbaImg, uint8_t* output, astcenc_swizzle swizzle, int w, int h, compress_context_wrap* wrap)
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
    auto outputSize = GetOutputLen(w, h, wrap->block);

    compression_workload work;
    work.context = wrap->context;
    work.image = &image;
    work.swizzle = swizzle;
    work.data_out = output + 16;
    work.data_len = outputSize - 16;
    work.error = ASTCENC_SUCCESS;

    if (wrap->thread_count > 1)
    {
        launch_threads(wrap->thread_count, compression_workload_runner, &work);
    }
    else
    {
        work.error = astcenc_compress_image(
            work.context, work.image, &work.swizzle,
            work.data_out, work.data_len, 0);
    }

    astcenc_compress_reset(wrap->context);
    if (work.error != ASTCENC_SUCCESS)
    {
        return work.error;
    }
    // header;
    output[0] = ASTC_MAGIC_ID & 0xFF;
    output[1] = (ASTC_MAGIC_ID >> 8) & 0xFF;
    output[2] = (ASTC_MAGIC_ID >> 16) & 0xFF;
    output[3] = (ASTC_MAGIC_ID >> 24) & 0xFF;

    output[4] = static_cast<uint8_t>(wrap->block);
    output[5] = static_cast<uint8_t>(wrap->block);
    output[6] = static_cast<uint8_t>(1);

    output[7 + 0] = image.dim_x & 0xFF;
    output[7 + 1] = (image.dim_x >> 8) & 0xFF;
    output[7 + 2] = (image.dim_x >> 16) & 0xFF;

    output[10 + 0] = image.dim_y & 0xFF;
    output[10 + 1] = (image.dim_y >> 8) & 0xFF;
    output[10 + 2] = (image.dim_y >> 16) & 0xFF;

    output[13 + 0] = image.dim_z & 0xFF;
    output[13 + 1] = (image.dim_z >> 8) & 0xFF;
    output[13 + 2] = (image.dim_z >> 16) & 0xFF;


    return 0;
}

