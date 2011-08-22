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
#include "stdafx.h"
#include <assert.h>
#include "winnt.h"
#include "Profiler.h"
#include "basehdr.h"




#pragma warning (disable: 4996) 

#define ARRAY_SIZE(s) (sizeof(s) / sizeof(s[0]))

// global reference to the profiler object (this) used by the static functions
CProfiler* g_pICorProfilerCallback = NULL;

// CProfiler
CProfiler::CProfiler() 
{

}

HRESULT CProfiler::FinalConstruct()
{
	return S_OK;
}

void CProfiler::FinalRelease()
{

}

// ----  CALLBACK FUNCTIONS ------------------

// this function simply forwards the FunctionEnter call the global profiler object
EXTERN_C void __stdcall EnterStub(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func, COR_PRF_FUNCTION_ARGUMENT_INFO *argumentInfo)
{
	// make sure the global reference to our profiler is valid
    if (g_pICorProfilerCallback != NULL)
      g_pICorProfilerCallback->Enter(functionID, (FUNCTIONINFO*) clientData);
}

#ifdef _X86_
// this function is called by the CLR when a function has been entered
void _declspec(naked) FunctionEnterNaked(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func, COR_PRF_FUNCTION_ARGUMENT_INFO *argumentInfo)
{
    __asm
    {
        push    ebp                 // Create a standard frame
        mov     ebp,esp
        pushad                      // Preserve all registers

        mov     eax,[ebp+0x14]      // argumentInfo
        push    eax
        mov     ecx,[ebp+0x10]      // func
        push    ecx
        mov     edx,[ebp+0x0C]      // clientData
        push    edx
        mov     eax,[ebp+0x08]      // functionID
        push    eax
        call    EnterStub

        popad                       // Restore all registers
        pop     ebp                 // Restore EBP
        ret     16
    }
}
#else

EXTERN_C void FunctionEnterNaked(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func, COR_PRF_FUNCTION_ARGUMENT_INFO *argumentInfo);

#endif

// this function simply forwards the FunctionLeave call the global profiler object
EXTERN_C void __stdcall LeaveStub(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func, COR_PRF_FUNCTION_ARGUMENT_RANGE *retvalRange)
{
	// make sure the global reference to our profiler is valid
    if (g_pICorProfilerCallback != NULL)
        g_pICorProfilerCallback->Leave(functionID, (FUNCTIONINFO*) clientData);
}

#ifdef _X86_
// this function is called by the CLR when a function is exiting
void _declspec(naked) FunctionLeaveNaked(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func, COR_PRF_FUNCTION_ARGUMENT_RANGE *retvalRange)
{
    __asm
    {
        push    ebp                 // Create a standard frame
        mov     ebp,esp
        pushad                      // Preserve all registers

        mov     eax,[ebp+0x14]      // argumentInfo
        push    eax
        mov     ecx,[ebp+0x10]      // func
        push    ecx
        mov     edx,[ebp+0x0C]      // clientData
        push    edx
        mov     eax,[ebp+0x08]      // functionID
        push    eax
        call    LeaveStub

        popad                       // Restore all registers
        pop     ebp                 // Restore EBP
        ret     16
    }
}
#else

EXTERN_C void FunctionLeaveNaked(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func, COR_PRF_FUNCTION_ARGUMENT_RANGE *retvalRange);

#endif

// this function simply forwards the FunctionLeave call the global profiler object
EXTERN_C void __stdcall TailcallStub( FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func)
{
    if (g_pICorProfilerCallback != NULL)
        g_pICorProfilerCallback->Tailcall(functionID, (FUNCTIONINFO*) clientData);
}

#ifdef _X86_
// this function is called by the CLR when a tailcall occurs.  A tailcall occurs when the 
// last action of a method is a call to another method.
void _declspec(naked) FunctionTailcallNaked(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func)
{
    __asm
    {
        push    ebp                 // Create a standard frame
        mov     ebp,esp
        pushad                      // Preserve all registers

        mov     ecx,[ebp+0x10]      // func
        push    ecx
        mov     edx,[ebp+0x0C]      // clientData
        push    edx
        mov     eax,[ebp+0x08]      // functionID
        push    eax
        call    TailcallStub

        popad                       // Restore all registers
        pop     ebp                 // Restore EBP
        ret     12
    }
}
#else

EXTERN_C void FunctionTailcallNaked(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func);

#endif

// ----  MAPPING FUNCTIONS ------------------

// this function is called by the CLR when a function has been mapped to an ID
UINT_PTR CProfiler::FunctionMapper(FunctionID functionID, BOOL *pbHookFunction)
{
  UINT_PTR res = 0;
	// make sure the global reference to our profiler is valid.  Forward this
	// call to our profiler object
  if (g_pICorProfilerCallback != NULL)
  {
    res = g_pICorProfilerCallback->MapFunction(functionID);
    *pbHookFunction = res;
  }

	// we must return the function ID passed as a parameter
	return res;
}

const wchar_t* FILTERS[] = { L"System.", L"Microsoft.Win32.", L"<Module>::", L"<CrtImplementationDetails>." };

// the static function called by .Net when a function has been mapped to an ID
UINT_PTR CProfiler::MapFunction(FunctionID functionID)
{
  ThreadID tid;
  if (CORPROF_E_NOT_MANAGED_THREAD != 
      m_pICorProfilerInfo->GetCurrentThreadID(&tid))
  {
    // see if this function is in the map
    std::map<FunctionID, FUNCTIONINFO*>::iterator iter = m_functionMap.find(functionID);
    if (iter == m_functionMap.end())
    {
	    // declared in this block so they are not created if the function is found
	    WCHAR szMethodName[NAME_BUFFER_SIZE];
	    const WCHAR* p = NULL;
	    USES_CONVERSION;

      BOOL isstatic, isprop;
      ULONG argcount;
      WCHAR returntype[NAME_BUFFER_SIZE];
      WCHAR params[NAME_BUFFER_SIZE];


      GetFunctionProperties(functionID, &isstatic, &isprop, &argcount, returntype, NAME_BUFFER_SIZE, params, NAME_BUFFER_SIZE, szMethodName, NAME_BUFFER_SIZE);

      BOOL instr = true;

      for (int i = 0; i < ARRAY_SIZE(FILTERS); i++)
      {
        instr &= wcsncmp(szMethodName, FILTERS[i], wcslen(FILTERS[i])) != 0;
        if (!instr)
        {
          break;
        }
      }

      //if (instr)
      {
        WCHAR methname[NAME_BUFFER_SIZE];
        
        wsprintf(methname, L"%s(%s):%s", szMethodName, params, returntype);

        FUNCTIONINFO* fi = (FUNCTIONINFO*)malloc(sizeof(FUNCTIONINFO));
        fi->callcount = 0;
        fi->extime = 0;
        fi->inctime = 0;
        fi->name = wcsdup(methname);

        m_functionMap.insert(std::pair<FunctionID, FUNCTIONINFO*>(functionID, fi));

        return (UINT_PTR) fi;
      }
      return 0;
    }
  }
  return 0;
}

STACK* MakeStack(int size)
{
  STACK* s = (STACK*) malloc(sizeof(STACK));
  s->buffer = (FUNCTIONSAMPLE*) malloc(sizeof(FUNCTIONSAMPLE) * size);
  s->size = size;
  s->top = -1;
  return s;
}

__forceinline FUNCTIONSAMPLE* Peek(STACK* stack)
{
  return &stack->buffer[stack->top];
}

__forceinline FUNCTIONSAMPLE* Pop(STACK* stack)
{
  return &stack->buffer[stack->top--];
}

__forceinline void Push(STACK* stack, FUNCTIONSAMPLE s)
{
  stack->top++;
  if (stack->top == stack->size)
  {
    int newsize = stack->size * 2;
    stack->buffer = (FUNCTIONSAMPLE*) realloc(stack->buffer, sizeof(FUNCTIONSAMPLE) * newsize);
    stack->size = newsize;
  }
  stack->buffer[stack->top] = s;
}

void FreeStack(STACK* stack)
{
  free(stack->buffer);
  free(stack);
}

__forceinline BOOL IsEmpty(STACK* s)
{
  return s->top == -1;
}


__forceinline FUNCTIONSAMPLE MakeSample(FUNCTIONINFO* fi, __int64 time)
{
  FUNCTIONSAMPLE fs;
  fs.info = fi;
  fs.extime = 0;
  fs.inctime = 0;
  fs.lastextime = time;
  fs.lastinctime = time;

  return fs;
}

__forceinline BOOL IsInStack(STACK* s, FUNCTIONINFO* fi)
{
  for (int i = s->top; i >= 0; i--)
  {
    if (s->buffer[i].info == fi)
    {
      return TRUE;
    }
  }
  return FALSE;
}


// ----  CALLBACK HANDLER FUNCTIONS ------------------

// our real handler for FunctionEnter notification
__forceinline void CProfiler::Enter(FunctionID functionID, FUNCTIONINFO* fi)
{
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);

  __int64 time = t.QuadPart - init_time.QuadPart;

  if (!IsEmpty(m_stack))
  {
    FUNCTIONSAMPLE* prev = Peek(m_stack);
    prev->extime += (time - prev->lastextime);
  }

  fi->callcount++;

  FUNCTIONSAMPLE fs = MakeSample(fi, time);
  Push(m_stack, fs);
}

// our real handler for FunctionLeave notification
__forceinline void CProfiler::Leave(FunctionID functionID, FUNCTIONINFO* fi)
{
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);

  __int64 time = t.QuadPart - init_time.QuadPart;

  FUNCTIONSAMPLE* leave = Pop(m_stack);
  leave->extime += (time - leave->lastextime);
  leave->inctime += (time - leave->lastinctime);
  leave->info->extime += leave->extime;

  if (!IsInStack(m_stack, leave->info))
  {
    leave->info->inctime += leave->inctime;
  }

  if (!IsEmpty(m_stack))
  {
    FUNCTIONSAMPLE* parent = Peek(m_stack);
    parent->lastextime = time;
  }
}

// our real handler for the FunctionTailcall notification
__forceinline void CProfiler::Tailcall(FunctionID functionID, FUNCTIONINFO* fi)
{
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);

  __int64 time = t.QuadPart - init_time.QuadPart;

  FUNCTIONSAMPLE* leave = Pop(m_stack);
  leave->extime += (time - leave->lastextime);
  leave->inctime += (time - leave->lastinctime);
  leave->info->extime += leave->extime;

  if (!IsInStack(m_stack, leave->info))
  {
    leave->info->inctime += leave->inctime;
  }

  if (!IsEmpty(m_stack))
  {
    FUNCTIONSAMPLE* parent = Peek(m_stack);
    parent->lastextime = time;
  }
}

// ----  ICorProfilerCallback IMPLEMENTATION ------------------

// called when the profiling object is created by the CLR
STDMETHODIMP CProfiler::Initialize(IUnknown *pICorProfilerInfoUnk)
{
  //DebugBreak();
	// set up our global access pointer
	g_pICorProfilerCallback = this;
  m_stack = MakeStack(16384);

	// get the ICorProfilerInfo interface
    HRESULT hr = pICorProfilerInfoUnk->QueryInterface(IID_ICorProfilerInfo, (LPVOID*)&m_pICorProfilerInfo);
    if (FAILED(hr))
        return E_FAIL;
	// determine if this object implements ICorProfilerInfo2
    hr = pICorProfilerInfoUnk->QueryInterface(IID_ICorProfilerInfo2, (LPVOID*)&m_pICorProfilerInfo2);
    if (FAILED(hr))
	{
		// we still want to work if this call fails, might be an older .NET version
		m_pICorProfilerInfo2.p = NULL;
	}

	// determine if this object implements ICorProfilerInfo3
    hr = pICorProfilerInfoUnk->QueryInterface(IID_ICorProfilerInfo3, (LPVOID*)&m_pICorProfilerInfo3);
    if (FAILED(hr))
	{
		// we still want to work if this call fails, might be an older .NET version
		m_pICorProfilerInfo3.p = NULL;
	}

	// Indicate which events we're interested in.
	hr = SetEventMask();
    if (FAILED(hr))
       return E_FAIL;

	// set the enter, leave and tailcall hooks
	if (m_pICorProfilerInfo2.p == NULL)
	{
		// note that we are casting our functions to the definitions for the callbacks
		hr = m_pICorProfilerInfo->SetEnterLeaveFunctionHooks((FunctionEnter*)&FunctionEnterNaked, 
      (FunctionLeave*)&FunctionLeaveNaked, (FunctionTailcall*)&FunctionTailcallNaked);
		if (SUCCEEDED(hr))
			hr = m_pICorProfilerInfo->SetFunctionIDMapper((FunctionIDMapper*)&FunctionMapper);
	}
	else
	{
		hr = m_pICorProfilerInfo2->SetEnterLeaveFunctionHooks2(FunctionEnterNaked, FunctionLeaveNaked, FunctionTailcallNaked);
		if (SUCCEEDED(hr))
			hr = m_pICorProfilerInfo2->SetFunctionIDMapper(FunctionMapper);
	}
	// report our success or failure to the log file
    if (FAILED(hr))
      return E_FAIL;

    QueryPerformanceCounter(&init_time);

    return S_OK;
}

// called when the profiler is being terminated by the CLR
STDMETHODIMP CProfiler::Shutdown()
{
  FreeStack(m_stack);

  LARGE_INTEGER f;
  QueryPerformanceFrequency(&f);

  double fz = f.QuadPart/1000.0;

  FILE* output = fopen("report.tab", "w");

  fprintf(output, "Name\tCount\tInc Time\tEx Time\tAvg Inc Time\tAvg Ex Time\n");

  std::map<FunctionID, FUNCTIONINFO*>::iterator iter =  m_functionMap.begin();
  
  while (iter != m_functionMap.end())
  {
    FUNCTIONINFO* fi = iter->second;

    fprintf(output, "%S\t%i\t%.2f\t%.2f\t%.4f\t%.4f\n",
      fi->name,
      fi->callcount,
      fi->inctime / fz,
      fi->extime / fz,
      (fi->inctime / fz) / fi->callcount,
      (fi->extime / fz)  / fi->callcount);

    free(fi->name);
    free(fi);
    iter++;
  }

  fflush(output);
  fclose(output);

	// tear down our global access pointers
	g_pICorProfilerCallback = NULL;

  return S_OK;
}


///<summary>
// We are monitoring events that are interesting for determining
// the hot spots of a managed CLR program (profilee). This includes
// thread related events, function enter/leave events, exception 
// related events, and unmanaged/managed transition events. Note 
// that we disable inlining. Although this does indeed affect the 
// execution time, it provides better accuracy for determining
// hot spots.
//
// If the system does not support high precision counters, then
// do not profile anything. This is determined in the constructor.
///</summary>
HRESULT CProfiler::SetEventMask()
{
	//COR_PRF_MONITOR_NONE	= 0,
	//COR_PRF_MONITOR_FUNCTION_UNLOADS	= 0x1,
	//COR_PRF_MONITOR_CLASS_LOADS	= 0x2,
	//COR_PRF_MONITOR_MODULE_LOADS	= 0x4,
	//COR_PRF_MONITOR_ASSEMBLY_LOADS	= 0x8,
	//COR_PRF_MONITOR_APPDOMAIN_LOADS	= 0x10,
	//COR_PRF_MONITOR_JIT_COMPILATION	= 0x20,
	//COR_PRF_MONITOR_EXCEPTIONS	= 0x40,
	//COR_PRF_MONITOR_GC	= 0x80,
	//COR_PRF_MONITOR_OBJECT_ALLOCATED	= 0x100,
	//COR_PRF_MONITOR_THREADS	= 0x200,
	//COR_PRF_MONITOR_REMOTING	= 0x400,
	//COR_PRF_MONITOR_CODE_TRANSITIONS	= 0x800,
	//COR_PRF_MONITOR_ENTERLEAVE	= 0x1000,
	//COR_PRF_MONITOR_CCW	= 0x2000,
	//COR_PRF_MONITOR_REMOTING_COOKIE	= 0x4000 | COR_PRF_MONITOR_REMOTING,
	//COR_PRF_MONITOR_REMOTING_ASYNC	= 0x8000 | COR_PRF_MONITOR_REMOTING,
	//COR_PRF_MONITOR_SUSPENDS	= 0x10000,
	//COR_PRF_MONITOR_CACHE_SEARCHES	= 0x20000,
	//COR_PRF_MONITOR_CLR_EXCEPTIONS	= 0x1000000,
	//COR_PRF_MONITOR_ALL	= 0x107ffff,
	//COR_PRF_ENABLE_REJIT	= 0x40000,
	//COR_PRF_ENABLE_INPROC_DEBUGGING	= 0x80000,
	//COR_PRF_ENABLE_JIT_MAPS	= 0x100000,
	//COR_PRF_DISABLE_INLINING	= 0x200000,
	//COR_PRF_DISABLE_OPTIMIZATIONS	= 0x400000,
	//COR_PRF_ENABLE_OBJECT_ALLOCATED	= 0x800000,
	// New in VS2005
	//	COR_PRF_ENABLE_FUNCTION_ARGS	= 0x2000000,
	//	COR_PRF_ENABLE_FUNCTION_RETVAL	= 0x4000000,
	//  COR_PRF_ENABLE_FRAME_INFO	= 0x8000000,
	//  COR_PRF_ENABLE_STACK_SNAPSHOT	= 0x10000000,
	//  COR_PRF_USE_PROFILE_IMAGES	= 0x20000000,
	// End New in VS2005
	//COR_PRF_ALL	= 0x3fffffff,
	//COR_PRF_MONITOR_IMMUTABLE	= COR_PRF_MONITOR_CODE_TRANSITIONS | COR_PRF_MONITOR_REMOTING | COR_PRF_MONITOR_REMOTING_COOKIE | COR_PRF_MONITOR_REMOTING_ASYNC | COR_PRF_MONITOR_GC | COR_PRF_ENABLE_REJIT | COR_PRF_ENABLE_INPROC_DEBUGGING | COR_PRF_ENABLE_JIT_MAPS | COR_PRF_DISABLE_OPTIMIZATIONS | COR_PRF_DISABLE_INLINING | COR_PRF_ENABLE_OBJECT_ALLOCATED | COR_PRF_ENABLE_FUNCTION_ARGS | COR_PRF_ENABLE_FUNCTION_RETVAL | COR_PRF_ENABLE_FRAME_INFO | COR_PRF_ENABLE_STACK_SNAPSHOT | COR_PRF_USE_PROFILE_IMAGES

	// set the event mask 
	DWORD eventMask = (DWORD)(COR_PRF_MONITOR_ENTERLEAVE | COR_PRF_DISABLE_INLINING);
	return m_pICorProfilerInfo->SetEventMask(eventMask);
}

// creates the fully scoped name of the method in the provided buffer
HRESULT CProfiler::GetFullMethodName(FunctionID functionID, LPWSTR wszMethod, int cMethod)
{
	IMetaDataImport* pIMetaDataImport = 0;
	HRESULT hr = S_OK;
	mdToken funcToken = 0;
	WCHAR szFunction[NAME_BUFFER_SIZE];
	WCHAR szClass[NAME_BUFFER_SIZE];

	// get the token for the function which we will use to get its name
	hr = m_pICorProfilerInfo->GetTokenAndMetaDataFromFunction(functionID, IID_IMetaDataImport, (LPUNKNOWN *) &pIMetaDataImport, &funcToken);
	if(SUCCEEDED(hr))
	{
		mdTypeDef classTypeDef;
		ULONG cchFunction;
		ULONG cchClass;

		// retrieve the function properties based on the token
		hr = pIMetaDataImport->GetMethodProps(funcToken, &classTypeDef, szFunction, NAME_BUFFER_SIZE, &cchFunction, 0, 0, 0, 0, 0);
		if (SUCCEEDED(hr))
		{
			// get the function name
			hr = pIMetaDataImport->GetTypeDefProps(classTypeDef, szClass, NAME_BUFFER_SIZE, &cchClass, 0, 0);
			if (SUCCEEDED(hr))
			{
				// create the fully qualified name
				_snwprintf_s(wszMethod,cMethod,cMethod,L"%s.%s",szClass,szFunction);
			}
		}
		// release our reference to the metadata
		pIMetaDataImport->Release();
	}

	return hr;
}

/***************************************************************************************
 *  Method:
 *
 *
 *  Purpose:
 *
 *
 *  Parameters: 
 *
 *
 *  Return value:
 *
 *
 *  Notes:
 *
 ***************************************************************************************/
/* public */
HRESULT CProfiler::GetNameFromClassID( ClassID classID, WCHAR className[] )
{
    HRESULT hr = E_FAIL;
    
    
    if ( m_pICorProfilerInfo != NULL )
    {
        ModuleID moduleID;
        mdTypeDef classToken;

        
        hr = m_pICorProfilerInfo->GetClassIDInfo( classID, 
                                              &moduleID,  
                                              &classToken );                                                                                                                                              
        if ( SUCCEEDED( hr ) )
        {             
            IMetaDataImport *pMDImport = NULL;
                
            
            hr = m_pICorProfilerInfo->GetModuleMetaData( moduleID, 
                                                     (ofRead | ofWrite),
                                                     IID_IMetaDataImport, 
                                                     (IUnknown **)&pMDImport );
            if ( SUCCEEDED( hr ) )
            {
                if ( classToken != mdTypeDefNil )
                {
                    ClassID *classTypeArgs = NULL;
                    ULONG32 classTypeArgCount = 0;
#ifdef mdGenericPar
                    if (m_pICorProfilerInfo2 != NULL)
                    {
                        hr = m_pICorProfilerInfo2->GetClassIDInfo2(classID,
                                                               NULL,
                                                               NULL,
                                                               NULL,
                                                               0,
                                                               &classTypeArgCount,
                                                               NULL);
                        
                        if (SUCCEEDED(hr) && classTypeArgCount > 0)
                        {
                            classTypeArgs = (ClassID *)_alloca(classTypeArgCount*sizeof(classTypeArgs[0]));

                            hr = m_pICorProfilerInfo2->GetClassIDInfo2(classID,
                                                                   NULL,
                                                                   NULL,
                                                                   NULL,
                                                                   classTypeArgCount,
                                                                   &classTypeArgCount,
                                                                   classTypeArgs);
                        }
                        if (!SUCCEEDED(hr))
                            classTypeArgs = NULL;
                    }
#endif // mdGenericPar
                    DWORD dwTypeDefFlags = 0;
                    ULONG genericArgCount = 0;
                    hr = GetClassName(pMDImport, classToken, className, classTypeArgs, &genericArgCount);
                    if ( FAILED( hr ) )
                    {
                        ;//Failure( "_GetClassNameHelper() FAILED" );
                    }
                }
                else
                    ;//DEBUG_OUT( ("The class token is mdTypeDefNil, class does NOT have MetaData info") );


                pMDImport->Release ();
            }
            else
            {
//                Failure( "IProfilerInfo::GetModuleMetaData() => IMetaDataImport FAILED" );
                wcscpy_s(className, MAX_LENGTH, L"???");
                hr = S_OK;
            }
        }
        else    
            ;//Failure( "ICorProfilerInfo::GetClassIDInfo() FAILED" );
    }
    else
        ;//Failure( "ICorProfilerInfo Interface has NOT been Initialized" );


    return hr;

} // PrfHelper::GetNameFromClassID



static void StrAppend(__out_ecount(cchBuffer) char *buffer, const char *str, size_t cchBuffer)
{
    size_t bufLen = strlen(buffer) + 1;
    if (bufLen <= cchBuffer)
        strncat_s(buffer, cchBuffer, str, cchBuffer-bufLen);
}

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

void CProfiler::AppendTypeArgName(ULONG argIndex, ClassID *actualClassTypeArgs, ClassID *actualMethodTypeArgs, 
                                  BOOL methodFormalArg, __out_ecount(cchBuffer) char *buffer, size_t cchBuffer)
{
    char argName[MAX_LENGTH];

    argName[0] = '\0';

    ClassID classId = 0;
    if (methodFormalArg && actualMethodTypeArgs != NULL)
        classId = actualMethodTypeArgs[argIndex];
    if (!methodFormalArg && actualClassTypeArgs != NULL)
        classId = actualClassTypeArgs[argIndex];

    if (classId != 0)
    {
        WCHAR className[MAX_LENGTH];

        HRESULT hr = GetNameFromClassID(classId, className);
        if (SUCCEEDED(hr))
            _snprintf_s( argName, ARRAY_LEN(argName), ARRAY_LEN(argName)-1, "%S", className);
    }

    if (argName[0] == '\0')
    {
        char argStart = methodFormalArg ? 'M' : 'T';
        if (argIndex <= 6)
        {
            // the first 7 parameters are printed as M, N, O, P, Q, R, S 
            // or as T, U, V, W, X, Y, Z 
            sprintf_s( argName, ARRAY_LEN(argName), "%c", argIndex + argStart);
        }
        else
        {
            // everything after that as M7, M8, ... or T7, T8, ...
            sprintf_s( argName, ARRAY_LEN(argName), "%c%u", argStart, argIndex);
        }
    }

    StrAppend( buffer, argName, cchBuffer);
}

/***************************************************************************************
 *  Method:
 *
 *
 *  Purpose:
 *
 *
 *  Parameters: 
 *
 *
 *  Return value:
 *
 *
 *  Notes:
 *
 ***************************************************************************************/
DECLSPEC
/* static public */
PCCOR_SIGNATURE CProfiler::ParseElementType( IMetaDataImport *pMDImport,
                                           PCCOR_SIGNATURE signature,
                                           ClassID *classTypeArgs,
                                           ClassID *methodTypeArgs,
                                           __out_ecount(cchBuffer) char *buffer,
                                           size_t cchBuffer)
{   
    switch ( *signature++ ) 
    {   
        case ELEMENT_TYPE_VOID:
            StrAppend( buffer, "void", cchBuffer);   
            break;                  
        
        
        case ELEMENT_TYPE_BOOLEAN:  
            StrAppend( buffer, "bool", cchBuffer);   
            break;  
        
        
        case ELEMENT_TYPE_CHAR:
            StrAppend( buffer, "wchar", cchBuffer);  
            break;      
                    
        
        case ELEMENT_TYPE_I1:
            StrAppend( buffer, "int8", cchBuffer );   
            break;      
        
        
        case ELEMENT_TYPE_U1:
            StrAppend( buffer, "unsigned int8", cchBuffer );  
            break;      
        
        
        case ELEMENT_TYPE_I2:
            StrAppend( buffer, "int16", cchBuffer );  
            break;      
        
        
        case ELEMENT_TYPE_U2:
            StrAppend( buffer, "unsigned int16", cchBuffer ); 
            break;          
        
        
        case ELEMENT_TYPE_I4:
            StrAppend( buffer, "int32", cchBuffer );  
            break;
            
        
        case ELEMENT_TYPE_U4:
            StrAppend( buffer, "unsigned int32", cchBuffer ); 
            break;      
        
        
        case ELEMENT_TYPE_I8:
            StrAppend( buffer, "int64", cchBuffer );  
            break;      
        
        
        case ELEMENT_TYPE_U8:
            StrAppend( buffer, "unsigned int64", cchBuffer ); 
            break;      
        
        
        case ELEMENT_TYPE_R4:
            StrAppend( buffer, "float32", cchBuffer );    
            break;          
        
        
        case ELEMENT_TYPE_R8:
            StrAppend( buffer, "float64", cchBuffer );    
            break;      
        
        
        case ELEMENT_TYPE_U:
            StrAppend( buffer, "unsigned int_ptr", cchBuffer );   
            break;       
        
        
        case ELEMENT_TYPE_I:
            StrAppend( buffer, "int_ptr", cchBuffer );    
            break;            
        
        
        case ELEMENT_TYPE_OBJECT:
            StrAppend( buffer, "Object", cchBuffer ); 
            break;       
        
        
        case ELEMENT_TYPE_STRING:
            StrAppend( buffer, "String", cchBuffer ); 
            break;       
        
        
        case ELEMENT_TYPE_TYPEDBYREF:
            StrAppend( buffer, "refany", cchBuffer ); 
            break;                     

        case ELEMENT_TYPE_CLASS:    
        case ELEMENT_TYPE_VALUETYPE:
        case ELEMENT_TYPE_CMOD_REQD:
        case ELEMENT_TYPE_CMOD_OPT:
            {   
                mdToken token;  
                char classname[MAX_LENGTH];


                classname[0] = '\0';
                signature += CorSigUncompressToken( signature, &token ); 
                if ( TypeFromToken( token ) != mdtTypeRef )
                {
                    HRESULT hr;
                    WCHAR zName[MAX_LENGTH];
                    
                    
                    hr = pMDImport->GetTypeDefProps( token, 
                                                     zName,
                                                     MAX_LENGTH,
                                                     NULL,
                                                     NULL,
                                                     NULL );
                    if ( SUCCEEDED( hr ) )
                    {
                        size_t convertedChars;
                        wcstombs_s( &convertedChars, classname, ARRAY_LEN(classname), zName, ARRAY_LEN(zName) );
                    }
                }
                    
                StrAppend( buffer, classname, cchBuffer );        
            }
            break;  
        
        
        case ELEMENT_TYPE_SZARRAY:   
            signature = ParseElementType( pMDImport, signature, classTypeArgs, methodTypeArgs, buffer, cchBuffer ); 
            StrAppend( buffer, "[]", cchBuffer );
            break;      
        
        
        case ELEMENT_TYPE_ARRAY:    
            {   
                ULONG rank;
                

                signature = ParseElementType( pMDImport, signature, classTypeArgs, methodTypeArgs, buffer, cchBuffer );                 
                rank = CorSigUncompressData( signature );  
                
                // The second condition is to guard against overflow bugs & shut up PREFAST
                if ( rank == 0 || rank >= 65536 ) 
                    StrAppend( buffer, "[?]", cchBuffer );

                else 
                {
                    ULONG *lower;   
                    ULONG *sizes;   
                    ULONG numsizes; 
                    ULONG arraysize = (sizeof ( ULONG ) * 2 * rank);
                    
                                         
                    lower = (ULONG *)_alloca( arraysize );                                                        
                    memset( lower, 0, arraysize ); 
                    sizes = &lower[rank];

                    numsizes = CorSigUncompressData( signature );   
                    if ( numsizes <= rank )
                    {
                        ULONG numlower;
                        ULONG i;                        
                        
                        for (i = 0; i < numsizes; i++ )  
                            sizes[i] = CorSigUncompressData( signature );   
                        
                        
                        numlower = CorSigUncompressData( signature );   
                        if ( numlower <= rank )
                        {
                            for ( i = 0; i < numlower; i++) 
                                lower[i] = CorSigUncompressData( signature ); 
                            
                            
                            StrAppend( buffer, "[", cchBuffer );  
                            for ( i = 0; i < rank; i++ )    
                            {   
                                if ( (sizes[i] != 0) && (lower[i] != 0) )   
                                {   
                                    char sizeBuffer[100];
                                    if ( lower[i] == 0 )
                                        sprintf_s ( sizeBuffer, ARRAY_LEN(sizeBuffer), "%d", sizes[i] ); 
                                    else    
                                    {   
                                        sprintf_s( sizeBuffer, ARRAY_LEN(sizeBuffer), "%d...", lower[i] );  
                                        
                                        if ( sizes[i] != 0 )    
                                            sprintf_s( sizeBuffer, ARRAY_LEN(sizeBuffer), "%d...%d", lower[i], (lower[i] + sizes[i] + 1) ); 
                                    }   
                                    StrAppend( buffer, sizeBuffer, cchBuffer );    
                                }
                                    
                                if ( i < (rank - 1) ) 
                                    StrAppend( buffer, ",", cchBuffer );  
                            }   
                            
                            StrAppend( buffer, "]", cchBuffer );  
                        }                       
                    }
                }
            } 
            break;  

        
        case ELEMENT_TYPE_PINNED:
            signature = ParseElementType( pMDImport, signature, classTypeArgs, methodTypeArgs, buffer, cchBuffer ); 
            StrAppend( buffer, "pinned", cchBuffer ); 
            break;  
         
        
        case ELEMENT_TYPE_PTR:   
            signature = ParseElementType( pMDImport, signature, classTypeArgs, methodTypeArgs, buffer, cchBuffer ); 
            StrAppend( buffer, "*", cchBuffer );  
            break;   
        
        
        case ELEMENT_TYPE_BYREF:   
            signature = ParseElementType( pMDImport, signature, classTypeArgs, methodTypeArgs, buffer, cchBuffer ); 
            StrAppend( buffer, "&", cchBuffer );  
            break;              

#ifdef mdGenericPar
		case ELEMENT_TYPE_VAR:
            AppendTypeArgName(CorSigUncompressData(signature), classTypeArgs, methodTypeArgs, FALSE, buffer, cchBuffer);
            break;

        case ELEMENT_TYPE_MVAR:
            AppendTypeArgName(CorSigUncompressData(signature), classTypeArgs, methodTypeArgs, TRUE, buffer, cchBuffer);
            break;
#endif // mdGenericPar

        default:    
        case ELEMENT_TYPE_END:  
        case ELEMENT_TYPE_SENTINEL: 
            StrAppend( buffer, "<UNKNOWN>", cchBuffer );  
            break;                                                              
                            
    } // switch 
    
    
    return signature;

} // PrfInfo::ParseElementType


DECLSPEC
/* static public */
HRESULT CProfiler::GetClassName(IMetaDataImport *pMDImport, mdToken classToken, WCHAR className[], ClassID *classTypeArgs, ULONG *totalGenericArgCount)
{
    DWORD dwTypeDefFlags = 0;
    HRESULT hr = S_OK;
    hr = pMDImport->GetTypeDefProps( classToken, 
                                     className, 
                                     MAX_LENGTH,
                                     NULL, 
                                     &dwTypeDefFlags, 
                                     NULL ); 
    if ( FAILED( hr ) )
    {
        return hr;
    }
    *totalGenericArgCount = 0;
    if (IsTdNested(dwTypeDefFlags))
    {
//      printf("%S is a nested class\n", className);
        mdToken enclosingClass = mdTokenNil;
        hr = pMDImport->GetNestedClassProps(classToken, &enclosingClass);
        if ( FAILED( hr ) )
        {
            return hr;
        }
//      printf("Enclosing class for %S is %d\n", className, enclosingClass);
        hr = GetClassName(pMDImport, enclosingClass, className, classTypeArgs, totalGenericArgCount);
//      printf("Enclosing class name %S\n", className);
        if (FAILED(hr))
            return hr;
        size_t length = wcslen(className);
        if (length + 2 < MAX_LENGTH)
        {
            className[length++] = '.';
            hr = pMDImport->GetTypeDefProps( classToken, 
                                            className + length, 
                                            (ULONG)(MAX_LENGTH - length),
                                            NULL, 
                                            NULL, 
                                            NULL );
            if ( FAILED( hr ) )
            {
                return hr;
            }
//          printf("%S is a nested class\n", className);
        }
    }

    WCHAR *backTick = wcschr(className, L'`');
    if (backTick != NULL)
    {
        *backTick = L'\0';
        ULONG genericArgCount = wcstoul(backTick+1, NULL, 10);

        if (genericArgCount >0)
        {
            char typeArgText[MAX_LENGTH];
            typeArgText[0] = '\0';

            StrAppend(typeArgText, "<", MAX_LENGTH);
            for (ULONG i = *totalGenericArgCount; i < *totalGenericArgCount + genericArgCount; i++)
            {
                if (i != *totalGenericArgCount)
                    StrAppend(typeArgText, ",", MAX_LENGTH);
                AppendTypeArgName(i, classTypeArgs, NULL, FALSE, typeArgText, MAX_LENGTH);
            }
            StrAppend(typeArgText, ">", MAX_LENGTH);

            *totalGenericArgCount += genericArgCount;
    
            _snwprintf_s(className, MAX_LENGTH, MAX_LENGTH-1, L"%s%S", className, typeArgText);
        }
    }
    return hr;
}

/***************************************************************************************
 *  Method:
 *
 *
 *  Purpose:
 *
 *
 *  Parameters: 
 *
 *
 *  Return value:
 *
 *
 *  Notes:
 *
 ***************************************************************************************/
DECLSPEC
/* static public */
HRESULT CProfiler::GetFunctionProperties( FunctionID functionID,
                                        BOOL *isStatic,
                                        BOOL *isprop,
                                        ULONG *argCount,
                                        __out_ecount(returnTypeStrLen) WCHAR *returnTypeStr, 
                                        size_t returnTypeStrLen,
                                        __out_ecount(functionParametersLen) WCHAR *functionParameters,
                                        size_t functionParametersLen,
                                        __out_ecount(functionNameLen) WCHAR *functionName,
                                        size_t functionNameLen )
{
    HRESULT hr = E_FAIL; // assume success
            
    _ASSERTE(returnTypeStr != NULL && returnTypeStrLen > 0);
    returnTypeStr[0] = 0;

    _ASSERTE(functionParameters != NULL && functionParametersLen > 0);
    functionParameters[0] = 0;

    _ASSERTE(functionName != NULL && functionNameLen > 0);
    functionName[0] = 0;
    *isprop = 0;

    if ( functionID != NULL )
    {
        mdToken funcToken = mdTypeDefNil;
        IMetaDataImport *pMDImport = NULL;      
        WCHAR funName[MAX_LENGTH] = L"UNKNOWN";
                
                
        
        //
        // Get the MetadataImport interface and the metadata token 
        //
        hr = m_pICorProfilerInfo->GetTokenAndMetaDataFromFunction( functionID, 
                                                               IID_IMetaDataImport, 
                                                               (IUnknown **)&pMDImport,
                                                               &funcToken );
        if ( SUCCEEDED( hr ) )
        {
            mdTypeDef classToken = mdTypeDefNil;
            DWORD methodAttr = 0;
            PCCOR_SIGNATURE sigBlob = NULL;

            hr = pMDImport->GetMethodProps( funcToken,
                                            &classToken,
                                            funName,
                                            MAX_LENGTH,
                                            0,
                                            &methodAttr,
                                            &sigBlob,
                                            NULL,
                                            NULL, 
                                            NULL );
            if ( SUCCEEDED( hr ) )
            {
                WCHAR className[MAX_LENGTH] = L"UNKNOWN";
                ClassID classId =0;

#ifdef __ICorProfilerInfo2_INTERFACE_DEFINED__
                if (m_pICorProfilerInfo2 != NULL)
                {
                    hr = m_pICorProfilerInfo2->GetFunctionInfo2(functionID,
                                                            0,
                                                            &classId,
                                                            NULL,
                                                            NULL,
                                                            0,
                                                            NULL,
                                                            NULL);
                    if (!SUCCEEDED(hr))
                        classId = 0;
                }
#endif // __ICorProfilerInfo2_INTERFACE_DEFINED__

                if (classId == 0)
                {
                    hr = m_pICorProfilerInfo->GetFunctionInfo(functionID,
                                                          &classId,
                                                          NULL,
                                                          NULL);
                }
                if (SUCCEEDED(hr) && classId != 0)
                {
                    hr = GetNameFromClassID(classId, className);
                }
                else if (classToken != mdTypeDefNil)
                {
                    ULONG classGenericArgCount = 0;
                    hr = GetClassName(pMDImport, classToken, className, NULL, &classGenericArgCount);
                }
                _snwprintf_s( functionName, functionNameLen, functionNameLen-1, L"%s::%s", className, funName );                    


                ULONG callConv;


                //
                // Is the method static ?
                //
                (*isStatic) = (BOOL)((methodAttr & mdStatic) != 0);

                //if (!wcsncmp(L"get_", funName, 4) || !wcsncmp(L"set_", funName, 4))
                //{
                //  *isprop = TRUE;
                //}
 
                //
                // Make sure we have a method signature.
                //
                char buffer[2 * MAX_LENGTH];
                
                
                sigBlob += CorSigUncompressData( sigBlob, &callConv );
                if ( callConv != IMAGE_CEE_CS_CALLCONV_FIELD )
                {
                    static char* callConvNames[8] = 
                    {   
                        "", 
                        "unmanaged cdecl ", 
                        "unmanaged stdcall ",  
                        "unmanaged thiscall ", 
                        "unmanaged fastcall ", 
                        "vararg ",  
                        "<error> "  
                        "<error> "  
                    };  
                    buffer[0] = '\0';
                    if ( (callConv & 7) != 0 )
                        sprintf_s( buffer, ARRAY_LEN(buffer), "%s ", callConvNames[callConv & 7]);   
                    
                    ULONG genericArgCount = 0;
                    ClassID *methodTypeArgs = NULL;
                    UINT32 methodTypeArgCount = 0;
                    ClassID *classTypeArgs = NULL;
                    ULONG32 classTypeArgCount = 0;
#ifdef mdGenericPar
                    if ((callConv & IMAGE_CEE_CS_CALLCONV_GENERIC) != 0)
                    {
                        //
                        // Grab the generic type argument count
                        //
                        sigBlob += CorSigUncompressData( sigBlob, &genericArgCount );
                    }
                    if (m_pICorProfilerInfo2 != NULL)
                    {
                        methodTypeArgs = (ClassID *)_alloca(genericArgCount*sizeof(methodTypeArgs[0]));

                        hr = m_pICorProfilerInfo2->GetFunctionInfo2( functionID,
                                                                 0,
                                                                 &classId,
                                                                 NULL,
                                                                 NULL,
                                                                 genericArgCount,
                                                                 &methodTypeArgCount,
                                                                 methodTypeArgs);

                        _ASSERTE(!SUCCEEDED(hr) || genericArgCount == methodTypeArgCount);
                        if (!SUCCEEDED(hr))
                            methodTypeArgs = NULL;
                        else
                        {
                            hr = m_pICorProfilerInfo2->GetClassIDInfo2(classId,
                                                                   NULL,
                                                                   NULL,
                                                                   NULL,
                                                                   0,
                                                                   &classTypeArgCount,
                                                                   NULL);
                        }
                        
                        if (SUCCEEDED(hr) && classTypeArgCount > 0)
                        {
                            classTypeArgs = (ClassID *)_alloca(classTypeArgCount*sizeof(classTypeArgs[0]));

                            hr = m_pICorProfilerInfo2->GetClassIDInfo2(classId,
                                                                   NULL,
                                                                   NULL,
                                                                   NULL,
                                                                   classTypeArgCount,
                                                                   &classTypeArgCount,
                                                                   classTypeArgs);
                        }
                        if (!SUCCEEDED(hr))
                            classTypeArgs = NULL;
                        hr = S_OK;
                    }
#endif // mdGenericPar
                    //
                    // Grab the argument count
                    //
                    sigBlob += CorSigUncompressData( sigBlob, argCount );

                    //
                    // Get the return type
                    //
                    sigBlob = ParseElementType( pMDImport, sigBlob, classTypeArgs, methodTypeArgs, buffer, ARRAY_LEN(buffer) );

                    // The second condition is to guard against overflow bugs & shut up PREFAST
                    if (genericArgCount != 0 && genericArgCount < 65536)
                    {
                        StrAppend(buffer, " <", ARRAY_LEN(buffer));
                        for (ULONG i = 0; i < genericArgCount; i++)
                        {
                            if (i != 0)
                                StrAppend(buffer, ",", ARRAY_LEN(buffer));
                            AppendTypeArgName(i, classTypeArgs, methodTypeArgs, TRUE, buffer, ARRAY_LEN(buffer));
                        }
                        StrAppend(buffer, ">", ARRAY_LEN(buffer));
                    }
                    //
                    // if the return typ returned back empty, write void
                    //
                    if ( buffer[0] == '\0' )
                        sprintf_s( buffer, ARRAY_LEN(buffer), "void" );

                    _snwprintf_s( returnTypeStr, returnTypeStrLen, returnTypeStrLen-1, L"%S",buffer );
                
                    //
                    // Get the parameters
                    //                              
                    for ( ULONG i = 0; 
                          (SUCCEEDED( hr ) && (sigBlob != NULL) && (i < (*argCount))); 
                          i++ )
                    {
                        buffer[0] = '\0';

                        sigBlob = ParseElementType( pMDImport, sigBlob, classTypeArgs, methodTypeArgs, buffer, ARRAY_LEN(buffer)-1 );
                        buffer[ARRAY_LEN(buffer)-1] = '\0';

                        if ( i == 0 ) 
                        {
                            _snwprintf_s( functionParameters, functionParametersLen, functionParametersLen-1, L"%S", buffer );
                        }
                        else if ( sigBlob != NULL )
                        {
                            _snwprintf_s( functionParameters, functionParametersLen, functionParametersLen-1, L"%s+%S", functionParameters, buffer );
                        }
                    
                        else
                            hr = E_FAIL;
                    }
                }
                else
                {
                    //
                    // Get the return type
                    //
                    buffer[0] = '\0';
                    sigBlob = ParseElementType( pMDImport, sigBlob, NULL, NULL, buffer, ARRAY_LEN(buffer)-1 );
                    buffer[ARRAY_LEN(buffer)-1] = L'\0';
                    _snwprintf_s( returnTypeStr, returnTypeStrLen, returnTypeStrLen-1, L"%s %S",returnTypeStr, buffer );
                }
            } 

            pMDImport->Release();
        }
    } 
    //
    // This corresponds to an unmanaged frame
    //
    else
    {
        //
        // Set up return parameters
        //
        hr = S_OK;
        *argCount = 0;
        *isStatic = FALSE;
        wcscpy_s(functionName, functionNameLen, L"UNMANAGED FRAME" );   
    }

    
    return hr;

} // PrfInfo::GetFunctionProperties




