/*
 * Copyright (c) 1993, David Greenman
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: kern_exec.c,v 1.79 1998/03/02 05:47:55 peter Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/filedesc.h>
#include <sys/fcntl.h>
#include <sys/acct.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
#include <sys/wait.h>
#include <sys/proc.h>
#include <sys/pioctl.h>
#include <sys/namei.h>
#include <sys/sysent.h>
#include <sys/shm.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/buf.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_zone.h>
#include <vm/vm_pager.h>

#include <machine/reg.h>

static int *exec_copyout_strings __P((struct image_params *));

/*
 * XXX trouble here if sizeof(caddr_t) != sizeof(int), other parts
 * of the sysctl code also assumes this, and sizeof(int) == sizeof(long).
 */
static struct ps_strings *ps_strings = PS_STRINGS;
SYSCTL_INT(_kern, KERN_PS_STRINGS, ps_strings, 0, &ps_strings, 0, "");

static caddr_t usrstack = (caddr_t)USRSTACK;
SYSCTL_INT(_kern, KERN_USRSTACK, usrstack, 0, &usrstack, 0, "");

/*
 * execsw_set is constructed for us by the linker.  Each of the items
 * is a pointer to a `const struct execsw', hence the double pointer here.
 */
static const struct execsw **execsw = 
	(const struct execsw **)&execsw_set.ls_items[0];

#ifndef _SYS_SYSPROTO_H_
struct execve_args {
        char    *fname; 
        char    **argv;
        char    **envv; 
};
#endif

/*
 * execve() system call.
 */
int
execve(p, uap)
	struct proc *p;
	register struct execve_args *uap;
{
	struct nameidata nd, *ndp;
	int *stack_base;
	int error, len, i;
	struct image_params image_params, *imgp;
	struct vattr attr;

	imgp = &image_params;

	/*
	 * Initialize part of the common data
	 */
	imgp->proc = p;
	imgp->uap = uap;
	imgp->attr = &attr;
	imgp->argc = imgp->envc = 0;
	imgp->argv0 = NULL;
	imgp->entry_addr = 0;
	imgp->vmspace_destroyed = 0;
	imgp->interpreted = 0;
	imgp->interpreter_name[0] = '\0';
	imgp->auxargs = NULL;
	imgp->vp = NULL;
	imgp->firstpage = NULL;

	/*
	 * Allocate temporary demand zeroed space for argument and
	 *	environment strings
	 */
	imgp->stringbase = (char *)kmem_alloc_wait(exec_map, ARG_MAX + PAGE_SIZE);
	if (imgp->stringbase == NULL) {
		error = ENOMEM;
		goto exec_fail;
	}
	imgp->stringp = imgp->stringbase;
	imgp->stringspace = ARG_MAX;
	imgp->image_header = imgp->stringbase + ARG_MAX;

	/*
	 * Translate the file name. namei() returns a vnode pointer
	 *	in ni_vp amoung other things.
	 */
	ndp = &nd;
	NDINIT(ndp, LOOKUP, LOCKLEAF | FOLLOW | SAVENAME,
	    UIO_USERSPACE, uap->fname, p);

interpret:

	error = namei(ndp);
	if (error) {
		kmem_free_wakeup(exec_map, (vm_offset_t)imgp->stringbase,
			ARG_MAX + PAGE_SIZE);
		goto exec_fail;
	}

	imgp->vp = ndp->ni_vp;

	/*
	 * Check file permissions (also 'opens' file)
	 */
	error = exec_check_permissions(imgp);
	if (error) {
		VOP_UNLOCK(imgp->vp, 0, p);
		goto exec_fail_dealloc;
	}

	error = exec_map_first_page(imgp);
	VOP_UNLOCK(imgp->vp, 0, p);
	if (error)
		goto exec_fail_dealloc;

	/*
	 * Loop through list of image activators, calling each one.
	 *	If there is no match, the activator returns -1. If there
	 *	is a match, but there was an error during the activation,
	 *	the error is returned. Otherwise 0 means success. If the
	 *	image is interpreted, loop back up and try activating
	 *	the interpreter.
	 */
	for (i = 0; execsw[i]; ++i) {
		if (execsw[i]->ex_imgact)
			error = (*execsw[i]->ex_imgact)(imgp);
		else
			continue;
		if (error == -1)
			continue;
		if (error)
			goto exec_fail_dealloc;
		if (imgp->interpreted) {
			exec_unmap_first_page(imgp);
			/* free old vnode and name buffer */
			vrele(ndp->ni_vp);
			zfree(namei_zone, ndp->ni_cnd.cn_pnbuf);
			/* set new name to that of the interpreter */
			NDINIT(ndp, LOOKUP, LOCKLEAF | FOLLOW | SAVENAME,
			    UIO_SYSSPACE, imgp->interpreter_name, p);
			goto interpret;
		}
		break;
	}
	/* If we made it through all the activators and none matched, exit. */
	if (error == -1) {
		error = ENOEXEC;
		goto exec_fail_dealloc;
	}

	/*
	 * Copy out strings (args and env) and initialize stack base
	 */
	stack_base = exec_copyout_strings(imgp);
	p->p_vmspace->vm_minsaddr = (char *)stack_base;

	/*
	 * If custom stack fixup routine present for this process
	 * let it do the stack setup.
	 * Else stuff argument count as first item on stack
	 */
	if (p->p_sysent->sv_fixup)
		(*p->p_sysent->sv_fixup)(&stack_base, imgp);
	else
		suword(--stack_base, imgp->argc);

	/*
	 * For security and other reasons, the file descriptor table cannot
	 * be shared after an exec.
	 */
	if (p->p_fd->fd_refcnt > 1) {
		struct filedesc *tmp;

		tmp = fdcopy(p);
		fdfree(p);
		p->p_fd = tmp;
	}

	/* close files on exec */
	fdcloseexec(p);

	/* reset caught signals */
	execsigs(p);

	/* name this process - nameiexec(p, ndp) */
	len = min(ndp->ni_cnd.cn_namelen,MAXCOMLEN);
	bcopy(ndp->ni_cnd.cn_nameptr, p->p_comm, len);
	p->p_comm[len] = 0;

	/*
	 * mark as execed, wakeup the process that vforked (if any) and tell
	 * it that it now has it's own resources back
	 */
	p->p_flag |= P_EXEC;
	if (p->p_pptr && (p->p_flag & P_PPWAIT)) {
		p->p_flag &= ~P_PPWAIT;
		wakeup((caddr_t)p->p_pptr);
	}

	/*
	 * Implement image setuid/setgid.
	 *
	 * Don't honor setuid/setgid if the filesystem prohibits it or if
	 * the process is being traced.
	 */
	if ((attr.va_mode & VSUID && p->p_ucred->cr_uid != attr.va_uid ||
	     attr.va_mode & VSGID && p->p_ucred->cr_gid != attr.va_gid) &&
	    (imgp->vp->v_mount->mnt_flag & MNT_NOSUID) == 0 &&
	    (p->p_flag & P_TRACED) == 0) {
		/*
		 * Turn off syscall tracing for set-id programs, except for
		 * root.
		 */
		if (p->p_tracep && suser(p->p_ucred, &p->p_acflag)) {
			p->p_traceflag = 0;
			vrele(p->p_tracep);
			p->p_tracep = NULL;
		}
		/*
		 * Set the new credentials.
		 */
		p->p_ucred = crcopy(p->p_ucred);
		if (attr.va_mode & VSUID)
			p->p_ucred->cr_uid = attr.va_uid;
		if (attr.va_mode & VSGID)
			p->p_ucred->cr_gid = attr.va_gid;
		setsugid(p);
	} else {
		if (p->p_ucred->cr_uid == p->p_cred->p_ruid &&
		    p->p_ucred->cr_gid == p->p_cred->p_rgid)
			p->p_flag &= ~P_SUGID;
	}

	/*
	 * Implement correct POSIX saved-id behavior.
	 */
	p->p_cred->p_svuid = p->p_ucred->cr_uid;
	p->p_cred->p_svgid = p->p_ucred->cr_gid;

	/*
	 * Store the vp for use in procfs
	 */
	if (p->p_textvp)		/* release old reference */
		vrele(p->p_textvp);
	VREF(ndp->ni_vp);
	p->p_textvp = ndp->ni_vp;

	/*
	 * If tracing the process, trap to debugger so breakpoints
	 * 	can be set before the program executes.
	 */
	STOPEVENT(p, S_EXEC, 0);

	if (p->p_flag & P_TRACED)
		psignal(p, SIGTRAP);

	/* clear "fork but no exec" flag, as we _are_ execing */
	p->p_acflag &= ~AFORK;

	/* Set entry address */
	setregs(p, imgp->entry_addr, (u_long)stack_base);

exec_fail_dealloc:

	/*
	 * free various allocated resources
	 */
	if (imgp->firstpage)
		exec_unmap_first_page(imgp);

	if (imgp->stringbase != NULL)
		kmem_free_wakeup(exec_map, (vm_offset_t)imgp->stringbase,
			ARG_MAX + PAGE_SIZE);

	if (ndp->ni_vp) {
		vrele(ndp->ni_vp);
		zfree(namei_zone, ndp->ni_cnd.cn_pnbuf);
	}

	if (error == 0)
		return (0);

exec_fail:
	if (imgp->vmspace_destroyed) {
		/* sorry, no more process anymore. exit gracefully */
		exit1(p, W_EXITCODE(0, SIGABRT));
		/* NOT REACHED */
		return(0);
	} else {
		return(error);
	}
}

int
exec_map_first_page(imgp)
	struct image_params *imgp;
{
	int s, rv, i;
	int initial_pagein;
	vm_page_t ma[VM_INITIAL_PAGEIN];
	vm_object_t object;


	if (imgp->firstpage) {
		exec_unmap_first_page(imgp);
	}

	object = imgp->vp->v_object;
	s = splvm();

	ma[0] = vm_page_grab(object, 0, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);

	if ((ma[0]->valid & VM_PAGE_BITS_ALL) != VM_PAGE_BITS_ALL) {
		initial_pagein = VM_INITIAL_PAGEIN;
		if (initial_pagein > object->size)
			initial_pagein = object->size;
		for (i = 1; i < initial_pagein; i++) {
			if (ma[i] = vm_page_lookup(object, i)) {
				if ((ma[i]->flags & PG_BUSY) || ma[i]->busy)
					break;
				if (ma[i]->valid)
					break;
				ma[i]->flags |= PG_BUSY;
			} else {
				ma[i] = vm_page_alloc(object, i, VM_ALLOC_NORMAL);
				if (ma[i] == NULL)
					break;
			}
		}
		initial_pagein = i;

		rv = vm_pager_get_pages(object, ma, initial_pagein, 0);
		ma[0] = vm_page_lookup(object, 0);

		if ((rv != VM_PAGER_OK) || (ma[0] == NULL)) {
			vm_page_protect(ma[0], VM_PROT_NONE);
			vm_page_free(ma[0]);
			splx(s);
			return EIO;
		}
	}

	vm_page_wire(ma[0]);
	PAGE_WAKEUP(ma[0]);
	splx(s);

	pmap_kenter((vm_offset_t) imgp->image_header, VM_PAGE_TO_PHYS(ma[0]));
	imgp->firstpage = ma[0];

	return 0;
}

void
exec_unmap_first_page(imgp)
	struct image_params *imgp;
{
	if (imgp->firstpage) {
		pmap_kremove((vm_offset_t) imgp->image_header);
		vm_page_unwire(imgp->firstpage);
		imgp->firstpage = NULL;
	}
}

/*
 * Destroy old address space, and allocate a new stack
 *	The new stack is only SGROWSIZ large because it is grown
 *	automatically in trap.c.
 */
int
exec_new_vmspace(imgp)
	struct image_params *imgp;
{
	int error;
	struct vmspace *vmspace = imgp->proc->p_vmspace;
	caddr_t	stack_addr = (caddr_t) (USRSTACK - SGROWSIZ);
	vm_map_t map = &vmspace->vm_map;

	imgp->vmspace_destroyed = 1;

	/*
	 * Blow away entire process VM, if address space not shared,
	 * otherwise, create a new VM space so that other threads are
	 * not disrupted
	 */
	if (vmspace->vm_refcnt == 1) {
		if (vmspace->vm_shm)
			shmexit(imgp->proc);
		pmap_remove_pages(&vmspace->vm_pmap, 0, USRSTACK);
		vm_map_remove(map, 0, USRSTACK);
	} else {
		vmspace_exec(imgp->proc);
		vmspace = imgp->proc->p_vmspace;
		map = &vmspace->vm_map;
	}

	/* Allocate a new stack */
	error = vm_map_insert(&vmspace->vm_map, NULL, 0,
		(vm_offset_t) stack_addr, (vm_offset_t) USRSTACK,
		VM_PROT_ALL, VM_PROT_ALL, 0);
	if (error)
		return (error);

	vmspace->vm_ssize = SGROWSIZ >> PAGE_SHIFT;

	/* Initialize maximum stack address */
	vmspace->vm_maxsaddr = (char *)USRSTACK - MAXSSIZ;

	return(0);
}

/*
 * Copy out argument and environment strings from the old process
 *	address space into the temporary string buffer.
 */
int
exec_extract_strings(imgp)
	struct image_params *imgp;
{
	char	**argv, **envv;
	char	*argp, *envp;
	int	error, length;

	/*
	 * extract arguments first
	 */

	argv = imgp->uap->argv;

	if (argv) {
		argp = (caddr_t) fuword(argv);
		if (argp == (caddr_t) -1)
			return (EFAULT);
		if (argp)
			argv++;
		if (imgp->argv0)
			argp = imgp->argv0;
		if (argp) {
			do {
				if (argp == (caddr_t) -1)
					return (EFAULT);
				if ((error = copyinstr(argp, imgp->stringp,
				    imgp->stringspace, &length))) {
					if (error == ENAMETOOLONG)
						return(E2BIG);
					return (error);
				}
				imgp->stringspace -= length;
				imgp->stringp += length;
				imgp->argc++;
			} while ((argp = (caddr_t) fuword(argv++)));
		}
	}	

	/*
	 * extract environment strings
	 */

	envv = imgp->uap->envv;

	if (envv) {
		while ((envp = (caddr_t) fuword(envv++))) {
			if (envp == (caddr_t) -1)
				return (EFAULT);
			if ((error = copyinstr(envp, imgp->stringp,
			    imgp->stringspace, &length))) {
				if (error == ENAMETOOLONG)
					return(E2BIG);
				return (error);
			}
			imgp->stringspace -= length;
			imgp->stringp += length;
			imgp->envc++;
		}
	}

	return (0);
}

/*
 * Copy strings out to the new process address space, constructing
 *	new arg and env vector tables. Return a pointer to the base
 *	so that it can be used as the initial stack pointer.
 */
int *
exec_copyout_strings(imgp)
	struct image_params *imgp;
{
	int argc, envc;
	char **vectp;
	char *stringp, *destp;
	int *stack_base;
	struct ps_strings *arginfo;
	int szsigcode;

	/*
	 * Calculate string base and vector table pointers.
	 * Also deal with signal trampoline code for this exec type.
	 */
	arginfo = PS_STRINGS;
	szsigcode = *(imgp->proc->p_sysent->sv_szsigcode);
	destp =	(caddr_t)arginfo - szsigcode - SPARE_USRSPACE -
		roundup((ARG_MAX - imgp->stringspace), sizeof(char *));

	/*
	 * install sigcode
	 */
	if (szsigcode)
		copyout(imgp->proc->p_sysent->sv_sigcode,
			((caddr_t)arginfo - szsigcode), szsigcode);

	/*
	 * If we have a valid auxargs ptr, prepare some room
	 * on the stack.
	 */
	if (imgp->auxargs)
	/*
	 * The '+ 2' is for the null pointers at the end of each of the
	 * arg and env vector sets, and 'AT_COUNT*2' is room for the
	 * ELF Auxargs data.
	 */
		vectp = (char **)(destp - (imgp->argc + imgp->envc + 2 +
				  AT_COUNT*2) * sizeof(char*));
	else 
	/*
	 * The '+ 2' is for the null pointers at the end of each of the
	 * arg and env vector sets
	 */
		vectp = (char **)
			(destp - (imgp->argc + imgp->envc + 2) * sizeof(char*));

	/*
	 * vectp also becomes our initial stack base
	 */
	stack_base = (int *)vectp;

	stringp = imgp->stringbase;
	argc = imgp->argc;
	envc = imgp->envc;

	/*
	 * Copy out strings - arguments and environment.
	 */
	copyout(stringp, destp, ARG_MAX - imgp->stringspace);

	/*
	 * Fill in "ps_strings" struct for ps, w, etc.
	 */
	suword(&arginfo->ps_argvstr, (int)vectp);
	suword(&arginfo->ps_nargvstr, argc);

	/*
	 * Fill in argument portion of vector table.
	 */
	for (; argc > 0; --argc) {
		suword(vectp++, (int)destp);
		while (*stringp++ != 0)
			destp++;
		destp++;
	}

	/* a null vector table pointer seperates the argp's from the envp's */
	suword(vectp++, 0);

	suword(&arginfo->ps_envstr, (int)vectp);
	suword(&arginfo->ps_nenvstr, envc);

	/*
	 * Fill in environment portion of vector table.
	 */
	for (; envc > 0; --envc) {
		suword(vectp++, (int)destp);
		while (*stringp++ != 0)
			destp++;
		destp++;
	}

	/* end of vector table is a null pointer */
	suword(vectp, 0);

	return (stack_base);
}

/*
 * Check permissions of file to execute.
 *	Return 0 for success or error code on failure.
 */
int
exec_check_permissions(imgp)
	struct image_params *imgp;
{
	struct proc *p = imgp->proc;
	struct vnode *vp = imgp->vp;
	struct vattr *attr = imgp->attr;
	int error;

	/* Get file attributes */
	error = VOP_GETATTR(vp, attr, p->p_ucred, p);
	if (error)
		return (error);

	/*
	 * 1) Check if file execution is disabled for the filesystem that this
	 *	file resides on.
	 * 2) Insure that at least one execute bit is on - otherwise root
	 *	will always succeed, and we don't want to happen unless the
	 *	file really is executable.
	 * 3) Insure that the file is a regular file.
	 */
	if ((vp->v_mount->mnt_flag & MNT_NOEXEC) ||
	    ((attr->va_mode & 0111) == 0) ||
	    (attr->va_type != VREG)) {
		return (EACCES);
	}

	/*
	 * Zero length files can't be exec'd
	 */
	if (attr->va_size == 0)
		return (ENOEXEC);

	/*
	 *  Check for execute permission to file based on current credentials.
	 */
	error = VOP_ACCESS(vp, VEXEC, p->p_ucred, p);
	if (error)
		return (error);

	/*
	 * Check number of open-for-writes on the file and deny execution
	 * if there are any.
	 */
	if (vp->v_writecount)
		return (ETXTBSY);

	/*
	 * Call filesystem specific open routine (which does nothing in the
	 * general case).
	 */
	error = VOP_OPEN(vp, FREAD, p->p_ucred, p);
	if (error)
		return (error);

	return (0);
}
