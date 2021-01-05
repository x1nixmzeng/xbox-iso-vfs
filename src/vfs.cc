// Part of xbox-iso-vfs

#include "vfs.h"

#include <algorithm>

namespace vfs {
SetupState Container::setup(const std::wstring &filename) {
  auto stream = std::make_unique<xdvdfs::Stream>();

  stream->m_file.open(filename.c_str(),
                      std::ifstream::binary | std::ifstream::in);
  if (!stream->m_file.is_open()) {
    return SetupState::ErrorFile;
  }

  xdvdfs::VolumeDescriptor vd;
  vd.readFromFile(*stream);

  if (!vd.validate()) {
    // Some "dual layer" ISO files will have both a video and game partition
    constexpr static size_t sc_gamePartitionOffset =
        xdvdfs::SECTOR_SIZE * 32 * 6192;

    stream->m_offset = sc_gamePartitionOffset;

    vd.readFromFile(*stream);

    if (!vd.validate()) {
      return SetupState::ErrorFormat;
    }
  }

  xdvdfs::FileEntry de = vd.getRootDirEntry(*stream);
  build(*stream, de);

  // Cache creation time on volume descriptor (shared between all sub files and
  // folders)
  m_volumeModified = vd.getCreationTime();

  // Cache file size of the input file
  stream->m_file.seekg(0, std::ifstream::end);
  m_volumeSize = static_cast<uint64_t>(stream->m_file.tellg());

  // Promote local variable
  std::swap(stream, m_stream);

  m_name = std::filesystem::path(filename).replace_extension("").filename();

  return SetupState::Success;
}

const xdvdfs::FileEntry *
Container::getEntry(const std::filesystem::path &path) const {
  auto handle = getHandle(path);
  if (handle != sc_invalidHandle) {
    return &m_entries[handle];
  }

  return nullptr;
}

const xdvdfs::FileEntry *Container::getEntry(EntryHandle handle) const {
  if (handle < m_entries.size()) {
    return &m_entries[handle];
  }

  return nullptr;
}

Container::FileResults
Container::getFolderList(const std::filesystem::path &path) const {
  FileResults results;

  auto handle = getHandle(path);
  for (size_t i = 0; i < m_parentHandles.size(); ++i) {
    if (m_parentHandles[i] == handle) {
      results.emplace_back(i);
    }
  }

  return results;
}

void Container::build(xdvdfs::Stream &file, xdvdfs::FileEntry &dirent) {
  xdvdfs::FileEntry root("\\");
  auto newHandle = registerFileEntry(root, sc_invalidHandle);

  buildFromTreeRecursive(file, dirent, newHandle);
}

void Container::buildFromTreeRecursive(xdvdfs::Stream &file,
                                       xdvdfs::FileEntry &dirent,
                                       EntryHandle parent) {
  auto newHandle = registerFileEntry(dirent, parent);

  if (dirent.isDirectory()) {
    xdvdfs::FileEntry entry = dirent.getFirstEntry(file);
    if (entry.validate()) {
      buildFromTreeRecursive(file, entry, newHandle);
    }
  }

  if (dirent.hasLeftChild()) {
    xdvdfs::FileEntry entry = dirent.getLeftChild(file);

    if (entry.validate()) {
      buildFromTreeRecursive(file, entry, parent);
    }
  }

  if (dirent.hasRightChild()) {
    xdvdfs::FileEntry entry = dirent.getRightChild(file);
    if (entry.validate()) {
      buildFromTreeRecursive(file, entry, parent);
    }
  }
}

Container::EntryHandle
Container::registerFileEntry(const xdvdfs::FileEntry &dirent,
                             EntryHandle parent) {
  m_entries.emplace_back(dirent);
  m_parentHandles.emplace_back(parent);

  auto newHandle = m_entries.size() - 1;

  auto entryKey = resolveEntryKey(newHandle);
  m_entryMap.emplace(entryKey, newHandle);

  return newHandle;
}

std::string Container::makeEntryKey(const std::filesystem::path &path) const {
  auto result = path.string();

  // Enforce case-insensitive name resolution
  std::transform(result.begin(), result.end(), result.begin(),
                 [](char c) { return static_cast<char>(::tolower(c)); });

  return result;
}

std::string Container::resolveEntryKey(EntryHandle handle) const {
  // Parent traversal; build a reverse list of any path handles
  std::vector<EntryHandle> handles;
  handles.emplace_back(handle);

  while (m_parentHandles[handle] != sc_invalidHandle) {
    auto parentIndex = m_parentHandles[handle];
    handles.emplace_back(parentIndex);

    handle = parentIndex;
  }

  // Switch the path ordering bottom -> top
  std::reverse(handles.begin(), handles.end());

  // Build path
  std::filesystem::path finalPath;

  for (auto &childHandle : handles) {
    finalPath /= m_entries[childHandle].getFilename();
  }

  return makeEntryKey(finalPath);
}

Container::EntryHandle
Container::getHandle(const std::filesystem::path &path) const {
  auto key = makeEntryKey(path);

  auto it = m_entryMap.find(key);
  if (it != m_entryMap.end()) {
    return it->second;
  }

  return sc_invalidHandle;
}
} // namespace vfs
