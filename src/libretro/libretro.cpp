
#include <cstring>
#include <cstdlib>
#include <memory>
#include <iostream>

#include "libretro.h"
#include "../sysfont.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"

// Callbacks
static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static retro_environment_t environ_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;

// Font data
static uint16_t *sysfont_data;
static int sysfont_width;
static int sysfont_height;

unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}

void retro_cheat_reset(void) 
{
    // Empty
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
    // Empty
}

bool retro_load_game(const struct retro_game_info *info)
{
    return true;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
    return false;
}

void retro_unload_game(void)
{
    // Empty
}

unsigned retro_get_region(void)
{
    return RETRO_REGION_NTSC;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
    // Empty
}


void *retro_get_memory_data(unsigned id)
{
    return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
    return 0;
}

size_t retro_serialize_size(void)
{
    return 0;
}

bool retro_serialize(void *data, size_t size)
{
    return false;
}

bool retro_unserialize(const void *data, size_t size)
{
    return false;
}

void retro_deinit(void)
{
    // Empty
}

// libretro global setters
void retro_set_environment(retro_environment_t cb)
{
    environ_cb = cb;

    // Allow running core with no game selected
    bool no_rom = true;
    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_rom);

    static const struct retro_controller_description controllers[] = {
        { "PuzzleScript Controller", RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0) },
    };

    static const struct retro_controller_info ports[] = {
        { controllers, 1 },
        { NULL, 0 },
    };

    cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
    audio_batch_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
    video_cb = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
    //audio_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
    input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
    input_state_cb = cb;
}

void retro_init(void)
{
    /* set up some logging */
    struct retro_log_callback log;
    unsigned level = 4;

    if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log)) {
        log_cb = log.log;
    } else {
        log_cb = NULL;
    }
    // the performance level is guide to frontend to give an idea of how intensive this core is to run
    environ_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);

    int channels{0};
    stbi_uc *data = stbi_load_from_memory(sysfont_png, sysfont_png_len, &sysfont_width, &sysfont_height, &channels, 0);
    sysfont_data = new uint16_t[256 * 9 * sysfont_height]();

    std::cout << "sysfont is " << sysfont_width << " x " << sysfont_height << std::endl;
    std::cout << "sysfont_data =" << (void*)sysfont_data << std::endl;
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            std::cout << (int)*(sysfont_data + c * 3 + r * 3 * sysfont_width) << " ";
        }
        std::cout << "\n";
    }
    std::cout << std::endl;
    for (int r = 0; r < 9; r++) {
        for (int ch = 0; ch < sysfont_width / 9; ch++) {
            for (int c = 0; c < 9; c++) {
                stbi_uc col = *(data + r * sysfont_width * 3 + ch * 9 * 3 + c * 3);
                if (col) {
                    sysfont_data[r * 256 * 9 + (ch + 33) * 9 + c] = 0xffff;
                }
            }
        }
    }
}

void retro_get_system_info(struct retro_system_info *info)
{
    memset(info, 0, sizeof(*info));
    info->library_name = "PuzzleScript";
    info->library_version = "0.2";
    info->need_fullpath = false;
    info->valid_extensions = "pz";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    int pixel_format = RETRO_PIXEL_FORMAT_RGB565;

    memset(info, 0, sizeof(*info));
    info->timing.fps            = 30.0f;
    info->timing.sample_rate    = 44100;
    info->geometry.base_width   = 640;
    info->geometry.base_height  = 480;
    info->geometry.max_width    = 640;
    info->geometry.max_height   = 480;
    // Don't request any specific aspect ratio

    // the performance level is guide to frontend to give an idea of how intensive this core is to run
    environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &pixel_format);
}

void retro_reset(void)
{
    // Do stuff
}

constexpr int audio_buffer_len{44100/30*2};

uint16_t framebuffer[640*480];

uint8_t offset{0};
int16_t audio_buffer[audio_buffer_len];

int x{320};
int y{240};
int cursor_x{0};
int cursor_y{0};

void draw_letter(int x, int y, int letter)
{
    uint16_t *fb = &framebuffer[x + 640*y];
    uint16_t *rom = &sysfont_data[letter * 9];
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            *(fb + r * 640 + c) = *(rom + r * 256 * 9 + c);
        }
    }
}

void draw_text(int col, int row, std::string txt)
{
    for (int i = 0; i < txt.size(); i++) {
        draw_letter(col * 9 + i * 9, row * 9, txt[i]);
    }
}

// Run a single frame
void retro_run(void)
{
    // Run one frame

    // Get input for frame
    input_poll_cb();
    if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP)) {
        y--;
    }
    if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN)) {
        y++;
    }
    if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT)) {
        x--;
    }
    if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT)) {
        x++;
    }

    // Render audio frame
    audio_batch_cb(audio_buffer, audio_buffer_len / 2);

    // Render video frame
    for (int row = 0; row < 480; row++) {
        for (int col = 0; col < 640; col++) {
            uint8_t r = col % 256;
            uint8_t g = row % 256;
            uint8_t b = (row + col + offset) % 256;
            framebuffer[row * 640 + col] = ((r >> 3) << (5 + 6)) | ((g >> 2) << 5) | (b >> 3);
        }
    }
    for (int h = 0; h < 20; h++) {
        for (int w = 0; w < 20; w++) {
            framebuffer[(h + y)* 640 + (w + x)] = 0x00000000;
        }
    }
    offset++;
    offset = offset & 0xff;

    // Draw text
    draw_text(20, 10, "Hello, Zoe!");

    video_cb(framebuffer, 640, 480, sizeof(uint16_t) * 640);
}
