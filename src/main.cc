// Part of xbox-iso-vfs

#include <dokan/dokan.h>
#include <dokan/fileinfo.h>

#include "vfs.h"
#include "vfs_operations.h"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

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
    // bool launchMountPath{ true };
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

  void runDokan() {
    DOKAN_OPTIONS dokanOptions;
    ZeroMemory(&dokanOptions, sizeof(DOKAN_OPTIONS));

    dokanOptions.Version = DOKAN_VERSION;
    // Use default thread count
    dokanOptions.ThreadCount = 0;
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
  }

  static void showUsage() {
    std::wcout
        << "xbox-iso-vfs is a utility to mount Xbox ISO files on Windows\n";
    std::wcout << "Written by x1nixmzeng\n\n";
    std::wcout << "xbox-iso-vfs.exe [/d|/l] <iso_file> <mount_path>\n";
    std::wcout
        << "  /d           Display debug Dokan output in console window\n";
    // std::wcout << "  /l           Launch the mount path when successful\n";
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
      }
      // else if (arg == L"--launch" || arg == L"/l") {
      //	params.launchMountPath = true;
      //}
      else if (i + 1 >= argc) {
        showUsage();
      }

      params.filePath = arg;
      params.mountPoint = argv[i + 1];
      break;
    }

    if (params.filePath.empty()) {
      showUsage();
      return false;
    }

    if (std::filesystem::exists(params.filePath) == false) {
      std::wcout << "The file " << params.filePath << " does not exist\n";
      std::wcout
          << "This parameter should be a path to an existing Xbox ISO file\n";
      return false;
    }

    if (params.mountPoint.empty()) {
      showUsage();
      return false;
    }

    if (std::filesystem::exists(params.mountPoint)) {
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
