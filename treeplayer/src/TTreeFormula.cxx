// @(#)root/treeplayer:$Name:  $:$Id: TTreeFormula.cxx,v 1.45 2001/05/31 08:56:32 brun Exp $
// Author: Rene Brun   19/01/96

/*************************************************************************
 * Copyright (C) 1995-2000, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include "TROOT.h"
#include "TTreeFormula.h"
#include "TTree.h"
#include "TBranch.h"
#include "TFunction.h"
#include "TLeafC.h"
#include "TLeafObject.h"
#include "TDataMember.h"
#include "TMethodCall.h"
#include "TCutG.h"
#include "TRandom.h"
#include "TInterpreter.h"
#include "TDataType.h"
#include "TStreamerInfo.h"
#include "TStreamerElement.h"
#include "TBranchElement.h"
#include "TLeafElement.h"
#include "TArrayI.h"

#include <stdio.h>
#include <math.h>

const Int_t kMETHOD   = 1000;
const Int_t kDATAMEMBER = 1000;
const Int_t kMaxLen     = 512;

ClassImp(TTreeFormula)

//______________________________________________________________________________
//
// This class is a small helper class to implement reading a data member
// on an object stored in a TTree. 

class TFormLeafInfo : public TObject {
public:
   TFormLeafInfo(TClass* classptr = 0, Long_t offset = 0, 
                 TStreamerElement* element = 0) :
     fClass(classptr),fOffset(offset),fElement(element),
     fCounter(0), fNext(0) {
      if (fClass) fInfo = fClass->GetStreamerInfo();
      else fInfo = 0;
   };
   virtual ~TFormLeafInfo() { delete fCounter; delete fNext; };
   TClass           *fClass;
   TStreamerInfo    *fInfo;
   Long_t            fOffset;
   TStreamerElement *fElement;
   TFormLeafInfo    *fCounter;
   TFormLeafInfo    *fNext;

   virtual Int_t     GetCounterValue(TLeaf* leaf);
   virtual Double_t  ReadValue(char *where, Int_t instance = 0);
   virtual Double_t  GetValue(TLeaf *leaf, Int_t instance = 0);
   virtual void     *GetValuePointer(TLeaf *leaf, Int_t instance = 0);

   inline char* GetObjectAddress(TLeafElement* leaf) {
      char* thisobj = 0;
      TBranchElement * branch = (TBranchElement*)((TLeafElement*)leaf)->GetBranch();
      TStreamerInfo * info = branch->GetInfo();
      Int_t id = branch->GetID();
      Int_t offset = (id<0)?0:info->GetOffsets()[id]; 
      char* address = (char*)branch->GetAddress();
      if (address) {
         Int_t type = (id<0)?0:info->GetTypes()[id];
         switch (type) {
         case TStreamerInfo::kOffsetL + TStreamerInfo::kObjectp:
         case TStreamerInfo::kOffsetL + TStreamerInfo::kObjectP:
           Error("GetValuePointer","Type (%d) not yet supported\n",type);
           break;
         case TStreamerInfo::kObject: 
         case TStreamerInfo::kTString: 
         case TStreamerInfo::kTNamed: 
         case TStreamerInfo::kTObject: 
         case TStreamerInfo::kAny:
           thisobj = (char*)(address+offset);
           break;
         default:
           thisobj = (char*) *(void**)(address+offset);
         }
      } else thisobj = branch->GetObject();
      return thisobj;
   }
};

//______________________________________________________________________________
Int_t TFormLeafInfo::GetCounterValue(TLeaf* leaf) {
//*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
//*-*
//  Return the size of the underlying array for the current entry in the TTree.

   if (!fCounter) return 1;
   return (Int_t)fCounter->GetValue(leaf);
}

//
// This class is a small helper class to implement reading a data member
// on an object stored in a TTree. 

class TFormLeafInfoDirect : public TFormLeafInfo {
public:
   TFormLeafInfoDirect() :
     TFormLeafInfo(0,0,0) {
   };
   virtual ~TFormLeafInfoDirect() { };

   virtual Double_t  ReadValue(char *where, Int_t instance = 0) {
      Error("ReadValue","Should not be used in a TFormLeafInfoDirect");
      return 0;
   } 
   virtual Double_t  GetValue(TLeaf *leaf, Int_t instance = 0) {
      return leaf->GetValue(instance);
   }
   virtual void     *GetValuePointer(TLeaf *leaf, Int_t instance = 0) {
      if (leaf->IsA() != TLeafElement::Class()) {
         return leaf->GetValuePointer();
      } else {
         return GetObjectAddress((TLeafElement*)leaf);
      }
   }

};

//______________________________________________________________________________
//
// This class is a small helper class to implement reading a data member
// on a TClonesArray object stored in a TTree. 

class TFormLeafInfoClones : public TFormLeafInfo {
public:
   TFormLeafInfoClones(TClass* classptr = 0, Long_t offset = 0, 
                       TStreamerElement* element = 0) :
     TFormLeafInfo(classptr,offset,element) {};
   virtual Int_t     GetCounterValue(TLeaf* leaf);
   virtual Double_t  ReadValue(char *where, Int_t instance = 0);
   virtual Double_t  GetValue(TLeaf *leaf, Int_t instance = 0);
};

//______________________________________________________________________________
Int_t TFormLeafInfoClones::GetCounterValue(TLeaf* leaf) {
   // Return the current size of the the TClonesArray

   if (!fCounter) return 1;
   return (Int_t)fCounter->ReadValue((char*)GetValuePointer(leaf,0)) + 1;
}
//______________________________________________________________________________
Double_t TFormLeafInfoClones::ReadValue(char *where, Int_t instance) { 
   // Return the value of the underlying data member inside the 
   // clones array.
   
   if (fNext==0) return 0;
   Int_t len,index,sub_instance;
   len = fNext->fElement->GetArrayLength();
   if (len) {
      index = instance / len;
      sub_instance = instance % len;
   } else {
      index = instance;
      sub_instance = 0;
   }
   TClonesArray * clones = (TClonesArray*)where;
   // Note we take advantage of having only one physically variable
   // dimension:
   char * obj = (char*)clones->UncheckedAt(index);
   return fNext->ReadValue(obj,sub_instance);
}

//______________________________________________________________________________
Double_t TFormLeafInfoClones::GetValue(TLeaf *leaf, Int_t instance) { 
   // Return the value of the underlying data member inside the 
   // clones array.

   if (fNext==0) return 0;
   Int_t len,index,sub_instance;
   len = fNext->fElement->GetArrayLength();
   if (len) {
      index = instance / len;
      sub_instance = instance % len;
   } else {
      index = instance;
      sub_instance = 0;
   }
   TClonesArray * clones = (TClonesArray*)GetValuePointer(leaf);
   // Note we take advantage of having only one physically variable
   // dimension:
   char * obj = (char*)clones->UncheckedAt(index);
   return fNext->ReadValue(obj,sub_instance);
}

//______________________________________________________________________________
//
// This class is a small helper class to implement reading a data member
// on a TClonesArray object stored in a TTree. 

class TFormLeafInfoPointer : public TFormLeafInfo {
public:
   TFormLeafInfoPointer(TClass* classptr = 0, Long_t offset = 0, 
                        TStreamerElement* element = 0) :
      TFormLeafInfo(classptr,offset,element) { };

   virtual Double_t  ReadValue(char *where, Int_t instance = 0) { 
      // Return the value of the underlying pointer data member 

      if (!fNext) return 0;
      char * whereoffset = where+fOffset;
      switch (fElement->GetType()) {
         // basic types
         case TStreamerInfo::kObjectp: 
         case TStreamerInfo::kObjectP: 
         {TObject **obj = (TObject**)(whereoffset);    
          return fNext->ReadValue((char*)*obj,instance); }

         case TStreamerInfo::kObject: 
         case TStreamerInfo::kTString: 
         case TStreamerInfo::kTNamed: 
         case TStreamerInfo::kTObject: 
         case TStreamerInfo::kOffsetL + TStreamerInfo::kObjectp:
         case TStreamerInfo::kOffsetL + TStreamerInfo::kObjectP:
         case TStreamerInfo::kAny:
           {TObject *obj = (TObject*)(whereoffset);   
            return fNext->ReadValue((char*)obj,instance); }
        
         case kOther_t:  
         default:        return 0;
      }
     
   } ;
   virtual Double_t  GetValue(TLeaf *leaf, Int_t instance = 0) {
      // Return the value of the underlying pointer data member  

      if (!fNext) return 0;
      char * where = (char*)GetValuePointer(leaf,instance);
      return fNext->ReadValue(where,instance);
   };
};

//______________________________________________________________________________
//
// This class is a small helper class to implement reading a data member
// on a TClonesArray object stored in a TTree. 

class TFormLeafInfoMethod : public TFormLeafInfo {
public:
   TMethodCall *fMethod;
   
   TFormLeafInfoMethod(TClass* classptr = 0, TMethodCall *method = 0) :
     TFormLeafInfo(classptr,0,0),fMethod(method) {};
  
   virtual Double_t  ReadValue(char *where, Int_t instance = 0) {
      // Execute the method on the given address 

      void *thisobj = where;
      TMethodCall::EReturnType r = fMethod->ReturnType();

      if (r == TMethodCall::kLong) {
         Long_t l;
         fMethod->Execute(thisobj, l);
         return (Double_t) l;
      }
      if (r == TMethodCall::kDouble) {
         Double_t d;
         fMethod->Execute(thisobj, d);
         return (Double_t) d;
      }
      if (fNext) {
        char * result = 0;
        fMethod->Execute(thisobj, &result);
        return fNext->ReadValue(result,instance);
      } else fMethod->Execute(thisobj);

      return 0;
   }
};

//______________________________________________________________________________
//
// This class is a small helper class to implement reading a data member
// on a variable size array inside a TClonesArray object stored in a TTree. 

class TFormLeafInfoMultiVarDim : public TFormLeafInfo {
public:
  Int_t fNsize;
  TArrayI fSizes;           // Array of sizes of the variable dimension
  TFormLeafInfo *fCounter2; // Information on how to read the secondary dimensions
  Int_t fSumOfSizes;        // Sum of the content of fSizes
  Int_t fDim;               // physical number of the dimension that is variable
  Int_t fVirtDim;           // number of the virtual dimension to which this object correspond.
  Int_t fPrimaryIndex;      // Index of the dimensions that is indexing the second dimension's size

  TFormLeafInfoMultiVarDim() :
     TFormLeafInfo(0,0,0),fNsize(0),fCounter2(0),fSumOfSizes(0),
     fDim(0),fVirtDim(-1),fPrimaryIndex(0)
  {
  }

  virtual Double_t  GetValue(TLeaf *leaf, Int_t instance = 0) {
     return ((TLeafElement*)leaf)->GetValueSubArray(fPrimaryIndex,instance);
     //char * where;
     //if (leaf->InheritsFrom("TLeafObject") ) {
     //   where = (char*)((TLeafObject*)leaf)->GetObject();
     //} else {
     //   where = GetObjectAddress((TLeafElement*)leaf);
     //}
     //return ReadValue(where,instance);  
  }
   
  virtual Double_t  ReadValue(char *where, Int_t instance = 0) {
     // Return a dummy value for now
     //    fprintf(stderr,"Primary Index is %d Instance is %d\n",fPrimaryIndex,instance);
    // TClonesArray* clones = (TClonesArray*)where;
     // return br->GetInfo()->GetValueClones(clones,br->GetID(),fPrimaryIndex,instance,br->fOffset)
     return (double)(int)where;
  }

  virtual void LoadSizes(TBranchElement* branch) {
     if (!fCounter2 || !fCounter) return;
     fNsize =((TBranchElement*) branch->GetBranchCount())->GetNdata();
     if (fNsize > fSizes.GetSize()) fSizes.Set(fNsize);
     fSumOfSizes = 0;
     for (Int_t i=0; i<fNsize; i++) {
        Int_t size = (Int_t)fCounter2->GetValue((TLeaf*)branch->GetBranchCount2()->GetListOfLeaves()->At(0),i);
        fSumOfSizes += size;
        fSizes.AddAt( size, i );
     }
  }
  
  virtual Int_t GetSize(Int_t index) {
     return fSizes.At(index);
  }

  virtual void SetSize(Int_t index, Int_t val) {
     fSumOfSizes += (val - fSizes.At(index));
     fSizes.AddAt(val,index);
  }

  virtual void UpdateSizes(TArrayI *garr) {
     if (!garr) return;
     if (garr->GetSize()<fNsize) garr->Set(fNsize);
     for (Int_t i=0; i<fNsize; i++) {
        Int_t local = fSizes.At(i);
        Int_t global = garr->At(i);
        if (global==0 || (local!=0 && local<global)) global = local;
        garr->AddAt(local,i);
     }
  }

  virtual void SetPrimaryIndex(Int_t index) { 
     fPrimaryIndex = index;
  }

};
      
//______________________________________________________________________________
//
//     A TreeFormula is used to pass a selection expression
//     to the Tree drawing routine. See TTree::Draw
//
//  A TreeFormula can contain any arithmetic expression including
//  standard operators and mathematical functions separated by operators.
//  Examples of valid expression:
//          "x<y && sqrt(z)>3.2"
//

//______________________________________________________________________________
TTreeFormula::TTreeFormula(): TFormula(),fMultiVarDim(kFALSE)
{
//*-*-*-*-*-*-*-*-*-*-*Tree Formula default constructor*-*-*-*-*-*-*-*-*-*
//*-*                  ================================

   fTree       = 0;
   fLookupType = 0;
   fNindex     = 0;
   fNcodes     = 0;
}

//______________________________________________________________________________
TTreeFormula::TTreeFormula(const char *name,const char *expression, TTree *tree)
  :TFormula(),fMultiVarDim(kFALSE),fCumulUsedVarDims(0)
{
//*-*-*-*-*-*-*-*-*-*-*Normal Tree Formula constructor*-*-*-*-*-*-*-*-*-*-*
//*-*                  ===============================
//

   fTree         = tree;
   fNindex       = kMAXFOUND;
   fLookupType   = new Int_t[fNindex];
   fNcodes       = 0;
   fMultiplicity = 0;
   Int_t i,j,k;

   for (j=0; j<kMAXCODES; j++) {
      fNdimensions[j] = 0;
      fLookupType[j] = kDirect;
      fNdata[j] = 1;
      for (k = 0; k<kMAXFORMDIM; k++) {
         fIndexes[j][k] = -1;
         fCumulSizes[j][k] = 1;
         fVarIndexes[j][k] = 0;
      }
   }
   for (k = 0; k<kMAXFORMDIM+1; k++) {
      fCumulUsedSizes[k] = 1;
      fUsedSizes[k] = 1;
      fVirtUsedSizes[k] = 1;
      fVarDims[k] = 0;
   }

   if (Compile(expression)) {fTree = 0; return; }
   if (fNcodes >= kMAXFOUND) {
      Warning("TTreeFormula","Too many items in expression:%s",expression);
      fNcodes = kMAXFOUND;
   }
   SetName(name);
   for (i=0;i<fNcodes;i++) {
      if (fCodes[i] < 0) continue;
      //TLeaf *leaf = GetLeaf(i);
      TLeaf *leaf = (TLeaf*)fLeaves.UncheckedAt(i);
      if (leaf->InheritsFrom("TLeafC") && !leaf->IsUnsigned()) SetBit(kIsCharacter);
      if (leaf->InheritsFrom("TLeafB") && !leaf->IsUnsigned()) SetBit(kIsCharacter);
      
      // Reminder of the meaning of fMultiplicity:
      //  -1: Only one or 0 element per entry but contains variable length array!
      //   0: Only one element per entry, no variable length array
      //   1: loop over the elements of a variable length array
      //   2: loop over elements of fixed length array (nData is the same for all entry)
      
      if (leaf->GetLeafCount()) {
         // We assume only one possible variable length dimension (the left most)
         fMultiplicity = 1;
      } else if (fLookupType[i]==kDataMember) {
         TFormLeafInfo * leafinfo = (TFormLeafInfo*)GetLeafInfo(i);
         TStreamerElement * elem = ((TFormLeafInfo*)GetLeafInfo(i))->fElement;
         if (fMultiplicity!=1) {
            if (leafinfo->fCounter) fMultiplicity = 1;
            else if (elem && elem->GetArrayDim()>0) fMultiplicity = 2;
         }
      } else {
         if (leaf->GetLenStatic()>1 && fMultiplicity!=1) fMultiplicity = 2;
      }
      
      Int_t virt_dim = 0;
      for (k = 0; k < fNdimensions[i]; k++) {
         // At this point fCumulSizes[i][k] actually contain the physical
         // dimension of the k-th dimensions. 
         if ( (fCumulSizes[i][k]>=0) && (fIndexes[i][k] >= fCumulSizes[i][k]) ) {
            // unreacheable element requested:
            fCumulUsedSizes[virt_dim] = 0;
         }
         if ( fIndexes[i][k] < 0 ) virt_dim++;
         fFixedSizes[i][k] = fCumulSizes[i][k];
      }
      // Add up the cumulative size
      for (k = fNdimensions[i]; (k > 0); k--) {
         // NOTE: When support for inside variable dimension is added this
         // will become inacurate (since one of the value in the middle of the chain
         // is unknown until GetNdata is called.
         fCumulSizes[i][k-1] *= TMath::Abs(fCumulSizes[i][k]);
      }
      // NOTE: We assume that the inside variable dimensions are dictated by the
      // first index.
      if (fCumulSizes[i][0]>0) fNdata[i] = fCumulSizes[i][0];
   }
   
   // For here we keep fCumulUsedSizes sign aware.
   // This will be reset properly (if needed) by GetNdata.
   fCumulUsedSizes[kMAXFORMDIM] = fUsedSizes[kMAXFORMDIM];
   for (k = kMAXFORMDIM; (k > 0) ; k--) { 
      if (fUsedSizes[k-1]>=0) {
         fCumulUsedSizes[k-1] = fUsedSizes[k-1] * fCumulUsedSizes[k];
      } else {
         fCumulUsedSizes[k-1] = - TMath::Abs(fCumulUsedSizes[k]);
      }
   }

   // Now that we know the virtual dimension we know if a loop over EvalInstance
   // is needed or not.
   if (fCumulUsedSizes[0]==1 && fMultiplicity!=0) {
      // Case where even though we have an array.  We know that they will always
      // only be one element.
      fMultiplicity -= 2;
   } else if (fCumulUsedSizes[0]<0 && fMultiplicity==2) {
      // Case of a fixed length array that have one of its indices given
      // by a variable. 
      fMultiplicity = 1; 
   }

}

//______________________________________________________________________________
TTreeFormula::~TTreeFormula()
{
//*-*-*-*-*-*-*-*-*-*-*Tree Formula default destructor*-*-*-*-*-*-*-*-*-*-*
//*-*                  =================================

   fNames.Delete();
   fDataMembers.Delete();
   if (fLookupType) delete [] fLookupType;
   for (int j=0; j<fNcodes; j++) {
      for (int k = 0; k<fNdimensions[j]; k++) {
         if (fVarIndexes[j][k]) delete fVarIndexes[j][k];
         fVarIndexes[j][k] = 0;
      }
   }
   for (int l = 0; l<kMAXFORMDIM; l++) {
      if (fVarDims[l]) delete fVarDims[l];
      fVarDims[l] = 0;
   }
   if (fCumulUsedVarDims) delete fCumulUsedVarDims;

}

//______________________________________________________________________________
void TTreeFormula::DefineDimensions(const char *info, Int_t code, Int_t& virt_dim) {
  // This method is used internally to decode the dimensions of the variables

   // We assume that there are NO white spaces in the info string
   const char * current;
   Int_t size, scanindex;
   
   current = info;
   // the next value could be before the string but
   // that's okay because the next operation is ++
   // (this is to avoid (?) a if statement at the end of the
   // loop)
   if (current[0] != '[') current--;
   while (current) {
      current++;
      scanindex = sscanf(current,"%d",&size);
      // if scanindex is 0 then we have a name index thus a variable
      // array (or TClonesArray!).
      
      if (scanindex==0) size = -1;
      
      DefineDimensions(code, size, virt_dim);

      if (fNdimensions[code] >= kMAXFORMDIM) {
         // NOTE: test that fNdimensions[code] is NOT too big!!

         break;
      }
      current = (char*)strstr( current, "[" );
   }
   
}

//______________________________________________________________________________
void TTreeFormula::DefineDimensions(Int_t code, Int_t size, Int_t& virt_dim) {
   // This method is used internally to decode the dimensions of the variables

   Int_t vsize = 0;
   
   if (fIndexes[code][fNdimensions[code]]==-2) {
      TTreeFormula *indexvar = fVarIndexes[code][fNdimensions[code]];
      // ASSERT(indexvar!=0);
      Int_t index_multiplicity = indexvar->GetMultiplicity();
      switch (index_multiplicity) {
         case -1: 
         case  0:
         case  2: 
            vsize = indexvar->GetNdata();
            break;
         case  1:
            vsize = -1;
            break;
      };
   } else vsize = size;

   fCumulSizes[code][fNdimensions[code]] = size;

   if ( fIndexes[code][fNdimensions[code]] < 0 ) {
      if (vsize<0) 
         fVirtUsedSizes[virt_dim] = -1 * TMath::Abs(fVirtUsedSizes[virt_dim]);
      else
         if ( TMath::Abs(fVirtUsedSizes[virt_dim])==1
              || (vsize < TMath::Abs(fVirtUsedSizes[virt_dim]) ) ) {
            // Absolute values represent the min of all real dimensions
            // that are known.  The fact that it is negatif indicates
            // that one of the leaf has a variable size for this 
            // dimensions.
            if (fVirtUsedSizes[virt_dim] < 0) {
               fVirtUsedSizes[virt_dim] = -1 * vsize;
            } else {
               fVirtUsedSizes[virt_dim] = vsize;
            }
         }
      fUsedSizes[virt_dim] = fVirtUsedSizes[virt_dim];
      virt_dim++;
   }

   fNdimensions[code] ++;

}

//______________________________________________________________________________
void TTreeFormula::DefineDimensions(Int_t code, TFormLeafInfo *leafinfo,  Int_t& virt_dim) {
   // This method is used internally to decode the dimensions of the variables

   Int_t ndim, size, current;

   const TStreamerElement * elem = leafinfo->fElement;

   if (elem->IsA() == TStreamerBasicPointer::Class()) {

      ndim = 1;
      size = -1;

      TStreamerBasicPointer *array = (TStreamerBasicPointer*)elem;
      TClass * cl = leafinfo->fClass;
      Int_t offset;
      TStreamerElement* counter = cl->GetStreamerInfo()->GetStreamerElement(array->GetCountName(),offset);
      leafinfo->fCounter = new TFormLeafInfo(cl,offset,counter);

   } else if (elem->GetClassPointer() == TClonesArray::Class() ) {

      ndim = 1;
      size = -1;
      
      TClass * ClonesClass = TClonesArray::Class();
      Int_t c_offset;
      TStreamerElement *counter = ClonesClass->GetStreamerInfo()->GetStreamerElement("fLast",c_offset);
      leafinfo->fCounter = new TFormLeafInfo(ClonesClass,c_offset,counter);

   } else if (elem->GetArrayDim()>0) {

      ndim = elem->GetArrayDim();
      size = elem->GetMaxIndex(0);

   } else return;

   current = 0;
   do {
      DefineDimensions(code, size, virt_dim);

      if (fNdimensions[code] >= kMAXFORMDIM) {
         // NOTE: test that fNdimensions[code] is NOT too big!!

         break;
      }
      current++;
      size = elem->GetMaxIndex(current);
   } while (current<ndim);

}

//______________________________________________________________________________
void TTreeFormula::DefineDimensions(Int_t code, TBranchElement *branch,  Int_t& virt_dim) {
   // This method is used internally to decode the dimensions of the variables

   TBranchElement * leafcount2 = branch->GetBranchCount2();
   if (leafcount2) {
      // With have a second variable dimensions  
      fMultiVarDim = kTRUE;
      if (!fCumulUsedVarDims) fCumulUsedVarDims = new TArrayI;
      TFormLeafInfoMultiVarDim * info;
      info = (TFormLeafInfoMultiVarDim *)fDataMembers.At(code);
      info->fCounter = new TFormLeafInfoDirect();
      info->fCounter2 = new TFormLeafInfoDirect();
      info->fDim = fNdimensions[code];
      if (fIndexes[code][info->fDim]<0) {
        info->fVirtDim = virt_dim;
        if (!fVarDims[virt_dim]) fVarDims[virt_dim] = new TArrayI;
      } 
      DefineDimensions(code,-1,virt_dim);      
   }
}
   
//______________________________________________________________________________
Int_t TTreeFormula::DefinedVariable(TString &name)
{
//*-*-*-*-*-*Check if name is in the list of Tree/Branch leaves*-*-*-*-*
//*-*        ==================================================
//
//   This member function redefines the function in TFormula
//   If a leaf has a name corresponding to the argument name, then
//   returns a new code.
//   A TTreeFormula may contain more than one variable.
//   For each variable referenced, the pointers to the corresponding
//   branch and leaf is stored in the object arrays fBranches and fLeaves.
//
//   name can be :
//      - Leaf_Name (simple variable or data member of a ClonesArray)
//      - Branch_Name.Leaf_Name
//      - Branch_Name.Method_Name
//      - Leaf_Name[index]
//      - Branch_Name.Leaf_Name[index]
//      - Branch_Name.Leaf_Name[index1]
//      - Branch_Name.Leaf_Name[][index2]
//      - Branch_Name.Leaf_Name[index1][index2]
//   New additions:
//      - Branch_Name.Leaf_Name[OtherLeaf_Name]
//      - Branch_Name.Datamember_Name
//      - '.' can be replaced by '->' 
//   and
//      - Branch_Name[index1].Leaf_Name[index2]
//      - Leaf_name[index].Action().OtherAction(param)
//      - Leaf_name[index].Action()[val].OtherAction(param)

   if (!fTree) return -1;
   // Later on we will need to read one entry, let's make sure 
   // it is a real entry.
   Int_t readentry = fTree->GetReadEntry();
   if (readentry==-1) readentry=0;
   fNpar = 0;
   Int_t nchname = name.Length();
   if (nchname > kMaxLen) return -1;
   Int_t i,k;

   const char *cname = name.Data();

   char   first[kMaxLen];  first[0] = '\0';
   char  second[kMaxLen]; second[0] = '\0';
   char   right[kMaxLen];  right[0] = '\0';
   char    dims[kMaxLen];   dims[0] = '\0';
   char    work[kMaxLen];   work[0] = '\0';
   char scratch[kMaxLen];
   char *current;

   TLeaf *leaf=0, *tmp_leaf=0;
   TBranch *branch=0, *tmp_branch=0;

   Bool_t final = kFALSE;
   Bool_t ismethod = kFALSE;
   for (i=0, current = &(work[0]); i<=nchname && !final;i++ ) { 
      // We will treated the terminator as a token.
      *current++ = cname[i];
      
      // Check if we have the beginning of a function call
      if (cname[i] == '(') {
         // Right now we do not allow nested paranthesis
         i++;
         while( cname[i]!=')' && cname[i] ) {
            *current++ = cname[i++];
         }
         *current++ = cname[i++];
         *current='\0';
         char *params = strchr(work,'(');
         if (params) { 
            *params = 0; params++; 
         } else params = ")";
            
         // What we have so far might be a member function of one of the
         // leaves that are not splitted (for example "GetNtrack" for the Event class.
         TClass * cl = 0;
         TIter next (fTree->GetListOfLeaves());
         TLeaf *leafcur;
         while (!leaf && (leafcur = (TLeaf*)next())) {
            // The following code is used somewhere else, we need to factor it out.
            if (leafcur->InheritsFrom("TLeafObject") ) {
               TLeafObject *lobj = (TLeafObject*)leafcur;
               cl = lobj->GetClass();
            } else if (leafcur->InheritsFrom("TLeafElement") ) {
               TBranchElement *BranchEl = (TBranchElement *)leafcur->GetBranch();
               Int_t type = BranchEl->GetStreamerType();
               if (type==-1) {
                  cl =  BranchEl->GetInfo()->GetClass();
               } else if (type>60) {
                  // Case of an object data member.  Here we allow for the 
                  // variable name to be ommitted.  Eg, for Event.root with split 
                  // level 1 or above  Draw("GetXaxis") is the same as Draw("fH.GetXaxis()")
                  cl =  BranchEl->GetInfo()->GetClass();
                  TStreamerElement* element = (TStreamerElement*)
                    cl->GetStreamerInfo()->GetElements()->At(BranchEl->GetID());
                  cl = element->GetClassPointer();
               }
            }
            if (cl && cl->GetClassInfo() && cl->GetMethodAllAny(work)) {
               // Let's try to see if the function we found belongs to the current
               // class.  Note that this implementation currently can not work if
               // one the argument is another leaf or data member of the object.
               // (Anyway we do NOT support this case).
               TMethodCall *method = new TMethodCall(cl, work, params);
               if (method->GetMethod()) {
                  // We have a method that works.
                  // We will use it.
                  leaf = leafcur;
                  ismethod = kTRUE;
               }
               delete method;
            }
            cl = 0;
         }
         if (!leaf) {
            // This actually not really any error, we probably received something
            // like "abs(some_val)", let TFormula decompose it first.
            return -1;
         }
         //         if (!leaf->InheritsFrom("TLeafObject") ) {
         // If the leaf that we found so far is not a TLeafObject then there is 
            // nothing we would be able to do.
         //   Error("TTreeFormula::DefinedVariable","Need a TLeafObject to call a function!");
         // return -1;
         //}
         // We need to recover the info not used.
         strcpy(right,work);
         strcat(right,"(");
         strcat(right,params);
         //         i++;
         final = kTRUE;
         break;
      }
      if (cname[i] == '.' || cname[i] == '[' || cname[i] == '\0' ) {
         // A delimiter happened let's see if what we have seen
         // so far does point to a leaf.
         
         *current = '\0';
         if (!leaf && !branch) {
            // So far, we have not found a matching leaf or branch. 
            strcpy(first,work);
            
            branch = fTree->FindBranch(first);
            leaf = fTree->FindLeaf(first);
            
            // Look with the delimiter removed (we look with it first
            // because a dot is allowed at the end of some branches).
            if (cname[i]) first[strlen(first)-1]='\0'; 
            if (!branch) branch = fTree->FindBranch(first);
            if (!leaf) leaf = fTree->FindLeaf(first); 
            
            if (leaf || branch) {
               if (leaf && leaf->IsOnTerminalBranch() ) {
                  // This is a non-object leaf, it should NOT be specified more except for
                  // dimensions. 
                  final = kTRUE;
               }
               // we reset work
               current = &(work[0]);
            } else {
               // What we have so far might be a data member of one of the
               // leaves that are not splitted (for example "fNtrack" for the Event class.
               TClass * cl = 0;
               TIter next (fTree->GetListOfLeaves());
               TLeaf *leafcur;
               while ((leafcur = (TLeaf*)next())) {
                  // The following code is used somewhere else, we need to factor it out.
                  if (leafcur->InheritsFrom("TLeafObject") && 
                      leafcur->GetBranch()->GetListOfBranches()->Last()==0) {
                     TLeafObject *lobj = (TLeafObject*)leafcur;
                     cl = lobj->GetClass();
                  } else if (leafcur->InheritsFrom("TLeafElement") && leafcur->IsOnTerminalBranch()) {
                     TLeafElement * lElem = (TLeafElement*) leafcur;
                     if (lElem->IsOnTerminalBranch()) {
                        TBranchElement *BranchEl = (TBranchElement *)leafcur->GetBranch();
                        Int_t type = BranchEl->GetStreamerType();
                        if (type==-1) {
                           cl =  BranchEl->GetInfo()->GetClass();
                        } else if (type>60) {
                           cl =  BranchEl->GetInfo()->GetClass();
                           TStreamerElement* element = (TStreamerElement*)
                             cl->GetStreamerInfo()->GetElements()->At(BranchEl->GetID());
                           cl = element->GetClassPointer();
                        }
                     }
                  }
                  if (cl ==  TClonesArray::Class()) {
                     // we have a unsplit TClonesArray leaves
                     TStreamerElement * begin = new TStreamerElement("begin","fake",0,
                                                                     TStreamerInfo::kAny,"TClonesArray");
                     TFormLeafInfo* clonesinfo = new TFormLeafInfoClones(cl, 0, begin);
                     leafcur->GetBranch()->GetEntry(readentry);
                     TClonesArray * clones = (TClonesArray*)clonesinfo->GetValuePointer(leafcur,0);
                     cl = clones->GetClass();
                     delete clonesinfo;
                  }
                  if (cl) {
                     Int_t offset;
                     TStreamerInfo* info =  cl->GetStreamerInfo();
                     TStreamerElement* element = info?info->GetStreamerElement(first,offset):0;
                     if (!element) {
                        TIter next( cl->GetStreamerInfo()->GetElements() );
                        TStreamerElement * curelem;
                        while ((curelem = (TStreamerElement*)next())) {
                           if (curelem->GetClassPointer() ==  TClonesArray::Class()) {
                              Int_t clones_offset;
                              cl->GetStreamerInfo()->GetStreamerElement(curelem->GetName(),clones_offset);
                              TFormLeafInfo* clonesinfo = new TFormLeafInfo(cl, clones_offset, curelem);
                              TBranch *branch = leafcur->GetBranch();
                              branch->GetEntry(readentry);
                              TClonesArray * clones = (TClonesArray*)clonesinfo->GetValuePointer(leafcur,0);
                              delete clonesinfo;
                              TClass *sub_cl = clones->GetClass();
                              element = sub_cl->GetStreamerInfo()->GetStreamerElement(work,offset);
                              if (element) break;
                           } // if clones array
                        } // loop on elements
                     } 
                     if (element) break; 
                     else cl = 0;
                  }
               }
               if (cl) {
                  leaf = leafcur;
                  branch = leaf->GetBranch();
                  if (leaf->IsOnTerminalBranch()) {
                     final = kTRUE;
                     strcpy(right,first);
                     //We need to put the delimiter back!
                     if (cname[i]=='.') strcat(right,".");
                  };
               }
            }
         } else {
            if (final) {
               Error("TTreeFormula::DefinedVariable",
                     "Unexpected control flow!");
               return -1;
            }
            
            // No dot is allowed in subbranches and leaves, so
            // we always remove it in the present case. 
            if (cname[i]) work[strlen(work)-1] = '\0';
            sprintf(scratch,"%s.%s",first,work);
            
            if (branch) {
               tmp_leaf = branch->FindLeaf(work);
               if (!tmp_leaf)  tmp_leaf = branch->FindLeaf(scratch);
            }
            if (tmp_leaf && tmp_leaf->IsOnTerminalBranch() ) {
               // This is a non-object leaf, it should NOT be specified more except for
               // dimensions. 
               final = kTRUE;
            } 
            
            if (branch) {
               tmp_branch = branch->FindBranch(work);
               if (!tmp_branch) tmp_branch = branch->FindBranch(scratch);
            }
            if (tmp_branch) {
               branch=tmp_branch;
               
               // NOTE: Should we look for a leaf within here?
               if (!final) {
                  tmp_leaf = branch->FindLeaf(work);
                  if (!tmp_leaf)  tmp_leaf = branch->FindLeaf(scratch);
                  if (tmp_leaf && tmp_leaf->IsOnTerminalBranch() ) {
                     // This is a non-object leaf, it should NOT be specified more except for
                     // dimensions. 
                     final = kTRUE;
                     leaf = tmp_leaf;
                  } 
               }
            }
            if (tmp_leaf) {
               // Something was found.
               if (second[0]) strcat(second,".");
               strcat(second,work);
               current = &(work[0]);
               leaf = tmp_leaf;
            } else {
               //We need to put the delimiter back!
               work[strlen(work)] = cname[i];
            }             
         }
      }
      if (cname[i] == '[') {
         int bracket = i;
         int bracket_level = 1;
         int j;
         for (j=++i; j<nchname && (bracket_level>0 || cname[j]=='['); j++, i++) {
            if (cname[j]=='[') 
               bracket_level++;
            else if (cname[j]==']') 
               bracket_level--;
         }
         if (bracket_level != 0) {
            //Error("TTreeFormula::DefinedVariable","Bracket unbalanced");
            return -1;
         }
         strncat(dims,&cname[bracket],j-bracket);
         if (current!=work) *(--current) = '\0'; // remove bracket.
         --i;
      }
   }
   // Copy the left over for later use.
   if (i<nchname) {
      strcat(right,&cname[i]);
   } 
   
   if (!final && branch) { // NOTE: should we add && !leaf ???
      leaf = (TLeaf*)branch->GetListOfLeaves()->UncheckedAt(0);
      final = leaf->IsOnTerminalBranch();
   }

   
   if (leaf) { // We found a Leaf.
             
      // Save the information
      
      Int_t code = fNcodes++;
      
      // We need to move all dimensions information from 'right'
      // to dims so that they are ALL processed here.

      Int_t rightlen = strlen(right);
      for(i=0,k=0; i<rightlen; i++, k++) {
         if (right[i] == '[') {
            int bracket = i;
            int bracket_level = 1;
            int j;
            for (j=++i; j<rightlen && (bracket_level>0 || right[j]=='['); j++, i++) {
            if (right[j]=='[') 
                bracket_level++;
            else if (right[j]==']') 
                bracket_level--;
            }
            if (bracket_level != 0) {
               //Error("TTreeFormula::DefinedVariable","Bracket unbalanced");
               return -1;
            }
            strncat(dims,&right[bracket],j-bracket);
            k += j-bracket;
         }
         if (i!=k) right[i] = right[k];
      }
      right[i]='\0';

      // If needed will now parse the indexes specified for
      // arrays.
      if (dims[0]) {
         current = &( dims[0] );
         Int_t dim = 0;
         char varindex[kMaxLen];
         Int_t index;
         Int_t scanindex ;
         while (current) {
            current++;
            if (current[0] == ']') {
               fIndexes[code][dim] = -1; // Loop over all elements;
            } else {
               scanindex = sscanf(current,"%d",&index);
               if (scanindex) {
                  fIndexes[code][dim] = index;
               } else {
                  fIndexes[code][dim] = -2; // Index is calculated via a variable.
                  strcpy(varindex,current);
                  char *end = (char*)strstr( varindex, "]" );
                  if (end != 0) {
                     *end = '\0';
                     fVarIndexes[code][dim] = new TTreeFormula("index_var",
                                                                  varindex,
                                                                  fTree);
                  }
               }
            }
            dim ++;
            if (dim >= kMAXFORMDIM) {
               // NOTE: test that dim this is NOT too big!!
               break;
            }
            current = (char*)strstr( current, "[" );
         }
      }

      //  fLeaves.Add( leaf );
      //  fCodes[code] = fLeaves.GetLast(); // reference to the leaf
      // We need to record the location in the list of leaves because 
      // the tree might actually be a chain and in that case the leaf will
      // change from tree to tree!.
      // This actually could be wrong if the trees of the chain have a different
      // structure.
      //fCodes[code] = fTree->GetListOfLeaves()->IndexOf(leaf);
      TTree *tleaf = leaf->GetBranch()->GetTree();
      fCodes[code] = tleaf->GetListOfLeaves()->IndexOf(leaf);
      TNamed *named = new TNamed(leaf->GetName(),tleaf->GetName());
      fNames.AddAtAndExpand(named,code);
      fLeaves.AddAtAndExpand(leaf,code);
      
      // Analyze the content of 'right'
      
      Int_t virt_dim = 0;

      TClass * cl = 0;
      if (leaf->InheritsFrom("TLeafObject") ) {
         TLeafObject *lobj = (TLeafObject*)leaf;
         cl = lobj->GetClass();
         if (strlen(right)==0) strcpy(right,work);
      } else if (leaf->InheritsFrom("TLeafElement")) {
         TBranchElement *BranchEl = (TBranchElement *)leaf->GetBranch();
         Int_t type = BranchEl->GetStreamerType();
         switch(type) {
            case TStreamerInfo::kObject: 
            case TStreamerInfo::kTString: 
            case TStreamerInfo::kTNamed: 
            case TStreamerInfo::kTObject: 
            case TStreamerInfo::kOffsetL + TStreamerInfo::kObjectp:
            case TStreamerInfo::kOffsetL + TStreamerInfo::kObjectP:
            case TStreamerInfo::kAny:
            case TStreamerInfo::kObjectp: 
            case TStreamerInfo::kObjectP: {
              TStreamerInfo *info = BranchEl->GetInfo();
              TStreamerElement *elem = (TStreamerElement *)info->GetElements()->At(BranchEl->GetID());
              if (elem) cl = elem->GetClassPointer(); 
            }
            break;
            case -1: {
              TStreamerInfo *info = BranchEl->GetInfo();
              cl = info->GetClass(); 
            }
            break;
         }
      }
      if (cl) {
         Int_t offset;
         Int_t nchname = strlen(right);
         TFormLeafInfo *maininfo = 0;
         TFormLeafInfo *previnfo = 0;
         TFormLeafInfo *leafinfo = 0;
         TStreamerElement* element;
         for (i=0, current = &(work[0]); i<=nchname;i++ ) { 
            // We will treated the terminator as a token.
            if (right[i] == '(') {
               // Right now we do not allow nested paranthesis
               do {
                  *current++ = right[i++];
               } while(right[i]!=')' && right[i]);
               *current++ = right[i++];
               *current='\0';
               char *params = strchr(work,'(');
               if (params) { 
                  *params = 0; params++; 
               } else params = ")";
            
               if (cl->GetClassInfo()==0) {
                  Error("DefinedVariable","Class probably unavailable:%s",cl->GetName());
                  return -1;
               }             
               TMethodCall *method = new TMethodCall(cl, work, params);
               if (!method->GetMethod()) {
                  Error("TTreeFormula","Unknown method:%s",right);
                  return -1;
               }
               switch(method->ReturnType()) {
                  case TMethodCall::kLong:   
                        leafinfo = new TFormLeafInfoMethod(cl,method);
                        break;
                  case TMethodCall::kDouble:
                        leafinfo = new TFormLeafInfoMethod(cl,method);
                        break;
                  case TMethodCall::kOther: 
                       {TString return_type = gInterpreter->TypeName(method->GetMethod()->GetReturnTypeName());
                       leafinfo = new TFormLeafInfoMethod(cl,method);
                       if (return_type != "void") {
                          cl = gROOT->GetClass(return_type.Data());
                       }
                      };    break;
                  default:
                  Error("DefineVariable","Method %s from %s has an impossible return type %d",
                        work,cl->GetName(),method->ReturnType());
                  return -1;
               }
               if (maininfo==0) {
                  maininfo = leafinfo;
               }
               if (previnfo==0) {
                  previnfo = leafinfo;
               } else {
                  previnfo->fNext = leafinfo;
                  previnfo = leafinfo;
               }
               leafinfo = 0; 
               current = &(work[0]);
               continue;
            } else if (i > 0 && (right[i] == '.' || right[i] == '[' || right[i] == '\0') ) {
               // A delimiter happened let's see if what we have seen
               // so far does point to a data member.
               *current = '\0';
               
               // skip it all if there is nothing to look at
               if (strlen(work)==0) continue;
                  
               Bool_t mustderef = kFALSE;
               if (cl == TClonesArray::Class()) {
                  // We are NEVER interested in the ClonesArray object but only
                  // in its contents.  
                  // We need to retrieve the class of its content.
                  
                  TBranch *branch = leaf->GetBranch();
                  branch->GetEntry(readentry);
                  TClonesArray * clones;
                  if (previnfo) clones = (TClonesArray*)previnfo->GetValuePointer(leaf,0);
                  else {
                     // we have a unsplit TClonesArray leaves
                     TStreamerElement * begin = new TStreamerElement("begin","fake",0,
                                                                     TStreamerInfo::kAny,"TClonesArray");
                     TClass *mother_cl;
                     if (leaf->IsA()==TLeafObject::Class()) {
                        // in this case mother_cl is not really used
                        mother_cl = cl;
                     } else {
                        mother_cl = ((TBranchElement*)branch)->GetInfo()->GetClass();
                     }

                     TFormLeafInfo* clonesinfo = new TFormLeafInfoClones(mother_cl, 0, begin);
                     // The dimension needs to be handled!
                     DefineDimensions(code,clonesinfo,virt_dim);

                     mustderef = kTRUE;
                     previnfo = clonesinfo;
                     maininfo = clonesinfo;

                     clones = (TClonesArray*)clonesinfo->GetValuePointer(leaf,0);
                  }
                  TClass * inside_cl = clones->GetClass();
                  if (1 || inside_cl) cl = inside_cl;
                  // if inside_cl is nul ... we have a problem of inconsistency :(
                  if (0 && strlen(work)==0) {
                     // However in this case we have NO content :(
                     // so let get the number of objects
                     //strcpy(work,"fLast");
                  }
               }

               element = cl->GetStreamerInfo()->GetStreamerElement(work,offset);
                  
               if (!element) {
                  // We allow for looking for a data member inside a class inside
                  // a TClonesArray without mentioning the TClonesArrays variable name
                  TIter next( cl->GetStreamerInfo()->GetElements() );
                  TStreamerElement * curelem;
                  while ((curelem = (TStreamerElement*)next())) {
                     if (curelem->GetClassPointer() ==  TClonesArray::Class()) {
                        Int_t clones_offset;
                        cl->GetStreamerInfo()->GetStreamerElement(curelem->GetName(),clones_offset);
                        TFormLeafInfo* clonesinfo = new TFormLeafInfo(cl, clones_offset, curelem);
                        TClonesArray * clones;
                        leaf->GetBranch()->GetEntry(readentry);
                        clones = (TClonesArray*)clonesinfo->GetValuePointer(leaf,0);
                        TClass *sub_cl = clones->GetClass();
                        element = sub_cl->GetStreamerInfo()->GetStreamerElement(work,offset);
                        delete clonesinfo;
                        if (element) {
                           leafinfo = new TFormLeafInfoClones(cl,clones_offset,curelem);
                           DefineDimensions(code,leafinfo,virt_dim);
                           if (maininfo==0) maininfo = leafinfo;
                           if (previnfo==0) previnfo = leafinfo;
                           else {
                              previnfo->fNext = leafinfo;
                              previnfo = leafinfo;
                           }
                           leafinfo = 0;
                           cl = sub_cl;
                           break;
                        } 
                     }
                  }
                  
               }
                  
               if (element) {
                  Int_t type = element->GetType();
                  if (type<60) {
                     // This is a basic type ...
                     if (leafinfo) {
                        leafinfo->fOffset += offset;
                        leafinfo->fElement = element;                       
                     } else {
                        leafinfo = new TFormLeafInfo(cl,offset,element);
                     }
                  } else if (type == TStreamerInfo::kObjectp || 
                             type == TStreamerInfo::kObjectP || 
                             type == TStreamerInfo::kOffsetL + TStreamerInfo::kObjectp || 
                             type == TStreamerInfo::kOffsetL + TStreamerInfo::kObjectP) {
                     // this is a pointer to be followed.
                     if (element->GetClassPointer()!=  TClonesArray::Class()) {
                        leafinfo = new TFormLeafInfoPointer(cl,offset,element);
                        mustderef = kTRUE;
                     } else {
                        leafinfo = new TFormLeafInfoClones(cl,offset,element);
                        mustderef = kTRUE;
                     }
                  } else if (type == TStreamerInfo::kAny || 
                             type == TStreamerInfo::kObject || 
                             type == TStreamerInfo::kTString  || 
                             type == TStreamerInfo::kTNamed  || 
                             type == TStreamerInfo::kTObject ) {
                        // this is an embedded object. We can increase the offset.
                     if (leafinfo) {
                        leafinfo->fOffset += offset;
                        leafinfo->fElement = element;                       
                     } else {
                        leafinfo = new TFormLeafInfo(cl,offset,element);
                     }
                  } else if (type == TStreamerInfo::kBase ||
                             type == TStreamerInfo::kStreamer ||
                             type == TStreamerInfo::kStreamLoop ) {
                     // Unsupported case.
                     Error("DefinedVariable","%s is a datamember of %s BUT is not of a supported type (%d)",
                           right,cl->GetName(),type);
                     return -1;
                  }
               } else {
                  Error("DefinedVariable","%s is not a datamember of %s",work,cl->GetName());
                  return -1;
               }
                  
               DefineDimensions(code,leafinfo,virt_dim);
               if (maininfo==0) {
                  maininfo = leafinfo;
               }
               if (previnfo==0) {
                  previnfo = leafinfo;
               } else if (previnfo!=leafinfo) {
                  previnfo->fNext = leafinfo;
                  previnfo = leafinfo;
               }
               if (mustderef) leafinfo = 0;
               if (right[i]!='\0') {
                  cl = element->GetClassPointer();
               }
               current = &(work[0]);

               if (right[i] == '[') {
                 int bracket = i;
                 int bracket_level = 1;
                 int j;
                 for (j=++i; j<nchname && (bracket_level>0 || right[j]=='['); j++, i++) {
                   if (right[j]=='[') 
                     bracket_level++;
                   else if (right[j]==']') 
                     bracket_level--;
                 }
                 if (bracket_level != 0) {
                   //Error("TTreeFormula::DefinedVariable","Bracket unbalanced");
                   return -1;
                 }
                 strncat(dims,&right[bracket],j-bracket);
                 if (current!=work) *(--current) = '\0'; // remove bracket.
                 --i;
               }
            } else 
               *current++ = right[i];
         }
         if (maininfo) {
            fDataMembers.AddAtAndExpand(maininfo,code);
            fLookupType[code] = kDataMember; 
         }  
      }         
      
      // Let see if we can understand the structure of this branch.
      // Usually we have: leafname[fixed_array] leaftitle[var_array]\type
      // (with fixed_array that can be a multi-dimension array.
      const char *tname = leaf->GetTitle();
      char *leaf_dim = (char*)strstr( tname, "[" );
      
      const char *bname = leaf->GetBranch()->GetName();
      char *branch_dim = (char*)strstr(bname,"[");
      if (branch_dim) branch_dim++; // skip the '['
      
      if (leaf_dim) {
         leaf_dim++; // skip the '['
         if (!branch_dim || strncmp(branch_dim,leaf_dim,strlen(branch_dim))) {
            // then both are NOT the same so do the leaf title first:
            DefineDimensions( leaf_dim, code, virt_dim);
         }
      }
      if (branch_dim) {
         // then both are NOT same so do the branch name next:
         DefineDimensions( branch_dim, code, virt_dim);
      }
      
      if (leaf->IsA() == TLeafElement::Class()) {
         TBranchElement* branch = (TBranchElement*) leaf->GetBranch();
         if (branch->GetBranchCount2()) {
            // Switch from old direct style to using a TLeafInfo
            if (fLookupType[code] == kDataMember) 
               Warning("TTreeFormula",
                       "Already in kDataMember mode when handling multiple variable dimensions");            
            fLookupType[code] = kDataMember;
            fDataMembers.AddAtAndExpand(new TFormLeafInfoMultiVarDim(),code);
            
            // Feed the information into the Dimensions system
            DefineDimensions( code, branch, virt_dim );
         }
      }             

      if (leaf->InheritsFrom("TLeafC") && !leaf->IsUnsigned()) return 5000+code;
      if (leaf->InheritsFrom("TLeafB") && !leaf->IsUnsigned()) return 5000+code;
      return code;
   }
   
//*-*- May be a graphical cut ?
   TCutG *gcut = (TCutG*)gROOT->GetListOfSpecials()->FindObject(name.Data());
   if (gcut) {
      if (!gcut->GetObjectX()) {
         TTreeFormula *fx = new TTreeFormula("f_x",gcut->GetVarX(),fTree);
         gcut->SetObjectX(fx);
      }
      if (!gcut->GetObjectY()) {
         TTreeFormula *fy = new TTreeFormula("f_y",gcut->GetVarY(),fTree);
         gcut->SetObjectY(fy);
      }
      //these 3 lines added by Romain.Holzmann@gsi.de
      Int_t mulx = ((TTreeFormula*)gcut->GetObjectX())->GetMultiplicity();
      Int_t muly = ((TTreeFormula*)gcut->GetObjectY())->GetMultiplicity();
      if(mulx || muly) fMultiplicity = -1;

      fMethods.AddAtAndExpand(gcut,0);
      Int_t code = fNcodes;
      fCodes[code] = -1;
      fNcodes++;
      fLookupType[code] = -1;
      return code;
   }
   return -1;
}

Int_t TTreeFormula::GetRealInstance(Int_t instance, Int_t codeindex) {

      // Now let calculate what physical instance we really need.
      // Some redundant code is used to speed up the cases where
      // they are no dimensions.
      // We know that instance is less that fCumulUsedSize[0] so
      // we can skip the modulo when virt_dim is 0.
      Int_t real_instance = 0;
      Int_t virt_dim;
      Int_t max_dim = fNdimensions[codeindex];
      if ( max_dim ) {
         virt_dim = 0;
         max_dim--;
         
         if (!fMultiVarDim) {
            if (fIndexes[codeindex][0]>=0) {
               real_instance = fIndexes[codeindex][0] * fCumulSizes[codeindex][1];
            } else {
               Int_t local_index;
               local_index = ( instance / fCumulUsedSizes[virt_dim+1]);
               if (fIndexes[codeindex][0]==-2) {
                  // NOTE: Should we check that this is a valid index?
                  local_index = (Int_t)fVarIndexes[codeindex][0]->EvalInstance(local_index);
               }
               real_instance = local_index * fCumulSizes[codeindex][1];
               virt_dim ++;
            }
         } else {
            // NOTE: We assume that ONLY the first dimension of a leaf can have a variable
            // size AND contain the index for the size of yet another sub-dimension.  
            // I.e. a variable size array inside a variable size array can only have its 
            // size vary with the VERY FIRST physical dimension of the leaf.
            // Thus once the index of the first dimension is found, all other dimensions 
            // are fixed!

            // NOTE: We could unroll some of this loops to avoid a few tests.
            TFormLeafInfoMultiVarDim * info;
            info = dynamic_cast<TFormLeafInfoMultiVarDim *>(fDataMembers.At(0));
            Int_t local_index;

            fCumulUsedSizes[kMAXFORMDIM] = 1;
            if (fIndexes[codeindex][0]<0) {
               local_index = 0;
               if (instance) {
                 Int_t virt_accum = 0;
                 do {
                   virt_accum += fCumulUsedVarDims->At(local_index);
                   local_index++;
                 } while( instance >= virt_accum );
                 local_index--;
                 instance -= (virt_accum - fCumulUsedVarDims->At(local_index));
               }
               virt_dim ++;
            } else {
               local_index = fIndexes[codeindex][0];
            }

            // Inform the (appropriate) MultiVarLeafInfo that the clones array index is 
            // local_index.
            for(Int_t d = kMAXFORMDIM; d>0; d--) {
               if (fVarDims[d]) {
                  fCumulUsedSizes[d] = fCumulUsedSizes[d+1] * fVarDims[d]->At(local_index);
               } else {
                  fCumulUsedSizes[d] = fCumulUsedSizes[d+1] * fUsedSizes[d];
               }
            }
            if (info) {
               // When we have multiple variable dimensions, the LeafInfo only expect
               // the instance after the primary index has been set.
               info->SetPrimaryIndex(local_index);
               real_instance = 0;

               // Let's update fCumulSizes for the rest of the code.
               fCumulSizes[codeindex][info->fDim] =  info->GetSize(local_index)
                                                    *fCumulSizes[codeindex][info->fDim+1];
               for(Int_t k=info->fDim -1; k>0; k++) {
                  fCumulSizes[codeindex][k] = fCumulSizes[codeindex][k+1]*fFixedSizes[codeindex][k];
               }
            } else {
               real_instance = local_index * fCumulSizes[codeindex][1];
            }
         }
         if (max_dim>0) {
            for (Int_t dim = 1; dim < max_dim; dim++) {
               if (fIndexes[codeindex][dim]>=0) {
                  real_instance += fIndexes[codeindex][dim] * fCumulSizes[codeindex][dim+1];
               } else {
                  Int_t local_index;
                  if (virt_dim && fCumulUsedSizes[virt_dim]>1) {
                     local_index = ( ( instance % fCumulUsedSizes[virt_dim] )
                                     / fCumulUsedSizes[virt_dim+1]);
                  } else {
                     local_index = ( instance / fCumulUsedSizes[virt_dim+1]);
                  }
                  if (fIndexes[codeindex][dim]==-2) {
                     // NOTE: Should we check that this is a valid index?
                     local_index = (Int_t)fVarIndexes[codeindex][dim]->EvalInstance(local_index);
                  }
                  real_instance += local_index * fCumulSizes[codeindex][dim+1];
                  virt_dim ++;
               }
            }
            if (fIndexes[codeindex][max_dim]>=0) {
               real_instance += fIndexes[codeindex][max_dim];
            } else {
               Int_t local_index;
               if (virt_dim && fCumulUsedSizes[virt_dim]>1) {
                  local_index = instance % fCumulUsedSizes[virt_dim];
               } else {
                  local_index = instance;
               }      
               if (fIndexes[codeindex][max_dim]==-2) {
                  local_index = (Int_t)fVarIndexes[codeindex][max_dim]->EvalInstance(local_index);
               }
               real_instance += local_index;
            }
         } // if (max_dim-1>0)
      } // if (max_dim)
      return real_instance;
}

//______________________________________________________________________________
Double_t TTreeFormula::EvalInstance(Int_t instance)
{
//*-*-*-*-*-*-*-*-*-*-*Evaluate this treeformula*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
//*-*                  =========================
//

   const Int_t kMAXSTRINGFOUND = 10;
   Int_t i,pos,pos2,int1,int2,real_instance;
   Float_t aresult;
   Double_t tab[kMAXFOUND];
   Double_t param[kMAXFOUND];
   Double_t dexp;
   char *tab2[kMAXSTRINGFOUND];
   
   if (fNoper == 1 && fNcodes > 0) {
      if (fCodes[0] < 0) {
         TCutG *gcut = (TCutG*)fMethods.At(0);
         TTreeFormula *fx = (TTreeFormula *)gcut->GetObjectX();
         TTreeFormula *fy = (TTreeFormula *)gcut->GetObjectY();
         Double_t xcut = fx->EvalInstance(instance);
         Double_t ycut = fy->EvalInstance(instance);
         return gcut->IsInside(xcut,ycut);
      }
      TLeaf *leaf = (TLeaf*)fLeaves.UncheckedAt(0);

      real_instance = GetRealInstance(instance,0);

      if (!instance) leaf->GetBranch()->GetEntry(fTree->GetReadEntry());
      else if (real_instance>fNdata[0]) return 0;
      switch(fLookupType[0]) {
         case kDirect: return leaf->GetValue(real_instance);
         case kMethod: return GetValueFromMethod(0,leaf);
         case kDataMember: return ((TFormLeafInfo*)fDataMembers.UncheckedAt(0))->GetValue(leaf,real_instance);
         default: return 0;
      }         
   }
   for(i=0;i<fNval;i++) {
      if (fCodes[i] < 0) {
         TCutG *gcut = (TCutG*)fMethods.At(i);
         TTreeFormula *fx = (TTreeFormula *)gcut->GetObjectX();
         TTreeFormula *fy = (TTreeFormula *)gcut->GetObjectY();
         Double_t xcut = fx->EvalInstance(instance);
         Double_t ycut = fy->EvalInstance(instance);
         param[i] = gcut->IsInside(xcut,ycut);
      } else {
         TLeaf *leaf = (TLeaf*)fLeaves.UncheckedAt(i);
         
         // Now let calculate what physical instance we really need.
         // Some redundant code is used to speed up the cases where
         // they are no dimensions.
         real_instance = GetRealInstance(instance,i);

         if (!instance) leaf->GetBranch()->GetEntry(fTree->GetReadEntry());
         else if (real_instance>fNdata[i]) return 0;
         switch(fLookupType[i]) {
            case kDirect: param[i] = leaf->GetValue(real_instance); break;
            case kMethod: param[i] = GetValueFromMethod(i,leaf); break; 
            case kDataMember: param[i] = ((TFormLeafInfo*)fDataMembers.UncheckedAt(i))->
                                GetValue(leaf,real_instance); break;
            default: param[i] = 0;
         }         
      }
   }
   pos  = 0;
   pos2 = 0;
   for (i=0; i<fNoper; i++) {
      Int_t action = fOper[i];
//*-*- a tree string
      if (action >= 105000) {
         TLeaf *leafc = (TLeaf*)fLeaves.UncheckedAt(action-105000);
         leafc->GetBranch()->GetEntry(fTree->GetReadEntry());
         pos2++; tab2[pos2-1] = (char*)leafc->GetValuePointer();
         continue;
      }
//*-*- a tree variable
      if (action >= 100000) {
         pos++; tab[pos-1] = param[action-100000];
         continue;
      }
//*-*- String
      if (action == 80000) {
         pos2++; tab2[pos2-1] = (char*)fExpr[i].Data();
         continue;
      }
//*-*- numerical value
      if (action >= 50000) {
         pos++; tab[pos-1] = fConst[action-50000];
         continue;
      }
      if (action == 0) {
         pos++;
         sscanf((const char*)fExpr[i],"%g",&aresult);
         tab[pos-1] = aresult;
//*-*- basic operators and mathematical library
      } else if (action < 100) {
         switch(action) {
            case   1 : pos--; tab[pos-1] += tab[pos]; break;
            case   2 : pos--; tab[pos-1] -= tab[pos]; break;
            case   3 : pos--; tab[pos-1] *= tab[pos]; break;
            case   4 : pos--; if (tab[pos] == 0) tab[pos-1] = 0; //  division by 0
                              else               tab[pos-1] /= tab[pos];
                       break;
            case   5 : {pos--; int1=Int_t(tab[pos-1]); int2=Int_t(tab[pos]); tab[pos-1] = Double_t(int1%int2); break;}
            case  10 : tab[pos-1] = TMath::Cos(tab[pos-1]); break;
            case  11 : tab[pos-1] = TMath::Sin(tab[pos-1]); break;
            case  12 : if (TMath::Cos(tab[pos-1]) == 0) {tab[pos-1] = 0;} // { tangente indeterminee }
                       else tab[pos-1] = TMath::Tan(tab[pos-1]);
                       break;
            case  13 : if (TMath::Abs(tab[pos-1]) > 1) {tab[pos-1] = 0;} //  indetermination
                       else tab[pos-1] = TMath::ACos(tab[pos-1]);
                       break;
            case  14 : if (TMath::Abs(tab[pos-1]) > 1) {tab[pos-1] = 0;} //  indetermination
                       else tab[pos-1] = TMath::ASin(tab[pos-1]);
                       break;
            case  15 : tab[pos-1] = TMath::ATan(tab[pos-1]); break;
            case  70 : tab[pos-1] = TMath::CosH(tab[pos-1]); break;
            case  71 : tab[pos-1] = TMath::SinH(tab[pos-1]); break;
            case  72 : if (TMath::CosH(tab[pos-1]) == 0) {tab[pos-1] = 0;} // { tangente indeterminee }
                       else tab[pos-1] = TMath::TanH(tab[pos-1]);
                       break;
            case  73 : if (tab[pos-1] < 1) {tab[pos-1] = 0;} //  indetermination
                       else tab[pos-1] = TMath::ACosH(tab[pos-1]);
                       break;
            case  74 : tab[pos-1] = TMath::ASinH(tab[pos-1]); break;
            case  75 : if (TMath::Abs(tab[pos-1]) > 1) {tab[pos-1] = 0;} // indetermination
                       else tab[pos-1] = TMath::ATanH(tab[pos-1]); break;
            case  16 : pos--; tab[pos-1] = TMath::ATan2(tab[pos-1],tab[pos]); break;
            case  17 : pos--; tab[pos-1] = fmod(tab[pos-1],tab[pos]); break;
            case  20 : pos--; tab[pos-1] = TMath::Power(tab[pos-1],tab[pos]); break;
            case  21 : tab[pos-1] = tab[pos-1]*tab[pos-1]; break;
            case  22 : tab[pos-1] = TMath::Sqrt(TMath::Abs(tab[pos-1])); break;
            case  23 : pos2 -= 2; pos++;if (tab2[pos2] && strstr(tab2[pos2],tab2[pos2+1])) tab[pos-1]=1;
                            else tab[pos-1]=0; break;
            case  30 : if (tab[pos-1] > 0) tab[pos-1] = TMath::Log(tab[pos-1]);
                       else {tab[pos-1] = 0;} //{indetermination }
                       break;
            case  31 : dexp = tab[pos-1];
                       if (dexp < -70) {tab[pos-1] = 0; break;}
                       if (dexp >  70) {tab[pos-1] = TMath::Exp(70); break;}
                       tab[pos-1] = TMath::Exp(dexp); break;
            case  32 : if (tab[pos-1] > 0) tab[pos-1] = TMath::Log10(tab[pos-1]);
                       else {tab[pos-1] = 0;} //{indetermination }
                       break;
            case  40 : pos++; tab[pos-1] = TMath::ACos(-1); break;
            case  41 : tab[pos-1] = TMath::Abs(tab[pos-1]); break;
            case  42 : if (tab[pos-1] < 0) tab[pos-1] = -1; else tab[pos-1] = 1; break;
            case  43 : tab[pos-1] = Double_t(Int_t(tab[pos-1])); break;
            case  50 : pos++; tab[pos-1] = gRandom->Rndm(1); break;
            case  60 : pos--; if (tab[pos-1]!=0 && tab[pos]!=0) tab[pos-1]=1;
                              else tab[pos-1]=0; break;
            case  61 : pos--; if (tab[pos-1]!=0 || tab[pos]!=0) tab[pos-1]=1;
                              else tab[pos-1]=0; break;
            case  62 : pos--; if (tab[pos-1] == tab[pos]) tab[pos-1]=1;
                              else tab[pos-1]=0; break;
            case  63 : pos--; if (tab[pos-1] != tab[pos]) tab[pos-1]=1;
                              else tab[pos-1]=0; break;
            case  64 : pos--; if (tab[pos-1] < tab[pos]) tab[pos-1]=1;
                              else tab[pos-1]=0; break;
            case  65 : pos--; if (tab[pos-1] > tab[pos]) tab[pos-1]=1;
                              else tab[pos-1]=0; break;
            case  66 : pos--; if (tab[pos-1]<=tab[pos]) tab[pos-1]=1;
                              else tab[pos-1]=0; break;
            case  67 : pos--; if (tab[pos-1]>=tab[pos]) tab[pos-1]=1;
                              else tab[pos-1]=0; break;
            case  68 : if (tab[pos-1]!=0) tab[pos-1] = 0; else tab[pos-1] = 1; break;
            case  76 : pos2 -= 2; pos++; if (!strcmp(tab2[pos2+1],tab2[pos2])) tab[pos-1]=1;
                              else tab[pos-1]=0; break;
            case  77 : pos2 -= 2; pos++;if (strcmp(tab2[pos2+1],tab2[pos2])) tab[pos-1]=1;
                              else tab[pos-1]=0; break;
            case  78 : pos--; tab[pos-1]= ((Int_t) tab[pos-1]) & ((Int_t) tab[pos]); break;
            case  79 : pos--; tab[pos-1]= ((Int_t) tab[pos-1]) | ((Int_t) tab[pos]); break;
         }
      }
    }
    Double_t result = tab[0];
    return result;
}

//______________________________________________________________________________
TObject *TTreeFormula::GetLeafInfo(Int_t code) const
{
//*-*-*-*-*-*-*-*Return DataMember corresponding to code*-*-*-*-*-*
//*-*            =======================================
//
//  function called by TLeafObject::GetValue
//  with the value of fLookupType computed in TTreeFormula::DefinedVariable

   return fDataMembers.UncheckedAt(code);

}

//______________________________________________________________________________
TLeaf *TTreeFormula::GetLeaf(Int_t n) const
{
//*-*-*-*-*-*-*-*Return leaf corresponding to serial number n*-*-*-*-*-*
//*-*            ============================================
//

   //return (TLeaf*)fTree->GetListOfLeaves()->UncheckedAt(fCodes[n]);
   return (TLeaf*)fLeaves.UncheckedAt(n);
}

//______________________________________________________________________________
TMethodCall *TTreeFormula::GetMethodCall(Int_t code) const
{
//*-*-*-*-*-*-*-*Return methodcall corresponding to code*-*-*-*-*-*
//*-*            =======================================
//
//  function called by TLeafObject::GetValue
//  with the value of fLookupType computed in TTreeFormula::DefinedVariable

   return (TMethodCall *)fMethods.UncheckedAt(code);

}

//______________________________________________________________________________
Int_t TTreeFormula::GetNdata() 
{
//*-*-*-*-*-*-*-*Return number of data words in the leaf*-*-*-*-*-*-*-*
//*-*-*-*-*-*-*-*Changed to Return number of available instances in the formula*-*-*-*-*-*-*-*
//*-*            =======================================
//

   // new version of GetNData:
   // Possible problem: we only allow one variable dimension so far.
   if (fMultiplicity==0) return 1;
   
   if (fMultiplicity==2) return fCumulUsedSizes[0];
   
   // We have at least one leaf with a variable size:
   Int_t  overall, size, k;
   
   overall = 1;
   for(k=0; k<=kMAXFORMDIM; k++) {
      fUsedSizes[k] = TMath::Abs(fVirtUsedSizes[k]);
      if (fVarDims[k]) {
         for(Int_t i0=0;i0<fVarDims[k]->GetSize();i0++) {
            fVarDims[k]->AddAt(0,i0);
         }
      }
   }
   
   for (Int_t i=0;i<fNcodes;i++) {
      if (fCodes[i] < 0) continue;
      
      // NOTE: Currently only the leafcount can indicates a dimension that
      // is physically variable.  So only the left-most dimension is variable.
      // When an API is introduced to be able to determine a variable inside dimensions
      // one would need to add a way to recalculate the values of fCumulSizes for this
      // leaf.  This would probably require the addition of a new data member
      // fSizes[kMAXCODES][kMAXFORMDIM];
      // Also note that EvalInstance expect all the values (but the very first one)
      // of fCumulSizes to be positive.  So indicating that a physical dimension is 
      // variable (expected for the first one) can NOT be done via negative values of
      // fCumulSizes.
      
      TLeaf *leaf = (TLeaf*)fLeaves.UncheckedAt(i);
      Bool_t hasBranchCount2 = kFALSE;
      if (leaf->GetLeafCount()) {
         TLeaf* leafcount = leaf->GetLeafCount();
         TBranch *branchcount = leafcount->GetBranch();
         TFormLeafInfoMultiVarDim * info = 0;
         if (leaf->IsA() == TLeafElement::Class()) {
            //if branchcount address not yet set, GetEntry will set the address
            // read branchcount value
            if (!branchcount->GetAddress()) branchcount->GetEntry(fTree->GetReadEntry());
            else branchcount->TBranch::GetEntry(fTree->GetReadEntry());
            size = ((TBranchElement*)branchcount)->GetNdata();

            TBranchElement* branch = (TBranchElement*) leaf->GetBranch();
            if (branch->GetBranchCount2()) {
               branch->GetBranchCount2()->GetEntry(fTree->GetReadEntry());

               // Here we need to add the code to take in consideration the 
               // double variable length
               // We fill up the array of sizes in the TLeafInfo:
               info = (TFormLeafInfoMultiVarDim *)fDataMembers.At(i);
               if (info) info->LoadSizes(branch);
               hasBranchCount2 = kTRUE;
               if (info->fVirtDim>=0) info->UpdateSizes(fVarDims[info->fVirtDim]);
               
               // Refresh the fCumulSizes[i] to have '1' for the
               // double variable dimensions
               fCumulSizes[i][info->fDim] =  fCumulSizes[i][info->fDim+1];
               for(Int_t k=info->fDim -1; k>=0; k--) {
                  fCumulSizes[i][k] = fCumulSizes[i][k+1]*fFixedSizes[i][k];
               }
               // Update fCumulUsedSizes
               // UpdateMultiVarSizes(info->fDim,info,i)
               //Int_t fixed = fCumulSizes[i][info->fDim+1];
               //for(Int_t k=info->fDim - 1; k>=0; k++) {
               //   Int_t fixed *= fFixedSizes[i][k];
               //   for(Int_t l=0;l<size; l++) {
               //     fCumulSizes[i][k] += info->GetSize(l) * fixed; 
               //}
            }
         } else {
            branchcount->GetEntry(fTree->GetReadEntry());
            size = leaf->GetLen() / leaf->GetLenStatic();
         }
         if (hasBranchCount2) {
            // We assume that fCumulSizes[i][1] contains the product of the fixed sizes
            fNdata[i] = fCumulSizes[i][1] * ((TFormLeafInfoMultiVarDim *)fDataMembers.At(i))->fSumOfSizes;
         } else {
            fNdata[i] = size * fCumulSizes[i][1];
         }
         if (fIndexes[i][0]==-1) {
            // Case where the index is not specified AND the 1st dimension has a variable
            // size.
            if (fUsedSizes[0]==1 || (size!=1 && size<fUsedSizes[0]) ) fUsedSizes[0] = size;
            if (info && fIndexes[i][info->fDim]>=0) {
               for(Int_t j=0; j<size; j++) {
                  if (fIndexes[i][info->fDim] >= info->GetSize(j)) {
                     info->SetSize(j,0);
                     if (size>fCumulUsedVarDims->GetSize()) fCumulUsedVarDims->Set(size);
                     fCumulUsedVarDims->AddAt(-1,j);
                  }
               }
            }
         } else if (fIndexes[i][0] >= size) {
            // unreacheable element requested:
            fUsedSizes[0] = 0;             
            overall = 0;
         } else if (hasBranchCount2) {
            TFormLeafInfoMultiVarDim * info;
            info = (TFormLeafInfoMultiVarDim *)fDataMembers.At(i);
            if (fIndexes[i][info->fDim] >= info->GetSize(fIndexes[i][0])) {
               // unreacheable element requested:
               fUsedSizes[0] = 0;
               overall = 0;
            }
         }
      } else if (fLookupType[i]==kDataMember) {
         TFormLeafInfo *leafinfo = (TFormLeafInfo*)fDataMembers.UncheckedAt(i);
         if (leafinfo->fCounter) {
            TBranch *branch = leaf->GetBranch();
            branch->GetEntry(fTree->GetReadEntry());
            size = (Int_t) leafinfo->GetCounterValue(leaf);
            if (fIndexes[i][0]==-1) {
               // Case where the index is not specified AND the 1st dimension has a variable
               // size.
               if (fUsedSizes[0]==1 || (size!=1 && size<fUsedSizes[0]) ) fUsedSizes[0] = size;
            } else if (fIndexes[i][0] >= size) {
               // unreacheable element requested:
               fUsedSizes[0] = 0;             
               overall = 0;
            }
            fNdata[i] = size * fCumulSizes[i][1];
         }
      }


      // However we allow several dimensions that virtually vary via the size of their
      // index variables.  So we have code to recalculate fCumulUsedSizes.
      Int_t index;
      TFormLeafInfoMultiVarDim * info;
      info = (TFormLeafInfoMultiVarDim *)fDataMembers.At(i);
      for(Int_t k=0, virt_dim=0; k < fNdimensions[i]; k++) {        
         if (fIndexes[i][k]<0) {
            if (fIndexes[i][k]==-2 && fVirtUsedSizes[virt_dim]<0) {
               // if fVirtUsedSize[virt_dim) is positive then VarIndexes[i][k]->GetNdata()
               // is always the same and has already been factored in fUsedSize[virt_dim]
               index = fVarIndexes[i][k]->GetNdata();
               if (fUsedSizes[virt_dim]==1 || (index!=1 && index<fUsedSizes[virt_dim]) ) 
                  fUsedSizes[virt_dim] = index;
            } else if (hasBranchCount2 && k==info->fDim) {
               // NOTE: We assume the indexing of variable sizes on the first index!
               if (fIndexes[i][0]>=0) {
                  index = info->GetSize(fIndexes[i][0]);
                  if (fUsedSizes[virt_dim]==1 || (index!=1 && index<fUsedSizes[virt_dim]) ) 
                     fUsedSizes[virt_dim] = index;
               } 
            }
            virt_dim++;
         } 
      }
   }
   if (overall==0) return 0;
   if (fMultiplicity==-1) return fCumulUsedSizes[0];
   overall = 1;
   if (!fMultiVarDim) {
      for (k = kMAXFORMDIM; (k >= 0) ; k--) { 
        if (fUsedSizes[k]>=0) {
           overall *= fUsedSizes[k];
           fCumulUsedSizes[k] = overall;
        } else {
           Error("TTreeFormula::GetNdata","GetNdata: a dimension is still negative!");
        }
      }
   } else {
      overall = 0; // Since we work with additions in this section
      if (fUsedSizes[0]>fCumulUsedVarDims->GetSize()) fCumulUsedVarDims->Set(fUsedSizes[0]);
      for(Int_t i = 0; i < fUsedSizes[0]; i++) {
         Int_t local_overall = 1;
         for (k = kMAXFORMDIM; (k > 0) ; k--) { 
            if (fVarDims[k]) {
               Int_t index = fVarDims[k]->At(i);
               if (fUsedSizes[k]==1 || (index!=1 && index<fUsedSizes[k])) 
                 local_overall *= index;
               else local_overall *= fUsedSizes[k];
            } else {
               local_overall *= fUsedSizes[k];
            }
         }
         // a negative value indicates that this value of the primary index
         // will lead to an invalid index; So we skip it.
         if (fCumulUsedVarDims->At(i)<0) fCumulUsedVarDims->AddAt(0,i);
         else {
            fCumulUsedVarDims->AddAt(local_overall,i);
            overall += local_overall;
         }
      }
   }
   return overall;

}

//______________________________________________________________________________
Double_t TTreeFormula::GetValueFromMethod(Int_t i, TLeaf *leaf) const
{
//*-*-*-*-*-*-*-*Return result of a leafobject method*-*-*-*-*-*-*-*
//*-*            ====================================
//

   TMethodCall *m = GetMethodCall(i);
   
   if (m==0) return 0;
   
   void *thisobj; 
   if (leaf->InheritsFrom("TLeafObject") ) thisobj = ((TLeafObject*)leaf)->GetObject();
   else {
      TBranchElement * branch = (TBranchElement*)((TLeafElement*)leaf)->GetBranch();
      Int_t offset =  branch->GetInfo()->GetOffsets()[branch->GetID()];
      char* address = (char*)branch->GetAddress();

      if (address) thisobj = (char*) *(void**)(address+offset);
      else thisobj = branch->GetObject();
   }

   TMethodCall::EReturnType r = m->ReturnType();

   if (r == TMethodCall::kLong) {
      Long_t l;
      m->Execute(thisobj, l);
      return (Double_t) l;
   }
   if (r == TMethodCall::kDouble) {
      Double_t d;
      m->Execute(thisobj, d);
      return (Double_t) d;
   }
   m->Execute(thisobj);

   return 0;

}

//______________________________________________________________________________
Bool_t TTreeFormula::IsInteger() const
{
   // return TRUE if the formula corresponds to one single Tree leaf
   // and this leaf is short, int or unsigned short, int
   // When a leaf is of type integer, the generated histogram is forced
   // to have an integer bin width
      
   if (fNoper > 1) return kFALSE;
   if (fLeaves.GetEntries() != 1) return kFALSE;
   TLeaf *leaf = (TLeaf*)fLeaves.At(0);
   if (!leaf) return kFALSE;
   if (!strcmp(leaf->GetTypeName(),"Int_t"))    return kTRUE;
   if (!strcmp(leaf->GetTypeName(),"Short_t"))  return kTRUE;
   if (!strcmp(leaf->GetTypeName(),"UInt_t"))   return kTRUE;
   if (!strcmp(leaf->GetTypeName(),"UShort_t")) return kTRUE;
   return kFALSE;   
}

//______________________________________________________________________________
void* TFormLeafInfo::GetValuePointer(TLeaf *leaf, Int_t instance) 
{

   char *thisobj = 0;
   if (leaf->InheritsFrom("TLeafObject") ) {
      thisobj = (char*)((TLeafObject*)leaf)->GetObject();
   } else {
      thisobj = GetObjectAddress((TLeafElement*)leaf);
   }

   switch (fElement->GetType()) {
         // basic types
      case kChar_t:   
      case kUChar_t:  
      case kShort_t:  
      case kUShort_t: 
      case kInt_t:    
      case kUInt_t:   
      case kLong_t:   
      case kULong_t:  
      case kFloat_t:  
      case kDouble_t: 
      case kchar:     
      case TStreamerInfo::kCounter:  
                      return (Int_t*)(thisobj+fOffset);

         // array of basic types  array[8]
      case TStreamerInfo::kOffsetL + kChar_t :
         {Char_t *val   = (Char_t*)(thisobj+fOffset);   return &(val[instance]);}
      case TStreamerInfo::kOffsetL + kShort_t:  
         {Short_t *val  = (Short_t*)(thisobj+fOffset);  return &(val[instance]);}
      case TStreamerInfo::kOffsetL + kInt_t:   
         {Int_t *val    = (Int_t*)(thisobj+fOffset);    return &(val[instance]);}
      case TStreamerInfo::kOffsetL + kLong_t:  
         {Long_t *val   = (Long_t*)(thisobj+fOffset);   return &(val[instance]);}
      case TStreamerInfo::kOffsetL + kFloat_t: 
         {Float_t *val  = (Float_t*)(thisobj+fOffset);  return &(val[instance]);}
      case TStreamerInfo::kOffsetL + kDouble_t:
         {Double_t *val = (Double_t*)(thisobj+fOffset); return &(val[instance]);}
      case TStreamerInfo::kOffsetL + kUChar_t: 
         {UChar_t *val  = (UChar_t*)(thisobj+fOffset);  return &(val[instance]);}
      case TStreamerInfo::kOffsetL + kUShort_t:
         {UShort_t *val = (UShort_t*)(thisobj+fOffset); return &(val[instance]);}
      case TStreamerInfo::kOffsetL + kUInt_t:  
         {UInt_t *val   = (UInt_t*)(thisobj+fOffset);   return &(val[instance]);}
      case TStreamerInfo::kOffsetL + kULong_t: 
         {ULong_t *val  = (ULong_t*)(thisobj+fOffset);  return &(val[instance]);}

         // pointer to an array of basic types  array[n]
      case TStreamerInfo::kOffsetP + kChar_t:  
         {Char_t **val   = (Char_t**)(thisobj+fOffset);   return &((*val)[instance]);}
      case TStreamerInfo::kOffsetP + kShort_t: 
         {Short_t **val  = (Short_t**)(thisobj+fOffset);  return &((*val)[instance]);}
      case TStreamerInfo::kOffsetP + kInt_t:   
         {Int_t **val    = (Int_t**)(thisobj+fOffset);    return &((*val)[instance]);}
      case TStreamerInfo::kOffsetP + kLong_t:  
         {Long_t **val   = (Long_t**)(thisobj+fOffset);   return &((*val)[instance]);}
      case TStreamerInfo::kOffsetP + kFloat_t: 
         {Float_t **val  = (Float_t**)(thisobj+fOffset);  return &((*val)[instance]);}
      case TStreamerInfo::kOffsetP + kDouble_t:
         {Double_t **val = (Double_t**)(thisobj+fOffset); return &((*val)[instance]);}
      case TStreamerInfo::kOffsetP + kUChar_t: 
         {UChar_t **val  = (UChar_t**)(thisobj+fOffset);  return &((*val)[instance]);}
      case TStreamerInfo::kOffsetP + kUShort_t:
         {UShort_t **val = (UShort_t**)(thisobj+fOffset); return &((*val)[instance]);}
      case TStreamerInfo::kOffsetP + kUInt_t:  
         {UInt_t **val   = (UInt_t**)(thisobj+fOffset);   return &((*val)[instance]);}
      case TStreamerInfo::kOffsetP + kULong_t: 
         {ULong_t **val  = (ULong_t**)(thisobj+fOffset);  return &((*val)[instance]);}

      case TStreamerInfo::kObjectp: 
      case TStreamerInfo::kObjectP: 
        {TObject **obj = (TObject**)(thisobj+fOffset);   return *obj; }

      case TStreamerInfo::kObject: 
      case TStreamerInfo::kTString: 
      case TStreamerInfo::kTNamed: 
      case TStreamerInfo::kTObject: 
      case TStreamerInfo::kOffsetL + TStreamerInfo::kObjectp:
      case TStreamerInfo::kOffsetL + TStreamerInfo::kObjectP:
      case TStreamerInfo::kAny:
        {TObject *obj = (TObject*)(thisobj+fOffset);   return obj; }
        
      case kOther_t:  
      default:        return 0;
   }

}


//______________________________________________________________________________
Double_t TFormLeafInfo::GetValue(TLeaf *leaf, Int_t instance)
{
//*-*-*-*-*-*-*-*Return result of a leafobject method*-*-*-*-*-*-*-*
//*-*            ====================================
//

   char *thisobj = 0;
   if (leaf->InheritsFrom("TLeafObject") ) {
      thisobj = (char*)((TLeafObject*)leaf)->GetObject();
   } else {
      thisobj = GetObjectAddress((TLeafElement*)leaf);
   }
   return ReadValue(thisobj,instance);
}

//______________________________________________________________________________
Double_t TFormLeafInfo::ReadValue(char *thisobj, Int_t instance)
{
   if (fNext) return fNext->ReadValue(thisobj+fOffset,instance);
   //   return fInfo->ReadValue(thisobj+fOffset,fElement->GetType(),instance,1);
   switch (fElement->GetType()) {
         // basic types
      case kChar_t:   return (Double_t)(*(Char_t*)(thisobj+fOffset));
      case kUChar_t:  return (Double_t)(*(UChar_t*)(thisobj+fOffset));
      case kShort_t:  return (Double_t)(*(Short_t*)(thisobj+fOffset));
      case kUShort_t: return (Double_t)(*(UShort_t*)(thisobj+fOffset));
      case kInt_t:    return (Double_t)(*(Int_t*)(thisobj+fOffset)); 
      case kUInt_t:   return (Double_t)(*(UInt_t*)(thisobj+fOffset)); 
      case kLong_t:   return (Double_t)(*(Long_t*)(thisobj+fOffset));
      case kULong_t:  return (Double_t)(*(ULong_t*)(thisobj+fOffset));
      case kFloat_t:  return (Double_t)(*(Float_t*)(thisobj+fOffset));
      case kDouble_t: return (Double_t)(*(Double_t*)(thisobj+fOffset));
      case kchar:     return (Double_t)(*(char*)(thisobj+fOffset));
      case TStreamerInfo::kCounter:  
                      return (Double_t)(*(Int_t*)(thisobj+fOffset));

         // array of basic types  array[8]
      case TStreamerInfo::kOffsetL + kChar_t :
         {Char_t *val   = (Char_t*)(thisobj+fOffset);   return Double_t(val[instance]);}
      case TStreamerInfo::kOffsetL + kShort_t:  
         {Short_t *val  = (Short_t*)(thisobj+fOffset);  return Double_t(val[instance]);}
      case TStreamerInfo::kOffsetL + kInt_t:   
         {Int_t *val    = (Int_t*)(thisobj+fOffset);    return Double_t(val[instance]);}
      case TStreamerInfo::kOffsetL + kLong_t:  
         {Long_t *val   = (Long_t*)(thisobj+fOffset);   return Double_t(val[instance]);}
      case TStreamerInfo::kOffsetL + kFloat_t: 
         {Float_t *val  = (Float_t*)(thisobj+fOffset);  return Double_t(val[instance]);}
      case TStreamerInfo::kOffsetL + kDouble_t:
         {Double_t *val = (Double_t*)(thisobj+fOffset); return Double_t(val[instance]);}
      case TStreamerInfo::kOffsetL + kUChar_t: 
         {UChar_t *val  = (UChar_t*)(thisobj+fOffset);  return Double_t(val[instance]);}
      case TStreamerInfo::kOffsetL + kUShort_t:
         {UShort_t *val = (UShort_t*)(thisobj+fOffset); return Double_t(val[instance]);}
      case TStreamerInfo::kOffsetL + kUInt_t:  
         {UInt_t *val   = (UInt_t*)(thisobj+fOffset);   return Double_t(val[instance]);}
      case TStreamerInfo::kOffsetL + kULong_t: 
         {ULong_t *val  = (ULong_t*)(thisobj+fOffset);  return Double_t(val[instance]);}

         // pointer to an array of basic types  array[n]
      case TStreamerInfo::kOffsetP + kChar_t:  
         {Char_t **val   = (Char_t**)(thisobj+fOffset);   return Double_t((*val)[instance]);}
      case TStreamerInfo::kOffsetP + kShort_t: 
         {Short_t **val  = (Short_t**)(thisobj+fOffset);  return Double_t((*val)[instance]);}
      case TStreamerInfo::kOffsetP + kInt_t:   
         {Int_t **val    = (Int_t**)(thisobj+fOffset);    return Double_t((*val)[instance]);}
      case TStreamerInfo::kOffsetP + kLong_t:  
         {Long_t **val   = (Long_t**)(thisobj+fOffset);   return Double_t((*val)[instance]);}
      case TStreamerInfo::kOffsetP + kFloat_t: 
         {Float_t **val  = (Float_t**)(thisobj+fOffset);  return Double_t((*val)[instance]);}
      case TStreamerInfo::kOffsetP + kDouble_t:
         {Double_t **val = (Double_t**)(thisobj+fOffset); return Double_t((*val)[instance]);}
      case TStreamerInfo::kOffsetP + kUChar_t: 
         {UChar_t **val  = (UChar_t**)(thisobj+fOffset);  return Double_t((*val)[instance]);}
      case TStreamerInfo::kOffsetP + kUShort_t:
         {UShort_t **val = (UShort_t**)(thisobj+fOffset); return Double_t((*val)[instance]);}
      case TStreamerInfo::kOffsetP + kUInt_t:  
         {UInt_t **val   = (UInt_t**)(thisobj+fOffset);   return Double_t((*val)[instance]);}
      case TStreamerInfo::kOffsetP + kULong_t: 
         {ULong_t **val  = (ULong_t**)(thisobj+fOffset);  return Double_t((*val)[instance]);}

      case kOther_t:  
      default:        return 0;
   }
}

//______________________________________________________________________________
char *TTreeFormula::PrintValue(Int_t mode) const
{
//*-*-*-*-*-*-*-*Return value of variable as a string*-*-*-*-*-*-*-*
//*-*            ====================================
//
//      mode = -2 : Print line with ***
//      mode = -1 : Print column names
//      mode = 0  : Print column values

   const int kMAXLENGTH = 1024;
   static char value[kMAXLENGTH];

   if (mode == -2) {
      for (int i = 0; i < kMAXLENGTH-1; i++)
         value[i] = '*';
      value[kMAXLENGTH-1] = 0;
   } else if (mode == -1)
      sprintf(value, "%s", GetTitle());

   if (TestBit(kIsCharacter)) {
      if (mode == 0) {
         TLeaf *leaf = (TLeaf*)fLeaves.UncheckedAt(0);
         leaf->GetBranch()->GetEntry(fTree->GetReadEntry());
         strncpy(value, (char*)leaf->GetValuePointer(), kMAXLENGTH-1);
         value[kMAXLENGTH-1] = 0;
      }
   } else {
      if (mode == 0) {
         //NOTE: This is terrible form ... but is forced upon us by the fact that we can not
         //use the mutable keyword AND we should keep PrintValue const.
         ((TTreeFormula*)this)->GetNdata();
         sprintf(value,"%9.9g",((TTreeFormula*)this)->EvalInstance(0));
         char *expo = strchr(value,'e');
         if (expo) {
            if (value[0] == '-') strcpy(expo-6,expo);            
            else                 strcpy(expo-5,expo);            
         }
      }
   }
   return &value[0];
}

//______________________________________________________________________________
void TTreeFormula::Streamer(TBuffer &R__b)
{
   // Stream an object of class TTreeFormula.

   if (R__b.IsReading()) {
      UInt_t R__s, R__c;
      Version_t R__v = R__b.ReadVersion(&R__s, &R__c);
      if (R__v > 2) {
         TTreeFormula::Class()->ReadBuffer(R__b, this, R__v, R__s, R__c);
         return;
      }
      //====process old versions before automatic schema evolution
      TFormula::Streamer(R__b);
      R__b >> fTree;
      R__b >> fNcodes;
      R__b.ReadFastArray(fCodes, fNcodes);
      R__b >> fMultiplicity;
      R__b >> fInstance;
      R__b >> fNindex;
      if (fNindex) {
         fLookupType = new Int_t[fNindex];
         R__b.ReadFastArray(fLookupType, fNindex);
      }
      fMethods.Streamer(R__b);
      //====end of old versions
      
   } else {
      TTreeFormula::Class()->WriteBuffer(R__b,this);
   }
}

//______________________________________________________________________________
void TTreeFormula::UpdateFormulaLeaves()
{
   // this function is called TTreePlayer::UpdateFormulaLeaves, itself
   // called by TChain::LoadTree when a new Tree is loaded.
   // Because Trees in a TChain may have a different list of leaves, one
   // must update the leaves numbers in the TTreeFormula used by the TreePlayer.

   // A safer alternative would be to recompile the whole thing .... However
   // currently compile HAS TO be called from the constructor! 
   Int_t nleaves = fNames.GetEntriesFast();
   for (Int_t i=0;i<nleaves;i++) {
      TLeaf *leaf = fTree->GetLeaf(fNames[i]->GetName());
      fLeaves[i] = leaf;
   }
      
}
