// @(#)root/base:$Name:  $:$Id:$
// Author: Richard Maunder  10/3/2005

/*************************************************************************
 * Copyright (C) 1995-2000, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include "TViewer3DPad.h"
#include "TVirtualPad.h"
#include "TView.h"
#include "TBuffer3D.h"
#include "TBuffer3DTypes.h"

#include <assert.h>

//______________________________________________________________________________
Bool_t TViewer3DPad::PreferLocalFrame() const
{
   return kFALSE;
}


//______________________________________________________________________________
void TViewer3DPad::BeginScene()
{
   assert(!fBuilding);

   // Create a 3D view if none exists
   TView *view = fPad.GetView();
   if (!view) {
      view = new TView(1);
      if (!view) {
         assert(kFALSE);
         return;
      }
      view->SetAutoRange(kTRUE);
      fPad.SetView(view);
   }
   if (!view->IsPerspective()) view->SetPerspective();
   fBuilding = kTRUE;
}

//______________________________________________________________________________
void TViewer3DPad::EndScene()
{
   assert(fBuilding);

   fBuilding= kFALSE;

   // If we are doing for auto-range pass on view invoke another pass
   TView *view = fPad.GetView();
   if (view) {
      if (view->GetAutoRange()) {
         view->SetAutoRange(kFALSE);
         fPad.Paint();
      }
   } else {
      assert(kFALSE);
      return;
   }
}

//______________________________________________________________________________
Int_t TViewer3DPad::AddObject(const TBuffer3D & buffer, Bool_t * addChildren)
{
   // Accept any children
   if (addChildren) {
      *addChildren = kTRUE;
   }

   TView * view = fPad.GetView();
   if (!view) {
      assert(kFALSE);
      return TBuffer3D::kNone;
   }

   UInt_t reqSections = TBuffer3D::kCore|TBuffer3D::kRawSizes|TBuffer3D::kRaw;
   if (!buffer.SectionsValid(reqSections)) {
      return reqSections;
   }

   UInt_t i;
   Int_t i0, i1, i2;

   // Range pass
   if (view->GetAutoRange()) {
      Double_t x0, y0, z0, x1, y1, z1;

      x0 = x1 = buffer.fPnts[0];
      y0 = y1 = buffer.fPnts[1];
      z0 = z1 = buffer.fPnts[2];
      for (i=1; i<buffer.NbPnts(); i++) {
         i0 = 3*i; i1 = i0+1; i2 = i0+2;
         x0 = buffer.fPnts[i0] < x0 ? buffer.fPnts[i0] : x0;
         y0 = buffer.fPnts[i1] < y0 ? buffer.fPnts[i1] : y0;
         z0 = buffer.fPnts[i2] < z0 ? buffer.fPnts[i2] : z0;
         x1 = buffer.fPnts[i0] > x1 ? buffer.fPnts[i0] : x1;
         y1 = buffer.fPnts[i1] > y1 ? buffer.fPnts[i1] : y1;
         z1 = buffer.fPnts[i2] > z1 ? buffer.fPnts[i2] : z1;
      }
      view->SetRange(x0,y0,z0,x1,y1,z1,2);
   }
   // Actual drawing pass
   else {
      // Do not show semi transparent objects
      if (buffer.fTransparency > 50) {
         return TBuffer3D::kNone;
      }
      if (buffer.Type()== TBuffer3DTypes::kMarker ) {
         Double_t pndc[3], temp[3];
         for (i=0; i<buffer.NbPnts(); i++) {
            for ( i0=0; i0<3; i0++ ) temp[i0] = buffer.fPnts[3*i+i0];
            view->WCtoNDC(temp, pndc);
            fPad.PaintPolyMarker(1, &pndc[0], &pndc[1]);
         }
      } else {
         for (i=0; i<buffer.NbSegs(); i++) {
            i0 = 3*buffer.fSegs[3*i+1];
            Double_t *ptpoints_0 = &(buffer.fPnts[i0]);
            i0 = 3*buffer.fSegs[3*i+2];
            Double_t *ptpoints_3 = &(buffer.fPnts[i0]);
            fPad.PaintLine3D(ptpoints_0, ptpoints_3);
         }
      }
   }

   return TBuffer3D::kNone;
}

//______________________________________________________________________________
Int_t TViewer3DPad::AddObject(UInt_t /*placedID*/, const TBuffer3D & buffer, Bool_t * addChildren)
{
   // We don't support placedID - discard
   return AddObject(buffer,addChildren);
}
