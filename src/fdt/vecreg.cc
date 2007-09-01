/*  vector_flirt.cc

    Saad Jbabdi, FMRIB Image Analysis Group

    Copyright (C) 2007 University of Oxford */

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

#include "vecreg.h"
#include "utils/options.h"

using namespace Utilities;

string title="vecreg (Version 1.0)\nVector Affine/non linear Tranformation with Orientation Preservation";
string examples="vecreg -i <input4Dvector> -o <output4D> -t <transformation>";

Option<bool> verbose(string("-v,--verbose"),false,
		       string("switch on diagnostic messages"),
		       false,no_argument);
Option<bool> help(string("-h,--help"),false,
		       string("display this message"),
		       false,no_argument);
Option<string> ivector(string("-i,--input"),string(""),
		       string("filename of input vector"),
		       true,requires_argument);
Option<string> ovector(string("-o,--output"),string(""),
		       string("filename of output registered vector"),
		       true,requires_argument);
Option<string> ref(string("-r,--ref"),string(""),
		       string("filename of reference (target) volume"),
		       true,requires_argument);
Option<string> matrix(string("-t,--affine"),string(""),
		       string("filename of affine transformation matrix"),
		       false,requires_argument);
Option<string> warp(string("-w,--warpfield"),string(""),
		       string("filename of 4D warp field for nonlinear registration"),
		       false,requires_argument);
Option<string> interpmethod(string("--interp"),string("nearestneighbour"),
		       string("interpolation method : nearestneighbour (default) or trilinear or sinc"),
		       false,requires_argument);
Option<string> maskfile(string("-m,--mask"),string(""),
		       string("brain mask in input space"),
		       false,requires_argument);
////////////////////////////////////////////////////////


void vecreg_aff(const volume4D<float>& tens,
		volume4D<float>& oV1,
		const volume<float>& refvol,
		const Matrix& M,
		const volume<float>& mask){

  Matrix iM(4,4),R(3,3);
  iM=M.i();
  // extract rotation matrix from M
  Matrix F(3,3),u(3,3),v(3,3);
  DiagonalMatrix d(3);
  F=M.SubMatrix(1,3,1,3);
  SVD(F*F.t(),d,u,v);
  R=(u*sqrt(d)*v.t()).i()*F;

  ColumnVector seeddim(3),targetdim(3);
  seeddim << tens.xdim() << tens.ydim() << tens.zdim();
  targetdim  << refvol.xdim() << refvol.ydim() << refvol.zdim();
  SymmetricMatrix Tens(3);
  ColumnVector X_seed(3),X_target(3); 
  ColumnVector V_seed(3),V_target(3);
  for(int z=0;z<oV1.zsize();z++)
    for(int y=0;y<oV1.ysize();y++)
      for(int x=0;x<oV1.xsize();x++){

	// compute seed coordinates
	X_target << x << y << z;
	X_seed=vox_to_vox(X_target,targetdim,seeddim,iM);
	
	if(mask((int)X_seed(1),(int)X_seed(2),(int)X_seed(3))==0){
	  continue;
	}

	// compute interpolated tensor
	Tens << tens[0].interpolate(X_seed(1),X_seed(2),X_seed(3))
	     << tens[1].interpolate(X_seed(1),X_seed(2),X_seed(3))
	     << tens[2].interpolate(X_seed(1),X_seed(2),X_seed(3))
	     << tens[3].interpolate(X_seed(1),X_seed(2),X_seed(3))
	     << tens[4].interpolate(X_seed(1),X_seed(2),X_seed(3))
	     << tens[5].interpolate(X_seed(1),X_seed(2),X_seed(3));
  
	// compute first eigenvector
	EigenValues(Tens,d,v);
	V_seed = v.Column(3);

	// rotate vector
	V_target=R*V_seed;
	V_target/=sqrt(V_target.SumSquare());
	oV1(x,y,z,0)=V_target(1);
	oV1(x,y,z,1)=V_target(2);
	oV1(x,y,z,2)=V_target(3);
      }
  
}
void sjgradient(const volume<float>& im,volume4D<float>& grad){
  
  grad.reinitialize(im.xsize(),im.ysize(),im.zsize(),3);
  copybasicproperties(im,grad[0]);

  int fx,fy,fz,bx,by,bz;
  float dx,dy,dz; 
  for (int z=0; z<grad.zsize(); z++){
    fz = z ==(grad.zsize()-1) ? 0 :  1;
    bz = z == 0              ? 0 : -1;
    dz = (fz==0 || bz==0)    ? 1.0 :  2.0;
    for (int y=0; y<grad.ysize(); y++){
      fy = y ==(grad.ysize()-1) ? 0 :  1;
      by = y == 0              ? 0 : -1;
      dy = (fy==0 || by==0)    ? 1.0 :  2.0;
      for (int x=0; x<grad.xsize(); x++){
	fx = x ==(grad.xsize()-1) ? 0 :  1;
	bx = x == 0              ? 0 : -1;
	dx = (fx==0 || bx==0)    ? 1.0 :  2.0;
	grad[0](x,y,z) = (im(x+fx,y,z) - im(x+bx,y,z))/dx;
	grad[1](x,y,z) = (im(x,y+fy,z) - im(x,y+by,z))/dy;
	grad[2](x,y,z) = (im(x,y,z+fz) - im(x,y,z+bz))/dz;
      }
    }
  }

}


void vecreg_nonlin(const volume4D<float>& tens,volume4D<float>& oV1,
		   const volume<float>& refvol,volume4D<float>& warp,
		   const volume<float>& mask){

  ColumnVector X_seed(3),X_target(3);
  
  //float dxx=tens.xdim(),dyy=tens.ydim(),dzz=tens.zdim();
  float dx=oV1.xdim(),dy=oV1.ydim(),dz=oV1.zdim();
  //float nxx=(float)tens.xsize()/2.0,nyy=(float)tens.ysize()/2.0,nzz=(float)tens.zsize()/2.0;
  //float nx=(float)oV1.xsize()/2.0,ny=(float)oV1.ysize()/2.0,nz=(float)oV1.zsize()/2.0;

  // transform mm warp to voxel warp
  // the warpfield here has been transfomed by MJ's script
  for(int z=0;z<warp[0].zsize();z++)
    for(int y=0;y<warp[0].ysize();y++)
      for(int x=0;x<warp[0].xsize();x++){
	warp[0](x,y,z) /= dx;
	warp[1](x,y,z) /= dy;
	warp[2](x,y,z) /= dz;
      }
 

  // compute transformation jacobian
  volume4D<float> jx(mask.xsize(),mask.ysize(),mask.zsize(),3);
  volume4D<float> jy(mask.xsize(),mask.ysize(),mask.zsize(),3);
  volume4D<float> jz(mask.xsize(),mask.ysize(),mask.zsize(),3);
  sjgradient(warp[0],jx);
  sjgradient(warp[1],jy);
  sjgradient(warp[2],jz);


  ColumnVector V_seed(3),V_target(3);
  Matrix R(3,3),I(3,3);I<<1<<0<<0<<0<<1<<0<<0<<0<<1;
  Matrix F(3,3),Jw(3,3),u(3,3),v(3,3);
  DiagonalMatrix d(3);  
  SymmetricMatrix Tens(3);
  for(int z=0;z<oV1.zsize();z++)
    for(int y=0;y<oV1.ysize();y++)
       for(int x=0;x<oV1.xsize();x++){

	 X_seed << round(x+warp[0](x,y,z))
		<< round(y+warp[1](x,y,z))
		<< round(z+warp[2](x,y,z));
	 if(mask((int)X_seed(1),(int)X_seed(2),(int)X_seed(3))==0){
	   continue;
	 }
	
	 // compute interpolated tensor
	 Tens << tens[0].interpolate(X_seed(1),X_seed(2),X_seed(3))
	      << tens[1].interpolate(X_seed(1),X_seed(2),X_seed(3))
	      << tens[2].interpolate(X_seed(1),X_seed(2),X_seed(3))
	      << tens[3].interpolate(X_seed(1),X_seed(2),X_seed(3))
	      << tens[4].interpolate(X_seed(1),X_seed(2),X_seed(3))
	      << tens[5].interpolate(X_seed(1),X_seed(2),X_seed(3));
	 // compute first eigenvector
	 EigenValues(Tens,d,v);
	 V_seed = v.Column(3);

	 // Local Jacobian of the backward warpfield
	 Jw <<   jx(x,y,z,0) <<  jx(x,y,z,1) << jx(x,y,z,2)
	    <<   jy(x,y,z,0) <<  jy(x,y,z,1) << jy(x,y,z,2)
	    <<   jz(x,y,z,0) <<  jz(x,y,z,1) << jz(x,y,z,2);
	 
	 // compute local forward affine transformation	
	 F = (I + Jw).i();
	 
	 // reorient according to affine reorientation scheme
	 SVD(F*F.t(),d,u,v);
	 R=(u*sqrt(d)*v.t()).i()*F;

	 //R=ppd(F,V_seed);
	 
	 V_target=R*V_seed;
	 V_target/=sqrt(V_target.SumSquare());
	 
	 oV1(x,y,z,0)=V_target(1);
	 oV1(x,y,z,1)=V_target(2);
	 oV1(x,y,z,2)=V_target(3);
       }
  
  
}



int do_vecreg(){
  volume4D<float> ivol,warpvol;
  volume<float> refvol,mask;
  Matrix Aff(4,4);
  volumeinfo vinfo;

  if((matrix.set())){
    read_ascii_matrix(Aff,matrix.value());
  }
  if((warp.set())){
    if(verbose.value()) cerr << "Loading warpfield" << endl;
    read_volume4D(warpvol,warp.value());
  }
  if(verbose.value()) cerr << "Loading volumes" << endl;
  read_volume4D(ivol,ivector.value());
  read_volume(refvol,ref.value(),vinfo);

  
  volume4D<float> ovol(refvol.xsize(),refvol.ysize(),refvol.zsize(),3);
  copybasicproperties(refvol,ovol);

  // set interpolation method
  if(interpmethod.value()=="nearestneighbour")
    ivol.setinterpolationmethod(nearestneighbour);
  else if(interpmethod.value()=="sinc")
    ivol.setinterpolationmethod(sinc);
  else
    ivol.setinterpolationmethod(trilinear);

  if(maskfile.value()!="")
    read_volume(mask,maskfile.value());
  else{
    mask.reinitialize(ivol[0].xsize(),ivol[0].ysize(),ivol[0].zsize());
    copybasicproperties(ivol,mask);
    for(int z=0;z<mask.zsize();z++)
      for(int y=0;y<mask.ysize();y++)
	for(int x=0;x<mask.xsize();x++){
	  if(ivol(x,y,z,0)*ivol(x,y,z,0)
	     +ivol(x,y,z,1)*ivol(x,y,z,1)
	     +ivol(x,y,z,2)*ivol(x,y,z,2) != 0)
	    mask(x,y,z) = 1;
	  else
	    mask(x,y,z) = 0;
	}
  }

  ///////////////////////
  // tensor for interpolation
  volume4D<float> tens(ivol.xsize(),ivol.ysize(),ivol.zsize(),6);
  copybasicproperties(ivol,tens);
  for(int z=0;z<ivol.zsize();z++) 
    for(int y=0;y<ivol.ysize();y++)  
      for(int x=0;x<ivol.xsize();x++){
	tens(x,y,z,0)=ivol(x,y,z,0)*ivol(x,y,z,0);
	tens(x,y,z,1)=ivol(x,y,z,1)*ivol(x,y,z,0);
	tens(x,y,z,2)=ivol(x,y,z,1)*ivol(x,y,z,1);
	tens(x,y,z,3)=ivol(x,y,z,2)*ivol(x,y,z,0);
	tens(x,y,z,4)=ivol(x,y,z,2)*ivol(x,y,z,1);
	tens(x,y,z,5)=ivol(x,y,z,2)*ivol(x,y,z,2);
      }

  //time_t _time=time(NULL);
  if(matrix.set()){
    if(verbose.value()) cerr << "Affine registration" << endl;
    vecreg_aff(tens,ovol,refvol,Aff,mask);
  }
  else{
    if(verbose.value()) cerr << "Nonlinear registration" << endl;
    vecreg_nonlin(tens,ovol,refvol,warpvol,mask);
  }
  //cout<<"elapsed time:"<<time(NULL)-_time<<" sec"<<endl;

  save_volume4D(ovol,ovector.value());

  return 0;

}


int main(int argc,char *argv[]){

  Tracer tr("main");
  OptionParser options(title,examples);

  try{
    options.add(verbose);
    options.add(help);
    options.add(ivector);
    options.add(ovector);
    options.add(ref);
    options.add(matrix);
    options.add(warp);
    options.add(interpmethod);
    options.add(maskfile);

    options.parse_command_line(argc,argv);

    
    if ( (help.value()) || (!options.check_compulsory_arguments(true)) ){
      options.usage();
      exit(EXIT_FAILURE);
    }
    if( (matrix.set()) && (warp.set()) ){
      options.usage();
      cerr << endl
	   << "Cannot specify both --affine AND --warpfield"
	   << endl << endl;
      exit(EXIT_FAILURE);
    }
    if( (matrix.unset()) && (warp.unset()) ){
      options.usage();
      cerr << endl
	   << "Please Specify either --affine OR --warpfield"
	   << endl << endl;
      exit(EXIT_FAILURE);
    }
  }
  catch(X_OptionError& e) {
    options.usage();
    cerr << endl << e.what() << endl;
    exit(EXIT_FAILURE);
  } 
  catch(std::exception &e) {
    cerr << e.what() << endl;
  } 
  
  return do_vecreg();
  
  
}
