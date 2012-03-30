/****************************************************************************
 * KONOHA2.0 COPYRIGHT, LICENSE NOTICE, AND DISCRIMER
 *
 * Copyright (c) 2006-2012, Kimio Kuramitsu <kimio at ynu.ac.jp>
 *           (c) 2008-      Konoha Team konohaken@googlegroups.com
 * All rights reserved.
 *
 * You may choose one of the following two licenses when you use konoha.
 * If you want to use the latter license, please contact us.
 *
 * (1) GNU General Public License 3.0 (with K_UNDER_GPL)
 * (2) Konoha Non-Disclosure License 1.0
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#include <konoha2/konoha2.h>
#include "konoha2/gc.h"

#ifdef K_USING_LOGPOOL
#include <logpool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


void MODCODE_init(CTX, kcontext_t *ctx);
void MODEVAL_init(CTX, kcontext_t *ctx);

/* ------------------------------------------------------------------------ */

#define IS_ROOTCTX(o)  (_ctx == (CTX_t)o)

static void klogger_init(CTX, kcontext_t *ctx)
{
#ifdef K_USING_LOGPOOL
	logpool_syslog_param pa = {8, 1024};
	ctx->logger = (struct klogger_t *) ltrace_open((ltrace_t*)_ctx->parent->logger, &pa);
	if(_ctx == NULL) {
		const char *ptrace = getenv("DEOS_TRACE");
		if(ptrace == NULL) {
			ptrace = "$(setenv DEOS_TRACE )";
		}
		ltrace_record(_ctx->logger, LOG_NOTICE, "konoha:newtrace",
				LOG_s("parent", ptrace), LOG_u("ppid", getppid()));
	}
	else {
		ltrace_record(_ctx->logger, LOG_NOTICE, "konoha:newtrace",
				LOG_s("parent", _ctx->trace));
	}
#else
	ctx->logger = NULL;
#endif
}

static void klogger_free(CTX, kcontext_t *ctx)
{
#ifdef K_USING_LOGPOOL
	ltrace_close((ltrace_t*)_ctx->logger);
#endif
}

// -------------------------------------------------------------------------
// ** util **


// -------------------------------------------------------------------------
// ** global **

void konoha_ginit(int argc, const char **argv)
{

}

static void konoha_init(void)
{
	static int isInit = 0;
	if(isInit == 0) {
		isInit = 1;
	}
}

void knh_beginContext(CTX, void **bottom)
{
	_ctx->stack->cstack_bottom = bottom;
}

void knh_endContext(CTX)
{
	_ctx->stack->cstack_bottom = NULL;
}

/* ------------------------------------------------------------------------ */
/* stack */

static void kstack_init(CTX, kcontext_t *ctx, size_t n)
{
	size_t i;
	kstack_t *base = (kstack_t*)KNH_ZMALLOC(sizeof(kstack_t));
	base->stacksize = n;
	base->stack = (ksfp_t*)KNH_ZMALLOC(sizeof(ksfp_t)*n);
	base->stack_uplimit = base->stack + (n - 64);
	for(i = 0; i < n; i++) {
		KINITv(base->stack[i].o, K_NULL);
	}
	KINITv(base->gcstack, new_(Array, K_PAGESIZE/sizeof(void*)));
	KARRAY_INIT(base->cwb, K_PAGESIZE * 4, char);
	KARRAY_INIT(base->ref, K_PAGESIZE, REF_t);
	base->tail = base->ref.refhead;
	ctx->esp = base->stack;
	ctx->stack = base;
}

static void kstack_reftrace(CTX, kcontext_t *ctx)
{
	ksfp_t *sp = ctx->stack->stack;
	BEGIN_REFTRACE((_ctx->esp - sp)+1);
	while(sp < ctx->esp) {
		KREFTRACEv(sp[0].o);
		sp++;
	}
	KREFTRACEv(ctx->stack->gcstack);
	END_REFTRACE();
}

static void kstack_free(CTX, kcontext_t *ctx)
{
	KARRAY_FREE(_ctx->stack->cwb, char);
	KARRAY_FREE(_ctx->stack->ref, REF_t);
	KNH_FREE(_ctx->stack->stack, sizeof(ksfp_t) * ctx->stack->stacksize);
	KNH_FREE(_ctx->stack, sizeof(kstack_t));
}

REF_t* kstack_tail(CTX, size_t size)
{
	kstack_t *stack = _ctx->stack;
	size_t ref_size = stack->tail - stack->ref.refhead;
	if(stack->ref.max < size + ref_size) {
		KARRAY_EXPAND(stack->ref, size + ref_size, REF_t);
		stack->tail = stack->ref.refhead + ref_size;
	}
	REF_t *tail = stack->tail;
	stack->tail = NULL;
	return tail;
}

static kbool_t kshare_setModule(CTX, int x, kmodshare_t *d, kline_t pline)
{
	if(_ctx->modshare[x] == NULL) {
		_ctx->modshare[x] = d;
		return 1;
	}
	else {
		kreportf(ERR_, pline, "already registered: %s", _ctx->modshare[x]->name);
		return 0;
	}
}

/* ------------------------------------------------------------------------ */
/* [kcontext] */

// module
// share local
//       logger
// mem   mem
// share stack
// modshare[128] mod[128]
// keval

void MODEVAL_defMethods(CTX);

static kcontext_t* new_context(const kcontext_t *_ctx)
{
	kcontext_t *newctx;
	static volatile size_t ctxid_counter = 0;
	ctxid_counter++;
	if(_ctx == NULL) {  // NULL means first one
		klib2_t *klib2 = (klib2_t*)malloc(sizeof(klib2_t) + sizeof(kcontext_t));
		bzero(klib2, sizeof(klib2_t) + sizeof(kcontext_t));
		klib2_init(klib2);
		klib2->KsetModule = kshare_setModule;
		newctx = (kcontext_t*)(klib2 + 1);
		newctx->lib2 = klib2;
		_ctx = (CTX_t)newctx;

		klogger_init(_ctx, newctx);
		MODGCSHARE_init(_ctx, newctx);
		kshare_init(_ctx, newctx);
		newctx->modshare = (kmodshare_t**)KNH_ZMALLOC(sizeof(kmodshare_t*) * K_PKGMATRIX);
	}
	else {   // others take ctx as its parent
		newctx = (kcontext_t*)KNH_ZMALLOC(sizeof(kcontext_t));
		newctx->lib2 = _ctx->lib2;
		newctx->memshare = _ctx->memshare;
		newctx->share = _ctx->share;
		newctx->modshare = _ctx->modshare;
		klogger_init(_ctx, newctx);
	}
	//MODGC_init(_ctx, newctx);
	kstack_init(_ctx, newctx, K_PAGESIZE * 16);
	newctx->mod = (kmod_t**)KNH_ZMALLOC(sizeof(kmod_t*) * K_PKGMATRIX);
//	for(i = 0; i < K_PKGMATRIX; i++) {
//		if(newctx->modshare[i] != NULL && newctx->modshare[i]->new_local != NULL) {
//			newctx->mod[i] = newctx->modshare[i]->new_local((CTX_t)newctx, newctx->modshare[i]);
//		}
//	}
	if(IS_ROOTCTX(newctx)) {
		MODCODE_init(_ctx, newctx);
		MODEVAL_init(_ctx, newctx);
		kshare_init_methods(_ctx);
		MODEVAL_defMethods(_ctx);
	}
	return newctx;
}

static void kcontext_reftrace(CTX, kcontext_t *ctx)
{
	size_t i;
	if(IS_ROOTCTX(_ctx)) {
		kshare_reftrace(_ctx, ctx);
		for(i = 0; i < K_PKGMATRIX; i++) {
			kmodshare_t *p = ctx->modshare[i];
			if(p != NULL && p->reftrace != NULL) {
				p->reftrace(_ctx, p);
			}
		}
	}
	kstack_reftrace(_ctx, ctx);
	for(i = 0; i < K_PKGMATRIX; i++) {
		kmod_t *p = ctx->mod[i];
		if(p != NULL && p->reftrace != NULL) {
			p->reftrace(_ctx, p);
		}
	}
}

void kSystem_reftraceAll(CTX)
{
	kcontext_reftrace(_ctx, (kcontext_t*)_ctx);
}

static void kcontext_free(CTX, kcontext_t *ctx)
{
	size_t i;
	for(i = 0; i < K_PKGMATRIX; i++) {
		kmod_t *p = ctx->mod[i];
		if(p != NULL && p->reftrace != NULL) {
			p->free(_ctx, p);
		}
	}
	KNH_FREE(_ctx->mod, sizeof(kmod_t*) * K_PKGMATRIX);
	kstack_free(_ctx, ctx);
	if(IS_ROOTCTX(_ctx)){  // share
		klib2_t *klib2 = (klib2_t*)ctx - 1;
		for(i = 0; i < K_PKGMATRIX; i++) {
			kmodshare_t *p = ctx->modshare[i];
			if(p != NULL && p->free != NULL) {
				p->free(_ctx, p);
			}
		}
		KNH_FREE(_ctx->modshare, sizeof(kmodshare_t*) * K_PKGMATRIX);
		MODGCSHARE_gc_destroy(_ctx, ctx);
		kshare_free(_ctx, ctx);
		MODGCSHARE_free(_ctx, ctx);
		MODGC_free(_ctx, ctx);
		klogger_free(_ctx, ctx);
		free(klib2/*, sizeof(klib2_t) + sizeof(kcontext_t)*/);
	}
	else {
		klogger_free(_ctx, ctx);
		KNH_FREE(ctx, sizeof(kcontext_t));
	}
}

konoha_t konoha_open(void)
{
	konoha_init();
	return (konoha_t)new_context(NULL);
}

void konoha_close(konoha_t konoha)
{
	kcontext_free((CTX_t)konoha, (kcontext_t*)konoha);
}
#ifdef __cplusplus
}
#endif
