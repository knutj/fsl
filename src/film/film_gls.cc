/*  film_gls.cc

    Mark Woolrich, FMRIB Image Analysis Group

    Copyright (C) 1999-2000 University of Oxford  */

/*  Part of FSL - FMRIB's Software Library
    http://www.fmrib.ox.ac.uk/fsl
    fsl@fmrib.ox.ac.uk
    
    Developed at FMRIB (Oxford Centre for Functional Magnetic Resonance
    Imaging of the Brain), Department of Clinical Neurology, Oxford
    University, Oxford, UK
    
    
    LICENCE
    
    FMRIB Software Library, Release 4.0 (c) 2007, The University of
    Oxford (the "Software")
    
    The Software remains the property of the University of Oxford ("the
    University").
    
    The Software is distributed "AS IS" under this Licence solely for
    non-commercial use in the hope that it will be useful, but in order
    that the University as a charitable foundation protects its assets for
    the benefit of its educational and research purposes, the University
    makes clear that no condition is made or to be implied, nor is any
    warranty given or to be implied, as to the accuracy of the Software,
    or that it will be suitable for any particular purpose or for use
    under any specific conditions. Furthermore, the University disclaims
    all responsibility for the use which is made of the Software. It
    further disclaims any liability for the outcomes arising from using
    the Software.
    
    The Licensee agrees to indemnify the University and hold the
    University harmless from and against any and all claims, damages and
    liabilities asserted by third parties (including claims for
    negligence) which arise directly or indirectly from the use of the
    Software or the sale of any products based on the Software.
    
    No part of the Software may be reproduced, modified, transmitted or
    transferred in any form or by any means, electronic or mechanical,
    without the express permission of the University. The permission of
    the University is not required if the said reproduction, modification,
    transmission or transference is done without financial return, the
    conditions of this Licence are imposed upon the receiver of the
    product, and all original and amended source code is included in any
    transmitted product. You may be held legally responsible for any
    copyright infringement that is caused or encouraged by your failure to
    abide by these terms and conditions.
    
    You are not permitted under this Licence to use this Software
    commercially. Use for which any financial return is received shall be
    defined as commercial use, and includes (1) integration of all or part
    of the source code or the Software into a product for sale or license
    by or on behalf of Licensee to third parties or (2) use of the
    Software or any derivative of it for research with the final aim of
    developing software products for sale or license to a third party or
    (3) use of the Software or any derivative of it for research with the
    final aim of developing non-software products for sale or license to a
    third party, or (4) use of the Software to provide any service to an
    external organisation for which payment is received. If you are
    interested in using the Software commercially, please contact Isis
    Innovation Limited ("Isis"), the technology transfer company of the
    University, to negotiate a licence. Contact details are:
    innovation@isis.ox.ac.uk quoting reference DE/1112. */

#include <iostream>
#include <fstream>
#include <sstream>
#define WANT_STREAM
#define WANT_MATH

#include "newmatap.h"
#include "newmatio.h"
#include "miscmaths/volumeseries.h"
#include "miscmaths/volume.h"
#include "miscmaths/miscmaths.h"
#include "utils/log.h"
#include "AutoCorrEstimator.h"
#include "paradigm.h"
#include "FilmGlsOptions.h"
#include "glimGls.h"
#include <vector>
#include <string>

using namespace NEWMAT;
using namespace FILM;
using namespace Utilities;
using namespace MISCMATHS;

int main(int argc, char *argv[])
{
  try{
    
    // Setup logging:
    Log& logger = LogSingleton::getInstance();

    // parse command line
    FilmGlsOptions& globalopts = FilmGlsOptions::getInstance();
    globalopts.parse_command_line(argc, argv, logger);

    // load data
    VolumeSeries x;
    x.read(globalopts.inputfname);

    int sizeTS = x.getNumVolumes();

    // if needed for SUSAN spatial smoothing, output the halfway volume for use later
    Volume epivol;
    if(globalopts.smoothACEst)
      {
	epivol = x.getVolume(int(sizeTS/2)).AsColumn();
	epivol.setInfo(x.getInfo());
	epivol.writeAsInt(logger.getDir() + "/" + globalopts.epifname);
      }

    // This also removes the mean from each of the time series:
    x.thresholdSeries(globalopts.thresh, true);

    // if needed for SUSAN spatial smoothing, also threshold the epi volume
    if(globalopts.smoothACEst)
      {
	epivol.setPreThresholdPositions(x.getPreThresholdPositions());
	epivol.threshold();
      }
    
    int numTS = x.getNumSeries();

    // Load paradigm:
    Paradigm parad;
    if(!globalopts.ac_only)
      {	
	parad.load(globalopts.paradigmfname, "", "", false, sizeTS);
      }
    else
      {
	// set design matrix to be one ev with all ones:
	Matrix mat(sizeTS,1);
	mat = 1;
	parad.setDesignMatrix(mat);
      }

    if(globalopts.verbose)
      {
	write_vest(logger.appendDir("Gc"), parad.getDesignMatrix());
      }
    

    OUT(parad.getDesignMatrix().Nrows());
    OUT(parad.getDesignMatrix().Ncols());
    OUT(sizeTS);
    OUT(numTS);

    // Setup GLM:
    int numParams = parad.getDesignMatrix().Ncols();
    GlimGls glimGls(numTS, sizeTS, numParams);

    // Residuals container:
    VolumeSeries residuals(sizeTS, numTS, x.getInfo(), x.getPreThresholdPositions());

    // Setup autocorrelation estimator:
    AutoCorrEstimator acEst(residuals);

    if(!globalopts.noest)
      {
	cout << "Calculating residuals..." << endl; 
	for(int i = 1; i <= numTS; i++)
	  {						    
	    glimGls.setData(x.getSeries(i), parad.getDesignMatrix(), i);
	    residuals.setSeries(glimGls.getResiduals(),i);
	  }
	cout << "Completed" << endl; 
	
	cout << "Estimating residual autocorrelation..." << endl; 
		
	if(globalopts.fitAutoRegressiveModel)
	  {
	    acEst.fitAutoRegressiveModel();
	  }
	else if(globalopts.tukey)
	  {    
	    if(globalopts.tukeysize == 0)
	      globalopts.tukeysize = (int)(2*sqrt(sizeTS))/2;

	    acEst.calcRaw();

	    // SUSAN smooth raw estimates:
	    if(globalopts.smoothACEst)
	      {
		acEst.spatiallySmooth(logger.getDir() + "/" + globalopts.epifname, epivol, globalopts.ms, globalopts.epifname, globalopts.susanpath, globalopts.epith, globalopts.tukeysize);		    
	      }	    
		
	    acEst.tukey(globalopts.tukeysize);
	  }
	else if(globalopts.multitaper)
	  {
	    acEst.calcRaw();
	    acEst.multitaper(int(globalopts.multitapersize));
	  }
	else if(globalopts.pava)
	  {
	    acEst.calcRaw();

	    // Smooth raw estimates:
	    if(globalopts.smoothACEst)
	      { 
		acEst.spatiallySmooth(logger.getDir() + "/" + globalopts.epifname, epivol, globalopts.ms, globalopts.epifname, globalopts.susanpath, globalopts.epith);		    
	      }
	    
	    acEst.pava();
	  }
	    
      }
    cout << "Completed" << endl; 

    cout << "Prewhitening and Computing PEs..." << endl;
    cout << "Percentage done:" << endl;
    int co = 1;

    Matrix mean_prewhitened_dm(parad.getDesignMatrix().Nrows(),parad.getDesignMatrix().Ncols());
    mean_prewhitened_dm=0;

    for(int i = 1; i <= numTS; i++)
      {						
	ColumnVector xw(sizeTS);
	ColumnVector xprew(sizeTS);
	   
	if(!globalopts.noest)
	  {
	    acEst.setDesignMatrix(parad.getDesignMatrix());
	    
	    // Use autocorr estimate to prewhiten data:
	    xprew = x.getSeries(i);	
	    Matrix designmattw;
	    acEst.preWhiten(xprew, xw, i, designmattw);
	    
	    if ( (100.0*i)/numTS > co )
	      {
		cout << co << ",";
		cout.flush();
		co++;
	      }
	    
	    x.setSeries(xw,i);
	    
	    glimGls.setData(x.getSeries(i), designmattw, i);

	    if(globalopts.output_pwdata || globalopts.verbose)
	      {
		mean_prewhitened_dm=mean_prewhitened_dm+designmattw;		
	      }
	  }
	else
	  {
	    if ( (100.0*i)/numTS > co )
	      {
		cout << co << ",";
		cout.flush();
		co++;
	      }
	    
	    glimGls.setData(x.getSeries(i), parad.getDesignMatrix(), i);
	    residuals.setSeries(glimGls.getResiduals(),i);

	    if(globalopts.output_pwdata || globalopts.verbose)
	      {
		mean_prewhitened_dm=mean_prewhitened_dm+parad.getDesignMatrix();		
	      }
	  }
      }

    if(globalopts.output_pwdata || globalopts.verbose)
      {
	mean_prewhitened_dm=mean_prewhitened_dm/numTS;
      }

    cerr << "Completed" << endl;

    cerr << "Saving results... " << endl;

    // write residuals
    //residuals.unthresholdSeries(x.getInfo(),x.getPreThresholdPositions());
    //residuals.writeAsFloat(logger.getDir() + "/res4d");
    residuals.writeThresholdedSeriesAsFloat(x.getInfo(),x.getPreThresholdPositions(),logger.getDir() + "/res4d");
    residuals.CleanUp();

    if(globalopts.output_pwdata || globalopts.verbose)
      {
	// Write out whitened data
	x.writeThresholdedSeriesAsFloat(x.getInfo(),x.getPreThresholdPositions(),logger.getDir() + "/prewhitened_data");

	// Write out whitened design matrix
	write_vest(logger.appendDir("mean_prewhitened_dm.mat"), mean_prewhitened_dm);
		
      }

    x.CleanUp();

    // Write out threshac:
    VolumeSeries& threshac = acEst.getEstimates();
    int cutoff = sizeTS/2;
    if(globalopts.tukey)
      cutoff = globalopts.tukeysize;
    cutoff = MISCMATHS::Max(1,cutoff);
    threshac = threshac.Rows(1,cutoff);
    VolumeInfo volinfo = x.getInfo();
    volinfo.v = cutoff;
    volinfo.intent_code = NIFTI_INTENT_ESTIMATE;
    threshac.unthresholdSeries(volinfo,x.getPreThresholdPositions());
    threshac.writeAsFloat(logger.getDir() + "/threshac1");
    threshac.thresholdSeries();
    threshac.CleanUp();

    // save gls results:
    glimGls.Save(x.getInfo(), x.getPreThresholdPositions());
    glimGls.CleanUp();

    cerr << "Completed" << endl;
  }  
  catch(Exception p_excp) 
    {
      cerr << p_excp.what() << endl;
    }
  catch(...)
    {
      cerr << "Uncaught exception!" << endl;
    }

  return 0;
}

