

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 7.00.0555 */
/* at Fri Aug 05 15:57:19 2011
 */
/* Compiler settings for DotNetProfiler.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 7.00.0555 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif // __RPCNDR_H_VERSION__

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __DotNetProfiler_h__
#define __DotNetProfiler_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IProfiler_FWD_DEFINED__
#define __IProfiler_FWD_DEFINED__
typedef interface IProfiler IProfiler;
#endif 	/* __IProfiler_FWD_DEFINED__ */


#ifndef __Profiler_FWD_DEFINED__
#define __Profiler_FWD_DEFINED__

#ifdef __cplusplus
typedef class Profiler Profiler;
#else
typedef struct Profiler Profiler;
#endif /* __cplusplus */

#endif 	/* __Profiler_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 


#ifndef __IProfiler_INTERFACE_DEFINED__
#define __IProfiler_INTERFACE_DEFINED__

/* interface IProfiler */
/* [unique][helpstring][uuid][object] */ 


EXTERN_C const IID IID_IProfiler;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("98ED3004-7454-49E1-9902-F218F0578CC3")
    IProfiler : public IUnknown
    {
    public:
    };
    
#else 	/* C style interface */

    typedef struct IProfilerVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IProfiler * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            __RPC__deref_out  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IProfiler * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IProfiler * This);
        
        END_INTERFACE
    } IProfilerVtbl;

    interface IProfiler
    {
        CONST_VTBL struct IProfilerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IProfiler_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IProfiler_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IProfiler_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IProfiler_INTERFACE_DEFINED__ */



#ifndef __DotNetProfilerLib_LIBRARY_DEFINED__
#define __DotNetProfilerLib_LIBRARY_DEFINED__

/* library DotNetProfilerLib */
/* [helpstring][version][uuid] */ 


EXTERN_C const IID LIBID_DotNetProfilerLib;

EXTERN_C const CLSID CLSID_Profiler;

#ifdef __cplusplus

class DECLSPEC_UUID("9E2B38F2-7355-4C61-A54F-434B7AC266C0")
Profiler;
#endif
#endif /* __DotNetProfilerLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


