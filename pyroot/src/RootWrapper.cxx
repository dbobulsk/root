// @(#)root/pyroot:$Name:  $:$Id: RootWrapper.cxx,v 1.16 2004/11/05 09:05:45 brun Exp $
// Author: Wim Lavrijsen, Apr 2004

// Bindings
#include "PyROOT.h"
#include "RootWrapper.h"
#include "Pythonize.h"
#include "ObjectHolder.h"
#include "MethodDispatcher.h"
#include "ConstructorDispatcher.h"
#include "MethodHolder.h"
#include "ClassMethodHolder.h"
#include "PropertyHolder.h"
#include "MemoryRegulator.h"
#include "TPyClassGenerator.h"
#include "Utility.h"

// ROOT
#include "TROOT.h"
#include "TSystem.h"
#include "TBenchmark.h"
#include "TRandom.h"
#include "TApplication.h"
#include "TStyle.h"
#include "TGeometry.h"
#include "TMethod.h"
#include "TMethodArg.h"
#include "TBaseClass.h"
#include "TInterpreter.h"
#include "TGlobal.h"

// CINT
#include "Api.h"

// Standard
#include <assert.h>
#include <map>
#include <string>
#include <algorithm>
#include <vector>


//- data _______________________________________________________________________
extern PyObject* g_modroot;

namespace {

   std::map< std::string, std::string > c2p_operators_;

   class InitOperators_ {
   public:
      InitOperators_() {
         c2p_operators_[ "[]" ]  = "__getitem__";
         c2p_operators_[ "()" ]  = "__call__";
         c2p_operators_[ "+" ]   = "__add__";
         c2p_operators_[ "-" ]   = "__sub__";
         c2p_operators_[ "*" ]   = "__mul__";
         c2p_operators_[ "/" ]   = "__div__";
         c2p_operators_[ "%" ]   = "__mod__";
         c2p_operators_[ "**" ]  = "__pow__";
         c2p_operators_[ "<<" ]  = "__lshift__";
         c2p_operators_[ ">>" ]  = "__rshift__";
         c2p_operators_[ "&" ]   = "__and__";
         c2p_operators_[ "|" ]   = "__or__";
         c2p_operators_[ "^" ]   = "__xor__";
         c2p_operators_[ "+=" ]  = "__iadd__";
         c2p_operators_[ "-=" ]  = "__isub__";
         c2p_operators_[ "*=" ]  = "__imul__";
         c2p_operators_[ "/=" ]  = "__idiv__";
         c2p_operators_[ "/=" ]  = "__imod__";
         c2p_operators_[ "**=" ] = "__ipow__";
         c2p_operators_[ "<<=" ] = "__ilshift__";
         c2p_operators_[ ">>=" ] = "__irshift__";
         c2p_operators_[ "&=" ]  = "__iand__";
         c2p_operators_[ "|=" ]  = "__ior__";
         c2p_operators_[ "^=" ]  = "__ixor__";
         c2p_operators_[ "==" ]  = "__eq__";
         c2p_operators_[ "!=" ]  = "__ne__";
         c2p_operators_[ ">" ]   = "__gt__";
         c2p_operators_[ "<" ]   = "__lt__";
         c2p_operators_[ ">=" ]  = "__ge__";
         c2p_operators_[ "<=" ]  = "__le__";
      }
   } initOperators_;

} // unnamed namespace


//- helpers --------------------------------------------------------------------
namespace {

   class PyROOTApplication : public TApplication {
   public:
      PyROOTApplication( const char* acn, int* argc, char** argv ) :
            TApplication( acn, argc, argv ) {
         SetReturnFromRun( kTRUE );          // prevents ROOT from exiting python
      }
   };

   inline void addToScope( const char* label, TObject* obj, TClass* cls ) {
      PyModule_AddObject(
         PyImport_AddModule( const_cast< char* >( "libPyROOT" ) ),
         const_cast< char* >( label ),
         PyROOT::bindRootObject( new PyROOT::ObjectHolder( obj, cls, false ) )
      );
   }

} // unnamed namespace


//------------------------------------------------------------------------------
void PyROOT::initRoot() {
// setup interpreter locks to allow for threading in ROOT
   PyEval_InitThreads();

// setup root globals (bind later)
   if ( !gBenchmark ) gBenchmark = new TBenchmark();
   if ( !gStyle ) gStyle = new TStyle();
   if ( !gApplication ) {
      char* argv[1];
      int argc = 0;
      gApplication = new PyROOTApplication( "PyROOT", &argc, argv );
   }

// bind ROOT globals (ObjectHolder instances will be properly destroyed)
   addToScope( "gROOT", gROOT, gROOT->IsA() );
   addToScope( "gSystem", gSystem, gSystem->IsA() );
   addToScope( "gInterpreter", gInterpreter, gInterpreter->IsA() );

// memory management
   gROOT->GetListOfCleanups()->Add( new MemoryRegulator() );

// python side class construction, managed by ROOT
   gROOT->AddClassGenerator( new TPyClassGenerator() );
}


int PyROOT::buildRootClassDict( TClass* cls, PyObject* pyclass ) {
   assert( cls != 0 );

   std::string className = cls->GetName();
   bool isNamespace = cls->Property() & G__BIT_ISNAMESPACE;
   bool hasConstructor = false;

// load all public methods and data members
   typedef std::map< std::string, MethodDispatcher > DispatcherCache_t;
   DispatcherCache_t dispCache;

   TIter nextmethod( cls->GetListOfMethods() );
   while ( TMethod* mt = (TMethod*)nextmethod() ) {
   // allow only public methods
      if ( !( mt->Property() & kIsPublic ) )
         continue;

   // retrieve method name
      std::string mtName = mt->GetName();

   // filter C++ destructors
      if ( mtName[0] == '~' )
         continue;

   // operators
      if ( 8 < mtName.size() && mtName.substr( 0, 8 ) == "operator" ) {
         std::string op = mtName.substr( 8, std::string::npos );

         if ( op == "=" || op == " new" || op == " new[]" ||
              op == " delete" || op == " delete[]" )
            continue;

      // map C++ operator to python equivalent
         std::map< std::string, std::string >::iterator pop = c2p_operators_.find( op );
         if ( pop != c2p_operators_.end() )
            mtName = pop->second;
      }

   // use full signature as a doc string
      std::string doc( mt->GetReturnTypeName() );
      (((((doc += ' ') += className) += "::") += mtName) += mt->GetSignature());

   // decide on method type
      bool isStatic = isNamespace || ( mt->Property() & G__BIT_ISSTATIC );

   // construct holder
      MethodHolder* pmh = 0;
      if ( isStatic == true )                     // class method
         pmh = new ClassMethodHolder( cls, mt );
      else if ( mtName == className ) {           // constructor
         pmh = new ConstructorDispatcher( cls, mt );
         mtName = "__init__";
         hasConstructor = true;
      }
      else                                        // member function
         pmh = new MethodHolder( cls, mt );

   // lookup method dispatcher and store method
      MethodDispatcher& md = (*(dispCache.insert(
         std::make_pair( mtName, MethodDispatcher( mtName, isStatic ) ) ).first)).second;
      md.addMethod( pmh );
   }

// add the methods to the class dictionary
   for ( DispatcherCache_t::iterator imd = dispCache.begin(); imd != dispCache.end(); ++imd ) {
      MethodDispatcher::addToClass( new MethodDispatcher( imd->second ), pyclass );
   }

// special case if there's no constructor defined
   if ( ! hasConstructor ) {
      MethodDispatcher* pmd = new MethodDispatcher( "__init__", false );
      pmd->addMethod( new ConstructorDispatcher( cls, 0 ) );
      MethodDispatcher::addToClass( pmd, pyclass );
   }

// collect data members
   TIter nextmember( cls->GetListOfDataMembers() );
   while ( TDataMember* mb = (TDataMember*)nextmember() ) {
   // allow only public members
      if ( !( mb->Property() & kIsPublic ) )
         continue;

   // enums
      if ( mb->IsEnum() ) {
         long offset = 0;
         G__DataMemberInfo dmi = cls->GetClassInfo()->GetDataMember( mb->GetName(), &offset );
         PyObject* val = PyInt_FromLong( *((int*)((G__var_array*)dmi.Handle())->p[dmi.Index()]) );
         PyObject_SetAttrString( pyclass, const_cast< char* >( mb->GetName() ), val );
      }

   // property object
      else
         PropertyHolder::addToClass( new PropertyHolder( mb ), pyclass );
   }

// all ok, done
   return 0;
}


PyObject* PyROOT::buildRootClassBases( TClass* cls ) {
   TList* allbases = cls->GetListOfBases();

   std::vector< std::string >::size_type nbases = 0;
   if ( allbases != 0 )
      nbases = allbases->GetSize();

// collect bases, remove duplicates
   std::vector< std::string > uqb;
   uqb.reserve( nbases );

   TIter nextbase( allbases );
   while ( TBaseClass* base = (TBaseClass*)nextbase() ) {
      std::string name = base->GetName();
      if ( std::find( uqb.begin(), uqb.end(), name ) == uqb.end() ) {
         uqb.push_back( name );
      }
   }

// allocate a tuple for the base classes, special case for no bases
   nbases = uqb.size();

   PyObject* pybases = PyTuple_New( nbases ? nbases : 1 );
   if ( ! pybases )
      return 0;

// build all the bases
   if ( nbases == 0 ) {
      Py_INCREF( &PyBaseObject_Type );
      PyTuple_SET_ITEM( pybases, 0, (PyObject*)&PyBaseObject_Type );
   }
   else {
      for ( std::vector< std::string >::size_type ibase = 0; ibase < nbases; ++ibase ) {
         PyObject* pyclass = makeRootClassFromString( uqb[ ibase ] );
         if ( ! pyclass ) {
            Py_DECREF( pybases );
            return 0;
         }

         PyTuple_SET_ITEM( pybases, ibase, pyclass );
      }
   }

   return pybases;
}


PyObject* PyROOT::makeRootClass( PyObject*, PyObject* args ) {
   std::string cname = PyString_AsString( PyTuple_GetItem( args, 0 ) );

   if ( PyErr_Occurred() )
      return 0;

   return makeRootClassFromString( cname );
}


PyObject* PyROOT::makeRootClassFromString( const std::string& cname ) {
// retrieve ROOT class (this verifies className)
   TClass* cls = gROOT->GetClass( cname.c_str() );
   if ( cls == 0 ) {
      PyErr_SetString( PyExc_TypeError,
         ( "requested class " + cname + " does not exist" ).c_str() );
      return 0;
   }

   PyObject* pycname = PyString_FromString( const_cast< char* >( cname.c_str() ) );

// first try to retrieve the class representation from the ROOT module
   PyObject* pyclass = PyObject_GetAttr( g_modroot, pycname );

// build if the class does not yet exist
   if ( pyclass == 0 ) {
   // ignore error generated from the failed lookup
      PyErr_Clear();

   // start with an empty dictionary
      PyObject* dct = PyDict_New();

   // construct the base classes
      PyObject* pybases = buildRootClassBases( cls );
      if ( pybases != 0 ) {
      // create a fresh Python class, given bases, name and empty dictionary
         pyclass = PyObject_CallFunction(
            (PyObject*)&PyType_Type, const_cast< char* >( "OOO" ), pycname, pybases, dct );
         Py_DECREF( pybases );
      }

      Py_DECREF( dct );

   // fill the dictionary, if successful
      if ( pyclass != 0 ) {
         if ( buildRootClassDict( cls, pyclass ) != 0 ) {
         // something failed in building the dictionary
            Py_DECREF( pyclass );
            pyclass = 0;
         }
         else {
            Py_INCREF( pyclass );            // PyModule_AddObject steals reference
            PyModule_AddObject( g_modroot, const_cast< char* >( cname.c_str() ), pyclass );
         }
      }
   }

   Py_DECREF( pycname );

// add python-like features
   if ( ! pythonize( pyclass, cname ) ) {
      Py_XDECREF( pyclass );
      pyclass = 0;
   }

// all done
   return pyclass;
}


PyObject* PyROOT::getRootGlobal( PyObject*, PyObject* args ) {
// get the requested name
   std::string ename = PyString_AsString( PyTuple_GetItem( args, 0 ) );

   if ( PyErr_Occurred() )
      return 0;

   return getRootGlobalFromString( ename );
}


PyObject* PyROOT::getRootGlobalFromString( const std::string& gname ) {
// loop over globals to find this enum
   TIter nextGlobal( gROOT->GetListOfGlobals( kTRUE ) );
   while ( TGlobal* gb = (TGlobal*)nextGlobal() ) {
      if ( gb->GetName() == gname && gb->GetAddress() ) {

         if ( G__TypeInfo( gb->GetTypeName() ).Property() & G__BIT_ISENUM )
         // enum, deref and return as long
            return PyInt_FromLong( *((int*)gb->GetAddress()) );

         else
         // TGlobal, attempt to get the actual class and cast as appropriate
            return bindRootGlobal( gb );
      }
   }

// nothing found
   Py_INCREF( Py_None );
   return Py_None;
}


PyObject* PyROOT::bindRootObject( ObjectHolder* obh, bool force ) {
// for safety
   if ( obh == 0 ) {
      Py_INCREF( Py_None );
      return Py_None;
   }

   TClass* cls = obh->objectIsA();

// only known and knowable objects will be bound
   if ( cls != 0 ) {
      PyObject* pyclass = makeRootClassFromString( cls->GetName() );

      if ( pyclass ) {

         TObject* tobj = (TObject*) cls->DynamicCast( TObject::Class(), obh->getObject() );

         if ( force == false ) {
         // use the old reference if the object already exists
            PyObject* oldObject = MemoryRegulator::RetrieveObject( tobj );
            if ( oldObject )
               return oldObject;
         }

      // if not: instantiate an object of this class, with the holder added to it
         PyObject* newObject = PyType_GenericNew( (PyTypeObject*)pyclass, NULL, NULL );
         Py_DECREF( pyclass );

      // bind, register and return if successful
         if ( newObject != 0 ) {
         // private to the object is the instance holder
            PyObject* cobj = PyCObject_FromVoidPtr( obh, &destroyObjectHolder );
            PyObject_SetAttr( newObject, Utility::theObjectString_, cobj );
            Py_DECREF( cobj );

         // memory management
            MemoryRegulator::RegisterObject( newObject, tobj );

         // successful completion
            return newObject;
         }
      }
   }

// confused ...
   PyErr_SetString( PyExc_TypeError, "failed to bind ROOT object" );
   return 0;
}


PyObject* PyROOT::bindRootGlobal( TGlobal* gb ) {
   TClass* cls = gROOT->GetClass( gb->GetTypeName() );
   if ( ! cls )
      cls = TGlobal::Class();

   if ( Utility::isPointer( gb->GetFullTypeName() ) )
      return bindRootObject(
         new AddressHolder( (void**)gb->GetAddress(), cls, false ), true );

   return bindRootObject( new ObjectHolder( (void*)gb->GetAddress(), cls, false ) );
}

