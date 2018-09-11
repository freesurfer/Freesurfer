#pragma once
/*
 * @file utilities common to mrisurf*.c but not used outside them
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
// Configurable support for checking that some of the parallel loops get the same
// results regardless of thread count
//
// Two common causes of this being false are
//          floating point reductions
//          random numbers being generated by parallel code 
//

static const int debugNonDeterminism = 0;
extern int fix_vertex_area;


//#define BEVIN_REPRODUCIBLES_CHECK
#ifdef BEVIN_REPRODUCIBLES_CHECK

    #define BEVIN_MRISCOMPUTENORMALS_CHECK
    #define BEVIN_MRISCOMPUTETRIANGLEPROPERTIES_CHECK
    #define BEVIN_MRISCOMPUTEDISTANCEERROR_CHECK
    #define BEVIN_MRISCOMPUTENONLINEARAREASSE_CHECK
    #define BEVIN_MRISCOMPUTESSE_CHECK
#endif


// Configurable support for enabling the new code that does get reproducible results
//
#define BEVIN_MRISCOMPUTENORMALS_REPRODUCIBLE
#define BEVIN_MRISCOMPUTETRIANGLEPROPERTIES_REPRODUCIBLE
#define BEVIN_MRISAVGINTERVERTEXDIST_REPRODUCIBLE
#define BEVIN_MRISORIENTELLIPSOID_REPRODUCIBLE
#define BEVIN_MRISCOMPUTECORRELATIONERROR_REPRODUCIBLE
#define BEVIN_MRISCOMPUTEDISTANCEERROR_REPRODUCIBLE
#define BEVIN_MRISCOMPUTENONLINEARAREASSE_REPRODUCIBLE
#define BEVIN_MRISCOMPUTESSE_REPRODUCIBLE
#define BEVIN_MRISCOMPUTEQUADRATICCURVATURESSE_REPRODUCIBLE


// Includes
//
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef __APPLE__
#include <mcheck.h>
#endif

#include "mri.h"
#include "mrisurf.h"
#include "mrishash_internals.h"
#include "MRISrigidBodyAlignGlobal.h"
#include "chklc.h"
#include "cma.h"
#include "colortab.h"
#include "const.h"
#include "diag.h"
#include "error.h"
#include "fio.h"
#include "fnv_hash.h"
#include "gifti_local.h"
#include "icosahedron.h"
#include "machine.h"
#include "macros.h"
#include "matrix.h"
#include "mri2.h"
#include "mri_circulars.h"
#include "mri_identify.h"
#include "proto.h"
#include "realm.h"
#include "selxavgio.h"
#include "sort.h"
#include "stats.h"
#include "tags.h"
#include "talairachex.h"
#include "timer.h"
#include "topology/topo_parms.h"
#include "transform.h"
#include "tritri.h"
#include "utils.h"
#include "voxlist.h"

#include "annotation.h"

#include "romp_support.h"

#ifdef SIGN
#undef SIGN /* get rid of silly NRC version */
#endif



#define DMALLOC 0

#if DMALLOC
#include "dmalloc.h"
#endif

#ifndef SQR
#define SQR(x) ((x) * (x))
#endif

#define F_DOT(a, b) (a[0] * b[0] + a[1] * b[1] + a[2] * b[2])
#define F_CROSS(a, b, d) \
  (d[0] = a[1] * b[2] - b[1] * a[2], d[1] = a[2] * b[0] - b[2] * a[0], d[2] = a[0] * b[1] - b[0] * a[1])

#ifndef SIGN
#define SIGN(x) (((x) > 0) ? 1.0 : -1.0)
#endif
#ifndef SQR3
#define SQR3(x) (SQR(x[0]) + SQR(x[1]) + SQR(x[2]))
#endif
#ifndef NORM3
#define NORM3(x) (sqrt(SQR3(x)))
#endif
#ifndef MAX
#define MAX(a, b) (((a) < (b)) ? b : a)
#endif
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? a : b)
#endif
#ifndef MAX3
#define MAX3(a, b, c) (MAX(a, MAX(b, c)))
#endif
#ifndef MIN3
#define MIN3(a, b, c) (MIN(a, MIN(b, c)))
#endif

#ifndef V4_LOAD
#define V4_LOAD(v, x, y, z, r) (VECTOR_ELT(v, 1) = x, VECTOR_ELT(v, 2) = y, VECTOR_ELT(v, 3) = z, VECTOR_ELT(v, 4) = r);
#endif

static float mrisNormalize(float v[3])
{
  float d = sqrt(SQR(v[0]) + SQR(v[1]) + SQR(v[2]));
  
  if (d > 0) {
    v[0] /= d;
    v[1] /= d;
    v[2] /= d;
  }
  
  return d;
}


//#define ILL_CONDITIONED   5000.0
#define ILL_CONDITIONED 500000.0

#define MAX_INT_FACES 100000


static int closeEnough(float a, float b) {
  float mag = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
  if (mag < 1.0e-6) return 1;
  if (fabs(a-b) < mag*1.0e-2) return 1;
  return 0; 
}


int mrisCheckSurfaceNbrs(MRI_SURFACE *mris);


static int project_point_onto_sphere(float cx, float cy, float cz, float radius, float *pcx, float *pcy, float *pcz)
{
  float x2, y2, z2, d, dist, dx, dy, dz;

  x2 = cx * cx;
  y2 = cy * cy;
  z2 = cz * cz;
  dist = sqrt(x2 + y2 + z2);
  if (FZERO(dist)) {
    d = 0;
  }
  else {
    d = 1 - radius / dist;
  }
  dx = d * cx;
  dy = d * cy;
  dz = d * cz;
  *pcx = cx - dx;
  *pcy = cy - dy;
  *pcz = cz - dz;
  return (NO_ERROR);
}

// mostly for diagnostics
#define MAXVERTICES 10000000
#define MAXFACES (2 * MAXVERTICES)
#define MAX_NBHD_VERTICES 20000


// uncomment this to expose code which shows timings of gpu activities:
//#define FS_CUDA_TIMINGS


/*---------------------------- STRUCTURES -------------------------*/

/*---------------------------- CONSTANTS -------------------------*/

#define D_DIST          0.1         // sampling distance along tangent plane for computing derivatives

#define UNFOUND_DIST    1000000.0f  // much longer than any surface dimension

#define MAX_NBRS 10000

#define REPULSE_K 1.0
#define REPULSE_E 0.25  // was 0.5
    // if (dist+REPULSE_E) < 1 it will have a big effect
    // bigger distances decay rapidly

#define NO_NEG_DISTANCE_TERM 0
#define ONLY_NEG_AREA_TERM 1
#define ANGLE_AREA_SCALE 0.0

#define ORIG_AREAS 0
#define CURRENT_AREAS 1
#define AVERAGE_AREAS 0

#define CURV_SCALE 1000.0
#define MIN_DT_SCALE 0.01

#if 0
#define MAX_SMALL 10
#define TOTAL_SMALL (4 * MAX_SMALL)
#else
#define MAX_SMALL 50000
#define TOTAL_SMALL 15000
#endif

#define METRIC_SCALE 1

#define MAX_NBHD_SIZE 200
#define MAX_NEG_AREA_PCT 0.005f


#define MAX_ASYNCH_MM     0.3
#define MAX_ASYNCH_NEW_MM 0.3


#define NOT_USED                            0 // not used
#define USED_IN_TESSELLATION                1 // used in the final tessellation
#define USED_IN_ORIGINAL_TESSELLATION       2 // used in the original tessellation but not the new one (at first)
#define USED_IN_NEW_TESSELLATION            3 // used in the new tessellation (was NOT_USED before)
#define USED_IN_BOTH_TESSELLATION           4 // used in both tessellations (was USED_IN_NEW_TESSELLATION before)
#define USED_IN_NEW_TEMPORARY_TESSELLATION  5 // used in a temporary retessellation (was NOT_USED before)
#define USED_IN_BOTH_TEMPORARY_TESSELLATION 6 // used in a temporary retessellation (was USED_IN_ORIGINAL_RETESSELLATION before)


/* edge intersection onto the sphere */
#define SPHERE_INTERSECTION 1

/* speeds things up */
#define MATRIX_ALLOCATION 1

/* add extra vertices to avoid topological inconsistencies */
#define ADD_EXTRA_VERTICES 0
/* defect becomes a enclosed patch requires ADD_VERTEX_VERTICES to avoid top. inconsistencies */
#define FIND_ENCLOSING_LOOP ADD_EXTRA_VERTICES

/* solve the neighboring defect problem
   becomes obsolete because of ADD_EXTRA_VERTICES */
#define MERGE_NEIGHBORING_DEFECTS 1

/* limit the convex hull to the strict minimum set of first neighbors*/
#define SMALL_CONVEX_HULL 1

/* */
#define MRIS_FIX_TOPOLOGY_ERROR_MODE 1
#define DEBUG_HOMEOMORPHISM 0

#define WHICH_OUTPUT stderr

#define IMAGES_PER_SURFACE 3 /* mean, variance, and dof */
#define SURFACES sizeof(curvature_names) / sizeof(curvature_names[0])
#define PARAM_IMAGES (IMAGES_PER_SURFACE * SURFACES)


extern int mrisurf_activeRealmTreesSize;
extern int mrisurf_orig_clock;

static bool hasActiveRealmTrees() {
    return mrisurf_activeRealmTreesSize > 0;
}

#define CHANGES_ORIG                                                                        \
    if (hasActiveRealmTrees()) {                                                            \
        static int latest;                                                                  \
        if (mrisurf_orig_clock != latest) {                                                 \
            latest = mrisurf_orig_clock;                                                    \
            fprintf(stderr,"%s:%d changes orig <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n",     \
                __FILE__,__LINE__);                                                         \
        }                                                                                   \
    }                                                                                       \
    // end of macro


// mrisurf_base
//
extern double NEG_AREA_K;
/* limit the size of the ratio so that the exp() doesn't explode */
#define MAX_NEG_RATIO (400 / NEG_AREA_K)

VOXEL_LIST **vlst_alloc(MRI_SURFACE *mris, int max_vox);
int vlst_free(MRI_SURFACE *mris, VOXEL_LIST ***pvl);
int vlst_enough_data(MRI_SURFACE *mris, int vno, VOXEL_LIST *vl, double displacement);
int vlst_add_to_list(VOXEL_LIST *vl_src, VOXEL_LIST *vl_dst);

int edgesIntersect(MRI_SURFACE *mris, EDGE *edge1, EDGE *edge2);

int load_orig_triangle_vertices(MRI_SURFACE *mris, int fno, double U0[3], double U1[3], double U2[3]);
int load_triangle_vertices     (MRI_SURFACE *mris, int fno, double U0[3], double U1[3], double U2[3], int which);

int mrisDirectionTriangleIntersection(
    MRI_SURFACE *mris, float x0, float y0, float z0, float nx, float ny, float nz, MHT *mht, double *pdist, int vno);
int mrisAllNormalDirectionCurrentTriangleIntersections(
    MRI_SURFACE *mris, VERTEX *v, MHT *mht, double *pdist, int *flist);

int mrisWriteSnapshots(MRI_SURFACE *mris, INTEGRATION_PARMS *parms, int t);
int mrisWriteSnapshot (MRI_SURFACE *mris, INTEGRATION_PARMS *parms, int t);
    // not yet moved there, because saves and restores vertices

#define MAX_4_NEIGHBORS 100
#define MAX_3_NEIGHBORS 70
#define MAX_2_NEIGHBORS 20
#define MAX_1_NEIGHBORS 8
#define MAX_NEIGHBORS (10000)

extern const float * sigmas;
extern       double nsigmas;

bool MRISreallocVertices            (MRI_SURFACE* mris, int max_vertices, int nvertices);
void MRISgrowNVertices              (MRI_SURFACE* mris, int nvertices);
void MRIStruncateNVertices          (MRI_SURFACE* mris, int nvertices);
void MRISremovedVertices            (MRI_SURFACE* mris, int nvertices);
bool MRISreallocFaces               (MRI_SURFACE* mris, int max_faces, int nfaces);
bool MRISallocateFaces              (MRI_SURFACE* mris, int nfaces);
void MRISgrowNFaces                 (MRI_SURFACE* mris, int nfaces);
void MRIStruncateNFaces             (MRI_SURFACE* mris, int nfaces);
void MRISremovedFaces               (MRI_SURFACE* mris, int nfaces);
void MRISoverAllocVerticesAndFaces  (MRI_SURFACE* mris, int max_vertices, int max_faces, int nvertices, int nfaces);

void insertActiveRealmTree(MRIS const * const mris, RealmTree* realmTree, GetXYZ_FunctionType getXYZ);
void removeActiveRealmTree(RealmTree* realmTree);

extern int noteVnoMovedInActiveRealmTreesCount;
void noteVnoMovedInActiveRealmTrees              (MRIS const * const mris, int vno);
void notifyActiveRealmTreesChangedNFacesNVertices(MRIS const * const mris);

extern char *mrisurf_surface_names[3];
extern char *curvature_names[3];
int MRISsetCurvatureName(int nth, char *name);
int MRISprintCurvatureNames(FILE *fp);
