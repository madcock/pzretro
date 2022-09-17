
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

#include "libretro.h"
#include "namespaced_bundled.h"
#include "stb_image.h"
#include "duktape.h"

#include "audio.h"
#include "graphics.h"
#include "js.h"
#include "sprite.h"

// Callbacks
retro_log_printf_t log_cb;
retro_video_refresh_t video_cb;
retro_input_poll_t input_poll_cb;
retro_input_state_t input_state_cb;
retro_environment_t environ_cb;
retro_audio_sample_t audio_cb;
retro_audio_sample_batch_t audio_batch_cb;

// Font data
uint16_t *sysfont_data;
int sysfont_width;
int sysfont_height;

// JavaScript context
std::unique_ptr<js::Context> js_context;

unsigned retro_api_version()
{
    return RETRO_API_VERSION;
}

void retro_cheat_reset() 
{
    // Empty
}

void retro_cheat_set(unsigned /*index*/, bool /*enabled*/, const char */*code*/)
{
    // Empty
}

bool retro_load_game(const struct retro_game_info */*info*/)
{
    return true;
}

bool retro_load_game_special(unsigned /*game_type*/, const struct retro_game_info */*info*/, size_t /*num_info*/)
{
    return false;
}

void retro_unload_game()
{
    // Empty
}

unsigned retro_get_region()
{
    return RETRO_REGION_NTSC;
}

void retro_set_controller_port_device(unsigned /*port*/, unsigned /*device*/)
{
    // Empty
}

void *retro_get_memory_data(unsigned /*id*/)
{
    return nullptr;
}

size_t retro_get_memory_size(unsigned /*id*/)
{
    return 0;
}

size_t retro_serialize_size()
{
    return 0;
}

bool retro_serialize(void */*data*/, size_t /*size*/)
{
    return false;
}

bool retro_unserialize(const void */*data*/, size_t /*size*/)
{
    return false;
}

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
    audio_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
    input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
    input_state_cb = cb;
}

void draw_letter(int x, int y, int letter)
{
    uint16_t *fb = &graphics::framebuffer[x + graphics::stride * y];
    uint16_t *rom = &sysfont_data[letter * 9];
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            *(fb + r * graphics::stride + c) = *(rom + r * 256 * 9 + c);
        }
    }
}

std::thread logic_thread; 

uint8_t g_offset{0};
std::atomic<bool> logic_thread_active{true};

void logic_update()
{
    using namespace std::chrono_literals;

    while(logic_thread_active) {
        g_offset++;
        std::this_thread::sleep_for(100ms);
    }
}

void retro_init()
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
    stbi_uc *data = stbi_load_from_memory(bundled::data_sysfont_png, bundled::data_sysfont_png_len, &sysfont_width, &sysfont_height, &channels, 0);
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
    // Setup duktape
    js_context = std::make_unique<js::Context>();
    js_context->eval("id = native_sprite_add(64, 64);");
    js_context->eval("native_fill_rect(id, '#ff0000', 0, 0, 16, 64);");
    js_context->eval("native_fill_rect(id, '#00ff00', 16, 0, 16, 64);");
    js_context->eval("native_fill_rect(id, '#0000ff', 32, 0, 16, 64);");

    logic_thread = std::thread(logic_update);
}

void retro_deinit()
{
    sprite::sprites.clear();
    js_context.reset(nullptr);
    logic_thread_active = false;
    logic_thread.join();
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

    *info = retro_system_av_info{};
    info->timing.fps            = graphics::fps;
    info->timing.sample_rate    = audio::framerate;
    info->geometry.base_width   = graphics::width;
    info->geometry.base_height  = graphics::height;
    info->geometry.max_width    = graphics::width;
    info->geometry.max_height   = graphics::height;
    // Don't request any specific aspect ratio

    // the performance level is guide to frontend to give an idea of how intensive this core is to run
    environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &pixel_format);
}

void retro_reset()
{
    // Do stuff
}

int x{graphics::width / 2};
int y{graphics::height / 2};

// Run a single frame
void retro_run()
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
    {
        std::lock_guard<std::mutex> guard(audio::mutex);
        audio_batch_cb(audio::buffer, audio::buffer_len / 2);
    }

    // Render video frame
    graphics::clear();
    graphics::fill(x, y, 20, 20, 0xf000);
    for (auto &sprite : sprite::sprites) {
        sprite.draw(0, 0, sprite.width, sprite.height);
    }

    {
        std::lock_guard<std::mutex> guard(graphics::mutex);
        video_cb(graphics::framebuffer, graphics::width, graphics::height, sizeof(uint16_t) * graphics::stride);
    }
}
