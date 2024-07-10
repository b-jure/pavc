#ifndef PAVCSTATE_H
#define PAVCSTATE_H


#include "pcommon.h"


/* state change callback */
typedef void (*pavc_Statechangecb)(pa_context *, void *);

/* context success callback */
typedef void (*pavc_Ctxsuccesscb)(pa_context *, int, void*);

/* sink info callback */
typedef void (*pavc_Sinkinfocb)(pa_context *, const pa_sink_info *, int, void *);


struct pavc_State {
	pavc_Allocfunction alloc; /* allocator */
	void *ud; /* userdata for 'alloc' */
        pa_threaded_mainloop* ml;
        pa_mainloop_api* mlapi;
        pa_operation* op;
        pa_context* ctx;
        const pa_sink_info** si;
        unsigned int nsi; /* number of elements in 'si' */
        unsigned int sizesi; /* size of 'si' */
        unsigned char running; /* true if mainloopo is running */
};



/* create/destroy state */
pavc_State *pavc_state_new(pavc_Allocfunction fn, void *ud);
void pavc_state_delete(pavc_State *pavc);


/* threaded event loop */
void pavc_state_newthreadedml(pavc_State *pavc);
void pavc_state_startthreadedml(pavc_State *pavc);
void pavc_state_lockthreadedml(pavc_State *pavc);
void pavc_state_unlockthreadedml(pavc_State *pavc);
void pavc_state_signalthreadedml(pavc_State *pavc, int sig);
void pavc_state_getthreadedmlapi(pavc_State *pavc);
void pavc_state_newcontext(pavc_State *pavc, const char *name);


/* (event loop) wait on states */
void pavc_state_waitctxstate(pavc_State *pavc, pa_context_state_t state);
void pavc_state_waitopstate(pavc_State *pavc, pa_operation_state_t state);


/* check/unref current operation (no PulseAudio operations) */
int pavc_state_haveop(pavc_State *pavc);
void pavc_state_removeop(pavc_State *pavc);


/* operates on pavc_State sink array (no PulseAudio operations) */
void pavc_state_addsinkinfo(pavc_State *pavc, const pa_sink_info *si);
const pa_sink_info *pavc_state_getsinkinfo(pavc_State *pavc, unsigned int i);
const pa_sink_info *pavc_state_getlastsinkinfo(pavc_State *pavc);
const pa_sink_info *pavc_state_removelastsinkinfo(pavc_State *pavc);
const pa_sink_info *pavc_state_removesinkinfoindex(pavc_State *pavc, unsigned int i);
unsigned int pavc_state_getsinkcount(pavc_State *pavc);

/* fill pavc_State sink array (performs PulseAudio operation) */
void pavc_state_getsinkinfoname(pavc_State *pavc, const char *name, pavc_Sinkinfocb cb, void *ud);
void pavc_state_getsinkinfolist(pavc_State *pavc, pavc_Sinkinfocb cb, void *ud);
void pavc_state_setsinkvolumeindex(pavc_State *pavc, const pa_sink_info *si, pa_cvolume *cvnew, pavc_Ctxsuccesscb cb, void *ud);
void pavc_state_setsinkmuteindex(pavc_State *pavc, const pa_sink_info *si, int mute, pavc_Ctxsuccesscb cb, void *ud);

/* retrieve latest operation error */
const char *pavc_state_getoperrormsg(pavc_State *pavc);

/* connect to PulseAudio server */
void pavc_state_connect(pavc_State *pavc, pavc_Statechangecb cb, void *ud, const char *server, pa_context_flags_t flags, const pa_spawn_api *api);


/* throw error */
p_noret pavc_state_error(pavc_State *pavc, const char *err);
const char *pavc_state_checkerror(pavc_State *pavc);

#endif
