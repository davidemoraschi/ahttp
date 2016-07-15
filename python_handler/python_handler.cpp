// python_handler.cpp : Defines the entry point for the DLL application.
//

#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>

#include "ahttplib.hpp"
#include "aconnect/util.hpp"

#include "wrappers.hpp"

namespace algo = boost::algorithm;
namespace fs = boost::filesystem;

#include "module.inl"

// constants

const aconnect::string UploadsDirParam = "uploads-dir";

// globals
boost::mutex pythonExecMutex;
aconnect::string uploadsDirPath;
ahttp::HttpServerSettings *globalServerSettings;
python::object mainModule;

#if defined (WIN32)

    HMODULE moduleHandle = NULL;

    BOOL APIENTRY DllMain( HMODULE hModule,
                           DWORD  ul_reason_for_call,
                           LPVOID lpReserved
    					 )
    {
    	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    		moduleHandle = hModule;
    	return TRUE;
    }
#endif

void executeScript (aconnect::string_constref, HttpContextWrapper * );


/* 
*	Handler initialization function - return true if initialization performed suñcessfully
*/
HANDLER_EXPORT bool initHandler  (const aconnect::str2str_map& params, ahttp::HttpServerSettings *globalSettings)
{
	assert (globalSettings);
	assert (globalSettings->logger());

	boost::mutex::scoped_lock lock(pythonExecMutex);

	globalServerSettings = globalSettings;

	Py_InitializeEx (1);

	if ( 0 == Py_IsInitialized()) {
		globalSettings->logger()->error ("Python interpreter was not initialized correctly");
		return false;
	}

	aconnect::str2str_map::const_iterator it = params.find(UploadsDirParam);
	
	if (it != params.end()) {
		uploadsDirPath = it->second;
		if (!fs::exists (uploadsDirPath))
			fs::create_directories(uploadsDirPath);
	} else {
		globalSettings->logger()->error ("Mandatory parameter '%s' is absent", UploadsDirParam.c_str() );
	}

	try {
		
		// Register the module with the interpreter
		if (PyImport_AppendInittab("python_handler", initpython_handler) == -1)
		{
			globalSettings->logger()->error ("Failed to register 'python_handler' in the interpreter's built-in modules");
			return false;
		}

		// Retrieve the main module
		mainModule = python::import("__main__");
		
		// register classes
		python::import ("python_handler");

	} catch (...) {
		globalSettings->logger()->error("Python interpreter initialization failed: unknown exception caught");
		return false;
	}

	return true;
}


class GilThreadStateGuard
{
public:
	GilThreadStateGuard (PyGILState_STATE gstate) :
	  gstate_ (gstate) { }

	  ~GilThreadStateGuard () {
		  PyGILState_Release (gstate_);
		  
	  }
protected:
	PyGILState_STATE gstate_;
};

class ThreadStateGuard
{
public:
	ThreadStateGuard (PyThreadState *state, void(*onDestroy)(PyThreadState *) = PyEval_RestoreThread ) :
	  state_ (state), onDestroy_(onDestroy) { assert(state); }
	  ~ThreadStateGuard () {
		  onDestroy_ (state_);

	  }
protected:
	PyThreadState *state_;
	void(*onDestroy_)(PyThreadState *);
};
    

    
aconnect::string loadPythonError()
{
	using namespace python;

	boost::mutex::scoped_lock lock(pythonExecMutex);

	aconnect::string exDesc;
	PyObject* type = NULL, 
		*value = NULL, 
		*traceback = NULL;

	try {
			
		if ( !PyErr_Occurred ())
			return "Undefined Python exception caught";
	
		PyErr_Fetch (&type, &value, &traceback);
		PyErr_Clear();

		if (!type || !value)
			return exDesc;

		aconnect::string_constptr format  = "Python exception caught, type: %s\n"
			"Exception value: %s\n";
		
		str info (format % make_tuple ( handle<> (type), handle<> (value) ) );
		exDesc = extract<aconnect::string> ( info );

		if (NULL != traceback) {
			
			reference_existing_object::apply<TracebackLoaderWrapper*>::type converter;
			TracebackLoaderWrapper loader;
			
			handle<> loaderHandle ( converter( &loader ) );
			object tracebackLoader = object( loaderHandle );
			
			if (traceback && traceback != Py_None)
				PyTraceBack_Print(traceback, loaderHandle.get() );
			
			exDesc += extract<aconnect::string> ( str (tracebackLoader.attr ("content") ) );
			
	} 
		
	} catch (...) {
		exDesc = "Python exception description loading failed";
	}

	return exDesc;
}

/* 
*	Main request processing function - return false if request should
*	be processed by other handlers or by server (true: request completed)
*/
HANDLER_EXPORT bool processHandlerRequest (ahttp::HttpContext& context)
{
	using namespace ahttp;
	using namespace aconnect;
	
	if ( !util::fileExists (context.FileSystemPath.string()) ) {
		HttpServer::processError404 (context);
		return true;
	}

	if (!context.isClientConnected())
		return true;

	// init context
	context.UploadsDirPath = uploadsDirPath;
	HttpContextWrapper wrapper (&context);

	// boost::mutex::scoped_lock lock(pythonExecMutex);

    // UNDONE: Investigate mutli-threaded Python!
	/*
	PyEval_InitThreads( );
	if ( !PyEval_ThreadsInitialized() )
        throw aconnect::request_processing_error ("Threading support is not initialized");
	*/
    
	/*
	// PyGILState_Ensure does not work correctly
        PyGILState_STATE gstate = PyGILState_Ensure();
        GilThreadStateGuard guard (gstate);
    */

	// PyThreadState *threadState = PyEval_SaveThread();
	// ThreadStateGuard guard (threadState);

	// PyThreadState* threadState = Py_NewInterpreter( );
	// ThreadStateGuard guard (threadState, Py_EndInterpreter);
	
	try 
	{
		executeScript (context.FileSystemPath.string(), &wrapper);
		context.setHtmlResponse();
			
	} catch (std::exception const &ex)  {
		context.Log->error ("Exception caught (%s): %s", 
			typeid(ex).name(), ex.what());
		
		HttpServer::processServerError(context, 500, ex.what());
	
	} catch (python::error_already_set const &)  {
		string errDesc = loadPythonError ();

		if (errDesc.find ('%') == string::npos)
			context.Log->error (errDesc.c_str());
		else
			context.Log->error ( algo::replace_all_copy(errDesc, "%", "%%").c_str() );

		// prepare HTML response
		errDesc = util::escapeHtml (errDesc);
		algo::replace_all (errDesc, "\n", "<br />");
		algo::replace_all (errDesc, "  ", "&nbsp;&nbsp;");
		HttpServer::processServerError(context, 500, errDesc.c_str() );
		
	} catch (...)  {
		context.Log->error ("Unknown exception caught");
		HttpServer::processServerError(context, 500);
	}
	

	return true;
}


void executeScript (aconnect::string_constref scriptPath, HttpContextWrapper *wrapper )
{
	using namespace python;
	
	// Retrieve the main module's namespace
	dict global (mainModule.attr("__dict__"));
	dict local (global.copy ());
	
	// prepare globals
	reference_existing_object::apply<HttpContextWrapper*>::type converter;
	handle<> wrapperHandle ( converter( wrapper ) );

	local["http_context"] = wrapperHandle;
	PySys_SetObject("stdout", wrapperHandle.get());	// 'write' method will be used
	PySys_SetObject("stderr", wrapperHandle.get());	// 'write' method will be used

	PyObject *pyfile = NULL;
	// copied from exec_file function
	// exec_file (scriptPath.c_str(), global, local);

	{
		boost::mutex::scoped_lock lock(pythonExecMutex);
		pyfile = PyFile_FromString (const_cast<char*>(scriptPath.c_str()), "r");
	}

	python::handle<> file (pyfile);

	PyObject* result = PyRun_File (PyFile_AsFile(file.get()),
		scriptPath.c_str(),
		Py_file_input,
		global.ptr(), 
		local.ptr());
	
	if (!result) 
		throw_error_already_set();
}

