#pragma once

#include "resource.h"
#include <resource.h>
#include <WeaselIPC.h>
#include <WeaselUI.h>
#include <RimeWithWeasel.h>
#include <WeaselUtility.h>
#include <filesystem>
#include <functional>
#include <memory>
#include <winsparkle.h>

#include "WeaselTrayIcon.h"

namespace fs = std::filesystem;

class WeaselServerApp {
 public:
  static bool execute(const fs::path& cmd, const std::wstring& args) {
    return (uintptr_t)ShellExecuteW(NULL, NULL, cmd.c_str(), args.c_str(), NULL,
                                    SW_SHOWNORMAL) > 32;
  }

  static bool explore(const fs::path& path) {
    std::wstring quoted_path(L"\"" + path.wstring() + L"\"");
    return (uintptr_t)ShellExecuteW(NULL, L"explore", quoted_path.c_str(), NULL,
                                    NULL, SW_SHOWNORMAL) > 32;
  }

  static bool open(const fs::path& path) {
    return (uintptr_t)ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL,
                                    SW_SHOWNORMAL) > 32;
  }

  static bool check_update() {
    MessageBoxW(NULL, L"此 HD2 专用测试版不使用官方小狼毫更新通道。",
                L"小狼毫 HD2 Unicode", MB_ICONINFORMATION | MB_OK);
    return true;
  }

  static fs::path install_dir() {
    WCHAR exe_path[MAX_PATH] = {0};
    GetModuleFileNameW(GetModuleHandle(NULL), exe_path, _countof(exe_path));
    return fs::path(exe_path).remove_filename();
  }

 public:
  WeaselServerApp();
  ~WeaselServerApp();
  int Run();

 protected:
  void SetupMenuHandlers();

  weasel::Server m_server;
  weasel::UI m_ui;
  WeaselTrayIcon tray_icon;
  std::unique_ptr<RimeWithWeaselHandler> m_handler;
};
