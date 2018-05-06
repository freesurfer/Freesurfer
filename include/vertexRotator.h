/**
 * @file  vertexRotator.h
 * @brief rapidly rotate vertexs around the x=0 y=0 z=0 axes
 *
 */
/*
 * Original Author: Bevin Brett
 * CVS Revision Info:
 *    $Author: bbrett $
 *    $Date: 2018/05/04 07:52:00 $
 *    $Revision: 1.0 $
 *
 * Copyright © 2018 The General Hospital Corporation (Boston, MA) "MGH"
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

#pragma once

#include "base.h"

void rotateVertices(
  float*         xv_out, float*       yv_out, float      * zv_out,
  float const *  xv_inp, float const* yv_inp, float const* zv_inp,
  size_t         nvertices,
  float          alpha,       // rotate around z axis - last rotation
  float          beta,        // rotate around y axis - middle rotation
  float          gamma);      // rotate around x axis - first rotation
