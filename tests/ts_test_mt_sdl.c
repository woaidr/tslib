/*
 * (C) 2017 Martin Keppligner <martink@posteo.de>
 *
 * This file is part of tslib.
 *
 * ts_calibrate is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * ts_calibrate is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this tool.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <stdint.h>

#if defined (__FreeBSD__)

#include <dev/evdev/input.h>
#define TS_HAVE_EVDEV

#elif defined (__linux__)

#include <linux/input.h>
#define TS_HAVE_EVDEV

#endif

#ifdef TS_HAVE_EVDEV
#include <sys/ioctl.h>
#endif

#include <tslib.h>

const int BLOCK_SIZE = 9;

static void help(void)
{
	struct ts_lib_version_data *ver = ts_libversion();

	printf("tslib %s (library 0x%X)\n", ver->package_version, ver->version_num);
	printf("\n");
	printf("Usage: ts_test_mt [-v] [-i <device>] [-j <slots>] [-r <rotate_value>]\n");
	printf("\n");
	printf("        <device>       Override the input device to use\n");
	printf("        <slots>        Override the number of possible touch contacts\n");
	printf("                       Automatically detected only on Linux, but not\n");
	printf("                       for all devices\n");
	printf("        <rotate_value> 0 ... no rotation; 0 degree (default)\n");
	printf("                       1 ... clockwise orientation; 90 degrees\n");
	printf("                       2 ... upside down orientation; 180 degrees\n");
	printf("                       3 ... counterclockwise orientation; 270 degrees\n");
	printf("\n");
	printf("Example (Linux): ts_test_mt -r $(cat /sys/class/graphics/fbcon/rotate)\n");
	printf("\n");
}

static void draw_line(SDL_Renderer *r, int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
	int32_t tmp;
	int32_t dx = x2 - x1;
	int32_t dy = y2 - y1;

	if (abs(dx) < abs(dy)) {
		if (y1 > y2) {
			tmp = x1;
			x1 = x2;
			x2 = tmp;

			tmp = y1;
			y1 = y2;
			y2 = tmp;

			dx = -dx;
			dy = -dy;
		}

		x1 <<= 16;

		dx = (dx << 16) / dy;
		while (y1 <= y2) {
			SDL_RenderDrawPoint(r, x1 >> 16, y1);
			y1 += dx;
			y1++;
		}
	} else {
		if (x1 > x2) {
			tmp = x1;
			x1 = x2;
			x2 = tmp;

			tmp = y1;
			y1 = y2;
			y2 = tmp;

			dx = -dx;
			dy = -dy;
		}

		y1 <<= 16;

		dy = dx ? (dy << 16) : 0;
		while (x1 <= x2) {
			SDL_RenderDrawPoint(r, x1, y1 >> 16);
			y1 += dy;
			x1++;
		}
	}
}

static void draw_crosshair(SDL_Renderer *r, int32_t x, int32_t y)
{
	SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
	draw_line(r, x - 10, y, x - 2, y);
	draw_line(r, x + 2, y, x + 10, y);
	draw_line(r, x, y - 10, x, y - 2);
	draw_line(r, x, y + 2, x, y + 10);

	SDL_SetRenderDrawColor(r, 0xff, 0xe0, 0x80, 255);
	draw_line(r, x - 6, y - 9, x - 9, y - 9);
	draw_line(r, x - 9, y - 8, x - 9, y - 6);
	draw_line(r, x - 9, y + 6, x - 9, y + 9);
	draw_line(r, x - 8, y + 9, x - 6, y + 9);
	draw_line(r, x + 6, y + 9, x + 9, y + 9);
	draw_line(r, x + 9, y + 8, x + 9, y + 6);
	draw_line(r, x + 9, y - 6, x + 9, y - 9);
	draw_line(r, x + 8, y - 9, x + 6, y - 9);
}

int main(int argc, char **argv)
{
	struct tsdev *ts;
#ifdef TS_HAVE_EVDEV
	struct input_absinfo slot;
#endif
	int32_t user_slots = 0;
	int32_t max_slots = 1;
	const char *tsdevice = NULL;
	struct ts_sample_mt **samp_mt = NULL;
	short verbose = 0;
	int ret;
	int i;
	SDL_Window *sdlWindow;
	SDL_Renderer *sdlRenderer;
	SDL_Event ev;
	SDL_Rect r;

	while (1) {
		const struct option long_options[] = {
			{ "help",         no_argument,       NULL, 'h' },
			{ "verbose",      no_argument,       NULL, 'v' },
			{ "idev",         required_argument, NULL, 'i' },
			{ "slots",        required_argument, NULL, 'j' },
			{ "rotate",       required_argument, NULL, 'r' },
		};

		int option_index = 0;
		int c = getopt_long(argc, argv, "hi:vj:r:", long_options, &option_index);

		errno = 0;
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			help();
			return 0;

		case 'v':
			verbose = 1;
			break;

		case 'i':
			tsdevice = optarg;
			break;

		case 'j':
			user_slots = atoi(optarg);
			if (user_slots <= 0) {
				help();
				return 0;
			}
			break;

		case 'r':
			/* TODO */
			help();
			return 0;

			break;

		default:
			help();
			return 0;
		}

		if (errno) {
			char *str = "option ?";
			str[7] = c & 0xff;
			perror(str);
		}
	}

	ts = ts_setup(tsdevice, 0);
	if (!ts) {
		perror("ts_setup");
		return errno;
	}

#ifdef TS_HAVE_EVDEV
	if (ioctl(ts_fd(ts), EVIOCGABS(ABS_MT_SLOT), &slot) < 0) {
		perror("ioctl EVIOGABS");
		ts_close(ts);
		return errno;
	}

	max_slots = slot.maximum + 1 - slot.minimum;
#endif
	if (user_slots > 0)
		max_slots = user_slots;

	samp_mt = malloc(sizeof(struct ts_sample_mt *));
	if (!samp_mt) {
		ts_close(ts);
		return -ENOMEM;
	}
	samp_mt[0] = calloc(max_slots, sizeof(struct ts_sample_mt));
	if (!samp_mt[0]) {
		free(samp_mt);
		ts_close(ts);
		return -ENOMEM;
	}

	r.w = r.h = BLOCK_SIZE;

	SDL_SetMainReady();

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			     "Couldn't initialize SDL: %s", SDL_GetError());
		goto out;
	}

	if (SDL_CreateWindowAndRenderer(0, 0,
					SDL_WINDOW_FULLSCREEN_DESKTOP,
					&sdlWindow, &sdlRenderer)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			     "Couldn't create window and renderer: %s",
			     SDL_GetError());
		goto out;
	}

	SDL_ShowCursor(SDL_DISABLE);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

	SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
	SDL_RenderClear(sdlRenderer);

	while (1) {
		ret = ts_read_mt(ts, samp_mt, max_slots, 1);
		if (ret < 0) {
			SDL_Quit();
			goto out;
		}

		if (ret != 1)
			continue;

		SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
		SDL_RenderClear(sdlRenderer);

		for (i = 0; i < max_slots; i++) {
			if (samp_mt[0][i].valid != 1)
				continue;

			draw_crosshair(sdlRenderer,
				       samp_mt[0][i].x, samp_mt[0][i].y);

			if (verbose) {
				printf("%ld.%06ld: (slot %d) %6d %6d %6d\n",
					samp_mt[0][i].tv.tv_sec,
					samp_mt[0][i].tv.tv_usec,
					samp_mt[0][i].slot,
					samp_mt[0][i].x,
					samp_mt[0][i].y,
					samp_mt[0][i].pressure);
                        }
		}

		SDL_PollEvent(&ev);
		switch (ev.type) {
			case SDL_KEYDOWN:
			case SDL_QUIT:
				SDL_ShowCursor(SDL_ENABLE);
				SDL_DestroyWindow(sdlWindow);
				SDL_Quit();
				goto out;
		}

		SDL_RenderPresent(sdlRenderer);
	}
out:
	if (ts)
		ts_close(ts);

	if (samp_mt) {
		if (samp_mt[0])
			free(samp_mt[0]);

		free(samp_mt);
	}
	return 0;
}