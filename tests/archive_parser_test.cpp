#include <catch2/catch.hpp>
#include <cstring>
#include <sstream>

#include "archive_parser.hpp"

TEST_CASE("Basic file store")
{
    unsigned comp_level = GENERATE(0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U);

    std::stringstream arch_file;
    ArchiveParser arch = ArchiveParser::MakeArchive(arch_file);
    ArchiveParser::CompressionStrategy dcs("LZW", comp_level);
    
    const char *fc1 = "TestTest1";
    const char *fc2 = "TestTest2";
    const char *fc3 = "TestTest3";

    std::istringstream ifs;
    std::stringstream temp_file;

    temp_file = std::stringstream();
    ifs.str(fc1);
    arch.addFile("file1.txt", ifs, dcs, temp_file);

    temp_file = std::stringstream();
    ifs.str(fc2);
    arch.addFile("file2.txt", ifs, dcs, temp_file);

    temp_file = std::stringstream();
    ifs.str(fc3);
    arch.addFile("file3.txt", ifs, dcs, temp_file);

    arch.addFolder("folder1");

    CHECK(arch.verify());

    std::ostringstream ofs;

    CHECK(arch.getFileType("file1.txt") == ArchiveParser::fileType::file);
    ofs = std::ostringstream();
    arch.readFile("file1.txt", ofs);
    CHECK(ofs.str() == fc1);

    CHECK(arch.getFileType("file2.txt") == ArchiveParser::fileType::file);
    ofs = std::ostringstream();
    arch.readFile("file2.txt", ofs);
    CHECK(ofs.str() == fc2);

    CHECK(arch.getFileType("file3.txt") == ArchiveParser::fileType::file);
    ofs = std::ostringstream();
    arch.readFile("file3.txt", ofs);
    CHECK(ofs.str() == fc3);

    std::size_t files_cnt = 0;
    for(const ArchiveParser::value_type &file : arch)
    {
        ++files_cnt;
        CHECK(file.verify());
        if(file.getFileName() == "file1.txt")
        {
            ofs = std::ostringstream();
            file.readFile(ofs);
            CHECK(ofs.str() == fc1);
            CHECK(file.getFileType() == ArchiveParser::fileType::file);
        }
        else if(file.getFileName() == "file2.txt")
        {
            ofs = std::ostringstream();
            file.readFile(ofs);
            CHECK(ofs.str() == fc2);
            CHECK(file.getFileType() == ArchiveParser::fileType::file);
        }
        else if(file.getFileName() == "file3.txt")
        {
            ofs = std::ostringstream();
            file.readFile(ofs);
            CHECK(ofs.str() == fc3);
            CHECK(file.getFileType() == ArchiveParser::fileType::file);
        }
        else if(file.getFileName() == "folder1")
        {
            CHECK(file.getFileType() == ArchiveParser::fileType::folder);
        }
        else
        {
            CHECK(false);
        }
    }
    CHECK(files_cnt == 4);

    arch.deleteFile("file2.txt");
    arch.deleteFile("folder1");

    CHECK(arch.verify());

    files_cnt = 0;
    for(const ArchiveParser::value_type &file : arch)
    {
        ++files_cnt;
        CHECK(file.verify());
        ofs = std::ostringstream();
        CHECK(file.getFileType() == ArchiveParser::fileType::file);
        file.readFile(ofs);
        if(file.getFileName() == "file1.txt")
        {
            CHECK(ofs.str() == fc1);
        }
        else if(file.getFileName() == "file3.txt")
        {
            CHECK(ofs.str() == fc3);
        }
        else
        {
            CHECK(false);
        }
    }
    CHECK(files_cnt == 2);

    const char *fc4 = "ASDSAasd4";
    temp_file = std::stringstream();
    ifs.str(fc4);
    arch.addFile("file4.txt", ifs, dcs, temp_file);

    CHECK(arch.verify());

    files_cnt = 0;
    for(const ArchiveParser::value_type &file : arch)
    {
        ++files_cnt;
        CHECK(file.verify());
        ofs = std::ostringstream();
        CHECK(file.getFileType() == ArchiveParser::fileType::file);
        file.readFile(ofs);
        if(file.getFileName() == "file1.txt")
        {
            CHECK(ofs.str() == fc1);
        }
        else if(file.getFileName() == "file3.txt")
        {
            CHECK(ofs.str() == fc3);
        }
        else if(file.getFileName() == "file4.txt")
        {
            CHECK(ofs.str() == fc4);
        }
        else
        {
            CHECK(false);
        }
    }
    CHECK(files_cnt == 3);
}
