/*****************************************************************************
 * Project: RooFit                                                           *
 * Package: RooFitModels                                                     *
 *    File: $Id: RooBifurGauss.cc,v 1.12 2003/10/16 23:27:14 wverkerke Exp $
 * Authors:                                                                  *
 *   Abi Soffer, Colorado State University, abi@slac.stanford.edu            *
 *                                                                           *
 * Copyright (c) 2000-2004, Regents of the University of California,         *
 *                          Colorado State University                        *
 *                          and Stanford University. All rights reserved.    *
 *                                                                           *
 * Redistribution and use in source and binary forms,                        *
 * with or without modification, are permitted according to the terms        *
 * listed in LICENSE (http://roofit.sourceforge.net/license.txt)             *
 *****************************************************************************/
#include <iostream.h>
#include <math.h>

#include "RooFitModels/RooBifurGauss.hh"
#include "RooFitCore/RooAbsReal.hh"

ClassImp(RooBifurGauss)

static const char rcsid[] =
"$Id: RooBifurGauss.cc,v 1.12 2003/10/16 23:27:14 wverkerke Exp $";

RooBifurGauss::RooBifurGauss(const char *name, const char *title,
			     RooAbsReal& _x, RooAbsReal& _mean,
			     RooAbsReal& _sigmaL, RooAbsReal& _sigmaR) :
  RooAbsPdf(name, title), 
  x     ("x"     , "Dependent"  , this, _x),
  mean  ("mean"  , "Mean"       , this, _mean),
  sigmaL("sigmaL", "Left Sigma" , this, _sigmaL),
  sigmaR("sigmaR", "Right Sigma", this, _sigmaR)

{
}

RooBifurGauss::RooBifurGauss(const RooBifurGauss& other, const char* name) : 
  RooAbsPdf(other,name), x("x",this,other.x), mean("mean",this,other.mean),
  sigmaL("sigmaL",this,other.sigmaL), sigmaR("sigmaR", this, other.sigmaR)
{
}

Double_t RooBifurGauss::evaluate() const {

  Double_t arg = x - mean;

  Double_t coef(0.0);

  if (arg < 0.0){
    if (fabs(sigmaL) > 1e-30) {
      coef = -0.5/(sigmaL*sigmaL);
    }
  } else {
    if (fabs(sigmaR) > 1e-30) {
      coef = -0.5/(sigmaR*sigmaR);
    }
  }
    
  return exp(coef*arg*arg);
}

Int_t RooBifurGauss::getAnalyticalIntegral(RooArgSet& allVars, RooArgSet& analVars) const 
{
  if (matchArgs(allVars,analVars,x)) return 1 ;
  return 0 ;
}

Double_t RooBifurGauss::analyticalIntegral(Int_t code) const 
{
  switch(code) {
  case 1: 
    {
      static Double_t root2 = sqrt(2.) ;
      static Double_t rootPiBy2 = sqrt(atan2(0.0,-1.0)/2.0);
      
      Double_t coefL(0.0), coefR(0.0);
      if (fabs(sigmaL) > 1e-30) {
	coefL = -0.5/(sigmaL*sigmaL);
      }

      if (fabs(sigmaR) > 1e-30) {
	coefR = -0.5/(sigmaR*sigmaR);
      }

      Double_t xscaleL = root2*sigmaL;
      Double_t xscaleR = root2*sigmaR;

      Double_t integral = 0.0;
      if(x.max() < mean)
      {
	integral = sigmaL * ( erf((x.max() - mean)/xscaleL) - erf((x.min() - mean)/xscaleL) );
      }
      else if (x.min() > mean)
      {
	integral = sigmaR * ( erf((x.max() - mean)/xscaleR) - erf((x.min() - mean)/xscaleR) );
      }
      else
      {
	integral = sigmaR*erf((x.max() - mean)/xscaleR) - sigmaL*erf((x.min() - mean)/xscaleL);
      }
      //      return rootPiBy2*(sigmaR*erf((x.max() - mean)/xscaleR) - 
      //			sigmaL*erf((x.min() - mean)/xscaleL));
      return integral*rootPiBy2;
    }
  }  

  assert(0) ; 
  return 0 ; // to prevent compiler warnings
}
