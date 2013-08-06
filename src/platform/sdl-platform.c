/*
 *  sdl-platform.c
 *
 *  Created by Matt Kimball.
 *  Copyright 2012. All rights reserved.
 *  
 *  This file is part of Brogue.
 *
 *  Brogue is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Brogue is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Brogue.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <stdlib.h>

#include <SDL/SDL.h>

#ifdef __APPLE__
#include <SDL_ttf/SDL_ttf.h>
#else
#include <SDL/SDL_ttf.h>
#endif

#include "platform.h"
#include "sdl-keymap.h"
#include "sdl-svgset.h"

#ifdef __APPLE__
#define APP_PATH "Brogue.app/Contents/Resources/"
#else
#define APP_PATH ""
#endif

#define FONT_FILENAME APP_PATH "Andale_Mono.ttf"
// #define FONT_FILENAME APP_PATH "Monaco.ttf"
#define SVG_PATH APP_PATH "svg"

#define SCREEN_WIDTH 100
#define SCREEN_HEIGHT 34
#define MIN_FONT_SIZE 4
#define MAX_FONT_SIZE 128
#define FRAME_TIME (1000 / 30)

extern playerCharacter rogue;
extern short brogueFontSize;

/*  We store characters drawn to the console so that we can redraw them
    when scaling the font or switching to full-screen.  */
struct SDL_CONSOLE_CHAR
{
    uchar c;
    SDL_Color fg, bg;
    int flags;
};
typedef struct SDL_CONSOLE_CHAR SDL_CONSOLE_CHAR;

/*  We store font metrics for all potential font sizes so that we can
    intelligently select font sizes for the user.  */
struct SDL_CONSOLE_FONT_METRICS
{
    unsigned size;
    int width;
    int descent;
};
typedef struct SDL_CONSOLE_FONT_METRICS SDL_CONSOLE_FONT_METRICS;

/*  We store important information about the state of the console in 
    an instance of SDL_CONSOLE.  */
struct SDL_CONSOLE
{
    char *app_path;

    int fullscreen;
    SDL_Surface *screen;
    SDL_Event queued_event;
    int is_queued_event_present;

    TTF_Font *font;
    SDL_CONSOLE_FONT_METRICS *metrics;
    SDL_SVGSET *svgset;

    int mouse_x, mouse_y;
    SDL_KEYMAP *keymap;

    int screen_dirty;
    SDL_CONSOLE_CHAR *screen_contents;
    SDL_CONSOLE_FONT_METRICS *all_metrics;

    unsigned last_frame_timestamp;
    int enable_frame_time_display;
    unsigned frame_count;
    unsigned accumulated_frame_time;
};
typedef struct SDL_CONSOLE SDL_CONSOLE;

static SDL_CONSOLE console;

/*  Return a path relative to the executable location.  While we are
    in game we change to the save game directory, so we can't use
    relative paths.  */
char *SdlConsole_getAppPath(char *file)
{
    char *path = malloc(strlen(file) + strlen(console.app_path) + 2);
    sprintf(path, "%s/%s", console.app_path, file);
    return path;
}

/*  Brogue uses 0-100 as its color range, but SDL uses 0-255.  */
void SdlConsole_scaleColor(SDL_Color *color)
{
    color->r = color->r * 255 / 100;
    color->g = color->g * 255 / 100;
    color->b = color->b * 255 / 100;
}

/*  Plot an individual glyph on the console.  */
void SdlConsole_plotChar(uchar inputChar,
			 short xLoc, short yLoc,
			 short foreRed, short foreGreen, short foreBlue,
			 short backRed, short backGreen, short backBlue,
                         int flags)
{
    SDL_Color fg = { foreRed, foreGreen, foreBlue };
    SDL_Color bg = { backRed, backGreen, backBlue };
    SDL_Rect rect = { xLoc * console.metrics->width, 
		      yLoc * console.metrics->size, 
		      console.metrics->width, 
		      console.metrics->size };
    SDL_Surface *glyph;
    SDL_CONSOLE_CHAR *console_char;
    unsigned bg_color;
    int minx, maxx, miny, maxy;
    int err;

    if (xLoc < 0 || xLoc >= SCREEN_WIDTH)
    {
	return;
    }
    if (yLoc < 0 || yLoc >= SCREEN_HEIGHT)
    {
	return;
    }

    /*  Store the glyphs currently displayed for redraw during font resize.  */
    console_char = &console.screen_contents[yLoc * SCREEN_WIDTH + xLoc];
    console_char->c = inputChar;
    console_char->fg = fg;
    console_char->bg = bg;
    console_char->flags = flags;
    console.screen_dirty = 1;

    SdlConsole_scaleColor(&fg);
    SdlConsole_scaleColor(&bg);

    /*  Clear the full cell to the background color.  */
    bg_color = SDL_MapRGB(console.screen->format, bg.r, bg.g, bg.b);
    SDL_FillRect(console.screen, &rect, bg_color);

    glyph = NULL;

    // flags = 0;
    if (flags & PLOT_CHAR_TILE)
    {
	int frame = SDL_GetTicks() / FRAME_TIME; 

	glyph = SdlSvgset_render(console.svgset, inputChar, frame, fg);
    }

    if (glyph == NULL)
    {
	glyph = TTF_RenderGlyph_Blended(console.font, inputChar, fg);
	if (glyph == NULL)
	{
	    return;
	}

	err = TTF_GlyphMetrics(console.font, inputChar, 
			       &minx, &maxx, &miny, &maxy, NULL);
	if (err)
	{
	    SDL_FreeSurface(glyph);
	    return;
	}
	
	/*  Center the glyph horizontally, adjust for descenders vertically. */
	rect.x += (console.metrics->width - maxx + minx) / 2;
	rect.y += (console.metrics->size - maxy) + 
	    console.metrics->descent - 1;
    }
	
    /*  Blit the character to the screen.  */
    SDL_BlitSurface(glyph, NULL, console.screen, &rect);
    SDL_FreeSurface(glyph);
}

/*  We redraw everything when resizing the font.  */
void SdlConsole_redrawCharacters(void)
{
    int x, y;

    for (y = 0; y < SCREEN_HEIGHT; y++)
    {
	for (x = 0; x < SCREEN_WIDTH; x++)
	{
	    SDL_CONSOLE_CHAR *console_char;
	    SDL_Color *fg, *bg;

	    console_char = &console.screen_contents[y * SCREEN_WIDTH + x];
	    fg = &console_char->fg;
	    bg = &console_char->bg;

	    SdlConsole_plotChar(console_char->c, x, y,
				fg->r, fg->g, fg->b,
				bg->r, bg->g, bg->b,
		                console_char->flags);
	}
    }
}

/*  Choose the initial font size by trying to fill most of the screen.  */
int SdlConsole_getFontSizeForScreen(void)
{
    int size;
    const SDL_VideoInfo *video_info;
    int target_width, target_height;

    video_info = SDL_GetVideoInfo();
    size = MIN_FONT_SIZE;
    target_width = video_info->current_w * 15 / 16;
    target_height = video_info->current_h * 15 / 16;
    while (size < MAX_FONT_SIZE)
    {
	SDL_CONSOLE_FONT_METRICS *next_metrics = 
	    &console.all_metrics[size + 1 - MIN_FONT_SIZE];
	
	if (next_metrics->width * SCREEN_WIDTH > target_width)
	{
	    break;
	}
	if (next_metrics->size * SCREEN_HEIGHT > target_height)
	{
	    break;
	}
	
	size++;
    }
    
    return size;
}

/*  Extract metric information for all font sizes we might use to display.  */
void SdlConsole_generateFontMetrics(void)
{
    int size;

    for (size = MIN_FONT_SIZE; size <= MAX_FONT_SIZE; size++)
    {
	char *font_path;
	TTF_Font *font;
	SDL_CONSOLE_FONT_METRICS *metrics = 
	    &console.all_metrics[size - MIN_FONT_SIZE];

	font_path = SdlConsole_getAppPath(FONT_FILENAME);
	font = TTF_OpenFont(font_path, size);
	free(font_path);
	if (font == NULL)
	{
	    printf("%s\n", TTF_GetError());
	    exit(1);
	}

	metrics->size = size;
	metrics->descent = TTF_FontDescent(font);
	if (TTF_SizeText(font, "M", &metrics->width, NULL))
	{
	    printf("%s\n", TTF_GetError());
	    exit(1);
	}

	TTF_CloseFont(font);
    }

    size = brogueFontSize;
    if (size < MIN_FONT_SIZE || size >= MAX_FONT_SIZE)
    {
	size = -1;
    }

    if (size == -1)
    {
	size = SdlConsole_getFontSizeForScreen();
    }

    console.metrics = &console.all_metrics[size - MIN_FONT_SIZE];
}

/*  We need to reload the font and adjust the video mode when changing
    font size.  The fullscreen enable/disable path goes through here, too.  */
void SdlConsole_scaleFont(void)
{
    int width, height;
    char *font_path;
    char *svg_path;

    if (console.font != NULL)
    {
	TTF_CloseFont(console.font);
    }
    if (console.svgset != NULL)
    {
	SdlSvgset_free(console.svgset);
    }

    font_path = SdlConsole_getAppPath(FONT_FILENAME);
    console.font = TTF_OpenFont(font_path, console.metrics->size);
    free(font_path);
    if (console.font == NULL)
    {
	printf("%s\n", TTF_GetError());
	exit(1);
    }

    svg_path = SdlConsole_getAppPath(SVG_PATH);
    console.svgset = SdlSvgset_alloc(svg_path, 
				     console.metrics->width,
				     console.metrics->size);
    free(svg_path);
    if (console.svgset == NULL)
    {
	printf("Failure to load SVG set\n");
	exit(1);
    }

    width = console.metrics->width * SCREEN_WIDTH;
    height = console.metrics->size * SCREEN_HEIGHT;
    console.screen = NULL;
    if (console.fullscreen)
    {
	console.screen = SDL_SetVideoMode(width, height, 32, 
					  SDL_SWSURFACE | SDL_ANYFORMAT |
					  SDL_FULLSCREEN);
    }

    /*  If fullscreen fails, we'll fall back to trying windowed.  */
    if (console.screen == NULL)
    {
	console.screen = SDL_SetVideoMode(width, height, 32,
					  SDL_SWSURFACE | SDL_ANYFORMAT);
    }

    if (console.screen == NULL)
    {
	printf("Failure to set video mode\n");
	exit(1);
    }

    SdlConsole_redrawCharacters();
}

/*  Flip the screen buffer if anything has been drawn since the last flip.  */
void SdlConsole_refresh(void)
{
    if (console.screen_dirty)
    {
	SDL_Flip(console.screen);
	console.screen_dirty = 0;
    }
}

/*  Pause for a known amount of time.  Return true if an event happened.  */
boolean SdlConsole_pauseForMilliseconds(short milliseconds)
{
    SdlConsole_refresh();
    SDL_Delay(milliseconds);

    while (!console.is_queued_event_present)
    {
	if (!SDL_PollEvent(&console.queued_event))
	{
	    break;
	}

	if (console.queued_event.type == SDL_KEYUP)
	{
	    continue;
	}

	console.is_queued_event_present = 1;
    }

    return console.is_queued_event_present;
}

/*  Return true if mouse motion is immediately pending in the event queue.  */
boolean SdlConsole_isMouseMotionPending(void)
{
    if (!console.is_queued_event_present)
    {
	if (!SDL_PollEvent(&console.queued_event))
	{
	    return 0;
	}

	console.is_queued_event_present = 1;
    }

    return console.queued_event.type == SDL_MOUSEMOTION;
}

/*  Increase the font size.  We check the width because bumping up the
    height but keeping the same width feels a bit too incremental.  */
void SdlConsole_increaseFont(void)
{
    int size = console.metrics->size;
    while (size < MAX_FONT_SIZE)
    {
	if (console.all_metrics[size - MIN_FONT_SIZE].width
	    != console.metrics->width)
	{
	    break;
	}
	
	size++;
    }
    console.metrics = &console.all_metrics[size - MIN_FONT_SIZE];

    console.fullscreen = 0;
    SdlConsole_scaleFont();
}

/*  Decrease the font size.  */
void SdlConsole_decreaseFont(void)
{
    int size = console.metrics->size;
    while (size > MIN_FONT_SIZE)
    {
	if (console.all_metrics[size - MIN_FONT_SIZE].width
	    != console.metrics->width)
	{
	    break;
	}
	
	size--;
    }
    console.metrics = &console.all_metrics[size - MIN_FONT_SIZE];

    console.fullscreen = 0;
    SdlConsole_scaleFont();
}

/*  Capture a screenshot, assigning a filename based of the screenshots
    which already exist in the running directory.  */
void SdlConsole_captureScreenshot(void)
{
    int screenshot_num = 0;
    char screenshot_file[32];
	
    while (screenshot_num <= 999)
    {
	FILE *f;

	snprintf(screenshot_file, 32, "screenshot%03d.bmp", screenshot_num);
	f = fopen(screenshot_file, "rb");
	if (f == NULL)
	{
	    break;
	}
	fclose(f);

	screenshot_num++;
    }

    SDL_SaveBMP(console.screen, screenshot_file);
}

/*  Handle console specific key responses.  */
int SdlConsole_processKey(SDL_KeyboardEvent *keyboardEvent, boolean textInput)
{
    SDLKey key;
    uchar unicode;
    int mod;

    SdlKeymap_translate(console.keymap, keyboardEvent);

    key = keyboardEvent->keysym.sym;
    mod = keyboardEvent->keysym.mod;
    unicode = keyboardEvent->keysym.unicode;

    /*  Some control characters we aren't interested in -- we'd rather
	have the alphabetic key.  */
    if (unicode < 27)
    {
	keyboardEvent->keysym.unicode = key;
    }

    if (key == SDLK_PAGEUP || (!textInput && unicode == '+'))
    {
	SdlConsole_increaseFont();
	return 1;
    }
    if (key == SDLK_PAGEDOWN || (!textInput && unicode == '-'))
    {
	SdlConsole_decreaseFont();
	return 1;
    }

    if (key == SDLK_PRINT || key == SDLK_F11)
    {
	SdlConsole_captureScreenshot();
	return 1;
    }
    if (key == SDLK_F12)
    {
	console.fullscreen = !console.fullscreen;
	SdlConsole_scaleFont();
	return 1;
    }
    if (key == SDLK_f && (mod & KMOD_CTRL))
    {
	console.enable_frame_time_display = !console.enable_frame_time_display;
    }

    /*  
	We don't want characters such as shift to be interpreted in
        text input fields.  
    */
    if (key >= SDLK_NUMLOCK && key <= SDLK_COMPOSE)
    {
	return 1;
    }

    if (key == SDLK_BACKSPACE)
    {
	keyboardEvent->keysym.unicode = DELETE_KEY;
    }
    if (key == SDLK_UP || key == SDLK_KP8)
    {
	keyboardEvent->keysym.unicode = UP_KEY;
    }
    if (key == SDLK_LEFT || key == SDLK_KP4)
    {
	keyboardEvent->keysym.unicode = LEFT_KEY;
    }
    if (key == SDLK_RIGHT || key == SDLK_KP6)
    {
	keyboardEvent->keysym.unicode = RIGHT_KEY;
    }
    if (key == SDLK_DOWN || key == SDLK_KP2)
    {
	keyboardEvent->keysym.unicode = DOWN_KEY;
    }
    if (key == SDLK_KP7)
    {
	keyboardEvent->keysym.unicode = UPLEFT_KEY;
    }
    if (key == SDLK_KP9)
    {
	keyboardEvent->keysym.unicode = UPRIGHT_KEY;
    }
    if (key == SDLK_KP1)
    {
	keyboardEvent->keysym.unicode = DOWNLEFT_KEY;
    }
    if (key == SDLK_KP3)
    {
	keyboardEvent->keysym.unicode = DOWNRIGHT_KEY;
    }

    return 0;
}

/*  
    Display the frame generation time average in the upper right 
    corner of the display, so that we can measure performance while
    developing.  
*/
void SdlConsole_displayFrameTime(char *str)
{
    int len;
    int i;

    len = strlen(str);
    for (i = 0; i < len; i++)
    {
	SdlConsole_plotChar(str[i], SCREEN_WIDTH - len + i, 0,
			    255, 255, 255, 0, 0, 0, 0);
    }
}

/*
    If we have generated the previous frame faster than 30 FPS, then 
    sleep for the remainder of the frame to avoid hogging the CPU and
    draining batteries unnecessarily.
*/
void SdlConsole_waitForNextFrame(int wait_time)
{
    int now, delay, frame_time;

    now = SDL_GetTicks();

    if (console.last_frame_timestamp == 0)
    {
	console.last_frame_timestamp = now;
    }

    frame_time = now - console.last_frame_timestamp;
    if (console.enable_frame_time_display)
    {
	double average_frame_time;
	char frame_time_str[32];

	console.accumulated_frame_time += frame_time;
	if (console.frame_count % 30 == 0)
	{
	    average_frame_time = 
		(double)console.accumulated_frame_time / 30.0;

	    snprintf(frame_time_str, sizeof(frame_time_str), 
		     "%6.2f ms", average_frame_time);
	    SdlConsole_displayFrameTime(frame_time_str);
	    
	    console.accumulated_frame_time = 0;
	}
    }

    delay = wait_time - frame_time;
    if (delay > 0)
    {
	SDL_Delay(wait_time); 
    }

    console.last_frame_timestamp = SDL_GetTicks();
    console.frame_count++;
}

/*  Return the queued SDL event, if we have one.  Otherwise, return
    the next event from SDL.  */
void SdlConsole_nextSdlEvent(SDL_Event *event, int wait_time)
{
    if (console.is_queued_event_present)
    {
	*event = console.queued_event;
	console.is_queued_event_present = 0;
    }
    else
    {
	int success = 0;

	while (!success)
	{
	    if (wait_time == -1)
	    {
		success = SDL_WaitEvent(event);

		if (!success)
		{
		    printf("%s\n", SDL_GetError());
		}
	    }
	    else
	    {
		success = SDL_PollEvent(event);
		if (!SDL_PollEvent(event))
		{
		    SdlConsole_waitForNextFrame(wait_time);
		    
		    shuffleTerrainColors(3, true);
		    commitDraws();
		    SdlConsole_redrawCharacters();
		    SdlConsole_refresh();

		    success = SDL_PollEvent(event);
		}
	    }
	}
    }
}

/*  Check for shift or ctrl held down.  */
boolean SdlConsole_modifierHeld(int modifier)
{
    int modstate = SDL_GetModState();

    if (modifier == 0)
    {
	return modstate & KMOD_SHIFT;
    }
    
    if (modifier == 1)
    {
	return modstate & KMOD_CTRL;
    }

    return 0;
}

/*  Wait for the next input Brogue needs to respond to, and return it.  */
void SdlConsole_nextKeyOrMouseEvent(rogueEvent *returnEvent, 
				    boolean textInput, 
				    boolean colorsDance)
{
    SdlConsole_refresh();

    memset(returnEvent, 0, sizeof(rogueEvent));

    while (1)
    {
	SDL_Event event;

	SdlConsole_nextSdlEvent(&event,
				colorsDance ? FRAME_TIME : -1);

	returnEvent->shiftKey = SdlConsole_modifierHeld(0);
	returnEvent->controlKey = SdlConsole_modifierHeld(1);

	if (event.type == SDL_KEYDOWN)
	{
	    SDL_KeyboardEvent *keyboardEvent = (SDL_KeyboardEvent *)&event;

	    if (SdlConsole_processKey(keyboardEvent, textInput))
	    {
		return;
	    }

	    returnEvent->eventType = KEYSTROKE;
	    returnEvent->param1 = keyboardEvent->keysym.unicode;

	    return;
	}

	if (event.type == SDL_MOUSEMOTION)
	{
	    if (SdlConsole_isMouseMotionPending())
	    {
		/*  If more mouse motion is already pending, drop this
		    mouse motion event in favor of the next one.  */
		continue;
	    }

	    SDL_MouseMotionEvent *mouseEvent = (SDL_MouseMotionEvent *)&event;
	    int mx = mouseEvent->x / console.metrics->width;
	    int my = mouseEvent->y / console.metrics->size;

	    if (mx == console.mouse_x && my == console.mouse_y)
	    {
		continue;
	    }

	    console.mouse_x = mx;
	    console.mouse_y = my;

	    returnEvent->eventType = MOUSE_ENTERED_CELL;
	    returnEvent->param1 = mx;
	    returnEvent->param2 = my;

	    return;
	}

	if (event.type == SDL_MOUSEBUTTONDOWN 
	    || event.type == SDL_MOUSEBUTTONUP)
	{
	    SDL_MouseButtonEvent *buttonEvent = (SDL_MouseButtonEvent *)&event;

	    int mx = buttonEvent->x / console.metrics->width;
	    int my = buttonEvent->y / console.metrics->size;

	    if (buttonEvent->button == SDL_BUTTON_LEFT)
	    {
		returnEvent->eventType = 
		    ((event.type == SDL_MOUSEBUTTONDOWN) ? 
		     MOUSE_DOWN : MOUSE_UP);
	    }
	    else if (buttonEvent->button == SDL_BUTTON_RIGHT)
	    {
		returnEvent->eventType = 
		    ((event.type == SDL_MOUSEBUTTONDOWN) ? 
		     RIGHT_MOUSE_DOWN : RIGHT_MOUSE_UP);
	    }
	    else
	    {
		continue;
	    }

	    returnEvent->param1 = mx;
	    returnEvent->param2 = my;

	    return;
	}

	if (event.type == SDL_QUIT)
	{
	    rogue.gameHasEnded = 1;
	    rogue.nextGame = NG_QUIT;
	    
	    returnEvent->eventType = KEYSTROKE;
	    returnEvent->param1 = ESCAPE_KEY;
	    
	    return;
	}
    }
}

/*  Set the window icon.  */
void SdlConsole_setIcon(void)
{
    SDL_Surface *icon;

    SDL_WM_SetCaption("Brogue " BROGUE_VERSION_STRING, "Brogue");

    icon = SDL_LoadBMP("icon.bmp");
    if (icon == NULL)
    {
	return;
    }

    SDL_WM_SetIcon(icon, NULL);
}

/*  Remap a key, allocating a keymap as necessary, as the keymap translation
    starts before the platform game loop.  */
void SdlConsole_remap(const char *input_name, const char *output_name)
{
    int err;

    if (console.keymap == NULL)
    {
	console.keymap = SdlKeymap_alloc();
    }

    err = SdlKeymap_addTranslation(console.keymap, input_name, output_name);
    if (err == EINVAL)
    {
	printf("Error mapping '%s' -> '%s'\n", input_name, output_name);
    }
}

/*  Allocate the structures to be used by the console.  */
void SdlConsole_allocate(void)
{
    int i;
    char app_dir[1024];

    if (getcwd(app_dir, 1024) == NULL)
    {
	printf("Failed to get working directory\n");
	exit(1);
    }

    console.app_path = malloc(strlen(app_dir) + 1);
    strcpy(console.app_path, app_dir);

    console.screen_contents = malloc(sizeof(SDL_CONSOLE_CHAR) * 
				     SCREEN_WIDTH * SCREEN_HEIGHT);
    console.all_metrics = malloc(sizeof(SDL_CONSOLE_FONT_METRICS) *
				 (MAX_FONT_SIZE - MIN_FONT_SIZE + 1));
    if (console.keymap == NULL)
    {
	console.keymap = SdlKeymap_alloc();
    }

    if (console.screen_contents == NULL || console.all_metrics == NULL
	|| console.keymap == NULL)
    {
	printf("Failed to allocate screen contents\n");
	exit(1);
    }

    memset(console.screen_contents, 0, 
	   sizeof(SDL_CONSOLE_CHAR) * SCREEN_WIDTH * SCREEN_HEIGHT);
    for (i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
    {
	console.screen_contents[i].c = ' ';
    }

    memset(console.all_metrics, 0, 
	   sizeof(SDL_CONSOLE_FONT_METRICS) 
	   * (MAX_FONT_SIZE - MIN_FONT_SIZE + 1));
}

/*  Free the memory used by the console.  */
void SdlConsole_free(void)
{
    free(console.screen_contents);
    console.screen_contents = NULL;

    free(console.all_metrics);
    console.all_metrics = NULL;

    SdlKeymap_free(console.keymap);
    console.keymap = NULL;

    if (console.svgset != NULL)
    {
	SdlSvgset_free(console.svgset);
	console.svgset = NULL;
    }

    if (console.font != NULL)
    {
	TTF_CloseFont(console.font);
	console.font = NULL;
    }
}

/*  The main entry point to the SDL-specific code.  */
void SdlConsole_gameLoop(void)
{
    int err;

    SdlConsole_allocate();

    err = SDL_Init(SDL_INIT_VIDEO);
    if (err)
    {
	printf("Failed to initialize SDL\n");
	exit(1);
    }
    atexit(SDL_Quit);

    err = TTF_Init();
    if (err)
    {
	printf("Failed to initialize SDL_TTF\n");
    }

    SdlConsole_setIcon();
    SdlConsole_generateFontMetrics();
    SdlConsole_scaleFont();

    SDL_EnableUNICODE(1);
    SDL_EnableKeyRepeat(175, 30);

    rogueMain();

    SdlConsole_free();
}

/*  The function table used by Brogue to call into the SDL implementation.  */
struct brogueConsole sdlConsole = {
    SdlConsole_gameLoop,
    SdlConsole_pauseForMilliseconds,
    SdlConsole_nextKeyOrMouseEvent,
    SdlConsole_plotChar,
    SdlConsole_remap,
    SdlConsole_modifierHeld,
};
