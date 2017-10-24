// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/utility/shell_content_utility_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"

#include "base/files/scoped_temp_dir.h"
#include "base/process/process.h"

// blpwtk2: Remove test-only code
//#include "content/public/test/test_mojo_service.mojom.h"

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shell/public/cpp/interface_registry.h"

#include <chrome/common/chrome_paths.h>
#include <chrome/common/chrome_utility_messages.h>
#include <chrome/utility/printing_handler.h>
#include <content/public/utility/content_utility_client.h>
#include <content/public/utility/utility_thread.h>
#include <ipc/ipc_message_macros.h>

namespace content {

namespace {

// blpwtk2: Remove test-only code
#if 0
class TestMojoServiceImpl : public mojom::TestMojoService {
 public:
  static void Create(mojo::InterfaceRequest<mojom::TestMojoService> request) {
    new TestMojoServiceImpl(std::move(request));
  }

  // mojom::TestMojoService implementation:
  void DoSomething(const DoSomethingCallback& callback) override {
    callback.Run();
  }

  void DoTerminateProcess(const DoTerminateProcessCallback& callback) override {
    base::Process::Current().Terminate(0, false);
  }

  void CreateFolder(const CreateFolderCallback& callback) override {
    // Note: This is used to check if the sandbox is disabled or not since
    //       creating a folder is forbidden when it is enabled.
    callback.Run(base::ScopedTempDir().CreateUniqueTempDir());
  }

  void GetRequestorName(const GetRequestorNameCallback& callback) override {
    NOTREACHED();
  }

 private:
  explicit TestMojoServiceImpl(
      mojo::InterfaceRequest<mojom::TestMojoService> request)
      : binding_(this, std::move(request)) {}

  mojo::StrongBinding<mojom::TestMojoService> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestMojoServiceImpl);
};
#endif

bool Send(IPC::Message* message) {
  return content::UtilityThread::Get()->Send(message);
}

// blpwtk2: Remove test-only code
#if 0
std::unique_ptr<shell::ShellClient> CreateTestApp(
    const base::Closure& quit_closure) {
  return std::unique_ptr<shell::ShellClient>(new TestMojoApp);
}
#endif

}  // namespace

ShellContentUtilityClient::ShellContentUtilityClient() {
  d_handlers.push_back(new printing::PrintingHandler());
}

ShellContentUtilityClient::~ShellContentUtilityClient() {
}

void ShellContentUtilityClient::RegisterMojoApplications(
    StaticMojoApplicationMap* apps) {
  // SHEZ: Remove test-only code
  //MojoApplicationInfo app_info;
  //app_info.application_factory = base::Bind(&CreateTestApp);
  //apps->insert(std::make_pair(kTestMojoAppUrl, app_info));
}

void ShellContentUtilityClient::ExposeInterfacesToBrowser(
    shell::InterfaceRegistry* registry) {
// blpwtk2: Remove test-only code
#if 0
  registry->AddInterface(base::Bind(&TestMojoServiceImpl::Create));
#endif
}

bool ShellContentUtilityClient::OnMessageReceived(const IPC::Message& message) {
  return false;
}


}  // namespace content
