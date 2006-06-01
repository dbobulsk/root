// @(#)root/guibuilder:$Name:  $:$Id: TRootGuiBuilder.h,v 1.10 2006/06/01 09:13:56 antcheva Exp $
// Author: Valeriy Onuchin   12/09/04

/*************************************************************************
 * Copyright (C) 1995-2004, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT_TRootGuiBuilder
#define ROOT_TRootGuiBuilder


//////////////////////////////////////////////////////////////////////////
//                                                                      //
// TRootGuiBuilder                                                      //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef ROOT_TGFrame
#include "TGFrame.h"
#endif
#ifndef ROOT_TGuiBuilder
#include "TGuiBuilder.h"
#endif

enum EGuiBuilderMenuIds {
   kGUIBLD_FILE_NEW,
   kGUIBLD_FILE_CLOSE,
   kGUIBLD_FILE_EXIT,
   kGUIBLD_FILE_START,
   kGUIBLD_FILE_STOP,
   kGUIBLD_FILE_SAVE,

   kGUIBLD_EDIT_PREF,

   kGUIBLD_WINDOW_HOR,
   kGUIBLD_WINDOW_VERT,
   kGUIBLD_WINDOW_CASCADE,
   kGUIBLD_WINDOW_OPAQUE,
   kGUIBLD_WINDOW_ARRANGE,

   kGUIBLD_HELP_CONTENTS,
   kGUIBLD_HELP_ABOUT,
   kGUIBLD_HELP_BUG
};


class TGShutter;
class TGMdiMainFrame;
class TGDockableFrame;
class TGMdiMenuBar;
class TGPopupMenu;
class TGStatusBar;
class TGuiBldDragManager;
class TGToolBar;
class TGMdiFrame;
class TGuiBldEditor;
class TGButton;
class TGPictureButton;
class TImage;
class TTimer;

class TRootGuiBuilder : public TGuiBuilder, public TGMainFrame {

private:
   TGuiBldDragManager *fManager;    // drag and drop manager
   TGButton          *fActionButton;// action button 
   TGToolBar         *fToolBar;     // guibuider toolbar
   TGShutter         *fShutter;     // widget palette
   TGMdiMainFrame    *fMain;        // main mdi frame
   TGDockableFrame   *fToolDock;    // dockable frame where toolbar is located 
   TGDockableFrame   *fShutterDock; // dockable frame where widget palette is located  
   TGMdiMenuBar      *fMenuBar;     // guibuilder menu bar
   TGPopupMenu       *fMenuFile;    // "File" popup menu
   TGPopupMenu       *fMenuWindow;  // "Window" popup menu
   TGPopupMenu       *fMenuEdit;    // "Edit" popup menu
   TGPopupMenu       *fMenuHelp;    // "Help" popup menu
   TGStatusBar       *fStatusBar;   //  guibuilder status bar
   TGFrame           *fSelected;    //  selected frame
   TGMdiFrame        *fEditable;    //  mdi frame where editted frame is  located
   TGuiBldEditor     *fEditor;      // frame property editor
   const TGPicture   *fIconPic;     // icon picture
   TGPictureButton   *fStartButton; // start button

   static TGGC       *fgBgnd;
   static TGGC       *fgBgndPopup;
   static TGGC       *fgBgndPopupHlght;

   void InitMenu();
   void EnableLassoButtons(Bool_t on = kTRUE);
   void EnableSelectedButtons(Bool_t on = kTRUE);
   void EnableEditButtons(Bool_t on = kTRUE);
   void BindKeys();
   TGButton *FindActionButton(const char *name, const char *section);

public:
   TRootGuiBuilder(const TGWindow *p = 0);
   virtual ~TRootGuiBuilder();

   virtual void      AddAction(TGuiBldAction *act, const char *sect);
   virtual void      AddMacro(const char *macro, TImage *img);
   virtual void      AddSection(const char *sect);
   virtual TGFrame  *ExecuteAction();
   virtual void      HandleButtons();
   virtual void      Show() { MapRaised(); }
   virtual void      Hide();
   virtual void      ChangeSelected(TGFrame *f);
   virtual void      Update();
   virtual Bool_t    IsSelectMode() const;
   virtual Bool_t    IsGrabButtonDown() const;
   virtual Bool_t    OpenProject(Event_t *event = 0);
   virtual Bool_t    SaveProject(Event_t *event = 0);
   virtual Bool_t    NewProject(Event_t *event = 0);
   virtual Bool_t    HandleKey(Event_t *event);
   virtual void      HandleMenu(Int_t id);
   virtual void      CloseWindow();
   virtual void      HandleWindowClosed(Int_t id);
   virtual void      UpdateStatusBar(const char *text = 0);
   virtual void      EraseStatusBar();
   virtual void      SwitchToolbarButton();

   TGMdiFrame *FindEditableMdiFrame(const TGWindow *win);
   TGuiBldEditor    *GetEditor() const { return fEditor; }
   TGDockableFrame  *GetToolDock() const { return fToolDock; }
   static TGFrame   *HSplitter();
   static TGFrame   *VSplitter();
   TGMdiMainFrame   *GetMdiMain() const { return fMain; }  
   TGMdiFrame       *GetEditable() const { return fEditable; }

   static ULong_t    GetBgnd();
   static TGGC      *GetBgndGC();

   static ULong_t    GetPopupBgnd();
   static TGGC      *GetPopupBgndGC();

   static ULong_t    GetPopupHlght();
   static TGGC      *GetPopupHlghtGC();

   static void       PropagateBgndColor(TGFrame *frame, Pixel_t color);

   static TGPopupMenu *CreatePopup();
   static TGFrame     *BuildListTree();
   static TGFrame     *BuildCanvas();
   static TGFrame     *BuildShutter();
   static TGFrame     *BuildTextEdit();
   static TGFrame     *BuildTab();
   static TGFrame     *BuildListBox();
   static TGFrame     *BuildComboBox();
   static TGFrame     *BuildH3DLine();
   static TGFrame     *BuildV3DLine();

   ClassDef(TRootGuiBuilder,0)  // ROOT GUI Builder
};


#endif
