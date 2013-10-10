// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_TARGET_PROCESS_H_
#define SANDBOX_WIN_SRC_TARGET_PROCESS_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/sandbox_types.h"

namespace base {
namespace win {

class StartupInformation;

};  // namespace win
};  // namespace base

namespace sandbox {

class AttributeList;
class SharedMemIPCServer;
class ThreadProvider;

// TargetProcess models a target instance (child process). Objects of this
// class are owned by the Policy used to create them.
class TargetProcess {
 public:
  // The constructor takes ownership of |initial_token| and |lockdown_token|.
  TargetProcess(HANDLE initial_token, HANDLE lockdown_token, HANDLE job,
                ThreadProvider* thread_pool);
  ~TargetProcess();

  // TODO(cpu): Currently there does not seem to be a reason to implement
  // reference counting for this class since is internal, but kept the
  // the same interface so the interception framework does not need to be
  // touched at this point.
  void AddRef() {}
  void Release() {}

  // Creates the new target process. The process is created suspended.
  DWORD Create(const wchar_t* exe_path,
               const wchar_t* command_line,
               bool inherit_handles,
               const base::win::StartupInformation& startup_info,
               base::win::ScopedProcessInformation* target_info);

#if SANDBOX_DLL
  // Returns true if the exe has sandbox.lib linked into it. The sandbox DLL
  // will only be injected if this returns false.
  bool ExeHasSandbox() const
  {
    return exe_has_sandbox_;
  }

  // Injects sandbox DLL to target process, while target process is in
  // suspended state.
  DWORD InjectSandboxDll(const wchar_t* module_path);

  // Close the initial token. This is normally done in Create(), but for
  // sandbox dll mode, we need to wait until after InjectSandboxDll.
  void CloseInitialToken();
#endif

  // Returns the target exe name or sandbox DLL name (if the sandbox.lib is
  // linked into a DLL).
  const wchar_t* SandboxModuleName() const
  {
#if SANDBOX_DLL
    if (exe_has_sandbox_)
      return Name();
    DCHECK(module_path_.get());
    return module_path_.get();
#else
    return Name();
#endif
  }

  // Returns the address of the target main exe or sandbox dll (if the
  // sandbox.lib is linked into a DLL).
  HMODULE SandboxModule() const
  {
#if SANDBOX_DLL
    if (exe_has_sandbox_)
      return MainModule();
    DCHECK(module_base_address_);
    return reinterpret_cast<HMODULE>(module_base_address_);
#else
    return MainModule();
#endif
  }

  // Destroys the target process.
  void Terminate();

  // Creates the IPC objects such as the BrokerDispatcher and the
  // IPC server. The IPC server uses the services of the thread_pool.
  DWORD Init(Dispatcher* ipc_dispatcher, void* policy,
             uint32 shared_IPC_size, uint32 shared_policy_size);

  // Returns the handle to the target process.
  HANDLE Process() const {
    return sandbox_process_info_.process_handle();
  }

  // Returns the handle to the job object that the target process belongs to.
  HANDLE Job() const {
    return job_;
  }

  // Returns the address of the target main exe. This is used by the
  // interceptions framework.
  HMODULE MainModule() const {
    return reinterpret_cast<HMODULE>(base_address_);
  }

  // Returns the name of the executable.
  const wchar_t* Name() const {
    return exe_name_.get();
  }

  // Returns the process id.
  DWORD ProcessId() const {
    return sandbox_process_info_.process_id();
  }

  // Returns the handle to the main thread.
  HANDLE MainThread() const {
    return sandbox_process_info_.thread_handle();
  }

  // Transfers a 32-bit variable between the broker and the target.
  ResultCode TransferVariable(const char* name, void* address, size_t size);

 private:
  // Details of the target process.
  base::win::ScopedProcessInformation sandbox_process_info_;
  // The token associated with the process. It provides the core of the
  // sbox security.
  base::win::ScopedHandle lockdown_token_;
  // The token given to the initial thread so that the target process can
  // start. It has more powers than the lockdown_token.
  base::win::ScopedHandle initial_token_;
  // Kernel handle to the shared memory used by the IPC server.
  base::win::ScopedHandle shared_section_;
  // Job object containing the target process.
  HANDLE job_;
  // Reference to the IPC subsystem.
  scoped_ptr<SharedMemIPCServer> ipc_server_;
  // Provides the threads used by the IPC. This class does not own this pointer.
  ThreadProvider* thread_pool_;

#if SANDBOX_DLL
  // True if the exe has sandbox.lib linked into it.
  bool exe_has_sandbox_;
  // Base address of the sandbox module. This is only set if exe_has_sandbox_
  // is is false (i.e. only if a sandbox dll has been injected).
  void* module_base_address_;
  // Full name of the sandbox module. This is only set if exe_has_sandbox_ is
  // false (i.e. only if a sandbox dll has been injected).
  scoped_ptr_malloc<wchar_t>  module_path_;
#endif

  // Base address of the main executable
  void* base_address_;
  // Full name of the target executable.
  scoped_ptr_malloc<wchar_t> exe_name_;

  // Function used for testing.
  friend TargetProcess* MakeTestTargetProcess(HANDLE process,
                                              HMODULE base_address);

  DISALLOW_IMPLICIT_CONSTRUCTORS(TargetProcess);
};

// Creates a mock TargetProcess used for testing interceptions.
// TODO(cpu): It seems that this method is not going to be used anymore.
TargetProcess* MakeTestTargetProcess(HANDLE process, HMODULE base_address);


}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_TARGET_PROCESS_H_
