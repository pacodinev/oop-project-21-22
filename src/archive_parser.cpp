#include "archive_parser.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <array>
#include <ios>
#include <istream>
#include <memory>
#include <mutex>
#include <random>
#include <stdexcept>
#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <vector>

#include "LZW.hpp"
#include "compressor_base.hpp"
#include "crc32.hpp"
#include "noop_copressor.hpp"

ArchiveParser::CompressionStrategy::CompressionStrategy(const char *alg, unsigned options)
{
    if(std::strcmp(alg, "NONE")==0)
    {
        m_alg = Algorithm::none;
        m_algOptions = 0;
    }
    else if(std::strcmp(alg, "LZW")==0)
    {
        m_alg = Algorithm::LZW;
        if(options >= 10)
        {
            throw std::runtime_error("Invalid LZW options");
        }
        m_algOptions = static_cast<std::uint8_t>(options);
    }
    else
    {
        throw std::runtime_error("Unknown compression algorithm");
    }
}

ArchiveParser::CompressionStrategy::CompressionStrategy(std::uint8_t alg, unsigned options)
{
    if(alg == static_cast<std::uint8_t>(Algorithm::none))
    {
        m_alg = Algorithm::none;
        m_algOptions = 0;
    }
    else if(alg == static_cast<std::uint8_t>(Algorithm::LZW))
    {
        m_alg = Algorithm::LZW;
        if(options >= 10)
        {
            throw std::runtime_error("Invalid LZW options");
        }
        m_algOptions = static_cast<std::uint8_t>(options);
    }
    else
    {
        throw std::runtime_error("Unknown compression algorithm");
    }
}
const char* ArchiveParser::CompressionStrategy::getAlgStr() const
{
    switch (m_alg) {
    case Algorithm::none:
        return "NONE";
    case Algorithm::LZW:
        return "LZW";
    }
    return nullptr;
}

std::unique_ptr<Compressor> ArchiveParser::CompressionStrategy::getCompressor(std::ostream &out) const
{
    if(m_alg == Algorithm::none)
    {
        return std::make_unique<NoneCompressor>(out);
    }
    else if(m_alg == Algorithm::LZW)
    {
        unsigned dict_size;
        switch(m_algOptions)
        {
            case 0:
                dict_size = 9; break;
            case 1:
                dict_size = 10; break;
            case 2:
                dict_size = 11; break;
            case 3:
                dict_size = 13; break;
            case 4:
                dict_size = 14; break;
            case 5:
                dict_size = 16; break;
            case 6:
                dict_size = 18; break;
            case 7:
                dict_size = 21; break;
            case 8:
                dict_size = 24; break;
            case 9:
                dict_size = 26; break;
        }
        return makeLZWCompressor(dict_size, out);
    }
    return nullptr;
}
std::unique_ptr<Decompressor> ArchiveParser::CompressionStrategy::getDecompressor(std::ostream &out) const
{
    if(m_alg == Algorithm::none)
    {
        return std::make_unique<NoneDecompressor>(out);
    }
    else if(m_alg == Algorithm::LZW)
    {
        unsigned dict_size;
        switch(m_algOptions)
        {
            case 0:
                dict_size = 9; break;
            case 1:
                dict_size = 10; break;
            case 2:
                dict_size = 11; break;
            case 3:
                dict_size = 13; break;
            case 4:
                dict_size = 14; break;
            case 5:
                dict_size = 16; break;
            case 6:
                dict_size = 18; break;
            case 7:
                dict_size = 21; break;
            case 8:
                dict_size = 24; break;
            case 9:
                dict_size = 26; break;
        }
        return makeLZWDecompressor(dict_size, out);
    }
    return nullptr;
}

ArchiveParser::ArchiveParser(const char *archivePath)
    : m_archiveStrg(archivePath, std::fstream::in | std::fstream::out | std::fstream::binary),
      m_archive(m_archiveStrg),
      m_archiveHeader()
{
    m_archive.get().exceptions(std::iostream::failbit | std::iostream::badbit);
    m_archive.get().seekg(0, std::iostream::beg);
    std::array<char, M_MAGIC_SIZE> curFileMagic; // NOLINT
    m_archive.get().read(curFileMagic.data(), M_MAGIC_SIZE);
    bool magicOk = std::equal(M_FORMAT_MAGIC.begin(), M_FORMAT_MAGIC.end(), 
                                curFileMagic.begin());
    if(!magicOk)
    {
        throw std::runtime_error("ArchiveParser: file magic does not match");
    }

    readArchiveHeader();
    if(m_archiveHeader.header_version != 0)
    {
        throw std::runtime_error("Unknown header format");
    }
}

ArchiveParser::ArchiveParser(std::iostream &archive)
    : m_archive(archive), m_archiveHeader()
{
    m_archive.get().exceptions(std::iostream::failbit | std::iostream::badbit);
    m_archive.get().seekg(0, std::iostream::beg);
    std::array<char, M_MAGIC_SIZE> curFileMagic; // NOLINT
    m_archive.get().read(curFileMagic.data(), M_MAGIC_SIZE);
    bool magicOk = std::equal(M_FORMAT_MAGIC.begin(), M_FORMAT_MAGIC.end(), 
                                curFileMagic.begin());
    if(!magicOk)
    {
        throw std::runtime_error("ArchiveParser: file magic does not match");
    }

    readArchiveHeader();
    if(m_archiveHeader.header_version != 0)
    {
        throw std::runtime_error("Unknown header format");
    }
}

ArchiveParser ArchiveParser::MakeArchive(const char *archivePath)
{
    std::fstream archiveFile(archivePath, std::fstream::out | std::fstream::binary | std::fstream::trunc);
    archiveFile.exceptions(std::fstream::badbit | std::fstream::failbit);
    archiveFile.seekp(0, std::fstream::beg);
    archiveHeader arcHead;
    arcHead.header_version = 0;
    arcHead.first_file_pos = 0;
    arcHead._reserved = 0;
    archiveFile.write(M_FORMAT_MAGIC.data(), M_FORMAT_MAGIC.size());
    archiveFile.write(reinterpret_cast<char*>(&arcHead.header_version), sizeof(arcHead.header_version)); // NOLINT
    archiveFile.write(reinterpret_cast<char*>(&arcHead._reserved), sizeof(arcHead._reserved)); // NOLINT
    archiveFile.write(reinterpret_cast<char*>(&arcHead.first_file_pos), sizeof(arcHead.first_file_pos)); // NOLINT
    archiveFile.close();

    return ArchiveParser(archivePath);
}

ArchiveParser ArchiveParser::MakeArchive(std::iostream &archive)
{
    archive.exceptions(std::fstream::badbit | std::fstream::failbit);
    archive.seekp(0, std::fstream::beg);
    archiveHeader arcHead;
    arcHead.header_version = 0;
    arcHead.first_file_pos = 0;
    arcHead._reserved = 0;
    archive.write(M_FORMAT_MAGIC.data(), M_FORMAT_MAGIC.size());
    archive.write(reinterpret_cast<char*>(&arcHead.header_version), sizeof(arcHead.header_version)); // NOLINT
    archive.write(reinterpret_cast<char*>(&arcHead._reserved), sizeof(arcHead._reserved)); // NOLINT
    archive.write(reinterpret_cast<char*>(&arcHead.first_file_pos), sizeof(arcHead.first_file_pos)); // NOLINT
    archive.sync();

    return ArchiveParser(archive);
}

void ArchiveParser::readArchiveHeader()
{
    std::streamoff old_off = m_archive.get().tellg();
    m_archive.get().seekg(M_MAGIC_SIZE, std::iostream::beg);
    m_archive.get().read(reinterpret_cast<char*>(&m_archiveHeader.header_version), sizeof(m_archiveHeader.header_version)); // NOLINT
    m_archive.get().read(reinterpret_cast<char*>(&m_archiveHeader._reserved), sizeof(m_archiveHeader._reserved)); // NOLINT
    m_archive.get().read(reinterpret_cast<char*>(&m_archiveHeader.first_file_pos), sizeof(m_archiveHeader.first_file_pos)); // NOLINT
    m_archive.get().seekg(old_off);
}

void ArchiveParser::writeArchiveHeader()
{
    std::streamoff old_off = m_archive.get().tellp();
    m_archive.get().seekp(M_MAGIC_SIZE, std::iostream::beg);
    m_archive.get().write(reinterpret_cast<const char*>(&m_archiveHeader.header_version), sizeof(m_archiveHeader.header_version)); // NOLINT
    m_archive.get().write(reinterpret_cast<const char*>(&m_archiveHeader._reserved), sizeof(m_archiveHeader._reserved)); // NOLINT
    m_archive.get().write(reinterpret_cast<const char*>(&m_archiveHeader.first_file_pos), sizeof(m_archiveHeader.first_file_pos)); // NOLINT
    m_archive.get().seekp(old_off);
}

ArchiveParser::FileHeader ArchiveParser::readFileHeader(FileOffsetType file_pos) const
{
    std::iostream &archive = const_cast<std::iostream&>(m_archive.get()); // NOLINT
    
    FileHeader res; // NOLINT
    std::streamoff old_off = archive.tellg();
    archive.seekg(file_pos, std::iostream::beg); // NOLINT
    archive.read(reinterpret_cast<char*>(&res.file_size), sizeof(res.file_size)); // NOLINT
    archive.read(reinterpret_cast<char*>(&res.next_file_pos), sizeof(res.next_file_pos)); // NOLINT
    archive.read(reinterpret_cast<char*>(&res.checksum), sizeof(res.checksum)); // NOLINT
    archive.read(reinterpret_cast<char*>(&res.name_size), sizeof(res.name_size)); // NOLINT
    archive.read(reinterpret_cast<char*>(&res.file_type), sizeof(res.file_type)); // NOLINT
    archive.read(reinterpret_cast<char*>(&res.compression_alg), sizeof(res.compression_alg)); // NOLINT
    archive.read(reinterpret_cast<char*>(&res.compression_alg_args), sizeof(res.compression_alg_args)); // NOLINT
    archive.seekg(old_off);
    res.cur_file_pos = file_pos;

    return res;
}

void ArchiveParser::writeFileHeader(const FileHeader &fih)
{
    std::streamoff old_off = m_archive.get().tellp();
    m_archive.get().seekp(fih.cur_file_pos, std::iostream::beg); // NOLINT
    m_archive.get().write(reinterpret_cast<const char*>(&fih.file_size), sizeof(fih.file_size)); // NOLINT
    m_archive.get().write(reinterpret_cast<const char*>(&fih.next_file_pos), sizeof(fih.next_file_pos)); // NOLINT
    m_archive.get().write(reinterpret_cast<const char*>(&fih.checksum), sizeof(fih.checksum)); // NOLINT
    m_archive.get().write(reinterpret_cast<const char*>(&fih.name_size), sizeof(fih.name_size)); // NOLINT
    m_archive.get().write(reinterpret_cast<const char*>(&fih.file_type), sizeof(fih.file_type)); // NOLINT
    m_archive.get().write(reinterpret_cast<const char*>(&fih.compression_alg), sizeof(fih.compression_alg)); // NOLINT
    m_archive.get().write(reinterpret_cast<const char*>(&fih.compression_alg_args), sizeof(fih.compression_alg_args)); // NOLINT
    m_archive.get().seekp(old_off);
}

ArchiveParser::FileOffsetType ArchiveParser::getLastFilePos() const
{
    if(m_archiveHeader.first_file_pos == 0) 
    {
        return 0; // no last file
    }
    if(m_lastFilePosValid)
    {
        return m_lastFilePos;
    }
    FileHeader cur_file; // NOLINT
    FileOffsetType next_file = m_archiveHeader.first_file_pos;
    while(next_file != 0)
    {
        cur_file = readFileHeader(next_file);
        next_file = cur_file.next_file_pos;
    }
    return cur_file.cur_file_pos;
}

void ArchiveParser::updateLastFilePos(FileOffsetType new_last)
{
    if(m_archiveHeader.first_file_pos == 0)
    {
        m_archiveHeader.first_file_pos = new_last;
        writeArchiveHeader();
    }
    m_lastFilePos = new_last;
    m_lastFilePosValid = true;
}


ArchiveParser::FileOffsetType ArchiveParser::calculateFileEntrySize(std::size_t name_size, std::size_t file_size)
{
    std::size_t res = 0;
    res += FileHeader::HEADER_SIZE;
    res += name_size;
    res += file_size;

    return res;
}

ArchiveParser::FileOffsetType ArchiveParser::allocateFileEntrySpace(std::size_t file_entry_size) const
{
    /*
    // TODO: use interval tree or smth other to find holes in file
    // for now, just return the end of the file
    (void) file_entry_size;

    std::iostream &archive = m_archive.get(); // NOLINT
    std::size_t old_pos = static_cast<FileOffsetType>(archive.tellp());
    archive.seekp(0, std::iostream::end);
    std::size_t end_pos = static_cast<FileOffsetType>(archive.tellp());
    archive.seekp(static_cast<std::streamsize>(old_pos));
    return end_pos;
    */

    // Find a suitable hole

    std::vector<std::pair<FileOffsetType, FileOffsetType> > distPairs;
    for(const FileInfo &file : *this)
    {
        distPairs.push_back(file.getEntryBeginEnd());
    }
    std::size_t best_pos = distPairs.size();
    if(!distPairs.empty())
    {
        std::sort(distPairs.begin(), distPairs.end());
        for(std::size_t i=0; i<distPairs.size()-1; i++)
        {
            if(distPairs[i+1].first < distPairs[i].second)
            {
                throw std::runtime_error("Archive is corrupted!");
            }
            std::size_t cur_dist = distPairs[i+1].first - distPairs[i].second;
            if(cur_dist == 0)
            {
                continue;
            }
            if(cur_dist == file_entry_size)
            {
                best_pos = i;
                break;
            }
            if(cur_dist >= file_entry_size)
            {
                if(best_pos == distPairs.size())
                {
                    best_pos = i;
                }
                else
                {
                    std::size_t best_dist = distPairs[best_pos+1].first - distPairs[best_pos].second;
                    if(best_dist > cur_dist)
                    {
                        best_pos = i;
                    }
                }
            }
        }
    }

    if(best_pos == distPairs.size())
    {
        std::iostream &archive = m_archive.get(); // NOLINT
        std::size_t old_pos = static_cast<FileOffsetType>(archive.tellp());
        archive.seekp(0, std::iostream::end);
        std::size_t end_pos = static_cast<FileOffsetType>(archive.tellp());
        archive.seekp(static_cast<std::streamsize>(old_pos));
        return end_pos;
    }
    return distPairs[best_pos].second;
}


void ArchiveParser::calcCrcFileEntry(FileHeader &header, const char *name, std::istream &file, std::size_t file_size)
{
    CRC32 crc;
    crc(header.file_size);
    crc(header.name_size);
    crc(header.file_type);
    crc(header.compression_alg);
    crc(header.compression_alg_args);
    crc(name, name+std::strlen(name)); // NOLINT
    std::streamoff old_off = file.tellg();
    crc(file, file_size);
    file.seekg(old_off, std::istream::beg);

    header.checksum = crc.getResult();
}

void ArchiveParser::calcCrcFolderEntry(FileHeader &header, const char *name)
{
    CRC32 crc;
    crc(header.file_size);
    crc(header.name_size);
    crc(header.file_type);
    crc(header.compression_alg);
    crc(header.compression_alg_args);
    crc(name, name+std::strlen(name)); // NOLINT

    header.checksum = crc.getResult();
}

bool ArchiveParser::verifyCrcFileEntry(const FileHeader &header) const
{
    CRC32 crc;
    crc(header.file_size);
    crc(header.name_size);
    crc(header.file_type);
    crc(header.compression_alg);
    crc(header.compression_alg_args);

    std::streamoff old_off = m_archive.get().tellg();

    m_archive.get().seekg(header.cur_file_pos+FileHeader::HEADER_SIZE, std::iostream::beg);
    crc(m_archive.get(), header.name_size);
    crc(m_archive.get(), header.file_size);

    m_archive.get().seekg(old_off, std::iostream::beg);

    return crc.getResult() == header.checksum;
}

static void stream_dd(std::istream &ins, std::size_t size, std::ostream &outs)
{
    constexpr std::size_t BUFFER_SIZE = 1024;
    std::array<char, BUFFER_SIZE> buf; // NOLINT
    while(size > BUFFER_SIZE)
    {
        ins.read(buf.data(), BUFFER_SIZE);
        outs.write(buf.data(), BUFFER_SIZE);
        size -= BUFFER_SIZE;
    }

    ins.read(buf.data(), size);   // NOLINT
    outs.write(buf.data(), size); // NOLINT
}

void ArchiveParser::writeFileEntry (const FileHeader &header, const char *name, std::istream &file, std::size_t file_size)
{
    writeFileHeader(header);
    std::streamoff old_pos = m_archive.get().tellp();
    m_archive.get().seekp(header.cur_file_pos+FileHeader::HEADER_SIZE, std::iostream::beg); // NOLINT
    m_archive.get().write(name, std::strlen(name)); // NOLINT
    stream_dd(file, file_size, m_archive);

    m_archive.get().seekp(old_pos);

    FileOffsetType last_file_pos = getLastFilePos();
    if(last_file_pos != 0)
    {
        FileHeader fih = readFileHeader(last_file_pos);
        assert(fih.next_file_pos == 0);
        fih.next_file_pos = header.cur_file_pos;
        writeFileHeader(fih);
    }
    else // last_file_pos = 0
    {
        m_archiveHeader.first_file_pos = header.cur_file_pos;
        writeArchiveHeader();
    }
    updateLastFilePos(header.cur_file_pos);
}

void ArchiveParser::writeFolderEntry (const FileHeader &header, const char *name)
{
    assert(header.file_size == 0);
    writeFileHeader(header);
    std::streamoff old_pos = m_archive.get().tellp();
    m_archive.get().seekp(header.cur_file_pos+FileHeader::HEADER_SIZE, std::iostream::beg); // NOLINT
    m_archive.get().write(name, std::strlen(name)); // NOLINT

    m_archive.get().seekp(old_pos);

    FileOffsetType last_file_pos = getLastFilePos();
    if(last_file_pos != 0)
    {
        FileHeader fih = readFileHeader(last_file_pos);
        assert(fih.next_file_pos == 0);
        fih.next_file_pos = header.cur_file_pos;
        writeFileHeader(fih);
    }
    else // last_file_pos = 0
    {
        m_archiveHeader.first_file_pos = header.cur_file_pos;
        writeArchiveHeader();
    }
    updateLastFilePos(header.cur_file_pos);
}

// true - OK
bool ArchiveParser::checkArchiveConsistency() const
{
    std::vector<std::pair<FileOffsetType, FileOffsetType> > distPairs;
    for(const FileInfo &file : *this)
    {
        distPairs.push_back(file.getEntryBeginEnd());
    }
    if(!distPairs.empty())
    {
        std::sort(distPairs.begin(), distPairs.end());
        for(std::size_t i=0; i<distPairs.size()-1; i++)
        {
            if(distPairs[i+1].first < distPairs[i].second)
            {
                return false;
            }
        }
    }
    return true;
}

void ArchiveParser::addFile(const char *name, std::istream &file, const CompressionStrategy &comps, const char *tempFilePath)
{
    std::array<char, 7> tempSuffix;

    std::random_device rdev;
    std::mt19937 gen(rdev());
    std::uniform_int_distribution<unsigned> dist(0, 61);

    for(unsigned i=0; i<tempSuffix.size()-1; ++i)
    {
        unsigned code = dist(gen);
        char chr;
        if(code < 26)
        {
            chr = static_cast<char>(code - 0 + 'A');
        }
        else if(code < 26+26)
        {
            chr = static_cast<char>(code - 26 + 'a');
        }
        else 
        {
            chr = static_cast<char>(code - (26+26) + '0');
        }
        tempSuffix[i] = chr;
    }
    tempSuffix.back()='\0';

    std::string tempFileName = std::string(tempFilePath) + std::string(name) + ".temp." + tempSuffix.data();

    if(boost::filesystem::exists(tempFileName))
    {
        throw std::runtime_error("Temporary file with name: " + tempFileName + " exists!");
    }

    std::fstream temp_file(tempFileName, 
                std::fstream::in | std::fstream::out | std::fstream::trunc | std::fstream::binary); // no option O_CREAT :(

    try
    {
        addFile(name, file, comps, temp_file);
    }
    catch(...)
    {
        temp_file.close();
        boost::filesystem::remove(tempFileName);
        throw;
    }

    temp_file.close();
    boost::filesystem::remove(tempFileName);
}

void ArchiveParser::addFile(const char *name, std::istream &file, const CompressionStrategy &comps, std::iostream &temp_file)
{
    std::size_t nameSize = std::strlen(name);
    if(nameSize > std::numeric_limits<uint16_t>::max() - 1)
    {
        throw std::runtime_error(std::string("Name: \"") + name + "\" too large!");
    }
    if(findFile(name) != cend())
    {
        throw std::runtime_error(std::string("File with the name \"") + name + "\" already exits");
    }

    temp_file.exceptions(std::iostream::badbit | std::iostream::failbit);

    std::unique_ptr<Compressor> comp = comps.getCompressor(temp_file);
    Compressor &com = *comp;
    //LZWCompressor<16> lzwC(temp_file); //NOLINT
    file.seekg(0, std::istream::end);
    std::size_t file_size = static_cast<std::size_t>(file.tellg());
    file.seekg(0, std::istream::beg);
    com(file, file_size);
    com.finish();

    temp_file.seekg(0, std::istream::end);
    std::size_t compressed_file_size = static_cast<std::size_t>(temp_file.tellg());
    temp_file.seekg(0, std::istream::beg);

    if(compressed_file_size >= file_size)
    {

        std::size_t entrySize = calculateFileEntrySize(nameSize, file_size);
        FileOffsetType newEntryPos = allocateFileEntrySpace(entrySize);

        CompressionStrategy nocomp ("NONE", 0);

        FileHeader fih; // NOLINT
        fih.cur_file_pos = newEntryPos;
        fih.next_file_pos = 0;
        fih.file_size = file_size;
        fih.name_size = static_cast<std::uint16_t>(nameSize);
        fih.file_type = fileType::file;
        fih.compression_alg = nocomp.getAlgVal();
        fih.compression_alg_args = nocomp.getAlgOptionsVal();

        file.seekg(0, std::istream::beg);
        calcCrcFileEntry(fih, name, file, file_size);

        file.seekg(0, std::istream::beg);
        writeFileEntry(fih, name, file, file_size);
    }
    else
    {
        std::size_t entrySize = calculateFileEntrySize(nameSize, compressed_file_size);
        FileOffsetType newEntryPos = allocateFileEntrySpace(entrySize);

        FileHeader fih; // NOLINT
        fih.cur_file_pos = newEntryPos;
        fih.next_file_pos = 0;
        fih.file_size = compressed_file_size;
        fih.name_size = static_cast<std::uint16_t>(nameSize);
        fih.file_type = fileType::file;
        fih.compression_alg = comps.getAlgVal();
        fih.compression_alg_args = comps.getAlgOptionsVal();

        temp_file.seekg(0, std::istream::beg);
        calcCrcFileEntry(fih, name, temp_file, compressed_file_size);

        temp_file.seekg(0, std::istream::beg);
        writeFileEntry(fih, name, temp_file, compressed_file_size);
    }
}

std::string ArchiveParser::readFileName (const FileHeader &header) const
{
    std::iostream &archive = m_archive.get(); // NOLINT
    std::streamoff old_off = archive.tellg();
    archive.seekg(header.cur_file_pos+FileHeader::HEADER_SIZE, std::iostream::beg); // NOLINT
    std::string res;
    res.resize(header.name_size);
    archive.read(const_cast<char*>(res.data()), header.name_size); // NOLINT
    archive.seekg(old_off);

    return res;
}

void ArchiveParser::readAndDecompressFileContents (const FileHeader &header, std::ostream &out) const
{
    std::iostream &archive = m_archive.get(); // NOLINT

    std::streamoff oldOff = archive.tellg();
    std::streamoff newOff = static_cast<std::streamoff>(header.cur_file_pos+
                                    FileHeader::HEADER_SIZE+header.name_size);
    archive.seekg(newOff, std::iostream::beg);
    //LZWDecompressor<16> lzwD(out); // TODO: this
    CompressionStrategy comps(header.compression_alg, header.compression_alg_args);
    std::unique_ptr<Decompressor> decp = comps.getDecompressor(out);
    Decompressor &dec = *decp;
    dec(archive, header.file_size);
    archive.seekg(oldOff);
}

ArchiveParser::const_iterator ArchiveParser::cbefore_begin() const
{
    return FileIterator(*this, archiveHeader::FIRST_FILE_FIELD_POS);
}

ArchiveParser::const_iterator ArchiveParser::cbegin() const
{
    return FileIterator(*this, m_archiveHeader.first_file_pos);
}

ArchiveParser::const_iterator ArchiveParser::cend() const
{
    return FileIterator(*this, 0);
}

ArchiveParser::FileIterator& ArchiveParser::FileIterator::operator++()
{
    assert(m_filePos != 0);

    if(m_filePos == archiveHeader::FIRST_FILE_FIELD_POS)
    {
        m_filePos = m_archive.m_archiveHeader.first_file_pos;
    }
    else
    {
        FileHeader fih = m_archive.readFileHeader(m_filePos);
        m_filePos = fih.next_file_pos;
    }
    return *this;
}

ArchiveParser::FileInfo& ArchiveParser::FileIterator::operator*()
{
    assert(m_filePos != archiveHeader::FIRST_FILE_FIELD_POS);
    m_fileInfo = FileInfo(m_archive, m_filePos);

    return m_fileInfo;
}

ArchiveParser::const_iterator ArchiveParser::findFile(const char *name) const
{
    for(const_iterator it=this->cbegin(); it != this->cend(); ++it)
    {
        if(it->getFileName() == name)
        {
            return it;
        }
    }

    return this->cend();
}

void ArchiveParser::deleteAfter(const_iterator pos)
{
    if(pos.m_filePos == 0)
    {
        return;
    }
    FileHeader prevFileHeader = readFileHeader(pos.m_filePos);
    if(prevFileHeader.next_file_pos == 0)
    {
        return;
    }
    FileHeader curFileHeader = readFileHeader(prevFileHeader.next_file_pos);
    FileOffsetType nextFileOff = curFileHeader.next_file_pos;
    prevFileHeader.next_file_pos = nextFileOff;
    writeFileHeader(prevFileHeader);
    if(getLastFilePos() == curFileHeader.cur_file_pos)
    {
        updateLastFilePos(prevFileHeader.cur_file_pos);
    }

    // TODO: punch POSIX hole in the file
}

void ArchiveParser::readFile(const char *name, std::ostream &out) const
{
    const_iterator itf = findFile(name);
    if(itf == cend())
    {
        throw std::runtime_error("File not found in archive");
    }
    itf->readFile(out);
}

void ArchiveParser::deleteFile(const char *name)
{
    const_iterator prev = cbefore_begin();
    const_iterator cur = cbegin();

    while(cur != cend())
    {
        if(cur->getFileName() == name)
        {
            deleteAfter(prev);
            return;
        }

        ++prev; ++cur;
    }
}

ArchiveParser::fileType ArchiveParser::getFileType(const char *name) const
{
    const_iterator fit = findFile(name);
    if(fit == cend())
    {
        throw std::runtime_error("No file with this name found!");
    }
    return fit->getFileType();
}

void ArchiveParser::addFolder(const char *name)
{
    std::size_t nameSize = std::strlen(name);
    if(nameSize > std::numeric_limits<uint16_t>::max() - 1)
    {
        throw std::runtime_error(std::string("Name: \"") + name + "\" too large!");
    }
    if(findFile(name) != cend())
    {
        throw std::runtime_error(std::string("Folder with the name \"") + name + "\" already exits");
    }

    std::size_t entrySize = calculateFileEntrySize(nameSize, 0);

    FileOffsetType newEntryPos = allocateFileEntrySpace(entrySize);

    CompressionStrategy nocomp("NONE", 0);
    FileHeader fih; // NOLINT
    fih.cur_file_pos = newEntryPos;
    fih.next_file_pos = 0;
    fih.file_size = 0;
    fih.name_size = static_cast<std::uint16_t>(nameSize);
    fih.file_type = fileType::folder;
    fih.compression_alg = nocomp.getAlgVal();
    fih.compression_alg_args = nocomp.getAlgOptionsVal();
    
    calcCrcFolderEntry(fih, name);
    
    writeFolderEntry(fih, name);
}

// true - OK
bool ArchiveParser::verify() const
{
    if(!checkArchiveConsistency()) 
    {
        return false;
    }
    for(FileInfo &file : *this)
    {
        if(!file.verify())
        {
            return false;
        }
    }

    return true;
}
