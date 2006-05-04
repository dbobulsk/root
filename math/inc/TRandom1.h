// @(#)root/base:$Name:  $:$Id: TRandom1.h,v 1.3 2003/01/26 21:03:16 brun Exp $
// Author: Rene Brun   04/03/99

/*************************************************************************
 * Copyright (C) 1995-2000, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT_TRandom1
#define ROOT_TRandom1



//////////////////////////////////////////////////////////////////////////
//                                                                      //
// TRandom1                                                             //
//                                                                      //
// Ranlux random number generator class (periodicity > 10**14)          //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef ROOT_TRandom
#include "TRandom.h"
#endif

class TRandom1 : public TRandom {

protected:
  Long64_t        fTheSeed;
  Int_t           fNskip;
  Int_t           fLuxury;
  Int_t           fIlag;
  Int_t           fJlag;
  Int_t           fCount24;
  Float_t         fFloatSeedTable[24];
  Float_t         fCarry;
  const Int_t     fIntModulus;
  static Int_t    fgNumEngines;
  static Int_t    fgMaxIndex;
  const Long64_t *fTheSeeds;
  const Double_t  fMantissaBit24;
  const Double_t  fMantissaBit12;

public:
   TRandom1();
   TRandom1(Long64_t seed, Int_t lux = 3 );
   TRandom1(Int_t rowIndex, Int_t colIndex, Int_t lux );
   virtual ~TRandom1();
   virtual  Int_t    GetLuxury() const {return fLuxury;}
   virtual Long64_t  GetTheSeed() const {return fTheSeed;}
                    // Gets the current seed.
   const Long64_t   *GetTheSeeds() const {return fTheSeeds;}
                     // Gets the current array of seeds.
   static   void     GetTableSeeds(Long64_t* seeds, Int_t index);
                     // Gets back seed values stored in the table, given the index.
   virtual  Double_t Rndm(Int_t i=0);
   virtual  void     RndmArray(Int_t size, Float_t *vect);
   virtual  void     RndmArray(Int_t size, Double_t *vect);
   virtual  void     SetSeed2(Long64_t seed, Int_t lux=3);
                     // Sets the state of the algorithm according to seed.
   virtual  void     SetSeeds(const Long64_t * seeds, Int_t lux=3);
                     // Sets the state of the algorithm according to the zero terminated
                     // array of seeds. Only the first seed is used.

   ClassDef(TRandom1,1)  //Ranlux Random number generators with periodicity > 10**14
};

R__EXTERN TRandom *gRandom;

#endif
