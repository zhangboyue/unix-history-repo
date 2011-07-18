/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 * NETLOGIC_BSD */

#ifndef __NLM_CPUCONTROL_H__
#define __NLM_CPUCONTROL_H__

#define XLP_CPU_BLOCKID_IFU	0
#define XLP_CPU_BLOCKID_ICU	1

#define XLP_CPU_BLOCKID_IEU	2
#define XLP_CPU_BLOCKID_LSU	3
#define XLP_LSU_DEFEATURE	0x304
#define XLP_LSU_CERRLOG_REGID 	0x09

#define XLP_CPU_BLOCKID_MMU	4
#define XLP_CPU_BLOCKID_PRF	5

#define XLP_CPU_BLOCKID_SCH	7
#define XLP_SCHED_DEFEATURE	0x700

#define XLP_CPU_BLOCKID_SCU	8
#define XLP_CPU_BLOCKID_FPU	9

#define XLP_CPU_BLOCKID_MAP	10

/* Offsets of interest from the 'MAP' Block */
#define XLP_BLKID_MAP_THREADMODE                    0x00
#define XLP_BLKID_MAP_EXT_EBASE_ENABLE              0x04
#define XLP_BLKID_MAP_CCDI_CONFIG                   0x08
#define XLP_BLKID_MAP_THRD0_CCDI_STATUS             0x0c
#define XLP_BLKID_MAP_THRD1_CCDI_STATUS             0x10
#define XLP_BLKID_MAP_THRD2_CCDI_STATUS             0x14
#define XLP_BLKID_MAP_THRD3_CCDI_STATUS             0x18
#define XLP_BLKID_MAP_THRD0_DEBUG_MODE              0x1c
#define XLP_BLKID_MAP_THRD1_DEBUG_MODE              0x20
#define XLP_BLKID_MAP_THRD2_DEBUG_MODE              0x24
#define XLP_BLKID_MAP_THRD3_DEBUG_MODE              0x28
#define XLP_BLKID_MAP_MISC_STATE                    0x60
#define XLP_BLKID_MAP_DEBUG_READ_CTL                0x64
#define XLP_BLKID_MAP_DEBUG_READ_REG0               0x68
#define XLP_BLKID_MAP_DEBUG_READ_REG1               0x6c

#endif /* __NLM_CPUCONTROL_H__ */
