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
#ifndef ROO_DATA_HIST
#define ROO_DATA_HIST

#include "TObject.h"
#include "RooFitCore/RooTreeData.hh"
#include "RooFitCore/RooArgSet.hh"

class RooAbsArg;
class RooAbsReal ;
class RooAbsCategory ;
class Roo1DTable ;
class RooPlot;
class RooFitContext ;
class RooHistArray ;

class RooDataHist : public RooTreeData {
public:

  // Constructors, factory methods etc.
  RooDataHist() ; 
  RooDataHist(const char *name, const char *title, const RooArgSet& vars) ;
  RooDataHist(const RooDataHist& other, const char* newname = 0) ;
  virtual TObject* Clone(const char* newname=0) const { return new RooDataHist(*this,newname?newname:GetName()) ; }
  virtual ~RooDataHist() ;

  // Add one ore more rows of data
  virtual void add(const RooArgSet& row, Double_t weight=1) ;
  virtual const RooArgSet* RooDataHist::get(Int_t masterIdx) const ;

  virtual Double_t weight() const { return _curWeight ; }

  virtual void reset() ;

  // Plot the distribution of a real valued arg
  virtual Roo1DTable* table(RooAbsCategory& cat, const char* cuts="", const char* opts="") const ;
  virtual RooPlot *plotOn(RooPlot *frame, const char* cuts="", Option_t* drawOptions="P") const ;

 
protected:
 
  RooDataHist(const char* name, const char* title, RooDataHist* h, const RooArgSet& varSubset, 
	      const RooFormulaVar* cutVar, Bool_t copyCache) ;
  virtual RooAbsData* reduceEng(const RooArgSet& varSubset, const RooFormulaVar* cutVar, Bool_t copyCache=kTRUE) ;


  Int_t calcTreeIndex() const ;

  Int_t     _arrSize ;
  Int_t*    _idxMult ; //! do not persist
  Double_t*     _wgt ; //[_arrSize]  Weight array

  mutable Double_t _curWeight ;

private:

  ClassDef(RooDataHist,1) // Binned data set
};

#endif

