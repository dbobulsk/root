#include "Rtypes.h"
#include <iostream.h>

// -- CLASS DESCRIPTION [AUX] --
// Print banner message when RooFit library is loaded

Int_t doBanner()
{
  cout << endl
       << "\033[1mRooFit -- Developed by Wouter Verkerke and David Kirkby\033[0m " << endl 
              << "          Copyright (C) 2001-2002 University of California & Stanford University" << endl 
              << "          All rights reserved, please read http://roofit.sourceforge.net/license.txt" << endl << endl ;
  return 0 ;
}

static Int_t dummy = doBanner() ;
