/*  glimGls.cc

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

#include <sstream>

#include "glimGls.h"
#include "miscmaths/miscmaths.h"
#include "utils/log.h"
#include "miscmaths/volume.h"
#include "miscmaths/volumeseries.h"

using namespace MISCMATHS;
using namespace Utilities;

namespace FILM {

  GlimGls::GlimGls(const int pnumTS, const int psizeTS, const int pnumParams) : 
    numTS(pnumTS),
    sizeTS(psizeTS),
    numParams(pnumParams),
    corrections(numParams*numParams,numTS),
    b(numParams, numTS),
    sigmaSquareds(numTS),
    dof(sizeTS - numParams)
  {
    I.ReSize(sizeTS);
    for(int i=1; i<=sizeTS; i++)
      {
	I(i,i)=1;
      }
  }
  
  void GlimGls::CleanUp()
  {
    corrections.CleanUp();
    sigmaSquareds.CleanUp();
    b.CleanUp();
    r.CleanUp();
  }

  void GlimGls::setData(const ColumnVector& y, const Matrix& x, const int ind)
    {
      // compute b
      //Matrix inv_xx = (x.t()*x).i();
      Matrix inv_xx = pinv(x.t()*x);
      b.Column(ind) = inv_xx*x.t()*y;

      // compute r
      Matrix R = I-x*inv_xx*x.t();
      r = R*y;

      // compute sigma squareds 
      //      sigmaSquareds(ind) = (r.t()*r/sizeTS).AsScalar();
      sigmaSquareds(ind) = (r.t()*r).AsScalar()/R.Trace();

      // set corrections
      SetCorrection(inv_xx, ind);
    }

  void GlimGls::Save(const VolumeInfo& pvolinfo, const ColumnVector& prethreshpos)
    {
      // Need to save b, sigmaSquareds, corrections and dof 
      Log& logger = LogSingleton::getInstance();

      VolumeInfo volinfo = pvolinfo;

      // b:
      Volume peVol;
      for(int i = 1; i <= numParams; i++)
	{
	  peVol = b.Row(i).AsColumn();
	  volinfo.intent_code = NIFTI_INTENT_ESTIMATE;
	  peVol.setInfo(volinfo);
	  peVol.setPreThresholdPositions(prethreshpos);
	  peVol.unthreshold();
	  
	  // Add param number to "pe" to create filename:
	  ostringstream osc;
	  osc << i;
	  
	  peVol.writeAsFloat(logger.getDir() + "/pe" + osc.str().c_str());
	}

      // sigmaSquareds:
      volinfo.intent_code = NIFTI_INTENT_ESTIMATE;
      sigmaSquareds.setInfo(volinfo);
      sigmaSquareds.setPreThresholdPositions(prethreshpos);
      sigmaSquareds.unthreshold();	
      sigmaSquareds.writeAsFloat(logger.getDir() + "/sigmasquareds");

      // dof:
      ColumnVector dofVec(1);
      dofVec = dof;
      write_ascii_matrix(logger.appendDir("dof"), dofVec);
      
      // corrections (are the pes correlation matrix (x.t()*x).i() reshapen to a vector):
      VolumeInfo newvolinfo = volinfo;
      newvolinfo.v = numParams*numParams;
      newvolinfo.intent_code = NIFTI_INTENT_NONE;
      //corrections.setInfo(newvolinfo);
      //corrections.setPreThresholdPositions(prethreshpos);
      //corrections.unthresholdSeries();
      //corrections.writeAsFloat(logger.getDir() + "/corrections");
      corrections.writeThresholdedSeriesAsFloat(newvolinfo,prethreshpos,logger.getDir() + "/corrections");
    }

  void GlimGls::SetCorrection(const Matrix& corr, const int ind)
    {
      Tracer ts("SetCorrection");

      // puts Matrix corr which is p*p into Matrix correction
      // as a p*p length row:
      int p = corr.Nrows();
      
      for (int i = 1; i <= p; i++)
	{
	  for (int j = 1; j <= p; j++)
	    {
	      corrections((i-1)*p + j, ind) = corr(i,j); 
	    }
	}
    }

}







