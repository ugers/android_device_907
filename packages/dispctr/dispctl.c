/*-
 * Copyright (c) 2012 FUKAUMI Naoki.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "dispctl.h"

char *
fbmode2str(int mode)
{

	switch (mode) {
	case FB_MODE_SCREEN0:
		return "SCREEN0";
	case FB_MODE_SCREEN1:
		return "SCREEN1";
	case FB_MODE_TWO_SAME_SCREEN_TB:
		return "TWO_SAME_SCREEN_TB";
	case FB_MODE_TWO_DIFF_SCREEN_SAME_CONTENTS:
		return "TWO_DIFF_SCREEN_SAME_CONTENTS";
	default:
		return NULL;
	}
}

char *
type2str(int type)
{

	switch (type) {
	case OUTPUT_TYPE_NONE:
		return "NONE";
	case OUTPUT_TYPE_LCD:
		return "LCD";
	case OUTPUT_TYPE_TV:
		return "TV";
	case OUTPUT_TYPE_HDMI:
		return "HDMI";
	case OUTPUT_TYPE_VGA:
		return "VGA";
	default:
		return NULL;
	}
}

char *
tvmode2str(int mode)
{

	switch (mode) {
	case TV_MODE_480I:
		return "480i";
	case TV_MODE_576I:
		return "576i";
	case TV_MODE_480P:
		return "480p";
	case TV_MODE_576P:
		return "576p";
	case TV_MODE_720P_50HZ:
		return "720p 50Hz";
	case TV_MODE_720P_60HZ:
		return "720p 60Hz";
	case TV_MODE_1080I_50HZ:
		return "1080i 50Hz";
	case TV_MODE_1080I_60HZ:
		return "1080i 60Hz";
	case TV_MODE_1080P_24HZ:
		return "1080p 24Hz";
	case TV_MODE_1080P_50HZ:
		return "1080p 50Hz";
	case TV_MODE_1080P_60HZ:
		return "1080p 60Hz";
	default:
		return NULL;
	}
}

int
main(int argc, char *argv[])
{
	struct fb_para para;
	unsigned long cmd[4];
	int fd, retry = 10, tvmode, type;

	if ((fd = open("/dev/disp", O_RDWR)) == -1)
		err(EXIT_FAILURE, "/dev/disp");

	cmd[0] = 0;
	cmd[1] = (intptr_t)&para;
	if (ioctl(fd, CMD_FB | FB_GET_PARA, cmd) == -1) {
		(void)close(fd);
		err(EXIT_FAILURE, "FB_GET_PARA");
	}

	printf("Current fb mode: %s\n", fbmode2str(para.fb_mode));

	switch (para.fb_mode) {
	case FB_MODE_SCREEN0:
		cmd[0] = 0;
		break;
	case FB_MODE_SCREEN1:
		cmd[0] = 1;
		break;
	case FB_MODE_TWO_SAME_SCREEN_TB:
		cmd[0] = 2;
		break;
	case FB_MODE_TWO_DIFF_SCREEN_SAME_CONTENTS:
	default:
		(void)close(fd);
		errx(EXIT_FAILURE, "%s: unsupported fb mode",
		    fbmode2str(para.fb_mode));
	}

	printf("Detecting HDMI");
	while (ioctl(fd, CMD_HDMI | HDMI_GET_HPD_STATUS, cmd) != 2) {
		if (retry-- > 0) {
			printf(".");
			sleep(1);
		} else {
			printf(" not detected\n");
			(void)close(fd);
			return EXIT_SUCCESS;
		}
	}
	printf("\n");

	for (tvmode = TV_MODE_720P_50HZ; tvmode <= TV_MODE_1080P_60HZ; tvmode++) {
		cmd[1] = tvmode;
		if (ioctl(fd, CMD_HDMI | HDMI_SUPPORT_MODE, cmd) == 2)
			break;
	}

	if (tvmode > TV_MODE_1080P_60HZ) {
		(void)close(fd);
		errx(EXIT_FAILURE, "no tvmode is available");
	}

	if ((type = ioctl(fd, CMD_GET_OUTPUT_TYPE, cmd)) == -1) {
		(void)close(fd);
		err(EXIT_FAILURE, "GET_OUTPUT_TYPE");
	}

	printf("Current output type: %s\n", type2str(type));
	printf("Set output type HDMI, tvmode %s\n", tvmode2str(tvmode));

	switch (type) {
	case OUTPUT_TYPE_NONE:
		break;
	case OUTPUT_TYPE_LCD:
		if (ioctl(fd, CMD_LCD | LCD_OFF, cmd) == -1) {
			(void)close(fd);
			err(EXIT_FAILURE, "LCD_OFF");
		}
		break;
	case OUTPUT_TYPE_TV:
		if (ioctl(fd, CMD_TV | TV_OFF, cmd) == -1) {
			(void)close(fd);
			err(EXIT_FAILURE, "TV_OFF");
		}
		break;
	case OUTPUT_TYPE_HDMI:
		if (ioctl(fd, CMD_HDMI | HDMI_OFF, cmd) == -1) {
			(void)close(fd);
			err(EXIT_FAILURE, "HDMI_OFF");
		}
		break;
	case OUTPUT_TYPE_VGA:
		if (ioctl(fd, CMD_VGA | VGA_OFF, cmd) == -1) {
			(void)close(fd);
			err(EXIT_FAILURE, "VGA_OFF");
		}
		break;
	default:
		(void)close(fd);
		errx(EXIT_FAILURE, "%s: unsupported output type",
		    type2str(type));
	}

	cmd[1] = tvmode;
	if (ioctl(fd, CMD_HDMI | HDMI_SET_MODE, cmd) == -1) {
		(void)close(fd);
		err(EXIT_FAILURE, "HDMI_SET_MODE");
	}

	if (ioctl(fd, CMD_HDMI | HDMI_ON, cmd) == -1) {
		(void)close(fd);
		err(EXIT_FAILURE, "HDMI_ON");
	}

	(void)close(fd);

	return EXIT_SUCCESS;
}
