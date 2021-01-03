// Part of xbox-iso-vfs

#pragma once

#include "xdvdfs.h"

#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace vfs {
enum class SetupState {
  ErrorFile,
  ErrorFormat,
  Success,
};

class Container {
public:
  using EntryHandle = size_t;

  SetupState setup(const std::wstring &filename);

  const xdvdfs::FileEntry *getEntry(const std::filesystem::path &path) const;
  const xdvdfs::FileEntry *getEntry(EntryHandle handle) const;

  using FileResults = std::vector<EntryHandle>;
  FileResults getFolderList(const std::filesystem::path &path) const;

  uint64_t getVolumeModified() const { return m_volumeModified; }
  uint64_t getVolumeSize() const { return m_volumeSize; }

  xdvdfs::Stream *getFileStream() const { return m_stream.get(); }

  const std::wstring &getFilename() const { return m_name; }

protected:
  void build(xdvdfs::Stream &file, xdvdfs::FileEntry &dirent);
  void buildFromTreeRecursive(xdvdfs::Stream &file, xdvdfs::FileEntry &dirent,
                              EntryHandle parent);

  EntryHandle registerFileEntry(const xdvdfs::FileEntry &dirent,
                                EntryHandle parent);

  std::string makeEntryKey(const std::filesystem::path &path) const;
  std::string resolveEntryKey(EntryHandle handle) const;

  EntryHandle getHandle(const std::filesystem::path &path) const;

private:
  constexpr static EntryHandle sc_invalidHandle = ~0U;

  std::vector<xdvdfs::FileEntry> m_entries; // flat entries
  std::vector<EntryHandle> m_parentHandles; // flag lookup
  std::map<std::string, EntryHandle> m_entryMap;

  std::wstring m_name;

  uint64_t m_volumeModified{0};
  uint64_t m_volumeSize{0};

  std::unique_ptr<xdvdfs::Stream> m_stream;
};
} // namespace vfs
