// Part of xbox-iso-vfs

#pragma once

#include <fstream>
#include <limits>
#include <mutex>

namespace xdvdfs {
class Stream {
public:
  Stream() = default;

  std::ifstream m_file;
  std::streampos m_offset;

  std::mutex m_fileMutex;
};

constexpr static const int SECTOR_SIZE = 2048;
constexpr static const int VOLUME_DESCRIPTOR_SECTOR = 32;
constexpr static const uint8_t MAGIC_ID[] = "MICROSOFT*XBOX*MEDIA";

class FileEntry {
public:
  FileEntry() = default;
  FileEntry(const FileEntry &other);
  FileEntry(const std::string &name);

  void readFromFile(Stream &file, std::streampos pos, std::streamoff offset);
  const std::string &getFilename() const;
  uint32_t getFileSize() const;

  // matching dokany api for now
  uint32_t read(Stream &file, void *buffer, uint32_t bufferlength,
                int64_t offset) const;

  uint8_t getAttributes() const { return m_attributes; }
  bool isDirectory() const;
  bool hasLeftChild() const;
  bool hasRightChild() const;

  FileEntry getLeftChild(Stream &file);
  FileEntry getRightChild(Stream &file);
  FileEntry getFirstEntry(Stream &file);

  static const uint8_t FILE_READONLY = 0x01;
  static const uint8_t FILE_HIDDEN = 0x02;
  static const uint8_t FILE_SYSTEM = 0x04;
  static const uint8_t FILE_DIRECTORY = 0x10;
  static const uint8_t FILE_ARCHIVE = 0x20;
  static const uint8_t FILE_NORMAL = 0x80;

private:
  uint16_t m_leftSubTree{0};
  uint16_t m_rightSubTree{0};
  uint32_t m_startSector{0};
  uint32_t m_fileSize{0};
  uint8_t m_attributes{0};
  std::string m_filename;

  std::streampos m_sectorNumber;
};

class VolumeDescriptor {
public:
  void readFromFile(Stream &file);

  bool validate();
  FileEntry getRootDirEntry(Stream &file);
  uint64_t getCreationTime() const { return m_filetime; }

protected:
  uint8_t m_id1[0x14];           ///< 20 byte block containing the magic
  uint32_t m_rootDirTableSector; ///< sector number of the root directory table
  uint32_t m_rootDirTableSize;   ///< size of the root directory table in bytes
  uint64_t m_filetime; ///< FILETIME-structure containing the creation time
  uint8_t m_id2[0x14]; ///< 20 byte block containig the same magic
};

} // namespace xdvdfs
