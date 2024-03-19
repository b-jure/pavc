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


#include <pulse/operation.h>
#include <pulse/pulseaudio.h>
#include <stdio.h>
#include <string.h>

#include "util.h"


#define die(err)  \
        do { \
                cleanup(); \
                panic(err); \
        } while (0)

#define pavcdealloc(ptr) pavcalloc(ptr, 0)

#define pavcarray_push(array, len, cap, element) \
        do {\
                if ((len) == (cap)) {\
                        cap = ((cap) == 0 ? 8 : (cap) * 2); \
                        array = pavcalloc(array, cap); \
                } \
                (array)[(len)++] = element; \
        } while (0)

#define PAVC_INIT \
        { 0, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0 }


struct pa_volume_ctrl {
        unsigned int value;
        void (*usercmd)(const pa_sink_info* si);
        pa_threaded_mainloop* ml;
        pa_mainloop_api* mlapi;
        pa_operation* op;
        pa_context* ctx;
        const pa_sink_info** si;
        size_t slen;
        size_t scap;
};


static void cleanup(void);
static void* pavcalloc(void* ptr, size_t bytes);
static void usage(void);
static void sink_info_callback(pa_context* ctx, const pa_sink_info* si, int eol, void* ud);
static void state_change_callback(pa_context* ctx, void* ud);
static void context_success_callback(pa_context *ctx, int success, void* ud);
static void waitstate(pa_context_state_t state);
static void waitop(pa_operation_state_t state);
static void cmd_down(const pa_sink_info* si);
static void cmd_up(const pa_sink_info* si);
static void cmd_toggle(const pa_sink_info* si);
static void initpa(void);
static void connect(void (*state_change_cb)(pa_context *ctx, void *ud));
static void getsilist(void);
static void parse_args(int argc, char* *argv);


static struct pa_volume_ctrl pavc = PAVC_INIT;


static void*
pavcalloc(void* ptr, size_t size)
{
        if (size == 0) {
                free(ptr);
                return NULL;
        }
        if (!(ptr = realloc(ptr, size))) {
                perror(NULL);
                die(NULL);
        }
        return ptr;
}

static void
cleanup()
{
        if (pavc.ml) {
                if (pavc.ctx)
                        pa_context_unref(pavc.ctx);
                if (pavc.op)
                        pa_operation_unref(pavc.op);
                pa_threaded_mainloop_free(pavc.ml);
        }
        if (pavc.si)
                pavcdealloc(pavc.si);
}

static void
usage(void)
{
        die("usage: pavc [command [value]]\r\n"
            "       command - up | down | toggle\r\n"
            "       value   - 0..100 (%)\r\n");
}

static void
sink_info_callback(pa_context* c, const pa_sink_info* si, int eol, void* ud)
{
        unused(c);
        unused(ud);
        if (!eol)
                pavcarray_push(pavc.si, pavc.slen, pavc.scap, si);
        pa_threaded_mainloop_signal(pavc.ml, 0); // wake up
}

static void
state_change_callback(pa_context* ctx, void* ud)
{
        unused(ctx);
        unused(ud);
        pa_threaded_mainloop_signal(pavc.ml, 0); // wake up
}

static void
context_success_callback(pa_context *ctx, int success, void* ud)
{
        unused(ctx);
        unused(success);
        unused(ud);
        pa_threaded_mainloop_signal(pavc.ml, 0); // wake up
}

static void
waitstate(pa_context_state_t state)
{
        pa_context_state_t cstate;
        while((cstate = pa_context_get_state(pavc.ctx)) != state) {
                if (cstate == PA_CONTEXT_FAILED)
                        die("pavc: connection to PulseAudio server failed.\r\n"); 
                pa_threaded_mainloop_wait(pavc.ml);
        }
}

static void
waitop(pa_operation_state_t state)
{
        pa_operation_state_t ostate;

        while((ostate = pa_operation_get_state(pavc.op)) != state) {
                if (ostate == PA_OPERATION_CANCELLED)
                        die("pavc: operation failed.\r\n");
                pa_threaded_mainloop_wait(pavc.ml);
        }
}

static void
change_volume(const pa_sink_info *si, pa_cvolume *cvnew)
{
        pavc.op = pa_context_set_sink_volume_by_index(pavc.ctx, si->index,
                        cvnew, context_success_callback, NULL);
        if(pavc.op == NULL)
                die("pavc: pa_context_set_sink_volume_by_index() failed.\r\n");
        waitop(PA_OPERATION_DONE);
        pa_operation_unref(pavc.op);
        pavc.op = NULL;
}

static void
cmd_down(const pa_sink_info *si)
{
        pa_cvolume cvnew = si->volume;
        pa_volume_t dec = (PA_VOLUME_NORM * (pavc.value / 100.0));

        if(!pa_cvolume_dec(&cvnew, dec))
                die("pavc: pa_cvolume_dec() failed.\r\n");
        change_volume(si, &cvnew);
}

static void
cmd_up(const pa_sink_info *si)
{
        pa_cvolume cvnew = si->volume;
        pa_volume_t inc = (PA_VOLUME_NORM * (pavc.value / 100.0));

        if(!pa_cvolume_inc_clamp(&cvnew, inc, PA_VOLUME_NORM))
                die("pavc: pa_cvolume_inc_clamp() failed.\n\r");
        change_volume(si, &cvnew);
}

static void
cmd_toggle(const pa_sink_info *si)
{
        if (!(pavc.op = pa_context_set_sink_mute_by_index(pavc.ctx, si->index,
                                si->mute ^ 1, context_success_callback, NULL)))
                die("pavc: pa_context_set_sink_mute_by_index() failed.\r\n");
        waitop(PA_OPERATION_DONE);
        pa_operation_unref(pavc.op);
        pavc.op = NULL;
}

static void
initpa(void)
{
        if (!(pavc.ml = pa_threaded_mainloop_new()))
                die("pavc: pa_mainloop_new() failed.\r\n");
        if (!(pavc.mlapi = pa_threaded_mainloop_get_api(pavc.ml)))
                die("pavc: pa_threaded_mainloop_get_api() failed.\r\n");
        if (!(pavc.ctx = pa_context_new(pavc.mlapi, "psvc")))
                die("pavc: pa_context_new() failed.\r\n");
}

static void
connect(void (*state_change_cb)(pa_context*, void*))
{
        pa_context_set_state_callback(pavc.ctx, state_change_cb, NULL);
        if (pa_context_connect(pavc.ctx, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0)
                die("pavc: pa_context_connect()\r\n");
        waitstate(PA_CONTEXT_READY);
}

static void
getsilist(void)
{
        if (!(pavc.op = pa_context_get_sink_info_list(pavc.ctx, sink_info_callback, NULL)))
                die("pavc: pa_context_get_sink_info_list() failed.\r\n");
        waitop(PA_OPERATION_DONE);
        pa_operation_unref(pavc.op);
        pavc.op = NULL;
}

static void
parse_args(int argc, char** argv)
{
        const char* argcmd = argv[1];
        unsigned int value;

        if (!argcmd)
                usage();
        if (!strcmp(argcmd, "toggle")) {
                if (argc > 2)
                        usage();
                pavc.usercmd = cmd_toggle;
                return;
        } else if (argc != 3) {
                usage();
        } else if (!strcmp(argcmd, "up")) {
                pavc.usercmd = cmd_up;
        } else if (!strcmp(argcmd, "down")) {
                pavc.usercmd = cmd_down;
        } else
                usage();
        if (strlen(argv[2]) > 3 || stoui(argv[2], &value))
                usage();
        pavc.value = value;
}

int 
main(int argc, char** argv) 
{
        parse_args(argc, argv);
        initpa();
        pa_threaded_mainloop_start(pavc.ml);
        pa_threaded_mainloop_lock(pavc.ml);
        connect(state_change_callback);
        getsilist();
        for (size_t i = 0; i < pavc.slen; i++)
                pavc.usercmd(pavc.si[i]);
        pa_context_disconnect(pavc.ctx);
        pa_threaded_mainloop_unlock(pavc.ml);
        pa_threaded_mainloop_stop(pavc.ml);
        cleanup();
        return 0;
}
