/* @(#)root/gl:$Name:  $:$Id: LinkDef.h,v 1.1.1.1 

/*************************************************************************
 * Copyright (C) 1995-2000, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifdef __CINT__

#pragma link off all globals;
#pragma link off all classes;
#pragma link off all functions;

#pragma link C++ class TGLVertex3;
#pragma link C++ class TGLVector3;
#pragma link C++ class TGLRect;
#pragma link C++ class TGLPlane;
#pragma link C++ class TGLMatrix;
#pragma link C++ class TGLUtil;

#pragma link C++ class TGLBoundingBox;
#pragma link C++ class TGLDrawable;
#pragma link C++ class TGLLogicalShape;
#pragma link C++ class TGLPhysicalShape;

#pragma link C++ class TGLCamera;
#pragma link C++ class TGLOrthoCamera;
#pragma link C++ class TGLPerspectiveCamera;

#pragma link C++ class TGLScene;
#pragma link C++ class TGLDisplayListCache;

#pragma link C++ class TGLStopwatch;
#pragma link C++ class TGLViewer;
#pragma link C++ class TGLSAViewer;
#pragma link C++ class TGLSAFrame;

#pragma link C++ class TGLColorEditor;
#pragma link C++ class TGLGeometryEditor;
#pragma link C++ class TGLSceneEditor;
#pragma link C++ class TGLLightEditor;

#pragma link C++ class TArcBall;

#pragma link C++ class TGLRenderArea;
#pragma link C++ class TGLWindow;
#pragma link C++ class TGLSceneObject;
#pragma link C++ class TGLFaceSet;
#pragma link C++ class TGLPolyLine;
#pragma link C++ class TGLPolyMarker;
#pragma link C++ class TGLCylinder;
#pragma link C++ class TGLSphere;
#ifndef _WIN32
#pragma link C++ class TX11GL;
#endif

#endif
