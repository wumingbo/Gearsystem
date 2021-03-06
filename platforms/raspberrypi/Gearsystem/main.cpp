/*
 * Gearboy - Nintendo Game Boy Emulator
 * Copyright (C) 2012  Ignacio Sanchez

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <sys/time.h>
#include <SDL2/SDL.h>
#include <libconfig.h++>
#include "bcm_host.h"
#include "GLES/gl.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"
#include "gearsystem.h"

using namespace std;
using namespace libconfig;

bool running = true;
bool paused = false;

EGLDisplay display;
EGLSurface surface;
EGLContext context;

static const char *output_file = "gearsystem.cfg";

const float kGS_Width = 256.0f;
const float kGS_Height = 224.0f;
const float kGS_TexWidth = 1.0f;
const float kGS_TexHeight = kGS_Height / 256.0f;
const GLfloat kQuadTex[8] = { 0, 0, kGS_TexWidth, 0, kGS_TexWidth, kGS_TexHeight, 0, kGS_TexHeight};
GLshort quadVerts[8];

GearsystemCore* theGearsystemCore;
GS_Color* theFrameBuffer;
GLuint theGSTexture;

SDL_Joystick* game_pad = NULL;
SDL_Keycode kc_keypad_left, kc_keypad_right, kc_keypad_up, kc_keypad_down, kc_keypad_1, kc_keypad_2, kc_keypad_start, kc_emulator_pause, kc_emulator_quit;
bool jg_x_axis_invert, jg_y_axis_invert;
int jg_1, jg_2, jg_start, jg_x_axis, jg_y_axis;

uint32_t screen_width, screen_height;

SDL_Window* theWindow;

void update(void)
{
    SDL_Event keyevent;

    while (SDL_PollEvent(&keyevent))
    {
        switch(keyevent.type)
        {
            case SDL_QUIT:
            running = false;
            break;

            case SDL_JOYBUTTONDOWN:
            {
                if (keyevent.jbutton.button == jg_1)
                    theGearsystemCore->KeyPressed(Joypad_1, Key_1);
                else if (keyevent.jbutton.button == jg_2)
                    theGearsystemCore->KeyPressed(Joypad_1, Key_2);
                else if (keyevent.jbutton.button == jg_start)
                    theGearsystemCore->KeyPressed(Joypad_1, Key_Start);
            }
            break;

            case SDL_JOYBUTTONUP:
            {
                if (keyevent.jbutton.button == jg_1)
                    theGearsystemCore->KeyReleased(Joypad_1, Key_1);
                else if (keyevent.jbutton.button == jg_2)
                    theGearsystemCore->KeyReleased(Joypad_1, Key_2);
                else if (keyevent.jbutton.button == jg_start)
                    theGearsystemCore->KeyReleased(Joypad_1, Key_Start);
            }
            break;

            case SDL_JOYAXISMOTION:
            {
                if(keyevent.jaxis.axis == jg_x_axis)
                {
                    int x_motion = keyevent.jaxis.value * (jg_x_axis_invert ? -1 : 1);
                    if (x_motion < 0)
                        theGearsystemCore->KeyPressed(Joypad_1, Key_Left);
                    else if (x_motion > 0)
                        theGearsystemCore->KeyPressed(Joypad_1, Key_Right);
                    else
                    {
                        theGearsystemCore->KeyReleased(Joypad_1, Key_Left);
                        theGearsystemCore->KeyReleased(Joypad_1, Key_Right);
                    }
                }
                else if(keyevent.jaxis.axis == jg_y_axis)
                {
                    int y_motion = keyevent.jaxis.value * (jg_y_axis_invert ? -1 : 1);
                    if (y_motion < 0)
                        theGearsystemCore->KeyPressed(Joypad_1, Key_Up);
                    else if (y_motion > 0)
                        theGearsystemCore->KeyPressed(Joypad_1, Key_Down);
                    else
                    {
                        theGearsystemCore->KeyReleased(Joypad_1, Key_Up);
                        theGearsystemCore->KeyReleased(Joypad_1, Key_Down);
                    }
                }
            }
            break;

            case SDL_KEYDOWN:
            {
                if (keyevent.key.keysym.sym == kc_keypad_left)
                    theGearsystemCore->KeyPressed(Joypad_1, Key_Left);
                else if (keyevent.key.keysym.sym == kc_keypad_right)
                    theGearsystemCore->KeyPressed(Joypad_1, Key_Right);
                else if (keyevent.key.keysym.sym == kc_keypad_up)
                    theGearsystemCore->KeyPressed(Joypad_1, Key_Up);
                else if (keyevent.key.keysym.sym == kc_keypad_down)
                    theGearsystemCore->KeyPressed(Joypad_1, Key_Down);
                else if (keyevent.key.keysym.sym == kc_keypad_1)
                    theGearsystemCore->KeyPressed(Joypad_1, Key_1);
                else if (keyevent.key.keysym.sym == kc_keypad_2)
                    theGearsystemCore->KeyPressed(Joypad_1, Key_2);
                else if (keyevent.key.keysym.sym == kc_keypad_start)
                    theGearsystemCore->KeyPressed(Joypad_1, Key_Start);

                if (keyevent.key.keysym.sym == kc_emulator_quit)
                    running = false;
                else if (keyevent.key.keysym.sym == kc_emulator_pause)
                {
                    paused = !paused;
                    theGearsystemCore->Pause(paused);
                }
            }
            break;

            case SDL_KEYUP:
            {
                if (keyevent.key.keysym.sym == kc_keypad_left)
                    theGearsystemCore->KeyReleased(Joypad_1, Key_Left);
                else if (keyevent.key.keysym.sym == kc_keypad_right)
                    theGearsystemCore->KeyReleased(Joypad_1, Key_Right);
                else if (keyevent.key.keysym.sym == kc_keypad_up)
                    theGearsystemCore->KeyReleased(Joypad_1, Key_Up);
                else if (keyevent.key.keysym.sym == kc_keypad_down)
                    theGearsystemCore->KeyReleased(Joypad_1, Key_Down);
                else if (keyevent.key.keysym.sym == kc_keypad_1)
                    theGearsystemCore->KeyReleased(Joypad_1, Key_1);
                else if (keyevent.key.keysym.sym == kc_keypad_2)
                    theGearsystemCore->KeyReleased(Joypad_1, Key_2);
                else if (keyevent.key.keysym.sym == kc_keypad_start)
                    theGearsystemCore->KeyReleased(Joypad_1, Key_Start);
            }
            break;
        }
    }

    theGearsystemCore->RunToVBlank(theFrameBuffer);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 224, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*) theFrameBuffer);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    eglSwapBuffers(display, surface);
}

void init_sdl(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
    {
        Log("SDL Error Init: %s", SDL_GetError());
    }

    theWindow = SDL_CreateWindow("Gearsystem", 0, 0, 0, 0, 0);

    if (theWindow == NULL)
    {
        Log("SDL Error Video: %s", SDL_GetError());
    }

    SDL_ShowCursor(SDL_DISABLE);

    game_pad = SDL_JoystickOpen(0);

    if(game_pad == NULL)
    {
        Log("Warning: Unable to open game controller! SDL Error: %s\n", SDL_GetError());
    }

    kc_keypad_left = SDLK_LEFT;
    kc_keypad_right = SDLK_RIGHT;
    kc_keypad_up = SDLK_UP;
    kc_keypad_down = SDLK_DOWN;
    kc_keypad_1 = SDLK_a;
    kc_keypad_2 = SDLK_s;
    kc_keypad_start = SDLK_RETURN;
    kc_emulator_pause = SDLK_p;
    kc_emulator_quit = SDLK_ESCAPE;

    jg_x_axis_invert = false;
    jg_y_axis_invert = false;
    jg_1 = 2;
    jg_2 = 1;
    jg_start = 9;
    jg_x_axis = 0;
    jg_y_axis = 1;

    Config cfg;

    try
    {
        cfg.readFile(output_file);

        try
        {
            const Setting& root = cfg.getRoot();
            const Setting &gearsystem = root["Gearsystem"];

            string keypad_left, keypad_right, keypad_up, keypad_down, keypad_1, keypad_2, 
            keypad_start, keypad_select, emulator_pause, emulator_quit;
            gearsystem.lookupValue("keypad_left", keypad_left);
            gearsystem.lookupValue("keypad_right", keypad_right);
            gearsystem.lookupValue("keypad_up", keypad_up);
            gearsystem.lookupValue("keypad_down", keypad_down);
            gearsystem.lookupValue("keypad_1", keypad_1);
            gearsystem.lookupValue("keypad_2", keypad_2);
            gearsystem.lookupValue("keypad_start_pause", keypad_start);
            gearsystem.lookupValue("keypad_select", keypad_select);

            gearsystem.lookupValue("joystick_gamepad_1", jg_1);
            gearsystem.lookupValue("joystick_gamepad_2", jg_2);
            gearsystem.lookupValue("joystick_gamepad_start_pause", jg_start);
            gearsystem.lookupValue("joystick_gamepad_x_axis", jg_x_axis);
            gearsystem.lookupValue("joystick_gamepad_y_axis", jg_y_axis);
            gearsystem.lookupValue("joystick_gamepad_x_axis_invert", jg_x_axis_invert);
            gearsystem.lookupValue("joystick_gamepad_y_axis_invert", jg_y_axis_invert);

            gearsystem.lookupValue("emulator_pause", emulator_pause);
            gearsystem.lookupValue("emulator_quit", emulator_quit);

            kc_keypad_left = SDL_GetKeyFromName(keypad_left.c_str());
            kc_keypad_right = SDL_GetKeyFromName(keypad_right.c_str());
            kc_keypad_up = SDL_GetKeyFromName(keypad_up.c_str());
            kc_keypad_down = SDL_GetKeyFromName(keypad_down.c_str());
            kc_keypad_1 = SDL_GetKeyFromName(keypad_1.c_str());
            kc_keypad_2 = SDL_GetKeyFromName(keypad_2.c_str());
            kc_keypad_start = SDL_GetKeyFromName(keypad_start.c_str());

            kc_emulator_pause = SDL_GetKeyFromName(emulator_pause.c_str());
            kc_emulator_quit = SDL_GetKeyFromName(emulator_quit.c_str());
        }
        catch(const SettingNotFoundException &nfex)
        {
            std::cerr << "Setting not found" << std::endl;
        }
    }
    catch(const FileIOException &fioex)
    {
        Log("I/O error while reading file: %s", output_file);
    }
    catch(const ParseException &pex)
    {
        std::cerr << "Parse error at " << pex.getFile() << ":" << pex.getLine()
              << " - " << pex.getError() << std::endl;
    } 
}

void init_ogl(void)
{
    int32_t success = 0;
    EGLBoolean result;
    EGLint num_config;

    static EGL_DISPMANX_WINDOW_T nativewindow;

    DISPMANX_ELEMENT_HANDLE_T dispman_element;
    DISPMANX_DISPLAY_HANDLE_T dispman_display;
    DISPMANX_UPDATE_HANDLE_T dispman_update;
    VC_DISPMANX_ALPHA_T alpha;
    VC_RECT_T dst_rect;
    VC_RECT_T src_rect;

    static const EGLint attribute_list[] =
    {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_NONE
    };

    EGLConfig config;

    // Get an EGL display connection
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    assert(display!=EGL_NO_DISPLAY);

    // Initialize the EGL display connection
    result = eglInitialize(display, NULL, NULL);
    assert(EGL_FALSE != result);

    // Get an appropriate EGL frame buffer configuration
    result = eglChooseConfig(display, attribute_list, &config, 1, &num_config);
    assert(EGL_FALSE != result);

    // Create an EGL rendering context
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, NULL);
    assert(context!=EGL_NO_CONTEXT);

    // Create an EGL window surface
    success = graphics_get_display_size(0 /* LCD */, &screen_width, &screen_height);
    assert( success >= 0 );

    int32_t zoom = screen_width / GS_SMS_WIDTH;
    int32_t zoom2 = screen_height / GS_SMS_HEIGHT;

    if (zoom2 < zoom)
        zoom = zoom2;

    int32_t display_width = GS_SMS_WIDTH * zoom;
    int32_t display_height = GS_SMS_HEIGHT * zoom;
    int32_t display_offset_x = (screen_width / 2) - (display_width / 2);
    int32_t display_offset_y = (screen_height / 2) - (display_height / 2);

    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.width = screen_width;
    dst_rect.height = screen_height;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = screen_width << 16;
    src_rect.height = screen_height << 16;

    dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
    dispman_update = vc_dispmanx_update_start( 0 );

    alpha.flags = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
    alpha.opacity = 255;
    alpha.mask = 0;

    dispman_element = vc_dispmanx_element_add ( dispman_update, dispman_display,
        0/*layer*/, &dst_rect, 0/*src*/,
        &src_rect, DISPMANX_PROTECTION_NONE, &alpha, 0/*clamp*/, DISPMANX_NO_ROTATE/*transform*/);

    nativewindow.element = dispman_element;
    nativewindow.width = screen_width;
    nativewindow.height = screen_height;
    vc_dispmanx_update_submit_sync( dispman_update );

    surface = eglCreateWindowSurface( display, config, &nativewindow, NULL );
    assert(surface != EGL_NO_SURFACE);

    // Connect the context to the surface
    result = eglMakeCurrent(display, surface, surface, context);
    assert(EGL_FALSE != result);

    eglSwapInterval(display, 1);

    glGenTextures(1, &theGSTexture);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, theGSTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*) NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0.0f, screen_width, screen_height, 0.0f, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glViewport(0.0f, 0.0f, screen_width, screen_height);

    quadVerts[0] = display_offset_x;
    quadVerts[1] = display_offset_y;
    quadVerts[2] = display_offset_x + display_width;
    quadVerts[3] = display_offset_y;
    quadVerts[4] = display_offset_x + display_width;
    quadVerts[5] = display_offset_y + display_height;
    quadVerts[6] = display_offset_x;
    quadVerts[7] = display_offset_y + display_height;

    glVertexPointer(2, GL_SHORT, 0, quadVerts);
    glEnableClientState(GL_VERTEX_ARRAY);

    glTexCoordPointer(2, GL_FLOAT, 0, kQuadTex);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glClear(GL_COLOR_BUFFER_BIT);
}

void init(void)
{
    theGearsystemCore = new GearsystemCore();
    theGearsystemCore->Init();

    theFrameBuffer = new GS_Color[GS_SMS_WIDTH * GS_SMS_HEIGHT];

    for (int y = 0; y < GS_SMS_HEIGHT; ++y)
    {
        for (int x = 0; x < GS_SMS_WIDTH; ++x)
        {
            int pixel = (y * GS_SMS_WIDTH) + x;
            theFrameBuffer[pixel].red = theFrameBuffer[pixel].green = theFrameBuffer[pixel].blue = 0x00;
            theFrameBuffer[pixel].alpha = 0xFF;
        }
    }

    bcm_host_init();
    init_sdl();
    init_ogl();
}

void end(void)
{
    Config cfg;

    Setting &root = cfg.getRoot();
    Setting &address = root.add("Gearsystem", Setting::TypeGroup);

    address.add("keypad_left", Setting::TypeString) = SDL_GetKeyName(kc_keypad_left);
    address.add("keypad_right", Setting::TypeString) = SDL_GetKeyName(kc_keypad_right);
    address.add("keypad_up", Setting::TypeString) = SDL_GetKeyName(kc_keypad_up);
    address.add("keypad_down", Setting::TypeString) = SDL_GetKeyName(kc_keypad_down);
    address.add("keypad_1", Setting::TypeString) = SDL_GetKeyName(kc_keypad_1);
    address.add("keypad_2", Setting::TypeString) = SDL_GetKeyName(kc_keypad_2);
    address.add("keypad_start_pause", Setting::TypeString) = SDL_GetKeyName(kc_keypad_start);

    address.add("joystick_gamepad_1", Setting::TypeInt) = jg_1;
    address.add("joystick_gamepad_2", Setting::TypeInt) = jg_2;
    address.add("joystick_gamepad_start_pause", Setting::TypeInt) = jg_start;
    address.add("joystick_gamepad_x_axis", Setting::TypeInt) = jg_x_axis;
    address.add("joystick_gamepad_y_axis", Setting::TypeInt) = jg_y_axis;
    address.add("joystick_gamepad_x_axis_invert", Setting::TypeBoolean) = jg_x_axis_invert;
    address.add("joystick_gamepad_y_axis_invert", Setting::TypeBoolean) = jg_y_axis_invert;

    address.add("emulator_pause", Setting::TypeString) = SDL_GetKeyName(kc_emulator_pause);
    address.add("emulator_quit", Setting::TypeString) = SDL_GetKeyName(kc_emulator_quit);

    try
    {
        cfg.writeFile(output_file);
    }
    catch(const FileIOException &fioex)
    {
        Log("I/O error while writing file: %s", output_file);
    }

    SDL_JoystickClose(game_pad);

    SafeDeleteArray(theFrameBuffer);
    SafeDelete(theGearsystemCore);
    SDL_DestroyWindow(theWindow);
    SDL_Quit();
    bcm_host_deinit();
}

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3)
    {
        printf("usage: %s rom_path [options]\n", argv[0]);
        printf("options:\n-nosound\n");
        return -1;
    }

    init();

    if (argc > 2)
    {
        for (int i = 2; i < argc; i++)
        {
            if (strcmp("-nosound", argv[i]) == 0)
            {
                theGearsystemCore->EnableSound(false);
            }
            else
            {
                end();
                printf("invalid option: %s\n", argv[i]);
                return -1;
            }
        }
    }

    if (theGearsystemCore->LoadROM(argv[1]))
    {
        theGearsystemCore->LoadRam();

        while (running)
        {
            update();
        }

        theGearsystemCore->SaveRam();
    }

    end();

    return 0;
}
