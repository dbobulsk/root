// @(#)root/memstat:$Name$:$Id$
// Author: D.Bertini and M.Ivanov   18/06/2007 -- Anar Manafov (A.Manafov@gsi.de) 28/04/2008

/*************************************************************************
 * Copyright (C) 1995-2008, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/
#ifndef ROOT_TMEMSTATMANAGER
#define ROOT_TMEMSTATMANAGER

//****************************************************************************//
//
//  TMemStatManager
//  Memory statistic manager class
//
//****************************************************************************//
// STD
#include <map>
#include <vector>
#include <memory>
#include <cstdlib>
// ROOT
#include "TObject.h"
#include "TTimeStamp.h"
// Memstat
#include "TMemStatDepend.h"
#include "TMemStatInfo.h"


class TTree;
class TStackInfo;

typedef std::vector<Int_t> IntVector_t;
typedef std::auto_ptr<TFile> TFilePtr;

class TMemStatManager: public TObject
{
public:
   typedef std::vector<TCodeInfo> CodeInfoContainer_t;

   enum StatusBits {
      kUserDisable = BIT(18),       // user disable-enable switch  switch
      kStatDisable = BIT(16),       // true if disable statistic
      kStatRoutine = BIT(17)        // indicator inside of stat routine  (AddPointer or FreePointer)
   };
   enum EDumpTo { Tree, SysTree };

   TMemStatManager();
   virtual ~TMemStatManager();

   void Enable();                              //enable memory statistic
   void Disable();                             //dissable memory statisic
   void SetAutoStamp(UInt_t sizeMem, UInt_t n, UInt_t max) {
      fAutoStampSize = sizeMem;
      fAutoStampN = n;
      fAutoStampDumpSize = max;
   }
   void AddStamps(const char * stampname = 0);           //add  stamps to the list of stamps for changed stacks
   static void SAddStamps(const Char_t * stampname);             // static version add  stamps to the list of stamps for changed stacks

   static TMemStatManager* GetInstance();       //get instance of class - ONLY ONE INSTANCE
   static void Close();                         //close MemStatManager
   TInfoStamp &AddStamp();                   //add one stamp to the list of stamps
   TCodeInfo &GetCodeInfo(void *address);
   UInt_t GetCodeInfoIndex(void *address) {
      return fCodeInfoMap[address];
   }
   void DumpTo(EDumpTo _DumpTo, Bool_t _clearStamps = kTRUE, const char * _stampName = 0);  //write current status to file

public:
   typedef void (*StampCallback_t)(const Char_t * desription);
   //stack data members
   IntVector_t fSTHashTable; //!pointer to the hash table
   Int_t fCount;        //!number of entries in table
   Int_t fStampNumber;  //current stamp number
   std::vector<TStackInfo> fStackVector;            // vector withstack symbols
   std::vector<TInfoStamp> fStampVector;            // vector of stamp information
   std::vector<TTimeStamp> fStampTime;              // vector of stamp information
   CodeInfoContainer_t  fCodeInfoArray;          // vector with code info
   std::map<const void*, UInt_t> fCodeInfoMap;      //! map of code information
   Int_t fDebugLevel;                               //!debug level
   TMemStatManager::StampCallback_t fStampCallBack; //!call back function
   void SetUseGNUBuildinBacktrace(Bool_t _NewVal) {
      fUseGNUBuildinBacktrace = _NewVal;
   }

protected:
   TMemStatDepend::MallocHookFunc_t fPreviousMallocHook;    //!old malloc function
   TMemStatDepend::FreeHookFunc_t fPreviousFreeHook;        //!old free function
   void Init();
   TStackInfo *STAddInfo(Int_t size, void **stackptrs);
   TStackInfo *STFindInfo(Int_t size, void **stackptrs);
   void RehashLeak(Int_t newSize);                  //rehash leak pointers
   void *AddPointer(size_t size, void *ptr = 0);    //add pointer to the table
   void FreePointer(void *p);                       //free pointer
   static void *AllocHook(size_t size, const void* /*caller*/);
   static void FreeHook(void* ptr, const void* /*caller*/);
   TInfoStamp fLastStamp;           //last written stamp
   TInfoStamp fCurrentStamp;        //current stamp
   UInt_t fAutoStampSize;           //change of size invoking STAMP
   UInt_t fAutoStampN;              //change of number of allocation  STAMP
   UInt_t fAutoStampDumpSize;       //
   Int_t fMinStampSize;             // the minimal size to be dumped to tree
   //  memory infomation
   Int_t fSize;                     //!size of hash table
   TMemTable **fLeak;               //!pointer to the hash table
   Int_t fAllocCount;               //!number of memory allocation blocks
   TDeleteTable fMultDeleteTable;   //!pointer to the table
   TFilePtr fDumpFile;              //!file to dump current information
   TTree *fDumpTree;                //!tree to dump information
   TTree *fDumpSysTree;             //!tree to dump information
   static TMemStatManager *fgInstance; // pointer to instance
   static void *fgStackTop;             // stack top pointer

   void free_hashtable() {
      if (!fLeak)
         return;

      for (Int_t i = 0; i < fSize; ++i)
         free(fLeak[i]);
      free(fLeak);
   }

   Bool_t fUseGNUBuildinBacktrace;

   ClassDef(TMemStatManager, 1) // a manager of memstat sassions.
};

#endif
