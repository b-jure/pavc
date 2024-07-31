/* Copyright (C) 2024 Jure Bagić
 *
 * This file is part of pavc.
 * pavc is free software: you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * pavc is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with pavc.
 * If not, see <https://www.gnu.org/licenses/>. */


#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "pcommon.h"
#include "pstate.h"



static void sinkinfocb(pa_context* c, const pa_sink_info* si, int eol, void* ud)
{
	pavc_State *pavc;

	UNUSED(c);
	UNUSED(eol);
	pavc = (pavc_State*)ud;
	if (si) pavc_state_addsinkinfo(pavc, si);
	pavc_state_signalthreadedml(pavc, 0);
}


static void statechangecb(pa_context* ctx, void* ud)
{
	pavc_State *pavc;

	UNUSED(ctx);
	pavc = (pavc_State*)ud;
	pavc_state_signalthreadedml(pavc, 0);
}


static void ctxsuccesscb(pa_context *ctx, int success, void* ud)
{
	pavc_State *pavc;

	UNUSED(ctx);
	UNUSED(success);
	pavc = (pavc_State*)ud;
	pavc_state_signalthreadedml(pavc, 0);
}


static void initeventloop(pavc_State *pavc)
{
	pavc_state_newthreadedml(pavc);
	pavc_state_getthreadedmlapi(pavc);
	pavc_state_newcontext(pavc, "pavc");
	pavc_state_startthreadedml(pavc);
}


static void paconnect(pavc_State *pavc)
{
	pavc_state_connect(pavc, statechangecb, pavc, NULL, PA_CONTEXT_NOFLAGS, NULL);
	pavc_state_waitctxstate(pavc, PA_CONTEXT_READY);
}


static void getsilist(pavc_State *pavc)
{
	const char *err;

	pavc_state_getsinkinfolist(pavc, sinkinfocb, pavc);
	if (pavc_state_haveop(pavc)) {
		pavc_state_waitopstate(pavc, PA_OPERATION_DONE);
		if ((err = pavc_state_checkerror(pavc)))
			pavc_state_error(pavc, err);
		pavc_state_removeop(pavc);
	} else {
		pavc_state_error(pavc, "couldn't retrieve sink list");
	}
}


static void *pavc_alloc(void *ptr, void *ud, size_t osize, size_t size)
{
	UNUSED(osize);
	UNUSED(ud);
	if (size == 0) {
		free(ptr);
		return NULL;
	}
	return realloc(ptr, size);
}


static p_noret usagePavc(pavc_State *pavc)
{
	fputs(
	"\nSynopsis:\n"
	"pavc [command    [value]    [sink device name]]\n"
	"      toggle     N/A\n"
	"      up         0..100 (%)\n"
	"      down       0..100 (%)\n"
	"      volume     percent | decibel\n"
	"\nExamples:\n"
	" - pavc toggle (toggles mute on all sink devices)\n"
	" - pavc up 5 (increases volume by 5% on all sink devices)\n"
	" - pavc down 10 (decreases volume by 10%, on all sink devices)\n"
	" - pavc volume percent (returns the current volume level of all devices as percentage)\n"
	" - pavc volume decibel (returns the current volume level of all devices in decibels)\n\n",
	stderr);
	pavc_state_error(pavc, "usage error"); /* this flushes stderr */
}


static void changevolume(pavc_State *pavc, const pa_sink_info *si, pa_cvolume *cvnew)
{
	const char *err;

	pavc_state_setsinkvolumeindex(pavc, si, cvnew, ctxsuccesscb, pavc);
	if (pavc_state_haveop(pavc)) {
		pavc_state_waitopstate(pavc, PA_OPERATION_DONE);
		if ((err = pavc_state_checkerror(pavc)))
			pavc_state_error(pavc, err);
		pavc_state_removeop(pavc);
	} else {
		pavc_state_error(pavc, "failed setting sink volume");
	}
}



/* -------------------------------------------------------------------------
 * Commands
 * ------------------------------------------------------------------------- */


typedef void (*Cmdfunction)(pavc_State *pavc, const pa_sink_info *si, void *ud);


typedef struct PavcCmd {
	Cmdfunction fn;
	union {
		unsigned int n;
		const char *str;
	} val;
	const char *sinkname;
} PavcCmd;


#define scaleVOL(n)	(PA_VOLUME_NORM * ((n) / 100.0))


static void cmddown(pavc_State *pavc, const pa_sink_info *si, void *ud)
{
	pa_cvolume cvnew;
	pa_volume_t dec;

	cvnew = si->volume;
	dec = scaleVOL(*(unsigned int *)ud);
	if(pa_cvolume_dec(&cvnew, dec))
		changevolume(pavc, si, &cvnew);
	else
		pavc_state_error(pavc, "failed decrementing volume");
}


static void cmdup(pavc_State *pavc, const pa_sink_info *si, void *ud)
{
        pa_cvolume cvnew;
        pa_volume_t inc;

	cvnew = si->volume;
	inc = scaleVOL(*(unsigned int *)ud);
	if(pa_cvolume_inc_clamp(&cvnew, inc, PA_VOLUME_NORM))
		changevolume(pavc, si, &cvnew);
	else
		pavc_state_error(pavc, "failed incrementing volume");
}


static void cmdtoggle(pavc_State *pavc, const pa_sink_info *si, void *ud)
{
	const char *err;

	UNUSED(ud);
	pavc_state_setsinkmuteindex(pavc, si, si->mute^1, ctxsuccesscb, pavc);
	if (pavc_state_haveop(pavc)) {
		pavc_state_waitopstate(pavc, PA_OPERATION_DONE);
		if ((err = pavc_state_checkerror(pavc)))
			pavc_state_error(pavc, err);
		pavc_state_removeop(pavc);
	} else {
		pavc_state_error(pavc, "failed to toggle mute");
	}
}


static void cmdvolume(pavc_State *pavc, const pa_sink_info* si, void *ud)
{
	const char *unit;
	pa_volume_t avg;

	unit = *(const char **)ud;
	avg = pa_cvolume_avg(&si->volume);
	if(!strcmp(unit, "percent")) {
		avg = ((double)avg / (double)PA_VOLUME_NORM) * 100.0;
		printf("%u", (unsigned int)avg);
	} else if(!strcmp(unit, "decibel")) {
		printf("%g", (double)pa_sw_volume_to_dB(avg));
	} else {
		pavc_state_error(pavc, "invalid unit for 'volume' (try decibel or percent)");
	}
}


static int strtovolume(const char *str, unsigned int *vol)
{
        int c;

	*vol = 0;
	if (*str == '0') {
		if (str[1] != '\0')
			return -1;
		return 0;
	}
	while((c = *str++)) {
		if (isdigit(c)) *vol = *vol * 10 + (c - '0');
		else return -1;
	}
	*vol = ((*vol - 1) % 100) + 1; /* clamp */
        return 0;
}


static void parsetoggle(pavc_State *pavc, PavcCmd *cmd, int argc, char **argv)
{
	if (argc > 1)
		pavc_state_error(pavc, "too many arguments provided for 'toggle' command");
	if (argc == 1)
		cmd->sinkname = *argv;
	cmd->fn = &cmdtoggle;
}


static void parseupdown(pavc_State *pavc, PavcCmd *cmd, int argc, char **argv)
{
	if (argc <= 0)
		pavc_state_error(pavc, "up/down command is missing volume value");
	if (argc > 2) /* volume + optional name of sink device */
		pavc_state_error(pavc, "too many arguments provided for 'up/down' command");
        if (strtovolume(*argv, &cmd->val.n) < 0)
		pavc_state_error(pavc, "invalid volume value");
	if (argc == 2)
		cmd->sinkname = argv[1];
	cmd->fn = (*argv[-1] == 'u' ? &cmdup : &cmddown);
}


static void parsevolume(pavc_State *pavc, PavcCmd *cmd, int argc, char **argv)
{
	if (argc > 2)
		pavc_state_error(pavc, "too many arguments provided for 'volume' command");
	if (argc == 0)
		pavc_state_error(pavc, "missing unit specifier for 'volume' command");
	cmd->val.str = *argv;
	if (argc == 2)
		cmd->sinkname = argv[1];
	cmd->fn = &cmdvolume;
}


static void parseargs(pavc_State *pavc, PavcCmd *cmd, int argc, char** argv)
{
        const char* argcmd;

        if (argc <= 1) usagePavc(pavc);
	argcmd = argv[1]; /* skip command */
	argv += 2;
	argc -= 2;
	if (!strcmp(argcmd, "toggle")) {
		parsetoggle(pavc, cmd, argc, argv);
	} else if (!strcmp(argcmd, "up") || !strcmp(argcmd, "down")) {
		parseupdown(pavc, cmd, argc, argv);
	} else if (!strcmp(argcmd, "volume")) {
		parsevolume(pavc, cmd, argc, argv);
	} else {
		pavc_state_error(pavc, "invalid command");
	}
}


static void runthecommand(pavc_State *pavc, PavcCmd *cmd)
{
	unsigned int nsi;
	unsigned int i;
	const pa_sink_info *si;
	const char *err;

	if (cmd->sinkname) { /* only for specific sink device ? */
		pavc_state_getsinkinfoname(pavc, cmd->sinkname, sinkinfocb, pavc);
		if (pavc_state_haveop(pavc)) {
			pavc_state_waitopstate(pavc, PA_OPERATION_DONE);
			if ((err = pavc_state_checkerror(pavc)))
				pavc_state_error(pavc, err);
			pavc_state_removeop(pavc);
			(*cmd->fn)(pavc, pavc_state_getlastsinkinfo(pavc), &cmd->val);
		} else {
			pavc_state_error(pavc, "failed to retrieve sink information");
		}
	} else { /* run on all sink devices */
		getsilist(pavc);
		nsi = pavc_state_getsinkcount(pavc);
		for (i = 0; i < nsi; i++) {
			si = pavc_state_getsinkinfo(pavc, i);
			(*cmd->fn)(pavc, si, &cmd->val);
		}
	}
}


static void newstate(pavc_State **pavcp)
{
	if (p_unlikely((*pavcp = pavc_state_new(pavc_alloc, NULL)) == NULL)) {
		fputs("pavc: state allocation failed.\n", stderr);
		exit(EXIT_FAILURE);
	}
}


int main(int argc, char** argv) 
{
	pavc_State *pavc;
	PavcCmd cmd = { 0 };

	newstate(&pavc);
	parseargs(pavc, &cmd, argc, argv);
	initeventloop(pavc);
	pavc_state_lockthreadedml(pavc); /* get a lock */
	paconnect(pavc);
	runthecommand(pavc, &cmd);
	pavc_state_delete(pavc);
	return 0;
}
