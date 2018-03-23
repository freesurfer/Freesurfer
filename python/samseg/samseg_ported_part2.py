import logging

from gems2python import GEMS2Python

import numpy as np
import scipy.io

from samseg.bias_correction import backprojectKroneckerProductBasisFunctions, \
    projectKroneckerProductBasisFunctions, computePrecisionOfKroneckerProductBasisFunctions
from samseg.dev_utils.debug_client import create_part2_inspection_team, run_test_cases, \
    create_checkpoint_manager, load_starting_fixture
from samseg.kvlWarpMesh import kvlWarpMesh
from samseg.kvl_merge_alphas import kvlMergeAlphas

logger = logging.getLogger(__name__)

eps = np.finfo(float).eps


def require_np_array(np_array):
    return np.require(np_array, requirements=['F_CONTIGUOUS', 'ALIGNED'])


def load_mat_file(filename):
    return scipy.io.loadmat(filename, struct_as_record=False, squeeze_me=True)


def ensure_dims(np_array, dims):
    if np_array.ndim < dims:
        return np.expand_dims(np_array, axis=dims)
    elif np_array.ndim == dims:
        return np_array


def samsegment_part2(
        modelSpecifications,
        optimizationOptions,
        part1_results_dict,
        checkpoint_manager=None
):
    biasFieldCoefficients = part1_results_dict['biasFieldCoefficients']
    colors = part1_results_dict['colors']
    FreeSurferLabels = part1_results_dict['FreeSurferLabels']
    imageBuffers = part1_results_dict['imageBuffers']
    kroneckerProductBasisFunctions = part1_results_dict['kroneckerProductBasisFunctions']
    mask = part1_results_dict['mask']
    names = part1_results_dict['names']
    numberOfBasisFunctions = part1_results_dict['numberOfBasisFunctions']
    numberOfClasses = part1_results_dict['numberOfClasses']
    numberOfContrasts = part1_results_dict['numberOfContrasts']
    numberOfGaussians = part1_results_dict['numberOfGaussians']
    numberOfGaussiansPerClass = part1_results_dict['numberOfGaussiansPerClass']
    transformMatrix = part1_results_dict['transformMatrix']
    voxelSpacing = part1_results_dict['voxelSpacing']
    transform = GEMS2Python.KvlTransform(np.asfortranarray(transformMatrix))

    numberOfMultiResolutionLevels = len(optimizationOptions.multiResolutionSpecification)
    for multiResolutionLevel in range(numberOfMultiResolutionLevels):
        logger.debug('multiResolutionLevel=%d', multiResolutionLevel)
        maximumNumberOfIterations = optimizationOptions.multiResolutionSpecification[
            multiResolutionLevel].maximumNumberOfIterations
        estimateBiasField = optimizationOptions.multiResolutionSpecification[multiResolutionLevel].estimateBiasField
        historyOfCost = [1 / eps]
        logger.debug('maximumNumberOfIterations: %d', maximumNumberOfIterations)
        # Downsample the images, the mask, the mesh, and the bias field basis functions
        # Must be integer
        downSamplingFactors = np.uint32(np.round(optimizationOptions.multiResolutionSpecification[
                                                     multiResolutionLevel].targetDownsampledVoxelSpacing / voxelSpacing))
        downSamplingFactors[downSamplingFactors < 1] = 1
        downSampledMask = mask[::downSamplingFactors[0], ::downSamplingFactors[1], ::downSamplingFactors[2]]
        downSampledMaskIndices = np.where(downSampledMask)
        activeVoxelCount = len(downSampledMaskIndices[0])
        downSampledImageBuffers = np.zeros(downSampledMask.shape + (numberOfContrasts,), order='F')
        for contrastNumber in range(numberOfContrasts):
            logger.debug('first time contrastNumber=%d', contrastNumber)
            # No image smoothing
            # TODO: Remove need to check this. Matlab implicitly lets you expand one dim, our python code should have the shape (x, y, z, numberOfContrasts)
            if imageBuffers.ndim == 3:
                imageBuffers = np.expand_dims(imageBuffers, axis=3)

            downSampledImageBuffers[:, :, :, contrastNumber] = imageBuffers[::downSamplingFactors[0],
                                                               ::downSamplingFactors[1],
                                                               ::downSamplingFactors[2],
                                                               contrastNumber]
        downSampledKroneckerProductBasisFunctions = [np.array(kroneckerProductBasisFunction[::downSamplingFactor])
                                                     for kroneckerProductBasisFunction, downSamplingFactor in
                                                     zip(kroneckerProductBasisFunctions, downSamplingFactors)]
        downSampledImageSize = downSampledImageBuffers[:, :, :, 0].shape
        # Read the atlas mesh to be used for this multi-resolution level, taking into account the downsampling to position it
        # correctly
        downSamplingTransformMatrix = np.diag(1. / downSamplingFactors)
        downSamplingTransformMatrix = np.pad(downSamplingTransformMatrix, (0, 1), mode='constant', constant_values=0)
        downSamplingTransformMatrix[3][3] = 1

        totalTransformationMatrix = downSamplingTransformMatrix @ transform.as_numpy_array

        mesh_collection = GEMS2Python.KvlMeshCollection()
        mesh_collection.read(optimizationOptions.multiResolutionSpecification[multiResolutionLevel].atlasFileName)
        mesh_collection.k = modelSpecifications.K
        mesh_collection.transform(GEMS2Python.KvlTransform(require_np_array(totalTransformationMatrix)))

        mesh = mesh_collection.reference_mesh

        # Get the initial mesh node positions, also transforming them back into template space
        # (i.e., undoing the affine registration that we applied) for later usage
        initialNodePositions = mesh.points
        numberOfNodes = len(initialNodePositions)
        tmp = np.linalg.solve(totalTransformationMatrix,
                              np.pad(initialNodePositions, ((0, 0), (0, 1)), mode='constant', constant_values=1).T).T
        initialNodePositionsInTemplateSpace = tmp[:, 0:3]
        # If this is not the first multi-resolution level, apply the warp computed during the previous level
        if multiResolutionLevel > 0:
            logger.debug('starting multiResolutionLevel=%d', multiResolutionLevel)
            # Get the warp in template space
            [initialNodeDeformationInTemplateSpace, initial_averageDistance, initial_maximumDistance] = kvlWarpMesh(
                optimizationOptions.multiResolutionSpecification[multiResolutionLevel - 1].atlasFileName,
                nodeDeformationInTemplateSpaceAtPreviousMultiResolutionLevel,
                optimizationOptions.multiResolutionSpecification[multiResolutionLevel].atlasFileName)

            logger.debug('alpha multiResolutionLevel={0}'.format(multiResolutionLevel))
            # Apply this warp on the mesh node positions in template space, and transform into current space
            logger.debug('beta multiResolutionLevel=%d', multiResolutionLevel)
            desiredNodePositionsInTemplateSpace = initialNodePositionsInTemplateSpace + initialNodeDeformationInTemplateSpace

            logger.debug('gamma multiResolutionLevel=%d', multiResolutionLevel)
            tmp = (totalTransformationMatrix @ np.pad(desiredNodePositionsInTemplateSpace, ((0, 0), (0, 1)), 'constant',
                                                      constant_values=1).T).T
            logger.debug('delta multiResolutionLevel=%d}', multiResolutionLevel)
            desiredNodePositions = tmp[:, 0:3]
            logger.debug('epsilon multiResolutionLevel=%d', multiResolutionLevel)
            mesh.points = require_np_array(desiredNodePositions)
            logger.debug('finish multiResolutionLevel=%d', multiResolutionLevel)
            if checkpoint_manager:
                checkpoint_manager.increment_and_save({
                    'desiredNodePositions': desiredNodePositions,
                    'tmp': tmp,
                    'desiredNodePositionsInTemplateSpace': desiredNodePositionsInTemplateSpace,
                    'nodeDeformationInTemplateSpaceAtPreviousMultiResolutionLevel': nodeDeformationInTemplateSpaceAtPreviousMultiResolutionLevel,
                    'initialNodeDeformationInTemplateSpace': initialNodeDeformationInTemplateSpace,
                }, 'multiresWarp')
        # Set priors in mesh to the reduced (super-structure) ones
        alphas = mesh.alphas
        reducedAlphas, _, _, _, _ = kvlMergeAlphas(alphas, names, modelSpecifications.sharedGMMParameters,
                                                   FreeSurferLabels, colors);
        if checkpoint_manager:
            checkpoint_manager.increment_and_save({'reducedAlphas': reducedAlphas}, 'reducedAlphas')
        mesh.alphas = reducedAlphas

        # Algorithm-wise, we're just estimating sets of parameters for one given data (MR scan) that is
        # known and fixed throughout. However, in terms of bias field correction it will be computationally
        # more efficient to pre-compute the bias field corrected version of the scan ("corrected" with
        # the current estimate of the bias field) once and pass that on to different routines instead of the
        # original data.
        # For convenience (although potentially a recipe for future bug introduction), I'm also keeping a
        # vectorized form of that around -- this will be useful in various places in the EM-parts. So
        # effectively I have two redundant variables "downSampledBiasCorrectedImageBuffers" and "biasCorrectedData"
        # that really just encode the variable "biasFieldCoefficients" and so need to be meticiously updated each time
        # "biasFieldCoefficients" is updated (!)
        downSampledBiasCorrectedImageBuffers = np.zeros(downSampledImageSize + (numberOfContrasts,), order='F')
        biasCorrectedData = np.zeros((activeVoxelCount, numberOfContrasts), order='F')

        # TODO: remove this ensure_dims when part 1 is done
        biasFieldCoefficients = ensure_dims(biasFieldCoefficients, 2)
        for contrastNumber in range(numberOfContrasts):
            logger.debug('second time contrastNumber=%d', contrastNumber)
            downSampledBiasField = backprojectKroneckerProductBasisFunctions(downSampledKroneckerProductBasisFunctions,
                                                                             biasFieldCoefficients[:, contrastNumber])
            tmp = downSampledImageBuffers[:, :, :, contrastNumber] - downSampledBiasField * downSampledMask
            downSampledBiasCorrectedImageBuffers[:, :, :, contrastNumber] = tmp
            biasCorrectedData[:, contrastNumber] = tmp[downSampledMaskIndices]
        # Compute a color coded version of the atlas prior in the atlas's current pose, i.e., *before*
        # we start deforming. We'll use this just for visualization purposes
        posteriors = np.zeros((activeVoxelCount, numberOfGaussians), order='F')
        # Easier to work with vector notation in the EM computations
        # reshape into a matrix
        data = np.zeros((activeVoxelCount, numberOfContrasts))
        for contrastNumber in range(numberOfContrasts):
            tmp = downSampledImageBuffers[:, :, :, contrastNumber]
            data[:, contrastNumber] = tmp[downSampledMaskIndices]
        # Main iteration loop over both EM and deformation
        for iterationNumber in range(maximumNumberOfIterations):
            logger.debug('iterationNumber=%d', iterationNumber)
            #
            # Part I: estimate Gaussian mixture model parameters, as well as bias field parameters using EM.
            #
            #
            # Get the priors at the current mesh position
            tmp = mesh.rasterize_2(downSampledImageSize, -1)
            priors = tmp[downSampledMaskIndices] / 65535
            # Start EM iterations.
            if ((multiResolutionLevel == 0) and (iterationNumber == 0)):
                #
                # Initialize the mixture parameters if this is the first time ever you run this
                means = np.zeros((numberOfGaussians, numberOfContrasts))
                variances = np.zeros((numberOfGaussians, numberOfContrasts, numberOfContrasts))
                mixtureWeights = np.zeros((numberOfGaussians, 1))
                for classNumber in range(numberOfClasses):
                    # Calculate the global weighted mean and variance of this class, where the weights are given by the prior
                    prior = priors[:, classNumber]
                    mean = data.T @ prior / np.sum(prior)
                    tmp = data - mean
                    prior = np.expand_dims(prior, 1)
                    variance = tmp.T @ (tmp * prior) / np.sum(prior)
                    if modelSpecifications.useDiagonalCovarianceMatrices:
                        # Force diagonal covariance matrices
                        variance = np.diag(np.diag(variance))
                    # Based on this, initialize the mean and variance of the individual Gaussian components in this class'
                    # mixture model: variances are simply copied from the global class variance, whereas the means are
                    # determined by splitting the [ mean-sqrt( variance ) mean+sqrt( variance ) ] domain into equal intervals,
                    # the middle of which are taken to be the means of the Gaussians. Mixture weights are initialized to be
                    # all equal.
                    #
                    # This actually creates a mixture model that mimics the single Gaussian quite OK-ish: to visualize this
                    # do e.g.,
                    numberOfComponents = numberOfGaussiansPerClass[classNumber]

                    for componentNumber in range(numberOfComponents):
                        gaussianNumber = sum(numberOfGaussiansPerClass[: classNumber]) + componentNumber
                        variances[gaussianNumber, :, :] = variance
                        intervalSize = 2 * np.sqrt(np.diag(variance)) / numberOfComponents
                        means[gaussianNumber, :] = (mean - np.sqrt(np.diag(variance)) + intervalSize / 2 + (
                            componentNumber) * intervalSize).T
                        mixtureWeights[gaussianNumber] = 1 / numberOfComponents
            # Also remember the overall data variance for later usage in a conjugate prior on the variances
            dataMean = np.mean(data)
            tmp = data - dataMean
            dataVariance = np.var(tmp)
            numberOfPseudoMeasurementsOfWishartPrior = 1
            pseudoVarianceOfWishartPrior = dataVariance / numberOfPseudoMeasurementsOfWishartPrior
            historyOfEMCost = [1 / eps]
            for EMIterationNumber in range(100):
                logger.debug('EMIterationNumber=%d', EMIterationNumber)
                #
                # E-step: compute the posteriors based on the current parameters.
                #
                for classNumber in range(numberOfClasses):
                    prior = priors[:, classNumber]
                    numberOfComponents = numberOfGaussiansPerClass[classNumber]
                    for componentNumber in range(numberOfComponents):
                        gaussianNumber = sum(numberOfGaussiansPerClass[:classNumber]) + componentNumber
                        mean = means[gaussianNumber, :].T
                        variance = variances[gaussianNumber, :, :]
                        L = np.linalg.cholesky(variance)
                        means_corrected_bias = biasCorrectedData.T - mean
                        if L.shape == (1, 1):
                            scale = 1.0 / L[0, 0]
                            tmp = means_corrected_bias * scale
                        else:
                            tmp = np.linalg.solve(L, means_corrected_bias)
                        tmp *= tmp
                        scaled_squared_mahalanobis_distances = np.sum(tmp, axis=0) * -0.5
                        sqrtDeterminantOfVariance = np.prod(np.diag(L))
                        scaling = 1.0 / (2 * np.pi) ** (
                                numberOfContrasts / 2) / sqrtDeterminantOfVariance
                        gaussianLikelihoods = np.exp(scaled_squared_mahalanobis_distances) * scaling
                        gaussianLikelihoods = gaussianLikelihoods.T
                        posteriors[:, gaussianNumber] = gaussianLikelihoods * (mixtureWeights[gaussianNumber] * prior)
                normalizer = np.sum(posteriors, axis=1) + eps
                posteriors = posteriors / np.expand_dims(normalizer, 1)

                minLogLikelihood = -np.sum(np.log(normalizer))
                intensityModelParameterCost = 0
                for gaussianNumber in range(numberOfGaussians):
                    variance = variances[gaussianNumber, :, :]
                    # Evaluate unnormalized Wishart distribution (conjugate prior on precisions) with parameters
                    #
                    #   scale matrix V = inv( pseudoVarianceOfWishartPrior * numberOfPseudoMeasurementsOfWishartPrior )
                    #
                    # and
                    #
                    #   degrees of freedom n = numberOfPseudoMeasurementsOfWishartPrior + numberOfContrasts + 1
                    #
                    # which has pseudoVarianceOfWishartPrior as the MAP solution in the absence of any data
                    #
                    minLogUnnormalizedWishart = \
                        np.trace(np.linalg.solve(variance, np.array(pseudoVarianceOfWishartPrior).reshape(1,
                                                                                                          1))) * numberOfPseudoMeasurementsOfWishartPrior / 2 + \
                        numberOfPseudoMeasurementsOfWishartPrior / 2 * np.log(np.linalg.det(variance))
                    intensityModelParameterCost = intensityModelParameterCost + minLogUnnormalizedWishart
                historyOfEMCost.append(minLogLikelihood + intensityModelParameterCost)
                priorEMCost = historyOfEMCost[-2]
                currentEMCost = historyOfEMCost[-1]
                costChangeEM = priorEMCost - currentEMCost
                changeCostEMPerVoxel = costChangeEM / activeVoxelCount
                changeCostEMPerVoxelThreshold = optimizationOptions.absoluteCostPerVoxelDecreaseStopCriterion
                if changeCostEMPerVoxel < changeCostEMPerVoxelThreshold:
                    # Converged
                    print('EM converged!')
                    if checkpoint_manager:
                        checkpoint_manager.increment_and_save({
                            'activeVoxelCount': activeVoxelCount,
                            'priorEMCost': priorEMCost,
                            'currentEMCost': currentEMCost,
                            'costChangeEM': costChangeEM,
                            'changeCostEMPerVoxel': changeCostEMPerVoxel,
                            'changeCostEMPerVoxelThreshold': changeCostEMPerVoxelThreshold,
                            'minLogLikelihood': minLogLikelihood,
                            'intensityModelParameterCost': intensityModelParameterCost,
                        }, 'optimizerEmExit')
                    break
                #
                # M-step: update the model parameters based on the current posterior
                #
                # First the mixture model parameters
                for gaussianNumber in range(numberOfGaussians):
                    posterior = posteriors[:, gaussianNumber]
                    posterior = posterior.reshape(-1, 1)
                    mean = biasCorrectedData.T @ posterior / np.sum(posterior)
                    tmp = biasCorrectedData - mean.T
                    variance = (tmp.T @ (tmp * posterior) + \
                                pseudoVarianceOfWishartPrior * numberOfPseudoMeasurementsOfWishartPrior) \
                               / (np.sum(posterior) + numberOfPseudoMeasurementsOfWishartPrior)
                    if modelSpecifications.useDiagonalCovarianceMatrices:
                        # Force diagonal covariance matrices
                        variance = np.diag(np.diag(variance));
                    variances[gaussianNumber, :, :] = variance
                    means[gaussianNumber, :] = mean.T
                mixtureWeights = np.sum(posteriors + eps, axis=0).T
                for classNumber in range(numberOfClasses):
                    # mixture weights are normalized (those belonging to one mixture sum to one)
                    numberOfComponents = numberOfGaussiansPerClass[classNumber]
                    gaussianNumbers = np.array(
                        np.sum(numberOfGaussiansPerClass[:classNumber]) + np.array(range(numberOfComponents)),
                        dtype=np.uint32)
                    mixtureWeights[gaussianNumbers] = mixtureWeights[gaussianNumbers] / np.sum(
                        mixtureWeights[gaussianNumbers])
                # Now update the parameters of the bias field model.
                if (estimateBiasField and (iterationNumber > 0)):
                    #
                    # Bias field correction: implements Eq. 8 in the paper
                    #
                    #    Van Leemput, "Automated Model-based Bias Field Correction of MR Images of the Brain", IEEE TMI 1999
                    #
                    precisions = np.zeros_like(variances)
                    for classNumber in range(numberOfGaussians):
                        precisions[classNumber, :, :] = np.linalg.inv(variances[classNumber, :, :]).reshape(
                            (1, numberOfContrasts, numberOfContrasts))
                    lhs = np.zeros((np.prod(numberOfBasisFunctions) * numberOfContrasts, np.prod(
                        numberOfBasisFunctions) * numberOfContrasts))  # left-hand side of linear system
                    rhs = np.zeros(
                        (np.prod(numberOfBasisFunctions) * numberOfContrasts, 1))  # right-hand side of linear system
                    weightsImageBuffer = np.zeros(downSampledImageSize)
                    tmpImageBuffer = np.zeros(downSampledImageSize)
                    numberOfBasisFunctions_prod = np.prod(numberOfBasisFunctions)
                    for contrastNumber1 in range(numberOfContrasts):
                        logger.debug('third time contrastNumber=%d', contrastNumber)
                        tmp = np.zeros((data.shape[0], 1), order='F')
                        for contrastNumber2 in range(numberOfContrasts):
                            classSpecificWeights = posteriors * precisions[:, contrastNumber1, contrastNumber2].T
                            weights = np.sum(classSpecificWeights, 1);
                            # Build up stuff needed for rhs
                            predicted = np.sum(classSpecificWeights * np.expand_dims(means[:, contrastNumber2], 2).T / (
                                    np.expand_dims(weights, 1) + eps), 1)
                            residue = data[:, contrastNumber2] - predicted
                            tmp = tmp + weights.reshape(-1, 1) * residue.reshape(-1, 1)
                            # Fill in submatrix of lhs
                            weightsImageBuffer[downSampledMaskIndices] = weights
                            computedPrecisionOfKroneckerProductBasisFunctions = computePrecisionOfKroneckerProductBasisFunctions(
                                downSampledKroneckerProductBasisFunctions, weightsImageBuffer)
                            if checkpoint_manager:
                                checkpoint_manager.increment('bias_field_inner_loop')
                            lhs[
                            contrastNumber1 * numberOfBasisFunctions_prod: contrastNumber1 * numberOfBasisFunctions_prod + numberOfBasisFunctions_prod,
                            contrastNumber2 * numberOfBasisFunctions_prod:contrastNumber2 * numberOfBasisFunctions_prod + numberOfBasisFunctions_prod] = computedPrecisionOfKroneckerProductBasisFunctions
                        tmpImageBuffer[downSampledMaskIndices] = tmp.squeeze()
                        rhs[
                        contrastNumber1 * numberOfBasisFunctions_prod: contrastNumber1 * numberOfBasisFunctions_prod + numberOfBasisFunctions_prod] \
                            = projectKroneckerProductBasisFunctions(downSampledKroneckerProductBasisFunctions,
                                                                    tmpImageBuffer).reshape(-1, 1)
                    biasFieldCoefficients = np.linalg.solve(lhs, rhs).reshape(
                        (np.prod(numberOfBasisFunctions), numberOfContrasts))
                    for contrastNumber in range(numberOfContrasts):
                        downSampledBiasField = backprojectKroneckerProductBasisFunctions(
                            downSampledKroneckerProductBasisFunctions, biasFieldCoefficients[:, contrastNumber])
                        tmp = downSampledImageBuffers[:, :, :, contrastNumber] - downSampledBiasField * downSampledMask
                        downSampledBiasCorrectedImageBuffers[:, :, :, contrastNumber] = tmp
                        biasCorrectedData[:, contrastNumber] = tmp[downSampledMaskIndices]
                    if checkpoint_manager and checkpoint_manager.detailed:
                        checkpoint_manager.increment_and_save(
                            {
                                'biasFieldCoefficients': biasFieldCoefficients,
                                'lhs': lhs,
                                'rhs': rhs,
                                'downSampledBiasField': downSampledBiasField,
                                'downSampledBiasCorrectedImageBuffers': downSampledBiasCorrectedImageBuffers,
                                'biasCorrectedData': biasCorrectedData,
                                'computedPrecisionOfKroneckerProductBasisFunctions': computedPrecisionOfKroneckerProductBasisFunctions,
                            }, 'estimateBiasField')
                    pass
            historyOfEMCost = historyOfEMCost[1:]
            #
            # Part II: update the position of the mesh nodes for the current mixture model and bias field parameter estimates
            #
            downSampledBiasCorrectedImages = []
            for contrastNumber in range(numberOfContrasts):
                downSampledBiasCorrectedImages.append(GEMS2Python.KvlImage(
                    require_np_array(downSampledBiasCorrectedImageBuffers[:, :, :, contrastNumber])))

            # Set up cost calculator
            calculator = GEMS2Python.KvlCostAndGradientCalculator(
                typeName='AtlasMeshToIntensityImage',
                images=downSampledBiasCorrectedImages,
                boundaryCondition='Sliding',
                transform=transform,
                means=means,
                variances=variances,
                mixtureWeights=mixtureWeights,
                numberOfGaussiansPerClass=numberOfGaussiansPerClass)

            optimizerType = 'L-BFGS';
            optimization_parameters = {
                'Verbose': optimizationOptions.verbose,
                'MaximalDeformationStopCriterion': optimizationOptions.maximalDeformationStopCriterion,
                'LineSearchMaximalDeformationIntervalStopCriterion': optimizationOptions.lineSearchMaximalDeformationIntervalStopCriterion,
                'MaximumNumberOfIterations': optimizationOptions.maximumNumberOfDeformationIterations,
                'BFGS-MaximumMemoryLength': optimizationOptions.BFGSMaximumMemoryLength
            }
            optimizer = GEMS2Python.KvlOptimizer(optimizerType, mesh, calculator, optimization_parameters)
            historyOfDeformationCost = [];
            historyOfMaximalDeformation = [];
            nodePositionsBeforeDeformation = mesh.points
            while True:
                minLogLikelihoodTimesDeformationPrior, maximalDeformation = optimizer.step_optimizer_samseg()
                print("maximalDeformation={} minLogLikelihood={}".format(maximalDeformation,
                                                                         minLogLikelihoodTimesDeformationPrior))
                if maximalDeformation == 0:
                    break
                historyOfDeformationCost.append(minLogLikelihoodTimesDeformationPrior)
                historyOfMaximalDeformation.append(maximalDeformation)
            nodePositionsAfterDeformation = mesh.points
            maximalDeformationApplied = np.sqrt(
                np.max(np.sum((nodePositionsAfterDeformation - nodePositionsBeforeDeformation) ** 2, 1)))
            print('==============================')
            print(['iterationNumber: ', iterationNumber])
            print(['    maximalDeformationApplied: ', maximalDeformationApplied])
            print('==============================')
            if checkpoint_manager:
                checkpoint_manager.increment_and_save(
                    {
                        'maximalDeformationApplied': maximalDeformationApplied,
                        'nodePositionsAfterDeformation': nodePositionsAfterDeformation,
                    }, 'optimizer')
            # Keep track of the cost function we're optimizing
            historyOfCost.append(minLogLikelihoodTimesDeformationPrior + intensityModelParameterCost)
            priorCost = historyOfCost[-2]
            currentCost = historyOfCost[-1]
            costChange = priorCost - currentCost
            activeVoxelCount = len(downSampledMaskIndices[0])
            perVoxelDecrease = costChange / activeVoxelCount
            perVoxelDecreaseThreshold = optimizationOptions.absoluteCostPerVoxelDecreaseStopCriterion
            if perVoxelDecrease < perVoxelDecreaseThreshold:
                if checkpoint_manager:
                    checkpoint_manager.increment_and_save({
                        'activeVoxelCount': activeVoxelCount,
                        'priorCost': priorCost,
                        'currentCost': currentCost,
                        'costChange': costChange,
                        'perVoxelDecrease': perVoxelDecrease,
                        'perVoxelDecreaseThreshold': perVoxelDecreaseThreshold,
                        'minLogLikelihoodTimesDeformationPrior': minLogLikelihoodTimesDeformationPrior,
                        'intensityModelParameterCost': intensityModelParameterCost,
                    }, 'optimizerPerVoxelExit')
                break
        # Get the final node positions
        finalNodePositions = mesh.points
        # Transform back in template space (i.e., undoing the affine registration
        # that we applied), and save for later usage

        tmp = np.linalg.solve(totalTransformationMatrix,
                              np.pad(finalNodePositions, ((0, 0), (0, 1)), 'constant', constant_values=1).T).T
        finalNodePositionsInTemplateSpace = tmp[:, 0: 3]
        ## Record deformation delta here in lieu of maintaining history
        nodeDeformationInTemplateSpaceAtPreviousMultiResolutionLevel = \
            finalNodePositionsInTemplateSpace - initialNodePositionsInTemplateSpace


    return {
        'biasFieldCoefficients': biasFieldCoefficients,
        'imageBuffers': imageBuffers,
        'means': means,
        'mixtureWeights': mixtureWeights,
        'nodeDeformationInTemplateSpaceAtPreviousMultiResolutionLevel':
            nodeDeformationInTemplateSpaceAtPreviousMultiResolutionLevel,
        'transformMatrix': transform.as_numpy_array,
        'variances': variances,
    }


def test_samseg_ported_part2(case_name, case_file_folder, savePath):
    checkpoint_manager = create_checkpoint_manager(case_file_folder)
    fixture = load_starting_fixture()
    part1_results_dict, part1_results_dict_python, part1_results_dict_matlab = checkpoint_manager.substitute('part1', 1)
    part1_results_dict['savePath'] = savePath
    part2_results_dict = samsegment_part2(
        fixture['modelSpecifications'],
        fixture['optimizationOptions'],
        part1_results_dict,
        checkpoint_manager
    )
    checkpoint_manager.save(part2_results_dict, 'part2', 1)
    create_part2_inspection_team().inspect_all(checkpoint_manager)
    pass


if __name__ == '__main__':
    run_test_cases(action=test_samseg_ported_part2)
