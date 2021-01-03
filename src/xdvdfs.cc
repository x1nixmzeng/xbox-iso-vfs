// Part of xbox-iso-vfs

#include "xdvdfs.h"

#include <cstring>
#include <iostream>
#include <vector>

namespace xdvdfs {
bool VolumeDescriptor::validate() {
  if (std::memcmp(m_id1, MAGIC_ID, std::size(m_id1)) != 0) {
    return false;
  }

  if (m_rootDirTableSector == 0 || m_rootDirTableSize == 0) {
    return false;
  }

  if (std::memcmp(m_id1, MAGIC_ID, std::size(m_id2)) != 0) {
    return false;
  }

  return true;
}

FileEntry::FileEntry(const FileEntry &other)
    : m_leftSubTree(other.m_leftSubTree), m_rightSubTree(other.m_rightSubTree),
      m_startSector(other.m_startSector), m_fileSize(other.m_fileSize),
      m_attributes(other.m_attributes), m_filename(other.m_filename),
      m_sectorNumber(other.m_sectorNumber) {}

FileEntry::FileEntry(const std::string &name)
    : m_leftSubTree(0), m_rightSubTree(0), m_startSector(0), m_fileSize(0),
      m_attributes(FileEntry::FILE_DIRECTORY), m_filename(name),
      m_sectorNumber(0) {}

FileEntry VolumeDescriptor::getRootDirEntry(Stream &file) {
  FileEntry dirent;

  dirent.readFromFile(file, m_rootDirTableSector, 0);

  return dirent;
}

void VolumeDescriptor::readFromFile(Stream &file) {
  std::vector<char> buffer(SECTOR_SIZE);

  {
    std::lock_guard<std::mutex> lock(file.m_fileMutex);

    file.m_file.seekg(VOLUME_DESCRIPTOR_SECTOR * SECTOR_SIZE + file.m_offset,
                      std::ifstream::beg);
    file.m_file.read(buffer.data(), buffer.size());
  }

  std::copy(buffer.begin(), buffer.begin() + 0x14, m_id1);
  std::copy(buffer.begin() + 0x14, buffer.begin() + 0x18,
            reinterpret_cast<char *>(&m_rootDirTableSector));
  std::copy(buffer.begin() + 0x18, buffer.begin() + 0x1C,
            reinterpret_cast<char *>(&m_rootDirTableSize));
  std::copy(buffer.begin() + 0x1C, buffer.begin() + 0x24,
            reinterpret_cast<char *>(&m_filetime));
  std::copy(buffer.begin() + 0x7EC, buffer.end(), m_id2);
}
} // namespace xdvdfs

namespace xdvdfs {
void FileEntry::readFromFile(Stream &file, std::streampos sector,
                             std::streamoff offset) {
  std::vector<char> buffer(SECTOR_SIZE);

  {
    std::lock_guard<std::mutex> lock(file.m_fileMutex);

    file.m_file.seekg((sector * SECTOR_SIZE) + file.m_offset + offset,
                      std::ifstream::beg);
    file.m_file.read(buffer.data(), buffer.size());
  }

  std::copy(buffer.begin(), buffer.begin() + 0x02,
            reinterpret_cast<char *>(&m_leftSubTree));
  std::copy(buffer.begin() + 0x02, buffer.begin() + 0x04,
            reinterpret_cast<char *>(&m_rightSubTree));
  std::copy(buffer.begin() + 0x04, buffer.begin() + 0x08,
            reinterpret_cast<char *>(&m_startSector));
  std::copy(buffer.begin() + 0x08, buffer.begin() + 0x0C,
            reinterpret_cast<char *>(&m_fileSize));
  std::copy(buffer.begin() + 0x0C, buffer.begin() + 0x0D,
            reinterpret_cast<char *>(&m_attributes));

  size_t filenameLength = buffer[0x0D];
  m_filename = std::string(&buffer[0x0E], filenameLength);

  m_sectorNumber = sector;
}

const std::string &FileEntry::getFilename() const { return m_filename; }

uint32_t FileEntry::getFileSize() const { return m_fileSize; }

uint32_t FileEntry::read(Stream &file, void *buffer, uint32_t bufferlength,
                         int64_t offset) const {
  if (offset < 0) {
    return 0;
  }

  auto localOffset = static_cast<uint32_t>(offset);
  if (bufferlength && localOffset < getFileSize()) {
    auto readLength = bufferlength;

    if (localOffset + bufferlength >= getFileSize()) {
      readLength = getFileSize() - localOffset;
    }

    auto baseOffset = SECTOR_SIZE * static_cast<int64_t>(m_startSector) +
                      file.m_offset + localOffset;

    {
      std::lock_guard<std::mutex> lock(file.m_fileMutex);

      file.m_file.seekg(baseOffset, std::ifstream::beg);
      file.m_file.read(static_cast<char *>(buffer), readLength);

      return readLength;
    }
  }

  return 0;
}

bool FileEntry::isDirectory() const {
  return ((m_attributes & FileEntry::FILE_DIRECTORY) != 0);
}

bool FileEntry::hasLeftChild() const { return (m_leftSubTree != 0); }

bool FileEntry::hasRightChild() const { return (m_rightSubTree != 0); }

FileEntry FileEntry::getLeftChild(Stream &file) {
  FileEntry dirent;
  dirent.readFromFile(file, m_sectorNumber,
                      static_cast<std::streampos>(m_leftSubTree) * 4);

  return dirent;
}

FileEntry FileEntry::getRightChild(Stream &file) {
  FileEntry dirent;
  dirent.readFromFile(file, m_sectorNumber,
                      static_cast<std::streampos>(m_rightSubTree) * 4);

  return dirent;
}

FileEntry FileEntry::getFirstEntry(Stream &file) {
  FileEntry dirent;
  if (isDirectory()) {
    dirent.readFromFile(file, m_startSector, 0);
  }

  return dirent;
}
} // namespace xdvdfs
