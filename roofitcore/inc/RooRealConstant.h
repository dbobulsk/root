/*****************************************************************************
 * Project: BaBar detector at the SLAC PEP-II B-factory
 * Package: RooFitCore
 *    File: $Id$
 * Authors:
 *   WV, Wouter Verkerke, UC Santa Barbara, verkerke@slac.stanford.edu
 *   DK, David Kirkby, Stanford University, kirkby@hep.stanford.edu
 * History:
 *   16-Aug-2001 WV Created initial version
 *
 * Copyright (C) 2001 University of California
 *****************************************************************************/
#ifndef ROO_REAL_CONSTANT
#define ROO_REAL_CONSTANT

#include "Rtypes.h"

class RooRealVar ;
class RooArgList ;
class TIterator ;

class RooRealConstant {
public:

  static const RooRealVar& value(Double_t value) ;

protected:

  static void init() ;

  static RooArgList* _constDB ;
  static TIterator* _constDBIter ;

  ClassDef(RooRealConstant,0) // RooRealVar constants factory
};

#endif
