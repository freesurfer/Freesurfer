/**
 * @file  xUtilities.c
 * @brief general purpose utils
 */
/*
 * Original Author: Kevin Teich
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2011/03/02 00:04:55 $
 *    $Revision: 1.11 $
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

#include "xUtilities.h"
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

char *xUtil_ksaErrorString[xUtil_tErr_knNumErrorCodes] = {"No error.", "Invalid error code."};

int xUtil_gCancelListening = 0;
int xUtil_gCancelUserCanceled = 0;

xUtil_tErr xUtil_BreakStringIntoPathAndStem(char *isPathAndStem, char *osPath, char *osStem)
{
  xUtil_tErr eResult = xUtil_tErr_NoError;
  char *sSection = "";
  char sPathSection[20][60];
  int nSection = 0;
  int nNumSections = 0;

  sSection = strtok(isPathAndStem, "/");
  if (NULL == sSection) {
    /* no path, for some reason */
    strcpy(osPath, "");
    strcpy(osStem, isPathAndStem);
    goto cleanup;
  }
  else {
    /* got the first section of the path, now get the rest. */
    strcpy(sPathSection[0], sSection);
    nSection = 1;
    while (NULL != (sSection = strtok(NULL, "/"))) {
      strcpy(sPathSection[nSection++], sSection);
    }

    /* the last part was the stem */
    strcpy(osStem, sPathSection[--nSection]);

    /* now build the path into a single string */
    nNumSections = nSection;
    strcpy(osPath, "");
    for (nSection = 0; nSection < nNumSections; nSection++) {
      sprintf(osPath, "%s/%s", osPath, sPathSection[nSection]);
    }
  }

  goto cleanup;

  goto error;
error:

  if (xUtil_tErr_NoError != eResult) {
    DebugPrint(("Error %d in xUtil_BreakStringIntoPathAndStem: %s\n", eResult, xUtil_GetErrorString(eResult)));
  }

cleanup:

  return eResult;
}

char *xUtil_GetErrorString(xUtil_tErr ieCode)
{
  xUtil_tErr eCode = ieCode;

  if (ieCode < 0 || ieCode >= xUtil_tErr_knNumErrorCodes) {
    eCode = xUtil_tErr_InvalidErrorCode;
  }

  return xUtil_ksaErrorString[eCode];
}

struct timeval sStartTime = {0, 0};
struct timeval sEndTime = {0, 0};

void xUtil_StartTimer() { gettimeofday(&sStartTime, NULL); }

void xUtil_StopTimer(char *isMessage)
{
  gettimeofday(&sEndTime, NULL);

  if (NULL != isMessage) {
    DebugPrint(("%s: %lu usec\n",
                isMessage,
                (sEndTime.tv_sec * 1000000 + sEndTime.tv_usec) - (sStartTime.tv_sec * 1000000 + sStartTime.tv_usec)));
  }
  else {
    DebugPrint(("Timer stopped: %lu usec\n",
                (sEndTime.tv_sec * 1000000 + sEndTime.tv_usec) - (sStartTime.tv_sec * 1000000 + sStartTime.tv_usec)));
  }
}

void xUtil_strcpy(char *ipDest, char *ipSrc) { strcpy(ipDest, ipSrc); }

void xUtil_strncpy(char *ipDest, char *ipSrc, int inSize)
{
  strncpy(ipDest, ipSrc, inSize);

  if (ipDest[inSize - 1] != '\0') {
    ipDest[inSize - 1] = '\0';

    DebugPrint(("xUtil_strncpy: Buffer overflow while %s (inSize=%d)\n", DebugGetNote, inSize));
    xDbg_PrintStack();
  }
}

void xUtil_sprintf(char *ipDest, char *isFormat, ...)
{
  va_list args;

  va_start(args, isFormat);
  vsprintf(ipDest, isFormat, args);
  va_end(args);
}

void xUtil_snprintf(char *ipDest, int inSize, char *isFormat, ...)
{
  va_list args;

  memset(ipDest, 0, inSize);

  va_start(args, isFormat);
#ifdef IRIX
  vsprintf(ipDest, isFormat, args);
#else
  vsnprintf(ipDest, inSize, isFormat, args);
#endif
  va_end(args);

  if (ipDest[inSize - 1] != '\0') {
    ipDest[inSize - 1] = '\0';

    DebugPrint(("xUtil_snprintf: Buffer overflow while %s (inSize=%d)\n", DebugGetNote, inSize));
    DebugPrintStack;
  }
}

void xUtil_InitializeUserCancel()
{
  /* init the flags and register our handler. */
  xUtil_gCancelListening = 0;
  xUtil_gCancelUserCanceled = 0;
  signal(SIGINT, xUtil_HandleUserCancelCallback);
}

void xUtil_StartListeningForUserCancel()
{
  /* set our listening flag. */
  xUtil_gCancelListening = 1;
}

void xUtil_StopListeningForUserCancel()
{
  /* stop listening and reset the canceled flag. */
  xUtil_gCancelListening = 0;
  xUtil_gCancelUserCanceled = 0;
}

int xUtil_DidUserCancel()
{
  /* just return the canceled flag. */
  return xUtil_gCancelUserCanceled;
}

void xUtil_HandleUserCancelCallback(int signal)
{
  /* if we're listening, set the flag, if not, exit normally. */
  if (xUtil_gCancelListening) {
    xUtil_gCancelUserCanceled = 1;
  }
  else {
    printf("Killed\n");
    fflush(stdout);
    exit(1);
  }
}
