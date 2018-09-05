/*
 * @file utilities operating on Original
 *
 */
/*
 * surfaces Author: Bruce Fischl, extracted from mrisurf.c by Bevin Brett
 *
 * $ © copyright-2014,2018 The General Hospital Corporation (Boston, MA) "MGH"
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
#include "mrisurf_topology.h"


/*-----------------------------------------------------
  This supports code that accelerates finding the vertices and faces needed during defect correction.
  To do this, it must be able to tell when vertex orig[xyz] are changed.
  Such changes need to be reported via noteVnoMovedInActiveRealmTrees.
  To help decide where such calls had to be added, all changes to origx etc. that are not have a CHANGES_ORIG by them to check if should have been.
  To test it is correct, the code can scan all vertices of an mris and verify there origxyz are what was expected.
  ------------------------------------------------------*/

int mrisCheckSurfaceNbrs(MRI_SURFACE *mris)
{
  int vno, n, n2, found;

  return (1);
  
  for (vno = 0; vno < mris->nvertices; vno++) {
    VERTEX_TOPOLOGY const * const vt = &mris->vertices_topology[vno];

    for (n = 0; n < vt->vnum; n++) {
      VERTEX_TOPOLOGY const * const vnt = &mris->vertices_topology[vt->v[n]];
      found = 0;
      for (n2 = 0; n2 < vnt->vnum; n2++)
        if (vnt->v[n2] == vno) {
          found = 1;
          break;
        }
      if (found == 0) {
        DiagBreak();
        return (0);
      }
    }
  }
  return (1);
}

short FACE_vertexIndex_find(FACE *pFace, int avertex)
{
  //
  // PRECONDITIONS
  // o <avertex> denotes a vertex number to lookup in the <pFace>.
  //
  // POSTCONDITIONS
  // o The vertex index (0, 1, 2) containing the <avertex> is returned
  //   or -1 if not found.
  //
  int vertex = 0;
  int ret = -1;
  for (vertex = 0; vertex < VERTICES_PER_FACE; vertex++)
    if (pFace->v[vertex] == avertex) {
      ret = vertex;
    }
  return ret;
}

int FACE_vertexIndexAtMask_find(FACE *apFACE_I, VECTOR *apv_verticesCommon)
{
  //
  // PRECONDITIONS
  //  o Called after <apv_verticesCommon> has been processed by
  //    VERTICES_commonInFaces_find().
  //
  // POSTCONDITIONS
  //  o The vertex index in <apFACE_I> corresponding to the '-1'
  //    in <apv_verticesCommon> is returned. For two bordering faces
  //    that share an edge defined by <apv_verticesCommon>, this function
  //    determines the index of the vertex that is *not* on this shared
  //    edge.
  //
  // HISTORY
  // 03 July 2007
  //  o Initial design and coding.
  //

  int face = 0;
  int vertex = 0;
  short b_inCommon = 0;
  int ret = -1;

  for (face = 0; face < 3; face++) {
    vertex = apFACE_I->v[face];
    b_inCommon = VECTOR_elementIndex_find(apv_verticesCommon, (float)vertex);
    if (!b_inCommon) {
      ret = vertex;
      break;
    }
  }

  return ret;
}

short VERTICES_commonInFaces_find(FACE *apFACE_I, FACE *apFACE_J, VECTOR *apv_verticesCommon)
{
  //
  // PRECONDITIONS
  //  o The <apFACE>s must be triangles with 3 vertices each.
  //  o It is assumed (but not mandatory) that the FACES share
  //    at least one common vertex - or more often a common
  //    edge, i.e. two common vertices.
  //  o The <apv_uncommon> VECTOR's memory must be managed by the
  //    caller, i.e. created and freed, and should be a 3x1 VECTOR.
  //
  // POSTCONDITIONS
  //  o The number of vertices that are in common between the two
  //    faces are returned in the function name. This can be either
  //    (0, 1, 2, 3).
  //  o The indices of the common vertices are returned in
  //    apv_verticesCommon. This is a three element vector, with each
  //    element corresponding to a common vertex. Vertex indices that
  //    are not common between the two faces have a -1.
  //

  int i = 0;
  int j = 0;
  int k = 0;
  char *pch_function = "VERTICES_commonInFaces_find";
  short b_hit = 0;
  float f_val = -1.;

  if (apv_verticesCommon->rows != 3 || apv_verticesCommon->cols != 1) {
    ErrorExit(-1, "%s: Return VECTOR must be 3x1.\n", pch_function);
  }
  V3_LOAD(apv_verticesCommon, -1, -1, -1);

  for (i = 0; i < 3; i++) {
    b_hit = 0;
    f_val = -1.;
    for (j = 0; j < 3; j++) {
      if (apFACE_J->v[j] == apFACE_I->v[i]) {
        b_hit = 1;
      }
    }
    if (b_hit) {
      f_val = apFACE_I->v[i];
      VECTOR_ELT(apv_verticesCommon, ++k) = f_val;
    }
  }
  return k;
}

int MRIS_facesAtVertices_reorder(MRIS *apmris)
{
  //
  // PRECONDITIONS
  //  o <apmris> is a valid surface.
  //
  // POSTCONDITIONS
  //  o The 'f' FACE array at each vertex has its indices reordered
  //    so that bordering face indices index (i) and index (i+1)
  //    correspond to the actual geometric order of the faces about
  //    each vertex.
  //  o Note that the 'f' FACE array is changed at each vertex by
  //    this function.
  //
  // HISTORY
  //  02 July 2007
  //  o Initial design and coding.
  //

  int vertex = 0;
  int face = 0;
  int nfaces = 0;
  int orderedIndex = -1;
  int orderedFace = -1;
  VECTOR *pv_geometricOrderIndx = NULL;
  VECTOR *pv_logicalOrderFace = NULL;
  int ret = 1;
  char *pch_function = "MRIS_facesAtVertices_reorder";

  DebugEnterFunction((pch_function));
  fprintf(stderr, "\n");
  for (vertex = 0; vertex < apmris->nvertices; vertex++) {
    MRIS_vertexProgress_print(apmris, vertex, "Determining geometric order for vertex faces...");
    VERTEX_TOPOLOGY const * const pVERTEXt = &apmris->vertices_topology[vertex];
    VERTEX                * const pVERTEX  = &apmris->vertices         [vertex];
    nfaces = pVERTEXt->num;
    pv_geometricOrderIndx = VectorAlloc(nfaces, MATRIX_REAL);
    pv_logicalOrderFace = VectorAlloc(nfaces, MATRIX_REAL);
    ret = FACES_aroundVertex_reorder(apmris, vertex, pv_geometricOrderIndx);
    if (ret < 0) {
      pVERTEX->marked = 1;
      continue;
    }
    for (face = 0; face < nfaces; face++) {
      VECTOR_ELT(pv_logicalOrderFace, face + 1) = pVERTEXt->f[face];
    }
    for (face = 0; face < nfaces; face++) {
      orderedIndex = VECTOR_ELT(pv_geometricOrderIndx, face + 1);
      orderedFace = VECTOR_ELT(pv_logicalOrderFace, orderedIndex + 1);
      pVERTEXt->f[face] = orderedFace;
    }
    VectorFree(&pv_geometricOrderIndx);
    VectorFree(&pv_logicalOrderFace);
  }
  MRISdilateMarked(apmris, 1);  // neighbors of vertices we couldn't process are also suspect and should be skipped
  xDbg_PopStack();
  return ret;
}

int MRIScomputeGeometricProperties(MRIS *apmris)
{
  //
  // PRECONDITIONS
  //  o Needs to be called before computing discrete curvatures.
  //
  // POSTCONDITIONS
  //  o The face array at each vertex is re-ordered in a geometric sense.
  //  o Each pair of bordering faces at each vertex are processed to
  //    to determine overall convexity/concavity of "node".
  //
  // HISTORY
  // 03 July 2007
  //  o Initial design and coding.
  //

  int ret = 0;
  ret = MRIS_facesAtVertices_reorder(apmris);
  return ret;
}

short FACES_aroundVertex_reorder(MRIS *apmris, int avertex, VECTOR *pv_geometricOrder)
{
  //
  // PRECONDITIONS
  //  o <avertex> is a valid vertex on the surface.
  //  o <pll_faces> should not be allocated.
  //
  // POSTCONDITIONS
  //  o The face indices about vertex <avertex>, starting with face 0
  //    are returned in geometric order in the <pv_geometricOrder> vector.
  //    By geometric order is implied that the "next" index denotes
  //    the next face that directly borders the current face.
  //  o The number of connected faces is returned in the function name, or
  //    0 is there is some error.
  //
  // HISTORY
  //  25 June 2007
  //  o Initial design and coding.
  //

  char *pch_function = "FACES_aroundVertex_reorder";
  int nfaces = 0;
  int *pFaceIndex = NULL;
  int packedCount = 1;
  int i = 0;
  int I = 0;
  int j = 0;
  int k = 0;
  FACE *pFACE_I;
  FACE *pFACE_J;
  VECTOR *pv_commonVertices = NULL;  // Vector housing vertices that
  // are common between two
  // neighbouring faces.
  int commonVertices = 0;  // number of vertices in common
  // between two faces
  short b_borderFound = 0;

  pv_commonVertices = VectorAlloc(3, MATRIX_REAL);
  VERTEX_TOPOLOGY const * const pVERTEXt = &apmris->vertices_topology[avertex];
  nfaces     = pVERTEXt->num;
  pFaceIndex = pVERTEXt->f;

  DebugEnterFunction((pch_function));

  if (!nfaces) {
    fprintf(stderr,
            "\n\nFATAL ERROR encountered in function ''%s'.\nMesh structural error. Vertex %d has no faces.\n\n",
            pch_function,
            avertex);
    exit(1);
  }

  for (i = 1; i <= nfaces; i++) {
    VECTOR_ELT(pv_geometricOrder, i) = -1;
    pFACE_I = &apmris->faces[pFaceIndex[i - 1]];
  }
  VECTOR_ELT(pv_geometricOrder, 1) = 0;
  for (i = 0; i < nfaces; i++) {
    if (packedCount == nfaces) {
      break;
    }
    I = VECTOR_ELT(pv_geometricOrder, i + 1);
    pFACE_I = &apmris->faces[pFaceIndex[I]];
    for (j = 0; j < nfaces; j++) {
      k = (i + j) % nfaces;
      pFACE_J = &apmris->faces[pFaceIndex[k]];
      commonVertices = VERTICES_commonInFaces_find(pFACE_I, pFACE_J, pv_commonVertices);
      if (commonVertices == 2) {
        if (!VECTOR_elementIndex_find(pv_geometricOrder, k)) {
          VECTOR_ELT(pv_geometricOrder, i + 2) = k;
          b_borderFound = 1;
          packedCount++;
          break;
        }
      }
    }
    if (j == nfaces) DiagBreak();
  }
  VectorFree(&pv_commonVertices);
  xDbg_PopStack();
  if (packedCount != nfaces)
    ErrorReturn(-4,
                (-4,
                 "%s: packed / faces mismatch; vertex = %d, faces = %d, packed = %d",
                 pch_function,
                 avertex,
                 nfaces,
                 packedCount));

  return 1;
}


int MRISisSurfaceValid(MRIS *mris, int patch, int verbose)
{
  int n, m, p, q, vnop, nfound, mark;
  int euler, nedges, nvf, nffm, nffs, cf, nvc;
  FACE *fm;

  // check if every vertex has the same number of vertices and faces
  if (verbose == 2) {
    fprintf(stderr, "\nchecking for border faces...\n");
  }
  nvf = 0;
  for (n = 0; n < mris->nvertices; n++) {
    VERTEX_TOPOLOGY const * const vn = &mris->vertices_topology[n];
    if (vn->vnum != vn->num) {
      nvf++;
      if (verbose == 2 && patch == 0) {
        fprintf(stderr, "vertex %d : vnum = %d num=%d\n", n, vn->vnum, vn->num);
      }
    }
  }

  // check if some faces have more than 2 neighbors
  if (verbose == 2) {
    fprintf(stderr, "\nchecking for single or multiple faces...\n");
  }
  nffm = nffs = 0;
  nedges = 0;
  for (n = 0; n < mris->nvertices; n++) {
    VERTEX_TOPOLOGY const * const vn = &mris->vertices_topology[n];
    nedges += vn->vnum;
    for (p = 0; p < vn->vnum; p++) {
      vnop = vn->v[p];
      /* counts the # of faces that contain the edge n<-->vnop */
      cf = 0;
      for (m = 0; m < vn->num; m++) {
        fm = &mris->faces[vn->f[m]];
        // check if fm contains vnop
        for (q = 0; q < 3; q++) {
          if (fm->v[q] == vnop) {
            cf++;
            break;
          }
        }
      }
      if (cf < 2) {
        nffs++;
      }
      if (cf > 2) {
        nffm++;
      }
      if (verbose == 2) {
        if (cf < 2 && patch == 0) {
          fprintf(stderr, "edge %d <--> %d : single face\n", n, vnop);
        }
        if (cf > 2) {
          fprintf(stderr, "edge %d <--> %d : multiple face\n", n, vnop);
        }
      }
    }
  }
  nedges /= 2;
  euler = mris->nvertices - nedges + mris->nfaces;

  // finally check the # of connected neighbors of each vertex
  if (verbose == 2) {
    fprintf(stderr, "\nchecking for corner configurations...\n");
  }
  nvc = 0;
  for (n = 0; n < mris->nvertices; n++) {
    VERTEX_TOPOLOGY const * const vn = &mris->vertices_topology[n];
    for (p = 0; p < vn->vnum; p++) {
      vnop = vn->v[p];
      mris->vertices[vnop].marked = 0;
    }
    // mark first vertex
    vnop = vn->v[0];
    mris->vertices[vnop].marked = 1;
    // find connected neighbors
    nfound = 1;
    while (nfound) {
      nfound = 0;
      for (m = 0; m < vn->num; m++) {
        fm = &mris->faces[vn->f[m]];
        // check if fm contains a marked vertex
        mark = 0;
        for (q = 0; q < 3; q++) {
          if (fm->v[q] == n) {
            continue;
          }
          if (mris->vertices[fm->v[q]].marked) {
            mark = 1;
            break;
          }
        }
        if (mark) {
          for (q = 0; q < 3; q++) {
            if (fm->v[q] == n) {
              continue;
            }
            if (mris->vertices[fm->v[q]].marked) {
              continue;
            }
            mris->vertices[fm->v[q]].marked = 1;
            nfound = 1;
            break;
          }
        }
      }
    }
    // now check if there is a remaining vertex that is not marked
    for (p = 0; p < vn->vnum; p++) {
      vnop = vn->v[p];
      if (mris->vertices[vnop].marked == 0) {
        nvc++;
        if (verbose == 2) {
          fprintf(stderr, "vertex %d : corner configuration\n", n);
        }
        break;
      }
    }
  }
  if (verbose)
    fprintf(stderr,
            "Surface Diagnostics:       \n"
            "   eno=%d (nv=%d, nf=%d, ne=%d)\n"
            "   # of border vertices [ #v ~ #f ]     %d \n"
            "   # of edges with single face          %d \n"
            "   # of edges with more than 2 faces    %d \n"
            "   # of corner configurations           %d\n",
            euler,
            mris->nvertices,
            mris->nfaces,
            nedges,
            nvf,
            nffs,
            nffm,
            nvc);

  if (patch && (nffm || nvc)) {
    return 0;
  }
  else if (nvf || nffs || nffm || nvc) {
    return 0;
  }
  return 1;
}


/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  Is vno a vertex of face fno?
  ------------------------------------------------------*/
int vertexInFace(MRI_SURFACE *mris, int vno, int fno)
{
  VERTEX_TOPOLOGY const * const v = &mris->vertices_topology[vno];

  int n;
  for (n = 0; n < v->num; n++)
    if (v->f[n] == fno) {
      return (1);
    }
  return (0);
}

int edgeExists(MRI_SURFACE *mris, int vno1, int vno2)
{
  int n;
  VERTEX_TOPOLOGY const * const v = &mris->vertices_topology[vno1];
  for (n = 0; n < v->vnum; n++)
    if (v->v[n] == vno2) {
      return (1);
    }
  return (0);
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  See if any of the vertices in a triangle are marked
  ------------------------------------------------------*/
int triangleMarked(MRI_SURFACE *mris, int fno)
{
  int n;
  FACE *f;

  f = &mris->faces[fno];
  for (n = 0; n < VERTICES_PER_FACE; n++) {
    if (mris->vertices[f->v[n]].marked != 0) {
      return (1);
    }
  }
  return (0);
}


int findOtherEdgeFace(MRIS const *mris, int fno, int vno, int vn1)
{
  int n, m;

  VERTEX_TOPOLOGY const * const v1 = &mris->vertices_topology[vno];
  VERTEX_TOPOLOGY const * const v2 = &mris->vertices_topology[vn1];
  for (n = 0; n < v1->num; n++) {
    if (v1->f[n] == fno) {
      continue;
    }
    for (m = 0; m < v2->num; m++)
      if (v1->f[n] == v2->f[m]) {
        return (v1->f[n]);
      }
  }

  // fprintf(WHICH_OUTPUT,"edge (%d<-->%d) does not have two faces\n",vno,vn1);

  return fno;
}


int findNonMarkedFace(MRIS *mris, int vno, int vn1)
{
  int i, nf;
  int fn;
  FACE *f;

  // test
  if (vno < 0 || vno >= mris->nfaces || vn1 < 0 || vn1 >= mris->nfaces) {
    fprintf(stderr, "error in findNonmarkedFace\n");
    return -1;
  }
  VERTEX_TOPOLOGY const * const v = &mris->vertices_topology[vno];
  for (nf = 0; nf < v->num; nf++) {
    fn = v->f[nf];
    f = &mris->faces[fn];
    if (f->marked) {
      continue;
    }
    // check if correct face
    for (i = 0; i < 3; i++) {
      if (f->v[i] == vno) {
        continue;
      }
      if (f->v[i] == vn1) {
        return fn;
      }
    }
  }
  fprintf(stderr, "could not find other face in findNonMarkedFace\n");
  return -1;
}


/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  Add the edge vnot <--> vno2 to the tessellation
  ------------------------------------------------------*/
#define MAX_VLIST 255
int mrisAddEdge(MRI_SURFACE *mris, int vno1, int vno2)
{
  int vlist[MAX_VLIST];

  if (vno1 < 0 || vno2 < 0) {
    DiagBreak();
  }
  if (vno1 == Gdiag_no || vno2 == Gdiag_no) {
    DiagBreak();
  }

  if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON) {
    fprintf(stdout, "adding edge %d <--> %d\n", vno1, vno2);
  }

  /* add v2 link to v1 struct */
  {
    VERTEX_TOPOLOGY * const v = &mris->vertices_topology[vno1];
    if (v->vnum >= MAX_VLIST - 1) {
      ErrorExit(ERROR_NOMEMORY, "mrisAddEdge: too many edges (%d)", v->vnum);
    }

    memmove(vlist, v->v, v->vnum * sizeof(int));
    vlist[(unsigned int)v->vnum++] = vno2;
    v->vtotal = v->vnum;
    if (v->v) {
      free(v->v);
    }
    v->v = (int *)calloc(v->vnum, sizeof(int));
    if (!v->v) ErrorExit(ERROR_NO_MEMORY, "mrisAddEdge(%d, %d): could not allocate %d len vlist", v->vnum);

    memmove(v->v, vlist, v->vnum * sizeof(int));
  }
  
  /* add v1 link to v2 struct */
  {
    VERTEX_TOPOLOGY * const v = &mris->vertices_topology[vno2];
    memmove(vlist, v->v, v->vnum * sizeof(int));
    vlist[(unsigned int)v->vnum++] = vno1;
    v->vtotal = v->vnum;
    if (v->v) {
      free(v->v);
    }
    v->v = (int *)calloc(v->vnum, sizeof(int));
    if (!v->v) ErrorExit(ERROR_NO_MEMORY, "mrisAddEdge(%d, %d): could not allocate %d len vlist", v->vnum);

    memmove(v->v, vlist, v->vnum * sizeof(int));
  }
  
  return (NO_ERROR);
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  Is the triangle vno0-->vno1-->vno2 already in the tessellation
  ------------------------------------------------------*/
int isFace(MRI_SURFACE *mris, int vno0, int vno1, int vno2)
{
  FACE *f;
  int n, n1, vno;

  VERTEX_TOPOLOGY const * const vt = &mris->vertices_topology[vno0];
  for (n = 0; n < vt->num; n++) {
    f = &mris->faces[vt->f[n]];
    for (n1 = 0; n1 < VERTICES_PER_FACE; n1++) {
      vno = f->v[n1];
      if (vno != vno0 && vno != vno1 && vno != vno2) {
        break;
      }
    }
    if (n1 >= VERTICES_PER_FACE) {
      return (1);
    }
  }
  return (0);
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  Is the triangle vno0-->vno1-->vno2 already in the tessellation
  ------------------------------------------------------*/
int findFace(MRI_SURFACE *mris, int vno0, int vno1, int vno2)
{
  FACE *f;
  int n, n1, vno, fno;

  VERTEX_TOPOLOGY const * const vt = &mris->vertices_topology[vno0];
  for (n = 0; n < vt->num; n++) {
    f = &mris->faces[vt->f[n]];
    for (n1 = 0; n1 < VERTICES_PER_FACE; n1++) {
      vno = f->v[n1];
      fno = vt->f[n];
      if (vno != vno0 && vno != vno1 && vno != vno2) {
        break;
      }
    }
    if (n1 >= VERTICES_PER_FACE) {
      return (fno);
    }
  }
  return (-1);
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  Search the face for vno and set the v->n[] field
  appropriately.
  ------------------------------------------------------*/
int mrisSetVertexFaceIndex(MRI_SURFACE *mris, int vno, int fno)
{
  FACE *f;
  int n, i;

  VERTEX_TOPOLOGY const * const vt = &mris->vertices_topology[vno];
  f = &mris->faces[fno];

  for (n = 0; n < VERTICES_PER_FACE; n++) {
    if (f->v[n] == vno) {
      break;
    }
  }
  if (n >= VERTICES_PER_FACE) {
    return (ERROR_BADPARM);
  }

  for (i = 0; i < vt->num; i++)
    if (vt->f[i] == fno) {
      vt->n[i] = n;
    }

  return (n);
}

int computeOrientation(MRIS *mris, int f, int v0, int v1)
{
  FACE *face;

  face = &mris->faces[f];
  if (face->v[0] == v0) {
    if (face->v[1] == v1) {
      return 1;
    }
    else {
      return -1;
    }
  };
  if (face->v[1] == v0) {
    if (face->v[2] == v1) {
      return 1;
    }
    else {
      return -1;
    }
  };
  if (face->v[0] == v1) {
    return 1;
  }
  else {
    return -1;
  }
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int mrisMarkBadEdgeVertices(MRI_SURFACE *mris, int mark)
{
  int vno, n, nfaces, m, vno2, nmarked;

  for (nmarked = vno = 0; vno < mris->nvertices; vno++) {
    VERTEX_TOPOLOGY const * const vt = &mris->vertices_topology[vno];
    VERTEX                * const v  = &mris->vertices         [vno];
    
    if (v->ripflag) {
      continue;
    }
    if (vno == Gdiag_no) {
      DiagBreak();
    }
    for (n = 0; n < vt->vnum; n++) {
      vno2 = vt->v[n];
      if (vno2 < vno) {
        continue;
      }
      for (nfaces = m = 0; m < vt->vnum; m++) {
        if (vt->v[m] == vno2) {
          continue;
        }
        if (vertexNeighbor(mris, vno2, vt->v[m]) && isFace(mris, vno, vno2, vt->v[m])) {
          nfaces++;
        }
      }
      if (nfaces != 2) {
        v->marked = mark;
        mris->vertices[vno2].marked = mark;
        nmarked += 2;
        break;
      }
    }
  }
  return (nmarked);
}
/*! -----------------------------------------------------
  \fn int MRISdilateMarked(MRI_SURFACE *mris, int ndil)
  \brief Dilates the marked vertices by marking a vertex
  if any of its non-ripped neighbors is ripped.
  ------------------------------------------------------*/
int MRISdilateMarked(MRI_SURFACE *mris, int ndil)
{
  int vno, i, n, mx;

  // Loop through each dilation
  for (i = 0; i < ndil; i++) {

    // Set v->tx to 0 for unripped vertices
    for (vno = 0; vno < mris->nvertices; vno++) {
      VERTEX * const v = &mris->vertices[vno];
      if(v->ripflag) continue;
      v->tx = 0;
    }

    // Loop through vertices (skip ripped)
    for (vno = 0; vno < mris->nvertices; vno++) {
      VERTEX_TOPOLOGY const * const vt = &mris->vertices_topology[vno];
      VERTEX                * const v  = &mris->vertices         [vno];
      if(v->ripflag) continue;
      // set v->tx=1 if this vertex or any of its neightbors is marked
      mx = v->marked;
      for (n = 0; n < vt->vnum; n++) {
        VERTEX const * const vn = &mris->vertices[vt->v[n]];
        mx = MAX(vn->marked, mx);
      }
      v->tx = mx;
    }

    // Now copy tx into marked
    for (vno = 0; vno < mris->nvertices; vno++) {
      VERTEX * const v = &mris->vertices[vno];
      if (v->ripflag) continue;
      v->marked = (int)v->tx;
    }

  }// end loop over dilations
  return (NO_ERROR);
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int MRISdilateRipped(MRI_SURFACE *mris, int ndil)
{
  int vno, i, n;

  for (i = 0; i < ndil; i++) {
    for (vno = 0; vno < mris->nvertices; vno++) {
      VERTEX * const v = &mris->vertices[vno];
      v->tx = v->ripflag;
    }

    for (vno = 0; vno < mris->nvertices; vno++) {
      VERTEX_TOPOLOGY const * const vt = &mris->vertices_topology[vno];
      VERTEX                * const v  = &mris->vertices         [vno];
      
      if (v->ripflag == 0) {
        continue;
      }

      // turn on ripflag of all neighbors of this (ripped) vertex
      for (n = 0; n < vt->vnum; n++) {
        VERTEX * const vn = &mris->vertices[vt->v[n]];
        if (vn->ripflag == 0) {
          vn->tx = 1;
        }
      }
    }
    for (vno = 0; vno < mris->nvertices; vno++) {
      VERTEX * const v = &mris->vertices[vno];
      v->ripflag = (int)v->tx;
    }
  }
  MRISripFaces(mris);
  return (NO_ERROR);
}
/*---------------------------------------------------------------------
  MRI *MRISdilateConfined() - dilates surface mask niters iterations.
  If annotidmask >= 0, then dilation is confined to the annotidmask
  annotation. If newid >= 0, then the surface annot field of vertices
  in the dilated mask is set to the annot corresponding to newid.
  -------------------------------------------------------------------*/
MRI *MRISdilateConfined(MRIS *surf, MRI *mask, int annotidmask, int niters, int newid)
{
  int vtxno, annot, annotid, nnbrs, nbrvtxno, nthnbr, nthiter, new_annot;
  MRI *mri1, *mri2;
  float val;

  mri1 = MRIcopy(mask, NULL);
  mri2 = MRIcopy(mask, NULL);

  for (nthiter = 0; nthiter < niters; nthiter++) {
    // printf("iter %d\n",nthiter);

    for (vtxno = 0; vtxno < surf->nvertices; vtxno++) {

      /* Set to 0 if not in annotidmask (ie, dont dilate outside
      of annotidmask (if it is set) */
      if (annotidmask > -1) {
        annot = surf->vertices[vtxno].annotation;
        CTABfindAnnotation(surf->ct, annot, &annotid);
        if (annotid != annotidmask) {
          MRIsetVoxVal(mri2, vtxno, 0, 0, 0, 0);
          continue;
        }
      }

      // Check whether this vertex has been set
      val = MRIgetVoxVal(mri1, vtxno, 0, 0, 0);
      if (val) {
        MRIsetVoxVal(mri2, vtxno, 0, 0, 0, 1);
        continue;
      }

      // If it gets here, the vtx is in the annot and has not been set
      nnbrs = surf->vertices_topology[vtxno].vnum;
      for (nthnbr = 0; nthnbr < nnbrs; nthnbr++) {
        nbrvtxno = surf->vertices_topology[vtxno].v[nthnbr];
        if (surf->vertices[nbrvtxno].ripflag) {
          continue;  // skip ripped vtxs
        }
        val = MRIgetVoxVal(mri1, nbrvtxno, 0, 0, 0);
        if (val) {
          MRIsetVoxVal(mri2, vtxno, 0, 0, 0, 1);
          continue;
        }
      }
    }
    MRIcopy(mri2, mri1);
  }

  MRIfree(&mri2);
  // MRIwrite(mri1,"mymask2.mgh");

  if (newid > 0) {
    // Set annots in this mask to the given annot
    CTABannotationAtIndex(surf->ct, newid, &new_annot);
    for (vtxno = 0; vtxno < surf->nvertices; vtxno++) {
      if (MRIgetVoxVal(mri1, vtxno, 0, 0, 0)) {
        surf->vertices[vtxno].annotation = new_annot;
      }
    }
  }

  return (mri1);
}


/*-----------------------------------------------------
  Parameters:
  Returns value:
  Description
  ------------------------------------------------------*/
int MRISerodeRipped(MRI_SURFACE *mris, int ndil)
{
  int vno, i, n, mn;

  for (i = 0; i < ndil; i++) {
    for (vno = 0; vno < mris->nvertices; vno++) {
      VERTEX * const v = &mris->vertices[vno];
      v->tx = 0;
    }

    for (vno = 0; vno < mris->nvertices; vno++) {
      VERTEX_TOPOLOGY const * const vt = &mris->vertices_topology[vno];
      VERTEX                * const v  = &mris->vertices         [vno];
      mn = v->ripflag;
      for (n = 0; n < vt->vnum; n++) {
        VERTEX * const vn = &mris->vertices[vt->v[n]];
        mn = MIN(vn->ripflag, mn);
      }
      v->tx = mn;
    }
    for (vno = 0; vno < mris->nvertices; vno++) {
      VERTEX * const v = &mris->vertices[vno];
      v->ripflag = (int)v->tx;
    }
  }
  MRISripFaces(mris);
  return (NO_ERROR);
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int MRISerodeMarked(MRI_SURFACE *mris, int num)
{
  int vno, i, n, mn;

  for (i = 0; i < num; i++) {
    for (vno = 0; vno < mris->nvertices; vno++) {
      VERTEX * const v = &mris->vertices[vno];
      if (v->ripflag) {
        continue;
      }
      v->tx = 0;
    }

    for (vno = 0; vno < mris->nvertices; vno++) {
      VERTEX_TOPOLOGY const * const vt = &mris->vertices_topology[vno];
      VERTEX                * const v  = &mris->vertices         [vno];
      if (v->ripflag) {
        continue;
      }
      mn = v->marked;
      for (n = 0; n < vt->vnum; n++) {
        VERTEX * const vn = &mris->vertices[vt->v[n]];
        mn = MIN(vn->marked, mn);
      }
      v->tx = mn;
    }
    for (vno = 0; vno < mris->nvertices; vno++) {
      VERTEX * const v = &mris->vertices[vno];
      if (v->ripflag) {
        continue;
      }
      v->marked = (int)v->tx;
    }
  }
  return (NO_ERROR);
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int MRIScloseMarked(MRI_SURFACE *mris, int order)
{
  MRISdilateMarked(mris, order);
  MRISerodeMarked(mris, order);
  return (NO_ERROR);
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int MRISopenMarked(MRI_SURFACE *mris, int order)
{
  MRISerodeMarked(mris, order);
  MRISdilateMarked(mris, order);
  return (NO_ERROR);
}


int MRISripLabel(MRI_SURFACE *mris, LABEL *area)
{
  int i;
  VERTEX *v;

  for (i = 0; i < area->n_points; i++) {
    v = &mris->vertices[area->lv[i].vno];
    v->ripflag = 1;
  }
  return (NO_ERROR);
}
int MRISripNotLabel(MRI_SURFACE *mris, LABEL *area)
{
  int i, vno;
  VERTEX *v;

  for (i = 0; i < area->n_points; i++) {
    v = &mris->vertices[area->lv[i].vno];
    v->marked = 1;
  }
  for (vno = 0; vno < mris->nvertices; vno++) {
    v = &mris->vertices[vno];
    if (v->marked) {
      continue;
    }
    v->ripflag = 1;
  }
  for (i = 0; i < area->n_points; i++) {
    v = &mris->vertices[area->lv[i].vno];
    v->marked = 0;
  }
  return (NO_ERROR);
}
int MRISinvertMarks(MRI_SURFACE *mris)
{
  int vno;
  VERTEX *v;

  for (vno = 0; vno < mris->nvertices; vno++) {
    v = &mris->vertices[vno];
    if (v->ripflag) {
      continue;
    }
    v->marked = !v->marked;
  }
  return (NO_ERROR);
}

int MRISsetFlags(MRI_SURFACE *mris, int flags)
{
  int vno;

  for (vno = 0; vno < mris->nvertices; vno++) {
    mris->vertices[vno].flags |= flags;
  }
  return (NO_ERROR);
}

int MRISclearFlags(MRI_SURFACE *mris, int flags)
{
  int vno;

  for (vno = 0; vno < mris->nvertices; vno++) {
    mris->vertices[vno].flags &= (~flags);
  }
  return (NO_ERROR);
}

int MRIScopyMarkedToMarked2(MRI_SURFACE *mris)
{
  int vno;
  VERTEX *v;

  for (vno = 0; vno < mris->nvertices; vno++) {
    v = &mris->vertices[vno];
    v->marked2 = v->marked;
  }
  return (NO_ERROR);
}
int MRIScopyValsToAnnotations(MRI_SURFACE *mris)
{
  int vno;
  VERTEX *v;

  for (vno = 0; vno < mris->nvertices; vno++) {
    v = &mris->vertices[vno];
    v->annotation = v->val;
  }
  return (NO_ERROR);
}

int MRIScopyMarked2ToMarked(MRI_SURFACE *mris)
{
  int vno;
  VERTEX *v;

  for (vno = 0; vno < mris->nvertices; vno++) {
    v = &mris->vertices[vno];
    v->marked = v->marked2;
  }
  return (NO_ERROR);
}

int MRIScopyMarkedToMarked3(MRI_SURFACE *mris)
{
  int vno;
  VERTEX *v;

  for (vno = 0; vno < mris->nvertices; vno++) {
    v = &mris->vertices[vno];
    v->marked3 = v->marked;
  }
  return (NO_ERROR);
}

int MRIScopyMarked3ToMarked(MRI_SURFACE *mris)
{
  int vno;
  for (vno = 0; vno < mris->nvertices; vno++) {
    VERTEX * const v = &mris->vertices[vno];
    v->marked = v->marked3;
  }
  return (NO_ERROR);
}

/* assume that the mark is 1 */
int MRISexpandMarked(MRI_SURFACE *mris)
{
  int vno, n;

  for (vno = 0; vno < mris->nvertices; vno++) {
    VERTEX_TOPOLOGY const * const vt = &mris->vertices_topology[vno];
    VERTEX          const * const v  = &mris->vertices         [vno];
    if (v->marked == 1) {
      for (n = 0; n < vt->vnum; n++) {
        VERTEX * const vn = &mris->vertices[vt->v[n]];
        if (vn->marked == 0) {
          vn->marked = 2;
        }
      }
    }
  }
  for (vno = 0; vno < mris->nvertices; vno++) {
    VERTEX * const v = &mris->vertices[vno];
    if (v->marked == 2) {
      v->marked = 1;
    }
  }
  return (NO_ERROR);
}

int MRISstoreRipFlags(MRI_SURFACE *mris)
{
  int vno, fno;
  VERTEX *v;
  FACE *f;

  for (vno = 0; vno < mris->nvertices; vno++) {
    v = &mris->vertices[vno];
    v->oripflag = v->ripflag;
  }
  for (fno = 0; fno < mris->nfaces; fno++) {
    f = &mris->faces[fno];
    f->oripflag = f->ripflag;
  }
  return (NO_ERROR);
}

int MRISrestoreRipFlags(MRI_SURFACE *mris)
{
  int vno, fno;
  VERTEX *v;
  FACE *f;

  for (vno = 0; vno < mris->nvertices; vno++) {
    v = &mris->vertices[vno];
    v->ripflag = v->oripflag;
  }

  for (fno = 0; fno < mris->nfaces; fno++) {
    f = &mris->faces[fno];
    f->ripflag = f->oripflag;
  }

  return (NO_ERROR);
}

/*!
  \fn int MRISripMarked(MRI_SURFACE *mris)
  \brief Sets v->ripflag=1 if v->marked==1.
  Note: does not unrip any vertices.
*/
int MRISripMarked(MRI_SURFACE *mris)
{
  int vno;
  VERTEX *v;

  for (vno = 0; vno < mris->nvertices; vno++) {
    v = &mris->vertices[vno];
    if (v->marked) {
      v->ripflag = 1;
    }
  }
  return (NO_ERROR);
}
int MRISripUnmarked(MRI_SURFACE *mris)
{
  int vno;
  VERTEX *v;

  for (vno = 0; vno < mris->nvertices; vno++) {
    v = &mris->vertices[vno];
    if (v->marked == 0) {
      v->ripflag = 1;
    }
  }
  return (NO_ERROR);
}

// set all the marks to a user specified value INCLUDING RIPPED VERTICES!
int MRISsetAllMarks(MRI_SURFACE *mris, int mark)
{
  int vno;
  VERTEX *v;

  for (vno = 0; vno < mris->nvertices; vno++) {
    v = &mris->vertices[vno];
    v->marked = mark;
  }
  return (NO_ERROR);
}



/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  if nsize <=0 then neighborhood size gets reset back to whatever
  it's max was.
  ------------------------------------------------------*/
int MRISresetNeighborhoodSize(MRI_SURFACE *mris, int nsize)
{
  int vno;
  for (vno = 0; vno < mris->nvertices; vno++) {
    VERTEX_TOPOLOGY * const vt = &mris->vertices_topology[vno];    
    VERTEX          * const v  = &mris->vertices         [vno];
    if (v->ripflag) {
      continue;
    }
    if (vno == Gdiag_no) {
      DiagBreak();
    }
    switch (nsize) {
      default: /* reset back to original */
        switch (vt->nsize) {
          default:
          case 1:
            vt->vtotal = vt->vnum;
            break;
          case 2:
            vt->vtotal = vt->v2num;
            break;
          case 3:
            vt->vtotal = vt->v3num;
            break;
        }
        break;
      case 1:
        vt->vtotal = vt->vnum;
        break;
      case 2:
        vt->vtotal = vt->v2num;
        break;
      case 3:
        vt->vtotal = vt->v3num;
        break;
    }
  }
  mris->nsize = nsize;
  return (NO_ERROR);
}

#define MAX_4_NEIGHBORS 100
#define MAX_3_NEIGHBORS 70
#define MAX_2_NEIGHBORS 20
#define MAX_1_NEIGHBORS 8
#define MAX_NEIGHBORS (10000)


/*
  fills the vlist parameter with the indices of the vertices up to and include
  nlinks distances in terms of number of edges. Each vertex->marked field will be
  set to the number of edges between it and the central vertex.
*/
int MRISfindNeighborsAtVertex(MRI_SURFACE *mris, int vno, int nlinks, int *vlist)
{
  VERTEX_TOPOLOGY * const vt = &mris->vertices_topology[vno];    
  VERTEX          * const v  = &mris->vertices         [vno];

  int m, n, vtotal = 0, link_dist, ring_total;
  if (v->ripflag) return (0);

  v->marked = -1;
  for (n = 0; n < vt->vtotal; n++) {
    vlist[n] = vt->v[n];
    mris->vertices[vt->v[n]].marked = n < vt->vnum ? 1 : (n < vt->v2num ? 2 : 3);
  }
  if (nlinks < mris->nsize) {
    switch (nlinks) {
      case 1:
        vtotal = vt->vnum;
        break;
      case 2:
        vtotal = vt->v2num;
        break;
      case 3:
        vt->vtotal = vt->v3num;
        break;
      default:
        vtotal = 0;
        ErrorExit(ERROR_BADPARM, "MRISfindNeighborsAtVertex: nlinks=%d invalid", nlinks);
        break;
    }
  }
  else  // bigger than biggest neighborhood held at each vertex
  {
    v->marked = mris->nsize;
    link_dist = mris->nsize;
    vtotal = vt->vtotal;
    // at each iteration mark one more ring with the ring distance
    do {
      link_dist++;
      ring_total = 0;
      for (n = 0; n < vtotal; n++) {
        VERTEX_TOPOLOGY const * const vnt = &mris->vertices_topology[vlist[n]];    
        VERTEX          const * const vn  = &mris->vertices         [vlist[n]];
        if (vn->ripflag) continue;
        for (m = 0; m < vnt->vnum; m++)  // one more ring out
        {
          if (mris->vertices[vnt->v[m]].marked == 0) {
            vlist[vtotal + ring_total] = vnt->v[m];
            mris->vertices[vnt->v[m]].marked = link_dist;
            ring_total++;
          }
        }
      }
      vtotal += ring_total;
    } while (link_dist < nlinks);  // expand by one
  }
  return (vtotal);
}


/*
  \fn int MRIScountEdges(MRIS *surf)
  \brief Counts the number of edges on the surface. Edge struct
  is not needed.
*/
int MRIScountEdges(MRIS *surf)
{
  // This is not thread safe and cannot be made thread safe
  int nedges=0;
  int vtxno;
  for(vtxno=0; vtxno < surf->nvertices; vtxno++){
    VERTEX_TOPOLOGY const * const vt = &(surf->vertices_topology[vtxno]);
    int nthnbr;
    for(nthnbr = 0; nthnbr < vt->vnum; nthnbr++){
      if(vt->v[nthnbr] < vtxno) continue;
      nedges++;
    }
  }
  //printf("nedges %d\n",nedges);
  return(nedges);
}


/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int MRIScomputeEulerNumber(MRI_SURFACE *mris, int *pnvertices, int *pnfaces, int *pnedges)
{
  int eno, nfaces, nedges, nvertices, vno, fno, vnb, i;

  /*  MRISripFaces(mris) ;*/
  for (nfaces = fno = 0; fno < mris->nfaces; fno++)
    if (!mris->faces[fno].ripflag) {
      nfaces++;
    }

  for (nvertices = vno = 0; vno < mris->nvertices; vno++)
    if (!mris->vertices[vno].ripflag) {
      nvertices++;
    }

  for (nedges = vno = 0; vno < mris->nvertices; vno++)
    if (!mris->vertices[vno].ripflag) {
      VERTEX_TOPOLOGY const * const v1 = &mris->vertices_topology[vno];
      for (i = 0; i < v1->vnum; i++) {
        vnb = v1->v[i];
        /* already counted */
        if ((vnb > vno) && !mris->vertices[vnb].ripflag) {
          nedges++;
        }
      }
    }

  *pnfaces = nfaces;
  *pnvertices = nvertices;
  *pnedges = nedges;
  eno = nvertices - nedges + nfaces;
  return (eno);
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int MRIStopologicalDefectIndex(MRI_SURFACE *mris)
{
  int eno, nfaces, nedges, nvertices, vno, fno, vnb, i, dno;

  for (nfaces = fno = 0; fno < mris->nfaces; fno++)
    if (!mris->faces[fno].ripflag) {
      nfaces++;
    }

  for (nvertices = vno = 0; vno < mris->nvertices; vno++)
    if (!mris->vertices[vno].ripflag) {
      nvertices++;
    }

  for (nedges = vno = 0; vno < mris->nvertices; vno++)
    if (!mris->vertices[vno].ripflag) {
      VERTEX_TOPOLOGY const * const v1 = &mris->vertices_topology[vno];
      for (i = 0; i < v1->vnum; i++) {
        vnb = v1->v[i];
        /* already counted */
        if ((vnb > vno) && !mris->vertices[vnb].ripflag) {
          nedges++;
        }
      }
    }
#if 0
  nedges += nfaces ;        /* one additional edge added for each triangle */
  nfaces *= 2 ;             /* two triangular faces per face */
#endif
  eno = nvertices - nedges + nfaces;

  dno = abs(2 - eno) + abs(2 * nedges - 3 * nfaces);
  return (dno);
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int mrisRemoveEdge(MRI_SURFACE *mris, int vno1, int vno2)
{
  int i;
  VERTEX_TOPOLOGY * const v1 = &mris->vertices_topology[vno1];

  for (i = 0; i < v1->vnum; i++)
    if (v1->v[i] == vno2) {
      v1->vnum--;
      if (i < v1->vnum) /* not the (previous) last index */
      {
        memmove(&v1->v[i], &v1->v[i + 1], (v1->vnum - i) * sizeof(v1->v[0]));
      }
      return (NO_ERROR);
    }
  return (ERROR_BADPARM);
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int mrisRemoveFace(MRI_SURFACE *mris, int fno);
  
int mrisRemoveLink(MRI_SURFACE *mris, int vno1, int vno2)
{
  FACE *face;
  int vno, fno, nvalid;

  VERTEX_TOPOLOGY * const v1t = &mris->vertices_topology[vno1];
  VERTEX_TOPOLOGY * const v2t = &mris->vertices_topology[vno2];
  VERTEX          * const v1  = &mris->vertices         [vno1];
  VERTEX          * const v2  = &mris->vertices         [vno2];

  mrisRemoveEdge(mris, vno1, vno2);
  mrisRemoveEdge(mris, vno2, vno1);

  /* now remove all the faces which contain both edges */
  for (fno = 0; fno < v1t->num; fno++) {
    face = &mris->faces[v1t->f[fno]];
    for (vno = 0; vno < VERTICES_PER_FACE; vno++)
      if (face->v[vno] == vno2) /* found a face with both vertices */
      {
        face->ripflag = 1;
      }
    if (face->ripflag  >= 2) {
      mrisRemoveFace(mris, v1t->f[fno]);
    }
  }

  /* make sure vno1 and vno2 are still part of at least 1 valid face */
  for (nvalid = fno = 0; fno < v1t->num; fno++)
    if (!mris->faces[v1t->f[fno]].ripflag) {
      nvalid++;
    }
  if (nvalid <= 0) {
    v1->ripflag = 1;
  }

  for (nvalid = fno = 0; fno < v2t->num; fno++)
    if (!mris->faces[v2t->f[fno]].ripflag) {
      nvalid++;
    }
  if (nvalid <= 0) {
    v2->ripflag = 1;
  }

  return (NO_ERROR);
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  remove this face, as well as the link two vertices if
  they only exist through this face.
  ------------------------------------------------------*/
int mrisRemoveFace(MRI_SURFACE *mris, int fno)
{
  int vno1, vno2, vno;
  FACE *face;

  face = &mris->faces[fno];
  face->ripflag = 1;
  for (vno = 0; vno < VERTICES_PER_FACE; vno++) {
    vno1 = face->v[vno];
    vno2 = face->v[vno < VERTICES_PER_FACE - 1 ? vno + 1 : 0];
    if (!mrisCountValidLinks(mris, vno1, vno2)) {
      mrisRemoveEdge(mris, vno1, vno2);
      mrisRemoveEdge(mris, vno2, vno1);
    }
  }

  return (NO_ERROR);
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int MRISvalidVertices(MRI_SURFACE *mris)
{
  int vno, nvertices, nvalid;

  nvertices = mris->nvertices;
  for (vno = nvalid = 0; vno < nvertices; vno++)
    if (!mris->vertices[vno].ripflag) {
      nvalid++;
    }

  return (nvalid);
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int MRISmarkedVertices(MRI_SURFACE *mris)
{
  int vno, nvertices, nmarked;

  nvertices = mris->nvertices;
  for (vno = nmarked = 0; vno < nvertices; vno++)
    if (!mris->vertices[vno].ripflag && mris->vertices[vno].marked > 0) {
      nmarked++;
    }

  return (nmarked);
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int mrisValidFaces(MRI_SURFACE *mris)
{
  int fno, nfaces, nvalid;

  nfaces = mris->nfaces;
  for (fno = nvalid = 0; fno < nfaces; fno++)
    if (!mris->faces[fno].ripflag) {
      nvalid++;
    }

  return (nvalid);
}

/*-----------------------------------------------------*/
/*!
  \fn int MRISreverseFaceOrder(MRIS *mris)
  \brief Reverse order of the vertices in each face. This
  is needed when changing the sign of the x surface coord.
*/
int MRISreverseFaceOrder(MRIS *mris)
{
  int fno, vno0, vno1, vno2;
  FACE *f;

  for (fno = 0; fno < mris->nfaces; fno++) {
    f = &mris->faces[fno];
    vno0 = f->v[0];
    vno1 = f->v[1];
    vno2 = f->v[2];
    f->v[0] = vno2;
    f->v[1] = vno1;
    f->v[2] = vno0;
    mrisSetVertexFaceIndex(mris, vno0, fno);
    mrisSetVertexFaceIndex(mris, vno1, fno);
    mrisSetVertexFaceIndex(mris, vno2, fno);
  }
  return (NO_ERROR);
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  Go through all the (valid) faces in vno1 and see how man
  valid links to vno2 exists.
  ------------------------------------------------------*/
int mrisCountValidLinks(MRI_SURFACE *mris, int vno1, int vno2)
{
  int nvalid, fno, vno;
  FACE *face;
  VERTEX_TOPOLOGY const * const vt = &mris->vertices_topology[vno1];
  for (nvalid = fno = 0; fno < vt->num; fno++) {
    face = &mris->faces[vt->f[fno]];
    if (face->ripflag) {
      continue;
    }
    for (vno = 0; vno < VERTICES_PER_FACE; vno++)
      if (face->v[vno] == vno1) {
        if ((vno == VERTICES_PER_FACE - 1) && (face->v[0] == vno2)) {
          nvalid++;
        }
        else if ((vno < VERTICES_PER_FACE - 1) && (face->v[vno + 1] == vno2)) {
          nvalid++;
        }
      }
  }
  return (nvalid);
}

/*
  \fn int MRISedges(MRIS *surf)
  \brief Allocates and assigns edge structures. I think the corner values
  are not correct.
*/
int MRISedges(MRIS *surf)
{
  surf->nedges = MRIScountEdges(surf);
  surf->edges  = (MRI_EDGE *)calloc(surf->nedges,sizeof(MRI_EDGE));

  // This is not thread safe and cannot be made thread safe
  int edgeno = 0;
  int vtxno0;
  for(vtxno0=0; vtxno0 < surf->nvertices; vtxno0++){
    VERTEX_TOPOLOGY const * const v0t = &surf->vertices_topology[vtxno0];
    
    int nthnbr;
    for(nthnbr = 0; nthnbr < v0t->vnum; nthnbr++){
      int const vtxno1 = v0t->v[nthnbr];
      if(vtxno1 < vtxno0) continue;
      
      
      VERTEX_TOPOLOGY const * const v1t = &surf->vertices_topology[vtxno1];
      MRI_EDGE * const edge = &(surf->edges[edgeno]);
      edge->vtxno[0] = vtxno0;
      edge->vtxno[1] = vtxno1;

      // Find the two faces that adjoin this edge
      {
        int k = 0; // k is the nthface out of 2
        int n;
        for(n=0; n < v0t->num; n++){ // go thru faces of vtx1
          int m;
	  for(m=0; m < v1t->num; m++){ // go thru faces of vtx2
	    if(v0t->f[n] == v1t->f[m]){ // same face
	      if(k>1){
	        printf("ERROR: MRISedge(): too many faces: %d %d n=%d, m=%d, k=%d\n",vtxno0,vtxno1,n,m,k);
	        return(1);
	      }
	      FACE const * const f = &(surf->faces[v0t->f[n]]);
	      // get the ordinal corner numbers for the two vertices
              int c;
	      for(c=0; c<2; c++){
	        if(f->v[c] == vtxno0) edge->corner[k][0] = c;
	        if(f->v[c] == vtxno1) edge->corner[k][1] = c;
	      }
	      edge->faceno[k] = v0t->f[n];
	      k++;
	    }
	  }
        }
        if(k != 2){
	  printf("ERROR: MRISedge(): not enough faces %d %d k=%d\n",vtxno0,vtxno1,k);
	  return(1);
        }
      }
      
      // Now find the two vertices of the faces that are not common between the faces;
      // these are the 3rd corner for the two triangles
      {
        int k;
        for(k=0; k<2; k++){ // go thru the two faces
	  FACE const * const f = &(surf->faces[edge->faceno[k]]); // kth face
          int c;
	  for(c=0; c<3; c++){ // go thru the vertices of the kth face
	    if(f->v[c] != vtxno0 && f->v[c] != vtxno1){
	      // this is the 3rd corner if it is not vtx1 or vtx2
	      edge->vtxno[k+2] = f->v[c];
	      break;
	    }
	  }
        }
      }
      
      // Fill the corner matrix
      {
        int n;
        for(n=0; n<4; n++){ // go thru the four edge vertices
	  int const m = edge->vtxno[n];
          int k;
	  for(k=0; k<2; k++){ // go thru the two faces
	    FACE const * const f = &(surf->faces[edge->faceno[k]]); // kth face
            int c;
	    for(c=0; c<3; c++){ // go thru the vertices of the kth face
	      // Compare the edge vertex no against the face vertex no
	      if(f->v[c] == m){
	        edge->corner[n][k] = c;
	        break;
	      }
	    }
	  }
        }
      }
      
      // Note: corner[2][1] and corner[3][0] are not used
      edge->corner[2][1] = 9;
      edge->corner[3][0] = 9;

      edgeno++;
    }
  }

  return(0);
}


int MRISnotMarked(MRI_SURFACE *mris)
{
  int vno;
  VERTEX *v;

  for (vno = 0; vno < mris->nvertices; vno++) {
    v = &mris->vertices[vno];
    if (v->ripflag == 0) v->marked = !v->marked;
  }
  return (NO_ERROR);
}


/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int MRISripFaces(MRI_SURFACE *mris)
{
  int n, k;
  face_type *f;

  for (k = 0; k < mris->nfaces; k++) {
    mris->faces[k].ripflag = FALSE;
  }
  for (k = 0; k < mris->nfaces; k++) {
    f = &mris->faces[k];
    for (n = 0; n < VERTICES_PER_FACE; n++)
      if (mris->vertices[f->v[n]].ripflag) {
        f->ripflag = TRUE;
      }
  }

  for (k = 0; k < mris->nvertices; k++) {
    mris->vertices[k].border = FALSE;
  }
  for (k = 0; k < mris->nfaces; k++)
    if (mris->faces[k].ripflag) {
      f = &mris->faces[k];
      for (n = 0; n < VERTICES_PER_FACE; n++) {
        mris->vertices[f->v[n]].border = TRUE;
      }
    }
  return (NO_ERROR);
}


int MRISevertSurface(MRI_SURFACE *mris)
{
  int v0, fno;
  FACE *face;

  for (fno = 0; fno < mris->nfaces; fno++) {
    face = &mris->faces[fno];
    if (face->ripflag) continue;
    v0 = face->v[0];
    face->v[0] = face->v[1];
    face->v[1] = v0;
  }

  return (NO_ERROR);
}


int mrisRemoveVertexLink(MRI_SURFACE *mris, int vno1, int vno2)
{
  int n;
  VERTEX_TOPOLOGY * const v = &mris->vertices_topology[vno1];
  for (n = 0; n < v->vnum; n++)
    if (v->v[n] == vno2) {
      break;
    }

  if (n < v->vnum) {
    memmove(v->v + n, v->v + n + 1, (v->vtotal - (n + 1)) * sizeof(int));
    v->vnum--;
    v->vtotal--;
  }
  return (NO_ERROR);
}

int MRISremoveTriangleLinks(MRI_SURFACE *mris)
{
  int fno, which;
  FACE *f;

  if (!IS_QUADRANGULAR(mris)) {
    return (NO_ERROR);
  }
  if (mris->triangle_links_removed) {
    return (NO_ERROR);
  }

  mris->triangle_links_removed = 1;

  if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON) {
    fprintf(stdout, "removing non-quadrangular links.\n");
  }

  for (fno = 0; fno < mris->nfaces; fno += 2) {
    f = &mris->faces[fno];
    if (f->ripflag) {
      continue;
    }
    which = WHICH_FACE_SPLIT(f->v[0], f->v[1]);
    if (EVEN(which)) {
      mrisRemoveVertexLink(mris, f->v[1], f->v[2]);
      mrisRemoveVertexLink(mris, f->v[2], f->v[1]);
    }
    else {
      mrisRemoveVertexLink(mris, f->v[0], f->v[2]);
      mrisRemoveVertexLink(mris, f->v[2], f->v[0]);
    }
  }
  return (NO_ERROR);
}

/*-----------------------------------------------------
  ------------------------------------------------------*/
static int mrisInitializeNeighborhood(MRI_SURFACE *mris, int vno)
{
  int vtmp[MAX_NEIGHBORS], vnum, i, j, n, neighbors, nsize;

  VERTEX_TOPOLOGY * const vt = &mris->vertices_topology[vno];
  VERTEX          * const v  = &mris->vertices         [vno];
  if (vno == Gdiag_no) {
    DiagBreak();
  }

  vt->nsize = mris->nsize;
  if (v->ripflag || !vt->vnum) {
    return (ERROR_BADPARM);
  }
  memmove(vtmp, vt->v, vt->vnum * sizeof(int));

  /* mark 1-neighbors so we don't count them twice */
  v->marked = 1;

  vnum = neighbors = vt->vnum;
  for (nsize = 2; nsize <= vt->nsize; nsize++) {
    /* mark all current neighbors */
    vnum = neighbors; /* neighbors will be incremented during loop */
    for (i = 0; i < neighbors; i++) {
      mris->vertices[vtmp[i]].marked = 1;
    }
    for (i = 0; neighbors < MAX_NEIGHBORS && i < vnum; i++) {
      n = vtmp[i];
      VERTEX_TOPOLOGY const * const vnbt = &mris->vertices_topology[n];
      VERTEX          const * const vnb  = &mris->vertices         [n];
      if (vnb->ripflag) {
        continue;
      }

      for (j = 0; j < vnbt->vnum; j++) {
        VERTEX * const vnb2 = &mris->vertices[vnbt->v[j]];
        if (vnb2->ripflag || vnb2->marked) {
          continue;
        }
        vtmp[neighbors] = vnbt->v[j];
        vnb2->marked = 1;
        if (++neighbors >= MAX_NEIGHBORS) {
          fprintf(stdout, "vertex %d has too many neighbors!\n", vno);
          break;
        }
      }
    }
  }
  /*
    now reallocate the v->v structure and place the 2-connected neighbors
    suquentially after the 1-connected neighbors.
  */
  free(vt->v);
  vt->v = (int *)calloc(neighbors, sizeof(int));
  if (!vt->v)
    ErrorExit(ERROR_NO_MEMORY,
              "MRISsetNeighborhoodSize: could not allocate list of %d "
              "nbrs at v=%d",
              neighbors,
              vno);

  v->marked = 0;
  for (n = 0; n < neighbors; n++) {
    vt->v[n] = vtmp[n];
    mris->vertices[vtmp[n]].marked = 0;
  }
  if (v->dist) {
    free(v->dist);
  }
  if (v->dist_orig) {
    free(v->dist_orig);
  }

  v->dist = (float *)calloc(neighbors, sizeof(float));
  if (!v->dist)
    ErrorExit(ERROR_NOMEMORY,
              "MRISsetNeighborhoodSize: could not allocate list of %d "
              "dists at v=%d",
              neighbors,
              vno);
  v->dist_orig = (float *)calloc(neighbors, sizeof(float));
  if (!v->dist_orig)
    ErrorExit(ERROR_NOMEMORY,
              "MRISsetNeighborhoodSize: could not allocate list of %d "
              "dists at v=%d",
              neighbors,
              vno);
  switch (vt->nsize) {
    case 2:
      vt->v2num = neighbors;
      break;
    case 3:
      vt->v3num = neighbors;
      break;
    default: /* store old neighborhood size in v3num */
      vt->v3num = vt->vtotal;
      break;
  }
  vt->vtotal = neighbors;
  for (n = 0; n < neighbors; n++)
    for (i = 0; i < neighbors; i++)
      if (i != n && vt->v[i] == vt->v[n])
        fprintf(stdout, "warning: vertex %d has duplicate neighbors %d and %d!\n", vno, i, n);
  if ((vno == Gdiag_no) && (Gdiag & DIAG_SHOW) && DIAG_VERBOSE_ON) {
    fprintf(stdout, "v %d: vnum=%d, v2num=%d, vtotal=%d\n", vno, vt->vnum, vt->v2num, vt->vtotal);
    for (n = 0; n < neighbors; n++) {
      fprintf(stdout, "v[%d] = %d\n", n, vt->v[n]);
    }
  }

  return (NO_ERROR);
}



/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
#define MAX_VERTEX_NEIGHBORS 50
#define MAX_FACES 50
int mrisDivideFace(MRI_SURFACE *mris, int fno, int vno1, int vno2, int vnew_no);

int mrisDivideEdge(MRI_SURFACE *mris, int vno1, int vno2)
{
  int n, m, n1, n2;

  if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON) {
    fprintf(stdout, "dividing edge %d --> %d\n", vno1, vno2);
  }

  if (vno1 == Gdiag_no || vno2 == Gdiag_no || mris->nvertices == Gdiag_no) {
    printf("dividing edge %d --> %d, adding vertex number %d\n", vno1, vno2, mris->nvertices);
    DiagBreak();
  }
  VERTEX_TOPOLOGY const * const v1t = &mris->vertices_topology[vno1];
  VERTEX          const * const v1  = &mris->vertices         [vno1];
  VERTEX_TOPOLOGY const * const v2t = &mris->vertices_topology[vno2];
  VERTEX          const * const v2  = &mris->vertices         [vno2];
  
  if (v1->ripflag || v2->ripflag || mris->nvertices >= mris->max_vertices || mris->nfaces >= (mris->max_faces - 1)) {
    return (ERROR_NO_MEMORY);
  }

  /* check to make sure these vertices or the faces they are part of
     have enough room to expand.
  */
  if (v1t->vnum >= MAX_VERTEX_NEIGHBORS || v2t->vnum >= MAX_VERTEX_NEIGHBORS || v1t->num >= MAX_FACES ||
      v2t->num >= MAX_FACES) {
    return (ERROR_NO_MEMORY);
  }

  /* add 1 new vertex, 2 new faces, and 2 new edges */
  int const vnew_no = mris->nvertices;
  VERTEX_TOPOLOGY * const vnewt = &mris->vertices_topology[vnew_no];
  VERTEX          * const vnew  = &mris->vertices         [vnew_no];
  
  vnew->x = (v1->x + v2->x) / 2;
  vnew->y = (v1->y + v2->y) / 2;
  vnew->z = (v1->z + v2->z) / 2;
  vnew->tx = (v1->tx + v2->tx) / 2;
  vnew->ty = (v1->ty + v2->ty) / 2;
  vnew->tz = (v1->tz + v2->tz) / 2;

  vnew->infx = (v1->infx + v2->infx) / 2;
  vnew->infy = (v1->infy + v2->infy) / 2;
  vnew->infz = (v1->infz + v2->infz) / 2;

  vnew->pialx = (v1->pialx + v2->pialx) / 2;
  vnew->pialy = (v1->pialy + v2->pialy) / 2;
  vnew->pialz = (v1->pialz + v2->pialz) / 2;

  vnew->cx = (v1->cx + v2->cx) / 2;
  vnew->cy = (v1->cy + v2->cy) / 2;
  vnew->cz = (v1->cz + v2->cz) / 2;
  vnew->x = (v1->x + v2->x) / 2;
  vnew->y = (v1->y + v2->y) / 2;
  vnew->z = (v1->z + v2->z) / 2;
  vnew->odx = (v1->odx + v2->odx) / 2;
  vnew->ody = (v1->ody + v2->ody) / 2;
  vnew->odz = (v1->odz + v2->odz) / 2;
  vnew->val = (v1->val + v2->val) / 2;
  vnew->origx = (v1->origx + v2->origx) / 2; CHANGES_ORIG
  vnew->origy = (v1->origy + v2->origy) / 2;
  vnew->origz = (v1->origz + v2->origz) / 2;
  vnewt->vnum = 2; /* at least connected to two bisected vertices */

  /* count the # of faces that both vertices are part of */
  int flist[100];
  for (n = 0; n < v1t->num; n++) {
    int const fno = v1t->f[n];
    FACE const * const face = &mris->faces[fno];
    for (m = 0; m < VERTICES_PER_FACE; m++)
      if (face->v[m] == vno2) {
        if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON) {
          fprintf(stdout, " face %d shared.\n", fno);
        }
        flist[vnewt->num++] = fno;
        if (vnewt->num == 100) {
          ErrorExit(ERROR_BADPARM, "Too many faces to divide edge");
        }
        vnewt->vnum++;
        vnewt->vtotal = vnewt->vnum;
      }
  }

  MRISgrowNVertices(mris, mris->nvertices+1);

  /* will be part of two new faces also */
  // total array size is going to be vnew->num*2!
  if (vnewt->num >= 50) {
    ErrorExit(ERROR_BADPARM, "Too many faces to divide edge");
  }
  for (n = 0; n < vnewt->num; n++) {
    flist[vnewt->num + n] = mris->nfaces + n;
  }
  vnewt->num *= 2;
  vnewt->f = (int *)calloc(vnewt->num, sizeof(int));
  if (!vnewt->f) {
    ErrorExit(ERROR_NOMEMORY, "could not allocate %dth face list.\n", vnew_no);
  }
  vnewt->n = (uchar *)calloc(vnewt->num, sizeof(uchar));
  if (!vnewt->n) {
    ErrorExit(ERROR_NOMEMORY, "could not allocate %dth face list.\n", vnew_no);
  }
  vnewt->v = (int *)calloc(vnewt->vnum, sizeof(int));
  if (!vnewt->v) ErrorExit(ERROR_NOMEMORY, "could not allocate %dth vertex list.\n", vnew_no);

  vnewt->num = vnewt->vnum = 0;

  /* divide every face that both vertices are part of in two */
  for (n = 0; n < v1t->num; n++) {
    int const fno = v1t->f[n];
    FACE const * const face = &mris->faces[fno];
    for (m = 0; m < VERTICES_PER_FACE; m++)
      if (face->v[m] == vno2) {
        if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
          fprintf(stdout, "dividing face %d along edge %d-->%d.\n", fno, vno1, vno2);
        if (face->v[m] == Gdiag_no || vno2 == Gdiag_no) {
          DiagBreak();
        }
        mrisDivideFace(mris, fno, vno1, vno2, vnew_no);
      }
  }

  /* build vnew->f and vnew->n lists by going through all faces v1 and
     v2 are part of */
  int fno;
  for (fno = 0; fno < vnewt->num; fno++) {
    vnewt->f[fno] = flist[fno];
    FACE const * const face = &mris->faces[flist[fno]];
    for (n = 0; n < VERTICES_PER_FACE; n++)
      if (face->v[n] == vnew_no) {
        vnewt->n[fno] = (uchar)n;
      }
  }

  /* remove vno1 from vno2 list and visa-versa */
  for (n = 0; n < v1t->vnum; n++)
    if (v1t->v[n] == vno2) {
      v1t->v[n] = vnew_no;
      break;
    }
  for (n = 0; n < v2t->vnum; n++)
    if (v2t->v[n] == vno1) {
      v2t->v[n] = vnew_no;
      break;
    }
  /* build vnew->v list by going through faces it is part of and
     rejecting duplicates
  */
  for (fno = 0; fno < vnewt->num; fno++) {
    FACE const * const face = &mris->faces[vnewt->f[fno]];
    n1 = vnewt->n[fno] == 0 ? VERTICES_PER_FACE - 1 : vnewt->n[fno] - 1;
    n2 = vnewt->n[fno] == VERTICES_PER_FACE - 1 ? 0 : vnewt->n[fno] + 1;
    vno1 = face->v[n1];
    vno2 = face->v[n2];

    /* go through this faces vertices and see if they should be added to v[] */
    for (n = 0; n < vnewt->vnum; n++) {
      if (vnewt->v[n] == vno1) {
        vno1 = -1;
      }
      if (vnewt->v[n] == vno2) {
        vno2 = -1;
      }
    }
    if (vno1 >= 0) {
      vnewt->v[vnewt->vnum++] = vno1;
    }
    if (vno2 >= 0) {
      vnewt->v[vnewt->vnum++] = vno2;
    }
    vnewt->vtotal = vnewt->vnum;
  }
  if (0 && Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON) {
    fprintf(stdout, "%d edges and %d faces.\n", vnewt->vnum, vnewt->num);
  }

  if (!vnewt->vnum || !v1t->vnum || !v2t->vnum) {
    fprintf(stderr, "empty vertex (%d <-- %d --> %d!\n", vno1, vnew_no, vno2);
    DiagBreak();
  }
  if (vnewt->vnum != 4 || vnewt->num != 4) {
    DiagBreak();
  }
  mrisInitializeNeighborhood(mris, vnew_no);
  return (NO_ERROR);
}

/*
  nsubs = 1 --> divide edge in half
        = 2 --> divide edge in half twice, add 3 vertices
        = 3 --> divide edge in half three times, add 7 vertices
*/
#define MAX_SURFACE_FACES 200000
int MRISdivideEdges(MRI_SURFACE *mris, int nsubdivisions)
{
  int nadded, sub, nfaces, fno, nvertices, faces[MAX_SURFACE_FACES], index;
  FACE *f;

  for (nadded = sub = 0; sub < nsubdivisions; sub++) {
    nfaces = mris->nfaces;
    nvertices = mris->nvertices;  // before adding any
    for (fno = 0; fno < nfaces; fno++) {
      faces[fno] = fno;
    }
    for (fno = 0; fno < nfaces; fno++) {
      int tmp;

      index = (int)randomNumber(0.0, (double)(nfaces - 0.0001));
      tmp = faces[fno];
      if (faces[fno] == Gdiag_no || faces[index] == Gdiag_no) {
        DiagBreak();
      }
      faces[fno] = faces[index];
      faces[index] = tmp;
    }

    for (index = 0; index < nfaces; index++) {
      fno = faces[index];
      f = &mris->faces[fno];
      if (fno == Gdiag_no) {
        DiagBreak();
      }

      if (f->v[0] < nvertices && f->v[1] < nvertices)
        if (mrisDivideEdge(mris, f->v[0], f->v[1]) == NO_ERROR) {
          nadded++;
        }
      if (f->v[0] < nvertices && f->v[2] < nvertices)
        if (mrisDivideEdge(mris, f->v[0], f->v[2]) == NO_ERROR) {
          nadded++;
        }
      if (f->v[1] < nvertices && f->v[2] < nvertices)
        if (mrisDivideEdge(mris, f->v[1], f->v[2]) == NO_ERROR) {
          nadded++;
        }
    }
  }

  if (Gdiag & DIAG_SHOW && nadded > 0) {
    fprintf(stdout,
            "MRISdivideEdges(%d): %d vertices added: # of vertices=%d, # of faces=%d.\n",
            nsubdivisions,
            nadded,
            mris->nvertices,
            mris->nfaces);
#if 0
    eno = MRIScomputeEulerNumber(mris, &nvertices, &nfaces, &nedges) ;
    fprintf(stdout, "euler # = v-e+f = 2g-2: %d - %d + %d = %d --> %d holes\n",
            nvertices, nedges, nfaces, eno, 2-eno) ;
#endif
  }
  return (nadded);
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int MRISdivideLongEdges(MRI_SURFACE *mris, double thresh)
{
  double dist;
  int vno, nadded, n /*,nvertices, nfaces, nedges, eno*/;
  float x, y, z;

  /* make it squared so we don't need sqrts later */
  for (nadded = vno = 0; vno < mris->nvertices; vno++) {
    VERTEX_TOPOLOGY const * const vt = &mris->vertices_topology[vno];
    VERTEX          const * const v  = &mris->vertices         [vno];
    if (v->ripflag) {
      continue;
    }
    if (vno == Gdiag_no) {
      DiagBreak();
    }
    x = v->x;
    y = v->y;
    z = v->z;

    /*
      only add vertices if average neighbor vector is in
      normal direction, that is, if the region is concave or sulcal.
    */
    for (n = 0; n < vt->vnum; n++) {
      VERTEX const * const vn = &mris->vertices[vt->v[n]];
      dist = sqrt(SQR(vn->x - x) + SQR(vn->y - y) + SQR(vn->z - z));
      if (dist > thresh) {
        if (mrisDivideEdge(mris, vno, vt->v[n]) == NO_ERROR) {
          nadded++;
        }
      }
    }
  }

  if (Gdiag & DIAG_SHOW && nadded > 0) {
    fprintf(stdout,
            "%2.2f mm: %d vertices added: # of vertices=%d, # of faces=%d.\n",
            thresh,
            nadded,
            mris->nvertices,
            mris->nfaces);
#if 0
    eno = MRIScomputeEulerNumber(mris, &nvertices, &nfaces, &nedges) ;
    fprintf(stdout, "euler # = v-e+f = 2g-2: %d - %d + %d = %d --> %d holes\n",
            nvertices, nedges, nfaces, eno, 2-eno) ;
#endif
  }
  return (nadded);
}
#if 0
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int
mrisAddVertices(MRI_SURFACE *mris, double thresh)
{
  double   dist ;
  int      vno, nadded, n,nvertices, nfaces, nedges, eno ;
  VERTEX   *v, *vn ;
  float    x, y, z ;

  /* make it squared so we don't need sqrts later */
  if (Gdiag & DIAG_SHOW)
  {
    fprintf(stdout, "dividing edges more than %2.2f mm long.\n", thresh) ;
  }
  for (nadded = vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
    {
      continue ;
    }
    if (vno == Gdiag_no)
    {
      DiagBreak() ;
    }
    x = v->origx ;
    y = v->origy ;
    z = v->origz ;

    /*
      only add vertices if average neighbor vector is in
      normal direction, that is, if the region is concave or sulcal.
    */
    for (n = 0 ; n < v->vnum ; n++)
    {
      vn = &mris->vertices[v->v[n]] ;
      dist = sqrt(SQR(vn->origx-x) + SQR(vn->origy - y) + SQR(vn->origz - z));
      if (dist > thresh)
      {
        if (mrisDivideEdge(mris, vno, v->v[n]) == NO_ERROR)
        {
          nadded++ ;
        }
      }
    }
  }

  if (Gdiag & DIAG_SHOW)
  {
    fprintf(stdout, "%d vertices added: # of vertices=%d, # of faces=%d.\n",
            nadded, mris->nvertices, mris->nfaces) ;
    eno = MRIScomputeEulerNumber(mris, &nvertices, &nfaces, &nedges) ;
    fprintf(stdout, "euler # = v-e+f = 2g-2: %d - %d + %d = %d --> %d holes\n",
            nvertices, nedges, nfaces, eno, 2-eno) ;
  }
  return(nadded) ;
}
#endif
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int mrisDivideFace(MRI_SURFACE *mris, int fno, int vno1, int vno2, int vnew_no)
{
  int fnew_no, n, vno3, flist[5000], vlist[5000], nlist[5000];

  if (vno1 == Gdiag_no || vno2 == Gdiag_no || vnew_no == Gdiag_no) {
    DiagBreak();
  }

  /* divide this face in two, reusing one of the face indices, and allocating
     one new one
  */
  if (mris->nfaces >= mris->max_faces) {
    return (ERROR_NO_MEMORY);
  }

  fnew_no = mris->nfaces;
  MRISgrowNFaces(mris, fnew_no+1);

  FACE * const f1 = &mris->faces[fno];
  FACE * const f2 = &mris->faces[fnew_no];
  VERTEX_TOPOLOGY const * const v2   = &mris->vertices_topology[vno2];
  VERTEX_TOPOLOGY       * const vnew = &mris->vertices_topology[vnew_no];
  memmove(f2->v, f1->v, VERTICES_PER_FACE * sizeof(int));

  /* set v3 to be other vertex in face being divided */

  /* 1st construct f1 by replacing vno2 with vnew_no */
  for (vno3 = -1, n = 0; n < VERTICES_PER_FACE; n++) {
    if (f1->v[n] == vno2) /* replace it with vnew */
    {
      f1->v[n] = vnew_no;
      vnew->f[vnew->num] = fno;
      vnew->n[vnew->num++] = (uchar)n;
    }
    else if (f1->v[n] != vno1) {
      vno3 = f1->v[n];
    }
  }
  
  VERTEX_TOPOLOGY * const v3 = &mris->vertices_topology[vno3];

  if (vno1 == Gdiag_no || vno2 == Gdiag_no || vno3 == Gdiag_no) {
    DiagBreak();
  }

  /* now construct f2 */

  /*  replace vno1 with vnew_no in f2 */
  for (n = 0; n < VERTICES_PER_FACE; n++) {
    if (f2->v[n] == vno1) /* replace it with vnew */
    {
      f2->v[n] = vnew_no;
      vnew->f[vnew->num] = fnew_no;
      vnew->n[vnew->num++] = (uchar)n;
    }
  }

  /* now replace f1 in vno2 with f2 */
  for (n = 0; n < v2->num; n++)
    if (v2->f[n] == fno) {
      v2->f[n] = fnew_no;
    }

  /* add new face and edge connected to new vertex to v3 */
  memmove(flist, v3->f, v3->num  * sizeof(v3->f[0]));
  memmove(vlist, v3->v, v3->vnum * sizeof(v3->v[0]));
  memmove(nlist, v3->n, v3->num  * sizeof(v3->n[0]));
  free(v3->f);
  free(v3->v);
  free(v3->n);
  v3->v = (int *)calloc(v3->vnum + 1, sizeof(int));
  if (!v3->v) ErrorExit(ERROR_NO_MEMORY, "mrisDivideFace: could not allocate %d vertices", v3->vnum);
  v3->f = (int *)calloc(v3->num + 1, sizeof(int));
  if (!v3->f) ErrorExit(ERROR_NO_MEMORY, "mrisDivideFace: could not allocate %d faces", v3->num);
  v3->n = (uchar *)calloc(v3->num + 1, sizeof(uchar));
  if (!v3->n) ErrorExit(ERROR_NO_MEMORY, "mrisDivideFace: could not allocate %d nbrs", v3->n);
  memmove(v3->f, flist, v3->num * sizeof(v3->f[0]));
  memmove(v3->n, nlist, v3->num * sizeof(v3->n[0]));
  memmove(v3->v, vlist, v3->vnum * sizeof(v3->v[0]));
  v3->v[v3->vnum++] = vnew_no;
  v3->vtotal = v3->vnum;
  v3->f[v3->num] = fnew_no;

  /*  find position of v3 in new face f2 */
  for (n = 0; n < VERTICES_PER_FACE; n++) {
    if (f2->v[n] == vno3) {
      v3->n[v3->num] = n;
      break;
    }
  }
  if (n >= VERTICES_PER_FACE) fprintf(stderr, "could not find v3 (%d) in new face %d!\n", vno3, fnew_no);
  v3->num++;

  if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON) {
    fprintf(stdout, "face %d: (%d, %d, %d)\n", fno, f1->v[0], f1->v[1], f1->v[2]);
    fprintf(stdout, "face %d: (%d, %d, %d)\n", fnew_no, f2->v[0], f2->v[1], f2->v[2]);
  }

  mrisInitializeNeighborhood(mris, vno3);

  return (NO_ERROR);
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int mrisStoreVtotalInV3num(MRI_SURFACE *mris)
{
  int vno;

  for (vno = 0; vno < mris->nvertices; vno++) {
    VERTEX_TOPOLOGY * const v = &mris->vertices_topology[vno];
    v->v3num = v->vtotal;
  }
  return (NO_ERROR);
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  restore ripflag to 0 (NOTE: Won't undo MRISremoveRipped!!)
  ------------------------------------------------------*/
int MRISunrip(MRI_SURFACE *mris)
{
  int vno, fno;

  for (vno = 0; vno < mris->nvertices; vno++) {
    mris->vertices[vno].ripflag = 0;
  }
  for (fno = 0; fno < mris->nfaces; fno++) {
    mris->faces[fno].ripflag = 0;
  }
  return (NO_ERROR);
}


int MRIScountTotalNeighbors(MRI_SURFACE *mris, int nsize)
{
  int vno, total_nbrs;

  for (total_nbrs = vno = 0; vno < mris->nvertices; vno++) {
    VERTEX_TOPOLOGY const * const vt = &mris->vertices_topology[vno];
    VERTEX          const * const v  = &mris->vertices         [vno];
    if (v->ripflag) {
      continue;
    }
    switch (nsize) {
      case 0:
        total_nbrs++;
        break;
      case 1:
        total_nbrs += vt->vnum;
        break;
      case 2:
        total_nbrs += vt->v2num;
        break;
      case 3:
        total_nbrs += vt->v3num;
        break;
      default:
        ErrorExit(ERROR_UNSUPPORTED, "MRIScountNeighbors(%d): max nbhd size = 3", nsize);
    }
  }
  return (total_nbrs);
}

/*!
  \fn int MRIScountRipped(MRIS *mris)
  \brief Returns the total number of ripped vertices
 */
int MRIScountRipped(MRIS *mris)
{
  int nripped=0, vno;
  for (vno = 0 ; vno < mris->nvertices ; vno++){
    if(mris->vertices[vno].ripflag) nripped++;
  }
  return(nripped);
}
/*!
  \fn int MRIScountAllMarked(MRIS *mris)
  \brief Returns the total number of vertices have v->marked > 0
 */
int MRIScountAllMarked(MRIS *mris)
{
  int nmarked=0, vno;
  for (vno = 0 ; vno < mris->nvertices ; vno++){
    if(mris->vertices[vno].marked>0) nmarked++;
  }
  return(nmarked);
}
/*!
  \fn int MRIScountMarked(MRI_SURFACE *mris, int mark_threshold)
  \brief Returns the total number of non-ripped vertices 
  that have v->marked >= threshold
 */
int MRIScountMarked(MRI_SURFACE *mris, int mark_threshold)
{
  int vno, total_marked;
  VERTEX *v;

  for (total_marked = vno = 0; vno < mris->nvertices; vno++) {
    v = &mris->vertices[vno];
    if (v->ripflag) {
      continue;
    }
    if (v->marked >= mark_threshold) {
      total_marked++;
    }
  }
  return (total_marked);
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/

int MRISmarkRandomVertices(MRI_SURFACE *mris, float prob_marked)
{
  int vno;
  VERTEX *v;
  float r;

  for (vno = 0; vno < mris->nvertices; vno++) {
    v = &mris->vertices[vno];
    if (v->ripflag) {
      continue;
    }
    r = randomNumber(0.0, 1.0);
    if (r < prob_marked) {
      v->marked = 1;
    }
  }
  return (NO_ERROR);
}
/*!
  \fn int MRISclearMarks(MRI_SURFACE *mris)
  \brief Sets v->marked=0 for all unripped vertices
*/
int MRISclearMarks(MRI_SURFACE *mris)
{
  int vno;
  VERTEX *v;

  for (vno = 0; vno < mris->nvertices; vno++) {
    v = &mris->vertices[vno];
    if (v->ripflag) {
      continue;
    }
    v->marked = 0;
  }
  return (NO_ERROR);
}
/*!
  \fn int MRISclearMarks(MRI_SURFACE *mris)
  \brief Sets v->marked2=0 for all unripped vertices
*/
int MRISclearMark2s(MRI_SURFACE *mris)
{
  int vno;
  VERTEX *v;

  for (vno = 0; vno < mris->nvertices; vno++) {
    v = &mris->vertices[vno];
    if (v->ripflag) {
      continue;
    }
    v->marked2 = 0;
  }
  return (NO_ERROR);
}

int MRISclearFaceMarks(MRIS *mris)
{
  int fno;
  FACE *f;

  for (fno = 0; fno < mris->nfaces; fno++) {
    f = &mris->faces[fno];
    f->marked = 0;
  }
  return (NO_ERROR);
}

/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int MRISclearAnnotations(MRI_SURFACE *mris)
{
  int vno;
  VERTEX *v;

  for (vno = 0; vno < mris->nvertices; vno++) {
    v = &mris->vertices[vno];
    if (v->ripflag) {
      continue;
    }
    v->annotation = 0;
  }
  return (NO_ERROR);
}
/*-----------------------------------------------------
  Parameters:

  Returns value:

  Description
  ------------------------------------------------------*/
int MRISsetMarks(MRI_SURFACE *mris, int mark)
{
  int vno;
  VERTEX *v;

  for (vno = 0; vno < mris->nvertices; vno++) {
    v = &mris->vertices[vno];
    if (v->ripflag) {
      continue;
    }
    v->marked = mark;
  }
  return (NO_ERROR);
}
