#include "PyMostPopular.h"
#include "pyinterface.h"
#include "DataReader.h"
#include "DataWriter.h"

#include <Python.h>
#include <iostream>
#include <string>
#include <sstream>

using namespace std;


PyObject* MostPopular_new( PyTypeObject* type, PyObject* args, PyObject* kwdict )
{
   const char* dsfilename = NULL;
   char dlmchar = ',';
   int header = 0;
   int usercol = 0;
   int itemcol = 1;
   int ratingcol = 2;

   static char* kwlist[] = { const_cast<char*>( "dataset" ),
                             const_cast<char*>( "dlmchar" ),
                             const_cast<char*>( "header" ),
                             const_cast<char*>( "usercol" ),
                             const_cast<char*>( "itemcol" ),
                             const_cast<char*>( "ratingcol" ),
                             NULL };

   if( !PyArg_ParseTupleAndKeywords( args, kwdict, "s|ciiii", kwlist, &dsfilename,
                                     &dlmchar, &header, &usercol, &itemcol, &ratingcol ) )
   {
      return NULL;
   }

   if( NULL == dsfilename )
   {
      return NULL;
   }

   PyMostPopular* self = reinterpret_cast<PyMostPopular*>( type->tp_alloc( type, 0 ) );
   if( self != NULL )
   {
      self->m_trainingReader = new DataReader( dsfilename, dlmchar, header );
      if( NULL == self->m_trainingReader )
      {
         Py_DECREF( self );
         return NULL;
      }

      self->m_recAlgorithm = new AlgMostPopular( *self->m_trainingReader, usercol, itemcol, ratingcol );
      if( NULL == self->m_recAlgorithm )
      {
         Py_DECREF( self );
         return NULL;
      }
   }

   return reinterpret_cast<PyObject*>( self );
}

void MostPopular_dealloc( PyMostPopular* self )
{
   Py_XDECREF( self->m_trainingReader );
   Py_XDECREF( self->m_recAlgorithm );
#if PY_MAJOR_VERSION >= 3
   Py_TYPE( self )->tp_free( reinterpret_cast<PyObject*>( self ) );
#else
   self->ob_type->tp_free( reinterpret_cast<PyObject*>( self ) );
#endif
}

PyObject* MostPopular_train( PyMostPopular* self, PyObject* args, PyObject* kwdict )
{
   int topn = 10;

   static char* kwlist[] = { const_cast<char*>( "topn" ),
                             NULL };

   if( !PyArg_ParseTupleAndKeywords( args, kwdict, "|i", kwlist, &topn ) )
   {
      return NULL;
   }

   PrlSigHandler::registerObj( reinterpret_cast<PyObject*>( self ), PrlSigHandler::MOST_POPULAR );
   struct sigaction* pOldAction = PrlSigHandler::handlesignal( SIGINT );
   int cause = 0;
   Py_BEGIN_ALLOW_THREADS
   cause = dynamic_cast<AlgMostPopular*>( self->m_recAlgorithm )->train();
   Py_END_ALLOW_THREADS
   PrlSigHandler::restoresignal( SIGINT, pOldAction );

   if( AlgMostPopular::STOPPED == cause )
   {
      PyGILState_STATE gstate = PyGILState_Ensure();
      PyErr_SetString( PyExc_KeyboardInterrupt, "SIGINT received" );
      PyGILState_Release( gstate );
      return NULL;
   }

   Py_INCREF( Py_None );
   return Py_None;
}

PyObject* MostPopular_testrec( PyMostPopular* self, PyObject* args, PyObject* kwdict )
{
   const char* input_file = NULL;
   const char* output_file = NULL;
   char dlmchar = ',';
   int header = 0;
   int usercol = 0;
   int itemcol = 1;
   int topn = 10;

   static char* kwlist[] = { const_cast<char*>( "input_file" ),
                             const_cast<char*>( "output_file" ),
                             const_cast<char*>( "dlmchar" ),
                             const_cast<char*>( "header" ),
                             const_cast<char*>( "usercol" ),
                             const_cast<char*>( "topn" ),
                             NULL };

   if( !PyArg_ParseTupleAndKeywords( args, kwdict, "s|sciii", kwlist, &input_file,
                                     &output_file, &dlmchar, &header, &usercol, &topn ) )
   {
      return NULL;
   }

   if( NULL == input_file )
   {
      return NULL;
   }

   DataWriter dataWriter;
   if( NULL != output_file )
   {
      char dlm = '\t';
      string strfilename = output_file;
      if( strfilename.substr( strfilename.find_last_of( "." ) + 1 ) == "csv"  ||
          strfilename.substr( strfilename.find_last_of( "." ) + 1 ) == "json"    )
      {
         dlm = ',';
      }
      dataWriter.open( strfilename, dlm );
   }

   DataReader testReader( input_file, dlmchar, header );
   DataFrame testData( testReader, usercol, itemcol );

   PyObject* pyDict = PyDict_New();
   if( NULL == pyDict )
   {
      return NULL;
   }

   DataFrame::iterator ind;
   DataFrame::iterator end = testData.end();
   for( ind = testData.begin() ; ind != end ; ++ind )
   {
      std::string userId = ind->first.first;
      std::string itemId = ind->first.second;

      vector<string> itemList;
      if( !self->m_recAlgorithm->recommend( userId, topn, itemList ) )
      {
         continue;
      }

      PyObject* pyList = PyList_New( 0 );
      if( NULL == pyList )
      {
         return NULL;
      }

      vector<string>::iterator ind;
      vector<string>::iterator end = itemList.end();
      for( ind = itemList.begin() ; ind != end ; ++ind )
      {
#if PY_MAJOR_VERSION >= 3
         if( -1 == PyList_Append( pyList, PyBytes_FromString( ind->c_str() ) ) )
#else
         if( -1 == PyList_Append( pyList, PyString_FromString( ind->c_str() ) ) )
#endif
         {
            return NULL;
         }
      }

      PyDict_SetItemString( pyDict, userId.c_str(), pyList );

      if( dataWriter.isOpen() )
      {
         dataWriter.write( userId, itemList );
      }
   }

   return pyDict;
}

PyObject* MostPopular_recommend( PyMostPopular* self, PyObject* args, PyObject* kwdict )
{
   const char* userId = NULL;
   int topn = 10;

   static char* kwlist[] = { const_cast<char*>( "user" ),
                             const_cast<char*>( "topn" ),
                             NULL };

   if( !PyArg_ParseTupleAndKeywords( args, kwdict, "s|i", kwlist, &userId, &topn ) )
   {
      return NULL;
   }

   vector<string> itemList;
   self->m_recAlgorithm->recommend( userId, topn, itemList );

   PyObject* pyList = PyList_New( 0 );
   if( NULL == pyList )
   {
      return NULL;
   }

   vector<string>::iterator ind;
   vector<string>::iterator end = itemList.end();
   for( ind = itemList.begin() ; ind != end ; ++ind )
   {
      if( -1 == PyList_Append( pyList, Py_BuildValue( "s", ind->c_str() ) ) )
      {
         return NULL;
      }
   }

   return pyList;
}


