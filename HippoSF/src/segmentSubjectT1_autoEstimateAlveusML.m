% Segments the subfields from the original MPRAGE

% function put together by Eugenio to segment the subfields from the MPRAGE. It is based on
% segmentSubjectT1, which is in turn based on (pre-)processHippoSubfields.m
% It uses all labels in ASEG to initialize mesh deformation
%s
% segmentSubjectT1(subjectName,subjectDir,resolution,atlasMeshFileName,atlasDumpFileName,compressionLUTfileName,K,side,optimizerType,suffix,MRFconstant)
%d
% - subjectName: FreeSurfer subject name
% - subjectDir: FreeSurfer subject directory
% - resolution: voxel size at which we want to work (in mm).
% - atlasMeshFileName: the atlas to segment the data
% - atlasDumpFileName: corresponding imageDump.mgz (name *must* be imageDump.mgz)
% - compressionLUTfileName: corresponding compressionLUT.txt
% - K: stiffness of the mesh in the segmentation.
% - side: 'left' or 'right'
% - optimizerType: 'FixedStepGradientDescent','GradientDescent','ConjugateGradient','L-BFGS'
% - suffix: for output directory, e.g. 'T1based_GGAWLnoSimil'
% - FSpath: path to FreeSurfer executables
% - MRFconstant (optional): make it >0 for MRF cleanup (5 is reasonable, larger is smoother)
%           It does NOT affect volumes, which are computed from soft posteriors anyway


function segmentSubjectT1_autoEstimateAlveusML(subjectName,subjectDir,resolution,atlasMeshFileName,atlasDumpFileName,compressionLUTfileName,K,side,optimizerType,suffix,FSpath,MRFconstant)

% clear
% subjectName='eugenio';
% % subjectName='0003';
% subjectDir='/autofs/space/panamint_005/users/iglesias/brains/myBrain/myBrain/subjectdir/';
% % subjectDir='/autofs/space/panamint_005/users/iglesias/data/ADNI_SF_130607/';
% resolution=(1/3);
% atlasMeshFileName='/autofs/space/panamint_005/users/iglesias/atlases/atlasHippoAmygBuckner_170621_CJ_GD_allBuckner_BodyHead/AtlasMesh_merged.gz';
% atlasDumpFileName='/autofs/space/panamint_005/users/iglesias/atlases/atlasHippoAmygBuckner_170621_CJ_GD_allBuckner_BodyHead/imageDumpWithAmygdala.mgz';
% compressionLUTfileName='/autofs/homes/002/iglesias/matlab/code/Atlas3dFreeSurferJuly2017newAtlas/code/compressionLookupTable.txt';
% K=0.05;
% side='right';
% optimizerType='L-BFGS';
% % optimizerType='ConjugateGradient';
% suffix='GEMS2-dev-mac';
% FSpath='/usr/local/freesurfer/dev/bin/';
% MRFconstant=0;
% nargin=9;

% Eugenio November 2017: added option to write meshes and smoother resampling
DEBUG=0;
FAST=0; % set it to one to optimize just a bit (go through code fast)
WRITE_POSTERIORS=0;
WRITE_MESHES=0;
SMOOTH_LABEL_RESAMPLE=0;
aux=getenv('WRITE_POSTERIORS');
if ~isempty(aux)
    if str2double(aux)>0
        WRITE_POSTERIORS=1;
    end
end
aux=getenv('WRITE_MESHES');
if ~isempty(aux)
    if str2double(aux)>0
        WRITE_MESHES=1;
    end
end


% sanity check
if exist('MRFconstant','var')==0
    MRFconstant=0;
end

if nargin<9
    error('Not enough input arguments');
elseif strcmp(side,'left')==0 && strcmp(side,'right')==0
    error('Side must be ''left'' or ''right''');
elseif optimizerType(1)~='F' && optimizerType(1)~='G' && optimizerType(1)~='C' && optimizerType(1)~='L'
    error('Optimizer type must be ''FixedStepGradientDescent'',''GradientDescent'',''ConjugateGradient'',''L-BFGS''');
elseif exist([subjectDir '/' subjectName],'dir')==0
    error('Subject directory does not exist');
elseif ~isdeployed && (~isnumeric(resolution))
    error('Resolution must be numeric');
elseif exist(atlasMeshFileName,'file')==0
    error('Provided atlas mesh file does not exist');
elseif exist(atlasDumpFileName,'file')==0
    error('Provided imageDump.mgz does not exist');
elseif exist(compressionLUTfileName,'file')==0
    error('Provided LUT does not exist');
elseif ~isdeployed && (~isnumeric(K))
    error('K must be numeric');
elseif ~isdeployed && (~isnumeric(MRFconstant))
    error('MRFconstant must be numeric');
end


% Constants
HippoLabelLeft=17;
HippoLabelRight=53;

% In case we compiled it...
if isdeployed
    K=str2double(K);
    resolution=str2double(resolution);
    MRFconstant=str2double(MRFconstant);
else
    addpath([pwd() '/functions']);
    addpath('/usr/local/freesurfer/stable6_0_0/matlab')
    if ismac
        addpath('/autofs/space/panamint_005/users/iglesias/software/freesurfer.GEMS2.MAC/bin')
    elseif isunix
        addpath('/autofs/space/panamint_005/users/iglesias/software/freesurfer.GEMS2/bin')
    else
        error('Neither Linux nor Mac');
    end
end
time_start=clock;

% Clean up KVL memory space
kvlClear;


% Temporary directory: here we have a secret flag: if we are at the Martinos
% Center and we are using the cluster, we want to set USE_SCRATCH to 1 in order
% to avoid massive data flow between the cluster and your machine (assming your
% data is local).
tempdir=[subjectDir '/' subjectName '/tmp/hippoSF_T1_' suffix '_' side '/'];
aux=getenv('USE_SCRATCH');
if ~isempty(aux)
    if str2double(aux)>0
        if exist('/scratch/','dir')>0
            tempdir=['/scratch/' subjectName '_hippoSF_T1_' suffix '_' side '/'];
        end
    end
end
if exist(tempdir,'dir')==0
    mkdir(tempdir);
end

cd(tempdir);

% Next: register image dump to automated segmentation
disp('Registering imageDump.mgz to hippocampal mask from ASEG')


% Grab atlas (manually placed in FS atlas coordinate space!)
system(['cp ' atlasDumpFileName ' ./imageDump.mgz']);

% flip LR if right side - we only rotate along LR axis not to bias left vs right hippo segmentation
if strcmp(side,'right')>0
    aux=myMRIread('imageDump.mgz',0,tempdir);
    aux.vox2ras0(1,:)=-aux.vox2ras0(1,:);
    aux.vox2ras1(1,:)=-aux.vox2ras1(1,:);
    aux.vox2ras(1,:)=-aux.vox2ras(1,:);
    myMRIwrite(aux,'imageDump.mgz','float',tempdir);
end


% Target is masked aseg 
targetRegFileName=[tempdir '/hippoAmygBinaryMask.mgz'];
targetRegFileNameCropped=[tempdir '/hippoAmygBinaryMask_autoCropped.mgz'];
ASEG=myMRIread([subjectDir '/' subjectName '/mri/aseg.mgz'],0,tempdir);
TARGETREG=ASEG;
if strcmp(side,'left')>0
    TARGETREG.vol=255*double(ASEG.vol==HippoLabelLeft | ASEG.vol==HippoLabelLeft+1);
else
    TARGETREG.vol=255*double(ASEG.vol==HippoLabelRight | ASEG.vol==HippoLabelRight+1);
end
myMRIwrite(TARGETREG,targetRegFileName,'float',tempdir);

highres=0; if mean(ASEG.volres)<0.99, highres=1; end
if highres==1,
    system([FSpath '/mri_convert ' targetRegFileName ' aux.mgz -odt float -vs 1 1 1 -rt nearest >/dev/null']);
    system(['mv aux.mgz ' targetRegFileName ' >/dev/null']);
end


% Replacement of kvlAutoCrop
aux=myMRIread(targetRegFileName,0,tempdir);
[aux.vol,cropping]=cropLabelVol(aux.vol,6);
shift=aux.vox2ras0(1:3,1:3)*[cropping(2)-1; cropping(1)-1; cropping(3)-1];
aux.vox2ras0(1:3,4)=aux.vox2ras0(1:3,4)+shift;
aux.vox2ras1(1:3,4)=aux.vox2ras1(1:3,4)+shift;
aux.vox2ras(1:3,4)=aux.vox2ras(1:3,4)+shift;
aux.tkrvox2ras=[];
myMRIwrite(aux,targetRegFileNameCropped,'float',tempdir);



if 1==1  % This is to use an opened version
    aux=myMRIread(targetRegFileNameCropped,0,tempdir);
    strel=createSphericalStrel(1);
    aux.vol=255*double(imdilate(imerode(aux.vol>0,strel),strel));
    myMRIwrite(aux,targetRegFileNameCropped,'float',tempdir);
end

cmd=[FSpath '/mri_robust_register --mov imageDump.mgz  --dst ' targetRegFileNameCropped ...
    ' -lta trash.lta --mapmovhdr imageDump_coregistered.mgz  --sat 50'];
system(cmd);
% system([cmd ' >/dev/null']);
system('mv imageDump_coregistered.mgz imageDump.mgz' );

cmd=[FSpath '/mri_robust_register --mov imageDump.mgz  --dst ' targetRegFileNameCropped ...
    ' -lta trash.lta --mapmovhdr imageDump_coregistered.mgz --affine --sat 50'];
system(cmd);
% system([cmd ' >/dev/null']);
system('mv imageDump_coregistered.mgz imageDump.mgz' );


% Now, the idea is to refine the affine transform based on the hippo
% First, we prepare a modifided ASEG that we'll segment

% There's a bunch of labels in the ASEG don't have in our atlas...
ASEGbackup=ASEG;
ASEG.vol(ASEG.vol==15)=0;  % 4th vent -> background (we're killing brainstem anyway...)
ASEG.vol(ASEG.vol==16)=0;  % get rid of brainstem
ASEG.vol(ASEG.vol==7)=0;   % get rid of left cerebellum WM ...
ASEG.vol(ASEG.vol==8)=0;   % ... and of left cerebellum CT
ASEG.vol(ASEG.vol==46)=0;  % get rid of right cerebellum WM ...
ASEG.vol(ASEG.vol==47)=0;  % ... and of right cerebellum CT
ASEG.vol(ASEG.vol==80)=0;  % non-WM hippo -> background
ASEG.vol(ASEG.vol==85)=0;  % optic chiasm -> background
ASEG.vol(ASEG.vol==72)=4;  % 5th ventricle -> left-lat-vent

if strcmp(side,'left')>0
    ASEG.vol(ASEG.vol==5)=4;   % left-inf-lat-vent -> left-lat-vent
    ASEG.vol(ASEG.vol==30)=2;  % left-vessel -> left  WM
    ASEG.vol(ASEG.vol==14)=4;  % 3rd vent -> left-lat-vent
    ASEG.vol(ASEG.vol==24)=4;  % CSF -> left-lat-vent
    ASEG.vol(ASEG.vol==77)=2;  % WM hippoint -> left WM
    ASEG.vol(ASEG.vol>250)=2;  % CC labels -> left WM
    list2kill=[44 62 63 41 42 43 49 50 51 52 53 54 58 60];
    for k=1:length(list2kill)
        ASEG.vol(ASEG.vol==list2kill(k))=0;
    end
else
    BU=ASEG.vol;
    ASEG.vol(:)=0;
    ASEG.vol(BU==44)=4; % right-inf-lat-vent -> left-lat-vent
    ASEG.vol(BU==62)=2; % right-vessel -> left  WM
    ASEG.vol(BU==14)=4;  % 3rd vent -> left-lat-vent
    ASEG.vol(BU==24)=4;  % CSF -> left-lat-vent
    ASEG.vol(BU==77)=2;  % WM hippoint -> left WM
    ASEG.vol(BU>250)=2;  % CC labels -> left WM
    % left to right
    ASEG.vol(BU==41)=2; % WM
    ASEG.vol(BU==42)=3; % CT
    ASEG.vol(BU==43)=4; % LV
    ASEG.vol(BU==49)=10; % TH
    ASEG.vol(BU==50)=11; % CA
    ASEG.vol(BU==51)=12; % PU
    ASEG.vol(BU==52)=13; % PA
    ASEG.vol(BU==53)=17; % HP
    ASEG.vol(BU==54)=18; % AM
    ASEG.vol(BU==58)=26; % AA
    ASEG.vol(BU==60)=28; % DC
    ASEG.vol(BU==63)=31; % CP
end
ASEG.vol(ASEG.vol==0)=1;



% Write to disk
myMRIwrite(ASEG,'asegMod.mgz','float',tempdir);

% Eugenio July 2017
% We now merge hippo, amygdala and cortex in cheating image
ASEGcha=ASEG;
ASEGcha.vol(ASEGcha.vol==17)=3; 
ASEGcha.vol(ASEGcha.vol==18)=3; 
myMRIwrite(ASEGcha,'asegModCHA.mgz','float',tempdir);


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Now we pretty much copy-paste from preprocessHippoSubfields %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

asegFileName = [subjectDir '/' subjectName '/mri/aseg.mgz'];  % FreeSurfer's volumetric segmentation results. This are non-probabilistic, "crisp" definitions
boundingFileName = [tempdir 'imageDump.mgz']; % Bounding box
meshCollectionFileName = atlasMeshFileName; % The tetrahedral atlas mesh
compressionLookupTableFileName =compressionLUTfileName; % Look-up table belonging to the atlas

%
[ FreeSurferLabels, names, colors ] = kvlReadCompressionLookupTable( compressionLookupTableFileName );

% Read in aseg and also transform
% Eugenio July 2017
% [ synIm, transform ] = kvlReadCroppedImage( 'asegMod.mgz', boundingFileName );
[ synIm, transform ] = kvlReadCroppedImage( 'asegModCHA.mgz', boundingFileName );
synImBuffer = kvlGetImageBuffer( synIm );
synSize = size( synImBuffer );
if ~isdeployed && DEBUG>0
    figure
    showImage( synIm )
    title('Synthetic Image')
end


% read in collection, set K and apply transform
meshCollection = kvlReadMeshCollection( meshCollectionFileName );
kvlTransformMeshCollection( meshCollection, transform );
kvlSetKOfMeshCollection( meshCollection, K );

% Retrieve the reference mesh, i.e., the mesh representing the average shape.
mesh = kvlGetMesh( meshCollection, -1 );
originalNodePositions = kvlGetMeshNodePositions( mesh );
originalAlphas = kvlGetAlphasInMeshNodes( mesh );

% % Just for illustrative purposes, let's also display this mesh warped onto each
% % of the training subjects, as computed during the group-wise registration during
% % the atlas building
% if ~isdeployed && DEBUG>0
%     for meshNumber = 0 : 14  % C-style indexing
%         pause( .1 )
%         showImage( kvlColorCodeProbabilityImages( kvlRasterizeAtlasMesh( kvlGetMesh( meshCollection, meshNumber ), synSize ), colors ) )
%         title(['Reference mesh warped back onto subject ' num2str(1+meshNumber)]);
%     end
% end

% Eugenio July 2017: head / body, and also merged hippos/amygdala/cortex, and moved fissure with background
FreeSurferLabelGroups=[];
FreeSurferLabelGroups{end+1}={'Left-Cerebral-Cortex',... % cortex
    'Left-Hippocampus','alveus','subiculum-body','subiculum-head','Hippocampal_tail',... % hippo
    'molecular_layer_HP-body','molecular_layer_HP-head','GC-ML-DG-body','GC-ML-DG-head',...
    'CA4-body','CA4-head','CA1-body','CA1-head','CA3-body','CA3-head','HATA','fimbria',...
    'presubiculum-body','presubiculum-head','parasubiculum','Left-hippocampus-intensity-abnormality', ...
    'Left-Amygdala','Lateral-nucleus','Paralaminar-nucleus','Basal-nucleus',... % amygdala
    'Hippocampal-amygdala-transition-HATA','Accessory-Basal-nucleus','Amygdala-background',...
    'Corticoamygdaloid-transitio','Central-nucleus','Cortical-nucleus','Medial-nucleus',...
    'Anterior-amygdaloid-area-AAA'};
FreeSurferLabelGroups{end+1}={'Left-Cerebral-White-Matter'};
FreeSurferLabelGroups{end+1}={'Left-Lateral-Ventricle'};
FreeSurferLabelGroups{end+1}={'Left-choroid-plexus'};
FreeSurferLabelGroups{end+1}={'Background','hippocampal-fissure','Background-CSF','Background-vessels','Background-tissue','Unknown'};
FreeSurferLabelGroups{end+1}={'Left-VentralDC'};
FreeSurferLabelGroups{end+1}={'Left-Putamen'};
FreeSurferLabelGroups{end+1}={'Left-Pallidum'};
FreeSurferLabelGroups{end+1}={'Left-Thalamus-Proper'};
FreeSurferLabelGroups{end+1}={'Left-Accumbens-area'};
FreeSurferLabelGroups{end+1}={'Left-Caudate'};
FreeSurferLabelGroups{end+1}={'SUSPICIOUS'};

sameGaussianParameters=[];
for g=1:length(FreeSurferLabelGroups)
    sameGaussianParameters{end+1} = [];
    for FreeSurferLabel =  FreeSurferLabelGroups{g}
        sameGaussianParameters{end} = [ sameGaussianParameters{end} FreeSurferLabels( find( strcmp( FreeSurferLabel, cellstr( names ) ) ) ) ];
    end
    if isempty(sameGaussianParameters{end})
        sameGaussianParameters=sameGaussianParameters(1:end-1);
    end
end

% Eugenio July 2017
cheatingMeans=zeros(length( sameGaussianParameters),1);
cheatingVariances=0.01*ones(length( sameGaussianParameters),1);
for l=1:length(sameGaussianParameters)
    labels= sameGaussianParameters{l};
    if any(labels>=200 & labels<=226),  cheatingMeans(l)=3; %  cheatingMeans(l)=17; % HIPPO SF -> HIPPO
    elseif any(labels>=7000),  cheatingMeans(l)=3;  % cheatingMeans(l)=18; % AMYGDALOID SUBNUCLEI -> AMYGDALA
    elseif any(labels==0), cheatingMeans(l)=1; % BACKGROUND is 1 instead of 0
    elseif any(labels==999), cheatingMeans(l)=55; cheatingVariances(l)=55^2; % This is the generic, "suspicious" label we use for cysts...
    else cheatingMeans(l)=labels(1);
    end
end

% Compute the "reduced" alphas - those referring to the "super"-structures
[ reducedAlphas, reducingLookupTable ] = kvlReduceAlphas( originalAlphas, compressionLookupTableFileName, sameGaussianParameters );
if ( max( abs( sum( reducedAlphas, 2 ) - 1 ) ) > 1e-5 ) % Make sure these vectors really sum to 1
    error( 'The vector of prior probabilities in the mesh nodes must always sum to one over all classes' )
end

% Set the reduced alphas to be the alphas of the mesh
kvlSetAlphasInMeshNodes( mesh, reducedAlphas )


% Eugenio July 2017

% priors = kvlRasterizeAtlasMesh( mesh, synSize );
% MASK=imerode(sum(double(priors)/65535,4)>0.99,createSphericalStrel(3));

for l = 1 : size(reducedAlphas,2)    
    if l==1
       % This is a bit annoying, but the call to kvlRasterize with a single
       % label fills in the voxels outside the cuboid with l=1 (whereas the
       % call with multiple labels does not)
        sillyAlphas=zeros([size(reducedAlphas,1),2],'single');
        sillyAlphas(:,1)=reducedAlphas(:,1);
        sillyAlphas(:,2)=1-sillyAlphas(:,1);
        kvlSetAlphasInMeshNodes( mesh, sillyAlphas )
        prior = kvlRasterizeAtlasMesh( mesh, synSize);
        kvlSetAlphasInMeshNodes( mesh, reducedAlphas )
        sumpriors=prior(:,:,:,1);
    else
        prior = kvlRasterizeAtlasMesh( mesh, synSize, l-1 );
        sumpriors=sumpriors+prior;        
    end
end
MASK=imerode(sum(single( sumpriors / 65535 ),4)>0.99,createSphericalStrel(3));


cheatingImageBuffer=synImBuffer;
cheatingImageBuffer(~MASK)=0;
cheatingImage = kvlCreateImage( cheatingImageBuffer );

if ~isdeployed && DEBUG>0
    priors = kvlRasterizeAtlasMesh( mesh, synSize );
    figure
    for cheatingLabel = 1 : size( reducedAlphas, 2 )
        subplot( 3, 4, cheatingLabel )
        showImage( priors( :, :, :, cheatingLabel ) )
    end
    title('Priors for segmentation of fake intensity image')
end

if ~isdeployed && DEBUG>0
    figure
    showImage( cheatingImage, [], [ 0 110 ] )  % Using dynamic range for display that shows what's going on
    title('Fake intensity image to initialize atlas deformation')
end


% We use a multiscale approach here (Koen had a single one with sigma=3)
meshSmoothingSigmas = [ 3.0 2.0]';
% Eugenio November 2017: increased number of iterations by 50%
maxIt=[300,150];

numberOfMultiResolutionLevels = length( meshSmoothingSigmas );

time_ref_cheat_optimization=clock;

historyOfMinLogLikelihoodTimesPrior = [ 1/eps ];

for multiResolutionLevel = 1 : numberOfMultiResolutionLevels
    
    % Smooth the mesh using a Gaussian kernel.
    % It's good to smooth the mesh, otherwise we get weird compressions of the
    % mesh along the boundaries...
    kvlSetAlphasInMeshNodes( mesh, reducedAlphas )
    meshSmoothingSigma = meshSmoothingSigmas( multiResolutionLevel );
    fprintf( 'Smoothing mesh collection with kernel size %f ...', meshSmoothingSigma )
    kvlSmoothMeshCollection( meshCollection, meshSmoothingSigma )
    fprintf( 'done\n' )
    
    
    % Show the smoothed atlas
    if ~isdeployed && DEBUG>0
        figure
        priors = kvlRasterizeAtlasMesh( mesh, synSize );
        for cheatingLabel = 1 : size( reducedAlphas, 2 )
            subplot( 3, 4, cheatingLabel )
            showImage( priors( :, :, :, cheatingLabel ) )
        end
        title('Smoothed priors')
    end
    
    % Eugenio November 2017: GEMS2
    % Set up the black box optimizer for the mesh nodes
    if ( exist( 'cheatingOptimizer', 'var' ) == 1 )
        % The optimizer is very memory hungry when run in multithreaded mode.
        % Let's clear any old ones we may have lying around
        % Eugenio November 2017: GEMS2
        kvlClear( cheatingOptimizer );
        kvlClear( cheatingCalculator );
    end
    
    % Eugenio November 2017: GEMS2  (note that it uses variances instead of precisions)
    % Now the optimization per-se
    cheatingCalculator = kvlGetCostAndGradientCalculator('AtlasMeshToIntensityImage',...
    cheatingImage, 'Sliding',transform,cheatingMeans,cheatingVariances,ones(size(cheatingMeans)),ones(size(cheatingMeans)));
    
    verbose=0;
    maximalDeformationStopCriterion=1e-10;
    lineSearchMaximalDeformationIntervalStopCriterion=1e-10;
    maximumNumberOfDeformationIterations=1000;
    BFGSMaximumMemoryLength=12;
    
    % optimizer = kvlGetOptimizer( optimizerType, mesh, calculator);
    cheatingOptimizer = kvlGetOptimizer( optimizerType, mesh, cheatingCalculator, ...
                                    'Verbose', verbose, ...
                                    'MaximalDeformationStopCriterion', maximalDeformationStopCriterion, ... 
                                    'LineSearchMaximalDeformationIntervalStopCriterion', ...
                                     lineSearchMaximalDeformationIntervalStopCriterion, ...
                                    'MaximumNumberOfIterations', maximumNumberOfDeformationIterations, ... 
                                    'BFGS-MaximumMemoryLength', BFGSMaximumMemoryLength );
                                
    relativeChangeInCostStopCriterion = 1e-10;
    maxpuin=maxIt(multiResolutionLevel);
    if FAST>0
        maxpuin=20;
    end
    
    for positionUpdatingIterationNumber = 1 : maxpuin
        disp(['Resolution ' num2str(multiResolutionLevel) ', iteration ' num2str(positionUpdatingIterationNumber)]);
        % Calculate a good step. The first one is very slow because of various set-up issues
        % Eugenio May2018
        maximalDeformation=0;
        try
            tic
            % Eugenio November 2017: GEMS2
            [ minLogLikelihoodTimesPrior, maximalDeformation ] = kvlStepOptimizer( cheatingOptimizer );
            elapsedTime = toc;
            disp( [ 'Did one deformation step of max. ' num2str( maximalDeformation )  ' voxels in ' num2str( elapsedTime ) ' seconds' ] )
            minLogLikelihoodTimesPrior
        end
        if isnan(minLogLikelihoodTimesPrior)
            error('lhood is nan');
        end
        
        historyOfMinLogLikelihoodTimesPrior = [ historyOfMinLogLikelihoodTimesPrior; minLogLikelihoodTimesPrior ];
        
        % Test if we need to stop
        if ( ( maximalDeformation <= maximalDeformationStopCriterion ) | ...
                abs(( ( historyOfMinLogLikelihoodTimesPrior( end-1 ) - historyOfMinLogLikelihoodTimesPrior( end ) ) ...
                / historyOfMinLogLikelihoodTimesPrior( end ))) < relativeChangeInCostStopCriterion   )
            break;
        end
        
        % Show what we have
        if ~isdeployed && DEBUG>0
            subplot( 2, 2, 3 )
            showImage( kvlRasterizeAtlasMesh( mesh, synSize, 1 ) );
            title('Current atlas deformation')
            subplot( 2, 2, 4 )
            plot( historyOfMinLogLikelihoodTimesPrior( 2 : end ) )
            title('History of Log-lhood + Log-prior')
            drawnow
        end
    end
end
% Eugenio November 2017: GEMS2
kvlClear( cheatingOptimizer )
kvlClear( cheatingCalculator )

disp(['Fitting mesh to synthetic image from ASEG took ' num2str(etime(clock,time_ref_cheat_optimization)) ' seconds']);

if positionUpdatingIterationNumber==1
    error('Fitting mesh to synthetic image resulted in no deformation')
end



% OK, we're done. let's modify the mesh atlas in such a way that our computed mesh node positions are
% assigned to what was originally the mesh warp corresponding to the first training subject.
kvlSetAlphasInMeshNodes( mesh, originalAlphas )
updatedNodePositions = kvlGetMeshNodePositions( mesh );
kvlSetMeshCollectionPositions( meshCollection, ... %
    originalNodePositions, ... % reference position (average "shape")
    updatedNodePositions );


% Compare the average shape we started with, with the shape we have computed now in a little movie
if ~isdeployed && DEBUG>0
    figure
    originalPositionColorCodedPriors = ...
        kvlColorCodeProbabilityImages( kvlRasterizeAtlasMesh( kvlGetMesh( meshCollection, -1 ), synSize ), colors );
    updatedPositionColorCodedPriors = ...
        kvlColorCodeProbabilityImages( kvlRasterizeAtlasMesh( kvlGetMesh( meshCollection, 0 ), synSize ), colors );
    for i=1:20
        subplot(1,3,1)
        showImage( cheatingImage ), title('Cheating image')
        subplot(1,3,2)
        showImage( cheatingImage ), title('Cheating image')
        subplot(1,3,3)
        showImage( originalPositionColorCodedPriors ), title('Prior before deformation')
        pause(.5)
        subplot(1,3,1)
        showImage( originalPositionColorCodedPriors ), title('Prior before deformation')
        subplot(1,3,2)
        showImage( updatedPositionColorCodedPriors ), title('Prior after deformation')
        subplot(1,3,3)
        showImage( updatedPositionColorCodedPriors ), title('Prior after deformation')
        pause( .5 )
    end
end

% Write the resulting atlas mesh to file IN NATIVE ATLAS SPACE
% This is nice because all we need to do is to modify
% imageDump_coregistered with the T1-to-T2 transform to have the warped
% mesh in T2 space :-)
transformMatrix = double(kvlGetTransformMatrix( transform ));
inverseTransform = kvlCreateTransform( inv( transformMatrix ) );
kvlTransformMeshCollection( meshCollection, inverseTransform );
kvlWriteMeshCollection( meshCollection, 'warpedOriginalMesh.txt' );


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Now we pretty much copy-paste from processHippoSubfields %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% Clean up the Matlab work space
kvlClear % Clear all the wrapped C++ stuff
close all

% Provide the location of the image to be segmented, as well as the atlas that has been
% pre-registered affinely (i.e., 12 degrees of freedom) to the image.
meshCollectionFileName = 'warpedOriginalMesh.txt.gz'; % The tetrahedral atlas mesh


% Eugenio: we extract a block from norm.mgz and upsample it to the work
% resolution
% We also mask out non-brain voxels and also the cerebellum,
% brainstem and 3rd/4th ventricles, which can be annoying later on.i
%
imageFileName='T1resampled.mgz';
margin=15; % in mm
A=myMRIread([subjectDir '/' subjectName '/mri/norm.mgz'],0,tempdir);
NORM=A;
% L=myMRIread([subjectDir '/' subjectName '/mri/aseg.mgz'],0,tempdir);
L=ASEGbackup;
if strcmp(side,'left')
    [~,cropping]=cropLabelVol(L.vol==HippoLabelLeft,round(margin/mean(A.volres)));
else
    [~,cropping]=cropLabelVol(L.vol==HippoLabelRight,round(margin/mean(A.volres)));
end
Lcrop=applyCropping(L.vol,cropping);
Icrop=applyCropping(A.vol,cropping);
offsetVox=[cropping(2)-1;cropping(1)-1;cropping(3)-1;0];
RAScorner=L.vox2ras0*offsetVox;
vox2ras0New=[ L.vox2ras0(:,1:3)  L.vox2ras0(:,4)+RAScorner];
I=L;
I.vol=Icrop;
I.vox2ras0=vox2ras0New;
myMRIwrite(I,'tempT1.mgz','float',tempdir);
cmd=[FSpath '/mri_convert tempT1.mgz '  imageFileName ' -odt float -rt  cubic -vs ' num2str(resolution) ' '  num2str(resolution)  ' '  num2str(resolution)];
system([cmd ' >/dev/null']);
delete tempT1.mgz
system([FSpath '/mri_binarize --i asegMod.mgz --min 1.5 --dilate 2 --o asegModBinDilated.mgz >/dev/null']);
system([FSpath '/mri_convert asegModBinDilated.mgz asegModBinDilatedResampled.mgz -odt float -rt nearest -rl T1resampled.mgz >/dev/null']);
system([FSpath '/mri_mask -T 0.5 T1resampled.mgz asegModBinDilatedResampled.mgz T1resampled.mgz >/dev/null']);


% Eugenio: let's try masking anything that is not close to the hippo,
% brainstem and 3rd/4th ventricles, which can be annoying later on.
dilSize=round(5/mean(A.volres));
system([FSpath '/mri_binarize --i asegMod.mgz --min 16.5 --max 18.5 --o hippoMask.mgz >/dev/null']);
system([FSpath '/mri_binarize --i asegMod.mgz --min 16.5 --max 18.5 --o hippoMaskDilated5mm.mgz --dilate ' num2str(dilSize) ' >/dev/null']);
system([FSpath '/mri_convert hippoMask.mgz hippoMaskResampled.mgz -rt interpolate -rl T1resampled.mgz  -odt float >/dev/null']);
system([FSpath '/mri_binarize --i  hippoMaskResampled.mgz --min 0.5 --dilate ' num2str(round(3/resolution)) '  --o hippoMaskResampledDilated.mgz  >/dev/null']);
system([FSpath '/mri_mask -T 0.5 T1resampled.mgz  hippoMaskResampledDilated.mgz T1resampled.mgz >/dev/null']);



% Read the image data from disk. At the same time, construct a 3-D affine transformation (i.e.,
% translation, rotation, scaling, and skewing) as well - this transformation will later be used
% to initially transform the location of the atlas mesh's nodes into the coordinate system of
% the image.
[ image, transform ] = kvlReadCroppedImage( imageFileName, boundingFileName );
imageBuffer = kvlGetImageBuffer( image );
imageSize = size( imageBuffer );
if ~isdeployed && DEBUG>0
    figure
    showImage( imageBuffer ); % Automatically displays middle slices in each direction
    title('Image buffer to segment')
end

% Read the atlas mesh from file, and apply the previously determined transform to the location
% of its nodes.
% If you don't provide an explicit value for K, the value used to construct the atlas will be used (which is
% theoretically the only really valid one...)
meshCollection = kvlReadMeshCollection( meshCollectionFileName );
kvlTransformMeshCollection( meshCollection, transform );
kvlSetKOfMeshCollection( meshCollection, K );


% Retrieve the correct mesh to use from the meshCollection. Above, we pre-deformed
% the average shape hippocampal subfield atlas to match the whole hippocampus segmentation generated by FreeSurfer,
% and wrote it out to the first mesh in the meshCollection we're using now. So make sure to retrieve that one
% and not something else
mesh = kvlGetMesh( meshCollection, 0 );  % Use -1 if you don't want to use the preprocessed one, but really the
% one with average shape


% For reasons that escape me right now, I seem to have somehow decided that areas not covered by the
% mesh have probability 1 of belonging to the class with index 0 (don't ask)
if ~isdeployed && DEBUG>0
    figure
    for labelNumber = 0 : 14  % C-style numbering: 0 corresponds to first element...
        prior = kvlRasterizeAtlasMesh( mesh, imageSize, labelNumber );
        subplot( 4, 4, labelNumber+1 )
        showImage( prior )
        title(['Priors for ' names(labelNumber+1,:)]);
    end
end

% Rather than showing rasterized priors defined by the atlas mesh one by one, as we did above,
% we can color-code them and show everything as one image

% priors = kvlRasterizeAtlasMesh( mesh, imageSize ); % Without specifying a specific label, will rasterize all simultaneously
if ~isdeployed && DEBUG>0
    priors = kvlRasterizeAtlasMesh( mesh, imageSize ); % Without specifying a specific label, will rasterize all simultaneously
    colorCodedPriors = kvlColorCodeProbabilityImages( priors, colors );
    figure
    showImage( colorCodedPriors );
    title('Color-coded priors')
    clear colorCodedPriors
end

% Eugenio July 2017: let's avoid rasterizing all priors simultaneously
% (which was faster, but used much more memory)
alphas = kvlGetAlphasInMeshNodes( mesh );
nlabels = size(alphas,2);
for l = 1 : nlabels
    
    if l==1
       % This is a bit annoying, but the call to kvlRasterize with a single
       % label fills in the voxels outside the cuboid with p=1 (whereas the
       % call with multiple labels does not)
        sillyAlphas=zeros([size(originalAlphas,1),2],'single');
        sillyAlphas(:,1)=originalAlphas(:,1);
        sillyAlphas(:,2)=1-sillyAlphas(:,1);
        kvlSetAlphasInMeshNodes( mesh, sillyAlphas )
        prior = kvlRasterizeAtlasMesh( mesh, imageSize);
        kvlSetAlphasInMeshNodes( mesh, originalAlphas )
        sumpriors=prior(:,:,:,1);

    else
       
        prior = kvlRasterizeAtlasMesh( mesh, imageSize, l-1 );
        sumpriors=sumpriors+prior;
        
    end
end

% We're not interested in image areas that fall outside our cuboid ROI where our atlas is defined. Therefore,
% generate a mask of what's inside the ROI. Also, by convention we're skipping all voxels whose intensities
% is exactly zero (which we do to remove image areas far away for FreeSurfer's ASEG hippo segmentation) -
% also include that in the mask
% mask = imerode(single( sum( priors, 4 ) / 65535 ) > 0.99,createSphericalStrel(5));
mask = imerode(single( sumpriors / 65535 ) > 0.99,createSphericalStrel(5));
mask = mask & ( imageBuffer > 0 );
if ~isdeployed && DEBUG>0
    figure
    subplot( 1, 2, 1 )
    showImage( mask )
    title('Mask given by mesh')
end


% Apply the mask to the image we're analyzing by setting the intensity of all voxels not belonging
% to the brain mask to zero. This will automatically discard those voxels in subsequent C++ routines, as
% voxels with intensity zero are simply skipped in the computations.
% Note that it is not sufficient to simply change the intensities in the Matlab matrix imageBuffer holding
% a copy of the image intensities - we also need to explicitly write any modifications to the ITK object!
imageBuffer( find( ~mask ) ) = 0;
kvlSetImageBuffer( image, imageBuffer );
imageBuffer = kvlGetImageBuffer( image );
if ~isdeployed && DEBUG>0
    subplot( 1, 2, 2 )
    showImage( imageBuffer )
    title('Masked image Buffer')
end


% Merge classes
%%%%%%%%%%%%%%%
% Eugenio July 2017

FreeSurferLabelGroups=[];
if highres==0
    FreeSurferLabelGroups{end+1}={'Left-Cerebral-Cortex','Left-Hippocampus','Left-Amygdala','subiculum-head','subiculum-body','Hippocampal_tail','GC-ML-DG-head','GC-ML-DG-body','CA4-head','CA4-body','presubiculum-head','presubiculum-body',...
        'CA1-head','CA1-body','parasubiculum','CA3-head','CA3-body','HATA','Lateral-nucleus','Paralaminar-nucleus',...
        'Basal-nucleus','Hippocampal-amygdala-transition-HATA','Accessory-Basal-nucleus','Amygdala-background',...
        'Corticoamygdaloid-transitio','Central-nucleus','Cortical-nucleus','Medial-nucleus',...
        'Anterior-amygdaloid-area-AAA','molecular_layer_HP-body','molecular_layer_HP-head'};
else
    FreeSurferLabelGroups{end+1}={'Left-Cerebral-Cortex','Left-Hippocampus','Left-Amygdala','subiculum-head','subiculum-body','Hippocampal_tail','GC-ML-DG-head','GC-ML-DG-body','CA4-head','CA4-body','presubiculum-head','presubiculum-body',...
        'CA1-head','CA1-body','parasubiculum','CA3-head','CA3-body','HATA','Lateral-nucleus','Paralaminar-nucleus',...
        'Basal-nucleus','Hippocampal-amygdala-transition-HATA','Accessory-Basal-nucleus','Amygdala-background',...
        'Corticoamygdaloid-transitio','Central-nucleus','Cortical-nucleus','Medial-nucleus',...
        'Anterior-amygdaloid-area-AAA'};
    FreeSurferLabelGroups{end+1}={'molecular_layer_HP-body','molecular_layer_HP-head'};
end
FreeSurferLabelGroups{end+1}={'Left-Cerebral-White-Matter','fimbria'};
FreeSurferLabelGroups{end+1}={'alveus'};
FreeSurferLabelGroups{end+1}={'Left-Lateral-Ventricle','Background-CSF','SUSPICIOUS','Left-hippocampus-intensity-abnormality'};
FreeSurferLabelGroups{end+1}={'hippocampal-fissure'};
FreeSurferLabelGroups{end+1}={'Left-Pallidum'};
FreeSurferLabelGroups{end+1}={'Left-Putamen'};
FreeSurferLabelGroups{end+1}={'Left-Caudate'};
FreeSurferLabelGroups{end+1}={'Left-Thalamus-Proper'};
FreeSurferLabelGroups{end+1}={'Left-choroid-plexus'};
FreeSurferLabelGroups{end+1}={'Left-VentralDC'};
FreeSurferLabelGroups{end+1}={'Left-Accumbens-area'};
FreeSurferLabelGroups{end+1}={'Unknown','Background-tissue'};

sameGaussianParameters=[];
for g=1:length(FreeSurferLabelGroups)
    sameGaussianParameters{end+1} = [];
    for FreeSurferLabel =  FreeSurferLabelGroups{g}
        sameGaussianParameters{end} = [ sameGaussianParameters{end} FreeSurferLabels( find( strcmp( FreeSurferLabel, cellstr( names ) ) ) ) ];
    end
    if isempty(sameGaussianParameters{end})
        sameGaussianParameters=sameGaussianParameters(1:end-1);
    end
end
numberOfClasses=length(sameGaussianParameters);


% Compute the "reduced" alphas - those referring to the "super"-structures
[ reducedAlphas, reducingLookupTable ] = kvlReduceAlphas( originalAlphas, compressionLookupTableFileName, sameGaussianParameters );
if ( max( abs( sum( reducedAlphas, 2 ) - 1 ) ) > 1e-5 ) % Make sure these vectors really sum to 1
    error( 'The vector of prior probabilities in the mesh nodes must always sum to one over all classes' )
end

% Set the reduced alphas to be the alphas of the mesh
kvlSetAlphasInMeshNodes( mesh, reducedAlphas )
if  ~isdeployed && DEBUG>0
    priors = kvlRasterizeAtlasMesh( mesh, imageSize );
    figure
    for reducedLabel = 1 : size( reducedAlphas, 2 )
        subplot( 3, 4, reducedLabel )
        showImage( priors( :, :, :, reducedLabel ) )
        title(['Prior for "reduced" class ' num2str(reducedLabel)]);
    end
    priorsBefore=priors;
    clear priors
end


% Compute hyperparameters for estimation of Gaussian parameters
disp('Computing hyperparameters for estimation of Gaussian parameters')

DATA=myMRIread([subjectDir '/' subjectName '/mri/norm.mgz'],0,tempdir);
WMPARC=myMRIread([subjectDir '/' subjectName '/mri/wmparc.mgz'],0,tempdir);
aux=myMRIread('hippoMaskDilated5mm.mgz',0,tempdir);
WMPARC.vol(WMPARC.vol==0 & aux.vol==0)=-1;

nHyper=zeros(length(sameGaussianParameters),1);
meanHyper=zeros(length(sameGaussianParameters),1);
BGindex=0;
for g=1:length(sameGaussianParameters)
    labels=sameGaussianParameters{g};
    if any(labels==0)
        BGindex=g;
    end
    if any(labels==3 | labels==17 | labels==18 | labels > 7000 | labels==226)  % gray matter
        if strcmp(side,'left')>0, listMask=17; else, listMask=53; end
        % listMask=[3 42 17 53 18 54];
    elseif any(labels==2)  % white matter
        if strcmp(side,'left')>0, listMask=[3006 3007 3016]; else, listMask=[4006 4007 4016]; end
        % listMask=[2 41];
    elseif any(labels==26)    % accumbens area
        if strcmp(side,'left')>0, listMask=26; else, listMask=58; end
        % listMask=[26 58];
    elseif any(labels==4)  % CSF
        if strcmp(side,'left')>0, listMask=4; else, listMask=43; end
        % listMask=[4 43 14 15] ;
    elseif any(labels==0) % Background
        listMask=0;
    elseif any(labels==13)
        if strcmp(side,'left')>0, listMask=13; else, listMask=52; end
        % listMask=[13 52];
    elseif any(labels==12)
        if strcmp(side,'left')>0, listMask=12; else, listMask=51; end
        % listMask=[12 51];
    elseif any(labels==11)
        if strcmp(side,'left')>0, listMask=11; else, listMask=50; end
        % listMask=[11 50];
    elseif any(labels==10)
        if strcmp(side,'left')>0, listMask=10; else, listMask=49; end
        % listMask=[10 49];
    elseif any(labels==31)
        if strcmp(side,'left')>0, listMask=31; else, listMask=63; end
        % listMask=[31 63];
    elseif any(labels==28)
        if strcmp(side,'left')>0, listMask=28; else, listMask=60; end
        % listMask=[28 60];
    else
        listMask=[];
    end
    if length(listMask)>0
        MASK=zeros(size(DATA.vol));
        for l=1:length(listMask)
            MASK=MASK | WMPARC.vol==listMask(l);
        end
        % MASK=imerode(MASK,createSphericalStrel(1));
        MASK=imerode(MASK,createSphericalStrel(round(1/mean(DATA.volres))));
        data=DATA.vol(MASK & DATA.vol>0);
        meanHyper(g)=median(data);
        % nHyper(g)=10+length(data)/resolution^3;
        nHyper(g)=10+length(data)*prod(DATA.volres)/resolution^3;
    end
end
% if any nan, replace by background
ind=find(isnan(meanHyper));
meanHyper(ind)=55;
nHyper(ind)=10;


% Here's the part where we simulate partial voluming!
% disp('Estimating typical intensities of molecular layer and alveus')
disp('Estimating typical intensities of  alveus')
WMind=-1;
GMind=-1;
ALind=-1;
MLind=-1;
FISSind=-1;
CSFind=-1;
for g=1:length(sameGaussianParameters)
    labels=sameGaussianParameters{g};
    if any(labels==2)
        WMind=g;
    end
    if any(labels==3)
        GMind=g;
    end
    if any(labels==201)
        ALind=g;
    end
    if any(labels==245) && highres>0  % Eugenio July 2017 (changed 214 by 245)
        MLind=g;
    end
    if any(labels==215)
        FISSind=g;
    end
    if any(labels==4)
        CSFind=g;
    end
end

% Eugenio July 2017: again, rasterize priors one at the time
for l = 1 : size(reducedAlphas,2)    
    if l==1
       % This is a bit annoying, but the call to kvlRasterize with a single
       % label fills in the voxels outside the cuboid with p=1 (whereas the
       % call with multiple labels does not)
        sillyAlphas=zeros([size(reducedAlphas,1),2],'single');
        sillyAlphas(:,1)=reducedAlphas(:,1);
        sillyAlphas(:,2)=1-reducedAlphas(:,1);
        kvlSetAlphasInMeshNodes( mesh, sillyAlphas )
        prior = kvlRasterizeAtlasMesh( mesh, imageSize);
        kvlSetAlphasInMeshNodes( mesh, reducedAlphas )
        prior=prior(:,:,:,1);
        sumpriors=prior;
        L=ones(size(prior));
        PMAX=prior;
    else
        prior = kvlRasterizeAtlasMesh( mesh, imageSize, l-1 );
        sumpriors=sumpriors+prior;
        M=prior>PMAX;
        L(M)=l;
        PMAX(M)=prior(M);
    end
end
suma=single(sumpriors)/65535;
maskPriors=suma>.97;

% priors = kvlRasterizeAtlasMesh( mesh, imageSize );
% priors=double(priors)/65535;
% suma=sum(priors,4);
% maskPriors=suma>.97;
% priors=priors./(eps+repmat(suma,[1 1 1 numberOfClasses]));
% [~,L]=max(priors,[],4);



I=zeros(size(L));
for l=1:numberOfClasses
    if l==ALind || l==MLind
        I(L==l)=meanHyper(WMind);
    elseif l==FISSind
        I(L==l)=meanHyper(CSFind);
    else
        I(L==l)=meanHyper(l);
    end
end
I(~maskPriors)=0;
% I_PV=GaussFilt3d(I,1/(2.355*resolution));
I_PV=GaussFilt3d(I,mean(DATA.volres)/(2.355*resolution));

if ALind~=-1
    data=I_PV(L==ALind); % it's multimodal, so median won't cut it...
    [density,v]=ksdensity(data);
    [trash,idx]=max(density);
    meanHyper(ALind)=median(v(idx));
    nHyper(ALind)=(nHyper(GMind)+nHyper(WMind))/2;
end

if highres>0
    data=I_PV(L==MLind);
    meanHyper(MLind)=median(data);
    nHyper(MLind)=(nHyper(WMind)+nHyper(GMind))/2;
end

if FISSind~=-1
    data=I_PV(L==FISSind);
    meanHyper(FISSind)=median(data);
    nHyper(FISSind)=(nHyper(CSFind)+nHyper(GMind))/2;
end


%
% Multi-resolution scheme
%
% Specify here the size of the standard deviation of the Gaussian kernel used to smooth the priors/mesh. Use
% if you don't want to use multi-resolution
meshSmoothingSigmas = [ 1.5 .75 0 ]';
imageSmoothingSigmas = [0 0 0]'; 
maxItNos=[7 5 3];  % each iteration has 40 deformation steps

numberOfMultiResolutionLevels = length( meshSmoothingSigmas );

% Now the real work...
if  ~isdeployed && DEBUG>0
    multiResolutionFigure = figure;
    EMResultsFigure = figure;
    deformationMovieFigure = figure;
    costFigure = figure;
end
maskIndices = find( mask );


time_ref_optimization=clock;

imageBufferOrig=imageBuffer;

for multiResolutionLevel = 1 : numberOfMultiResolutionLevels
    
    % Smooth the mesh using a Gaussian kernel.
    kvlSetAlphasInMeshNodes( mesh, reducedAlphas )
    meshSmoothingSigma = meshSmoothingSigmas( multiResolutionLevel );
    fprintf( 'Smoothing mesh collection with kernel size %f ...', meshSmoothingSigma )
    kvlSmoothMeshCollection( meshCollection, meshSmoothingSigma )
    fprintf( 'done\n' )
    
    % Smooth the image using a Gaussian kernel
    imageSigma=imageSmoothingSigmas(multiResolutionLevel);
    if imageSigma>0
        imageBuffer=single(GaussFilt3dMask(imageBufferOrig,imageBufferOrig>0,imageSigma,resolution*ones(1,3)));
        kvlSetImageBuffer(image,imageBuffer);
    end
    
    
    % Show the smoothed atlas
    if  ~isdeployed && DEBUG>0
        figure( multiResolutionFigure )
        priors = kvlRasterizeAtlasMesh( mesh, imageSize );
        for reducedLabel = 1 : size( reducedAlphas, 2 )
            subplot( 3, 4, reducedLabel )
            showImage( priors( :, :, :, reducedLabel ) );
            title(['prior for reduced label ' num2str(reducedLabel)]);
        end
        clear priors
        subplot( 3, 4, 12 )
        showImage( kvlGetImageBuffer( image ) )
        title('image')
    end
    
    % Now with this smoothed atlas, we're ready for the real work. There are essentially two sets of parameters
    % to estimate in our generative model: (1) the mesh node locations (parameters of the prior), and (2) the
    % means and variances of the Gaussian intensity models (parameters of the
    % likelihood function, which is really a hugely simplistic model of the MR imaging process). Let's optimize
    % these two sets alternately until convergence. Optimizing the mesh node locations
    % is provided as a black box type of thing as it's implemented in C++ using complicated code - the other
    % set is much much better to experiment with in Matlab.
    
    
    % Iterations
    % Eugenio November 2017: we now do 30 instead of 20, since it's a bit faster
    maximumNumberOfIterations = maxItNos(multiResolutionLevel);  % Maximum number of iterations (includes one imaging model parameter estimation and
    positionUpdatingMaximumNumberOfIterations = 30;
    
    if FAST>0, maximumNumberOfIterations = 3; end  % in case we just wanna cruise throught it :-)
    
    
    historyOfCost = [ 1/eps ];
    
    % Compute a color coded version of the atlas prior in the atlas's current pose, i.e., *before*
    % we start deforming. We'll use this just for visualization purposes
    if  ~isdeployed && DEBUG>0
        oldColorCodedPriors = kvlColorCodeProbabilityImages( kvlRasterizeAtlasMesh( mesh, imageSize ) );
        figure( deformationMovieFigure )
        showImage( oldColorCodedPriors )
    end
    
    % Let's write this to file. In Unix/Linux systems, you can visualize progress over iterations
    % by doing animate -delay 10 *.png
    if  ~isdeployed && DEBUG>0
        fileName = [ 'colorCodedPrior_multiResolutionLevel' num2str( multiResolutionLevel ) '_iteration' sprintf( '%03d', 0 ) '.png' ];
        tmp = getframe( gcf );
        imwrite( tmp.cdata, fileName );
    end
    
    
    for iterationNumber = 1 : maximumNumberOfIterations
        disp(['Iteration ' num2str(iterationNumber) ' of ' num2str(maximumNumberOfIterations)]);
        %
        % Part I: estimate Gaussian mean and variances using EM
        %
        % See the paper
        %
        %     Automated Model-Based Bias Field Correction of MR Images of the Brain
        %     K. Van Leemput, F. Maes, D. Vandermeulen, P. Suetens
        %    IEEE Transactions on Medical Imaging, vol. 18, no. 10, pp. 885-896, October 1999
        %
        
        % Get the priors as dictated by the current mesh position, as well as the image intensities
        data = double( reshape( kvlGetImageBuffer( image ), [ prod( imageSize ) 1 ] ) ); % Easier to work with vector notation in the computations
        
        % Eugenio July 2017: again, avoid spike of memory use
        priors=zeros([length(maskIndices),numberOfClasses],'uint16');
        for l=1:numberOfClasses
            prior = kvlRasterizeAtlasMesh( mesh, imageSize, l-1 );
            priors(:,l)=prior(maskIndices);
        end
        
%         priors = kvlRasterizeAtlasMesh( mesh, imageSize );
%         priors = reshape( priors, [ prod( imageSize ) numberOfClasses ] ); % Easier to work with vector notation in the computations
%         priors = priors( maskIndices, : );  % Ignore everything that's has zero intensity
        
        
        data = data( maskIndices ); % Ignore everything that's has zero intensity
        
        % Start EM iterations. Initialize the parameters if this is the
        % first time ever you run this
        EPS=1e-2;
        posteriors = double( priors ) / 65535;
        if ( ( multiResolutionLevel == 1) & ( iterationNumber == 1 ) )
            
            for classNumber = 1 : numberOfClasses
                posterior = posteriors( :, classNumber );
                
                if sum(posterior)>EPS
                    
                    %   mu = data' * posterior / ( sum( posterior ) + eps );
                    %   variance = ( ( data - mu ).^2 )' * posterior / ( sum( posterior ) + eps );
                    
                    mu = (meanHyper(classNumber)*nHyper(classNumber) + data'*posterior) / ( nHyper(classNumber) + sum( posterior ) + EPS );
                    variance = (( ( data - mu ).^2 )' * posterior + nHyper(classNumber)*(mu-meanHyper(classNumber))^2 )/ ( sum( posterior ) + EPS );
                    
                    means( classNumber ) = mu;
                    variances( classNumber ) = variance+EPS;
                    
                else
                    means( classNumber ) = meanHyper(classNumber);
                    variances( classNumber ) = 100;
                    
                end
            end
            variances(variances==0)=100; % added by Eugenio, prevents nans...
            
            
        end % End test need for initialization
        
        stopCriterionEM = 1e-5;
        historyOfEMCost = [ 1/eps ];
        
        for EMIterationNumber = 1 : 100
            %
            % E-step: compute the posteriors based on the current parameters
            %
            minLogLikelihood = 0;
            for classNumber = 1 : numberOfClasses
                mu = means( classNumber );
                variance = variances( classNumber );
                prior = single( priors( :, classNumber ) ) / 65535;
                posteriors( :, classNumber ) = ( exp( -( data - mu ).^2 / 2 / variance ) .* prior ) ...
                    / sqrt( 2 * pi * variance);
                
                minLogLikelihood = minLogLikelihood+0.5*log(2*pi*variance)-0.5*log(nHyper(classNumber))...
                    +0.5*nHyper(classNumber)/variance*(mu-meanHyper(classNumber))^2; % contribution from prior
                
            end
            normalizer = sum( posteriors, 2 ) + eps;
            
            % Eugenio July 2017
            % posteriors = posteriors ./ repmat( normalizer, [ 1 numberOfClasses ] );
             posteriors = bsxfun(@rdivide,posteriors, normalizer);
            
            minLogLikelihood =  minLogLikelihood - sum( log( normalizer ) ) % This is what we're optimizing with EM
            if isnan(minLogLikelihood)
                error('lhood is nan');
            end
            
            historyOfEMCost = [ historyOfEMCost; minLogLikelihood ];
            
            
            % Check for convergence
            relativeChangeCost = ( historyOfEMCost(end-1) - historyOfEMCost(end) ) / ...
                historyOfEMCost(end);
            if ( relativeChangeCost < stopCriterionEM )
                % Converged
                disp( 'EM converged!' )
                break;
            end
            
            
            % Show posteriors
            if  ~isdeployed && DEBUG>0
                figure( EMResultsFigure )
                posterior = zeros( imageSize );
                for classNumber = 1 : numberOfClasses
                    subplot( 3, 4, classNumber )
                    posterior( maskIndices ) = posteriors( :, classNumber );
                    showImage( posterior )
                    title(['Posterior for class ' num2str(classNumber)]);
                end
            end
            
            % Also show EM cost function
            if  ~isdeployed && DEBUG>0
                subplot( 3, 4, 12 )
                plot( historyOfEMCost( 2 : end ) )
                title( 'EM cost' )
            end
            
            % Refresh figures
            drawnow
            %pause
            
            
            %
            % M-step: derive parameters from the posteriors
            %
            
            % Update parameters of Gaussian mixture model
            for classNumber = 1 : numberOfClasses
                posterior = posteriors( :, classNumber );
                EPS=1e-2;
                
                if sum(posterior)>EPS
                    
                    %   mu = data' * posterior / ( sum( posterior ) + eps );
                    %   variance = ( ( data - mu ).^2 )' * posterior / ( sum( posterior ) + eps );
                    
                    mu = (meanHyper(classNumber)*nHyper(classNumber) + data'*posterior) / ( nHyper(classNumber) + sum( posterior ) + EPS );
                    variance = (( ( data - mu ).^2 )' * posterior + nHyper(classNumber)*(mu-meanHyper(classNumber))^2 )/ ( sum( posterior ) + EPS );
                    
                    means( classNumber ) = mu;
                    variances( classNumber ) = variance+EPS;
                    
                else
                    means( classNumber ) = meanHyper(classNumber);
                    variances( classNumber ) = 100;
                    
                end
                
            end
            variances(variances==0)=100; % added by Eugenio, prevents nans...
            
            
        end % End EM iterations
        means'
        (variances').^2
        
        %
        % Part II: update the position of the mesh nodes for the current set of Gaussian parameters
        %
        
        % Do the deformation one step at a time, for maximally positionUpdatingMaximumNumberOfIterations
        % deformation steps or until a step occurs in which the mesh node that moves most moves less than
        % maximalDeformationStopCriterion voxels, whichever comes first. The underlying algorithm is a
        % Levenberg-Marquardt type of algorithm in that it uses the gradient and an approximation of the
        % Hessian to propose a new position. If the new position proposal degrades the cost function (i.e.,
        % the posterior probability of the mesh node positions given the data and the parameters of the
        % imaging model goes down), the Hessian approximation is repeatedly altered by multiplying its diagonal
        % elements with an increasing factor, thereby making the proposal more and more gradient-descent
        % like with smaller-and-smaller step sizes, until a (small) position proposal is obtained that actually
        % improves the cost function. Conversely, every time a good position proposal is obtained, the
        % multiplication of the diagonal elements of the Hessian approximation is decreased the next time
        % around, making the algorithm much more efficient compared to gradient-descent (i.e., take much
        % larger step sizes) whenever it is possible.
        %
        % If no position proposal can be made even when the multiplication factor of the Hessian approximation's
        % diagonal becomes very large, i.e., even when the proposal is a tiny tiny deformation only, the
        % mesh node optimization algorithm gives up and tells you it didn't do anything.
        %
        %
        % NOTE: recall that this procedure is really only one half of a global optimization problem
        % that includes estimating the imaging model parameters (i.e., Gaussian intensity as well.
        % Therefore, it may not make sense to wait 20 minutes to get a really good optimization
        % of the mesh node positions here, as the cost function we're optimizing will change anyway
        % once the imaging model parameters are updated in the next iterations. Since updating the
        % imaging model parameters is very fast compared to updating the mesh nodes, it probably makes
        % sense to re-estimate the imaging model parameters frequently after a partial (not full)
        % optimization of the mesh nodes.
        %
        
        % Eugenio November 2017: GEMS2
        if ( exist( 'optimizer', 'var' ) == 1 )
            % The optimizer is very memory hungry when run in multithreaded mode.
            % Let's clear any old ones we may have lying around
            try
                % Eugenio November 2017: GEMS2
                kvlClear( optimizer );
                kvlClear( calculator );
            catch ME
            end
        end
        
        haveMoved = false; % Keep track if we've ever moved or not
        
         % (note that it uses variances instead of precisions)
        calculator = kvlGetCostAndGradientCalculator('AtlasMeshToIntensityImage',...
            image, 'Sliding',transform,means',variances',ones(size(means')),ones(size(means')));
        
        verbose=0;
        maximalDeformationStopCriterion=1e-10;
        lineSearchMaximalDeformationIntervalStopCriterion=1e-10;
        maximumNumberOfDeformationIterations=1000;
        BFGSMaximumMemoryLength=12;
        
        optimizer = kvlGetOptimizer( optimizerType, mesh, calculator, ...
            'Verbose', verbose, ...
            'MaximalDeformationStopCriterion', maximalDeformationStopCriterion, ...
            'LineSearchMaximalDeformationIntervalStopCriterion', ...
            lineSearchMaximalDeformationIntervalStopCriterion, ...
            'MaximumNumberOfIterations', maximumNumberOfDeformationIterations, ...
            'BFGS-MaximumMemoryLength', BFGSMaximumMemoryLength );
        
                                
        for positionUpdatingIterationNumber = 1 : positionUpdatingMaximumNumberOfIterations
            % Calculate a good step. The first one is very slow because of various set-up issues
            disp(['Resolution level ' num2str(multiResolutionLevel) ' iteration ' num2str(iterationNumber) ' deformation iterations ' num2str(positionUpdatingIterationNumber)]);
            % Eugenio May2018
            maximalDeformation=0;
            try
                tic
                % Eugenio November 2017: GEMS2
                [ minLogLikelihoodTimesPrior, maximalDeformation ] = kvlStepOptimizer( optimizer );
                elapsedTime = toc;
                disp( [ 'Did one deformation step of max. ' num2str( maximalDeformation )  ' voxels in ' num2str( elapsedTime ) ' seconds' ] )
                minLogLikelihoodTimesPrior
            end
            if isnan(minLogLikelihoodTimesPrior)
                error('lhood is nan');
            end
            if ( maximalDeformation > 0 )
                haveMoved = true;
            end
            
            % Test if we need to stop
            if ( maximalDeformation <= maximalDeformationStopCriterion )
                disp( 'maximalDeformation is too small; stopping' );
                break;
            end
        end
        
        % Show a little movie comparing before and after deformation so far...
        if  ~isdeployed && DEBUG>0
            figure( deformationMovieFigure )
            newColorCodedPriors = kvlColorCodeProbabilityImages( kvlRasterizeAtlasMesh( mesh, imageSize ) );
            for i=1:10
                showImage( oldColorCodedPriors ), title('Previous priors')
                drawnow
                pause( 0.1 )
                showImage( newColorCodedPriors ), title('Updated priors')
                drawnow
                pause( 0.1 )
            end
        end
        
        % Let's write this to file. In Unix/Linux systems, you can visualize progress over iterations
        % by doing animate -delay 10 *.png
        if  ~isdeployed && DEBUG>0
            fileName = [ 'colorCodedPrior_multiResolutionLevel' num2str( multiResolutionLevel ) '_iteration' sprintf( '%03d', iterationNumber ) '.png' ];
            tmp = getframe( gcf );
            imwrite( tmp.cdata, fileName );
        end
        
        
        % Keep track of the cost function we're optimizing
        historyOfCost = [ historyOfCost; minLogLikelihoodTimesPrior ];
        if  ~isdeployed && DEBUG>0
            figure( costFigure )
            plot( historyOfCost( 2 : end ) )
            title( 'Cost' )
        end
        
        % Determine if we should stop the overall iterations over the two set of parameters
        if ( ( ~haveMoved ) || ( ( ( historyOfCost( end-1 ) - historyOfCost( end ) ) / historyOfCost( end ) ) <  1e-6 ) )
            % Converged
            break;
        end
        
        
    end % End looping over global iterations
    
    
    
end % End loop over multiresolution levels

disp(['Fitting mesh to image data took ' num2str(etime(clock,time_ref_optimization)) ' seconds']);

% Restore original image buffer
kvlSetImageBuffer(image,imageBufferOrig);

% OK, now that all the parameters have been estimated, segment the image with all the original
% labels instead of the reduced "super"-structure labels we created.

% Clear some memory
% Eugenio November 2017: GEMS2
try
    kvlClear( optimizer )
    kvlClear( calculator )
end

%
if  ~isdeployed && DEBUG>0
    figure
    subplot( 2, 1, 1 )
    showImage( kvlColorCodeProbabilityImages( kvlRasterizeAtlasMesh( mesh, imageSize )  ) );
    title('Priors')
    subplot( 2, 1, 2 )
    showImage( imageBuffer )
    title('Image')
end

% Undo the collapsing of several structures into "super"-structures
kvlSetAlphasInMeshNodes( mesh, originalAlphas )
numberOfClasses = size( originalAlphas, 2 );

% Eugenio July 2017 : this type of call is exactly what we're tryint to
% avoid....
data = double( reshape( imageBuffer, [ prod( imageSize ) 1 ] ) ); % Easier to work with vector notation in the computations
data = data( maskIndices );

posteriors=zeros([length(maskIndices),numberOfClasses],'single');
for classNumber = 1 : numberOfClasses
    prior = kvlRasterizeAtlasMesh( mesh, imageSize, classNumber-1 );
    mu = means( reducingLookupTable( classNumber ) );
    variance = variances( reducingLookupTable( classNumber ) );
    posteriors( :, classNumber ) = ( exp( -( data - mu ).^2 / 2 / variance ) ...
        .* (double(prior(maskIndices))/65535) ) / sqrt( 2 * pi * variance);
end
normalizer = sum( posteriors, 2 ) + eps;
posteriors = bsxfun(@rdivide,posteriors, normalizer);
posteriors = uint16( round( posteriors * 65535 ) );
 

% % Get the priors as dictated by the current mesh position
% data = double( reshape( imageBuffer, [ prod( imageSize ) 1 ] ) ); % Easier to work with vector notation in the computations
% priors = kvlRasterizeAtlasMesh( mesh, imageSize );
% priors = reshape( priors, [ prod( imageSize ) numberOfClasses ] );  % Easier to work with vector notation in the computations
% 
% % Ignore everything that's has zero intensity
% priors = priors( maskIndices, : );
% data = data( maskIndices );
% 
% % Calculate the posteriors
% posteriors = zeros( size( priors ), 'double' );
% for classNumber = 1 : numberOfClasses
%     % Get the parameters from the correct Gaussian
%     mu = means( reducingLookupTable( classNumber ) );
%     variance = variances( reducingLookupTable( classNumber ) );
%     prior = single( priors( :, classNumber ) ) / 65535;
%     
%     posteriors( :, classNumber ) = ( exp( -( data - mu ).^2 / 2 / variance ) .* prior ) ...
%         / sqrt( 2 * pi * variance);
% end
% normalizer = sum( posteriors, 2 ) + eps;
% posteriors = posteriors ./ repmat( normalizer, [ 1 numberOfClasses ] );
% posteriors = uint16( round( posteriors * 65535 ) );

% Display the posteriors
if  ~isdeployed && DEBUG>0
    
    priors = kvlRasterizeAtlasMesh( mesh, imageSize );
    priors = reshape( priors, [ prod( imageSize ) numberOfClasses ] );  % Easier to work with vector notation in the computations
    priors = priors( maskIndices, : );

    figure
    subplot( 1, 2, 1 )
    showImage( imageBuffer ); title('Image')
    subplot( 1, 2, 2 )
    tmp = zeros( prod(imageSize), numberOfClasses, 'uint16' );
    tmp( maskIndices, : ) = priors;
    tmp = reshape( tmp, [ imageSize numberOfClasses ] );
    showImage( kvlColorCodeProbabilityImages( tmp, colors ) ); title('Warped priors')
    
    figure
    for classNumber = 1 : numberOfClasses
        
        % Let's create a color image overlaying the posterior on top of gray scale
        overlayColor = [ 0 0 1 ]; % RGB value
        overlay = double( exp( imageBuffer / 1000 )  );
        overlay = overlay - min( overlay(:) );
        overlay = overlay / max( overlay(:) );
        posterior = single( tmp( :, :, :, classNumber ) / 65535 );
        overlay = overlay .* ( 1 - posterior );
        overlay = repmat( overlay, [ 1 1 1 3 ] );
        colorPosterior = reshape( posterior(:) * overlayColor, [ imageSize 3 ] );
        showImage( overlay + colorPosterior );
        
        name = deblank( names( classNumber, : ) );
        title( name, 'interpreter', 'none' )
        
        
        % disp( 'Enter to continue' )
        % pause
        drawnow
        pause( 0.5 )
        
    end
end

% Show the histogram of each structure, where individual voxels are weight according
% to their posterior of belonging to that structure. Overlay on top of it the Gaussian
% intensity model estimated with the EM procedure above - any clear deviations between
% the histogram and the Gaussian model indicate that the model is a bad fit for that
% structure, and that more accurate image modeling may be needed there.
if  ~isdeployed && DEBUG>0
    range = [ min( data(:) ) max( data(:) ) ];
    numberOfBins = 100;
    binEdges = [ 0 : numberOfBins ] / numberOfBins;
    binEdges = binEdges * ( range(2) - range(1) ) + range( 1 );
    binDelta = binEdges(2) - binEdges(1);
    binCenters = binEdges( 1 : numberOfBins ) + binDelta/2;
    [ totalHistogram, binNumbers ] = histc( data, binEdges );
    figure
    bar( binCenters, totalHistogram(1:end-1) )
    figure
    for classNumber = 1 : numberOfClasses
        posterior = double( posteriors( :, classNumber ) / 65535 );
        
        histogram = zeros( numberOfBins, 1 );
        for binNumber = 1 : numberOfBins % Ough this is hugely inefficient in Matlab
            histogram( binNumber ) = sum( posterior( find( binNumbers == binNumber ) ) );
        end
        histogram = histogram / sum( histogram );
        bar( binCenters, histogram )
        hold on
        mu = means( reducingLookupTable( classNumber ) );
        variance = variances( reducingLookupTable( classNumber ) );
        gauss = exp( -( binCenters - mu ).^2 / 2 / variance ) / sqrt( 2 * pi * variance );
        p = plot( binCenters, gauss * binDelta, 'r' );
        hold off
        name = deblank( names( classNumber, : ) );
        title( name, 'interpreter', 'none' )
        
        
        %         disp( 'Enter to continue' )
        %         pause
        drawnow
        pause( 0.5 )
    end
end

% Write the resulting atlas mesh, as well as the image we segmented, to file for future
% reference.
%
kvlWriteMeshCollection( meshCollection, 'warpedMesh.txt' ); % The warped mesh will have index 0, and will be

transformMatrix = double(kvlGetTransformMatrix( transform ));
inverseTransform = kvlCreateTransform( inv( transformMatrix ) );
kvlTransformMeshCollection( meshCollection, inverseTransform );
kvlWriteMeshCollection( meshCollection, 'warpedMeshNoAffine.txt' );



kvlWriteImage( image, 'image.mgz' );
system(['cp ' compressionLookupTableFileName ' .']);


% You can now use the C++ tools distributed with FreeSufer to inspect what we have as
% follows:
%
%    kvlViewMeshCollectionWithGUI warpedMesh.txt.gz 86 113 163 image.mgz
%
% where 86 113 163 are the dimensions of image.mgz, which you need to manually specify (don't ask!)
%


% Eugenio: write discrete labels (MAP)
% Note how we fill in the gaps in the regions outside the FOV with the
% prior!

% First we compute the shift when using kvlReadCroppedImage/kvlWrite
system([FSpath '/mri_convert asegMod.mgz asmr1.mgz -rl T1resampled.mgz -rt nearest -odt float >/dev/null']);
[ asmr, tasmr ] = kvlReadCroppedImage( 'asmr1.mgz', boundingFileName );
asmrB=kvlGetImageBuffer(asmr);
kvlWriteImage( asmr, 'asmr2.mgz' );
tmp1=myMRIread('asmr1.mgz',0,tempdir);
tmp2=myMRIread('asmr2.mgz',0,tempdir);
tmp3=tmp1; tmp3.vol=tmp2.vol;
myMRIwrite(tmp3,'asmr3.mgz','float',tempdir);
[I,J,K]=ind2sub(size(tmp1.vol),find(tmp1.vol==17));
Ic1=mean(I); Jc1=mean(J); Kc1=mean(K);
[I,J,K]=ind2sub(size(tmp2.vol),find(tmp2.vol==17));
Ic2=mean(I); Jc2=mean(J); Kc2=mean(K);
shift=round([Ic2-Ic1,Jc2-Jc1,Kc2-Kc1]);
shiftNeg=-shift; shiftNeg(shiftNeg<0)=0;
shiftPos=shift; shiftPos(shiftPos<0)=0;

% reorient image.mgz
aux=zeros(size(tmp2.vol)+shiftNeg);
aux2=myMRIread('image.mgz',0,tempdir);
aux(1+shiftNeg(1):shiftNeg(1)+size(aux2.vol,1),1+shiftNeg(2):shiftNeg(2)+size(aux2.vol,2),1+shiftNeg(3):shiftNeg(3)+size(aux2.vol,3))=aux2.vol;
aux=aux(1+shiftPos(1):end,1+shiftPos(2):end,1+shiftPos(3):end);
tmp3.vol=aux;
myMRIwrite(tmp3,'image.mgz','float',tempdir);



% Compute posteriors and volumes

% July 2017: make this memory efficient, and add new substructures. We also
% compute the segmentation along the way. Also, we do hippo and amygdala
% together. Ah! And we also do volumes for head and body
fidHP=fopen([tempdir '/volumesHippo.txt'],'w');
strOfInterestHP={'subiculum-body','subiculum-head','Hippocampal_tail','molecular_layer_HP-body','molecular_layer_HP-head','hippocampal-fissure','GC-ML-DG-body','GC-ML-DG-head','CA4-body','CA4-head','presubiculum-body','presubiculum-head','CA1-body','CA1-head','parasubiculum','fimbria','CA3-body','CA3-head','HATA'};
totVolHP=0;
foundHP=zeros([1,numberOfClasses]);
HPbodyList={'subiculum-body','CA1-body','presubiculum-body','molecular_layer_HP-body','CA3-body','GC-ML-DG-body','CA4-body','fimbria'};
HPheadList={'subiculum-head','presubiculum-head','CA1-head','parasubiculum','molecular_layer_HP-head','GC-ML-DG-head','CA4-head','CA3-head','HATA'};
totVolHPbody=0;
totVolHPhead=0;

fidAM=fopen([tempdir '/volumesAmygdala.txt'],'w');
strOfInterestAM={'Left-Amygdala','Lateral-nucleus','Paralaminar-nucleus',...
    'Basal-nucleus','Hippocampal-amygdala-transition-HATA','Accessory-Basal-nucleus','Amygdala-background',...
    'Corticoamygdaloid-transitio','Central-nucleus','Cortical-nucleus','Medial-nucleus','Anterior-amygdaloid-area-AAA'};
totVolAM=0;
foundAM=zeros([1,numberOfClasses]);

for i=1:numberOfClasses
    
    if i==1
        sillyAlphas=zeros([size(originalAlphas,1),2],'single');
        sillyAlphas(:,1)=originalAlphas(:,1);
        sillyAlphas(:,2)=1-sillyAlphas(:,1);
        kvlSetAlphasInMeshNodes( mesh, sillyAlphas )
        post = kvlRasterizeAtlasMesh( mesh, imageSize);
        post=post(:,:,:,1);
        kvlSetAlphasInMeshNodes( mesh, originalAlphas );
    else
        post=kvlRasterizeAtlasMesh( mesh, imageSize , i-1);
    end
    post(maskIndices)=posteriors(:,i);
    
    if i==1
        L=ones(size(post));
        MAXP=post;
    else
        M=post>MAXP;
        L(M)=i;
        MAXP(M)=post(M);
    end
    
    foundAM(i)=0;
    foundHP(i)=0;
    
    name=names(i,:);
    name=lower(name(name~=' '));
    
    for j=1:length(strOfInterestHP)
        if strcmp(name,lower(strOfInterestHP{j}))>0
            foundHP(i)=j;
        end
    end
    
    for j=1:length(strOfInterestAM)
        if strcmp(name,lower(strOfInterestAM{j}))>0
            foundAM(i)=j;
        end
    end
    
    if foundHP(i)>0 || foundAM(i)>0
        vol=resolution^3*(sum(double(post(:))/65535));
        if foundHP(i)>0
            str=strOfInterestHP{foundHP(i)};
            fprintf(fidHP,'%s %f\n',str,vol);
            if isempty(strfind(lower(names(i,:)),'hippocampal-fissure'))  % don't count the fissure towards the total volume
                totVolHP=totVolHP+vol;
            end
        else
            str=strOfInterestAM{foundAM(i)};
            fprintf(fidAM,'%s %f\n',str,vol);
            totVolAM=totVolAM+vol;
        end
        
        for j=1:length(HPbodyList)
            if strcmp(name,lower(HPbodyList{j}))>0
                totVolHPbody=totVolHPbody+vol;
            end
        end
        for j=1:length(HPheadList)
            if strcmp(name,lower(HPheadList{j}))>0
                totVolHPhead=totVolHPhead+vol;
            end
        end
        
        if WRITE_POSTERIORS>0
            kk1=double(post)/65535;
            kk2=zeros(size(kk1)); kk2(maskIndices)=1; kk1=kk1.*kk2;
            aux=zeros(size(tmp2.vol)+shiftNeg);
            aux(1+shiftNeg(1):shiftNeg(1)+size(tmp2.vol,1),1+shiftNeg(2):shiftNeg(2)+size(tmp2.vol,2),1+shiftNeg(3):shiftNeg(3)+size(tmp2.vol,3))=permute(kk1,[2 1 3]);
            aux=aux(1+shiftPos(1):end,1+shiftPos(2):end,1+shiftPos(3):end);
            tmp3.vol=aux;
            myMRIwrite(tmp3,['posterior_' side '_' strtrim(lower(names(i,:))) '_T1_' suffix '.mgz'],'float',tempdir);
        end
        
    end
end
if sum(foundHP>0)>1
    fprintf(fidHP,'Whole_hippocampal_body %f\n',totVolHPbody);
    fprintf(fidHP,'Whole_hippocampal_head %f\n',totVolHPhead);    
    fprintf(fidHP,'Whole_hippocampus %f\n',totVolHP);
    fclose(fidHP);
else
    fclose(fidHP);
    delete([tempdir '/volumesHippo.txt']);
end
if sum(foundAM>0)>1
    fprintf(fidAM,'Whole_amygdala %f\n',totVolAM);
    fclose(fidAM);
else
    fclose(fidAM);
    delete([tempdir '/volumesAmygdala.txt']);
end



% priorsFull = kvlRasterizeAtlasMesh( mesh, imageSize );
% posteriorsFull=priorsFull;
% 
% fid=fopen([tempdir '/volumesHippo.txt'],'w');
% % strOfInterest={'alveus','subiculum','Hippocampal_tail','molecular_layer_HP','hippocampal-fissure','GC-ML-DG','CA4','presubiculum','CA1','parasubiculum','fimbria','CA3','HATA'};
% % no alveus
% strOfInterest={'subiculum','Hippocampal_tail','molecular_layer_HP','hippocampal-fissure','GC-ML-DG','CA4','presubiculum','CA1','parasubiculum','fimbria','CA3','HATA'};
% totVol=0;
% found=zeros(1,size(priorsFull,4));
% for i=1:size(priorsFull,4)
%     tmp=posteriorsFull(:,:,:,i);
%     tmp(maskIndices)=posteriors(:,i);
%     posteriorsFull(:,:,:,i)=tmp;
%     found(i)=0;
%     str=[];
%     vol=0;
%     name=names(i,:);
%     name=lower(name(name~=' '));
%     for j=1:length(strOfInterest)
%         if strcmp(name,lower(strOfInterest{j}))>0
%             found(i)=j;
%         end
%     end
%     if found(i)>0
%         str=strOfInterest{found(i)};
%         vol=resolution^3*(sum(sum(sum(double(posteriorsFull(:,:,:,i))/65535))));
%         fprintf(fid,'%s %f\n',str,vol);
%         if isempty(strfind(lower(names(i,:)),'hippocampal-fissure'))  % don't count the fissure towards the total volume
%             totVol=totVol+vol;
%             
%             if WRITE_POSTERIORS>0
%                 kk1=double(posteriorsFull(:,:,:,i))/65535;
%                 kk2=zeros(size(kk1)); kk2(maskIndices)=1; kk1=kk1.*kk2;
%                 aux=zeros(size(tmp2.vol)+shiftNeg);
%                 aux(1+shiftNeg(1):shiftNeg(1)+size(tmp2.vol,1),1+shiftNeg(2):shiftNeg(2)+size(tmp2.vol,2),1+shiftNeg(3):shiftNeg(3)+size(tmp2.vol,3))=permute(kk1,[2 1 3]);
%                 aux=aux(1+shiftPos(1):end,1+shiftPos(2):end,1+shiftPos(3):end);
%                 tmp3.vol=aux;
%                 myMRIwrite(tmp3,['posterior_' side '_' strtrim(lower(names(i,:))) '_T1_' suffix '.mgz'],'float',tempdir);
%             end
%         end
%     end
% end
% if sum(found>0)>1
%     fprintf(fid,'Whole_hippocampus %f\n',totVol);
%     fclose(fid);
% else
%     fclose(fid);
%     delete([tempdir '/volumesHippo.txt']);
% end



% MAP estimates

% Eugenio July 2011
% [~,inds]=max(posteriorsFull,[],4);
inds=L;

kk1=FreeSurferLabels(inds);
kk2=zeros(size(kk1)); kk2(maskIndices)=1; kk1=kk1.*kk2;
aux=zeros(size(tmp2.vol)+shiftNeg);
aux(1+shiftNeg(1):shiftNeg(1)+size(kk1,2),1+shiftNeg(2):shiftNeg(2)+size(kk1,1),1+shiftNeg(3):shiftNeg(3)+size(kk1,3))=permute(kk1,[2 1 3]);
aux=aux(1+shiftPos(1):end,1+shiftPos(2):end,1+shiftPos(3):end);
tmp3.vol=aux;
myMRIwrite(tmp3,'discreteLabels_all.mgz','float',tempdir);
tmp3.vol(tmp3.vol<200)=0;

% Eugenio July 2017
% tmp3.vol(tmp3.vol>226 & tmp3.vol<7000)=0;
tmp3.vol(tmp3.vol>246 & tmp3.vol<7000)=0;

tmp3.vol(tmp3.vol==201)=0; % alveus
tmp3Mask=getLargestCC(tmp3.vol>0);
tmp3.vol(~tmp3Mask)=0;
myMRIwrite(tmp3,'discreteLabels.mgz','float',tempdir);

% Eugenio July 2011
% Write merged versions to disk as well
% First: tail / body /head

HippoBodyLabel=231;
HippoHeadLabel=232;

tmp4=tmp3;
for c=1:numberOfClasses
    name=names(c,:);
    name=lower(name(name~=' '));
    
    found=0;
    for j=1:length(HPbodyList)
        if strcmp(name,lower(HPbodyList{j}))>0
            found=1;
        end
    end
    if found==1
        tmp4.vol(tmp4.vol==FreeSurferLabels(c))=HippoBodyLabel;
    end
    
    found=0;
    for j=1:length(HPheadList)
        if strcmp(name,lower(HPheadList{j}))>0
            found=1;
        end
    end
    if found==1
        tmp4.vol(tmp4.vol==FreeSurferLabels(c))=HippoHeadLabel;
    end
end
tmp4.vol(tmp4.vol==215)=0; % kill the fissure
myMRIwrite(tmp4,'discreteLabelsWholeBodyHead.mgz','float',tempdir);
    
% Second: head+body of each subfield
tmp4=tmp3;
tmp4.vol(tmp3.vol==233 | tmp3.vol==234)=204; % presubiculum
tmp4.vol(tmp3.vol==235 | tmp3.vol==236)=205; % subiculum
tmp4.vol(tmp3.vol==237 | tmp3.vol==238)=206; % CA1
tmp4.vol(tmp3.vol==239 | tmp3.vol==240)=208; % CA3
tmp4.vol(tmp3.vol==241 | tmp3.vol==242)=209; % CA4
tmp4.vol(tmp3.vol==243 | tmp3.vol==244)=210; % GC-DG
tmp4.vol(tmp3.vol==245 | tmp3.vol==246)=214; % ML

myMRIwrite(tmp4,'discreteLabelsMergedBodyHead.mgz','float',tempdir);

% Third: same as above, but getting rid of internal labels
tmp4.vol(tmp4.vol==210)=209;  % GC-DG -> CA4 

% Molecular layer: replace by nearest label that is not background or
% fissure
[~,cropping]=cropLabelVol(tmp4.vol==214,2);
VOL=applyCropping(tmp4.vol,cropping);
llist=unique(VOL);
llist=llist(llist~=0 & llist~=215 & llist~=214);
mask=VOL==214;
for l=1:length(llist)
   label=llist(l);
   dmap=bwdist(VOL==label);
   if l==1
       mini=dmap(mask);
       seg=label;
   else
       dist=dmap(mask);
       m=dist<mini;
       mini(m)=dist(m);
       seg(m)=label;
   end   
end
VOL(mask)=seg;
tmp4.vol(cropping(1):cropping(4),cropping(2):cropping(5),cropping(3):cropping(6))=VOL;
myMRIwrite(tmp4,'discreteLabelsMergedBodyHeadNoMLorGCDG.mgz','float',tempdir);



% Eugenio July 2017
% I disabled this for now ...
if  MRFconstant>0

    % Eugenio July 2017
    error('MRF smoothing disabled for now');
    
%     EPS=1e-12;
%     [~,inds]=max(posteriorsFull,[],4);
%     tmp=FreeSurferLabels(inds);
%     kk=zeros(size(tmp)); kk(maskIndices)=1; tmp=tmp.*kk;
%     tmp(tmp<200)=0; tmp(tmp>226 & tmp<7000)=0;
%     [~,cropping]=cropLabelVol(tmp);
%     Ct=zeros([cropping(4)-cropping(1)+1,cropping(5)-cropping(2)+1,cropping(6)-cropping(3)+1,numberOfClasses]);
%     for c=1:numberOfClasses
%         Ct(:,:,:,c)=-log(EPS+double(posteriorsFull(cropping(1):cropping(4),cropping(2):cropping(5),cropping(3):cropping(6),c))/65535);
%     end
%     factor=-256/log(EPS);
%     Ct=int32(round(Ct*factor));
%     unaryTermWeight=int32(round(MRFconstant*factor));
%     
%     siz=[size(Ct,1) size(Ct,2) size(Ct,3)];
%     h = GCO_Create(prod(siz),numberOfClasses);
%     DC = zeros([numberOfClasses,prod(siz)],'int32');
%     for c=1:numberOfClasses
%         aux=Ct(:,:,:,c);
%         DC(c,:)=aux(:);
%     end
%     GCO_SetDataCost(h,DC);
%     aux=int32(double(unaryTermWeight)*(ones(numberOfClasses)-eye(numberOfClasses)));
%     GCO_SetSmoothCost(h,aux);
%     
%     row=zeros([prod(siz)*3,1]);
%     col=zeros([prod(siz)*3,1]);
%     t=1;
%     
%     Ifrom=1:siz(1)-1;
%     Ito=2:siz(1);
%     inc=length(Ito);
%     for j=1:siz(2)
%         J=j*ones(size(Ifrom));
%         for k=1:siz(3)
%             K=k*ones(size(Ifrom));
%             row(t:t+inc-1)=sub2ind(siz,Ifrom,J,K);
%             col(t:t+inc-1)=sub2ind(siz,Ito,J,K);
%             t=t+inc;
%         end
%     end
%     
%     Jfrom=1:siz(2)-1;
%     Jto=2:siz(2);
%     inc=length(Jto);
%     for i=1:siz(1)
%         I=i*ones(size(Jfrom));
%         for k=1:siz(3)
%             K=k*ones(size(Jfrom));
%             row(t:t+inc-1)=sub2ind(siz,I,Jfrom,K);
%             col(t:t+inc-1)=sub2ind(siz,I,Jto,K);
%             t=t+inc;
%         end
%     end
%     
%     Kfrom=1:siz(3)-1;
%     Kto=2:siz(3);
%     inc=length(Kto);
%     for i=1:siz(1)
%         I=i*ones(size(Kfrom));
%         for j=1:siz(2)
%             J=j*ones(size(Kfrom));
%             row(t:t+inc-1)=sub2ind(siz,I,J,Kfrom);
%             col(t:t+inc-1)=sub2ind(siz,I,J,Kto);
%             t=t+inc;
%         end
%     end
%     
%     row=row(1:t-1);
%     col=col(1:t-1);
%     
%     NEIGH=sparse(row,col,ones(size(row)),prod(siz),prod(siz));
%     GCO_SetNeighbors(h,NEIGH);
%     
%     
%     GCO_Expansion(h);      % Compute optimal labeling via alpha-expansion
%     ind=reshape(GCO_GetLabeling(h),siz);
%     
%     SEG=FreeSurferLabels(ind);
%     SEG(SEG>226 & SEG<7000)=0; SEG(SEG<200)=0;  SEG(SEG==201)=0;
%     
%     data=zeros(size(inds));
%     data(cropping(1):cropping(4),cropping(2):cropping(5),cropping(3):cropping(6))=SEG;
%     aux=zeros(size(tmp2.vol)+shiftNeg);
%     aux(1+shiftNeg(1):shiftNeg(1)+size(tmp2.vol,1),1+shiftNeg(2):shiftNeg(2)+size(tmp2.vol,2),1+shiftNeg(3):shiftNeg(3)+size(tmp2.vol,3))=permute(data,[2 1 3]);
%     aux=aux(1+shiftPos(1):end,1+shiftPos(2):end,1+shiftPos(3):end);
%     tmp3.vol=aux;
%     tmp3Mask=getLargestCC(tmp3.vol>0);
%     tmp3.vol(~tmp3Mask)=0;
%     myMRIwrite(tmp3,'discreteLabels_MRF.mgz','float',tempdir);
    
end


if DEBUG>0
    % Interesting for debugging: likelihood terms
    LOGLHOODS=zeros(size(posteriorsFull),'double');
    for i=1:length(means)
        gauss=-.5*log(2*pi*variances(i))-.5*(double(imageBuffer(maskIndices))-means(i)).^2/variances(i);
        kkk=zeros(imageSize);
        kkk(maskIndices)=gauss;
        LOGLHOODS(:,:,:,i)=double(kkk);
    end
    aux=zeros([size(tmp2.vol)+shiftNeg size(LOGLHOODS,4)]);
    aux(1+shiftNeg(1):shiftNeg(1)+size(tmp2.vol,1),1+shiftNeg(2):shiftNeg(2)+size(tmp2.vol,2),1+shiftNeg(3):shiftNeg(3)+size(tmp2.vol,3),:)=permute(LOGLHOODS,[2 1 3 4]);
    aux=aux(1+shiftPos(1):end,1+shiftPos(2):end,1+shiftPos(3):end,:);
    tmp3.vol=aux;
    myMRIwrite(tmp3,'loglikelihoods.mgz','float',tempdir);
    tmp3.vol=exp(tmp3.vol);
    myMRIwrite(tmp3,'likelihoods.mgz','float',tempdir);
end

% Convert to 1 mm FreeSurfer Space 
% Eugenio November 2017: smarter/smoother resampling
if SMOOTH_LABEL_RESAMPLE>0
    refFile=[subjectDir '/' subjectName '/mri/norm.mgz'];
    applyLTAsmoothLabels('discreteLabels.mgz',[],'discreteLabelsResampledT1.mgz',refFile,0,FSpath,tempdir);
    applyLTAsmoothLabels('discreteLabelsWholeBodyHead.mgz',[],'discreteLabelsWholeBodyHeadResampledT1.mgz',refFile,0,FSpath,tempdir);
    applyLTAsmoothLabels('discreteLabelsMergedBodyHead.mgz',[],'discreteLabelsMergedBodyHeadResampledT1.mgz',refFile,0,FSpath,tempdir);
    applyLTAsmoothLabels('discreteLabelsMergedBodyHeadNoMLorGCDG.mgz',[],'discreteLabelsMergedBodyHeadNoMLorGCDGResampledT1.mgz',refFile,0,FSpath,tempdir);
else
    system([FSpath '/mri_convert  discreteLabels.mgz  discreteLabelsResampledT1.mgz -rt nearest -odt float ' ...
        ' -rl ' subjectDir '/' subjectName '/mri/norm.mgz']);
    system([FSpath '/mri_convert  discreteLabelsWholeBodyHead.mgz  discreteLabelsWholeBodyHeadResampledT1.mgz -rt nearest -odt float ' ...
        ' -rl ' subjectDir '/' subjectName '/mri/norm.mgz']);
    system([FSpath '/mri_convert  discreteLabelsMergedBodyHead.mgz  discreteLabelsMergedBodyHeadResampledT1.mgz -rt nearest -odt float ' ...
        ' -rl ' subjectDir '/' subjectName '/mri/norm.mgz']);
    system([FSpath '/mri_convert  discreteLabelsMergedBodyHeadNoMLorGCDG.mgz  discreteLabelsMergedBodyHeadNoMLorGCDGResampledT1.mgz -rt nearest -odt float ' ...
        ' -rl ' subjectDir '/' subjectName '/mri/norm.mgz']);
end

    

% Move to MRI directory
% Eugenio July 2017: add new segmentation maps, and simplified code
% (left/right)
system(['mv discreteLabels.mgz ' subjectDir '/' subjectName '/mri/' side(1) 'h.hippoAmygLabels-T1.' suffix '.mgz']);
system(['mv discreteLabelsResampledT1.mgz ' subjectDir '/' subjectName '/mri/' side(1) 'h.hippoAmygLabels-T1.' suffix '.FSvoxelSpace.mgz']);
system(['mv discreteLabelsWholeBodyHead.mgz ' subjectDir '/' subjectName '/mri/' side(1) 'h.hippoAmygLabels-T1.' suffix '.HBT.mgz']);
system(['mv discreteLabelsWholeBodyHeadResampledT1.mgz ' subjectDir '/' subjectName '/mri/' side(1) 'h.hippoAmygLabels-T1.' suffix '.HBT.FSvoxelSpace.mgz']);
system(['mv discreteLabelsMergedBodyHead.mgz ' subjectDir '/' subjectName '/mri/' side(1) 'h.hippoAmygLabels-T1.' suffix '.FS60.mgz']);
system(['mv discreteLabelsMergedBodyHeadResampledT1.mgz ' subjectDir '/' subjectName '/mri/' side(1) 'h.hippoAmygLabels-T1.' suffix '.FS60.FSvoxelSpace.mgz']);
system(['mv discreteLabelsMergedBodyHeadNoMLorGCDG.mgz ' subjectDir '/' subjectName '/mri/' side(1) 'h.hippoAmygLabels-T1.' suffix '.CA.mgz']);
system(['mv discreteLabelsMergedBodyHeadNoMLorGCDGResampledT1.mgz ' subjectDir '/' subjectName '/mri/' side(1) 'h.hippoAmygLabels-T1.' suffix '.CA.FSvoxelSpace.mgz']);

system(['mv volumesHippo.txt ' subjectDir '/' subjectName '/mri/' side(1) 'h.hippoSfVolumes-T1.' suffix '.txt']);
system(['mv volumesAmygdala.txt ' subjectDir '/' subjectName '/mri/' side(1) 'h.amygNucVolumes-T1.' suffix '.txt']);




if WRITE_POSTERIORS>0
    d=dir('posterior_*.mgz');
    for i=1:length(d)
        if isempty(strfind(d(i).name,'left-amygdala')) % Eugenio: I don't really remember why this is necessary... but whatever, it doesn't hurt anyone
            system(['mv  ' d(i).name ' ' subjectDir '/' subjectName '/mri/']);
        end
    end
end

% Eugenio November 2017
if WRITE_MESHES>0
    system(['mv warpedMesh.txt.gz   ' subjectDir '/' subjectName '/mri/' side(1) 'h.hippoAmygMesh-T1.' suffix '.txt.gz']);
    system(['mv warpedMeshNoAffine.txt.gz   ' subjectDir '/' subjectName '/mri/' side(1) 'h.hippoAmygMeshAtlasSpace-T1.' suffix '.txt.gz']);
    system(['mv image.mgz   ' subjectDir '/' subjectName '/mri/' side(1) 'h.imageForMesh-T1.' suffix '.mgz']);    
    fid=fopen([subjectDir '/' subjectName '/mri/' side(1) 'h.affineTransformMesh-T1.' suffix '.txt'],'w');
    fprintf(fid,'%f %f %f %f \n',transformMatrix(1,1),transformMatrix(1,2),transformMatrix(1,3),transformMatrix(1,4));
    fprintf(fid,'%f %f %f %f \n',transformMatrix(2,1),transformMatrix(2,2),transformMatrix(2,3),transformMatrix(2,4));  
    fprintf(fid,'%f %f %f %f \n',transformMatrix(3,1),transformMatrix(3,2),transformMatrix(3,3),transformMatrix(3,4));
    fprintf(fid,'%f %f %f %f \n',0,0,0,1);
    fclose(fid);
end



%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Clean up, I guess... you might wanna skip this with debugging purposes... %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

cmd=['rm -r -f ' tempdir];
system([cmd ' >/dev/null']);


disp('Everything done!')
disp(['It took ' num2str(etime(clock,time_start)) ' seconds ']);


if isdeployed
    exit
end
