import numpy as np

from . import error


def hausdorffDistance(vol1, vol2, thresh=0.01, measure=np.max):
    """
    The Hausdorff distance between two volumes. Computes the max of
    the minimum euclidian distances between bordering voxels of
    the two input volumes. The mean minimum distance can be reported
    instead of the max minimum distance by setting the `measure` function
    argument to `np.mean`.

    Args:
        vol1: Image 1 in a 3D numpy array.
        vol2: Image 2 in a 3D numpy array.
        thresh (optional): Input volume threshold. Defaults to 0.01.
        measure (optional): Function used to calculate the final
            distance reported. Defaults to `np.max`.

    Returns:
        The Hausdorff distance.

    """
    # temporarily import these within the function
    from scipy.ndimage.morphology import binary_erosion
    from scipy.spatial.distance import cdist
    # make sure inputs are the same shape
    if vol1.shape != vol2.shape:
      error('hausdorffDistance - inputs must have the same shape')
      return np.NaN
    # find borders by eroding binary masks
    mask1 = vol1 >= thresh
    mask2 = vol2 >= thresh
    border1 = np.logical_and(mask1, np.logical_not(binary_erosion(mask1)))
    border2 = np.logical_and(mask2, np.logical_not(binary_erosion(mask2)))
    # compute voxel distances
    dists = cdist(np.vstack(np.nonzero(border1)).T, np.vstack(np.nonzero(border2)).T)
    return measure(np.concatenate((np.amin(dists, axis=0), np.amin(dists, axis=1))))
