/*****************************************************************************
 * Project: RooFit                                                           *
 * Package: RooFitCore                                                       *
 * @(#)root/roofitcore:$Name:  $:$Id: RooComplex.cxx,v 1.13 2007/05/11 09:11:58 verkerke Exp $
 * Authors:                                                                  *
 *   WV, Wouter Verkerke, UC Santa Barbara, verkerke@slac.stanford.edu       *
 *   DK, David Kirkby,    UC Irvine,         dkirkby@uci.edu                 *
 *                                                                           *
 * Copyright (c) 2000-2005, Regents of the University of California          *
 *                          and Stanford University. All rights reserved.    *
 *                                                                           *
 * Redistribution and use in source and binary forms,                        *
 * with or without modification, are permitted according to the terms        *
 * listed in LICENSE (http://roofit.sourceforge.net/license.txt)             *
 *****************************************************************************/

// -- CLASS DESCRIPTION [MISC] --

#include "RooFit.h"

#include "RooComplex.h"
#include "RooComplex.h"
#include "Riostream.h"
#include <iomanip>

ClassImp(RooComplex)

void RooComplex::Print() const {
//  WVE Solaric CC5.0 complains about this
  cout << *this << endl;
}

ostream& operator<<(ostream& os, const RooComplex& z)
{
  return os << "(" << z.re() << "," << z.im() << ")";
}
