/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef INCLUDED_BLPWTK2_STATICS_H
#define INCLUDED_BLPWTK2_STATICS_H

#include <blpwtk2_config.h>
#include <blpwtk2_threadmode.h>
#include <blpwtk2_pumpmode.h>

#include <base/files/file_path.h>
#include <base/threading/platform_thread.h>

#include <vector>

class MessageLoop;

namespace content {
    class DevToolsHttpHandler;
}  // close namespace content

namespace blpwtk2 {

class HttpTransactionHandler;

// Hold any static variables.  This will be used to store global state that
// may be setup before ToolkitImpl is instantiated, or any other global
// variables, or pointers to global objects that may be used by multiple
// components.
struct Statics {
    // The thread mode selected by the application.  See blpwtk_toolkit.h
    // for an explanation about this.
    static ThreadMode::Value threadMode;

    // The pump mode selected by the application.
    static PumpMode::Value pumpMode;

    // The id of the application's main thread (i.e. the thread that can enter
    // the blpwtk2 library).
    static base::PlatformThreadId applicationMainThreadId;

    // The id of the browser main thread.  In ORIGINAL thread mode, this is
    // the same as applicationMainThreadId.
    static base::PlatformThreadId browserMainThreadId;

    // Pointer to the single global DevToolsHttpHandler object, created and
    // destroyed by DevToolsHttpHandlerDelegateImpl.
    static content::DevToolsHttpHandler* devToolsHttpHandler;

    // The optional http transaction handler installed by the application.
    static HttpTransactionHandler* httpTransactionHandler;

    // MessageLoop for the in-process renderer thread.
    static MessageLoop* rendererMessageLoop;

    // MessageLoop for the browser main thread.
    static MessageLoop* browserMainMessageLoop;

    // Whether or not devtools is available.
    static bool hasDevTools;


    // ====== some utility functions =============

    static std::vector<base::FilePath>& getPluginPaths();
    static void registerPlugin(const char* pluginPath);

    static bool isOriginalThreadMode()
    {
        return ThreadMode::ORIGINAL == threadMode;
    }

    static bool isRendererMainThreadMode()
    {
        return ThreadMode::RENDERER_MAIN == threadMode;
    }

    static bool isInApplicationMainThread()
    {
        return base::PlatformThread::CurrentId() == applicationMainThreadId;
    }

    static bool isInBrowserMainThread()
    {
        return base::PlatformThread::CurrentId() == browserMainThreadId;
    }

    static void initApplicationMainThread();
    static void initBrowserMainThread();
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_STATICS_H

