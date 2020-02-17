/*
  This program wraps utilities developed within the ANTs toolbox.

        https://github.com/ANTsX/ANTs

  The itkN4BiasFieldCorrectionImageFilter class implements an nonuniform
  normalization algorithm described in the following paper.
  ITK-based spatially-adaptive filter described in the following paper.

        N. Tustison et al., N4ITK:  Improved N3 Bias Correction,
        IEEE Transactions on Medical Imaging, 29(6):1310-1320, June 2010.

  For ANTs license information, see distribution/docs/license.ants.txt
*/

#include "itkBSplineControlPointImageFilter.h"
#include "itkExpImageFilter.h"
#include "itkExtractImageFilter.h"
#include "itkImageRegionIterator.h"
#include "itkN4BiasFieldCorrectionImageFilter.h"
#include "itkShrinkImageFilter.h"

#include "argparse.h"
#include "mri.h"
#include "AntsN4BiasFieldCorrectionFs.help.xml.h"


int main(int argc, char **argv)
{
  // parse args
  ArgumentParser parser;
  parser.addHelp(AntsN4BiasFieldCorrectionFs_help_xml, AntsN4BiasFieldCorrectionFs_help_xml_len);
  parser.addArgument("-i", "--input",  1, String, true);
  parser.addArgument("-o", "--output", 1, String, true);
  parser.addArgument("-s", "--shrink", 1, Int, false);
  parser.parse(argc, argv);

  std::string inputname = parser.retrieve<std::string>("input");
  std::string outputname = parser.retrieve<std::string>("output");

  MRI* mri = MRIread(inputname.c_str());

  if (mri->nframes != 1) fs::fatal() << "input cannot be 4D (has " << mri->nframes << " frames)";

  // convert to ITK image
  ITKImageType::Pointer inputImage = mri->toITKImage();

  // set itk threads to 1
  itk::MultiThreader::SetGlobalDefaultNumberOfThreads(1);

  // create a "mask" that just covers the whole image
  ITKImageType::Pointer maskImage = ITKImageType::New();
  maskImage->CopyInformation(inputImage);
  maskImage->SetRegions(inputImage->GetRequestedRegion());
  maskImage->Allocate(false);
  maskImage->FillBuffer(itk::NumericTraits<ITKImageType::PixelType>::OneValue());

  // init the bias field correcter
  typedef itk::N4BiasFieldCorrectionImageFilter<ITKImageType, ITKImageType, ITKImageType> CorrecterType;
  CorrecterType::Pointer correcter = CorrecterType::New();

  // convergence options
  CorrecterType::VariableSizeArrayType maximumNumberOfIterations(4);
  maximumNumberOfIterations.Fill(50);
  correcter->SetMaximumNumberOfIterations(maximumNumberOfIterations);
  correcter->SetNumberOfFittingLevels(4);
  correcter->SetConvergenceThreshold(0.0);

  // shrink the image to save time
  int shrinkFactor = parser.exists("shrink") ? parser.retrieve<int>("shrink") : 4;
  std::cout << "Using shrink factor: " <<  shrinkFactor << std::endl;

  typedef itk::ShrinkImageFilter<ITKImageType, ITKImageType> ShrinkerType;
  ShrinkerType::Pointer shrinker = ShrinkerType::New();
  shrinker->SetInput(inputImage);
  shrinker->SetShrinkFactors(shrinkFactor);

  // shrink the mask
  ShrinkerType::Pointer maskshrinker = ShrinkerType::New();
  maskshrinker->SetInput(maskImage);
  maskshrinker->SetShrinkFactors(shrinkFactor);

  // run the correction
  correcter->SetInput(shrinker->GetOutput());
  correcter->SetMaskImage(maskshrinker->GetOutput());
  correcter->Update();

  // reconstruct the bias field at full image resolution
  typedef itk::BSplineControlPointImageFilter<CorrecterType::BiasFieldControlPointLatticeType, CorrecterType::ScalarImageType> BSplinerType;
  BSplinerType::Pointer bspliner = BSplinerType::New();
  bspliner->SetInput(correcter->GetLogBiasFieldControlPointLattice());
  bspliner->SetSplineOrder(correcter->GetSplineOrder());
  bspliner->SetSize(inputImage->GetLargestPossibleRegion().GetSize());
  bspliner->SetOrigin(inputImage->GetOrigin());
  bspliner->SetDirection(inputImage->GetDirection());
  bspliner->SetSpacing(inputImage->GetSpacing());
  bspliner->Update();

  // extract log field from bspline
  ITKImageType::Pointer logField = ITKImageType::New();
  logField->SetLargestPossibleRegion(inputImage->GetLargestPossibleRegion());
  logField->SetBufferedRegion(inputImage->GetBufferedRegion());
  logField->SetRequestedRegion(inputImage->GetRequestedRegion());
  logField->SetSpacing(inputImage->GetSpacing());
  logField->SetOrigin(inputImage->GetOrigin());
  logField->SetDirection(inputImage->GetDirection());
  logField->Allocate();

  itk::ImageRegionIterator<CorrecterType::ScalarImageType> ItB(bspliner->GetOutput(), bspliner->GetOutput()->GetLargestPossibleRegion());
  itk::ImageRegionIterator<ITKImageType> ItF(logField, logField->GetLargestPossibleRegion());
  for(ItB.GoToBegin(), ItF.GoToBegin(); !ItB.IsAtEnd(); ++ItB, ++ItF) ItF.Set(ItB.Get()[0]);

  // exp bias field
  typedef itk::ExpImageFilter<ITKImageType, ITKImageType> ExpFilterType;
  ExpFilterType::Pointer expFilter = ExpFilterType::New();
  expFilter->SetInput(logField);
  expFilter->Update();

  // divide original image by the bias field to get corrected image
  typedef itk::DivideImageFilter<ITKImageType, ITKImageType, ITKImageType> DividerType;
  DividerType::Pointer divider = DividerType::New();
  divider->SetInput1(inputImage);
  divider->SetInput2(expFilter->GetOutput());
  divider->Update();

  // crop divider to the input size
  ITKImageType::RegionType inputRegion;
  inputRegion.SetIndex(inputImage->GetLargestPossibleRegion().GetIndex());
  inputRegion.SetSize(inputImage->GetLargestPossibleRegion().GetSize());

  typedef itk::ExtractImageFilter<ITKImageType, ITKImageType> CropperType;
  CropperType::Pointer cropper = CropperType::New();
  cropper->SetInput(divider->GetOutput());
  cropper->SetExtractionRegion(inputRegion);
  cropper->SetDirectionCollapseToSubmatrix();
  cropper->Update();

  // load ITK image back into MRI and write to disk
  mri->loadITKImage(cropper->GetOutput());
  MRIwrite(mri, outputname.c_str());
  MRIfree(&mri);

  return 0;
}

