/* Copyright 1992-2000 by Abacus Research and
 * Development, Inc.  All rights reserved.
 */

#if !defined (OMIT_RCSID_STRINGS)
char ROMlib_rcsid_fileDouble[] =
	    "$Id: fileDouble.c 86 2005-05-25 00:47:12Z ctm $";
#endif

#include "rsys/common.h"
#include "FileMgr.h"
#include "rsys/file.h"

#if defined (CYGWIN32)
#include "winfs.h"
#endif

int afpd_conventions_p;
int netatalk_conventions_p;

char apple_double_quote_char;
const char *apple_double_fork_prefix;
int apple_double_fork_prefix_length;

/*
 * Coded more or less up to the spec:  APDA M0908LL/A
 *				       AppleSingle/AppleDouble Formats
 *				       For Foreign Files Developer's Note
 *
 * I don't like this spec for a variety of reasons:
 *
 *	The idea that you should ever have to look inside a file to determine
 *	whether %father is really 0xfather or the header file for "father"
 *	is messed.  The way we get around this is we always quote the first
 *	if that's really what they want, so the header file for "father"
 *	will be %%66ather (actually we only do that if the two characters
 *	are hex for something that we'd expand normally (i.e. '%', '/', NUL
 *	or something with the high bit on, like 0xFA.
 *
 *	The spec says you can put the descriptors in any order you want, but
 *	it would be much nicer if they had to be sorted by offset.  This is
 *	what we do/assume.
 */

#include "rsys/filedouble.h"
#include "rsys/suffix_maps.h"
#include "rsys/osutil.h"

PRIVATE defaulthead_t ourdefault
#if !defined(__alpha)
= {
    {
	CLC(DOUBLEMAGIC),
	CLC(SINGLEVERSION),
	{ 0L, 0L, 0L, 0L, },
	CWC(4),
    },
    {
#define LEN0	(sizeof(struct defaulthead))
	{ CLC(Macintosh_File_Info_ID), CLC(LEN0), CLC(sizeof(Single_attribs)) },
#define LEN1	(LEN0 + sizeof(Single_attribs))
	{ CLC(File_Dates_Info_ID),     CLC(LEN1), CLC(sizeof(Single_dates)) },
#define LEN2	(LEN1 + sizeof(Single_dates))
	{ CLC(Finder_Info_ID),         CLC(LEN2), CLC(sizeof(Single_finfo)) },
	{ CLC(Resource_Fork_ID),       CLC(512), 0L },
    }
};

#define initialize_ourdefault()

#else /* defined(__alpha) */
;

/*
 * compiler bug forces us to do this
 */

#define LEN0	(sizeof(struct defaulthead))
#define LEN1	(LEN0 + sizeof(Single_attribs))
#define LEN2	(LEN1 + sizeof(Single_dates))

PRIVATE void initialize_ourdefault( void )
{
    static int beenhere = 0;

    if (!beenhere) {
	beenhere = 1;
	ourdefault.head.magic    = CLC(DOUBLEMAGIC);
	ourdefault.head.version  = CLC(SINGLEVERSION);
	ourdefault.head.nentries = CWC(4);

	ourdefault.desc[0].id     = CLC(Macintosh_File_Info_ID);
	ourdefault.desc[0].offset = CLC(LEN0);
	ourdefault.desc[0].length = CLC(sizeof(Single_attribs));

	ourdefault.desc[1].id     = CLC(File_Dates_Info_ID);
	ourdefault.desc[1].offset = CLC(LEN1);
	ourdefault.desc[1].length = CLC(sizeof(Single_dates));

	ourdefault.desc[2].id     = CLC(Finder_Info_ID);
	ourdefault.desc[2].offset = CLC(LEN2);
	ourdefault.desc[2].length = CLC(sizeof(Single_finfo));

	ourdefault.desc[3].id     = CLC(Resource_Fork_ID);
	ourdefault.desc[3].offset = CLC(512);
	ourdefault.desc[3].length = CLC(0L);
    }
}
#endif /* defined(__alpha) */

PRIVATE defaultentries_t ourentries;

/* Detect the conventions per directory; mounted host volumes can mix them.
 * As in HFVExplorer, prefer Netatalk, then Basilisk/SheepShaver, then OS X.
 */
PRIVATE char *
sidecar_path (const char *pathname, const char *filename, const char *prefix)
{
  size_t parent = filename - pathname;
  char *result = malloc (parent + strlen (prefix) + strlen (filename) + 1);
  if (result)
    {
      memcpy (result, pathname, parent);
      sprintf (result + parent, "%s%s", prefix, filename);
    }
  return result;
}

PRIVATE boolean_t
sidecar_exists (const char *name, boolean_t directory)
{
  struct stat sb;
  return name && Ustat (name, &sb) == 0
    && (directory ? S_ISDIR (sb.st_mode) : S_ISREG (sb.st_mode));
}

PRIVATE boolean_t
appledouble_file_p (const char *name)
{
  Single_header header;
  int fd = Uopen (name, O_BINARY|O_RDONLY, 0);
  boolean_t result = fd >= 0
    && read (fd, &header, sizeof (header)) == sizeof (header)
    && header.magic == CLC (DOUBLEMAGIC);
  if (fd >= 0)
    Uclose (fd);
  return result;
}

PUBLIC boolean_t
ROMlib_basilisk_sidecar_p (const char *name)
{
  const char *leaf = strrchr (name, '/');
  return leaf && leaf - name >= 6 && !memcmp (leaf - 6, "/.finf", 6);
}

PUBLIC char *
ROMlib_sidecar_resource_name (const char *metadata)
{
  char *result = strdup (metadata);
  if (result && ROMlib_basilisk_sidecar_p (result))
    memcpy (strrchr (result, '/') - 5, ".rsrc", 5);
  return result;
}

PUBLIC char *
ROMlib_sidecar_rename_name (const char *source, const char *pathname,
                          const char *filename)
{
  const char *leaf = strrchr (source, '/');
  const char *prefix = apple_double_fork_prefix;
  if (ROMlib_basilisk_sidecar_p (source))
    prefix = ".finf/";
  else if (leaf && leaf - source >= 13
           && !memcmp (leaf - 13, "/.AppleDouble", 13))
    prefix = ".AppleDouble/";
  else if (leaf && !strncmp (leaf + 1, "._", 2))
    prefix = "._";
  return sidecar_path (pathname, filename, prefix);
}

PUBLIC char *
ROMlib_sidecar_name (const char *pathname, const char *filename)
{
  const char *prefixes[] = { ".AppleDouble/", ".finf/", "._" };
  char *candidate, *directory;
  int i, style = -1;
  DIR *dir;
  struct dirent *entry;

  /* Existing companions take precedence over directory defaults. */
  for (i = 0; i < 3; ++i)
    {
      candidate = sidecar_path (pathname, filename, prefixes[i]);
      if (sidecar_exists (candidate, FALSE)
          && (i != 2 || appledouble_file_p (candidate)))
        return candidate;
      if (i == 1)
        {
          char *resource = ROMlib_sidecar_resource_name (candidate);
          boolean_t exists = sidecar_exists (resource, FALSE);
          free (resource);
          if (exists)
            return candidate;
        }
      free (candidate);
    }
  for (i = 0; i < 2; ++i)
    {
      directory = sidecar_path (pathname, filename,
                               i == 0 ? ".AppleDouble" : ".finf");
      if (directory)
        directory[(filename - pathname) + (i == 0 ? 12 : 5)] = 0;
      if (sidecar_exists (directory, TRUE))
        style = i;
      free (directory);
      if (style >= 0)
        break;
    }
  if (style < 0)
    {
      directory = sidecar_path (pathname, filename, ".rsrc");
      if (directory)
        directory[(filename - pathname) + 5] = 0;
      if (sidecar_exists (directory, TRUE))
        style = 1;
      free (directory);
    }
  if (style < 0)
    {
#if defined (CYGWIN32)
      struct _finddata_t entry;
      intptr_t search;

      /* A Finder-info lookup is made for every displayed host file.
         Enumerating every sibling here makes browsing a large Windows
         directory quadratic. Ask Windows for AppleDouble names only. */
      directory = sidecar_path (pathname, filename, ".DS_Store");
      if (directory)
        directory[(filename - pathname) + 9] = 0;
      if (sidecar_exists (directory, FALSE))
        style = 2;
      free (directory);
      if (style < 0)
        {
          directory = sidecar_path (pathname, filename, "._*");
          if (directory)
            directory[(filename - pathname) + 3] = 0;
          search = directory ? _findfirst (directory, &entry) : -1;
          if (search != -1)
            {
              do
                {
                  char *probe = sidecar_path (pathname, filename, entry.name);
                  boolean_t valid;
                  if (probe)
                    probe[(filename - pathname) + strlen (entry.name)] = 0;
                  valid = probe && appledouble_file_p (probe);
                  free (probe);
                  if (valid)
                    {
                      style = 2;
                      break;
                    }
                }
              while (_findnext (search, &entry) == 0);
              _findclose (search);
            }
          free (directory);
        }
#else
      directory = sidecar_path (pathname, filename, "");
      if (directory)
        directory[filename - pathname] = 0;
      dir = directory ? Uopendir (directory) : NULL;
      if (dir)
        {
          while ((entry = readdir (dir)))
            if (!strcmp (entry->d_name, ".DS_Store"))
              {
                style = 2;
                break;
              }
            else if (!strncmp (entry->d_name, "._", 2))
              {
                char *probe = sidecar_path (pathname, filename, entry->d_name);
                boolean_t valid;
                if (probe)
                  probe[(filename - pathname) + strlen (entry->d_name)] = 0;
                valid = probe && appledouble_file_p (probe);
                free (probe);
                if (valid)
                  {
                    style = 2;
                    break;
                  }
              }
          closedir (dir);
        }
      free (directory);
#endif
    }
  return sidecar_path (pathname, filename,
                       style >= 0 ? prefixes[style] : apple_double_fork_prefix);
}

PUBLIC void
double_dir_op (char *name, double_dir_op_t op)
{
  char *last_slash;
  
  last_slash = strrchr (name, '/');
  if (last_slash)
    {
      char save;
      
      save = *last_slash;
      *last_slash = 0;
      switch (op)
	{
	case mkdir_op:
	  Umkdir (name, 0777);
	  break;
	case rmdir_op:
	  {
	    char *to_del, *to_unlink;
	    int len;

#define PARENT_FILE ".Parent"

	    *last_slash = save;
	    len = strlen (name) + 1 + apple_double_fork_prefix_length + 1;
	    to_del = alloca (len + 16);
	    sprintf (to_del, "%s/%s", name, apple_double_fork_prefix);
	    to_unlink = alloca (len + strlen(PARENT_FILE));
	    sprintf (to_unlink, "%s%s", to_del, PARENT_FILE);
	    unlink (to_unlink);
	    to_del[len-2] = 0; /* remove trailing slash */
	    Urmdir (to_del);
	    sprintf (to_del, "%s/.finf", name);
	    Urmdir (to_del);
	    sprintf (to_del, "%s/.rsrc", name);
	    Urmdir (to_del);
	    sprintf (to_del, "%s/.DS_Store", name);
	    Uunlink (to_del);
	  }
	  break;
	default:
	  warning_unexpected ("op = %d", op);
	  break;
	}
      *last_slash = save;
    }
}

PUBLIC int
ROMlib_sidecar_move (char *source, char *target)
{
  struct stat sb;
  int result = 0;
  if (ROMlib_basilisk_sidecar_p (source))
    {
      char *old_resource = ROMlib_sidecar_resource_name (source);
      char *new_resource = ROMlib_sidecar_resource_name (target);
      if (Ustat (old_resource, &sb) == 0)
        {
          double_dir_op (new_resource, mkdir_op);
          result = Urename (old_resource, new_resource);
        }
      free (old_resource);
      free (new_resource);
    }
  if (!result && Ustat (source, &sb) == 0)
    {
      double_dir_op (target, mkdir_op);
      result = Urename (source, target);
    }
  return result;
}

PUBLIC int
ROMlib_sidecar_delete (char *metadata)
{
  int result;
  if (ROMlib_basilisk_sidecar_p (metadata))
    {
      char *resource = ROMlib_sidecar_resource_name (metadata);
      result = Uunlink (resource);
      free (resource);
      if (result && errno != ENOENT && errno != ENOTDIR)
        return result;
    }
  return Uunlink (metadata);
}

A3(PUBLIC, OSErr, ROMlib_newresfork, char *, name, LONGINT *, fdp,
   boolean_t, unix_p)
{
    LONGINT fd;
    OSErr retval;


    if (netatalk_conventions_p)
      double_dir_op (name, mkdir_op);
    if (unix_p)
      {
	ourentries.finfo.finfo.fdType = TICKX("TEXT");
	ourentries.finfo.finfo.fdCreator = TICKX("UNIX");
      }
    else
      {
	ourentries.finfo.finfo.fdType = 0;
	ourentries.finfo.finfo.fdCreator = 0;
      }
    initialize_ourdefault();
    /* Basilisk stores Finder info as 16 bytes plus two reserved bytes, and
       its resource fork is an ordinary file starting at offset zero. */
    if (ROMlib_basilisk_sidecar_p (name) || strstr (name, "/.rsrc/"))
      {
        unsigned char finfo[18] = { 0 };
        double_dir_op (name, mkdir_op);
        fd = Uopen (name, O_BINARY|O_RDWR|O_CREAT, 0666);
        if (fd < 0)
          return ROMlib_maperrno ();
        if (ROMlib_basilisk_sidecar_p (name))
          {
            memcpy (finfo, &ourentries.finfo.finfo, sizeof (FInfo));
            if (write (fd, finfo, sizeof (finfo)) != sizeof (finfo))
              {
                retval = ROMlib_maperrno ();
                Uclose (fd);
                return retval;
              }
          }
        *fdp = fd;
        return noErr;
      }
    if ((fd = Uopen(name, (O_BINARY|O_RDWR|O_CREAT), 0666L)) < 0 ||
	     write(fd, (char *) &ourdefault, sizeof(ourdefault))
						       != sizeof(ourdefault) ||
	     write(fd, (char *) &ourentries, sizeof(ourentries))
						       != sizeof(ourentries)) {
	retval = ROMlib_maperrno();
	Uclose(fd);
    } else {
	retval = noErr;
	*fdp = fd;
    }
    fs_err_hook (retval);
    return retval;
}

/*
 * Modifications to hiddenbyname:
 *
 * haven't thought it through yet.
 */

#define OURBSIZE	512

A5(PRIVATE, BOOLEAN, getsetentry, GetOrSetType, gors, LONGINT, fd,
   Single_ID, sid, Single_descriptor *, savesdp,
   ULONGINT *, lengthp)
{
    LONGINT saveloc;
    struct defaulthead *dfp;
    INTEGER nread;
    Single_descriptor *sdp;
    char buf[OURBSIZE];
    BOOLEAN retval;
    INTEGER n;
    struct stat sbuf;

    retval = FALSE;
    saveloc = lseek(fd, 0L, L_INCR);
    lseek(fd, 0L, L_SET);
    nread = read(fd, buf, sizeof(buf));
    dfp = (struct defaulthead *) buf;
    if (nread >= (int) sizeof(Single_header)
	&& (dfp->head.magic == CLC(SINGLEMAGIC)
	    || dfp->head.magic == CLC(DOUBLEMAGIC)
	    || dfp->head.magic == SINGLEMAGIC
	    || dfp->head.magic == DOUBLEMAGIC)) {
	n = CW (dfp->head.nentries);
	if (n < 0 || n > (nread - (int) sizeof (Single_header))
	                 / (int) sizeof (Single_descriptor))
	  goto finished;
	for (sdp = dfp->desc; --n >= 0 && Cx(sdp->id) != sid; ++sdp)
	    ;
	if (n >= 0) {
	    ULONGINT offset = CL (sdp->offset), length = CL (sdp->length);
	    if (offset > 0x7FFFFFFF || length > 0x7FFFFFFF - offset
	        || offset < sizeof (Single_header)
	                 + CW (dfp->head.nentries) * sizeof (Single_descriptor)
	        || fstat (fd, &sbuf) < 0
	        || (length && (offset > (ULONGINT) sbuf.st_size
	                       || length > (ULONGINT) sbuf.st_size - offset))
	        || (sid == Finder_Info_ID && length < sizeof (FInfo)))
	      goto finished;
	    switch (gors) {
	    case Get:
		*savesdp = *sdp;
		if (lengthp)
		  {
		    Single_descriptor *other;
		    ULONGINT limit = 0x7FFFFFFF;
		    int count = MIN (CW (dfp->head.nentries),
		                     (nread - sizeof (Single_header))
		                     / sizeof (Single_descriptor));
		    for (other = dfp->desc; count-- > 0; ++other)
		      if (CL (other->offset) > offset)
		        limit = MIN (limit, CL (other->offset) - offset);
		    *lengthp = limit;
		  }
		break;
	    case Set:
		*sdp = *savesdp;
		lseek(fd, 0L, L_SET);
		if (write(fd, buf, nread) != nread)
		  goto finished;
		break;
	    default:
		gui_assert(0);
		break;
	    }
	    retval = TRUE;
	}
    }
    else if (sid == Finder_Info_ID && nread >= 18 && nread <= 256
             && fstat (fd, &sbuf) == 0 && sbuf.st_size == nread)
      {
        /* Basilisk/SheepShaver's compact .finf record. */
        savesdp->id = CLC (Finder_Info_ID);
        savesdp->offset = 0;
        savesdp->length = CLC (sizeof (FInfo));
        retval = gors == Get;
      }
finished:
    lseek(fd, saveloc, L_SET);
    return retval;
}

PUBLIC boolean_t
ROMlib_resource_fork_present (LONGINT fd)
{
  Single_descriptor entry;
  return getsetentry (Get, fd, Resource_Fork_ID, &entry, NULL);
}

#define IDWANTED(fp) ((fp->fcflags & fcfisres) ? Resource_Fork_ID : \
								  Data_Fork_ID)

A1(PUBLIC, LONGINT, ROMlib_FORKOFFSET, fcbrec *, fp)	/* INTERNAL */
{
    Single_descriptor d;
    Single_ID idwanted;

    if (fp->fcfd != fp->hiddenfd)
/*-->*/	return 0L;
    idwanted = IDWANTED(fp);
    if (getsetentry(Get, fp->fcfd, IDWANTED(fp), &d, NULL))
	return CL(d.offset);
    else
	return RESOURCEPREAMBLE;
}

A2(PRIVATE, void, writebyteat, LONGINT, fd, LONGINT, loc)
{
    LONGINT saveloc;

    saveloc = lseek(fd, 0L, L_INCR);
    lseek(fd, loc, L_SET);
    write(fd, "", 1);
    lseek(fd, saveloc, L_SET);
}

/* TODO: better error checking */

/* AppleDouble descriptors need not be sorted. When metadata follows the
 * resource fork, grow it at the end instead of overwriting that metadata.
 */
PRIVATE boolean_t
grow_resource_entry (LONGINT fd, Single_descriptor *entry, ULONGINT length)
{
  struct stat sb;
  unsigned char buffer[8192];
  ULONGINT copied = 0, old_offset = CL (entry->offset);
  ULONGINT old_length = CL (entry->length), new_offset;
  LONGINT position = lseek (fd, 0, L_INCR);

  if (fstat (fd, &sb) < 0 || sb.st_size > 0x7FFFFFFF - length)
    return FALSE;
  new_offset = sb.st_size;
  while (copied < old_length)
    {
      ULONGINT amount = MIN (sizeof (buffer), old_length - copied);
      if (lseek (fd, old_offset + copied, L_SET) < 0
          || read (fd, buffer, amount) != amount
          || lseek (fd, new_offset + copied, L_SET) < 0
          || write (fd, buffer, amount) != amount)
        goto fail;
      copied += amount;
    }
  if (ftruncate (fd, new_offset + length) < 0)
    goto fail;
  entry->offset = CL (new_offset);
  entry->length = CL (length);
  if (!getsetentry (Set, fd, Resource_Fork_ID, entry, NULL))
    goto fail;
  lseek (fd, new_offset + position - old_offset, L_SET);
  return TRUE;
fail:
  lseek (fd, position, L_SET);
  return FALSE;
}

A1(PUBLIC, OSErr, ROMlib_seteof, fcbrec *, fp)	/* INTERNAL */
{
    ULONGINT leof, peof, curloc;
    LONGINT fd;
    Single_descriptor d;
    Single_ID idwanted;
    INTEGER i;
    OSErr err;

    fd = fp->fcfd;
    leof = Cx(fp->fcleof);
    err = noErr;
    if (fd == fp->hiddenfd) {	/* mixed file */
	idwanted = IDWANTED(fp);
	if (getsetentry(Get, fd, idwanted, &d, &peof)) {
	    if (leof > peof) {
		if (idwanted != Resource_Fork_ID
		    || !grow_resource_entry (fd, &d, leof))
		  err = ioErr;
		else
		  peof = 0x7FFFFFFF;
	    } else {
		struct stat sb;
		ULONGINT end = CL (d.offset) + leof;

		/* PBAllocate/PBSetEOF precede the copy's first write. Extend
		   the host file before publishing the larger fork length;
		   otherwise the next descriptor read rejects its payload. */
		if (fstat (fd, &sb) < 0
		    || (end > (ULONGINT) sb.st_size && ftruncate (fd, end) < 0))
		  err = ROMlib_maperrno ();
		else
		  {
		    d.length = CL (leof);
		    if (!getsetentry (Set, fd, idwanted, &d, NULL))
		      err = ioErr;
		  }
	    }
	} else
	  {
	    err = fsDSIntErr;
	    warning_unexpected ("getsetentry (Get, %d, %d) failed", fd,
				idwanted);
	  }
	
    } else {			/* pure file */
	peof = Cx(fp->fcPLen);
	if (leof > peof)
	    writebyteat(fd, leof-1);
	else if (leof < peof) {
	    curloc = lseek(fd, 0L, L_INCR);
	    ftruncate(fd, leof);
	    if (curloc > leof)
		lseek(fd, leof, L_SET);
	}
	peof = leof;
    }
    if (err == noErr) {
	for (i = 0; i < NFCB; i++) {
	    if (ROMlib_fcblocks[i].fdfnum == fp->fdfnum &&
				     ROMlib_fcblocks[i].fcvptr == fp->fcvptr &&
				     (ROMlib_fcblocks[i].fcflags & fcfisres) ==
						    (fp->fcflags & fcfisres)) {
		ROMlib_fcblocks[i].fcleof  = CL(leof);
		ROMlib_fcblocks[i].fcPLen = CL(peof);
	    }
	}
    }
    fs_err_hook (err);
    return err;
}

A5(PRIVATE, BOOLEAN, getsetpiece, GetOrSetType, gors, LONGINT, fd,
		       Single_descriptor *, sdp, char *, bufp, LONGINT, length)
{
    LONGINT saveloc;
    BOOLEAN retval;

    saveloc = lseek(fd, 0L, L_INCR);
    /* Compact Finder records must not spill into adjacent entries. */
    if (gors == Get)
      memset (bufp, 0, length);
    length = MIN (length, CL (sdp->length));
    lseek(fd, Cx(sdp->offset), L_SET);
    switch (gors) {
    case Get:
	retval =  read(fd, bufp, length) == length;
	break;
    case Set:
	retval = write(fd, bufp, length) == length;
	break;
    default:
	retval = FALSE;
	gui_assert(0);
	break;
    }
    lseek(fd, saveloc, L_SET);
    return retval;
}

A1(PUBLIC, OSErr, ROMlib_geteofostype, fcbrec *, fp)	/* INTERNAL */
{
    LONGINT fd;
    Single_descriptor d;
    Single_ID idwanted;
    Single_finfo finfo;
    OSErr err;
    struct stat sbuf;

    fd = fp->hiddenfd;
    if (fstat(fp->fcfd, &sbuf) < 0)
	err = ROMlib_maperrno();
    else {
	err = noErr;
	if (fd == fp->fcfd) {	/* mixed file */
	    idwanted = IDWANTED(fp);
	    if (!getsetentry(Get, fd, idwanted, &d, NULL))
		err = fp->fcPLen = 0;
	    else
		fp->fcPLen = fp->fcleof = d.length;
	} else
	    fp->fcleof = fp->fcPLen = CL(sbuf.st_size);
	if (err == noErr) {
	    if (!getsetentry(Get, fd, Finder_Info_ID, &d, NULL) ||
		   (!getsetpiece(Get, fd, &d, (char *) &finfo, sizeof(finfo))))
	      {
		uint32 type;

		if (ROMlib_creator_and_type_from_filename
		    (fp->fcname[0], (char *) fp->fcname+1, NULL, &type))
		  fp->fcbFType = CL (type);
		else
		  fp->fcbFType = TICKX("TEXT");
	      }
	    else
		fp->fcbFType = finfo.finfo.fdType;
	}
    }
    fs_err_hook (err);
    return err;
}

A8(PUBLIC, OSErr,  ROMlib_hiddenbyname, GetOrSetType, gors,	/* INTERNAL */
		   char *, pathname, char *, rpathname, Single_dates *, datep,
		   FInfo *, finfop, FXInfo *, fxinfop, LONGINT *, lenp,
							      LONGINT *, rlenp)
{
    LONGINT rfd;
    struct stat sbuf;
    Single_finfo sfinfo;
    Single_descriptor d;
    OSErr retval;
    BOOLEAN done;

    retval = noErr;
    memset (&sfinfo, 0, sizeof (sfinfo));
    if (gors == Get)
      {
        memset (finfop, 0, sizeof (*finfop));
        memset (fxinfop, 0, sizeof (*fxinfop));
      }
    if (Ustat(pathname, &sbuf) < 0)
	retval = ROMlib_maperrno();
    else {
	done = FALSE;
	rfd = Uopen(rpathname, O_BINARY|(gors == Set ? O_RDWR : O_RDONLY), 0);
/*
if (rfd == -1)
fprintf(stderr, "%s(%d): open '%s' fails\n", __FILE__, __LINE__, rpathname);
*/
	if (rfd < 0) {
	    done = TRUE;
	    if (errno == ENOENT) {
		/* no resource fork (or AppleSingle) */
		/* right now we ignore that it could be AppleSingle */
		switch (gors) {
		case Get:
		    memset(datep,   0, sizeof(*datep));
		    memset(finfop,  0, sizeof(*finfop));
		    memset(fxinfop, 0, sizeof(*fxinfop));
		    *lenp = CL(sbuf.st_size);
		    *rlenp = 0;
		    datep->crdat = CL (UNIXTIMETOMACTIME (MIN (sbuf.st_ctime, sbuf.st_mtime)));
		    datep->moddat = CL (UNIXTIMETOMACTIME (sbuf.st_mtime));
		    if (ROMlib_basilisk_sidecar_p (rpathname))
		      {
		        char *resource = ROMlib_sidecar_resource_name (rpathname);
		        struct stat resource_stat;
		        if (Ustat (resource, &resource_stat) == 0)
		          *rlenp = CL (resource_stat.st_size);
		        free (resource);
		      }
		    break;
		case Set:
		    retval = ROMlib_newresfork(rpathname, &rfd, FALSE);
		    done = FALSE;
		    break;
		default:
		    gui_assert(0);
		    break;
		}
	    } else
		retval = ROMlib_maperrno();
	}
	if (!done && retval == noErr) {
	    switch (gors) {
	    case Get:
		*lenp = CL (sbuf.st_size);
		if (getsetentry(Get, rfd, File_Dates_Info_ID, &d, NULL))
		  getsetpiece(Get, rfd, &d, (char *) datep, sizeof(*datep));
		else
		  {
		    datep->crdat = CL (UNIXTIMETOMACTIME (MIN (sbuf.st_ctime, sbuf.st_mtime)));
		    datep->moddat = CL (UNIXTIMETOMACTIME (sbuf.st_mtime));
		    datep->accessdat = CL (UNIXTIMETOMACTIME (sbuf.st_atime));
		    datep->backupdat = 0;
		  }

		if (!getsetentry(Get, rfd, Finder_Info_ID,   &d, NULL))
		  warning_unexpected ("no finder info");
		else
		  {
		    getsetpiece(Get, rfd, &d, (char *) &sfinfo, sizeof(sfinfo));

		    if (finfop)
		      *finfop  = sfinfo.finfo;
		    if (fxinfop)
		      *fxinfop = sfinfo.fxinfo;

		    *lenp  = CL(sbuf.st_size);
		  }
		if (getsetentry(Get, rfd, Resource_Fork_ID,   &d, NULL))
		    *rlenp = d.length;
		else
		    *rlenp = 0;
		if (ROMlib_basilisk_sidecar_p (rpathname))
		  {
		    char *resource = ROMlib_sidecar_resource_name (rpathname);
		    struct stat resource_stat;
		    *rlenp = resource && Ustat (resource, &resource_stat) == 0
		      ? CL (resource_stat.st_size) : 0;
		    free (resource);
		  }
		break;
	    case Set:
		if (getsetentry(Get, rfd, File_Dates_Info_ID, &d, NULL))
		  getsetpiece(Set, rfd, &d, (char *) datep, sizeof(*datep));

		if (!getsetentry(Get, rfd, Finder_Info_ID,   &d, NULL))
		  warning_unexpected ("no finfo");
		else
		  {
		    if (!finfop || !fxinfop) {
		      sfinfo.finfo.fdCreator = TICKX("UNIX");
		      sfinfo.finfo.fdType    = TICKX("TEXT");
		      getsetpiece(Get, rfd, &d, (char *) &sfinfo,
							       sizeof(sfinfo));
		    }

		    if (finfop)
		      sfinfo.finfo  = *finfop;
		    if (fxinfop)
		      sfinfo.fxinfo = *fxinfop;
		    getsetpiece(Set, rfd, &d, (char *) &sfinfo, sizeof(sfinfo));
		  }
		break;
	    default:
		gui_assert(0);
		break;
	    }
	}
	Uclose(rfd);
    }
    fs_err_hook (retval);
    return retval;
}
