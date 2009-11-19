/**
 * @file  mri_compile_edits.c
 * @brief program to find all edits made to a subject and write out
 *    a .mgz volume summarizing them.
 *
 * REPLACE_WITH_LONG_DESCRIPTION_OR_REFERENCE
 */
/*
 * Original Author: REPLACE_WITH_FULL_NAME_OF_CREATING_AUTHOR 
 * CVS Revision Info:
 *    $Author: fischl $
 *    $Date: 2009/11/19 15:12:04 $
 *    $Revision: 1.1 $
 *
 * Copyright (C) 2002-2007,
 * The General Hospital Corporation (Boston, MA). 
 * All rights reserved.
 *
 * Distribution, usage and copying of this software is covered under the
 * terms found in the License Agreement file named 'COPYING' found in the
 * FreeSurfer source code root directory, and duplicated here:
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferOpenSourceLicense
 *
 * General inquiries: freesurfer@nmr.mgh.harvard.edu
 * Bug reports: analysis-bugs@nmr.mgh.harvard.edu
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#include "mri.h"
#include "macros.h"
#include "error.h"
#include "diag.h"
#include "proto.h"
#include "mri.h"
#include "colortab.h"
#include "utils.h"
#include "const.h"
#include "timer.h"
#include "version.h"

int main(int argc, char *argv[]) ;
static int get_option(int argc, char *argv[]) ;

char *Progname ;
static void usage_exit(int code) ;

static char sdir[STRLEN] = "";

#define EDIT_WM_OFF           1
#define EDIT_WM_ON            2
#define EDIT_BRAIN_OFF        3
#define EDIT_BRAIN_ON         4
#define EDIT_BRAINMASK_OFF    5
#define EDIT_BRAINMASK_ON     6
#define EDIT_FINALSURFS_OFF   7
#define EDIT_FINALSURFS_ON    8
#define EDIT_ASEG_CHANGED     9
#define CTAB_ENTRIES          EDIT_ASEG_CHANGED+1

int
main(int argc, char *argv[]) {
  char         **av, fname[STRLEN] ;
  int          ac, nargs, i ;
  char         *subject, *cp, mdir[STRLEN], *out_fname, *name ;
  int          msec, minutes, seconds, r, g, b, nedits = 0 ;
  struct timeb start ;
  MRI          *mri, *mri_edits, *mri_aseg_auto ;

  /* rkt: check for and handle version tag */
  nargs = handle_version_option (argc, argv, "$Id: mri_compile_edits.c,v 1.1 2009/11/19 15:12:04 fischl Exp $", "$Name:  $");
  if (nargs && argc - nargs == 1)
    exit (0);
  argc -= nargs;

  Progname = argv[0] ;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  TimerStart(&start) ;

  ac = argc ;
  av = argv ;
  for ( ; argc > 1 && ISOPTION(*argv[1]) ; argc--, argv++) {
    nargs = get_option(argc, argv) ;
    argc -= nargs ;
    argv += nargs ;
  }

  if (argc < 2)
    usage_exit(1) ;

  if (strlen(sdir) == 0)
  {
    cp = getenv("SUBJECTS_DIR") ;
    if (cp == NULL)
      ErrorExit(ERROR_BADPARM, "%s: SUBJECTS_DIR must be defined in the env or on cmdline with -sdir", Progname) ;
    strcpy(sdir, cp) ;
  }

  subject = argv[1] ;
  out_fname = argv[2] ;
  printf("compiling edits for subject %s and saving to %s\n", subject, out_fname) ;

  sprintf(mdir, "%s/%s/mri", sdir, subject) ;

  sprintf(fname, "%s/brain.mgz", mdir) ;
  mri = MRIread(fname) ;
  if (mri == NULL)
    ErrorExit(ERROR_NOFILE, "%s: could not load volume %s", Progname, fname) ;
  mri_edits = MRIclone(mri, NULL) ;
  nedits += MRIsetVoxelsWithValue(mri, mri_edits, WM_EDITED_OFF_VAL, EDIT_BRAIN_OFF) ;
  nedits += MRIsetVoxelsWithValue(mri, mri_edits, WM_EDITED_ON_VAL, EDIT_BRAIN_ON) ;
  MRIfree(&mri) ;

  sprintf(fname, "%s/wm.mgz", mdir) ;
  mri = MRIread(fname) ;
  if (mri == NULL)
    ErrorExit(ERROR_NOFILE, "%s: could not load volume %s", Progname, fname) ;
  nedits += MRIsetVoxelsWithValue(mri, mri_edits, WM_EDITED_OFF_VAL, EDIT_WM_OFF) ;
  nedits += MRIsetVoxelsWithValue(mri, mri_edits, WM_EDITED_ON_VAL, EDIT_WM_ON) ;
  MRIfree(&mri) ;

  sprintf(fname, "%s/brainmask.mgz", mdir) ;
  mri = MRIread(fname) ;
  if (mri == NULL)
    ErrorExit(ERROR_NOFILE, "%s: could not load volume %s", Progname, fname) ;
  nedits += MRIsetVoxelsWithValue(mri, mri_edits, WM_EDITED_OFF_VAL, EDIT_BRAINMASK_OFF) ;
  nedits += MRIsetVoxelsWithValue(mri, mri_edits, WM_EDITED_ON_VAL, EDIT_BRAINMASK_ON) ;
  MRIfree(&mri) ;

  sprintf(fname, "%s/brain.finalsurfs.mgz", mdir) ;
  mri = MRIread(fname) ;
  if (mri == NULL)
    ErrorExit(ERROR_NOFILE, "%s: could not load volume %s", Progname, fname) ;
  nedits += MRIsetVoxelsWithValue(mri, mri_edits, WM_EDITED_OFF_VAL, EDIT_FINALSURFS_OFF) ;
  nedits += MRIsetVoxelsWithValue(mri, mri_edits, WM_EDITED_ON_VAL, EDIT_FINALSURFS_ON) ;
  MRIfree(&mri) ;

  sprintf(fname, "%s/aseg.mgz", mdir) ;
  mri = MRIread(fname) ;
  if (mri == NULL)
    ErrorExit(ERROR_NOFILE, "%s: could not load volume %s", Progname, fname) ;
  sprintf(fname, "%s/aseg.mgz", mdir) ;
  mri_aseg_auto = MRIread(fname) ;
  if (mri_aseg_auto == NULL)
    ErrorExit(ERROR_NOFILE, "%s: could not load volume %s", Progname, fname) ;
  nedits += MRIsetDifferentVoxelsWithValue(mri,mri_aseg_auto,mri_edits, EDIT_ASEG_CHANGED);
  MRIfree(&mri) ; MRIfree(&mri_aseg_auto) ;
  

  mri_edits->ct = CTABalloc(CTAB_ENTRIES) ;
  strcpy (mri_edits->fname, "mri_compile_edits");
  for (i = 0 ; i < CTAB_ENTRIES ; i++)
  {
    switch (i)
    {
    case EDIT_WM_OFF:
      name = "wm-OFF" ;
      r = 0 ; g = 0 ; b = 255 ;
      break ;
    case EDIT_WM_ON:
      name = "wm-ON" ;
      r = 255 ; g = 0 ; b = 0 ;
      break ;
    case EDIT_BRAIN_OFF:
      name = "brain-OFF" ;
      r = 0 ; g = 255 ; b = 255 ;
      break ;
    case EDIT_BRAIN_ON:
      name = "brain-ON" ;
      r = 255 ; g = 255 ; b = 0 ;
      break ;
    case EDIT_BRAINMASK_OFF:
      name = "brainmask-OFF" ;
      r = 0 ; g = 64 ; b = 255 ;
      break ;
    case EDIT_BRAINMASK_ON:
      name = "brainmask-ON" ;
      r = 255 ; g = 26 ; b = 0 ;
      break ;
    case EDIT_FINALSURFS_OFF:
      name = "brain.finalsurf-OFF" ;
      r = 0 ; g = 128 ; b = 255 ;
      break ;
    case EDIT_FINALSURFS_ON:
      name = "brain.finalsurfs-ON" ;
      r = 255 ; g = 128 ; b = 0 ;
      break ;
    case EDIT_ASEG_CHANGED:
      name = "aseg-CHANGED" ;
      r = 255 ; g = 255 ; b = 128 ;
      break ;
    default:
      continue ;
    }

    strcpy(mri_edits->ct->entries[i]->name, name) ;
    mri_edits->ct->entries[i]->ri = r ;
    mri_edits->ct->entries[i]->gi = g ;
    mri_edits->ct->entries[i]->bi = b ;
    mri_edits->ct->entries[i]->ai = 255;
    
    /* Now calculate the float versions. */
    mri_edits->ct->entries[i]->rf = (float)mri_edits->ct->entries[i]->ri / 255.0;
    mri_edits->ct->entries[i]->gf = (float)mri_edits->ct->entries[i]->gi / 255.0;
    mri_edits->ct->entries[i]->bf = (float)mri_edits->ct->entries[i]->bi / 255.0;
    mri_edits->ct->entries[i]->af = (float)mri_edits->ct->entries[i]->ai / 255.0;
  }

  printf("%d edits found, saving results to %s\n", nedits, out_fname) ;
  MRIwrite(mri_edits, out_fname);
  msec = TimerStop(&start) ;
  seconds = nint((float)msec/1000.0f) ;
  minutes = seconds / 60 ;
  seconds = seconds % 60 ;
  fprintf(stderr, "edit compilation took %d minutes"
          " and %d seconds.\n", minutes, seconds) ;
  exit(0) ;
  return(0) ;
}
/*----------------------------------------------------------------------
            Parameters:

           Description:
----------------------------------------------------------------------*/
static int
get_option(int argc, char *argv[]) {
  int  nargs = 0 ;
  char *option ;

  option = argv[1] + 1 ;            /* past '-' */
  if (!stricmp(option, "sdir"))
  {
    strcpy(sdir, argv[2]) ;
    nargs = 1;
    printf("using %s as SUBJECTS_DIR\n", sdir) ;
  }
  else switch (toupper(*option)) {
  case '?':
  case 'U':
    usage_exit(0) ;
    break ;
  default:
    fprintf(stderr, "unknown option %s\n", argv[1]) ;
    exit(1) ;
    break ;
  }

  return(nargs) ;
}
/*----------------------------------------------------------------------
            Parameters:

           Description:
----------------------------------------------------------------------*/
static void
usage_exit(int code) {
  printf("usage: %s [options] <inverse operator> <EEG/MEG data file>",
         Progname) ;
  printf(
    "\tf <f low> <f hi> - apply specified filter (not implemented yet)\n"
  );
  printf("\tn - noise-sensitivity normalize inverse (default=1)") ;
  exit(code) ;
}





