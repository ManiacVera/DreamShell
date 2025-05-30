/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2004 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    BERO
    bero@geocities.co.jp

    based on SDL_nullevents.c by

    Sam Lantinga
    slouken@libsdl.org
*/

#ifdef SAVE_RCSID
static char rcsid =
 "@(#) $Id: SDL_dcevents.c,v 1.2 2004/01/04 16:49:24 slouken Exp $";
#endif

/* Being a null driver, there's no event stream. We just define stubs for
   most of the API. */

#include <kos.h>

#include "SDL.h"
#include "SDL_sysevents.h"
#include "SDL_events_c.h"
#include "SDL_dcvideo.h"
#include "SDL_dcevents_c.h"

#include <dc/maple.h>
#include <dc/maple/mouse.h>
#include <dc/maple/keyboard.h>

#define MIN_FRAME_UPDATE 16

const static unsigned short sdl_key[]= {
	/* 0*/ SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_a, SDLK_b, SDLK_c, SDLK_d, 
	/* 8*/ SDLK_e, SDLK_f, SDLK_g, SDLK_h, SDLK_i, SDLK_j, SDLK_k, SDLK_l, 
	/*10*/ SDLK_m, SDLK_n, SDLK_o, SDLK_p, SDLK_q, SDLK_r, SDLK_s, SDLK_t, 
	/*18*/ SDLK_u, SDLK_v, SDLK_w, SDLK_x, SDLK_y, SDLK_z, SDLK_1, SDLK_2, 
	/*20*/ SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8, SDLK_9, SDLK_0,  
	/*28*/ SDLK_RETURN, SDLK_ESCAPE, SDLK_BACKSPACE, SDLK_TAB, SDLK_SPACE, SDLK_MINUS, SDLK_PLUS, SDLK_LEFTBRACKET, 
	/*30*/ SDLK_RIGHTBRACKET, SDLK_BACKSLASH , SDLK_HASH, SDLK_SEMICOLON, SDLK_QUOTE, SDLK_BACKQUOTE, SDLK_COMMA, SDLK_PERIOD, 
	/*38*/ SDLK_SLASH, SDLK_CAPSLOCK, SDLK_F1, SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5, SDLK_F6,
	/*40*/ SDLK_F7, SDLK_F8, SDLK_F9, SDLK_F10, SDLK_F11, SDLK_F12, SDLK_PRINT, SDLK_SCROLLOCK, 
	/*48*/ SDLK_PAUSE, SDLK_INSERT, SDLK_HOME, SDLK_PAGEUP, SDLK_DELETE, SDLK_END, SDLK_PAGEDOWN, SDLK_RIGHT, 
	/*50*/ SDLK_LEFT, SDLK_DOWN, SDLK_UP, SDLK_NUMLOCK, SDLK_KP_DIVIDE, SDLK_KP_MULTIPLY, SDLK_KP_MINUS, SDLK_KP_PLUS, 
	/*58*/ SDLK_KP_ENTER, SDLK_KP1, SDLK_KP2, SDLK_KP3, SDLK_KP4, SDLK_KP5, SDLK_KP6, SDLK_KP7,
	/*60*/ SDLK_KP8, SDLK_KP9, SDLK_KP0, SDLK_KP_PERIOD, SDLK_BACKSLASH, SDLK_RMETA/* S3 */
};

const static unsigned short sdl_shift[] = {
	SDLK_LCTRL,SDLK_LSHIFT,SDLK_LALT,SDLK_LSUPER /* S1 */,
	SDLK_RCTRL,SDLK_RSHIFT,SDLK_RALT,SDLK_RSUPER /* S2 */,
};

#define	MOUSE_BACKWARD 	(1<<4) 	// TODO for usb4maple, need fix KOS
#define	MOUSE_FORWARD	(1<<5) 	// TODO for usb4maple, need fix KOS
#define	MOUSE_WHEELUP 	(1<<6) 		// TODO (1<<6)
#define	MOUSE_WHEELDOWN	(1<<7) 		// TODO (1<<7)

static void mouse_update(void) {
const static char sdl_mousebtn[] = {
	MOUSE_LEFTBUTTON,
	MOUSE_RIGHTBUTTON,
	MOUSE_SIDEBUTTON,
	MOUSE_BACKWARD, 	// TODO for usb4maple, need fix KOS
	MOUSE_FORWARD, 		// TODO for usb4maple, need fix KOS
	MOUSE_WHEELUP,
	MOUSE_WHEELDOWN
};

	mouse_state_t * cond = NULL;
	maple_device_t *mouse = NULL;
	static uint32_t prev_buttons;
	uint32_t buttons, changed;
	int i, dx, dy, dz;
	
	if(!(mouse = maple_enum_type(0, MAPLE_FUNC_MOUSE))) {
		return;
	}
	
	if (!(cond = (mouse_state_t *)maple_dev_status(mouse))) {
		return;
	}
	
	buttons = cond->buttons;
	
	if (cond->dz<0) buttons |= MOUSE_WHEELUP;
	if (cond->dz>0) buttons |= MOUSE_WHEELDOWN;

	dx = cond->dx;
	dy = cond->dy;
	dz = cond->dz;
	
	if (dx || dy || dz) {
		SDL_PrivateMouseMotion(0, 1, dx, dy, dz);
	}

	changed = buttons ^ prev_buttons;
	
	for(i = 0; i<sizeof(sdl_mousebtn); i++) {
		if (changed & sdl_mousebtn[i]) {
			//Do not flip state.
			SDL_PrivateMouseButton((buttons & sdl_mousebtn[i]) ? SDL_PRESSED : SDL_RELEASED, i, 0, 0);
		}
	}
	
	prev_buttons = buttons;
}

static void keyboard_update(void) {
	kbd_state_t	*state;
	maple_device_t *kbd;
	static SDL_keysym keysym;
	static uint8_t last_modifiers = 0;
	static key_state_t key_states[sizeof(sdl_key)] = {0};
	int i;
	
	if (!(kbd = maple_enum_type(0, MAPLE_FUNC_KEYBOARD))) {
		return;
	}

	if (!(state = (kbd_state_t *) kbd_get_state(kbd))) {
		return;
	}
	
	kbd_region_t region = state->region;
	kbd_mods_t mods = state->cond.modifiers;
	kbd_leds_t leds;
	leds.raw = 0;
	
	for(i=0; i<sizeof(sdl_shift); i++) {
		if ((state->cond.modifiers.raw & (1 << i)) != (last_modifiers & (1 << i))) {
			keysym.sym = sdl_shift[i];
			keysym.unicode = 0;
			SDL_PrivateKeyboard((state->cond.modifiers.raw & (1 << i)) ? SDL_PRESSED : SDL_RELEASED, &keysym);
		}
	}
	
	last_modifiers = state->cond.modifiers.raw;
	
	for(i = 0; i < sizeof(sdl_key); i++) {
		if (state->key_states[i].is_down != key_states[i].is_down) {
			int key = sdl_key[i];
			if (key) {
				keysym.sym = key;
				keysym.unicode = kbd_key_to_ascii(i, region, mods, leds);
				
				SDL_PrivateKeyboard(state->key_states[i].is_down ? SDL_PRESSED : SDL_RELEASED, &keysym);
			}
		}
		
		key_states[i].raw = state->key_states[i].raw;
	}
}

static __inline__ Uint32 myGetTicks(void) {
	return ((timer_us_gettime64() >> 10));
}

void DC_PumpEvents(_THIS) {
	static Uint32 last_time = 0;
	Uint32 now = myGetTicks();
	
	if ((now - last_time) > MIN_FRAME_UPDATE) {
		keyboard_update();
		mouse_update();
		last_time = now;
	}
}

void DC_InitOSKeymap(_THIS) {
	/* do nothing. */
}

/* end of SDL_dcevents.c ... */

