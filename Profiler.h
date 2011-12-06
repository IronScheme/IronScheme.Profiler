/*****************************************************************************
 * DotNetProfiler
 * 
 * Copyright (c) 2006 Scott Hackett
 * 
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the author be held liable for any damages arising from the
 * use of this software. Permission to use, copy, modify, distribute and sell
 * this software for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.
 * 
 * Scott Hackett (code@scotthackett.com)
 *****************************************************************************/

#pragma once
#include "resource.h"
#include "DotNetProfiler.h"
#include "CorProfilerCallbackImpl.h"
#include <map>

#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif

#define ASSERT_HR(x) _ASSERT(SUCCEEDED(x))
#define NAME_BUFFER_SIZE 1024

typedef struct FunctionInfo
{
  WCHAR* name;
  __int32 callcount;
  __int64 inctime;
  __int64 extime;
  ThreadID tid;

} FUNCTIONINFO;

typedef struct FunctionSample
{
  FUNCTIONINFO* info;
  __int64 extime;
  __int64 inctime;
  __int64 lastextime;
  __int64 lastinctime;
  ThreadID tid;

} FUNCTIONSAMPLE;

typedef struct Stack
{
  int top;
  int size;
  FUNCTIONSAMPLE* buffer;
} STACK;


// CProfiler
class ATL_NO_VTABLE CProfiler :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CProfiler, &CLSID_Profiler>,
	public CCorProfilerCallbackImpl
{
public:
	CProfiler();

	DECLARE_REGISTRY_RESOURCEID(IDR_PROFILER)
	BEGIN_COM_MAP(CProfiler)
		COM_INTERFACE_ENTRY(ICorProfilerCallback)
		COM_INTERFACE_ENTRY(ICorProfilerCallback2)
		COM_INTERFACE_ENTRY(ICorProfilerCallback3)
	END_COM_MAP()
	DECLARE_PROTECT_FINAL_CONSTRUCT()

	// overridden implementations of FinalConstruct and FinalRelease
	HRESULT FinalConstruct();
	void FinalRelease();

  // STARTUP/SHUTDOWN EVENTS
  STDMETHOD(Initialize)(IUnknown *pICorProfilerInfoUnk);
  STDMETHOD(Shutdown)();

	// callback functions
  void Enter(FunctionID functionID, FUNCTIONINFO* fi);
	void Leave(FunctionID functionID, FUNCTIONINFO* fi);
	void Tailcall(FunctionID functionID, FUNCTIONINFO* fi);

	// mapping functions
	static UINT_PTR _stdcall FunctionMapper(FunctionID functionId, BOOL *pbHookFunction);
	UINT_PTR MapFunction(FunctionID);

  //
  // process metadata for a function given its functionID
  //
  HRESULT GetFunctionProperties( FunctionID functionID,
                                 BOOL *isStatic,
                                 BOOL *isprop,
                                 ULONG *argCount,
                                 __out_ecount(returnTypeStrLen) WCHAR *returnTypeStr, 
                                 size_t returnTypeStrLen,
                                 __out_ecount(functionParametersLen) WCHAR *functionParameters,
                                 size_t functionParametersLen,
                                 __out_ecount(functionNameLen) WCHAR *functionName,
                                 size_t functionNameLen );
  
  //
  // print element type
  //
  PCCOR_SIGNATURE ParseElementType( IMetaDataImport *pMDImport, 
                                    PCCOR_SIGNATURE signature, 
                                    ClassID *classTypeArgs,
                                    ClassID *methodTypeArgs,
                                    __out_ecount(cchBuffer) char *buffer,
                                    size_t cchBuffer );
                                           


  HRESULT GetClassName(IMetaDataImport *pMDImport, mdToken classToken, WCHAR className[], ClassID *classTypeArgs, ULONG *totalGenericArgCount);

  void AppendTypeArgName(ULONG argIndex, ClassID *actualClassTypeArgs, ClassID *actualMethodTypeArgs, BOOL methodFormalArg, __out_ecount(cchBuffer) char *buffer, size_t cchBuffer);
  HRESULT GetNameFromClassID( ClassID classID, WCHAR className[] );

private:
    // container for ICorProfilerInfo reference
	CComQIPtr<ICorProfilerInfo> m_pICorProfilerInfo;
    // container for ICorProfilerInfo2 reference
	CComQIPtr<ICorProfilerInfo2> m_pICorProfilerInfo2;
	CComQIPtr<ICorProfilerInfo3> m_pICorProfilerInfo3;
	// STL map for our hashed functions
  std::map<FunctionID, FUNCTIONINFO*> m_functionMap;
	// the number of levels deep we are in the call stack
  LARGE_INTEGER init_time;
	// handle and filename of log file

  std::map<ThreadID, STACK*> m_stackMap;

	// gets the full method name given a function ID
	HRESULT GetFullMethodName(FunctionID functionId, LPWSTR wszMethod, int cMethod );
	// function to set up our event mask
	HRESULT SetEventMask();
};

OBJECT_ENTRY_AUTO(__uuidof(Profiler), CProfiler)
