// @(#)root/net:$Name:  $:$Id: TNetFile.h,v 1.11 2003/08/29 10:41:28 rdm Exp $
// Author: Fons Rademakers   14/08/97

/*************************************************************************
 * Copyright (C) 1995-2000, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT_TNetFile
#define ROOT_TNetFile


//////////////////////////////////////////////////////////////////////////
//                                                                      //
// TNetFile                                                             //
//                                                                      //
// A TNetFile is like a normal TFile except that it reads and writes    //
// its data via a rootd server.                                         //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef ROOT_TFile
#include "TFile.h"
#endif
#ifndef ROOT_TUrl
#include "TUrl.h"
#endif
#ifndef ROOT_MessageTypes
#include "MessageTypes.h"
#endif

class TSocket;


class TNetFile : public TFile {

protected:
   TUrl      fUrl;        //URL of file
   TString   fUser;       //remote user name
   Seek_t    fOffset;     //seek offset
   TSocket  *fSocket;     //connection to rootd server
   Int_t     fProtocol;   //rootd protocol level
   Int_t     fErrorCode;  //error code returned by rootd (matching gRootdErrStr)

   static Int_t fgClientProtocol;  //client protocol level

   TNetFile(const char *url, const char *ftitle, Int_t comp, Bool_t);
   virtual void ConnectServer(Int_t *stat, EMessageTypes *kind, Int_t netopt,
                              Int_t tcpwindowsize, Bool_t forceOpen,
                              Bool_t forceRead);
   virtual void Create(const char *url, Option_t *option, Int_t netopt);
   void   Init(Bool_t create);
   void   Print(Option_t *option) const;
   void   PrintError(const char *where, Int_t err);
   Int_t  Recv(Int_t &status, EMessageTypes &kind);
   Int_t  SysOpen(const char *pathname, Int_t flags, UInt_t mode);
   Int_t  SysClose(Int_t fd);
   Int_t  SysStat(Int_t fd, Long_t *id, Long_t *size, Long_t *flags, Long_t *modtime);

public:
   TNetFile(const char *url, Option_t *option = "", const char *ftitle = "",
            Int_t compress = 1, Int_t netopt = 0);
   TNetFile() : fUrl("dummy") { fSocket = 0; }
   virtual ~TNetFile();

   void    Close(Option_t *option="");  //*MENU*
   void    Flush();
   Int_t   GetErrorCode() const { return fErrorCode; }
   Bool_t  IsOpen() const;
   Int_t   ReOpen(Option_t *mode);
   Bool_t  ReadBuffer(char *buf, Int_t len);
   Bool_t  WriteBuffer(const char *buf, Int_t len);
   void    Seek(Seek_t offset, ERelativeTo pos = kBeg);

   static  Int_t GetClientProtocol();

   ClassDef(TNetFile,1)  //A ROOT file that reads/writes via a rootd server
};

#endif
