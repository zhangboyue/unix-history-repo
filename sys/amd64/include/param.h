/*-
 * Copyright (c) 2002 David E. O'Brien.  All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)param.h	8.1 (Berkeley) 6/10/93
 * $FreeBSD$
 */

/*
 * Machine dependent constants for AMD64.
 */

/*
 * Round p (pointer or byte index) up to a correctly-aligned value
 * for all data types (int, long, ...).   The result is u_long and
 * must be cast to any desired pointer type.
 */
#ifndef _ALIGNBYTES
#define	_ALIGNBYTES	(sizeof(long) - 1)
#endif
#ifndef _ALIGN
#define	_ALIGN(p)	(((u_long)(p) + _ALIGNBYTES) &~ _ALIGNBYTES)
#endif

#ifndef _NO_NAMESPACE_POLLUTION

#define __HAVE_ACPI
#define __PCI_REROUTE_INTERRUPT

#ifndef _MACHINE_PARAM_H_
#define	_MACHINE_PARAM_H_

#ifndef MACHINE
#define	MACHINE		"amd64"
#endif
#ifndef MACHINE_ARCH
#define	MACHINE_ARCH	"amd64"
#endif

#if defined(SMP) || defined(KLD_MODULE)
#define MAXCPU		32
#else
#define MAXCPU		1
#endif

#define	ALIGNBYTES		_ALIGNBYTES
#define	ALIGN(p)		_ALIGN(p)
/*
 * ALIGNED_POINTER is a boolean macro that checks whether an address
 * is valid to fetch data elements of type t from on this architecture.
 * This does not reflect the optimal alignment, just the possibility
 * (within reasonable limits). 
 */
#define	ALIGNED_POINTER(p, t)	1


/* Size of the level 1 page table units */
#define NPTEPG		(PAGE_SIZE/(sizeof (pt_entry_t)))
#define	NPTEPGSHIFT	9		/* LOG2(NPTEPG) */
#define PAGE_SHIFT	12		/* LOG2(PAGE_SIZE) */
#define PAGE_SIZE	(1<<PAGE_SHIFT)	/* bytes/page */
#define PAGE_MASK	(PAGE_SIZE-1)
/* Size of the level 2 page directory units */
#define	NPDEPG		(PAGE_SIZE/(sizeof (pd_entry_t)))
#define	NPDEPGSHIFT	9		/* LOG2(NPDEPG) */
#define	PDRSHIFT	21              /* LOG2(NBPDR) */
#define	NBPDR		(1<<PDRSHIFT)   /* bytes/page dir */
#define	PDRMASK		(NBPDR-1)
/* Size of the level 3 page directory pointer table units */
#define	NPDPEPG		(PAGE_SIZE/(sizeof (pdp_entry_t)))
#define	NPDPEPGSHIFT	9		/* LOG2(NPDPEPG) */
#define	PDPSHIFT	30		/* LOG2(NBPDP) */
#define	NBPDP		(1<<PDPSHIFT)	/* bytes/page dir ptr table */
#define	PDPMASK		(NBPDP-1)
/* Size of the level 4 page-map level-4 table units */
#define	NPML4EPG	(PAGE_SIZE/(sizeof (pml4_entry_t)))
#define	NPML4EPGSHIFT	9		/* LOG2(NPML4EPG) */
#define	PML4SHIFT	39		/* LOG2(NBPML4) */
#define	NBPML4		(1ul<<PML4SHIFT)/* bytes/page map lev4 table */
#define	PML4MASK	(NBPML4-1)

#define	MAXPAGESIZES	3	/* maximum number of supported page sizes */

#define IOPAGES	2		/* pages of i/o permission bitmap */

#ifndef	KSTACK_PAGES
#define	KSTACK_PAGES	4	/* pages of kstack (with pcb) */
#endif
#define	KSTACK_GUARD_PAGES 1	/* pages of kstack guard; 0 disables */

/*
 * Ceiling on amount of swblock kva space, can be changed via
 * the kern.maxswzone /boot/loader.conf variable.
 */
#ifndef VM_SWZONE_SIZE_MAX
#define	VM_SWZONE_SIZE_MAX	(32 * 1024 * 1024)
#endif

/*
 * Mach derived conversion macros
 */
#define	round_page(x)	((((unsigned long)(x)) + PAGE_MASK) & ~(PAGE_MASK))
#define	trunc_page(x)	((unsigned long)(x) & ~(PAGE_MASK))
#define trunc_2mpage(x)	((unsigned long)(x) & ~PDRMASK)
#define round_2mpage(x)	((((unsigned long)(x)) + PDRMASK) & ~PDRMASK)

#define	atop(x)		((unsigned long)(x) >> PAGE_SHIFT)
#define	ptoa(x)		((unsigned long)(x) << PAGE_SHIFT)

#define	amd64_btop(x)	((unsigned long)(x) >> PAGE_SHIFT)
#define	amd64_ptob(x)	((unsigned long)(x) << PAGE_SHIFT)

#define	pgtok(x)	((unsigned long)(x) * (PAGE_SIZE / 1024)) 

#endif /* !_MACHINE_PARAM_H_ */
#endif /* !_NO_NAMESPACE_POLLUTION */
