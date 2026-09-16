/* dinput.h - DirectInput, named by PreRTS.h and so reached by all 600 files in GameEngine.
**
** The devices themselves live in GameEngineDevice/Win32Device (Win32DIMouse, Win32DIKeyboard) and
** are replaced by the platform layer here, not shimmed: DirectInput has no macOS counterpart and a
** fake one would only defer the work.  What this file carries is the handful of names that appear
** in headers, so the include resolves and the engine above it compiles.
*/

#ifndef WIN32COMPAT_SHIM_DINPUT_H
#define WIN32COMPAT_SHIM_DINPUT_H

#include "Platform/Win32Compat.h"

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif

struct IDirectInput8A;
struct IDirectInputDevice8A;
typedef IDirectInput8A *       LPDIRECTINPUT8;
typedef IDirectInputDevice8A * LPDIRECTINPUTDEVICE8;

struct DIMOUSESTATE2 {
	LONG lX, lY, lZ;
	BYTE rgbButtons[8];
};

struct DIDEVICEOBJECTDATA {
	DWORD     dwOfs;
	DWORD     dwData;
	DWORD     dwTimeStamp;
	DWORD     dwSequence;
	UINT_PTR  uAppData;
};

#define DIK_ESCAPE  0x01
#define DI_OK       ((HRESULT)0)

#endif
