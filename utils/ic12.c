/**
 * @file  ic12.c
 * @brief ic0.tri - icosahedron
 *
 * in memory version of ic0.tri
 */
/*
 * Original Author: Bruce Fischl
 * CVS Revision Info:
 *    $Author: nicks $
 *    $Date: 2011/03/02 00:04:45 $
 *    $Revision: 1.2 $
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

#include <stdio.h>
#include "diag.h"
#include "error.h"
#include "icosahedron.h"
#include "mrisurf.h"

#define ICO_NVERTICES 12
// clang-format off
IC_VERTEX ic12_vertices[ICO_NVERTICES]
    = { { .0000, .0000, 1.0000 },  { .8944, .0000, .4472 },   { .2764, .8507, .4472 },    { -.7236, .5257, .4472 },
        { -.7236, -.5257, .4472 }, { .2764, -.8507, .4472 },  { .7236, -.5257, -.4472 },  { .7236, .5257, -.4472 },
        { -.2764, .8507, -.4472 }, { -.8944, .0000, -.4472 }, { -.2764, -.8507, -.4472 }, { .0000, .0000, -1.0000 } };

#define ICO_NFACES 20

IC_FACE ic12_faces[ICO_NFACES]
    = { { { 1, 5, 4 } },   { { 1, 6, 5 } },    { { 1, 2, 6 } },   { { 1, 3, 2 } },   { { 1, 4, 3 } },
        { { 4, 9, 3 } },   { { 4, 10, 9 } },   { { 4, 5, 10 } },  { { 5, 11, 10 } }, { { 5, 6, 11 } },
        { { 6, 7, 11 } },  { { 6, 2, 7 } },    { { 2, 8, 7 } },   { { 2, 3, 8 } },   { { 3, 9, 8 } },
        { { 9, 10, 12 } }, { { 10, 11, 12 } }, { { 11, 7, 12 } }, { { 7, 8, 12 } },  { { 8, 9, 12 } } };
// clang-format on
MRI_SURFACE *ic12_make_surface(int max_vertices, int max_faces)
{
  MRI_SURFACE *mris;
  int vno, fno, n, vn, n1, n2;
  static int first_time = 1;

  if (first_time) {
    first_time = 0;
    for (fno = 0; fno < ICO_NFACES; fno++) {
      vno = ic12_faces[fno].vno[1];
      ic12_faces[fno].vno[1] = ic12_faces[fno].vno[2];
      ic12_faces[fno].vno[2] = vno;
    }
  }

  mris = MRISoverAlloc(max_vertices, max_faces, ICO_NVERTICES, ICO_NFACES);

  /* position vertices */
  for (vno = 0; vno < ICO_NVERTICES; vno++) {
    VERTEX * const v = &mris->vertices[vno];

    v->x = 100.0 * ic12_vertices[vno].x;
    v->y = 100.0 * ic12_vertices[vno].y;
    v->z = 100.0 * ic12_vertices[vno].z;
  }

  /* fill in faces, and count # of faces each vertex is part of */
  for (fno = 0; fno < ICO_NFACES; fno++) {
    FACE * const f = &mris->faces[fno];
    if (fno == 15) DiagBreak();
    for (n = 0; n < VERTICES_PER_FACE; n++) {
      f->v[n] = ic12_faces[fno].vno[n] - 1; /* make it zero-based */
      VERTEX_TOPOLOGY * const vt = &mris->vertices_topology[f->v[n]];
      vt->num++;
      vt->vnum += 2; /* will remove duplicates later */
    }
  }

  for (vno = 0; vno < ICO_NVERTICES; vno++) {
    VERTEX_TOPOLOGY * const vt = &mris->vertices_topology[vno];
    vt->v = (int *)calloc(vt->vnum / 2, sizeof(int));
    if (!vt->v) ErrorExit(ERROR_NOMEMORY, "ic12: could not allocate %dth vertex list.", vno);
    vt->vnum = 0;
  }

  /* now build list of neighbors */
  for (fno = 0; fno < ICO_NFACES; fno++) {
    FACE * const f = &mris->faces[fno];
    if (fno == 3) DiagBreak();
    for (n = 0; n < VERTICES_PER_FACE; n++) {
      VERTEX_TOPOLOGY * const vt = &mris->vertices_topology[f->v[n]];

      /* now add an edge to other 2 vertices if not already in list */
      for (n1 = 0; n1 < VERTICES_PER_FACE; n1++) {
        if (n1 == n) /* don't connect vertex to itself */
          continue;
        vn = ic12_faces[fno].vno[n1] - 1; /* make it zero-based */

        /* now check to make sure it's not a duplicate */
        for (n2 = 0; n2 < vt->vnum; n2++) {
          if (vt->v[n2] == vn) {
            vn = -1; /* mark it as a duplicate */
            break;
          }
        }
        if (vn >= 0) vt->v[vt->vnum++] = vn;
      }
    }
  }

  /* now allocate face arrays in vertices */
  for (vno = 0; vno < ICO_NVERTICES; vno++) {
    VERTEX_TOPOLOGY * const vt = &mris->vertices_topology[vno];
    VERTEX          * const v  = &mris->vertices         [vno];
    vt->f = (int *)calloc(vt->num, sizeof(int));
    if (!vt->f) ErrorExit(ERROR_NO_MEMORY, "ic12: could not allocate %d faces", vt->num);
    vt->n = (uchar *)calloc(vt->num, sizeof(uchar));
    if (!vt->n) ErrorExit(ERROR_NO_MEMORY, "ic12: could not allocate %d nbrs", vt->n);
    vt->num = 0; /* for use as counter in next section */
    v->dist = (float *)calloc(vt->vnum, sizeof(float));
    if (!v->dist)
      ErrorExit(ERROR_NOMEMORY,
                "mrisFindNeighbors: could not allocate list of %d "
                "dists at v=%d",
                vt->vnum,
                vno);
    v->dist_orig = (float *)calloc(vt->vnum, sizeof(float));
    if (!v->dist_orig)
      ErrorExit(ERROR_NOMEMORY,
                "mrisFindNeighbors: could not allocate list of %d "
                "dists at v=%d",
                vt->vnum,
                vno);
    vt->nsize = 1;
    vt->vtotal = vt->vnum;
  }

  /* fill in face indices in vertex structures */
  for (fno = 0; fno < ICO_NFACES; fno++) {
    FACE const * const f = &mris->faces[fno];
    for (n = 0; n < VERTICES_PER_FACE; n++) {
      VERTEX_TOPOLOGY * const vt = &mris->vertices_topology[f->v[n]];
      vt->n[vt->num] = n;
      vt->f[vt->num++] = fno;
    }
  }

  MRIScomputeMetricProperties(mris);
#if 0
  for (fno = 0 ; fno < mris->nfaces ; fno++)
  {
    float dot ;
    int   ano ;

    FACE * const f = &mris->faces[fno] ;
    if (f->ripflag)
      continue ;

    /* now give the area an orientation: if the unit normal is pointing
       inwards on the ellipsoid then the area should be negative.
       */
    VERTEX const * const v = &mris->vertices[f->v[0]] ;
    dot = v->x * f->nx + v->y * f->ny + v->z * f->nz;
    if (dot < 0.0f)   /* not in same direction, area < 0 and reverse n */
    {
      f->area *= -1.0f ;
      f->nx *= -1.0f;
      f->ny *= -1.0f;
      f->nz *= -1.0f;
      for (ano = 0 ; ano < ANGLES_PER_TRIANGLE ; ano++)
        f->angle[ano] *= -1.0f ;
    }
  }
#endif
  mris->type = MRIS_ICO_SURFACE;
  MRISsetNeighborhoodSize(mris, 1);
  return (mris);
}
