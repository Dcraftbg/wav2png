#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <stdbool.h>

#include "dr_wav.h"
#include "stb_image_write.h"

const char* shift_args(int *argc, const char*** argv) {
    if((*argc) <= 0) return NULL;
    (*argc)--;
    return *(*argv)++;
}
bool find_size(uint32_t size, uint32_t* width, uint32_t* height) {
    uint32_t square = sqrt(size);
    if(square == 0) return false;
    for(size_t w = square-1; w > 0; --w) {
        uint32_t h = size / w;
        if(size % w == 0) {
            *width = w;
            *height = h;
            return true;
        }
    }
    return false;
}
uint32_t floatToARGB(float value) {
    if (value < 0.0f) {
        fprintf(stderr, "Less %f\n", value);
        value = 0.0f;
    }
    if (value > 1.0f) {
        fprintf(stderr, "More %f\n", value);
        value = 1.0f;
    }
    uint8_t alpha = 255;
    uint8_t red, green, blue;
    if (value < 0.33f) {
        red = 255;
        green = (uint8_t)(value * 3 * 255);
        blue = 0;
    } else if (value < 0.66f) {
        red = (uint8_t)((1 - (value - 0.33f) * 3) * 255);
        green = 255;
        blue = 0;
    } else {
        red = 0;
        green = (uint8_t)((1 - (value - 0.66f) * 3) * 255);
        blue = 255;
    }
    uint32_t argb = (alpha << 24) | (red << 16) | (green << 8) | blue;
    return argb;
}

uint32_t heatToARGB(float value) {
    uint8_t alpha = 255;
    if (value < -1.0f) value = -1.0f;
    if (value > 1.0f) value = 1.0f;
    uint8_t red, green, blue;
    if (value < 0) {
        blue = (uint8_t)(255 * (1 + value));
        green = 0;
        red = 0;
    } else if (value > 0) {
        red = (uint8_t)(255 * value);
        green = 0;
        blue = 0;
    } else {
        red = 0;
        green = 255;
        blue = 0;
    }
    return (alpha << 24) | (red << 16) | (green << 8) | blue;
}
enum {
    MODE_RAW,
    MODE_GRAY,
    MODE_COLORS,
    MODE_HEATMAP,
    MODE_COUNT
};
typedef struct {
    void* data;
    size_t comps;
} Pixels;
Pixels pixels_raw(float* floats, size_t count) {
    (void)count;
    return (Pixels){.data=floats, .comps=4};
}
Pixels pixels_gray(float* floats, size_t count) {
    uint8_t* pixels = malloc(count*sizeof(*pixels));
    if(!pixels) return (Pixels){.data=pixels, .comps=sizeof(*pixels)};
    for(size_t i = 0; i < count; ++i) {
        pixels[i] = floats[i] * ((float)(0xFF));
    }
    return (Pixels){.data=pixels, .comps=sizeof(*pixels)};
}
Pixels pixels_colors(float* floats, size_t count) {
    uint32_t* pixels = malloc(count*sizeof(*pixels));
    if(!pixels) return (Pixels){.data=pixels, .comps=sizeof(*pixels)};
    for(size_t i = 0; i < count; ++i) {
        pixels[i] = floatToARGB((floats[i]+1.0) * 0.5);
    }
    return (Pixels){.data=pixels, .comps=sizeof(*pixels)};
}
Pixels pixels_heatmap(float* floats, size_t count) {
    uint32_t* pixels = malloc(count*sizeof(*pixels));
    if(!pixels) return (Pixels){.data=pixels, .comps=sizeof(*pixels)};
    for(size_t i = 0; i < count; ++i) {
        pixels[i] = heatToARGB(floats[i]);
    }
    return (Pixels){.data=pixels, .comps=sizeof(*pixels)};
}

typedef struct {
    Pixels (*pixels)(float* floats, size_t count);
    size_t name_count;
    const char** names;
} Mode;
#define mkmode(_pixels, ...) { .pixels=_pixels, .name_count=(sizeof((const char*[]){__VA_ARGS__})/sizeof(const char*)), .names = ((const char*[]){__VA_ARGS__}) }
Mode modes[] = {
    [MODE_RAW]    = mkmode(pixels_raw    , "raw"             ),
    [MODE_GRAY]   = mkmode(pixels_gray   , "gray"            ),
    [MODE_COLORS] = mkmode(pixels_colors , "colors"          ),
    [MODE_HEATMAP]= mkmode(pixels_heatmap, "heat", "heatmap" ),
};

void usage(FILE* sink, const char* exe) {
    fprintf(sink, "%s <ipath> -o <opath>\n", exe);
    fprintf(sink, "  -m <mode>\n");
    fprintf(sink, "     Available modes:\n");
    for(size_t i = 0; i < sizeof(modes)/sizeof(*modes); ++i) {
        fprintf(sink, "     <");
        for(size_t j = 0; j < modes[i].name_count; ++j) {
            if(j != 0) fprintf(sink, "|");
            fprintf(sink, "%s", modes[i].names[j]);
        }
        fprintf(sink, ">\n");
    }
}
int main(int argc, const char** argv) {
    const char* program = shift_args(&argc, &argv);
    assert(program && "Expected program path as first argument");
    const char* arg;
    const char* ipath = NULL;
    const char* opath = NULL;
    const char* mode_str = "heat";
    size_t mode=MODE_HEATMAP;
    while((arg = shift_args(&argc, &argv))) {
        if(strcmp(arg, "-o") == 0) {
            opath = shift_args(&argc, &argv);
            if(!opath) {
                fprintf(stderr, "ERROR: Expected output path after -o\n");
                usage(stderr, program);
                return 1;
            }
        }
        else if(strcmp(arg, "-m") == 0) {
            mode_str = shift_args(&argc, &argv);
            if(!mode_str) {
                fprintf(stderr, "ERROR: Expected mode after -m\n");
                usage(stderr, program);
                return 1;
            }
            for(size_t i = 0; i < sizeof(modes)/sizeof(*modes); ++i) {
                for(size_t j = 0; j < modes[i].name_count; ++j) {
                    if(strcmp(mode_str, modes[i].names[j])==0) {
                        mode = i;
                        goto found_mode;
                    }
                }
            }
            fprintf(stderr, "ERROR: Unsupported mode `%s`\n", mode_str);
            usage(stderr, program);
            return 1;
            found_mode:
        }
        else if (!ipath) ipath=arg;
        else {
            fprintf(stderr, "ERROR: Unexpected argument `%s`\n", arg);
            usage(stderr, program);
            return 1;
        }
    }
    if(!ipath) {
        fprintf(stderr, "ERROR: Missing input path\n");
        usage(stderr, program);
        return 1;
    }
    if(!opath) {
        fprintf(stderr, "ERROR: Missing output path\n");
        usage(stderr, program);
        return 1;
    }
    printf("Mode: %s\n", mode_str);
    printf("`%s` -> `%s`\n", ipath, opath);
    unsigned int channels;
    unsigned int sampleRate;
    drwav_uint64 totalPCMFrameCount;
    float* pSampleData = drwav_open_file_and_read_pcm_frames_f32(ipath, &channels, &sampleRate, &totalPCMFrameCount, NULL);
    if (pSampleData == NULL) {
        fprintf(stderr, "ERROR: Failed to open `%s`: %s\n", ipath, strerror(errno));
        return 1;
    }
    printf("Sucessfully loaded `%s`:\n", ipath);
    printf("  channels:     %u\n"  , channels);
    printf("  sample rate:  %u\n"  , sampleRate);
    printf("  total frames: %llu\n", totalPCMFrameCount);
    // printf("%u channels; %u sample rate; %llu total frames\n", channels, sampleRate, totalPCMFrameCount);
    size_t count = totalPCMFrameCount*channels;
    uint32_t w, h;
    if(!find_size(count, &w, &h)) {
        w = count;
        h = 1;
    }
    printf("Found optimal size %ux%u\n", w, h);
    Pixels pixels = modes[mode].pixels(pSampleData, count);
    if(!pixels.data) {
        fprintf(stderr, "ERROR: Ran out of memory\n");
        goto pixels_err;
    }
    printf("Generated pixel data\n");
    printf("Writing to `%s`\n", opath);
    if(!stbi_write_png(opath, w, h, pixels.comps, pixels.data, pixels.comps*w)) {
        fprintf(stderr, "ERROR: Failed to write to `%s`: %s", opath, strerror(errno));
        goto stbi_err;
    }
    fprintf(stderr, "Sucessfully wrote to %s\n", opath);
    if((void*)pixels.data != (void*)pSampleData) free(pixels.data);
    drwav_free(pSampleData, NULL);
    return 0;
stbi_err:
    if((void*)pixels.data != (void*)pSampleData) free(pixels.data);
pixels_err:
    drwav_free(pSampleData, NULL);
    return 1;
}
