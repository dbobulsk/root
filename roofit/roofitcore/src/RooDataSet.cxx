/*****************************************************************************
 * Project: RooFit                                                           *
 * Package: RooFitCore                                                       *
 * @(#)root/roofitcore:$Id$
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

//////////////////////////////////////////////////////////////////////////////
// 
// BEGIN_HTML
// RooDataSet is a container class to hold unbinned data. Each data point
// in N-dimensional space is represented by a RooArgSet of RooRealVar, RooCategory 
// or RooStringVar objects 
// END_HTML
//

#include "RooFit.h"

#include "Riostream.h"
#include "Riostream.h"
#include <fstream>
#include "TTree.h"
#include "TH2.h"
#include "TDirectory.h"
#include "RooDataSet.h"
#include "RooPlot.h"
#include "RooAbsReal.h"
#include "Roo1DTable.h"
#include "RooCategory.h"
#include "RooFormulaVar.h"
#include "RooArgList.h"
#include "RooAbsRealLValue.h"
#include "RooRealVar.h"
#include "RooDataHist.h"
#include "RooMsgService.h"
#include "RooCmdConfig.h"
#include "RooHist.h"
#include "TROOT.h"
#include "TFile.h"
#include "RooTreeDataStore.h"
#include "RooVectorDataStore.h"
#include "RooCompositeDataStore.h"
#include "RooTreeData.h"
#include "RooSentinel.h"

#if (__GNUC__==3&&__GNUC_MINOR__==2&&__GNUC_PATCHLEVEL__==3)
char* operator+( streampos&, char* );
#endif

ClassImp(RooDataSet)
;


char* RooDataSet::_poolBegin = 0 ;
char* RooDataSet::_poolCur = 0 ;
char* RooDataSet::_poolEnd = 0 ;
#define POOLSIZE 1048576

struct POOLDATA 
{
  void* _base ;
} ;

static std::list<POOLDATA> _memPoolList ;

//_____________________________________________________________________________
void RooDataSet::cleanup()
{
  // Clear memoery pool on exit to avoid reported memory leaks

  std::list<POOLDATA>::iterator iter = _memPoolList.begin() ;
  while(iter!=_memPoolList.end()) {
    free(iter->_base) ;
    iter->_base=0 ;
    iter++ ;
  }
  _memPoolList.clear() ;
}


#ifdef USEMEMPOOL

//_____________________________________________________________________________
void* RooDataSet::operator new (size_t bytes)
{
  // Overloaded new operator guarantees that all RooDataSets allocated with new
  // have a unique address, a property that is exploited in several places
  // in roofit to quickly index contents on normalization set pointers. 
  // The memory pool only allocates space for the class itself. The elements
  // stored in the set are stored outside the pool.

  //cout << " RooDataSet::operator new(" << bytes << ")" << endl ;

  if (!_poolBegin || _poolCur+(sizeof(RooDataSet)) >= _poolEnd) {

    if (_poolBegin!=0) {
      oocxcoutD((TObject*)0,Caching) << "RooDataSet::operator new(), starting new 1MB memory pool" << endl ;
    }

    // Start pruning empty memory pools if number exceeds 3
    if (_memPoolList.size()>3) {
      
      void* toFree(0) ;

      for (std::list<POOLDATA>::iterator poolIter =  _memPoolList.begin() ; poolIter!=_memPoolList.end() ; ++poolIter) {

	// If pool is empty, delete it and remove it from list
	if ((*(Int_t*)(poolIter->_base))==0) {
	  oocxcoutD((TObject*)0,Caching) << "RooDataSet::operator new(), pruning empty memory pool " << (void*)(poolIter->_base) << endl ;

	  toFree = poolIter->_base ;
	  _memPoolList.erase(poolIter) ;
	  break ;
	}
      }      

      free(toFree) ;      
    }
    
    void* mem = malloc(POOLSIZE) ;

    _poolBegin = (char*)mem ;
    // Reserve space for pool counter at head of pool
    _poolCur = _poolBegin+sizeof(Int_t) ;
    _poolEnd = _poolBegin+(POOLSIZE) ;

    // Clear pool counter
    *((Int_t*)_poolBegin)=0 ;
    
    POOLDATA p ;
    p._base=mem ;
    _memPoolList.push_back(p) ;

    RooSentinel::activate() ;
  }

  char* ptr = _poolCur ;
  _poolCur += bytes ;

  // Increment use counter of pool
  (*((Int_t*)_poolBegin))++ ;

  return ptr ;

}



//_____________________________________________________________________________
void RooDataSet::operator delete (void* ptr)
{
  // Memory is owned by pool, we need to do nothing to release it

  // Decrease use count in pool that ptr is on
  for (std::list<POOLDATA>::iterator poolIter =  _memPoolList.begin() ; poolIter!=_memPoolList.end() ; ++poolIter) {
    if ((char*)ptr > (char*)poolIter->_base && (char*)ptr < (char*)poolIter->_base + POOLSIZE) {
      (*(Int_t*)(poolIter->_base))-- ;
      break ;
    }
  }
  
}

#endif


//_____________________________________________________________________________
RooDataSet::RooDataSet() : _wgtVar(0) 
{
  // Default constructor for persistence
}





//_____________________________________________________________________________
RooDataSet::RooDataSet(const char* name, const char* title, const RooArgSet& vars, const RooCmdArg& arg1, const RooCmdArg& arg2, const RooCmdArg& arg3,
		       const RooCmdArg& arg4,const RooCmdArg& arg5,const RooCmdArg& arg6,const RooCmdArg& arg7,const RooCmdArg& arg8)  :
  RooAbsData(name,title,RooArgSet(vars,(RooAbsArg*)RooCmdConfig::decodeObjOnTheFly("RooDataSet::RooDataSet", "IndexCat",0,0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8)))
{
  // Construct an unbinned dataset from a RooArgSet defining the dimensions of the data space. Optionally, data
  // can be imported at the time of construction.
  //
  // This constructor takes the following optional arguments
  //
  // Import(TTree*)              -- Import contents of given TTree. Only braches of the TTree that have names
  //                                corresponding to those of the RooAbsArgs that define the RooDataSet are
  //                                imported. 
  // ImportFromFile(const char* fileName, const char* treeName) -- Import tree with given name from file with given name.
  //
  // Import(RooDataSet&)         -- Import contents of given RooDataSet. Only observables that are common with
  //                                the definition of this dataset will be imported
  //
  // Index(RooCategory&)         -- Prepare import of datasets into a N+1 dimensional RooDataSet
  //                                where the extra discrete dimension labels the source of the imported histogram.
  //                              
  // Import(const char*,         -- Import a dataset to be associated with the given state name of the index category
  //              RooDataSet&)      specified in Index(). If the given state name is not yet defined in the index
  //                               category it will be added on the fly. The import command can be specified
  //                                multiple times. 
  //
  // Link(const char*, RooDataSet&) -- Link contents of supplied RooDataSet to this dataset for given index category state name.
  //                                   In this mode, no data is copied and the linked dataset must be remain live for the duration
  //                                   of this dataset. Note that link is active for both reading and writing, so modifications
  //                                   to the aggregate dataset will also modify its components. Link() and Import() are mutually exclusive.
  // OwnLinked()                    -- Take ownership of all linked datasets
  //
  // Import(map<string,RooDataSet*>&) -- As above, but allows specification of many imports in a single operation
  // Link(map<string,RooDataSet*>&)   -- As above, but allows specification of many links in a single operation
  //
  //                              
  // Cut(const char*)            -- Apply the given cut specification when importing data
  // Cut(RooFormulaVar&)         
  //
  // CutRange(const char*)       -- Only accept events in the observable range with the given name
  //
  // WeightVar(const char*)      -- Interpret the given variable as event weight rather than as observable
  // WeightVar(const RooAbsArg&) 
  //
  // StoreError(const RooArgSet&)     -- Store symmetric error along with value for given subset of observables
  // StoreAsymError(const RooArgSet&) -- Store asymmetric error along with value for given subset of observables
  //

  // Define configuration for this method
  RooCmdConfig pc(Form("RooDataSet::ctor(%s)",GetName())) ;
  pc.defineInt("ownLinked","OwnLinked",0) ;
  pc.defineObject("impTree","ImportTree",0) ;
  pc.defineObject("impData","ImportData",0) ;
  pc.defineObject("indexCat","IndexCat",0) ;
  pc.defineObject("impSliceData","ImportDataSlice",0,0,kTRUE) ; // array
  pc.defineString("impSliceState","ImportDataSlice",0,"",kTRUE) ; // array
  pc.defineObject("lnkSliceData","LinkDataSlice",0,0,kTRUE) ; // array
  pc.defineString("lnkSliceState","LinkDataSlice",0,"",kTRUE) ; // array
  pc.defineString("cutSpec","CutSpec",0,"") ; 
  pc.defineObject("cutVar","CutVar",0) ;
  pc.defineString("cutRange","CutRange",0,"") ;
  pc.defineString("wgtVarName","WeightVarName",0,"") ;
  pc.defineString("fname","ImportFromFile",0,"") ;
  pc.defineString("tname","ImportFromFile",1,"") ;
  pc.defineObject("wgtVar","WeightVar",0) ;
  pc.defineObject("dummy1","ImportDataSliceMany",0) ;
  pc.defineObject("dummy2","LinkDataSliceMany",0) ;
  pc.defineSet("errorSet","StoreError",0) ;
  pc.defineSet("asymErrSet","StoreAsymError",0) ;
  pc.defineMutex("ImportTree","ImportData","ImportDataSlice","LinkDataSlice","ImportFromFile") ;
  pc.defineMutex("CutSpec","CutVar") ;
  pc.defineMutex("WeightVarName","WeightVar") ;
  pc.defineDependency("ImportDataSlice","IndexCat") ;
  pc.defineDependency("LinkDataSlice","IndexCat") ;
  pc.defineDependency("OwnLinked","LinkDataSlice") ;

  
  RooLinkedList l ;
  l.Add((TObject*)&arg1) ;  l.Add((TObject*)&arg2) ;  
  l.Add((TObject*)&arg3) ;  l.Add((TObject*)&arg4) ;
  l.Add((TObject*)&arg5) ;  l.Add((TObject*)&arg6) ;  
  l.Add((TObject*)&arg7) ;  l.Add((TObject*)&arg8) ;

  // Process & check varargs 
  pc.process(l) ;
  if (!pc.ok(kTRUE)) {
    assert(0) ;
    return ;
  }

  // Extract relevant objects
  TTree* impTree = static_cast<TTree*>(pc.getObject("impTree")) ;
  RooDataSet* impData = static_cast<RooDataSet*>(pc.getObject("impData")) ;
  RooFormulaVar* cutVar = static_cast<RooFormulaVar*>(pc.getObject("cutVar")) ;
  const char* cutSpec = pc.getString("cutSpec","",kTRUE) ;
  const char* cutRange = pc.getString("cutRange","",kTRUE) ;
  const char* wgtVarName = pc.getString("wgtVarName","",kTRUE) ;
  RooRealVar* wgtVar = static_cast<RooRealVar*>(pc.getObject("wgtVar")) ;
  const char* impSliceNames = pc.getString("impSliceState","",kTRUE) ;
  const RooLinkedList& impSliceData = pc.getObjectList("impSliceData") ;
  const char* lnkSliceNames = pc.getString("lnkSliceState","",kTRUE) ;
  const RooLinkedList& lnkSliceData = pc.getObjectList("lnkSliceData") ;
  RooCategory* indexCat = static_cast<RooCategory*>(pc.getObject("indexCat")) ;
  RooArgSet* errorSet = pc.getSet("errorSet") ;
  RooArgSet* asymErrorSet = pc.getSet("asymErrSet") ;
  const char* fname = pc.getString("fname") ;
  const char* tname = pc.getString("tname") ;
  Int_t ownLinked = pc.getInt("ownLinked") ;


  // Case 1 --- Link multiple dataset as slices
  if (lnkSliceNames) {

    // Make import mapping if index category is specified
    map<string,RooAbsData*> hmap ;  
    if (indexCat) {
      char tmp[10240] ;
      strlcpy(tmp,lnkSliceNames,10240) ;      
      char* token = strtok(tmp,",") ;
      TIterator* hiter = lnkSliceData.MakeIterator() ;
      while(token) {
	hmap[token] = (RooAbsData*) hiter->Next() ;
	token = strtok(0,",") ;
      }
      delete hiter ;
    }
    
    // Lookup name of weight variable if it was specified by object reference
    if (wgtVar) {
      // coverity[UNUSED_VALUE]
      wgtVarName = wgtVar->GetName() ;
    }
        
    appendToDir(this,kTRUE) ;
    
    // Initialize RooDataSet with optional weight variable
    initialize(0) ;
    
    map<string,RooAbsDataStore*> storeMap ;
    RooCategory* icat = (RooCategory*) (indexCat ? _vars.find(indexCat->GetName()) : 0 ) ;
    if (!icat) {
      throw std::string("RooDataSet::RooDataSet() ERROR in constructor, cannot find index category") ;
    }
    for (map<string,RooAbsData*>::iterator hiter = hmap.begin() ; hiter!=hmap.end() ; ++hiter) {
      // Define state labels in index category (both in provided indexCat and in internal copy in dataset)
      if (indexCat && !indexCat->lookupType(hiter->first.c_str())) {
	indexCat->defineType(hiter->first.c_str()) ;
	coutI(InputArguments) << "RooDataSet::ctor(" << GetName() << ") defining state \"" << hiter->first << "\" in index category " << indexCat->GetName() << endl ;
      }
      if (icat && !icat->lookupType(hiter->first.c_str())) {	
	icat->defineType(hiter->first.c_str()) ;
      }
      icat->setLabel(hiter->first.c_str()) ;
      storeMap[icat->getLabel()]=hiter->second->store() ;

      // Take ownership of slice if requested
      if (ownLinked) {
	addOwnedComponent(hiter->first.c_str(),*hiter->second) ;
      }
    }

    // Create composite datastore
    _dstore = new RooCompositeDataStore(name,title,_vars,*icat,storeMap) ;
        
  } else {

    if (wgtVar) {
      wgtVarName = wgtVar->GetName() ;
    }

    // Create empty datastore 
    RooTreeDataStore* tstore(0) ;
    RooVectorDataStore* vstore(0) ;

    if (defaultStorageType==Tree) {
      tstore = new RooTreeDataStore(name,title,_vars,wgtVarName) ;
      _dstore = tstore ;
    } else if (defaultStorageType==Vector) {
      vstore = new RooVectorDataStore(name,title,_vars,wgtVarName) ;
      _dstore = vstore ;
    } else {
      _dstore = 0 ;
    }
    
    
    // Make import mapping if index category is specified
    map<string,RooDataSet*> hmap ;  
    if (indexCat) {
      char tmp[1024] ;
      strlcpy(tmp,impSliceNames,1024) ;
      char* token = strtok(tmp,",") ;
      TIterator* hiter = impSliceData.MakeIterator() ;
      while(token) {
	hmap[token] = (RooDataSet*) hiter->Next() ;
	token = strtok(0,",") ;
      }
      delete hiter ;
    }
    
    // process StoreError requests
    if (errorSet) {
      RooArgSet* intErrorSet = (RooArgSet*) _vars.selectCommon(*errorSet) ;
      intErrorSet->setAttribAll("StoreError") ;
      TIterator* iter = intErrorSet->createIterator() ;
      RooAbsArg* arg ;
      while((arg=(RooAbsArg*)iter->Next())) {
	arg->attachToStore(*_dstore) ;
      }
      delete iter ;
      delete intErrorSet ;
    }
    if (asymErrorSet) {
      RooArgSet* intAsymErrorSet = (RooArgSet*) _vars.selectCommon(*asymErrorSet) ;
      intAsymErrorSet->setAttribAll("StoreAsymError") ;
      TIterator* iter = intAsymErrorSet->createIterator() ;
      RooAbsArg* arg ;
      while((arg=(RooAbsArg*)iter->Next())) {
	arg->attachToStore(*_dstore) ;
    }
      delete iter ;
      delete intAsymErrorSet ;
    }
    
    // Lookup name of weight variable if it was specified by object reference
    if (wgtVar) {
      wgtVarName = wgtVar->GetName() ;
    }
    
    
    appendToDir(this,kTRUE) ;
    
    // Initialize RooDataSet with optional weight variable
    if (wgtVarName && *wgtVarName) {
      // Use the supplied weight column
      initialize(wgtVarName) ;    
    } else {
      if (impData && impData->_wgtVar && vars.find(impData->_wgtVar->GetName())) {
	// Use the weight column of the source data set
	initialize(impData->_wgtVar->GetName()) ;

      } else if (indexCat) {

	RooDataSet* firstDS = hmap.begin()->second ;
	if (firstDS->_wgtVar && vars.find(firstDS->_wgtVar->GetName())) {
	  initialize(firstDS->_wgtVar->GetName()) ;      
	} else {
	  initialize(0) ;
	}
      } else {
	initialize(0) ;
      }
    }
    
    // Import one or more datasets with a cut specification
    if (cutSpec && *cutSpec) {
      
      // Create a RooFormulaVar cut from given cut expression
      if (indexCat) {

	// Case 2a --- Import multiple RooDataSets as slices with cutspec
	RooCategory* icat = (RooCategory*) _vars.find(indexCat->GetName()) ;
	for (map<string,RooDataSet*>::iterator hiter = hmap.begin() ; hiter!=hmap.end() ; ++hiter) {
	  // Define state labels in index category (both in provided indexCat and in internal copy in dataset)
	  if (!indexCat->lookupType(hiter->first.c_str())) {
	    indexCat->defineType(hiter->first.c_str()) ;
	    coutI(InputArguments) << "RooDataSet::ctor(" << GetName() << ") defining state \"" << hiter->first << "\" in index category " << indexCat->GetName() << endl ;
	  }
	  if (!icat->lookupType(hiter->first.c_str())) {	
	    icat->defineType(hiter->first.c_str()) ;
	  }
	  icat->setLabel(hiter->first.c_str()) ;
	  
	  RooFormulaVar cutVarTmp(cutSpec,cutSpec,hiter->second->_vars) ;
	  _dstore->loadValues(hiter->second->store(),&cutVarTmp,cutRange) ;
	}
	
      } else if (impData) {

	// Case 3a --- Import RooDataSet with cutspec
	RooFormulaVar cutVarTmp(cutSpec,cutSpec,impData->_vars) ;
	_dstore->loadValues(impData->store(),&cutVarTmp,cutRange);
      } else if (impTree) {

	// Case 4a --- Import TTree from memory with cutspec
	RooFormulaVar cutVarTmp(cutSpec,cutSpec,_vars) ;
	if (tstore) {
	  tstore->loadValues(impTree,&cutVarTmp,cutRange);      
	} else {
	  RooTreeDataStore tmpstore(name,title,_vars,wgtVarName) ;
	  tmpstore.loadValues(impTree,&cutVarTmp,cutRange) ;
	  _dstore->append(tmpstore) ;
	}
      } else if (fname && strlen(fname)) {

	// Case 5a --- Import TTree from file with cutspec
	TFile *f = TFile::Open(fname) ;
	if (!f) {
	  coutE(InputArguments) << "RooDataSet::ctor(" << GetName() << ") ERROR file '" << fname << "' cannot be opened or does not exist" << endl ;	  
	  throw string(Form("RooDataSet::ctor(%s) ERROR file %s cannot be opened or does not exist",GetName(),fname)) ;
	}
	TTree* t = dynamic_cast<TTree*>(f->Get(tname)) ;
	if (!t) {
	  coutE(InputArguments) << "RooDataSet::ctor(" << GetName() << ") ERROR file '" << fname << "' does not contain a TTree named '" << tname << "'" << endl ;
	  throw string(Form("RooDataSet::ctor(%s) ERROR file %s does not contain a TTree named %s",GetName(),fname,tname)) ;
	}
	RooFormulaVar cutVarTmp(cutSpec,cutSpec,_vars) ;
	if (tstore) {
	  tstore->loadValues(t,&cutVarTmp,cutRange);      	
	} else {
	  RooTreeDataStore tmpstore(name,title,_vars,wgtVarName) ;
	  tmpstore.loadValues(impTree,&cutVarTmp,cutRange) ;
	  _dstore->append(tmpstore) ;
	}
	f->Close() ;

      }
      
      // Import one or more datasets with a cut formula
    } else if (cutVar) {
      
      if (indexCat) {
	
	// Case 2b --- Import multiple RooDataSets as slices with cutvar

	RooCategory* icat = (RooCategory*) _vars.find(indexCat->GetName()) ;
	for (map<string,RooDataSet*>::iterator hiter = hmap.begin() ; hiter!=hmap.end() ; ++hiter) {
	  // Define state labels in index category (both in provided indexCat and in internal copy in dataset)
	  if (!indexCat->lookupType(hiter->first.c_str())) {
	    indexCat->defineType(hiter->first.c_str()) ;
	    coutI(InputArguments) << "RooDataSet::ctor(" << GetName() << ") defining state \"" << hiter->first << "\" in index category " << indexCat->GetName() << endl ;
	  }
	  if (!icat->lookupType(hiter->first.c_str())) {	
	    icat->defineType(hiter->first.c_str()) ;
	  }
	  icat->setLabel(hiter->first.c_str()) ;
	  _dstore->loadValues(hiter->second->store(),cutVar,cutRange) ;
	}
	
	
      } else if (impData) {
	// Case 3b --- Import RooDataSet with cutvar
	_dstore->loadValues(impData->store(),cutVar,cutRange);
      } else if (impTree) {
	// Case 4b --- Import TTree from memory with cutvar
	if (tstore) {
	  tstore->loadValues(impTree,cutVar,cutRange);
	} else {
	  RooTreeDataStore tmpstore(name,title,_vars,wgtVarName) ;
	  tmpstore.loadValues(impTree,cutVar,cutRange) ;
	  _dstore->append(tmpstore) ;
	}
	} else if (fname && strlen(fname)) {
	// Case 5b --- Import TTree from file with cutvar
	TFile *f = TFile::Open(fname) ;
	if (!f) {
	  coutE(InputArguments) << "RooDataSet::ctor(" << GetName() << ") ERROR file '" << fname << "' cannot be opened or does not exist" << endl ;	  
	  throw string(Form("RooDataSet::ctor(%s) ERROR file %s cannot be opened or does not exist",GetName(),fname)) ;
	}
	TTree* t = dynamic_cast<TTree*>(f->Get(tname)) ;
	if (!t) {
	  coutE(InputArguments) << "RooDataSet::ctor(" << GetName() << ") ERROR file '" << fname << "' does not contain a TTree named '" << tname << "'" << endl ;
	  throw string(Form("RooDataSet::ctor(%s) ERROR file %s does not contain a TTree named %s",GetName(),fname,tname)) ;
	}
	if (tstore) {
	  tstore->loadValues(t,cutVar,cutRange);      	
	} else {
	  RooTreeDataStore tmpstore(name,title,_vars,wgtVarName) ;
	  tmpstore.loadValues(impTree,cutVar,cutRange) ;
	  _dstore->append(tmpstore) ;
	}

	f->Close() ;
      }
      
      // Import one or more datasets without cuts
    } else {
      
      if (indexCat) {
	
	RooCategory* icat = (RooCategory*) _vars.find(indexCat->GetName()) ;
	for (map<string,RooDataSet*>::iterator hiter = hmap.begin() ; hiter!=hmap.end() ; ++hiter) {
	  // Define state labels in index category (both in provided indexCat and in internal copy in dataset)
	  if (!indexCat->lookupType(hiter->first.c_str())) {
	    indexCat->defineType(hiter->first.c_str()) ;
	    coutI(InputArguments) << "RooDataSet::ctor(" << GetName() << ") defining state \"" << hiter->first << "\" in index category " << indexCat->GetName() << endl ;
	  }
	  if (!icat->lookupType(hiter->first.c_str())) {	
	    icat->defineType(hiter->first.c_str()) ;
	  }
	  icat->setLabel(hiter->first.c_str()) ;
	  // Case 2c --- Import multiple RooDataSets as slices
	  _dstore->loadValues(hiter->second->store(),0,cutRange) ;
	}
	
	
      } else if (impData) {
	// Case 3c --- Import RooDataSet
	_dstore->loadValues(impData->store(),0,cutRange);
      } else if (impTree) {
	// Case 4c --- Import TTree from memort
	if (tstore) {
	  tstore->loadValues(impTree,0,cutRange);
	} else {
	  RooTreeDataStore tmpstore(name,title,_vars,wgtVarName) ;
	  tmpstore.loadValues(impTree,0,cutRange) ;
	  _dstore->append(tmpstore) ;
	}
      } else if (fname && strlen(fname)) {
	// Case 5c --- Import TTree from file
	TFile *f = TFile::Open(fname) ;
	if (!f) {
	  coutE(InputArguments) << "RooDataSet::ctor(" << GetName() << ") ERROR file '" << fname << "' cannot be opened or does not exist" << endl ;	  
	  throw string(Form("RooDataSet::ctor(%s) ERROR file %s cannot be opened or does not exist",GetName(),fname)) ;
	}
	TTree* t = dynamic_cast<TTree*>(f->Get(tname)) ;
	if (!t) {
	  coutE(InputArguments) << "RooDataSet::ctor(" << GetName() << ") ERROR file '" << fname << "' does not contain a TTree named '" << tname << "'" << endl ;
	  throw string(Form("RooDataSet::ctor(%s) ERROR file %s does not contain a TTree named %s",GetName(),fname,tname)) ;
	}
	if (tstore) {
	  tstore->loadValues(t,0,cutRange);      	
	} else {
	  RooTreeDataStore tmpstore(name,title,_vars,wgtVarName) ;
	  tmpstore.loadValues(impTree,0,cutRange) ;
	  _dstore->append(tmpstore) ;
	}
	f->Close() ;
      }
    }
    
  }
}



//_____________________________________________________________________________
RooDataSet::RooDataSet(const char *name, const char *title, const RooArgSet& vars, const char* wgtVarName) :
  RooAbsData(name,title,vars)
{
  // Constructor of an empty data set from a RooArgSet defining the dimensions
  // of the data space.

//   cout << "RooDataSet::ctor(" << this << ") storageType = " << ((defaultStorageType==Tree)?"Tree":"Vector") << endl ;
  _dstore = (defaultStorageType==Tree) ? ((RooAbsDataStore*) new RooTreeDataStore(name,title,_vars,wgtVarName)) : 
                                         ((RooAbsDataStore*) new RooVectorDataStore(name,title,_vars,wgtVarName)) ;

  appendToDir(this,kTRUE) ;
  initialize(wgtVarName) ;
}


//_____________________________________________________________________________
RooDataSet::RooDataSet(const char *name, const char *title, RooDataSet *dset, 
		       const RooArgSet& vars, const char *cuts, const char* wgtVarName) :
  RooAbsData(name,title,vars)
{
  // Constructor of a data set from (part of) an existing data
  // set. The dimensions of the data set are defined by the 'vars'
  // RooArgSet, which can be identical to 'dset' dimensions, or a
  // subset thereof. The 'cuts' string is an optional RooFormula
  // expression and can be used to select the subset of the data
  // points in 'dset' to be copied. The cut expression can refer to
  // any variable in the source dataset. For cuts involving variables
  // other than those contained in the source data set, such as
  // intermediate formula objects, use the equivalent constructor
  // accepting RooFormulaVar reference as cut specification
  //
  // For most uses the RooAbsData::reduce() wrapper function, which
  // uses this constructor, is the most convenient way to create a
  // subset of an existing data
  //

  // Initialize datastore
  _dstore = new RooTreeDataStore(name,title,_vars,*dset->_dstore,cuts,wgtVarName) ;

  appendToDir(this,kTRUE) ;
  
  if (wgtVarName) {
    // Use the supplied weight column
    initialize(wgtVarName) ;    
  } else {
    if (dset->_wgtVar && vars.find(dset->_wgtVar->GetName())) {
      // Use the weight column of the source data set
      initialize(dset->_wgtVar->GetName()) ;
    } else {
      initialize(0) ;
    }
  }

}


//_____________________________________________________________________________
RooDataSet::RooDataSet(const char *name, const char *title, RooDataSet *dset, 
		       const RooArgSet& vars, const RooFormulaVar& cutVar, const char* wgtVarName) :
  RooAbsData(name,title,vars)
{
  // Constructor of a data set from (part of) an existing data
  // set. The dimensions of the data set are defined by the 'vars'
  // RooArgSet, which can be identical to 'dset' dimensions, or a
  // subset thereof. The 'cutVar' formula variable is used to select
  // the subset of data points to be copied.  For subsets without
  // selection on the data points, or involving cuts operating
  // exclusively and directly on the data set dimensions, the
  // equivalent constructor with a string based cut expression is
  // recommended.
  //
  // For most uses the RooAbsData::reduce() wrapper function, which
  // uses this constructor, is the most convenient way to create a
  // subset of an existing data

  // Initialize datastore
  _dstore = new RooTreeDataStore(name,title,_vars,*dset->_dstore,cutVar,wgtVarName) ;

  appendToDir(this,kTRUE) ;

  if (wgtVarName) {
    // Use the supplied weight column
    initialize(wgtVarName) ;    
  } else {
    if (dset->_wgtVar && vars.find(dset->_wgtVar->GetName())) {
      // Use the weight column of the source data set
      initialize(dset->_wgtVar->GetName()) ;
    } else {
      initialize(0) ;
    }
  }
}




//_____________________________________________________________________________
RooDataSet::RooDataSet(const char *name, const char *title, TTree *intree, 
		       const RooArgSet& vars, const RooFormulaVar& cutVar, const char* wgtVarName) :
  RooAbsData(name,title,vars)
{
  // Constructor of a data set from (part of) an ROOT TTRee. The dimensions
  // of the data set are defined by the 'vars' RooArgSet. For each dimension
  // specified, the TTree must have a branch with the same name. For category
  // branches, this branch should contain the numeric index value. Real dimensions
  // can be constructed from either 'Double_t' or 'Float_t' tree branches. In the
  // latter case, an automatic conversion is applied.
  //
  // The 'cutVar' formula variable
  // is used to select the subset of data points to be copied.
  // For subsets without selection on the data points, or involving cuts
  // operating exclusively and directly on the data set dimensions, the equivalent
  // constructor with a string based cut expression is recommended.

  // Initialize datastore
  _dstore = new RooTreeDataStore(name,title,_vars,*intree,cutVar,wgtVarName) ;

  appendToDir(this,kTRUE) ;

  initialize(wgtVarName) ;
}



//_____________________________________________________________________________
RooDataSet::RooDataSet(const char *name, const char *title, TTree *intree, 
		       const RooArgSet& vars, const char *selExpr, const char* wgtVarName) :
  RooAbsData(name,title,vars)
{
  // Constructor of a data set from (part of) an ROOT TTRee. The dimensions
  // of the data set are defined by the 'vars' RooArgSet. For each dimension
  // specified, the TTree must have a branch with the same name. For category
  // branches, this branch should contain the numeric index value. Real dimensions
  // can be constructed from either 'Double_t' or 'Float_t' tree branches. In the
  // latter case, an automatic conversion is applied.
  //
  // The 'cuts' string is an optional
  // RooFormula expression and can be used to select the subset of the data points 
  // in 'dset' to be copied. The cut expression can refer to any variable in the
  // vars argset. For cuts involving variables other than those contained in
  // the vars argset, such as intermediate formula objects, use the 
  // equivalent constructor accepting RooFormulaVar reference as cut specification
  //

  // Initialize datastore
  _dstore = new RooTreeDataStore(name,title,_vars,*intree,selExpr,wgtVarName) ;

  appendToDir(this,kTRUE) ;

  initialize(wgtVarName) ;
}



//_____________________________________________________________________________
RooDataSet::RooDataSet(RooDataSet const & other, const char* newname) :
  RooAbsData(other,newname), RooDirItem()
{
  // Copy constructor

  appendToDir(this,kTRUE) ;
  initialize(other._wgtVar?other._wgtVar->GetName():0) ;
}

//_____________________________________________________________________________
RooDataSet::RooDataSet(const char *name, const char *title, RooDataSet *dset, 
		       const RooArgSet& vars, const RooFormulaVar* cutVar, const char* cutRange,
		       Int_t nStart, Int_t nStop, Bool_t copyCache, const char* wgtVarName) :
  RooAbsData(name,title,vars)
{
  // Protected constructor for internal use only

  _dstore = (defaultStorageType==Tree) ? 
    ((RooAbsDataStore*) new RooTreeDataStore(name,title,*dset->_dstore,_vars,cutVar,cutRange,nStart,nStop,copyCache,wgtVarName)) :
    ((RooAbsDataStore*) new RooVectorDataStore(name,title,*dset->_dstore,_vars,cutVar,cutRange,nStart,nStop,copyCache,wgtVarName)) ;

  _cachedVars.add(_dstore->cachedVars()) ;

  appendToDir(this,kTRUE) ;
  initialize(dset->_wgtVar?dset->_wgtVar->GetName():0) ;
}


//_____________________________________________________________________________
RooArgSet RooDataSet::addWgtVar(const RooArgSet& origVars, const RooAbsArg* wgtVar)
{
  // Helper function for constructor that adds optional weight variable to construct
  // total set of observables

  RooArgSet tmp(origVars) ;
  if (wgtVar) tmp.add(*wgtVar) ;
  return tmp ;
}



//_____________________________________________________________________________
RooAbsData* RooDataSet::cacheClone(const RooAbsArg* newCacheOwner, const RooArgSet* newCacheVars, const char* newName) 
{
  // Return a clone of this dataset containing only the cached variables

  RooDataSet* dset = new RooDataSet(newName?newName:GetName(),GetTitle(),this,_vars,(RooFormulaVar*)0,0,0,2000000000,kTRUE,_wgtVar?_wgtVar->GetName():0) ;  
  //if (_wgtVar) dset->setWeightVar(_wgtVar->GetName()) ;

  RooArgSet* selCacheVars = (RooArgSet*) newCacheVars->selectCommon(dset->_cachedVars) ;
  dset->attachCache(newCacheOwner, *selCacheVars) ;
  delete selCacheVars ;

  return dset ;
}



//_____________________________________________________________________________
RooAbsData* RooDataSet::emptyClone(const char* newName, const char* newTitle, const RooArgSet* vars) const 
{
  // Return an empty clone of this dataset. If vars is not null, only the variables in vars
  // are added to the definition of the empty clone

  // If variables are given, be sure to include weight variable if it exists and is not included
  RooArgSet vars2 ;
  if (vars) {
    vars2.add(*vars) ;
    if (_wgtVar && !vars2.find(_wgtVar->GetName())) {
      vars2.add(*_wgtVar) ;
    }
  } else {
    vars2.add(_vars) ;
  }

  RooDataSet* dset = new RooDataSet(newName?newName:GetName(),newTitle?newTitle:GetTitle(),vars2,_wgtVar?_wgtVar->GetName():0) ; 
  //if (_wgtVar) dset->setWeightVar(_wgtVar->GetName()) ;
  return dset ;
}



//_____________________________________________________________________________
void RooDataSet::initialize(const char* wgtVarName) 
{
  // Initialize the dataset. If wgtVarName is not null, interpret the observable
  // with that name as event weight

  _varsNoWgt.removeAll() ;
  _varsNoWgt.add(_vars) ;
  _wgtVar = 0 ;
  if (wgtVarName) {
    RooAbsArg* wgt = _varsNoWgt.find(wgtVarName) ;
    if (!wgt) {
      coutW(DataHandling) << "RooDataSet::RooDataSet(" << GetName() << ") WARNING: designated weight variable " 
			  << wgtVarName << " not found in set of variables, no weighting will be assigned" << endl ;
    } else if (!dynamic_cast<RooRealVar*>(wgt)) {
      coutW(DataHandling) << "RooDataSet::RooDataSet(" << GetName() << ") WARNING: designated weight variable " 
			  << wgtVarName << " is not of type RooRealVar, no weighting will be assigned" << endl ;
    } else {
      _varsNoWgt.remove(*wgt) ;
      _wgtVar = (RooRealVar*) wgt ;
    }
  }
}



//_____________________________________________________________________________
RooAbsData* RooDataSet::reduceEng(const RooArgSet& varSubset, const RooFormulaVar* cutVar, const char* cutRange, 
				  Int_t nStart, Int_t nStop, Bool_t copyCache)
{
  // Implementation of RooAbsData virtual method that drives the RooAbsData::reduce() methods
  checkInit() ;

  RooArgSet tmp(varSubset) ;
  if (_wgtVar) {
    tmp.add(*_wgtVar) ;
  }
  RooDataSet* ret =  new RooDataSet(GetName(), GetTitle(), this, tmp, cutVar, cutRange, nStart, nStop, copyCache,_wgtVar?_wgtVar->GetName():0) ;
  
  // WVE - propagate optional weight variable
  //       check behaviour in plotting.
//   if (_wgtVar) {
//     ret->setWeightVar(_wgtVar->GetName()) ;
//   }
  return ret ;
}



//_____________________________________________________________________________
RooDataSet::~RooDataSet()
{
  // Destructor

  removeFromDir(this) ;
}



//_____________________________________________________________________________
RooDataHist* RooDataSet::binnedClone(const char* newName, const char* newTitle) const 
{
  // Return binned clone of this dataset

  TString title, name ;
  if (newName) {
    name = newName ;
  } else {
    name = Form("%s_binned",GetName()) ;
  }
  if (newTitle) {
    title = newTitle ;
  } else {
    title = Form("%s_binned",GetTitle()) ;
  }

  return new RooDataHist(name,title,*get(),*this) ;
}



//_____________________________________________________________________________
Double_t RooDataSet::weight() const 
{
  // Return event weight of current event

  return store()->weight() ; 
}




//_____________________________________________________________________________
void RooDataSet::weightError(Double_t& lo, Double_t& hi, ErrorType etype) const 
{
  store()->weightError(lo,hi,etype) ;
}



//_____________________________________________________________________________
Double_t RooDataSet::weightError(ErrorType etype) const 
{
  return store()->weightError(etype) ;
}



//_____________________________________________________________________________
const RooArgSet* RooDataSet::get(Int_t index) const
{
  // Return RooArgSet with coordinates of event 'index' 
  const RooArgSet* ret  = RooAbsData::get(index) ;
  return ret ? &_varsNoWgt : 0 ;
}


//_____________________________________________________________________________
Double_t RooDataSet::sumEntries() const 
{
  return store()->sumEntries() ;
  
  //---------

  // Shortcut for unweighted unselected datasets
  if (!isWeighted()) {
    return numEntries() ;
  }

  // Otherwise sum the weights in the event
  Double_t sumw(0) ;
  Int_t i ;
  for (i=0 ; i<numEntries() ; i++) {
    get(i) ;
    sumw += weight() ;
  }  

  return sumw ;  
}


//_____________________________________________________________________________
Double_t RooDataSet::sumEntries(const char* cutSpec, const char* cutRange) const 
{
  // Return the sum of weights in all entries matching cutSpec (if specified)
  // and in named range cutRange (if specified)

  // Setup RooFormulaVar for cutSpec if it is present
  RooFormula* select = 0 ;
  if (cutSpec) {
    select = new RooFormula("select",cutSpec,*get()) ;
  }
  
  // Shortcut for unweighted unselected datasets
  if (!select && !cutRange && !isWeighted()) {
    return numEntries() ;
  }

  // Otherwise sum the weights in the event
  Double_t sumw(0) ;
  Int_t i ;
  for (i=0 ; i<numEntries() ; i++) {
    get(i) ;
    if (select && select->eval()==0.) continue ;
    if (cutRange && !_vars.allInRange(cutRange)) continue ;
    sumw += weight() ;
  }

  if (select) delete select ;

  return sumw ;  
}




//_____________________________________________________________________________
Bool_t RooDataSet::isWeighted() const
{ 
    // Return true if dataset contains weighted events
    return store()->isWeighted() ;
}



//_____________________________________________________________________________
Bool_t RooDataSet::isNonPoissonWeighted() const
{
  // Returns true if histogram contains bins with entries with a non-integer weight

  // Return false if we have no weights
  if (!_wgtVar) return kFALSE ;
  
  // Now examine individual weights
  for (int i=0 ; i<numEntries() ; i++) {
    get(i) ;
    if (fabs(weight()-Int_t(weight()))>1e-10) return kTRUE ;
  }
  // If sum of weights is less than number of events there are negative (integer) weights
  if (sumEntries()<numEntries()) return kTRUE ;

  return kFALSE ;
}




//_____________________________________________________________________________
const RooArgSet* RooDataSet::get() const 
{ 
  // Return a RooArgSet with the coordinates of the current event
  return &_varsNoWgt ; 
} 



//_____________________________________________________________________________
void RooDataSet::add(const RooArgSet& data, Double_t wgt, Double_t wgtError) 
{
  // Add a data point, with its coordinates specified in the 'data' argset, to the data set. 
  // Any variables present in 'data' but not in the dataset will be silently ignored
  //
  checkInit() ;
  _varsNoWgt = data;
  if (_wgtVar) {
    _wgtVar->setVal(wgt) ;
    if (wgtError!=0.) {
      _wgtVar->setError(wgtError) ;
    }
  }
  fill();
}




//_____________________________________________________________________________
void RooDataSet::add(const RooArgSet& indata, Double_t inweight, Double_t weightErrorLo, Double_t weightErrorHi) 
{
  // Add a data point, with its coordinates specified in the 'data' argset, to the data set. 
  // Any variables present in 'data' but not in the dataset will be silently ignored
  //
  checkInit() ;

  _varsNoWgt = indata;
  if (_wgtVar) {
    _wgtVar->setVal(inweight) ;
    _wgtVar->setAsymError(weightErrorLo,weightErrorHi) ;
  }
  fill();
}





//_____________________________________________________________________________
void RooDataSet::addFast(const RooArgSet& data, Double_t wgt, Double_t wgtError) 
{
  // Add a data point, with its coordinates specified in the 'data' argset, to the data set. 
  // Layout and size of input argument data is ASSUMED to be the same as RooArgSet returned
  // RooDataSet::get()
  //

  checkInit() ;
  _varsNoWgt.assignFast(data,_dstore->dirtyProp());
  if (_wgtVar) {
    _wgtVar->setVal(wgt) ;
    if (wgtError!=0.) {
      _wgtVar->setError(wgtError) ;
    }
  }
  fill();
}



//_____________________________________________________________________________
Bool_t RooDataSet::merge(RooDataSet* data1, RooDataSet* data2, RooDataSet* data3, 
			 RooDataSet* data4, RooDataSet* data5, RooDataSet* data6) 
{
  checkInit() ;
  list<RooDataSet*> dsetList ;
  if (data1) dsetList.push_back(data1) ;
  if (data2) dsetList.push_back(data2) ;
  if (data3) dsetList.push_back(data3) ;
  if (data4) dsetList.push_back(data4) ;
  if (data5) dsetList.push_back(data5) ;
  if (data6) dsetList.push_back(data6) ;
  return merge(dsetList) ;
}



//_____________________________________________________________________________
Bool_t RooDataSet::merge(list<RooDataSet*>dsetList)
{
  // Merge columns of supplied data set(s) with this data set.  All
  // data sets must have equal number of entries.  In case of
  // duplicate columns the column of the last dataset in the list
  // prevails


  checkInit() ;
  // Sanity checks: data sets must have the same size
  for (list<RooDataSet*>::iterator iter = dsetList.begin() ; iter != dsetList.end() ; iter++) {
    if (numEntries()!=(*iter)->numEntries()) {
      coutE(InputArguments) << "RooDataSet::merge(" << GetName() << ") ERROR: datasets have different size" << endl ;
      return kTRUE ;    
    }
  }

  // Extend vars with elements of other dataset
  list<RooAbsDataStore*> dstoreList ;
  for (list<RooDataSet*>::iterator iter = dsetList.begin() ; iter != dsetList.end() ; iter++) {
    _vars.addClone((*iter)->_vars,kTRUE) ;
    dstoreList.push_back((*iter)->store()) ;
  }

  // Merge data stores
  RooAbsDataStore* mergedStore = _dstore->merge(_vars,dstoreList) ;
  mergedStore->SetName(_dstore->GetName()) ;
  mergedStore->SetTitle(_dstore->GetTitle()) ;

  // Replace current data store with merged store
  delete _dstore ;
  _dstore = mergedStore ;

  initialize(_wgtVar?_wgtVar->GetName():0) ;
  return kFALSE ;
}


//_____________________________________________________________________________
void RooDataSet::append(RooDataSet& data) 
{
  // Add all data points of given data set to this data set.
  // Observable in 'data' that are not in this dataset
  // with not be transferred
  checkInit() ;
  _dstore->append(*data._dstore) ;
}



//_____________________________________________________________________________
RooAbsArg* RooDataSet::addColumn(RooAbsArg& var, Bool_t adjustRange) 
{
  // Add a column with the values of the given (function) argument
  // to this dataset. The function value is calculated for each
  // event using the observable values of each event in case the
  // function depends on variables with names that are identical
  // to the observable names in the dataset

  checkInit() ;
  RooAbsArg* ret = _dstore->addColumn(var,adjustRange) ;
  _vars.addOwned(*ret) ;
  initialize(_wgtVar?_wgtVar->GetName():0) ;
  return ret ;
}


//_____________________________________________________________________________
RooArgSet* RooDataSet::addColumns(const RooArgList& varList) 
{
  // Add a column with the values of the given list of (function)
  // argument to this dataset. Each function value is calculated for
  // each event using the observable values of the event in case the
  // function depends on variables with names that are identical to
  // the observable names in the dataset

  checkInit() ;
  RooArgSet* ret = _dstore->addColumns(varList) ;  
  _vars.addOwned(*ret) ;
  initialize(_wgtVar?_wgtVar->GetName():0) ;
  return ret ;
}



//_____________________________________________________________________________
TH2F* RooDataSet::createHistogram(const RooAbsRealLValue& var1, const RooAbsRealLValue& var2, const char* cuts, const char *name) const
{
  // Create a TH2F histogram of the distribution of the specified variable
  // using this dataset. Apply any cuts to select which events are used.
  // The variable being plotted can either be contained directly in this
  // dataset, or else be a function of the variables in this dataset.
  // The histogram will be created using RooAbsReal::createHistogram() with
  // the name provided (with our dataset name prepended).
  checkInit() ;
  return createHistogram(var1, var2, var1.getBins(), var2.getBins(), cuts, name);
}



//_____________________________________________________________________________
TH2F* RooDataSet::createHistogram(const RooAbsRealLValue& var1, const RooAbsRealLValue& var2, 
				  Int_t nx, Int_t ny, const char* cuts, const char *name) const
{
  // Create a TH2F histogram of the distribution of the specified variable
  // using this dataset. Apply any cuts to select which events are used.
  // The variable being plotted can either be contained directly in this
  // dataset, or else be a function of the variables in this dataset.
  // The histogram will be created using RooAbsReal::createHistogram() with
  // the name provided (with our dataset name prepended).

  checkInit() ;
  static Int_t counter(0) ;

  Bool_t ownPlotVarX(kFALSE) ;
  // Is this variable in our dataset?
  RooAbsReal* plotVarX= (RooAbsReal*)_vars.find(var1.GetName());
  if(0 == plotVarX) {
    // Is this variable a client of our dataset?
    if (!var1.dependsOn(_vars)) {
      coutE(InputArguments) << GetName() << "::createHistogram: Argument " << var1.GetName() 
			    << " is not in dataset and is also not dependent on data set" << endl ;
      return 0 ; 
    }

    // Clone derived variable 
    plotVarX = (RooAbsReal*) var1.Clone()  ;
    ownPlotVarX = kTRUE ;

    //Redirect servers of derived clone to internal ArgSet representing the data in this set
    plotVarX->redirectServers(const_cast<RooArgSet&>(_vars)) ;
  }

  Bool_t ownPlotVarY(kFALSE) ;
  // Is this variable in our dataset?
  RooAbsReal* plotVarY= (RooAbsReal*)_vars.find(var2.GetName());
  if(0 == plotVarY) {
    // Is this variable a client of our dataset?
    if (!var2.dependsOn(_vars)) {
      coutE(InputArguments) << GetName() << "::createHistogram: Argument " << var2.GetName() 
			    << " is not in dataset and is also not dependent on data set" << endl ;
      return 0 ; 
    }

    // Clone derived variable 
    plotVarY = (RooAbsReal*) var2.Clone()  ;
    ownPlotVarY = kTRUE ;

    //Redirect servers of derived clone to internal ArgSet representing the data in this set
    plotVarY->redirectServers(const_cast<RooArgSet&>(_vars)) ;
  }

  // Create selection formula if selection cuts are specified
  RooFormula* select = 0;
  if(0 != cuts && strlen(cuts)) {
    select=new RooFormula(cuts,cuts,_vars);
    if (!select || !select->ok()) {
      delete select;
      return 0 ;
    }
  }
  
  TString histName(name);
  histName.Prepend("_");
  histName.Prepend(fName);
  histName.Append("_") ;
  histName.Append(Form("%08x",counter++)) ;

  // create the histogram
  TH2F* histogram=new TH2F(histName.Data(), "Events", nx, var1.getMin(), var1.getMax(), 
                                                      ny, var2.getMin(), var2.getMax());
  if(!histogram) {
    coutE(DataHandling) << fName << "::createHistogram: unable to create a new histogram" << endl;
    return 0;
  }

  // Dump contents  
  Int_t nevent= numEntries() ;
  for(Int_t i=0; i < nevent; ++i) 
  {
    get(i);
    
    if (select && select->eval()==0) continue ;
    histogram->Fill(plotVarX->getVal(), plotVarY->getVal()) ;
  }

  if (ownPlotVarX) delete plotVarX ;
  if (ownPlotVarY) delete plotVarY ;
  if (select) delete select ;

  return histogram ;
}





//_____________________________________________________________________________
RooPlot* RooDataSet::plotOnXY(RooPlot* frame, const RooCmdArg& arg1, const RooCmdArg& arg2,
			      const RooCmdArg& arg3, const RooCmdArg& arg4,
			      const RooCmdArg& arg5, const RooCmdArg& arg6,
			      const RooCmdArg& arg7, const RooCmdArg& arg8) const 
{
  // Special plot method for 'X-Y' datasets used in Chi^2 fitting. These datasets 
  // have one observable (X) and have weights (Y) and associated errors.
  //
  // Contents options
  // ---------------------
  // YVar(RooRealVar& var)           -- Designate specified observable as 'y' variable
  //                                    If not specified, the event weight will be the y variable
  // Histogram drawing options
  // -------------------------
  // DrawOption(const char* opt)     -- Select ROOT draw option for resulting TGraph object
  // LineStyle(Int_t style)          -- Select line style by ROOT line style code, default is solid
  // LineColor(Int_t color)          -- Select line color by ROOT color code, default is black
  // LineWidth(Int_t width)          -- Select line with in pixels, default is 3
  // MarkerStyle(Int_t style)        -- Select the ROOT marker style, default is 21
  // MarkerColor(Int_t color)        -- Select the ROOT marker color, default is black
  // MarkerSize(Double_t size)       -- Select the ROOT marker size
  // Rescale(Double_t factor)        -- Apply global rescaling factor to histogram
  //
  //
  // Misc. other options
  // -------------------
  // Name(const chat* name)          -- Give curve specified name in frame. Useful if curve is to be referenced later
  // Invisible(Bool_t flag)          -- Add curve to frame, but do not display. Useful in combination AddTo()
  // 

  checkInit() ;

  RooLinkedList argList ;
  argList.Add((TObject*)&arg1) ;  argList.Add((TObject*)&arg2) ;  
  argList.Add((TObject*)&arg3) ;  argList.Add((TObject*)&arg4) ;
  argList.Add((TObject*)&arg5) ;  argList.Add((TObject*)&arg6) ;  
  argList.Add((TObject*)&arg7) ;  argList.Add((TObject*)&arg8) ;

  // Process named arguments
  RooCmdConfig pc(Form("RooDataSet::plotOnXY(%s)",GetName())) ;
  pc.defineString("drawOption","DrawOption",0,"P") ;
  pc.defineString("histName","Name",0,"") ;
  pc.defineInt("lineColor","LineColor",0,-999) ;
  pc.defineInt("lineStyle","LineStyle",0,-999) ;
  pc.defineInt("lineWidth","LineWidth",0,-999) ;
  pc.defineInt("markerColor","MarkerColor",0,-999) ;
  pc.defineInt("markerStyle","MarkerStyle",0,8) ;
  pc.defineDouble("markerSize","MarkerSize",0,-999) ;
  pc.defineInt("fillColor","FillColor",0,-999) ;
  pc.defineInt("fillStyle","FillStyle",0,-999) ;
  pc.defineInt("histInvisible","Invisible",0,0) ;
  pc.defineDouble("scaleFactor","Rescale",0,1.) ;
  pc.defineObject("xvar","XVar",0,0) ;
  pc.defineObject("yvar","YVar",0,0) ;

  
  // Process & check varargs 
  pc.process(argList) ;
  if (!pc.ok(kTRUE)) {
    return frame ;
  }
  
  // Extract values from named arguments
  const char* drawOptions = pc.getString("drawOption") ;
  Int_t histInvisible = pc.getInt("histInvisible") ;
  const char* histName = pc.getString("histName",0,kTRUE) ;
  Double_t scaleFactor = pc.getDouble("scaleFactor") ;

  RooRealVar* xvar = (RooRealVar*) _vars.find(frame->getPlotVar()->GetName()) ;

  // Determine Y variable (default is weight, if present)
  RooRealVar* yvar = (RooRealVar*)(pc.getObject("yvar")) ;

  // Sanity check. XY plotting only applies to weighted datasets if no YVar is specified
  if (!_wgtVar && !yvar) {
    coutE(InputArguments) << "RooDataSet::plotOnXY(" << GetName() << ") ERROR: no YVar() argument specified and dataset is not weighted" << endl ;
    return 0 ;
  }
  
  RooRealVar* dataY = yvar ? (RooRealVar*) _vars.find(yvar->GetName()) : 0 ;
  if (yvar && !dataY) {
    coutE(InputArguments) << "RooDataSet::plotOnXY(" << GetName() << ") ERROR on YVar() argument, dataset does not contain a variable named " << yvar->GetName() << endl ;
    return 0 ;
  }


  // Make RooHist representing XY contents of data
  RooHist* graph = new RooHist ;
  if (histName) {
    graph->SetName(histName) ;
  } else {
    graph->SetName(Form("hxy_%s",GetName())) ;
  }
  
  for (int i=0 ; i<numEntries() ; i++) {
    get(i) ;
    Double_t x = xvar->getVal() ;
    Double_t exlo = xvar->getErrorLo() ;
    Double_t exhi = xvar->getErrorHi() ;
    Double_t y,eylo,eyhi ;
    if (!dataY) {
      y = weight() ;
      weightError(eylo,eyhi) ;
    } else {
      y = dataY->getVal() ;
      eylo = dataY->getErrorLo() ;
      eyhi = dataY->getErrorHi() ;
    }
    graph->addBinWithXYError(x,y,-1*exlo,exhi,-1*eylo,eyhi,scaleFactor) ;
  }

  // Adjust style options according to named arguments
  Int_t lineColor   = pc.getInt("lineColor") ;
  Int_t lineStyle   = pc.getInt("lineStyle") ;
  Int_t lineWidth   = pc.getInt("lineWidth") ;
  Int_t markerColor = pc.getInt("markerColor") ;
  Int_t markerStyle = pc.getInt("markerStyle") ;
  Size_t markerSize  = pc.getDouble("markerSize") ;
  Int_t fillColor = pc.getInt("fillColor") ;
  Int_t fillStyle = pc.getInt("fillStyle") ;

  if (lineColor!=-999) graph->SetLineColor(lineColor) ;
  if (lineStyle!=-999) graph->SetLineStyle(lineStyle) ;
  if (lineWidth!=-999) graph->SetLineWidth(lineWidth) ;
  if (markerColor!=-999) graph->SetMarkerColor(markerColor) ;
  if (markerStyle!=-999) graph->SetMarkerStyle(markerStyle) ;
  if (markerSize!=-999) graph->SetMarkerSize(markerSize) ;
  if (fillColor!=-999) graph->SetFillColor(fillColor) ;
  if (fillStyle!=-999) graph->SetFillStyle(fillStyle) ;

  // Add graph to frame
  frame->addPlotable(graph,drawOptions,histInvisible) ;
 
  return frame ;
}




//_____________________________________________________________________________
RooDataSet *RooDataSet::read(const char *fileList, const RooArgList &varList,
			     const char *verbOpt, const char* commonPath, 
			     const char* indexCatName) {
  // Read given list of ascii files, and construct a data set, using the given
  // ArgList as structure definition.
  //
  // Multiple file names in fileList should be comma separated. Each
  // file is optionally prefixed with 'commonPath' if such a path is
  // provided
  //
  // The arglist specifies the dimensions of the dataset to be built
  // and describes the order in which these dimensions appear in the
  // ascii files to be read. 
  //
  // Each line in the ascii file should contain N white space separated
  // tokens, with N the number of args in 'variables'. Any text beyond
  // N tokens will be ignored with a warning message.
  // [ NB: This format is written by RooArgList::writeToStream() ]
  // 
  // If the value of any of the variables on a given line exceeds the
  // fit range associated with that dimension, the entire line will be
  // ignored. A warning message is printed in each case, unless the
  // 'Q' verbose option is given. (Option 'D' will provide additional
  // debugging information) The number of events read and skipped
  // is always summarized at the end.
  //
  // When multiple files are read, a RooCategory arg in 'variables' can 
  // optionally be designated to hold information about the source file 
  // of each data point. This feature is enabled by giving the name
  // of the (already existing) category variable in 'indexCatName'
  //
  // If no further information is given a label name 'fileNNN' will
  // be assigned to each event, where NNN is the sequential number of
  // the source file in 'fileList'.
  // 
  // Alternatively it is possible to override the default label names
  // of the index category by specifying them in the fileList string:
  // When instead of "file1.txt,file2.txt" the string 
  // "file1.txt:FOO,file2.txt:BAR" is specified, a state named "FOO"
  // is assigned to the index category for each event originating from
  // file1.txt. The labels FOO,BAR may be predefined in the index 
  // category via defineType(), but don't have to be
  //
  // Finally, one can also assign the same label to multiple files,
  // either by specifying "file1.txt:FOO,file2,txt:FOO,file3.txt:BAR"
  // or "file1.txt,file2.txt:FOO,file3.txt:BAR"
  //

  // Make working copy of variables list 
  RooArgList variables(varList) ;

  // Append blinding state category to variable list if not already there
  Bool_t ownIsBlind(kTRUE) ;
  RooAbsArg* blindState = variables.find("blindState") ;
  if (!blindState) {
    blindState = new RooCategory("blindState","Blinding State") ;
    variables.add(*blindState) ;
  } else {
    ownIsBlind = kFALSE ;    
    if (blindState->IsA()!=RooCategory::Class()) {
      oocoutE((TObject*)0,DataHandling) << "RooDataSet::read: ERROR: variable list already contains" 
			  << "a non-RooCategory blindState member" << endl ;
      return 0 ;
    }
    oocoutW((TObject*)0,DataHandling) << "RooDataSet::read: WARNING: recycling existing "
			<< "blindState category in variable list" << endl ;
  }
  RooCategory* blindCat = (RooCategory*) blindState ;

  // Configure blinding state category
  blindCat->setAttribute("Dynamic") ;
  blindCat->defineType("Normal",0) ;
  blindCat->defineType("Blind",1) ;

  // parse the option string
  TString opts= verbOpt;
  opts.ToLower();
  Bool_t verbose= !opts.Contains("q");
  Bool_t debug= opts.Contains("d");

  RooDataSet *data= new RooDataSet("dataset", fileList, variables);
  if (ownIsBlind) { variables.remove(*blindState) ; delete blindState ; }
  if(!data) {
    oocoutE((TObject*)0,DataHandling) << "RooDataSet::read: unable to create a new dataset"
			<< endl;
    return 0;
  }

  // Redirect blindCat to point to the copy stored in the data set
  blindCat = (RooCategory*) data->_vars.find("blindState") ;

  // Find index category, if requested
  RooCategory *indexCat     = 0;
  //RooCategory *indexCatOrig = 0;
  if (indexCatName) { 
    RooAbsArg* tmp = 0;
    tmp = data->_vars.find(indexCatName) ;
    if (!tmp) {
      oocoutE((TObject*)0,DataHandling) << "RooDataSet::read: no index category named " 
			  << indexCatName << " in supplied variable list" << endl ;
      return 0 ;
    }
    if (tmp->IsA()!=RooCategory::Class()) {
      oocoutE((TObject*)0,DataHandling) << "RooDataSet::read: variable " << indexCatName 
			  << " is not a RooCategory" << endl ;
      return 0 ;
    }
    indexCat = (RooCategory*)tmp ;
    
    // Prevent RooArgSet from attempting to read in indexCat
    indexCat->setAttribute("Dynamic") ;
  }


  Int_t outOfRange(0) ;

  // Make local copy of file list for tokenizing 
  char fileList2[10240] ;
  strlcpy(fileList2,fileList,10240) ;
  
  // Loop over all names in comma separated list
  char *filename = strtok(fileList2,", ") ;
  Int_t fileSeqNum(0) ;
  while (filename) {
    // Determine index category number, if this option is active
    if (indexCat) {

      // Find and detach optional file category name 
      char *catname = strchr(filename,':') ;

      if (catname) {
	// Use user category name if provided
	*catname=0 ;
	catname++ ;

	const RooCatType* type = indexCat->lookupType(catname,kFALSE) ;
	if (type) {
	  // Use existing category index
	  indexCat->setIndex(type->getVal()) ;
	} else {
	  // Register cat name
	  indexCat->defineType(catname,fileSeqNum) ;
	  indexCat->setIndex(fileSeqNum) ;
	}
      } else {
	// Assign autogenerated name
	char newLabel[128] ;
	snprintf(newLabel,128,"file%03d",fileSeqNum) ;
	if (indexCat->defineType(newLabel,fileSeqNum)) {
	  oocoutE((TObject*)0,DataHandling) << "RooDataSet::read: Error, cannot register automatic type name " << newLabel 
			      << " in index category " << indexCat->GetName() << endl ;
	  return 0 ;
	}	
	// Assign new category number
	indexCat->setIndex(fileSeqNum) ;
      }
    }

    oocoutI((TObject*)0,DataHandling) << "RooDataSet::read: reading file " << filename << endl ;

    // Prefix common path 
    TString fullName(commonPath) ;
    fullName.Append(filename) ;
    ifstream file(fullName) ;

    if(!file.good()) {
      oocoutW((TObject*)0,DataHandling) << "RooDataSet::read: unable to open '"
	   << filename << "', skipping" << endl;
    }
    
//  Double_t value;
    Int_t line(0) ;
    Bool_t haveBlindString(false) ;

    while(file.good() && !file.eof()) {
      line++;
      if(debug) oocxcoutD((TObject*)0,DataHandling) << "reading line " << line << endl;

      // process comment lines
      if (file.peek() == '#')
	{
	  if(debug) oocxcoutD((TObject*)0,DataHandling) << "skipping comment on line " << line << endl;	    
	}
      else {	

	// Skip empty lines 
	// if(file.peek() == '\n') { file.get(); }

	// Read single line
	Bool_t readError = variables.readFromStream(file,kTRUE,verbose) ;
	data->_vars = variables ;
// 	Bool_t readError = data->_vars.readFromStream(file,kTRUE,verbose) ;

	// Stop at end of file or on read error
	if(file.eof()) break ;	
	if(!file.good()) {
	  oocoutE((TObject*)0,DataHandling) << "RooDataSet::read(static): read error at line " << line << endl ;
	  break;
	}	

	if (readError) {
	  outOfRange++ ;
	  continue ;
	}
	blindCat->setIndex(haveBlindString) ;
	data->fill(); // store this event
      }
    }

    file.close();

    // get next file name 
    filename = strtok(0," ,") ;
    fileSeqNum++ ;
  }

  if (indexCat) {
    // Copy dynamically defined types from new data set to indexCat in original list
    RooCategory* origIndexCat = (RooCategory*) variables.find(indexCatName) ;
    TIterator* tIter = indexCat->typeIterator() ;
    RooCatType* type = 0;
      while ((type=(RooCatType*)tIter->Next())) {
	origIndexCat->defineType(type->GetName(),type->getVal()) ;
      }
  }
  oocoutI((TObject*)0,DataHandling) << "RooDataSet::read: read " << data->numEntries()
				    << " events (ignored " << outOfRange << " out of range events)" << endl;
  return data;
}




//_____________________________________________________________________________
Bool_t RooDataSet::write(const char* filename)
{
  // Write the contents of this dataset to an ASCII file with the specified name
  // Each event will be written as a single line containing the written values
  // of each observable in the order they were declared in the dataset and
  // separated by whitespaces

  checkInit() ;

  // Open file for writing 
  ofstream ofs(filename) ;
  if (ofs.fail()) {
    coutE(DataHandling) << "RooDataSet::write(" << GetName() << ") cannot create file " << filename << endl ;
    return kTRUE ;
  }

  // Write all lines as arglist in compact mode
  coutI(DataHandling) << "RooDataSet::write(" << GetName() << ") writing ASCII file " << filename << endl ;
  Int_t i ;
  for (i=0 ; i<numEntries() ; i++) {
    RooArgList list(*get(i),"line") ;
    list.writeToStream(ofs,kTRUE) ;
  }

  if (ofs.fail()) {
    coutW(DataHandling) << "RooDataSet::write(" << GetName() << "): WARNING error(s) have occured in writing" << endl ;
  }
  return ofs.fail() ;
}



//_____________________________________________________________________________
void RooDataSet::printMultiline(ostream& os, Int_t contents, Bool_t verbose, TString indent) const 
{
  // Print info about this dataset to the specified output stream.
  //
  //   Standard: number of entries
  //      Shape: list of variables we define & were generated with

  checkInit() ;
  RooAbsData::printMultiline(os,contents,verbose,indent) ;
  if (_wgtVar) {
    os << indent << "  Dataset variable \"" << _wgtVar->GetName() << "\" is interpreted as the event weight" << endl ;
  }
}


//_____________________________________________________________________________
void RooDataSet::printValue(ostream& os) const 
{
  // Print value of the dataset, i.e. the sum of weights contained in the dataset
  os << numEntries() << " entries" ;
  if (isWeighted()) {
    os << " (" << sumEntries() << " weighted)" ;
  }
}



//_____________________________________________________________________________
void RooDataSet::printArgs(ostream& os) const 
{
  // Print argument of dataset, i.e. the observable names

  os << "[" ;    
  TIterator* iter = _varsNoWgt.createIterator() ;
  RooAbsArg* arg ;
  Bool_t first(kTRUE) ;
  while((arg=(RooAbsArg*)iter->Next())) {
    if (first) {
      first=kFALSE ;
    } else {
      os << "," ;
    }
    os << arg->GetName() ;
  }
  if (_wgtVar) {
    os << ",weight:" << _wgtVar->GetName() ;
  }
  os << "]" ;
  delete iter ;
}



//_____________________________________________________________________________
void RooDataSet::SetName(const char *name) 
{
  // Change the name of this dataset into the given name

  if (_dir) _dir->GetList()->Remove(this);
  TNamed::SetName(name) ;
  if (_dir) _dir->GetList()->Add(this);
}


//_____________________________________________________________________________
void RooDataSet::SetNameTitle(const char *name, const char* title) 
{
  // Change the title of this dataset into the given name

  if (_dir) _dir->GetList()->Remove(this);
  TNamed::SetNameTitle(name,title) ;
  if (_dir) _dir->GetList()->Add(this);
}


//______________________________________________________________________________
void RooDataSet::Streamer(TBuffer &R__b)
{
   // Stream an object of class RooDataSet.
   if (R__b.IsReading()) {

     UInt_t R__s, R__c;
     Version_t R__v = R__b.ReadVersion(&R__s, &R__c);

     if (R__v>1) {

       // Use new-style streaming for version >1
       R__b.ReadClassBuffer(RooDataSet::Class(),this,R__v,R__s,R__c);

     } else {

       // Legacy dataset conversion happens here. Legacy RooDataSet inherits from RooTreeData
       // which in turn inherits from RooAbsData. Manually stream RooTreeData contents on 
       // file here and convert it into a RooTreeDataStore which is installed in the 
       // new-style RooAbsData base class

       // --- This is the contents of the streamer code of RooTreeData version 1 ---
       UInt_t R__s1, R__c1;
       Version_t R__v1 = R__b.ReadVersion(&R__s1, &R__c1); if (R__v1) { }
       
       RooAbsData::Streamer(R__b);
       TTree* X_tree(0) ; R__b >> X_tree;
       RooArgSet X_truth ; X_truth.Streamer(R__b);
       TString X_blindString ; X_blindString.Streamer(R__b);
       R__b.CheckByteCount(R__s1, R__c1, RooTreeData::Class());
       // --- End of RooTreeData-v1 streamer
       
       // Construct RooTreeDataStore from X_tree and complete initialization of new-style RooAbsData
       _dstore = new RooTreeDataStore(X_tree,_vars) ;
       _dstore->SetName(GetName()) ;
       _dstore->SetTitle(GetTitle()) ;
       _dstore->checkInit() ;       

       // This is the contents of the streamer code of RooDataSet version 1
       RooDirItem::Streamer(R__b);
       _varsNoWgt.Streamer(R__b);
       R__b >> _wgtVar;
       R__b.CheckByteCount(R__s, R__c, RooDataSet::IsA());

       
     }
   } else {
      R__b.WriteClassBuffer(RooDataSet::Class(),this);
   }
}

