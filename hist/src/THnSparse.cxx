// @(#)root/hist:$Name:  $:$Id: THnSparse.cxx,v 1.353 2007/08/27 14:11:40 brun Exp $
// Author: Axel Naumann, 2007-09-11

/*************************************************************************
 * Copyright (C) 1995-2007, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include "THnSparse.h"

#include "TArrayI.h"
#include "TAxis.h"
#include "TCanvas.h"
#include "TClass.h"
#include "TDataMember.h"
#include "TDataType.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TH3D.h"
#include "TInterpreter.h"
#include "TMath.h"

//______________________________________________________________________________
//
// Use a THnSparse instead of TH1 / TH2 / TH3 / array for histogramming when
// only a small fraction of bins is filled. A 10-dimensional histogram with 10
// bins per dimension has 10^10 bins; in a naive implementation this will not
// fit in memory. THnSparse only allocates memory for the bins that have
// non-zero bin content instead, drastically reducing both the memory usage
// and the access time.
//
// To construct a THnSparse object you must use one of its templated, derived
// classes:
// THnSparseD (typedef for THnSparse<ArrayD>): bin content held by a Double_t,
// THnSparseF (typedef for THnSparse<ArrayF>): bin content held by a Float_t,
// THnSparseL (typedef for THnSparse<ArrayL>): bin content held by a Long_t,
// THnSparseI (typedef for THnSparse<ArrayI>): bin content held by an Int_t,
// THnSparseS (typedef for THnSparse<ArrayS>): bin content held by a Short_t,
// THnSparseC (typedef for THnSparse<ArrayC>): bin content held by a Char_t,
//
// They take name and title, the number of dimensions, and for each dimension
// the number of bins, the minimal, and the maximal value on the dimension's
// axis. A TH2 h("h","h",10, 0., 10., 20, -5., 5.) would correspond to
//   UInt_t bins[2] = {10, 20};
//   Double_t xmin[2] = {0., -5.};
//   Double_t xmax[2] = {10., 5.};
//   THnSparse hs("hs", "hs", 2, bins, min, max);
//
// * Filling
// A THnSparse is filled just like a regular histogram, using
// THnSparse::Fill(x, weight), where x is a n-dimensional Double_t value.
// To take errors into account, Sumw2() must be called before filling the
// histogram.
// Bins are allocated as needed; the status of the allocation can be observed
// by GetSparseFractionBins(), GetSparseFractionMem().
//
// * Fast Bin Content Access
// When iterating over a THnSparse one should only look at filled bins to save
// processing time. The number of filled bins is returned by
// THnSparse::GetNbins(); the bin content for each (linear) bin number can
// be retrieved by THnSparse::GetBinContent(linidx, (UInt_t*)coord).
// After the call, coord will contain the bin coordinate of each axis for the bin
// with linear index linidx. A possible call would be
//   cout << hs.GetBinContent(0, coord);
//   cout <<" is the content of bin [x = " << coord[0] "
//        << " | y = " << coord[1] << "]" << endl;
//
// * Projections
// The dimensionality of a THnSparse can be reduced by projecting it to
// 1, 2, 3, or n dimensions, which can be represented by a TH1, TH2, TH3, or
// a THnSparse. See the Projection() members.
//
// * Internal Representation
// An entry for a filled bin consists of its n-dimensional coordinates and
// its bin content. The coordinates are compacted to use as few bits as
// possible; e.g. a histogram with 10 bins in x and 20 bins in y will only
// use 4 bits for the x representation and 5 bits for the y representation.
// This is handled by the internal class THnSparseCompactBinCoord.
// Bin data (content and coordinates) are allocated in chunks of size
// fChunkSize; this parameter can be set when constructing a THnSparse. Each
// chunk is represented by an object of class THnSparseArrayChunk.
//
// Translation from an n-dimensional bin coordinate to the linear index within
// the chunks is done by GetBin(). It creates a hash from the compacted bin
// coordinates (the hash of a bin coordinate is the compacted coordinate itself
// if it takes less than 4 bytes, the minimal supported size of a Long_t).
// This hash is used to lookup the linear index in the TExMap member fBins;
// the coordinates of the entry fBins points to is compared to the coordinates
// passed to GetBin(). If they do not match, these two coordinates have the same
// hash - which is extremely unlikely but (for the case where the compact bin
// coordinates are larger than 4 bytes) possible. In this case, fBinsContinued
// contains a chain of linear indexes with the same hash. Iterating through this
// chain and comparing each bin coordinates with the one passed to GetBin() will
// retrieve the matching bin.

//______________________________________________________________________________
//
// THnSparseCompactBinCoord
//______________________________________________________________________________

class THnSparseCompactBinCoord {
public:
   THnSparseCompactBinCoord(UInt_t dim, UInt_t* nbins);
   ~THnSparseCompactBinCoord();
   void SetCoord(UInt_t* coord) { memcpy(fCurrentBin, coord, sizeof(UInt_t) * fNdimensions); }
   ULong64_t GetHash();
   UInt_t GetSize() const { return fCoordBufferSize; }
   UInt_t* GetCoord() const { return fCurrentBin; }
   Char_t* GetBuffer() const { return fCoordBuffer; }
   void GetCoordFromBuffer(UInt_t* coord) const;

protected:
   UInt_t GetNumBits(UInt_t n) const {
      // return the number of bits allocated by the number "n"
      UInt_t r = (n > 0);
      while (n/=2) ++r;
      return r;
   }
private:
   UInt_t  fNdimensions;     // number of dimensions
   UInt_t *fBitOffsets;      //[fNdimensions + 1] bit offset of each axis index
   Char_t *fCoordBuffer;     // compact buffer of coordinates
   UInt_t  fCoordBufferSize; // size of fBinCoordBuffer
   UInt_t *fCurrentBin;      // current coordinates
};
//______________________________________________________________________________
//______________________________________________________________________________



//______________________________________________________________________________
THnSparseCompactBinCoord::THnSparseCompactBinCoord(UInt_t dim, UInt_t* nbins):
   fNdimensions(dim), fBitOffsets(0), fCoordBuffer(0), fCoordBufferSize(0)
{
   // Initialize a THnSparseCompactBinCoord object with "dim" dimensions
   // and "bins" holding the number of bins for each dimension.

   fCurrentBin = new UInt_t[dim];
   fBitOffsets = new UInt_t[dim + 1];

   int shift = 0;
   for (UInt_t i = 0; i < dim; ++i) {
      fBitOffsets[i] = shift;
      shift += GetNumBits(nbins[i] + 2);
   }
   fBitOffsets[dim] = shift;
   fCoordBufferSize = (shift + 7) / 8;
   fCoordBuffer = new Char_t[fCoordBufferSize];
}


//______________________________________________________________________________
THnSparseCompactBinCoord::~THnSparseCompactBinCoord()
{
   // destruct a THnSparseCompactBinCoord

   delete [] fBitOffsets;
   delete [] fCoordBuffer;
   delete [] fCurrentBin;
}

//______________________________________________________________________________
void THnSparseCompactBinCoord::GetCoordFromBuffer(UInt_t* coord) const
{
   // Given the current fCoordBuffer, calculate ("decompact") the bin coordinates,
   // and return it in coord.

   for (UInt_t i = 0; i < fNdimensions; ++i) {
      const UInt_t offset = fBitOffsets[i] / 8;
      UInt_t shift = fBitOffsets[i] % 8;
      Int_t nbits = fBitOffsets[i + 1] - fBitOffsets[i];
      UChar_t* pbuf = (UChar_t*) fCoordBuffer + offset;
      coord[i] = *pbuf >> shift;
      UInt_t subst = (UInt_t) -1;
      subst = subst << nbits;
      nbits -= (8 - shift);
      shift = 8 - shift;
      for (Int_t n = 0; n * 8 <= nbits; ++n) {
         ++pbuf;
         coord[i] += *pbuf << shift;
         shift += 8;
      }
      coord[i] &= ~subst;
   }
}

//______________________________________________________________________________
ULong64_t THnSparseCompactBinCoord::GetHash() {
   // Calculate hash for compact bin index of the current bin.

   memset(fCoordBuffer, 0, fCoordBufferSize);
   for (UInt_t i = 0; i < fNdimensions; ++i) {
      const UInt_t offset = fBitOffsets[i] / 8;
      const UInt_t shift = fBitOffsets[i] % 8;
      ULong64_t val = fCurrentBin[i];

      Char_t* pbuf = fCoordBuffer + offset;
      *pbuf += 0xff & (val << shift);
      val = val >> (8 - shift);
      while (val) {
         ++pbuf;
         *pbuf += 0xff & val;
         val = val >> 8;
      }
   }

   // Bins are addressed in two different modes, depending
   // on whether the compact bin index fits into a Long_t or not.
   // If it does, we can use it as a "perfect hash" for the TExMap.
   // If not we build a hash from the compact bin index, and use that
   // as the TExMap's hash.
   // For the non-hash mode, the size of the compact bin index must be
   // smaller than Long_t on all supported platforms - not just the current
   // one, because we make this layout persistent, too. So just test for
   // its size <= 4.

   switch (fCoordBufferSize) {
   case 1: return fCoordBuffer[0];
   case 2: return fCoordBuffer[0] + (fCoordBuffer[1] <<  8l);
   case 3: return fCoordBuffer[0] + (fCoordBuffer[1] <<  8l)
              + (fCoordBuffer[2] << 16l);
   case 4: return fCoordBuffer[0] + (fCoordBuffer[1] <<  8l)
              + (fCoordBuffer[2] << 16l) + (fCoordBuffer[3] << 24l);
   }
   return TMath::Hash(fCoordBuffer, fCoordBufferSize);
}


//______________________________________________________________________________
//______________________________________________________________________________



ClassImp(THnSparseArrayChunk);

//______________________________________________________________________________
THnSparseArrayChunk::THnSparseArrayChunk(UInt_t coordsize, bool errors, TArray* cont):
      fContent(cont), fSingleCoordinateSize(coordsize), fCoordinatesSize(0),
      fCoordinates(0), fSumw2(0)
{
   // (Default) initialize a chunk. Takes ownership of cont (~THnSparseArrayChunk deletes it),
   // and create an ArrayF for errors if "errors" is true.

   fCoordinates = new Char_t[fSingleCoordinateSize * cont->GetSize()];

   if (errors) fSumw2 = new TArrayF(cont->GetSize());
}

//______________________________________________________________________________
THnSparseArrayChunk::~THnSparseArrayChunk()
{
   // Destructor
   delete fContent;
   delete [] fCoordinates;
   delete fSumw2;
}

//______________________________________________________________________________
void THnSparseArrayChunk::AddBin(ULong_t idx, Char_t* coordbuf)
{
   // Create a new bin in this chunk

   memcpy(fCoordinates + idx * fSingleCoordinateSize, coordbuf, fSingleCoordinateSize);
   fCoordinatesSize += fSingleCoordinateSize;
}

//______________________________________________________________________________
void THnSparseArrayChunk::Sumw2()
{
   // Turn on support of errors
   if (!fSumw2)
      fSumw2 = new TArrayF(fContent->GetSize());
}


//______________________________________________________________________________
//______________________________________________________________________________

ClassImp(THnSparse);

//______________________________________________________________________________
THnSparse::THnSparse():
   fNdimensions(0), fFilledBins(0), fEntries(0), fChunkSize(1024), fCompactCoord(0)
{
   // Construct an empty THnSparse.
}

//______________________________________________________________________________
THnSparse::THnSparse(const char* name, const char* title, UInt_t dim,
                     UInt_t* nbins, Double_t* xmin, Double_t* xmax, UInt_t chunksize):
   TNamed(name, title), fNdimensions(dim), fFilledBins(0), fEntries(0),
   fAxes(dim), fChunkSize(chunksize), fCompactCoord(0)
{
   // Construct a THnSparse with "dim" dimensions,
   // with chunksize as the size of the chunks.

   for (UInt_t i = 0; i < fNdimensions; ++i)
      fAxes.AddAtAndExpand(new TAxis(nbins[i], xmin[i], xmax[i]), i);
   fAxes.SetOwner();

   fCompactCoord = new THnSparseCompactBinCoord(dim, nbins);
}

//______________________________________________________________________________
THnSparse::~THnSparse() {
   // Destruct a THnSparse

   delete fCompactCoord;
}

//______________________________________________________________________________
void THnSparse::AddBinContent(UInt_t* coord, Double_t v)
{
   // Add "v" to the content of bin with coordinates "coord"

   GetCompactCoord()->SetCoord(coord);
   Long_t bin = GetBinIndexForCurrentBin(kTRUE);
   THnSparseArrayChunk* chunk = GetChunk(bin / fChunkSize);
   bin %= fChunkSize;
   v += chunk->fContent->GetAt(bin);
   return chunk->fContent->SetAt(v, bin);
}

//______________________________________________________________________________
THnSparseArrayChunk* THnSparse::AddChunk()
{
   // Create a new chunk of bin content
   THnSparseArrayChunk* first = 0;
   if (fBinContent.GetEntriesFast() > 0)
      first = GetChunk(0);
   Bool_t errors = first && first->fSumw2;
   THnSparseArrayChunk* chunk = new THnSparseArrayChunk(GetCompactCoord()->GetSize(), errors, GenerateArray());
   fBinContent.AddLast(chunk);
   return chunk;
}

//______________________________________________________________________________
Long_t THnSparse::GetBin(Double_t* x, Bool_t allocate /* = kTRUE */)
{
   // Get the bin index for the n dimensional tuple x,
   // allocate one if it doesn't exist yet and "allocate" is true.

   UInt_t *coord = GetCompactCoord()->GetCoord();
   for (UInt_t i = 0; i < fNdimensions; ++i)
      coord[i] = GetAxis(i)->FindBin(x[i]);

   return GetBinIndexForCurrentBin(allocate);
}


//______________________________________________________________________________
Long_t THnSparse::GetBin(const char* name[], Bool_t allocate /* = kTRUE */)
{
   // Get the bin index for the n dimensional tuple addressed by "name",
   // allocate one if it doesn't exist yet and "allocate" is true.

   UInt_t *coord = GetCompactCoord()->GetCoord();
   for (UInt_t i = 0; i < fNdimensions; ++i)
      coord[i] = GetAxis(i)->FindBin(name[i]);

   return GetBinIndexForCurrentBin(allocate);
}

//______________________________________________________________________________
Long_t THnSparse::GetBin(UInt_t* coord, Bool_t allocate /*= kTRUE*/)
{
   // Get the bin index for the n dimensional coordinates coord,
   // allocate one if it doesn't exist yet and "allocate" is true.
   GetCompactCoord()->SetCoord(coord);
   return GetBinIndexForCurrentBin(allocate);
}

//______________________________________________________________________________
Double_t THnSparse::GetBinContent(UInt_t *coord) const {
   // Get content of bin with coordinates "coord"
   GetCompactCoord()->SetCoord(coord);
   Long_t idx = const_cast<THnSparse*>(this)->GetBinIndexForCurrentBin(kFALSE);
   if (idx < 0) return 0.;
   THnSparseArrayChunk* chunk = GetChunk(idx / fChunkSize);
   return chunk->fContent->GetAt(idx % fChunkSize);
}

//______________________________________________________________________________
Double_t THnSparse::GetBinContent(Long64_t idx, UInt_t* coord /* = 0 */) const
{
   // Return the content of the filled bin number "idx".
   // If coord is non-null, it will contain the bin's coordinates for each axis
   // that correspond to the bin.

   if (idx >= 0) {
      THnSparseArrayChunk* chunk = GetChunk(idx / fChunkSize);
      idx %= fChunkSize;
      if (chunk && chunk->fContent->GetSize() > idx) {
         if (coord) {
            UInt_t sizeCompact = GetCompactCoord()->GetSize();
            memcpy(GetCompactCoord()->GetBuffer(), chunk->fCoordinates + idx * sizeCompact, sizeCompact);
            GetCompactCoord()->GetCoordFromBuffer(coord);
         }
         return chunk->fContent->GetAt(idx);
      }
   }
   if (coord)
      memset(coord, -1, sizeof(UInt_t) * fNdimensions);
   return 0.;
}

//______________________________________________________________________________
Double_t THnSparse::GetBinError(UInt_t *coord) const {
   // Get error of bin with coordinates "coord"

   if (!GetChunk(0) || !GetChunk(0)->fSumw2)
      return 0.;

   GetCompactCoord()->SetCoord(coord);
   Long_t idx = const_cast<THnSparse*>(this)->GetBinIndexForCurrentBin(kFALSE);
   if (idx < 0) return 0.;

   THnSparseArrayChunk* chunk = GetChunk(idx / fChunkSize);
   return chunk->fSumw2->GetAt(idx % fChunkSize);
}

//______________________________________________________________________________
Double_t THnSparse::GetBinError(Long64_t linidx) const {
   // Get content of bin addressed by linidx

   if (!GetChunk(0) || !GetChunk(0)->fSumw2)
      return 0.;

   if (linidx < 0) return 0.;
   THnSparseArrayChunk* chunk = GetChunk(linidx / fChunkSize);
   linidx %= fChunkSize;
   if (!chunk || chunk->fContent->GetSize() < linidx)
      return 0.;

   return chunk->fSumw2->GetAt(linidx);
}

//______________________________________________________________________________
Long_t THnSparse::GetBinIndexForCurrentBin(Bool_t allocate)
{
   // Return the index for fCurrentBinIndex.
   // If it doesn't exist then return -1, or allocate a new bin if allocate is set

   Long_t hash = GetCompactCoord()->GetHash();
   Long_t linidx = (Long_t) fBins.GetValue(hash);
   while (linidx) {
      // fBins stores index + 1!
      THnSparseArrayChunk* chunk = GetChunk((linidx - 1)/ fChunkSize);
      if (chunk->Matches((linidx - 1) % fChunkSize, GetCompactCoord()->GetBuffer()))
         return linidx - 1; // we store idx+1, 0 is "TExMap: not found"

      Long_t nextlinidx = fBinsContinued.GetValue(linidx);
      if (!nextlinidx) break;

      linidx = nextlinidx;
   }
   if (!allocate) return -1;

   ++fFilledBins;

   // allocate bin in chunk
   THnSparseArrayChunk *chunk = (THnSparseArrayChunk*) fBinContent.Last();
   Long_t newidx = chunk ? ((Long_t) chunk->GetEntries()) : -1;
   if (!chunk || newidx == fChunkSize) {
      chunk = AddChunk();
      newidx = 0;
   }
   chunk->AddBin(newidx, GetCompactCoord()->GetBuffer());

   // store translation between hash and bin
   newidx += (fBinContent.GetEntriesFast() - 1) * fChunkSize;
   if (!linidx)
      // fBins didn't find it
      fBins.Add(hash, newidx + 1);
   else
      // fBins contains one, but it's the wrong one;
      // add entry to fBinsContinued.
      fBinsContinued.Add(linidx, newidx + 1);
   return newidx;
}

//______________________________________________________________________________
THnSparseCompactBinCoord* THnSparse::GetCompactCoord() const
{
   // Return THnSparseCompactBinCoord object.

   if (!fCompactCoord) {
      UInt_t *bins = new UInt_t[fNdimensions];
      for (UInt_t d = 0; d < fNdimensions; ++d)
         bins[d] = GetAxis(d)->GetNbins();
      const_cast<THnSparse*>(this)->fCompactCoord
         = new THnSparseCompactBinCoord(fNdimensions, bins);
      delete [] bins;
   }
   return fCompactCoord;
}

//______________________________________________________________________________
Double_t THnSparse::GetSparseFractionBins() const {
   // Return the amount of filled bins over all bins

   Double_t nbinsTotal = 1.;
   for (UInt_t d = 0; d < fNdimensions; ++d)
      nbinsTotal *= GetAxis(d)->GetNbins() + 2;
   return fFilledBins / nbinsTotal;
}

//______________________________________________________________________________
Double_t THnSparse::GetSparseFractionMem() const {
   // Return the amount of used memory over memory that would be used by a
   // non-sparse n-dimensional histogram. The value is approximate.

   UInt_t arrayElementSize = 0;
   Double_t size = 0.;
   if (fFilledBins) {
      TClass* clArray = GetChunk(0)->fContent->IsA();
      TDataMember* dm = clArray ? clArray->GetDataMember("fArray") : 0;
      arrayElementSize = dm ? dm->GetDataType()->Size() : 0;
   }
   if (!arrayElementSize) {
      Warning("GetSparseFractionMem", "Cannot determine type of elements!");
      return -1.;
   }

   size += fFilledBins * (GetCompactCoord()->GetSize() + arrayElementSize + 2 * sizeof(Long_t) /* TExMap */);
   if (fFilledBins && GetChunk(0)->fSumw2)
      size += fFilledBins * sizeof(Float_t); /* fSumw2 */

   Double_t nbinsTotal = 1.;
   for (UInt_t d = 0; d < fNdimensions; ++d)
      nbinsTotal *= GetAxis(d)->GetNbins() + 2;

   return size / nbinsTotal / arrayElementSize;
}

//______________________________________________________________________________
TH1D* THnSparse::Projection(UInt_t xDim) const
{
   // Project all bins into a 1-dimensional histogram,
   // keeping only axis "xDim".

   TString name(GetName());
   name += "_";
   name += GetAxis(xDim)->GetName();
   TString title(GetTitle());
   Ssiz_t posInsert = title.First(';');
   if (posInsert == kNPOS) {
      title += " projection ";
      title += GetAxis(xDim)->GetTitle();
   } else {
      title.Insert(posInsert, GetAxis(xDim)->GetTitle());
      title.Insert(posInsert, " projection ");
   }

   TH1D* h = new TH1D(name, title, GetAxis(xDim)->GetNbins(),
                      GetAxis(xDim)->GetXmin(), GetAxis(xDim)->GetXmax());

   // Bool_t haveErrors = GetChunk(0) && GetChunk(0)->fSumw2;

   UInt_t* coord = new UInt_t[fNdimensions];
   memset(coord, 0, sizeof(UInt_t) * fNdimensions);
   for (Long64_t i = 0; i < GetNbins(); ++i) {
      Double_t v = GetBinContent(i, coord);
      h->AddBinContent(coord[xDim], v);
   }
   delete [] coord;

   return h;
}

//______________________________________________________________________________
TH2D* THnSparse::Projection(UInt_t xDim, UInt_t yDim) const
{
   // Project all bins into a 2-dimensional histogram,
   // keeping only axes "xDim" and "yDim".

   TString name(GetName());
   name += "_";
   name += GetAxis(xDim)->GetName();
   name += GetAxis(yDim)->GetName();

   TString title(GetTitle());
   Ssiz_t posInsert = title.First(';');
   if (posInsert == kNPOS) {
      title += " projection ";
      title += GetAxis(xDim)->GetTitle();
      title += GetAxis(yDim)->GetTitle();
   } else {
      title.Insert(posInsert, GetAxis(yDim)->GetTitle());
      title.Insert(posInsert, ", ");
      title.Insert(posInsert, GetAxis(xDim)->GetTitle());
      title.Insert(posInsert, " projection ");
   }

   TH2D* h = new TH2D(name, title, GetAxis(xDim)->GetNbins(),
                      GetAxis(xDim)->GetXmin(), GetAxis(xDim)->GetXmax(),
                      GetAxis(yDim)->GetNbins(),
                      GetAxis(yDim)->GetXmin(), GetAxis(yDim)->GetXmax());

   // Bool_t haveErrors = GetChunk(0) && GetChunk(0)->fSumw2;

   UInt_t* coord = new UInt_t[fNdimensions];
   memset(coord, 0, sizeof(UInt_t) * fNdimensions);
   for (Long64_t i = 0; i < GetNbins(); ++i) {
      Double_t v = GetBinContent(i, coord);
      Long_t bin = h->GetBin(coord[xDim], coord[yDim]);
      h->AddBinContent(bin, v);
   }
   delete [] coord;

   return h;
}

//______________________________________________________________________________
TH3D* THnSparse::Projection(UInt_t xDim, UInt_t yDim, UInt_t zDim) const
{
   // Project all bins into a 3-dimensional histogram,
   // keeping only axes "xDim", "yDim", and "zDim".

   TString name(GetName());
   name += "_";
   name += GetAxis(xDim)->GetName();
   name += GetAxis(yDim)->GetName();
   name += GetAxis(zDim)->GetName();

   TString title(GetTitle());
   Ssiz_t posInsert = title.First(';');
   if (posInsert == kNPOS) {
      title += " projection ";
      title += GetAxis(xDim)->GetTitle();
      title += GetAxis(yDim)->GetTitle();
      title += GetAxis(zDim)->GetTitle();
   } else {
      title.Insert(posInsert, GetAxis(zDim)->GetTitle());
      title.Insert(posInsert, ", ");
      title.Insert(posInsert, GetAxis(yDim)->GetTitle());
      title.Insert(posInsert, ", ");
      title.Insert(posInsert, GetAxis(xDim)->GetTitle());
      title.Insert(posInsert, " projection ");
   }

   TH3D* h = new TH3D(name, title, GetAxis(xDim)->GetNbins(),
                      GetAxis(xDim)->GetXmin(), GetAxis(xDim)->GetXmax(),
                      GetAxis(yDim)->GetNbins(),
                      GetAxis(yDim)->GetXmin(), GetAxis(yDim)->GetXmax(),
                      GetAxis(zDim)->GetNbins(),
                      GetAxis(zDim)->GetXmin(), GetAxis(zDim)->GetXmax());

   // Bool_t haveErrors = GetChunk(0) && GetChunk(0)->fSumw2;

   UInt_t* coord = new UInt_t[fNdimensions];
   memset(coord, 0, sizeof(UInt_t) * fNdimensions);
   for (Long64_t i = 0; i < GetNbins(); ++i) {
      Double_t v = GetBinContent(i, coord);
      Long_t bin = h->GetBin(coord[xDim], coord[yDim], coord[zDim]);
      h->AddBinContent(bin, v);
   }
   delete [] coord;

   return h;
}

//______________________________________________________________________________
THnSparse* THnSparse::Projection(UInt_t ndim, UInt_t* dim) const
{
   // Project all bins into a ndim-dimensional histogram,
   // keeping only axes "dim".

   TString name(GetName());
   name += "_";
   for (UInt_t d = 0; d < ndim; ++d)
      name += GetAxis(dim[d])->GetName();

   TString title(GetTitle());
   Ssiz_t posInsert = title.First(';');
   if (posInsert == kNPOS) {
      title += " projection ";
      for (UInt_t d = 0; d < ndim; ++d)
         title += GetAxis(dim[d])->GetTitle();
   } else {
      for (UInt_t d = ndim - 1; d >= 0; --d) {
         title.Insert(posInsert, GetAxis(dim[d])->GetTitle());
         if (dim > 0)
            title.Insert(posInsert, ", ");
      }
      title.Insert(posInsert, " projection ");
   }

   UInt_t* bins = new UInt_t[ndim];
   Double_t* xmin = new Double_t[ndim];
   Double_t* xmax = new Double_t[ndim];
   for (UInt_t d = 0; d < ndim; ++d) {
      bins[d] = GetAxis(dim[d])->GetNbins();
      xmin[d] = GetAxis(dim[d])->GetXmin();
      xmax[d] = GetAxis(dim[d])->GetXmax();
   }

   TString interpNew;
   interpNew.Form("new %s((const char*)0x%x,(const char*)0x%x,%d,(UInt_t*)0x%x,(Double_t*)0x%x,(Double_t*)0x%x)",
                  IsA()->GetName(), name.Data(), title.Data(), bins, xmin, xmax);
   TInterpreter::EErrorCode interpErr = TInterpreter::kNoError;
   THnSparse* h = (THnSparse*) gInterpreter->Calc(interpNew, &interpErr);
   if (interpErr != TInterpreter::kNoError)
      return 0;

   // Bool_t haveErrors = GetChunk(0) && GetChunk(0)->fSumw2;

   UInt_t* coord = new UInt_t[fNdimensions];
   memset(coord, 0, sizeof(UInt_t) * fNdimensions);
   for (Long64_t i = 0; i < GetNbins(); ++i) {
      Double_t v = GetBinContent(i, coord);
      for (UInt_t d = 0; d < ndim; ++d)
         bins[d] = coord[dim[d]];
      h->AddBinContent(bins, v);
   }

   delete [] bins;
   delete [] xmin;
   delete [] xmax;
   delete [] coord;

   return h;
}

//______________________________________________________________________________
void THnSparse::SetBinContent(UInt_t* coord, Double_t v)
{
   // Set content of bin with coordinates "coord" to "v"

   GetCompactCoord()->SetCoord(coord);
   Long_t bin = GetBinIndexForCurrentBin(kTRUE);
   THnSparseArrayChunk* chunk = GetChunk(bin / fChunkSize);
   return chunk->fContent->SetAt(v, bin % fChunkSize);
}

//______________________________________________________________________________
void THnSparse::SetBinError(UInt_t* coord, Double_t e)
{
   // Set error of bin with coordinates "coord" to "v", enable errors if needed

   GetCompactCoord()->SetCoord(coord);
   Long_t bin = GetBinIndexForCurrentBin(kTRUE);
   if (!GetChunk(0)->fSumw2)
      Sumw2();

   THnSparseArrayChunk* chunk = GetChunk(bin / fChunkSize);
   return chunk->fSumw2->SetAt(e, bin % fChunkSize);
}

//______________________________________________________________________________
void THnSparse::Sumw2()
{
   // Enable calculation of errors

   if (GetChunk(0) && GetChunk(0)->fSumw2) return;

   TIter iChunk(&fBinContent);
   THnSparseArrayChunk* chunk = 0;
   while ((chunk = (THnSparseArrayChunk*) iChunk()))
      chunk->Sumw2();
}

//______________________________________________________________________________
void THnSparse::Reset(Option_t * /*option = ""*/)
{
   // Clear the histogram
   fFilledBins = 0;
   fEntries = 0.;
   fBins.Clear();
   fBinsContinued.Clear();
   fBinContent.Delete();
}

