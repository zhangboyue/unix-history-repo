/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "@(#)exec.c	5.12 (Berkeley) %G%";
#endif /* not lint */

#include "sh.h"

/*
 * System level search and execute of a command.
 * We look in each directory for the specified command name.
 * If the name contains a '/' then we execute only the full path name.
 * If there is no search path then we execute only full path names.
 */

/*
 * As we search for the command we note the first non-trivial error
 * message for presentation to the user.  This allows us often
 * to show that a file has the wrong mode/no access when the file
 * is not in the last component of the search path, so we must
 * go on after first detecting the error.
 */
static char *exerr;		/* Execution error message */
static Char *expath;		/* Path for exerr */

/*
 * Xhash is an array of HSHSIZ bits (HSHSIZ / 8 chars), which are used
 * to hash execs.  If it is allocated (havhash true), then to tell
 * whether ``name'' is (possibly) present in the i'th component
 * of the variable path, you look at the bit in xhash indexed by
 * hash(hashname("name"), i).  This is setup automatically
 * after .login is executed, and recomputed whenever ``path'' is
 * changed.
 * The two part hash function is designed to let texec() call the
 * more expensive hashname() only once and the simple hash() several
 * times (once for each path component checked).
 * Byte size is assumed to be 8.
 */
#define	HSHSIZ		8192	/* 1k bytes */
#define HSHMASK		(HSHSIZ - 1)
#define HSHMUL		243
static char xhash[HSHSIZ / 8];

#define hash(a, b)	((a) * HSHMUL + (b) & HSHMASK)
#define bit(h, b)	((h)[(b) >> 3] & 1 << ((b) & 7))	/* bit test */
#define bis(h, b)	((h)[(b) >> 3] |= 1 << ((b) & 7))	/* bit set */
#ifdef VFORK
static int hits, misses;

#endif

/* Dummy search path for just absolute search when no path */
static Char *justabs[] = {STRNULL, 0};

static void pexerr();
static void texec();
static int hashname();

void
doexec(t)
    register struct command *t;
{
    register Char *dp, **pv, **av, *sav;
    register struct varent *v;
    register bool slash;
    register int hashval = 0, hashval1, i;
    Char   *blk[2];

    /*
     * Glob the command name. We will search $path even if this does something,
     * as in sh but not in csh.  One special case: if there is no PATH, then we
     * execute only commands which start with '/'.
     */
    blk[0] = t->t_dcom[0];
    blk[1] = 0;
    gflag = 0, tglob(blk);
    if (gflag) {
	pv = globall(blk);
	if (pv == 0) {
	    setname(short2str(blk[0]));
	    stderror(ERR_NAME | ERR_NOMATCH);
	}
	gargv = 0;
    }
    else
	pv = saveblk(blk);

    trim(pv);

    exerr = 0;
    expath = Strsave(pv[0]);
#ifdef VFORK
    Vexpath = expath;
#endif

    v = adrof(STRpath);
    if (v == 0 && expath[0] != '/') {
	blkfree(pv);
	pexerr();
    }
    slash = any(short2str(expath), '/');

    /*
     * Glob the argument list, if necessary. Otherwise trim off the quote bits.
     */
    gflag = 0;
    av = &t->t_dcom[1];
    tglob(av);
    if (gflag) {
	av = globall(av);
	if (av == 0) {
	    blkfree(pv);
	    setname(short2str(expath));
	    stderror(ERR_NAME | ERR_NOMATCH);
	}
	gargv = 0;
    }
    else
	av = saveblk(av);

    blkfree(t->t_dcom);
    t->t_dcom = blkspl(pv, av);
    xfree((ptr_t) pv);
    xfree((ptr_t) av);
    av = t->t_dcom;
    trim(av);

    if (*av == (Char *) 0 || **av == '\0')
	pexerr();

    xechoit(av);		/* Echo command if -x */
    /*
     * Since all internal file descriptors are set to close on exec, we don't
     * need to close them explicitly here.  Just reorient ourselves for error
     * messages.
     */
    SHIN = 0;
    SHOUT = 1;
    SHDIAG = 2;
    OLDSTD = 0;
    /*
     * We must do this AFTER any possible forking (like `foo` in glob) so that
     * this shell can still do subprocesses.
     */
    (void) sigsetmask((sigmask_t) 0);
    /*
     * If no path, no words in path, or a / in the filename then restrict the
     * command search.
     */
    if (v == 0 || v->vec[0] == 0 || slash)
	pv = justabs;
    else
	pv = v->vec;
    sav = Strspl(STRslash, *av);/* / command name for postpending */
#ifdef VFORK
    Vsav = sav;
#endif
    if (havhash)
	hashval = hashname(*av);
    i = 0;
#ifdef VFORK
    hits++;
#endif
    do {
	/*
	 * Try to save time by looking at the hash table for where this command
	 * could be.  If we are doing delayed hashing, then we put the names in
	 * one at a time, as the user enters them.  This is kinda like Korn
	 * Shell's "tracked aliases".
	 */
	if (!slash && pv[0][0] == '/' && havhash) {
	    hashval1 = hash(hashval, i);
	    if (!bit(xhash, hashval1))
		goto cont;
	}
	if (pv[0][0] == 0 || eq(pv[0], STRdot))	/* don't make ./xxx */
	    texec(*av, av);
	else {
	    dp = Strspl(*pv, sav);
#ifdef VFORK
	    Vdp = dp;
#endif
	    texec(dp, av);
#ifdef VFORK
	    Vdp = 0;
#endif
	    xfree((ptr_t) dp);
	}
#ifdef VFORK
	misses++;
#endif
cont:
	pv++;
	i++;
    } while (*pv);
#ifdef VFORK
    hits--;
#endif
#ifdef VFORK
    Vsav = 0;
#endif
    xfree((ptr_t) sav);
    pexerr();
}

static void
pexerr()
{
    /* Couldn't find the damn thing */
    if (expath) {
	setname(short2str(expath));
#ifdef VFORK
	Vexpath = 0;
#endif
	xfree((ptr_t) expath);
	expath = 0;
    }
    else
	setname("");
    if (exerr)
	stderror(ERR_NAME | ERR_STRING, exerr);
    stderror(ERR_NAME | ERR_COMMAND);
}

/*
 * Execute command f, arg list t.
 * Record error message if not found.
 * Also do shell scripts here.
 */
static void
texec(sf, st)
    Char   *sf;
    register Char **st;
{
    register char **t;
    register char *f;
    register struct varent *v;
    register Char **vp;
    Char   *lastsh[2];
    int     fd;
    unsigned char c;
    Char   *st0, **ost;

    /* The order for the conversions is significant */
    t = short2blk(st);
    f = short2str(sf);
#ifdef VFORK
    Vt = t;
#endif
    errno = 0;			/* don't use a previous error */
    (void) execv(f, t);
#ifdef VFORK
    Vt = 0;
#endif
    blkfree((Char **) t);
    switch (errno) {

    case ENOEXEC:
	/*
	 * From: casper@fwi.uva.nl (Casper H.S. Dik) If we could not execute
	 * it, don't feed it to the shell if it looks like a binary!
	 */
	if ((fd = open(f, O_RDONLY)) != -1) {
	    if (read(fd, (char *) &c, 1) == 1) {
		if (!Isprint(c) && (c != '\n' && c != '\t')) {
		    (void) close(fd);
		    /*
		     * We *know* what ENOEXEC means.
		     */
		    stderror(ERR_ARCH, f, strerror(errno));
		}
	    }
#ifdef _PATH_BSHELL
	    else
		c = '#';
#endif
	    (void) close(fd);
	}
	/*
	 * If there is an alias for shell, then put the words of the alias in
	 * front of the argument list replacing the command name. Note no
	 * interpretation of the words at this point.
	 */
	v = adrof1(STRshell, &aliases);
	if (v == 0) {
	    vp = lastsh;
	    vp[0] = adrof(STRshell) ? value(STRshell) : STR_SHELLPATH;
	    vp[1] = NULL;
#ifdef _PATH_BSHELL
	    if (fd != -1 && c != '#')
		vp[0] = STR_BSHELL;
#endif
	}
	else
	    vp = v->vec;
	st0 = st[0];
	st[0] = sf;
	ost = st;
	st = blkspl(vp, st);	/* Splice up the new arglst */
	ost[0] = st0;
	sf = *st;
	/* The order for the conversions is significant */
	t = short2blk(st);
	f = short2str(sf);
	xfree((ptr_t) st);
#ifdef VFORK
	Vt = t;
#endif
	(void) execv(f, t);
#ifdef VFORK
	Vt = 0;
#endif
	blkfree((Char **) t);
	/* The sky is falling, the sky is falling! */

    case ENOMEM:
	stderror(ERR_SYSTEM, f, strerror(errno));

    case ENOENT:
	break;

    default:
	if (exerr == 0) {
	    exerr = strerror(errno);
	    if (expath)
		xfree((ptr_t) expath);
	    expath = Strsave(sf);
#ifdef VFORK
	    Vexpath = expath;
#endif
	}
    }
}

/*ARGSUSED*/
void
execash(t, kp)
    char  **t;
    register struct command *kp;
{
    if (chkstop == 0 && setintr)
	panystop(0);
    rechist();
    (void) signal(SIGINT, parintr);
    (void) signal(SIGQUIT, parintr);
    (void) signal(SIGTERM, parterm);	/* if doexec loses, screw */
    lshift(kp->t_dcom, 1);
    exiterr = 1;
    doexec(kp);
    /* NOTREACHED */
}

void
xechoit(t)
    Char  **t;
{
    if (adrof(STRecho)) {
	flush();
	haderr = 1;
	blkpr(t), xputchar('\n');
	haderr = 0;
    }
}

/*VARARGS0*/
void
dohash()
{
    DIR    *dirp;
    register struct dirent *dp;
    register int cnt;
    int     i = 0;
    struct varent *v = adrof(STRpath);
    Char  **pv;
    int     hashval;

    havhash = 1;
    for (cnt = 0; cnt < sizeof xhash; cnt++)
	xhash[cnt] = 0;
    if (v == 0)
	return;
    for (pv = v->vec; *pv; pv++, i++) {
	if (pv[0][0] != '/')
	    continue;
	dirp = opendir(short2str(*pv));
	if (dirp == NULL)
	    continue;
	while ((dp = readdir(dirp)) != NULL) {
	    if (dp->d_ino == 0)
		continue;
	    if (dp->d_name[0] == '.' &&
		(dp->d_name[1] == '\0' ||
		 dp->d_name[1] == '.' && dp->d_name[2] == '\0'))
		continue;
	    hashval = hash(hashname(str2short(dp->d_name)), i);
	    bis(xhash, hashval);
	    /* tw_add_comm_name (dp->d_name); */
	}
	(void) closedir(dirp);
    }
}

void
dounhash()
{
    havhash = 0;
}

#ifdef VFORK
void
hashstat()
{
    if (hits + misses)
	xprintf("%d hits, %d misses, %d%%\n",
		hits, misses, 100 * hits / (hits + misses));
}

#endif

/*
 * Hash a command name.
 */
static int
hashname(cp)
    register Char *cp;
{
    register long h = 0;

    while (*cp)
	h = hash(h, *cp++);
    return ((int) h);
}
