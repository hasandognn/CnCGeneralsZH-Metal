/* atlbase.h - the four ATL names this tree uses, and nothing else.
**
** PreRTS.h includes <atlbase.h>, and PreRTS.h is included by all 600 files in GameEngine, so its
** absence stopped every one of them.  The actual use is four declarations, all of them in the WOL
** browser: FEBDispatch.h, GameEngine.cpp and WebBrowser.cpp name a CComModule, and W3DWebBrowser
** holds a CComQIPtr.  That subsystem embedded Internet Explorer to show a lobby that EA switched
** off years ago, and none of it builds off Windows.
**
** These declarations exist so the header is satisfied.  They are not an ATL: the browser files are
** excluded from this build, and anything else that reaches for a COM pointer through here is a
** mistake rather than a missing feature.
*/

#ifndef WIN32COMPAT_SHIM_ATLBASE_H
#define WIN32COMPAT_SHIM_ATLBASE_H

#include "Platform/Win32Compat.h"

struct IDispatch;
struct IUnknown;

class CComModule
{
public:
	HRESULT Init(void *, HINSTANCE, const void * = nullptr) { return S_OK; }
	void    Term(void)                                      { }
};

template<class T> class CComPtr
{
public:
	T * p;
	CComPtr(void) : p(nullptr) {}
	CComPtr(T * ptr) : p(ptr) {}
	operator T * (void) const { return p; }
	T * operator -> (void) const { return p; }
};

template<class T> class CComQIPtr : public CComPtr<T>
{
public:
	CComQIPtr(void) {}
	CComQIPtr(IUnknown *) {}       // the QueryInterface this would do has no COM to do it with
};

#endif
