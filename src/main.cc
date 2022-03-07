// Part of xbox-iso-vfs

#include <dokan/dokan.h>
#include <dokan/fileinfo.h>

#include "vfs.h"
#include "vfs_operations.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

template <class T> class Singleton {
public:
  Singleton() { ms_singleton = static_cast<T *>(this); }

  virtual ~Singleton() {
    if (ms_singleton == this) {
      ms_singleton = nullptr;
    }
  }

protected:
  static T *ms_singleton;
};

template <class T> T *Singleton<T>::ms_singleton{nullptr};

class App : public Singleton<App> {
public:
  struct Parameters {
    std::wstring filePath;
    std::wstring mountPoint;
    bool debugMode{false};
    bool launchMountPath{false};
  };

  App(const Parameters &params) : m_params(params) {}

  void run() {
    auto status = m_vfsContainer.setup(m_params.filePath);
    switch (status) {
    case vfs::SetupState::ErrorFile:
      std::wcout << "Failed to open file " << m_params.filePath << "\n";
      std::wcout << "File must exist\n";
      break;
    case vfs::SetupState::ErrorFormat:
      std::wcout << "Failed to read file " << m_params.filePath
                 << " as an Xbox ISO image\n";
      std::wcout << "Please create an issue on Github and give details about "
                    "this ISO\n";
      break;
    case vfs::SetupState::Success:
      runDokan();
      break;
    }
  }

  static void fileWatcher(std::wstring path) {

    std::filesystem::path mp(path);
    std::error_code errorCode;

    do {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } while (std::filesystem::exists(mp, errorCode) == false);

    ShellExecuteW(0, L"explore", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
  }

  void runDokan() {

    DokanInit();

    DOKAN_OPTIONS dokanOptions;
    ZeroMemory(&dokanOptions, sizeof(DOKAN_OPTIONS));

    dokanOptions.Version = DOKAN_VERSION;
    dokanOptions.SingleThread = FALSE;
    dokanOptions.Timeout = 0;
    dokanOptions.GlobalContext = reinterpret_cast<ULONG64>(&m_vfsContainer);
    dokanOptions.MountPoint = m_params.mountPoint.c_str();
    dokanOptions.Options |= DOKAN_OPTION_ALT_STREAM;
    dokanOptions.Options |= DOKAN_OPTION_WRITE_PROTECT;
    dokanOptions.Options |= DOKAN_OPTION_CURRENT_SESSION;

    if (m_params.debugMode) {
      dokanOptions.Options |= DOKAN_OPTION_STDERR | DOKAN_OPTION_DEBUG;
    }

    SetConsoleCtrlHandler(CtrlHandler, TRUE);

    DOKAN_OPERATIONS dokanOperations;
    ZeroMemory(&dokanOperations, sizeof(DOKAN_OPERATIONS));

    vfs::setup(dokanOperations);

    std::thread watcher;
    if (m_params.launchMountPath) {
      watcher = std::thread(fileWatcher, m_params.mountPoint);
    }

    auto status = DokanMain(&dokanOptions, &dokanOperations);

    // Solutions are suggested where possible
    switch (status) {
    case DOKAN_SUCCESS:
      break;
    case DOKAN_ERROR:
      std::wcout << "An unknown error occured with Dokan.\n";
      std::wcout << "Please create an issue on Github with any steps to "
                    "reproduce this\n";
      break;
    case DOKAN_DRIVE_LETTER_ERROR:
      std::wcout << "Dokan cannot use this drive letter.\n";
      std::wcout << "Use a different mount path on an unused drive\n";
      break;
    case DOKAN_DRIVER_INSTALL_ERROR:
      std::wcout << "Failed to communicate with Dokan driver.\n";
      std::wcout << "Check Dokan is installedand the service is running\n";
      break;
    case DOKAN_START_ERROR:
      std::wcout << "Dokan failed to startup.\n";
      std::wcout
          << "Try checking the service is running or try restarting your PC\n";
      break;
    case DOKAN_MOUNT_ERROR:
      std::wcout << "Dokan failed to mount to " << m_params.mountPoint << ".\n";
      std::wcout << "Try using a different mount path\n";
      break;
    case DOKAN_MOUNT_POINT_ERROR:
      std::wcout << "Dokan encountered an error using this mount point.\n";
      std::wcout << "Try using a different mount path\n";
      break;
    case DOKAN_VERSION_ERROR:
      std::wcout << "The installed version of Dokan (" << DokanVersion()
                 << ") is not compatiable with this app (" << DOKAN_VERSION
                 << ").\n";
      std::wcout << "Please install the matching Dokan driver version\n";
      break;
    default:
      std::wcout << "An unknown error occured (" << status << ").\n";
      std::wcout << "Please create an issue on Github with this number\n";
      break;
    }

    DokanShutdown();

    if (watcher.joinable()) {
      watcher.detach();
    }
  }

  static void showUsage() {
    std::wcout
        << "xbox-iso-vfs is a utility to mount Xbox ISO files on Windows\n";
    std::wcout << "Written by x1nixmzeng\n\n";
    std::wcout << "xbox-iso-vfs.exe [/d|/l] <iso_file> <mount_path>\n";
    std::wcout
        << "  /d           Display debug Dokan output in console window\n";
    std::wcout << "  /l           Open Windows Explorer to the mount path\n";
    std::wcout << "  <iso_file>   Path to the Xbox ISO file to mount\n";
    std::wcout << "  <mount_path> Driver letter (\"M:\\\") or folder path on "
                  "NTFS partition\n";
    std::wcout << "  /h           Show usage\n\n";
    std::wcout << "Unmount with CTRL + C in the console or alternatively via "
                  "\"dokanctl /u mount_path\".\n\n";
  }

  static bool readParameters(App::Parameters &params, int argc,
                             wchar_t **argv) {
    for (int i = 1; i < argc; ++i) {
      auto arg = std::wstring(argv[i]);

      if (arg == L"--help" || arg == L"/h") {
        showUsage();
        return false;
      } else if (arg == L"--debug" || arg == L"/d") {
        params.debugMode = true;
        continue;
      } else if (arg == L"--launch" || arg == L"/l") {
        params.launchMountPath = true;
        continue;
      } else if (i + 1 >= argc) {
        std::wcout << "Missing mount_path parameter. Use --help to see usage\n";
        return false;
      }

      params.filePath = arg;
      params.mountPoint = argv[i + 1];
      break;
    }

    if (params.filePath.empty()) {
      std::wcout << "Missing iso_file parameter. Use --help to see usage\n";
      return false;
    }

    std::error_code errorCode;
    if (std::filesystem::exists(params.filePath, errorCode) == false) {
      std::wcout << "The file " << params.filePath << " does not exist\n";
      std::wcout
          << "This parameter should be a path to an existing Xbox ISO file\n";
      return false;
    }

    if (params.mountPoint.empty()) {
      std::wcout << "Missing mount_path parameter. Use --help to see usage\n";
      return false;
    }

    if (std::filesystem::exists(params.mountPoint, errorCode)) {
      std::wcout << "The mount path or driver letter " << params.mountPoint
                 << " already exists\n";
      std::wcout << "This parameter should be a target path or drive used for "
                    "mounting, and should not exist\n";
      return false;
    }

    return true;
  }

private:
  static BOOL WINAPI CtrlHandler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT: {
      SetConsoleCtrlHandler(CtrlHandler, FALSE);
      DokanRemoveMountPoint(ms_singleton->m_params.mountPoint.c_str());

      return TRUE;
    }
    default:
      return FALSE;
    }
  }

  vfs::Container m_vfsContainer;
  Parameters m_params;
};

int wmain(int argc, wchar_t **argv) {
  App::Parameters params;

  if (App::readParameters(params, argc, argv)) {
    auto app = std::make_unique<App>(params);
    app->run();
  }

  return 0;
}
