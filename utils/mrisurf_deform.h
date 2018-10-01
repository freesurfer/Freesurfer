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

#include "mrisurf_vals.h"

#define VERTEX_INTERIOR 1
#define VERTEX_BORDER   2
#define VERTEX_CHULL    3

int mrisStoreVtotalInV3num(MRI_SURFACE *mris);

int mrisCalculateCanonicalFaceCentroid(MRI_SURFACE *mris, int fno, float *px, float *py, float *pz);

int mrisFlipPatch(MRI_SURFACE *mris);
int mrisTearStressedRegions(MRI_SURFACE *mris, INTEGRATION_PARMS *parms);
int mrisSmoothBoundaryNormals(MRI_SURFACE *mris, int niter);


double mrisComputeCorrelationError              (MRI_SURFACE *mris, INTEGRATION_PARMS *parms, int use_stds);
double mrisComputeCorrelationErrorTraceable     (MRI_SURFACE *mris, INTEGRATION_PARMS *parms, int use_stds, bool trace);
double mrisComputeDistanceError                 (MRI_SURFACE *mris, INTEGRATION_PARMS *parms);
double mrisComputeDuraError                     (MRI_SURFACE *mris, INTEGRATION_PARMS *parms);
double mrisComputeExpandwrapError               (MRI_SURFACE *mris, MRI *mri_brain, double l_expandwrap, double target_radius);
double mrisComputeIntensityError                (MRI_SURFACE *mris, INTEGRATION_PARMS *parms);
double mrisComputeIntensityGradientError        (MRI_SURFACE *mris, INTEGRATION_PARMS *parms);
double mrisComputeShrinkwrapError               (MRI_SURFACE *mris, MRI *mri_brain, double l_shrinkwrap);
double mrisComputeSphereError                   (MRI_SURFACE *mris, double l_sphere, double a);
double mrisComputeTargetLocationError           (MRI_SURFACE *mris, INTEGRATION_PARMS *parms);
double mrisComputeVectorCorrelationError        (MRI_SURFACE *mris, INTEGRATION_PARMS *parms, int use_stds);

double mrisComputeError                         (MRI_SURFACE *mris, INTEGRATION_PARMS *parms,
                                                                                           float *parea_rms,
                                                                                           float *pangle_rms,
                                                                                           float *pcurv_rms,
                                                                                           float *pdist_rms,
                                                                                           float *pcorr_rms);

double mrisComputeAshburnerTriangleEnergy       (MRI_SURFACE *mris, double l_ashburner_triangle, INTEGRATION_PARMS *parms);
double mrisComputeLaplacianEnergy               (MRI_SURFACE *mris);
double mrisComputeNonlinearSpringEnergy         (MRI_SURFACE *mris,                              INTEGRATION_PARMS *parms);
double mrisComputeRepulsiveEnergy               (MRI_SURFACE *mris, double l_repulse, MHT *mht_v_current, MHT *mht_f_current);
double mrisComputeRepulsiveRatioEnergy          (MRI_SURFACE *mris, double l_repulse);
double mrisComputeSpringEnergy                  (MRI_SURFACE *mris);
double mrisComputeSurfaceRepulsionEnergy        (MRI_SURFACE *mris, double l_repulse, MHT *mht);
double mrisComputeTangentialSpringEnergy        (MRI_SURFACE *mris);
double mrisComputeThicknessMinimizationEnergy   (MRI_SURFACE *mris, double l_thick_min,          INTEGRATION_PARMS *parms);
double mrisComputeThicknessNormalEnergy         (MRI_SURFACE *mris, double l_thick_normal,       INTEGRATION_PARMS *parms);
double mrisComputeThicknessParallelEnergy       (MRI_SURFACE *mris, double l_thick_parallel,     INTEGRATION_PARMS *parms);
double mrisComputeThicknessSmoothnessEnergy     (MRI_SURFACE *mris, double l_repulse,            INTEGRATION_PARMS *parms);
double mrisComputeThicknessSpringEnergy         (MRI_SURFACE *mris, double l_thick_spring,       INTEGRATION_PARMS *parms);

int    mrisComputeAngleAreaTerms                (MRI_SURFACE *mris,                              INTEGRATION_PARMS *parms);
int    mrisComputeAshburnerTriangleTerm         (MRI_SURFACE *mris, double l_ashburner_triangle, INTEGRATION_PARMS *parms);
int    mrisComputeBorderTerm                    (MRI_SURFACE *mris, double l_border);
int    mrisComputeConvexityTerm                 (MRI_SURFACE *mris, double l_convex);
int    mrisComputeCorrelationTerm               (MRI_SURFACE *mris,                              INTEGRATION_PARMS *parms);
int    mrisComputeDistanceTerm                  (MRI_SURFACE *mris,                              INTEGRATION_PARMS *parms);
int    mrisComputeExpandwrapTerm                (MRI_SURFACE *mris, MRI *mri_brain, double l_expandwrap);
int    mrisComputeExpansionTerm                 (MRI_SURFACE *mris, double l_expand);
int    mrisComputeIntensityGradientTerm         (MRI_SURFACE *mris, double l_grad, MRI *mri_brain, MRI *mri_smooth);
int    mrisComputeIntensityTerm                 (MRI_SURFACE *mris, double l_intensity, MRI *mri_brain, MRI *mri_smooth, double sigma, INTEGRATION_PARMS *parms);
int    mrisComputeLinkTerm                      (MRI_SURFACE *mris, double l_spring, int    pial);
int    mrisComputeMaxSpringTerm                 (MRI_SURFACE *mris, double l_spring);
int    mrisComputeNonlinearAreaTerm             (MRI_SURFACE *mris,                              INTEGRATION_PARMS *parms);
int    mrisComputeNonlinearDistanceTerm         (MRI_SURFACE *mris,                              INTEGRATION_PARMS *parms);
int    mrisComputeNonlinearSpringTerm           (MRI_SURFACE *mris, double l_nlspring,           INTEGRATION_PARMS *parms);
int    mrisComputeNonlinearTangentialSpringTerm (MRI_SURFACE *mris, double l_spring, double min_dist);
int    mrisComputeNormalSpringTerm              (MRI_SURFACE *mris, double l_spring);
int    mrisComputePolarCorrelationTerm          (MRI_SURFACE *mris,                              INTEGRATION_PARMS *parms);
int    mrisComputePolarVectorCorrelationTerm    (MRI_SURFACE *mris,                              INTEGRATION_PARMS *parms);
int    mrisComputeQuadraticCurvatureTerm        (MRI_SURFACE *mris, double l_curv);
int    mrisComputeRepulsiveRatioTerm            (MRI_SURFACE *mris, double l_repulse, MHT *mht_v);
int    mrisComputeRepulsiveTerm                 (MRI_SURFACE *mris, double l_repulse, MHT *mht_v, MHT *mht_f);
int    mrisComputeShrinkwrapTerm                (MRI_SURFACE *mris, MRI *mri_brain, double l_shrinkwrap);
int    mrisComputeSphereTerm                    (MRI_SURFACE *mris, double l_sphere, float radius, int    explode_flag);
int    mrisComputeSurfaceNormalIntersectionTerm (MRI_SURFACE *mris, MHT *mht, double l_norm, double max_dist);
int    mrisComputeSurfaceRepulsionTerm          (MRI_SURFACE *mris, double l_repulse, MHT *mht);
int    mrisComputeTangentialSpringTerm          (MRI_SURFACE *mris, double l_spring);
int    mrisComputeTargetLocationTerm            (MRI_SURFACE *mris, double l_location,           INTEGRATION_PARMS *parms);
int    mrisComputeThicknessMinimizationTerm     (MRI_SURFACE *mris, double l_thick_min,          INTEGRATION_PARMS *parms);
int    mrisComputeThicknessNormalTerm           (MRI_SURFACE *mris, double l_thick_normal,       INTEGRATION_PARMS *parms);
int    mrisComputeThicknessParallelTerm         (MRI_SURFACE *mris, double l_thick_parallel,     INTEGRATION_PARMS *parms);
int    mrisComputeThicknessSmoothnessTerm       (MRI_SURFACE *mris, double l_tsmooth,            INTEGRATION_PARMS *parms);
int    mrisComputeThicknessSpringTerm           (MRI_SURFACE *mris, double l_thick_spring,       INTEGRATION_PARMS *parms);
int    mrisComputeVectorCorrelationTerm         (MRI_SURFACE *mris,                              INTEGRATION_PARMS *parms);

int    mrisComputeWhichSurfaceRepulsionTerm     (MRI_SURFACE *mris, double l_repulse, MHT *mht, int which, float max_dot);

int mrisLogStatus(MRI_SURFACE *mris, INTEGRATION_PARMS *parms, FILE *fp, float dt, float old_sse);

int mrisOrientSurface     (MRI_SURFACE *mris);
int mrisOrientPlane       (MRI_SURFACE *mris);
int mrisRemoveNegativeArea(MRI_SURFACE *mris, INTEGRATION_PARMS *parms, int base_averages, float min_area_pct, int max_passes);
    
static int transform(float *xptr, float *yptr, float *zptr, float nx, float ny, float nz, float d)
{
  float x = *xptr, y = *yptr, z = *zptr;

  *zptr = nx * x + ny * y + nz * z;
  *yptr = -ny / d * x + nx / d * y;
  *xptr = nx * nz / d * x + ny * nz / d * y - d * z;
  /*
    printf("transform {%f,%f,%f} -> {%f,%f,%f}\n",
    x,y,z,*xptr,*yptr,*zptr);
  */
  return (NO_ERROR);
}

int mrisComputeDuraTerm(MRI_SURFACE *mris, double l_dura, MRI *mri_dura, double dura_thresh);

int mrisComputeSpringTerm           (MRI_SURFACE *mris, double l_spring);
int mrisComputeLaplacianTerm        (MRI_SURFACE *mris, double l_laplacian);
int mrisComputeNormalizedSpringTerm (MRI_SURFACE *mris, double l_spring);

int mrisProjectSurface                     (MRI_SURFACE *mris);
int mrisApplyTopologyPreservingGradient    (MRI_SURFACE *mris, double dt, int which_gradient);
int mrisApplyGradientPositiveAreaPreserving(MRI_SURFACE *mris, double dt);
int mrisApplyGradientPositiveAreaMaximizing(MRI_SURFACE *mris, double dt);

int mrisProjectSurface(MRI_SURFACE *mris);
int mrisApplyTopologyPreservingGradient(MRI_SURFACE *mris, double dt, int which_gradient);
int mrisCountCompressed(MRI_SURFACE *mris, double min_dist);
int mrisComputePlaneTerm(MRI_SURFACE *mris, double l_plane, double l_spacing);
int mrisComputeBorderTerm(MRI_SURFACE *mris, double l_border);
int mrisComputeConvexityTerm(MRI_SURFACE *mris, double l_convex);
int mrisComputeMaxSpringTerm(MRI_SURFACE *mris, double l_max_spring);
int mrisComputeAngleAreaTerms(MRI_SURFACE *mris, INTEGRATION_PARMS *parms);
double mrisComputeHistoNegativeLikelihood(MRI_SURFACE *mris, INTEGRATION_PARMS *parms);
double mrisComputeNegativeLogPosterior(MRI_SURFACE *mris, INTEGRATION_PARMS *parms, int *pnvox);
double mrisComputeNegativeLogPosterior2D(MRI_SURFACE *mris, INTEGRATION_PARMS *parms, int *pnvox);
double mrisRmsDistanceError(MRI_SURFACE *mris);
int mrisComputeTargetLocationTerm(MRI_SURFACE *mris, double l_location, INTEGRATION_PARMS *parms);
int mrisComputeIntensityTerm_mef(MRI_SURFACE *mris,
                                        double l_intensity,
                                        MRI *mri_30,
                                        MRI *mri_5,
                                        double sigma_global,
                                        float weight30,
                                        float weight5,
                                        INTEGRATION_PARMS *parms);
int mrisComputeShrinkwrapTerm(MRI_SURFACE *mris, MRI *mri_brain, double l_shrinkwrap);
int mrisComputeExpandwrapTerm(MRI_SURFACE *mris, MRI *mri_brain, double l_expandwrap);
int mrisComputeIntensityGradientTerm(MRI_SURFACE *mris, double l_grad, MRI *mri_brain, MRI *mri_smooth);
int mrisComputeSurfaceRepulsionTerm(MRI_SURFACE *mris, double l_repulse, MHT *mht);
int mrisComputeHistoTerm(MRI_SURFACE *mris, INTEGRATION_PARMS *parms);
int mrisComputePosteriorTerm(MRI_SURFACE *mris, INTEGRATION_PARMS *parms);
int mrisComputePosterior2DTerm(MRI_SURFACE *mris, INTEGRATION_PARMS *parms);
int mrisComputeWhichSurfaceRepulsionTerm(
    MRI_SURFACE *mris, double l_repulse, MHT *mht, int which, float dot_thresh);
int mrisComputeLaplacianTerm(MRI_SURFACE *mris, double l_lap);
int mrisAverageSignedGradients(MRI_SURFACE *mris, int num_avgs);
int mrisComputeNormalizedSpringTerm(MRI_SURFACE *const mris, double const l_spring);
int mrisComputeRepulsiveTerm(MRI_SURFACE *mris, double l_repulse, MHT *mht, MHT *mht_faces);
int mrisComputeThicknessSmoothnessTerm(MRI_SURFACE *mris, double l_tsmooth, INTEGRATION_PARMS *parms);
int mrisComputeThicknessMinimizationTerm(MRI_SURFACE *mris, double l_thick_min, INTEGRATION_PARMS *parms);
int mrisComputeThicknessParallelTerm(MRI_SURFACE *mris, double l_thick_parallel, INTEGRATION_PARMS *parms);
int mrisComputeNormalSpringTerm(MRI_SURFACE *mris, double l_spring);
int mrisComputeQuadraticCurvatureTerm(MRI_SURFACE *mris, double l_curv);
int mrisComputeNonlinearSpringTerm(MRI_SURFACE *mris, double l_nlspring, INTEGRATION_PARMS *parms);
int mrisComputeTangentialSpringTerm(MRI_SURFACE *mris, double l_spring);
int mrisComputeNonlinearTangentialSpringTerm(MRI_SURFACE *mris, double l_spring, double min_dist);
int mrisComputePositioningGradients(MRI_SURFACE *mris, INTEGRATION_PARMS *parms);
int mrisComputeLinkTerm(MRI_SURFACE *mris, double l_link, int pial);
double mrisRmsValError_mef(MRI_SURFACE *mris, MRI *mri_30, MRI *mri_5, float weight30, float weight5);
double mrisComputeSSE_MEF(
    MRI_SURFACE *mris, INTEGRATION_PARMS *parms, MRI *mri30, MRI *mri5, double weight30, double weight5, MHT *mht);
int mrisComputeIntensityTerm_mef(MRI_SURFACE *mris,
                                        double l_intensity,
                                        MRI *mri_30,
                                        MRI *mri_5,
                                        double sigma_global,
                                        float weight30,
                                        float weight5,
                                        INTEGRATION_PARMS *parms);
int mrisComputeSurfaceNormalIntersectionTerm(MRI_SURFACE *mris, MHT *mht, double l_norm, double max_dist);

int MRISrestoreExtraGradients(MRI_SURFACE *mris);

int mrisLogStatus(MRI_SURFACE *mris, INTEGRATION_PARMS *parms, FILE *fp, float dt, float old_sse);


