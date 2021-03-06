// @(#)root/roostats:$Id$
// Author: Danilo Piparo   25/08/2009


/*************************************************************************
 * Copyright (C) 1995-2008, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

//___________________________________________________
/*
BEGIN_HTML
<p>
HLFactory is an High Level model Factory allows you to
describe your models in a configuration file
(<i>datacards</i>) acting as an interface with the RooFactoryWSTool.
Moreover it provides tools for the combination of models and datasets.
</p>

END_HTML
*/

#include <iostream>
#include <fstream>

#include "RooStats/HLFactory.h"
#include "TFile.h"
#include "TObject.h"
#include "TObjArray.h"
#include "TObjString.h"

#include "RooSimultaneous.h"

ClassImp(RooStats::HLFactory) ;


using namespace RooStats;
using namespace RooFit;

//_______________________________________________________
HLFactory::HLFactory(const char *name,
                     const char *fileName,
                     bool isVerbose):
    TNamed(name,name),
    fComboCat(0),
    fComboBkgPdf(0),
    fComboSigBkgPdf(0),
    fComboDataset(0),
    fCombinationDone(false),
    fVerbose(isVerbose),
    fInclusionLevel(0),
    fOwnWs(true){
    // Constructor with the name of the config file to interpret and the
    // verbosity flag. The extension for the config files is assumed to
    // be ".rs".

    TString wsName(name);
    wsName+="_ws";
    fWs = new RooWorkspace(wsName,true);

    fSigBkgPdfNames.SetOwner();
    fBkgPdfNames.SetOwner();
    fDatasetsNames.SetOwner();

    // Start the parsing
    fReadFile(fileName);
}

//_______________________________________________________
HLFactory::HLFactory(const char* name,
                     RooWorkspace* externalWs,
                     bool isVerbose):
    TNamed(name,name),
    fComboCat(0),
    fComboBkgPdf(0),
    fComboSigBkgPdf(0),
    fComboDataset(0),
    fCombinationDone(false),
    fVerbose(isVerbose),
    fInclusionLevel(0),
    fOwnWs(false){
    // Constructor without a card but with an exrernal workspace.

    fWs=externalWs;
    fSigBkgPdfNames.SetOwner();
    fBkgPdfNames.SetOwner();
    fDatasetsNames.SetOwner();

}

//_______________________________________________________
HLFactory::HLFactory():
    TNamed("hlfactory","hlfactory"),
    fComboCat(0),
    fComboBkgPdf(0),
    fComboSigBkgPdf(0),
    fComboDataset(0),
    fCombinationDone(false),
    fVerbose(false),
    fInclusionLevel(0),
    fOwnWs(true){

    fWs = new RooWorkspace("hlfactory_ws",true);

    fSigBkgPdfNames.SetOwner();
    fBkgPdfNames.SetOwner();
    fDatasetsNames.SetOwner();

    }

//_______________________________________________________
HLFactory::~HLFactory(){
    // destructor

    if (fComboSigBkgPdf!=NULL)
        delete fComboSigBkgPdf;
    if (fComboBkgPdf!=NULL)
        delete fComboBkgPdf;
    if (fComboDataset!=NULL)
        delete fComboDataset;
    if (fComboCat!=NULL)
        delete fComboCat;

    if (fOwnWs)
        delete fWs;
}

//_______________________________________________________
int HLFactory::AddChannel(const char* label,
                          const char* SigBkgPdfName,
                          const char* BkgPdfName,
                          const char* DatasetName){
    // Add a channel to the combination. The channel can be specified as:
    //  - A signal plus background pdf
    //  - A background only pdf
    //  - A dataset
    // Once the combination of the pdfs is done, no more channels should be
    // added.

    if (fCombinationDone){
        std::cerr << "Cannot add anymore channels. "
                  << "Combination already carried out.\n";
        return -1;
        }

    if (SigBkgPdfName!=0){
        if (fWs->pdf(SigBkgPdfName)==NULL){
            std::cerr << "Pdf " << SigBkgPdfName << " not found in workspace!\n";
            return -1;
            }
        TObjString* name = new TObjString(SigBkgPdfName);
        fSigBkgPdfNames.Add(name);
        }

    if (BkgPdfName!=0){
        if (fWs->pdf(BkgPdfName)==NULL){
            std::cerr << "Pdf " << BkgPdfName << " not found in workspace!\n";
            return -1;
            }
        TObjString* name = new TObjString(BkgPdfName);
        fBkgPdfNames.Add(name);
        }

    if (DatasetName!=0){
        if (fWs->data(DatasetName)==NULL){
            std::cerr << "Dataset " << DatasetName << " not found in workspace!\n";
            return -1;
            }
        TObjString* name = new TObjString(DatasetName);
        fDatasetsNames.Add(name);
        }

    if (label!=0){
        TObjString* name = new TObjString(label);
        fLabelsNames.Add(name);
        }
    return 0;

}

//_______________________________________________________
RooAbsPdf* HLFactory::GetTotSigBkgPdf(){
    // Return the combination of the signal plus background channels.
    // The facory owns the object.

    if (fSigBkgPdfNames.GetSize()==0)
        return 0;

    if (fComboSigBkgPdf!=NULL)
        return fComboSigBkgPdf;

    if (!fNamesListsConsistent())
        return NULL;

    if (fSigBkgPdfNames.GetSize()==1){
        TString name(((TObjString*)fSigBkgPdfNames.At(0))->String());
        fComboSigBkgPdf=fWs->pdf(name);
        return fComboSigBkgPdf;
        }

    if (!fCombinationDone)
        fCreateCategory();

    RooArgList pdfs("pdfs");

    TIterator* it=fSigBkgPdfNames.MakeIterator();
    TObjString* ostring;
    TObject* obj;
    while ((obj = it->Next())){
        ostring=(TObjString*) obj;
        pdfs.add( *(fWs->pdf(ostring->String())) );
        }
    delete it;

    TString name(GetName());
    name+="_sigbkg";

    TString title(GetName());
    title+="_sigbkg";

    fComboSigBkgPdf=
      new RooSimultaneous(name,
                          title,
                          pdfs,
                          *fComboCat);

    return fComboSigBkgPdf;

    }
//_______________________________________________________
RooAbsPdf* HLFactory::GetTotBkgPdf(){
    // Return the combination of the background only channels.
    // If no background channel is specified a NULL pointer is returned.
    // The facory owns the object.

    if (fBkgPdfNames.GetSize()==0)
        return 0;

    if (fComboBkgPdf!=NULL)
        return fComboBkgPdf;

    if (!fNamesListsConsistent())
        return NULL;

    if (fBkgPdfNames.GetSize()==1){
        fComboBkgPdf=fWs->pdf(((TObjString*)fBkgPdfNames.First())->String());
        return fComboBkgPdf;
        }

    if (!fCombinationDone)
        fCreateCategory();

    RooArgList pdfs("pdfs");

    TIterator* it = fBkgPdfNames.MakeIterator();
    TObjString* ostring;
    TObject* obj;
    while ((obj = it->Next())){
        ostring=(TObjString*) obj;
        pdfs.add( *(fWs->pdf(ostring->String())) );
        }

    TString name(GetName());
    name+="_bkg";

    TString title(GetName());
    title+="_bkg";

    fComboBkgPdf=
      new RooSimultaneous(name,
                          title,
                          pdfs,
                          *fComboCat);

    return fComboBkgPdf;

}

//_______________________________________________________
RooDataSet* HLFactory::GetTotDataSet(){
   // Return the combination of the datasets.
   // If no dataset is specified a NULL pointer is returned.
   // The facory owns the object.

    if (fDatasetsNames.GetSize()==0)
        return 0;

    if (fComboDataset!=NULL)
        return fComboDataset;

    if (!fNamesListsConsistent())
        return NULL;

    if (fDatasetsNames.GetSize()==1){
        fComboDataset=(RooDataSet*)fWs->data(((TObjString*)fDatasetsNames.First())->String());
        return fComboDataset;
        }

    if (!fCombinationDone)
        fCreateCategory();

    TIterator* it = fDatasetsNames.MakeIterator();
    TObjString* ostring;
    TObject* obj = it->Next();
    ostring = (TObjString*) obj;
    fComboDataset = (RooDataSet*) fWs->data(ostring->String()) ;
    fComboDataset->Print();
    TString dataname(GetName());
    fComboDataset = new RooDataSet(*fComboDataset,dataname+"_TotData");
    int catindex=0;
    fComboCat->setIndex(catindex);
    fComboDataset->addColumn(*fComboCat);
    while ((obj = it->Next())){
        ostring=(TObjString*) obj;
        catindex++;
        RooDataSet* dummy = new RooDataSet(*(RooDataSet*)fWs->data(ostring->String()),"");
        fComboCat->setIndex(catindex);
        fComboCat->Print();
        dummy->addColumn(*fComboCat);
        fComboDataset->append(*dummy);
        delete dummy;
        }

    delete it;
    return fComboDataset;

}

//_______________________________________________________
RooCategory* HLFactory::GetTotCategory(){
   // Return the category.
   // The facory owns the object.

    if (fComboCat!=NULL)
        return fComboCat;

    if (!fNamesListsConsistent())
        return NULL;

    if (!fCombinationDone)
        fCreateCategory();

    return fComboCat;

    }

//_______________________________________________________
int HLFactory::ProcessCard(const char* filename){
    // Process an additional configuration file
    return fReadFile(filename,0);
    }

//_______________________________________________________
int HLFactory::fReadFile(const char*fileName, bool is_included){
    // Parses the configuration file. The objects can be specified following
    // the rules of the RooFactoryWSTool, plus some more flexibility.
    //
    // The official format for the datacards is ".rs".
    //
    // All the instructions end with a ";" (like in C++).
    //
    // Carriage returns and white lines are irrelevant but adviced since they
    // improve readability (like in C++).
    //
    // The (Roo)ClassName::objname(description) can be replaced with the more
    // "pythonic" objname = (Roo)ClassName(description).
    //
    // The comments can be specified with a "//" if on a single line or with
    // /* */ if on multiple lines (like in C++).
    //
    // The "#include path/to/file.rs" statement triggers the inclusion of a
    // configuration fragment.
    //
    // The "import myobject:myworkspace:myrootfile" will add to the Workspace
    // the object myobject located in myworkspace recorded in myrootfile.
    // Alternatively, one could choose the "import myobject:myrootfile" in case
    // no Workspace is present.
    //
    // The "echo" statement prompts a message on screen.

    // Check the deepness of the inclusion
    if (is_included)
        fInclusionLevel+=1;
    else
        fInclusionLevel=0;

    const int maxDeepness=50;
    if (fInclusionLevel>maxDeepness){
        TString warning("The inclusion stack is deeper than ");
        warning+=maxDeepness;
        warning+=". Is this a recursive inclusion?";
        Warning("fReadFile", "%s", warning.Data());
        }


    // open the config file and go through it
    std::ifstream ifile(fileName);

    if(ifile.fail()){
        TString error("File ");
        error+=fileName;
        error+=" could not be opened.";
        Error("fReadFile", "%s", error.Data());
        return -1;
        }

    TString ifileContent("");
    ifileContent.ReadFile(ifile);
    ifile.close();

    // Tokenise the file using the "\n" char and parse it line by line to strip
    // the comments.
    TString ifileContentStripped("");

    TObjArray* lines_array = ifileContent.Tokenize("\n");
    TIterator* lineIt=lines_array->MakeIterator();

    bool in_comment=false;
    TString line;
    TObject* line_o;

    while((line_o=(*lineIt)())){ // Start iteration on lines array
        line = (static_cast<TObjString*>(line_o))->GetString();

        // Are we in a multiline comment?
        if (in_comment)
            if (line.EndsWith("*/")){
                in_comment=false;
                if (fVerbose) Info("fReadFile","Out of multiline comment ...");

                continue;
                }

        // Was line a single line comment?

        if ((line.BeginsWith("/*") && line.EndsWith("*/")) ||
            line.BeginsWith("//")){
            if (fVerbose) Info("fReadFile","In single line comment ...");
            continue;
            }

        // Did a multiline comment just begin?
        if (line.BeginsWith("/*")){
            in_comment=true;
            if (fVerbose) Info("fReadFile","In multiline comment ...");
            continue;
            }

        ifileContentStripped+=line+"\n";
        }

    delete lines_array;
    delete lineIt;

    // Now proceed with the parsing of the stripped file

    lines_array = ifileContentStripped.Tokenize(";");
    lineIt=lines_array->MakeIterator();
    in_comment=false;

    const int nNeutrals=2;
    TString neutrals[nNeutrals]={"\t"," "};

    while((line_o=(*lineIt)())){

        line = (static_cast<TObjString*>(line_o))->GetString();

        // Strip spaces at the beginning and the end of the line
        line.Strip(TString::kBoth,' ');

        // Put the single statement in one single line
        line.ReplaceAll("\n","");

        // Do we have an echo statement? "A la RooFit"
        if (line.BeginsWith("echo")){
            line = line(5,line.Length()-1);
            if (fVerbose)
              std::cout << "Echoing line " << line.Data() << std::endl;
            std::cout << "[" << GetName() << "] echo: "
                      << line.Data() << std::endl;
            continue;
            }

        // Spaces and tabs at this point are not needed.
        for (int i=0;i<nNeutrals;++i)
            line.ReplaceAll(neutrals[i],"");


        if (fVerbose) Info("fReadFile","Reading --> %s <--", line.Data());

        // Was line a white space?
        if (line == ""){
            if (fVerbose) Info("fReadFile", "%s", "Empty line: skipping ...");
            continue;
            }

        // Do we have an include statement?
        // We treat this recursively.
        if (line.BeginsWith("#include")){
            line.ReplaceAll("#include","");
            if (fVerbose) Info("fReadFile","Reading included file...");
            fReadFile(line,true);
            continue;
            }

        // We parse the line
        if (fVerbose) Info("fReadFile","Parsing the line...");
        fParseLine(line);
        }

    delete lineIt;
    delete lines_array;

    return 0;
}


//_______________________________________________________
void HLFactory::fCreateCategory(){
    // Builds the category necessary for the mutidimensional models. Its name
    // will be <HLFactory name>_category and the types are specified by the
    // model labels.

    fCombinationDone=true;

    TString name(GetName());
    name+="_category";

    TString title(GetName());
    title+="_category";

    fComboCat=new RooCategory(name,title);

    TIterator* it=fLabelsNames.MakeIterator();
    TObjString* ostring;
    TObject* obj;
    while ((obj = it->Next())){
        ostring=(TObjString*) obj;
        fComboCat->defineType(ostring->String());
        }

    }

//_______________________________________________________
bool HLFactory::fNamesListsConsistent(){
    // Check the number of entries in each list. If not the same and the list
    // is not empty prompt an error.

    if ((fSigBkgPdfNames.GetEntries()==fBkgPdfNames.GetEntries() || fBkgPdfNames.GetEntries()==0) &&
        (fSigBkgPdfNames.GetEntries()==fDatasetsNames.GetEntries() || fDatasetsNames.GetEntries()==0) &&
        (fSigBkgPdfNames.GetEntries()==fLabelsNames.GetEntries() || fLabelsNames.GetEntries()==0))
        return true;
    else{
        std::cerr << "The number of datasets and models added as channels "
                  << " is not the same!\n";
        return false;
        }
    }

//_______________________________________________________
int HLFactory::fParseLine(TString& line){
    // Parse a single line and puts the content in the RooWorkSpace

    if (fVerbose) Info("fParseLine", "Parsing line: %s", line.Data());

    TString new_line("");

    const int nequals = line.CountChar('=');

    // Build with the factory a var or cat, or pipe the command directly.

    if (line.Contains("::") || // It is a ordinary statement
        nequals==0 || //it is a RooRealVar or cat with 0,1,2,3.. indexes
        (line.Contains("[") &&
         line.Contains("]") &&
         nequals>0 &&    // It is a cat like "tag[B0=1,B0bar=-1]"
         ! line.Contains("(") &&
         ! line.Contains(")"))) {
      fWs->factory(line);
      return 0;
      }

    // Transform the line o_name = o_class(o_descr) in o_class::o_name(o_descr)
    if (nequals==1 ||
        (nequals > 1 &&  line.Contains("SIMUL"))){

        // Divide the line in 3 components: o_name,o_class and o_descr
        // assuming that o_name=o_class(o_descr)
        const int equal_index=line.First('=');
        const int par_index=line.First('(');
        TString o_name(line(0,equal_index));
        TString o_class(line(equal_index+1,par_index-equal_index-1));
        TString o_descr(line(par_index+1,line.Length()-par_index-2));

        if (fVerbose) Info("fParseLine", "o_name=%s o_class=%s o_descr=%s",
                           o_name.Data(), o_class.Data(), o_descr.Data());

        // Now two cases either we wanna produce an object or import something
        // under a new name.
        if (o_class =="import"){// import a generic TObject into the WS
        // Now see if we have a workspace or not, according to the number of
        // entries in the description..

        TObjArray* descr_array = o_descr.Tokenize(",");

        const int n_descr_parts=descr_array->GetEntries();

        if (n_descr_parts<2 || n_descr_parts>3)
          Error("fParseLine","Import wrong syntax: cannot process %s", o_descr.Data());

        TString obj_name (static_cast<TObjString*>(descr_array->At(n_descr_parts-1))->GetString());
        TString ws_name("");
        TString rootfile_name (static_cast<TObjString*>(descr_array->At(0))->GetString());

        TFile* ifile=TFile::Open(rootfile_name);
        if (ifile==0)
            return 1;

        if (n_descr_parts==3){// in presence of a Ws
          o_descr.ReplaceAll(",",":");
          fWs->import(o_descr);
          }
        else if(n_descr_parts==2){ // in presence of an object in rootfile
          if (fVerbose)
            Info("fParseLine","Importing %s from %s under the name of %s",
                 obj_name.Data(), rootfile_name.Data(), o_name.Data());
          TObject* the_obj=ifile->Get(obj_name);
          fWs->import(*the_obj,o_name);
          }
        delete ifile;
        return 0;
        } // end of import block

        new_line=o_class+"::"+o_name+"("+o_descr+")";

        if (fVerbose){
            std::cout << "DEBUG: line: " << line.Data() << std::endl;
            std::cout << "DEBUG: new_line: " << new_line.Data() << std::endl;
            }

        fWs->factory(new_line);

        return 0;
        }

    else { // In case we do not know what to do we pipe it..
        fWs->factory(line);
        }

    return 0;

}




