// @(#)root/gl:$Name:  $:$Id: TGLRotateManip.cxx
// Author:  Richard Maunder  04/10/2005

/*************************************************************************
 * Copyright (C) 1995-2005, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include "TGLRotateManip.h"
#include "TGLPhysicalShape.h"
#include "TGLCamera.h"
#include "TGLIncludes.h"
#include "TMath.h"
#include "TError.h"

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// TGLRotateManip                                                       //
//                                                                      //
// Rotate manipulator - attaches to physical shape and draws local axes //
// widgets - rings drawn from attached physical center, in plane defined// 
// by axis. User can mouse over (turns yellow) and L click/drag to      //
// rotate attached physical round the ring center.                      // 
// Widgets use standard 3D package axes colours: X red, Y green, Z blue.//
//////////////////////////////////////////////////////////////////////////

ClassImp(TGLRotateManip)

//______________________________________________________________________________
TGLRotateManip::TGLRotateManip(TGLViewer & viewer) : 
   TGLManip(viewer),
   fShallowRing(kFALSE), fShallowFront(kTRUE),
   fActiveRingPlane(TGLVector3(1.0, 0.0, 0.0), TGLVertex3(0.0, 0.0, 0.0)),
   fActiveRingCenter(TGLVertex3(0.0, 0.0, 0.0)),
   fRingLine(TGLVertex3(0.0, 0.0, 0.0), TGLVertex3(0.0, 0.0, 0.0)),
   fRingLineOld(TGLVertex3(0.0, 0.0, 0.0), TGLVertex3(0.0, 0.0, 0.0))
{
   // Construct rotation manipulator, attached to supplied TGLViewer 
   // 'viewer', not bound to any physical shape.
}

//______________________________________________________________________________
TGLRotateManip::TGLRotateManip(TGLViewer & viewer, TGLPhysicalShape * shape) : 
   TGLManip(viewer, shape),
   fShallowRing(kFALSE), fShallowFront(kTRUE),
   fActiveRingPlane(TGLVector3(1.0, 0.0, 0.0), TGLVertex3(0.0, 0.0, 0.0)),
   fActiveRingCenter(TGLVertex3(0.0, 0.0, 0.0)),
   fRingLine(TGLVertex3(0.0, 0.0, 0.0), TGLVertex3(0.0, 0.0, 0.0)),
   fRingLineOld(TGLVertex3(0.0, 0.0, 0.0), TGLVertex3(0.0, 0.0, 0.0))
{
   // Construct rotation manipulator, attached to supplied TGLViewer 
   // 'viewer', bound to TGLPhysicalShape 'shape'.
}

//______________________________________________________________________________
TGLRotateManip::~TGLRotateManip() 
{
   // Destory the rotation manipulator
}
   
//______________________________________________________________________________
void TGLRotateManip::Draw(const TGLCamera & camera) const
{
   // Draw rotate manipulator - axis rings drawn from attached physical center,
   // in plane defined by axis as normal, in red(X), green(Y) and blue(Z), with 
   // white center sphere. If selected widget (mouse over) this is drawn in active 
   // colour (yellow).
   if (!fShape) {
      return;
   }

   const TGLBoundingBox & box = fShape->BoundingBox();
   Double_t widgetScale = CalcDrawScale(box, camera);
   Double_t ringRadius = widgetScale*10.0;

   // Get permitted manipulations on shape
   TGLPhysicalShape::EManip manip = fShape->GetManip();

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glDisable(GL_CULL_FACE);

   // Draw three axis rings where permitted
   // Not drawing will prevent interaction
   // GL name loading for hit testing - 0 reserved for no selection
   if (manip & TGLPhysicalShape::kRotateX) {
      glPushName(1);
      TGLUtil::DrawRing(box.Center(), box.Axis(0, kTRUE), ringRadius*1.004, 
                        fSelectedWidget == 1 ? fgYellow : fgRed);
      glPopName();
   } else {
      TGLUtil::DrawRing(box.Center(), box.Axis(0, kTRUE), ringRadius*1.004, fgGrey);
   }
   if (manip & TGLPhysicalShape::kRotateY) {
      glPushName(2);
      TGLUtil::DrawRing(box.Center(), box.Axis(1, kTRUE), ringRadius*1.002, 
                        fSelectedWidget == 2 ? fgYellow : fgGreen);
      glPopName();
   } else {
      TGLUtil::DrawRing(box.Center(), box.Axis(1, kTRUE), ringRadius*1.002, fgGrey);
   }
   if (manip & TGLPhysicalShape::kRotateZ) {
      glPushName(3);
      TGLUtil::DrawRing(box.Center(), box.Axis(2, kTRUE), ringRadius, 
                        fSelectedWidget == 3 ? fgYellow : fgBlue);
      glPopName();
   } else {
      TGLUtil::DrawRing(box.Center(), box.Axis(2, kTRUE), ringRadius, fgGrey);
   }
   // Draw white center sphere
   TGLUtil::DrawSphere(box.Center(), ringRadius/20.0, fgWhite);

   // Indicate we are in ring follow (non-shallow) mode 
   // by drawing line from center to dragged ring point 
   if (fActive) {
      if (fShallowRing) {
         TGLVertex3 eyeOnRing;
         if (fShallowFront) {
            eyeOnRing = fActiveRingCenter - (camera.EyeDirection()*ringRadius);
         } else {
            eyeOnRing = fActiveRingCenter + (camera.EyeDirection()*ringRadius);
         }

         eyeOnRing = fActiveRingPlane.NearestOn(eyeOnRing);
         TGLVector3 arrowDir = Cross(fActiveRingPlane.Norm(), eyeOnRing - fActiveRingCenter);
         arrowDir.Normalise();
         TGLUtil::DrawLine(eyeOnRing, arrowDir*ringRadius*1.3, TGLUtil::kLineHeadArrow, widgetScale, fgYellow);
         TGLUtil::DrawLine(eyeOnRing, -arrowDir*ringRadius*1.3, TGLUtil::kLineHeadArrow, widgetScale, fgYellow);
      } else {
         TGLVector3 activeVector = fRingLine.Vector();
         activeVector.Normalise();
         activeVector *= ringRadius;
         TGLUtil::DrawLine(fRingLine.Start(), activeVector, 
                           TGLUtil::kLineHeadNone, widgetScale, fgYellow);
      }
   }

   glEnable(GL_CULL_FACE);
   glDisable(GL_BLEND);
}

//______________________________________________________________________________
Bool_t TGLRotateManip::HandleButton(const Event_t * event, const TGLCamera & camera)
{
   // Handle mouse button event over manipulator - returns kTRUE if redraw required 
   // kFALSE otherwise.
   Bool_t captured = TGLManip::HandleButton(event, camera);

   if (captured) {
      // Find active selected axis
      UInt_t axisIndex = fSelectedWidget - 1; // Ugg sort out axis / widget id mapping
      TGLVector3 widgetAxis = fShape->BoundingBox().Axis(axisIndex, kTRUE);

      // Construct plane for the axis ring, using normal and center point
      fActiveRingPlane.Set(widgetAxis, fShape->BoundingBox().Center());
      fActiveRingCenter.Set(fShape->BoundingBox().Center());

      fRingLineOld = fRingLine = CalculateRingLine(fLastMouse, camera);
      
      // Is plane at shallow angle to eye line if angle between normal of plane and
      // eye line is ~90 deg (PI/4)
      Double_t planeEyeAngle = Angle(fActiveRingPlane.Norm(), camera.EyeDirection()) - TMath::ASin(1.0);
      Double_t shallowDelta = 0.15;
      if ((planeEyeAngle > -shallowDelta) && (planeEyeAngle < shallowDelta)) {  
         fShallowRing = kTRUE;

         // Work out ring follow direction - if clicked on back or front of ring.
         // If plane/eye angle very shallow force to front
			
			/* DISABLED - Force onto front always */
			fShallowFront = kTRUE;
			/*
         if ((planeEyeAngle > -shallowDelta/3.0) && (planeEyeAngle < shallowDelta/3.0) ||
             Dot(fRingLine.Vector(), camera.FrustumPlane(TGLCamera::kNear).Norm()) < 0.0) {
            fShallowFront = kTRUE;
         } else {
            fShallowFront = kFALSE;
         }*/
      } else {
         fShallowRing = kFALSE;
      }
   }

   return captured;
}

//______________________________________________________________________________
Bool_t TGLRotateManip::HandleMotion(const Event_t * event, const TGLCamera & camera)
{
   // Handle mouse motion over manipulator - if active (selected widget) rotate 
   // physical around selected ring widget plane normal. Returns kTRUE if redraw 
   // required kFALSE otherwise.
   if (fActive) {
      TPoint newMouse(event->fX, event->fY);

      // Calculate singed angle delta between old and new ring position using
      Double_t angle = CalculateAngleDelta(newMouse, camera);
      fShape->Rotate(fActiveRingCenter, fActiveRingPlane.Norm(), angle);
      fLastMouse = newMouse;
      return kTRUE;
   } else {
      return TGLManip::HandleMotion(event, camera);
   }
}

//______________________________________________________________________________
Double_t TGLRotateManip::CalculateAngleDelta(const TPoint & mouse, const TGLCamera & camera)
{
   // Calculate angle delta for rotation based on new mouse position
   if (fShallowRing) {
      std::pair<Bool_t, TGLLine3> nearLineIntersection = Intersection(fActiveRingPlane, 
                                                                      camera.FrustumPlane(TGLCamera::kNear));
      if (!nearLineIntersection.first) {
         Error("TGLRotateManip::CalculateAngleDelta", "active ring plane parallel to near clip?");
         return 1.0;
      }
      TGLLine3 nearLine = nearLineIntersection.second;
      TGLVector3 activePlaneNear = camera.WorldDeltaToViewport(nearLine.Start(), nearLine.Vector());
		activePlaneNear.Normalise();
      TGLVector3 mouseDelta(mouse.GetX() - fLastMouse.GetX(),
                            -(mouse.GetY() - fLastMouse.GetY()),
                            0.0);

      Double_t angle = Dot(activePlaneNear, mouseDelta) / 150.0;
      if (fShallowFront) {
         return -angle;
      } else {
         return angle;
      }
   } else {
      fRingLineOld = fRingLine;
      fRingLine = CalculateRingLine(fLastMouse, camera);
      return Angle(fRingLineOld.Vector(), fRingLine.Vector(), fActiveRingPlane.Norm());
   }
}

//______________________________________________________________________________
TGLLine3 TGLRotateManip::CalculateRingLine(const TPoint & mouse, const TGLCamera & camera) const
{
   // Calculated interaction line between 'mouse' viewport point, and current selected
   // widget (ring), under supplied 'camera' projection.
   // Find mouse position in viewport coords
   TPoint mouseViewport(mouse);
   camera.WindowToViewport(mouseViewport);

   // Find projection of mouse into world
   TGLLine3 viewportProjection = camera.ViewportToWorld(mouseViewport);

   // Find rotation line from ring center to this intersection on plane
   std::pair<Bool_t, TGLVertex3> ringPlaneInter =  Intersection(fActiveRingPlane, viewportProjection, kTRUE);
   
   // If intersection fails then ring is parallel to eye line - in this case
   // force line to run from center back towards viewer (opposite eye line)
   if (!ringPlaneInter.first) {
      return TGLLine3(fActiveRingCenter, -camera.EyeDirection());
   }
   return TGLLine3(fActiveRingCenter, ringPlaneInter.second);
}

