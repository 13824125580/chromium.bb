// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The GYP based build ends up defining USING_V8_SHARED when compiling this
// file.
#undef USING_V8_SHARED
#include "include/v8.h"

#if V8_OS_WIN
#include "src/base/win32-headers.h"

extern "C" void __cdecl __acrt_eagerly_load_locale_apis(void);

extern "C" {
BOOL WINAPI DllMain(HANDLE hinstDLL,
                    DWORD dwReason,
                    LPVOID lpvReserved) {
  if (dwReason == DLL_PROCESS_ATTACH) {
      __acrt_eagerly_load_locale_apis();
  }

  return TRUE;
}
}

#ifdef ALLOCATOR_SHIM
extern __int64 allocator_shim_counter;
extern "C" {
__declspec(dllexport) __int64 GetAllocatorShimCounter()
{
    return allocator_shim_counter;
}
}
#endif

#endif  // V8_OS_WIN
