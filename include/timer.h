/**
 * @file  timer.h
 * @brief REPLACE_WITH_ONE_LINE_SHORT_DESCRIPTION
 *
 * REPLACE_WITH_LONG_DESCRIPTION_OR_REFERENCE
 */
/*
 * Original Author: REPLACE_WITH_FULL_NAME_OF_CREATING_AUTHOR 
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2011/03/02 00:04:10 $
 *    $Revision: 1.5 $
 *
 * Copyright © 2011 The General Hospital Corporation (Boston, MA) "MGH"
 *
 * Terms and conditions for use, reproduction, distribution and contribution
 * are found in the 'FreeSurfer Software License Agreement' contained
 * in the file 'LICENSE' found in the FreeSurfer distribution, and here:
 *
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferSoftwareLicense
 *
 * Reporting: freesurfer@nmr.mgh.harvard.edu
 *
 */


#ifndef TIMER_H
#define TIMER_H

#if defined(__cplusplus)
extern "C" {
#endif


#include <sys/timeb.h>

struct timeb *TimerStart(struct timeb *then) ;
int TimerStop(struct timeb *then) ;

typedef struct NanosecsTimer {  struct timespec now; } NanosecsTimer;
typedef struct Nanosecs { long ns; } Nanosecs;
void     TimerStartNanosecs  (NanosecsTimer * now) ;
Nanosecs TimerElapsedNanosecs(NanosecsTimer * then) ;	// returns delta in nanosecs

// mach_gettime is a replacement for clock_gettime (not available on osx < 10.12)
#if defined(__APPLE__) && !defined(HAVE_CLOCK_GETTIME)
  typedef int clockid_t;
  int mach_gettime(clockid_t clk_id, struct timespec *tp);
#endif

#ifdef Linux
/* don't know why this doesn't work on linux, but.... */
extern int ftime (struct timeb *__timebuf);
#endif

#define TIMER_INTERVAL_BEGIN(NAME) 		\
      	struct timeb NAME;			\
      	TimerStart(&NAME);
 
#define TIMER_INTERVAL_END(NAME)		\
	fprintf(stderr, "%s:%d interval took %d msec\n", __FILE__, __LINE__, TimerStop(&NAME));
	


// Use this string for writing into files etc. where it might be compared by
// the test system.  The string is usually the current date and time but
// can be overridden with a string that can the same across runs.
//
const char* current_date_time();

#if defined(__cplusplus)
};
#endif

#endif
