// Part of xbox-iso-vfs

#include "vfs_operations.h"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

#include "vfs.h"

namespace vfs {
// TODO implement something relevant?
// https://www.lifewire.com/volume-serial-number-2626046
constexpr static const DWORD sc_volumeSerialNumber = 0x11115555;

namespace utils {
static vfs::Container *getContext(PDOKAN_FILE_INFO dokanfileinfo) {
  return reinterpret_cast<vfs::Container *>(
      dokanfileinfo->DokanOptions->GlobalContext);
}

void LlongToDwLowHigh(const LONGLONG &v, DWORD &low, DWORD &hight) {
  hight = v >> 32;
  low = static_cast<DWORD>(v);
}

void LlongToFileTime(LONGLONG v, FILETIME &filetime) {
  LlongToDwLowHigh(v, filetime.dwLowDateTime, filetime.dwHighDateTime);
}

LONGLONG DDwLowHighToLlong(const DWORD &low, const DWORD &high) {
  return static_cast<LONGLONG>(high) << 32 | low;
}
} // namespace utils

static NTSTATUS DOKAN_CALLBACK vfs_createfile(
    LPCWSTR filename, PDOKAN_IO_SECURITY_CONTEXT, ACCESS_MASK desiredaccess,
    ULONG fileattributes, ULONG, ULONG createdisposition, ULONG createoptions,
    PDOKAN_FILE_INFO dokanfileinfo) {
  auto vfsContext = utils::getContext(dokanfileinfo);

  ACCESS_MASK generic_desiredaccess;
  DWORD creation_disposition;
  DWORD file_attributes_and_flags;

  DokanMapKernelToUserCreateFileFlags(
      desiredaccess, fileattributes, createoptions, createdisposition,
      &generic_desiredaccess, &file_attributes_and_flags,
      &creation_disposition);

  auto e = vfsContext->getEntry(filename);

  if (e && e->isDirectory()) {

    // https://docs.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntcreatefile
    if (createoptions & FILE_NON_DIRECTORY_FILE) {
      return STATUS_FILE_IS_A_DIRECTORY;
    }

    dokanfileinfo->IsDirectory = true;
  } else {
    dokanfileinfo->IsDirectory = false;
  }

  if (e && desiredaccess & FILE_WRITE_DATA) {
    return STATUS_ACCESS_DENIED;
  }

  if (dokanfileinfo->IsDirectory) {
    if (creation_disposition == CREATE_NEW ||
        creation_disposition == OPEN_ALWAYS) {
      return STATUS_NOT_IMPLEMENTED;
    }
  } else {
    switch (creation_disposition) {
    case OPEN_ALWAYS: {
      if (!e) {
        return STATUS_NOT_IMPLEMENTED;
      }
    } break;
    case OPEN_EXISTING: {
      if (!e) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
      }
    } break;
    case CREATE_ALWAYS:
    case CREATE_NEW:
    case TRUNCATE_EXISTING: {
      return STATUS_ACCESS_DENIED;
    } break;
    }
  }

  /*
   * CREATE_NEW && OPEN_ALWAYS
   * If the specified file exists, the function fails and the last-error code is
   * set to ERROR_FILE_EXISTS
   */
  if (e && (creation_disposition == CREATE_NEW ||
            creation_disposition == OPEN_ALWAYS)) {
    return STATUS_OBJECT_NAME_COLLISION;
  }

  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK vfs_readfile(LPCWSTR filename, LPVOID buffer,
                                            DWORD bufferlength,
                                            LPDWORD readlength, LONGLONG offset,
                                            PDOKAN_FILE_INFO dokanfileinfo) {
  auto vfsContext = utils::getContext(dokanfileinfo);

  auto e = vfsContext->getEntry(filename);
  if (!e) {
    return STATUS_OBJECT_NAME_NOT_FOUND;
  }

  if (!e->isDirectory()) {
    auto stream = vfsContext->getFileStream();
    *readlength = e->read(*stream, buffer, bufferlength, offset);
  } else {
    *readlength = 0;
  }

  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK
vfs_getfileInformation(LPCWSTR filename, LPBY_HANDLE_FILE_INFORMATION buffer,
                       PDOKAN_FILE_INFO dokanfileinfo) {
  auto vfsContext = utils::getContext(dokanfileinfo);
  auto e = vfsContext->getEntry(filename);
  if (!e) {
    return STATUS_OBJECT_NAME_NOT_FOUND;
  }

  DWORD attribs = FILE_ATTRIBUTE_READONLY;

  if (e->isDirectory()) {
    attribs |= FILE_ATTRIBUTE_DIRECTORY;
  }

  buffer->dwFileAttributes = attribs;
  utils::LlongToFileTime(vfsContext->getVolumeModified(),
                         buffer->ftCreationTime);
  utils::LlongToFileTime(vfsContext->getVolumeModified(),
                         buffer->ftLastAccessTime);
  utils::LlongToFileTime(vfsContext->getVolumeModified(),
                         buffer->ftLastWriteTime);

  utils::LlongToDwLowHigh(e->getFileSize(), buffer->nFileSizeLow,
                          buffer->nFileSizeHigh);
  utils::LlongToDwLowHigh(0, buffer->nFileIndexLow, buffer->nFileIndexHigh);

  buffer->nNumberOfLinks = 1;
  buffer->dwVolumeSerialNumber = sc_volumeSerialNumber;

  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK vfs_findfiles(LPCWSTR filename,
                                             PFillFindData fill_finddata,
                                             PDOKAN_FILE_INFO dokanfileinfo) {
  auto vfsContext = utils::getContext(dokanfileinfo);

  auto folderContents = vfsContext->getFolderList(filename);

  WIN32_FIND_DATAW findData;
  ZeroMemory(&findData, sizeof(WIN32_FIND_DATAW));

  for (const auto &f : folderContents) {
    auto e = vfsContext->getEntry(f);

    auto name = e->getFilename();
    auto name_str = std::wstring(name.begin(), name.end());

    if (name_str.length() > MAX_PATH) {
      continue;
    }

    std::copy(name_str.begin(), name_str.end(), std::begin(findData.cFileName));
    findData.cFileName[name_str.length()] = L'\0';

    DWORD attribs = FILE_ATTRIBUTE_READONLY;
    if (e->isDirectory()) {
      attribs |= FILE_ATTRIBUTE_DIRECTORY;
    }
    findData.dwFileAttributes = attribs;

    utils::LlongToFileTime(vfsContext->getVolumeModified(),
                           findData.ftCreationTime);
    utils::LlongToFileTime(vfsContext->getVolumeModified(),
                           findData.ftLastAccessTime);
    utils::LlongToFileTime(vfsContext->getVolumeModified(),
                           findData.ftLastWriteTime);
    utils::LlongToDwLowHigh(e->getFileSize(), findData.nFileSizeLow,
                            findData.nFileSizeHigh);

    fill_finddata(&findData, dokanfileinfo);
  }

  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK vfs_getdiskfreespace(
    PULONGLONG free_bytes_available, PULONGLONG total_number_of_bytes,
    PULONGLONG total_number_of_free_bytes, PDOKAN_FILE_INFO dokanfileinfo) {
  auto vfsContext = utils::getContext(dokanfileinfo);

  *free_bytes_available = 0;
  *total_number_of_bytes = vfsContext->getVolumeSize();
  *total_number_of_free_bytes = 0;
  return STATUS_SUCCESS;
}

static NTSTATUS DOKAN_CALLBACK vfs_getvolumeinformation(
    LPWSTR volumename_buffer, DWORD volumename_size,
    LPDWORD volume_serialnumber, LPDWORD maximum_component_length,
    LPDWORD filesystem_flags, LPWSTR filesystem_name_buffer,
    DWORD filesystem_name_size, PDOKAN_FILE_INFO dokanfileinfo) {
  auto vfsContext = utils::getContext(dokanfileinfo);

  wcscpy_s(volumename_buffer, volumename_size,
           vfsContext->getFilename().c_str());

  *volume_serialnumber = sc_volumeSerialNumber;
  *maximum_component_length = 255;
  *filesystem_flags = FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES |
                      FILE_UNICODE_ON_DISK | FILE_READ_ONLY_VOLUME;

  wcscpy_s(filesystem_name_buffer, filesystem_name_size, L"Dokan XISO");

  return STATUS_SUCCESS;
}

void setup(DOKAN_OPERATIONS &dokanOperations) {
  // Implements only a subset of operations

  dokanOperations.ZwCreateFile = vfs_createfile;
  dokanOperations.ReadFile = vfs_readfile;
  dokanOperations.GetFileInformation = vfs_getfileInformation;
  dokanOperations.FindFiles = vfs_findfiles;
  dokanOperations.GetDiskFreeSpace = vfs_getdiskfreespace;
  dokanOperations.GetVolumeInformation = vfs_getvolumeinformation;
}
} // namespace vfs
