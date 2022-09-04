#pragma once

#include "compressor_base.hpp"
#include <array>
#include <boost/none.hpp>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <istream>
#include <memory>
#include <mutex>
#include <boost/optional.hpp>
#include <boost/optional/optional.hpp>
#include <type_traits>

// also ... only little endian

// NB!: const operations are *NOT* thread safe!
class ArchiveParser
{
public:
    enum class fileType : uint8_t
    {
        file,
        folder
    };

    struct CompressionStrategy
    {
        enum class Algorithm : std::uint8_t
        {
            none = 0,
            LZW
        };
        Algorithm m_alg;
        std::uint8_t m_algOptions;
        CompressionStrategy() : CompressionStrategy("NONE", 0) { }
        CompressionStrategy(const char *alg, unsigned options);
        CompressionStrategy(std::uint8_t alg, unsigned options);
        std::unique_ptr<Compressor> getCompressor(std::ostream &out) const;
        std::unique_ptr<Decompressor> getDecompressor(std::ostream &out) const;
        std::uint8_t getAlgVal() const
        {
            return static_cast<std::uint8_t>(m_alg);
        }
        std::uint8_t getAlgOptionsVal() const
        {
            return static_cast<std::uint8_t>(m_algOptions);
        }
        const char* getAlgStr() const;
    };

private:

    // typedefs
    using FileOffsetType = std::uint64_t;

    // static variables
    static constexpr unsigned M_MAGIC_SIZE = 8;
    static constexpr std::array<char, M_MAGIC_SIZE> M_FORMAT_MAGIC = 
                    { 'P', 'a', 'c', 'o', 'Z', 'I', 'P', 'P'};
    // structs
    struct archiveHeader
    {
        uint16_t header_version;
        uint16_t _reserved;
        FileOffsetType first_file_pos;
        static constexpr unsigned FIRST_FILE_FIELD_POS = M_MAGIC_SIZE + sizeof(header_version);
    };

    struct FileHeader
    {
        FileOffsetType cur_file_pos;
        FileOffsetType file_size;
        FileOffsetType next_file_pos; // zero if there is not a next file
        std::uint32_t checksum; //CRC32
        std::uint16_t name_size;
        fileType file_type;
        std::uint8_t compression_alg;
        std::uint8_t compression_alg_args;
        static constexpr unsigned HEADER_SIZE = sizeof(file_size) + 
            sizeof(next_file_pos) + sizeof(checksum) + sizeof(name_size) +
            sizeof(file_type) + sizeof(compression_alg) + sizeof(compression_alg_args);
    };

    // member variables
    std::fstream m_archiveStrg;
    std::reference_wrapper<std::iostream> m_archive;
    archiveHeader m_archiveHeader;
    FileOffsetType m_lastFilePos = 0;
    CompressionStrategy m_defaultCompStr;
    bool m_lastFilePosValid = false;

    // private member functions
    // all of these expect global_lock to be held
    void readArchiveHeader();
    void writeArchiveHeader();

    FileHeader readFileHeader(FileOffsetType file_pos) const;
    void writeFileHeader(const FileHeader &fih);

    FileOffsetType getLastFilePos() const;
    void updateLastFilePos(FileOffsetType new_last);
    void calcCrcFileEntry(FileHeader &header, const char *name, std::istream &file, std::size_t file_size);
    void calcCrcFolderEntry(FileHeader &header, const char *name);
    bool verifyCrcFileEntry(const FileHeader &header) const;

    static FileOffsetType calculateFileEntrySize(std::size_t name_size, std::size_t file_size);
    FileOffsetType allocateFileEntrySpace(std::size_t file_entry_size) const;
    void writeFileEntry (const FileHeader &header, const char *name, std::istream &file, std::size_t file_size);
    void writeFolderEntry (const FileHeader &header, const char *name);

    std::string readFileName (const FileHeader &header) const;
    void readAndDecompressFileContents (const FileHeader &header, std::ostream &out) const;

    bool checkArchiveConsistency() const;

public:

    class FileIterator;

    class FileInfo
    {
    private:
        const ArchiveParser *m_archive;
        FileHeader m_fileHeader;

        FileInfo() : m_archive(nullptr), m_fileHeader() {} // zero is invalid position

        FileInfo(const ArchiveParser &archive, FileOffsetType filePos) 
            : m_archive(&archive), m_fileHeader(m_archive->readFileHeader(filePos))
        { }

    public:

        std::string getFileName() const
        {
            assert(m_archive != nullptr);
            return m_archive->readFileName(m_fileHeader);
        }

        void readFile(std::ostream &out) const
        {
            assert(m_archive != nullptr);
            assert(m_fileHeader.file_type != fileType::folder);
            m_archive->readAndDecompressFileContents(m_fileHeader, out);
        }

        fileType getFileType() const
        {
            assert(m_archive != nullptr);
            return m_fileHeader.file_type;
        }

        static_assert(std::is_same<std::uint64_t, FileOffsetType>::value, "FileOffsetType is not uint64");
        std::uint64_t getCompressedFileSize() const
        {
            assert(m_archive != nullptr);
            return m_fileHeader.file_size;
        }

        bool verify() const
        {
            assert(m_archive != nullptr);
            return m_archive->verifyCrcFileEntry(m_fileHeader);
        }

        std::pair<FileOffsetType, FileOffsetType> getEntryBeginEnd() const
        {
            assert(m_archive != nullptr);
            FileOffsetType beg = m_fileHeader.cur_file_pos;
            FileOffsetType end = beg + calculateFileEntrySize(m_fileHeader.name_size, m_fileHeader.file_size);
            return std::make_pair(beg, end);
        }

        CompressionStrategy getCompressionStrg() const
        {
            assert(m_archive != nullptr);
            return CompressionStrategy(m_fileHeader.compression_alg, m_fileHeader.compression_alg_args);
        }
        
        friend class FileIterator;
    };

    class FileIterator
    {
    private:
        const ArchiveParser &m_archive;
        FileOffsetType m_filePos;
        FileInfo m_fileInfo;

        FileIterator(const ArchiveParser &archive, FileOffsetType filePos)
            : m_archive(archive), m_filePos(filePos)
        { }

    public:
        
        bool operator== (const FileIterator &other) const
        {
            assert(&m_archive == &other.m_archive);
            return m_filePos == other.m_filePos;
        }

        bool operator!= (const FileIterator &other) const
        {
            return !(*this == other);
        }

        FileIterator& operator++();

        FileIterator operator++(int) &
        {
            FileIterator res = *this;
            ++(*this);
            return res;
        }

        FileInfo& operator*();

        FileInfo* operator->()
        {
            return &(this->operator*());
        }

        friend class ArchiveParser;
    };

    using value_type = FileInfo;
    using reference = FileInfo&;
    using pointer = FileInfo*;
    using const_iterator = FileIterator;
    using iterator = const_iterator;

    ArchiveParser() = delete;

    explicit ArchiveParser(const char *archivePath);
    explicit ArchiveParser(std::iostream &archive);

    static ArchiveParser MakeArchive(const char *archivePath);
    static ArchiveParser MakeArchive(std::iostream &archive);

    ArchiveParser(ArchiveParser &&other) noexcept
        : m_archiveStrg(std::move(other.m_archiveStrg)),
          m_archive(other.m_archive), 
          m_archiveHeader(other.m_archiveHeader),
          m_lastFilePos(other.m_lastFilePos),
          m_lastFilePosValid(other.m_lastFilePosValid)
    {
    }
    ArchiveParser(const ArchiveParser &) = delete;
    
    void swap(ArchiveParser &other)
    {
        using std::swap;
        swap(m_archiveStrg, other.m_archiveStrg);
        swap(m_archive, other.m_archive);
        swap(m_archiveHeader, other.m_archiveHeader);
        swap(m_lastFilePos, other.m_lastFilePos);
        swap(m_lastFilePosValid, other.m_lastFilePosValid);
    }

    ArchiveParser &operator=(ArchiveParser &&other) noexcept
    {
        swap(other);
        return *this;
    }
    ArchiveParser &operator=(const ArchiveParser &) = delete; 
    ~ArchiveParser() = default; // TODO: flush file header

    const_iterator cbefore_begin() const;
    const_iterator cbegin() const;
    const_iterator cend() const;

    const_iterator before_begin() const
    {
        return cbefore_begin();
    }
    const_iterator begin() const
    {
        return cbegin();
    }
    const_iterator end() const
    {
        return cend();
    }

    iterator before_begin() // NOLINT
    {
        return cbefore_begin();
    }
    iterator begin() // NOLINT
    {
        return cbegin();
    }
    iterator end() // NOLINT
    {
        return cend();
    }

    const_iterator findFile(const char *name) const;
    void deleteAfter(const_iterator pos);

    void setDefaultCompressionStrategy(const CompressionStrategy &comp)
    {
        m_defaultCompStr = comp;
    }

    CompressionStrategy getDefaultCompressionStrategy()
    {
        return m_defaultCompStr;
    }

    void addFile(const char *name, std::istream &file, const CompressionStrategy &comps, const char *tempFilePath="");
    void addFile(const char *name, std::istream &file, const CompressionStrategy &comps, std::iostream &temp_file);
    void addFile(const char *name, std::istream &file)
    {
        addFile(name, file, getDefaultCompressionStrategy());
    }
    void addFolder(const char *name);
    void readFile(const char *name, std::ostream &out) const;
    void deleteFile(const char *name);
    fileType getFileType(const char *name) const;

    bool verify() const;
};

