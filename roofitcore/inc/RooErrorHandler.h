/*****************************************************************************
 * Project: BaBar detector at the SLAC PEP-II B-factory
 * Package: RooFitCore
 *    File: $Id$
 * Authors:
 *   WV, Wouter Verkerke, UC Santa Barbara, verkerke@slac.stanford.edu
 * History:
 *   18-Oct-2001 WV Created initial version
 *
 * Copyright (C) 2001 University of California
 *****************************************************************************/
#ifndef ROO_ERROR_HANDLER
#define ROO_ERROR_HANDLER

#include <signal.h>
#include "Rtypes.h"

class RooErrorHandler
{
public:
  // Soft assert function that interrupts macro execution but doesn't kill ROOT
  static void softAssert(Bool_t condition) { if (!condition) abort() ; }

  // Soft abort function that interrupts macro execution but doesn't kill ROOT
  static void softAbort() { raise(11) ; }
} ;
 
#endif
