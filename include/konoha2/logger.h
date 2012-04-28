/****************************************************************************
 * Copyright (c) 2012, the Konoha project authors. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ***************************************************************************/

#ifndef EVIDENCE_H_
#define EVIDENCE_H_

#define LOGPOL_RECORD       (1<<0) /* logger works only with this flag */
 /* Critical */
#define LOGPOL_CRIT         (1<<1)
/*DEFAULT, error*/
#define LOGPOL_ERR          (0)
#define LOGPOL_WARN         (1<<2) /* Warning */
#define LOGPOL_INFO         (1<<3) /* Information */
#define LOGPOL_DEBUG        (1<<4) /* Debug information */

/* LogPolicy */
#define _PreAction         (1<<5) /* preaction */
#define _ChangeEnv         (1<<6) /* change environment */
#define _SecurityAudit     (1<<7) /* security auditing */

/* ??? */
#define _SystemFault       (1<<8)
#define _ScriptFault       (1<<9)  /* scripter's fault */
#define _DataFault         (1<<10) /* invalid data */
#define _ExternalFault     (1<<11)

#define LOGPOL_INIT         (1<<12) /* DONT USE THIS */
#define LOGPOL_CFUNC        (1<<13) /* DONT USE THIS */

typedef struct klogconf_t {
	int policy;
	void *ptr; // for precompiled formattings
	union {
		const char *func;
		const struct _kMethod *mtd;
	};
} klogconf_t ;

#define LOG_END 0
#define LOG_s   1
#define LOG_u   2

#define KEYVALUE_u(K,V)    LOG_u, (K), ((uintptr_t)V)
#define KEYVALUE_s(K,V)    LOG_s, (K), (V)

#define LOG_uline          KEYVALUE_u("uline", sfp[K_RTNIDX].uline)

#define ktrace(POLICY, ...)    do {\
		static klogconf_t _logconf = {LOGPOL_RECORD|LOGPOL_INIT|LOGPOL_CFUNC|POLICY, NULL, {__FUNCTION__}};\
		if(TFLAG_is(int, _logconf.policy, LOGPOL_RECORD)) {\
			(KPI)->Ktrace(_ctx, &_logconf, ## __VA_ARGS__, LOG_END);\
		}\
	}while(0)\

#endif /* EVIDENCE_H_ */
