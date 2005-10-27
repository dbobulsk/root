// @(#)root/splot:$Name:  $:$Id: TSPlot.h,v 1.2 2005/10/27 17:06:29 brun Exp $
// Author: Muriel Pivk, Anna Kreshuk    10/2005

/**********************************************************************
 *                                                                    *
 * Copyright (c) 2005 ROOT Foundation,  CERN/PH-SFT                   *
 *                                                                    *
 **********************************************************************/

#ifndef ROOT_TSPLot
#define ROOT_TSPlot

#include "TMatrixD.h"
#include "TObjArray.h"
#include "TString.h"

class TH1D;
class TTree;

class TSPlot: public TObject {
protected:
   TMatrixD fXvar;           //!
   TMatrixD fYvar;           //!
   TMatrixD fYpdf;           //!
   TMatrixD fPdfTot;         //!
   TMatrixD fMinmax;         //mins and maxs of variables for histogramming
   TMatrixD fSWeights;       //computed sWeights

   TObjArray fXvarHists;     //histograms of control variables
   TObjArray fYvarHists;     //histograms of discriminating variables
   TObjArray fYpdfHists;     //histograms of pdfs
   TObjArray fSWeightsHists; //histograms of weighted variables

   TTree    *fTree;          //!
   TString* fTreename;       //The name of the data tree
   TString* fVarexp;         //Variables used for splot
   TString* fSelection;      //Selection on the tree 

  
   Int_t    fNx;             //Number of control variables
   Int_t    fNy;             //Number of discriminating variables
   Int_t    fNSpecies;       //Number of species
   Int_t    fNevents;        //Total number of events

   Double_t *fNumbersOfEvents; //[fNSpecies] estimates of numbers of events in each species

   void SPlots(Double_t *covmat, Int_t i_excl);

public:
   TSPlot();
   TSPlot(Int_t nx, Int_t ny, Int_t ne, Int_t ns, TTree* tree);
   virtual ~TSPlot();

   void       Browse(TBrowser *b);
   Bool_t     IsFolder() const { return kTRUE;}

   void       FillXvarHists(Int_t nbins = 100);
   void       FillYvarHists(Int_t nbins = 100);
   void       FillYpdfHists(Int_t nbins = 100);
   void       FillSWeightsHists(Int_t nbins = 50);

   TObjArray* GetSWeightsHists();
   TH1D*      GetSWeightsHist(Int_t ixvar, Int_t ispecies,Int_t iyexcl=-1);
   TObjArray* GetXvarHists();
   TH1D*      GetXvarHist(Int_t ixvar);
   TObjArray* GetYvarHists();
   TH1D*      GetYvarHist(Int_t iyvar);
   TObjArray* GetYpdfHists();
   TH1D*      GetYpdfHist(Int_t iyvar, Int_t ispecies);
   void       GetSWeights(TMatrixD &weights);
   void       GetSWeights(Double_t *weights);
   TString*   GetTreeName(){return fTreename;}
   TString*   GetTreeSelection() {return fSelection;}
   TString*   GetTreeExpression() {return fVarexp;}
   void       MakeSPlot(Option_t* option="v");

   void       SetNX(Int_t nx){fNx=nx;}
   void       SetNY(Int_t ny){fNy=ny;}
   void       SetNSpecies(Int_t ns){fNSpecies=ns;}
   void       SetNEvents(Int_t ne){fNevents=ne;}
   void       SetInitialNumbersOfSpecies(Int_t *numbers);
   void       SetTree(TTree *tree);
   void       SetTreeSelection(const char* varexp="", const char *selection="", Long64_t firstentry=0);

   ClassDef(TSPlot, 1)
};
 
#endif

