#include <pulse/introspect.h>
#include <stdio.h>
#include <string.h>

#include "pstate.h"
#include "pmem.h"


#define STATESIZE	sizeof(pavc_State)


pavc_State *pavc_state_new(pavc_Allocfunction fn, void *ud)
{
	pavc_State *pavc;

	pavc = (pavc_State*)(*fn)(NULL, ud, 0, STATESIZE);
	if (pavc == NULL) return NULL;
	pavc->alloc = fn;
	pavc->ud = ud;
	pavc->ml = NULL;
	pavc->mlapi = NULL;
	pavc->op = NULL;
	pavc->ctx = NULL;
	pavc->si = NULL;
	pavc->nsi = 0;
	pavc->sizesi = 0;
	pavc->running = 0;
	return pavc;
}


void pavc_state_delete(pavc_State *pavc)
{
        if (pavc->ml) {
                if (pavc->ctx) {
                        if(pa_context_get_state(pavc->ctx) == PA_CONTEXT_READY)
                                pa_context_disconnect(pavc->ctx);
                        pa_context_unref(pavc->ctx);
                } 
                if (pavc->op)
                        pa_operation_unref(pavc->op);
                if (pavc->running) {
			pa_threaded_mainloop_unlock(pavc->ml);
                        pa_threaded_mainloop_stop(pavc->ml);
                }
                pa_threaded_mainloop_free(pavc->ml);
        }
        if (pavc->si)
                pavc_mem_freearray(pavc, pavc->si, pavc->sizesi);
	pavc->alloc(pavc, pavc->ud, STATESIZE, 0);
}


static inline void printerror(const char *err)
{
	if (!err) err = "unspecified runtime error";
	fprintf(stderr, "pavc: %s.\n", err);
	fflush(stderr);
}


p_noret pavc_state_error(pavc_State *pavc, const char* err) 
{
	printerror(err);
	pavc_state_delete(pavc);
	exit(EXIT_FAILURE);
}


const char *pavc_state_checkerror(pavc_State *pavc)
{
	int errcode;

	pavc_assert(pavc->ctx);
	errcode = pa_context_errno(pavc->ctx);
	if (errcode != PA_OK)
		return pa_strerror(errcode);
	return NULL;
}


void pavc_state_newthreadedml(pavc_State *pavc)
{
	if ((pavc->ml = pa_threaded_mainloop_new()) == NULL)
		pavc_state_error(pavc, "couldn't create threaded mainloop object");
}


void pavc_state_getthreadedmlapi(pavc_State *pavc)
{
	pavc_assert(pavc->ml);
	if ((pavc->mlapi = pa_threaded_mainloop_get_api(pavc->ml)) == NULL)
		pavc_state_error(pavc, "couldn't retrieve threaded mainloop vtable");
}


void pavc_state_newcontext(pavc_State *pavc, const char *name)
{
	pavc_assert(pavc->mlapi);
	if ((pavc->ctx = pa_context_new(pavc->mlapi, name)) == NULL)
		pavc_state_error(pavc, "couldn't instantiate connection context");
}


void pavc_state_startthreadedml(pavc_State *pavc)
{
	pavc_assert(pavc->ml);
	if (pa_threaded_mainloop_start(pavc->ml) < 0)
		pavc_state_error(pavc, "couldn't start event loop thread");
	pavc->running = 1;
}


void pavc_state_lockthreadedml(pavc_State *pavc)
{
	pavc_assert(pavc->ml);
        pa_threaded_mainloop_lock(pavc->ml);
}


void pavc_state_unlockthreadedml(pavc_State *pavc)
{
	pavc_assert(pavc->ml);
	pa_threaded_mainloop_unlock(pavc->ml);
}


void pavc_state_waitctxstate(pavc_State *pavc, pa_context_state_t state)
{
        pa_context_state_t currstate;

	pavc_assert(pavc->ml);
	pavc_assert(pavc->ctx);
        while((currstate = pa_context_get_state(pavc->ctx)) != state) {
		if (currstate != PA_CONTEXT_FAILED)
			pa_threaded_mainloop_wait(pavc->ml);
		else
			pavc_state_error(pavc, "connection failed or was disconnected");
        }
}


void pavc_state_waitopstate(pavc_State *pavc, pa_operation_state_t state)
{
	pa_operation_state_t currstate;

	pavc_assert(pavc->ml);
	pavc_assert(pavc->op);
	while ((currstate = pa_operation_get_state(pavc->op)) != state) {
		if (currstate != PA_OPERATION_CANCELLED)
			pa_threaded_mainloop_wait(pavc->ml);
		else
			pavc_state_error(pavc, "operation failed");
	}
}


void pavc_state_connect(pavc_State *pavc, pavc_Statechangecb cb, void *ud, 
			const char *server, pa_context_flags_t flags, const pa_spawn_api *api)
{
        pa_context_set_state_callback(pavc->ctx, cb, ud);
        if (pa_context_connect(pavc->ctx, server, flags, api) < 0)
                pavc_state_error(pavc, "couldn't connect context to the default server");
}


int pavc_state_haveop(pavc_State *pavc)
{
	return (pavc->op != NULL);
}


void pavc_state_removeop(pavc_State *pavc)
{
	pavc_assert(pavc->op);
	pa_operation_unref(pavc->op);
	pavc->op = NULL;
}


void pavc_state_signalthreadedml(pavc_State *pavc, int sig)
{
	pavc_assert(pavc->ml);
	pa_threaded_mainloop_signal(pavc->ml, sig);
}


void pavc_state_addsinkinfo(pavc_State *pavc, const pa_sink_info *si)
{
	pavc_mem_growarray(pavc, pavc->si, &pavc->sizesi, pavc->nsi, UINT_MAX,
				const pa_sink_info*);
	pavc->si[pavc->nsi++] = si;
}


unsigned int pavc_state_getsinkcount(pavc_State *pavc)
{
	return pavc->nsi;
}


const pa_sink_info *pavc_state_getsinkinfo(pavc_State *pavc, unsigned int i)
{
	return (i < pavc->nsi ? pavc->si[i] : NULL);
}


const pa_sink_info *pavc_state_getlastsinkinfo(pavc_State *pavc)
{
	return (pavc->nsi > 0 ? pavc->si[pavc->nsi - 1] : NULL);
}


void pavc_state_setsinkvolumeindex(pavc_State *pavc, const pa_sink_info *si, pa_cvolume *cvnew,
					pavc_Ctxsuccesscb cb, void *ud)
{
	pavc_assert(pavc->ctx); /* must be connected */
	pavc->op = pa_context_set_sink_volume_by_index(pavc->ctx, si->index, cvnew, cb, ud);
	pavc_assert(pavc->op != NULL);
}


void pavc_state_setsinkmuteindex(pavc_State *pavc, const pa_sink_info *si, int mute,
					pavc_Ctxsuccesscb cb, void *ud)
{
	pavc_assert(pavc->ctx); /* must be connected */
	pavc->op = pa_context_set_sink_mute_by_index(pavc->ctx, si->index, mute, cb, ud);
}


void pavc_state_getsinkinfolist(pavc_State *pavc, pavc_Sinkinfocb cb, void *ud)
{
	pavc_assert(pavc->ctx); /* must be connected */
	pavc->op = pa_context_get_sink_info_list(pavc->ctx, cb, ud);
	pavc_assert(pavc->op != NULL);
}


void pavc_state_getsinkinfoname(pavc_State *pavc, const char *name, pavc_Sinkinfocb cb, void *ud)
{
	pavc_assert(pavc->ctx); /* must be connected */
	pavc->op = pa_context_get_sink_info_by_name(pavc->ctx, name, cb, ud);
	pavc_assert(pavc->op != NULL);
}


const pa_sink_info *pavc_state_removelastsinkinfo(pavc_State *pavc)
{
	return (pavc->nsi > 0 ? pavc->si[--pavc->nsi] : NULL);
}


const pa_sink_info *pavc_state_removesinkinfoindex(pavc_State *pavc, unsigned int i)
{
	const pa_sink_info *si;

	if (i < pavc->nsi) {
		if (i == pavc->nsi - 1)
			return pavc_state_removelastsinkinfo(pavc);
		si = pavc->si[i];
		memmove(&pavc->si[i], &pavc->si[i + 1], pavc->nsi - i - 1);
		pavc->nsi--;
		return si;
	}
	return NULL;
}


const char *pavc_state_getoperrormsg(pavc_State *pavc)
{
	int errcode;

	pavc_assert(pavc->ctx);
	errcode = pa_context_errno(pavc->ctx);
	return pa_strerror(errcode);
}
