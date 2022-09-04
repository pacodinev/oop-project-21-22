#include <algorithm>
#include <boost/filesystem/directory.hpp>
#include <boost/filesystem/file_status.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <iostream>
#include <fstream>
#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <boost/filesystem.hpp>

#include "archive_parser.hpp"

namespace fs = boost::filesystem;

static void parse_command_zip (std::istream &ins, std::ostream &outs, std::ostream &errs)
{
    (void) outs;
    (void) errs;
    std::string archive_path;
    ins >> archive_path;
    if(fs::exists(archive_path))
    {
        throw std::runtime_error(std::string("File with name ") + archive_path + " already exists!");
    }

    std::string other_args_str;
    std::getline(ins, other_args_str, '\n');
    std::istringstream other_args(other_args_str);

    std::string entry_str;
    try {
        ArchiveParser arch = ArchiveParser::MakeArchive(archive_path.c_str());
        // NOTE!!!: това задава каква да е компресията и какъв алгоритъм да е. Не съм го извел навън през командния ред
        arch.setDefaultCompressionStrategy(ArchiveParser::CompressionStrategy("LZW", 3));
        while(other_args >> entry_str)
        {
            fs::path entry(entry_str);
            entry = entry.lexically_normal();
            if(fs::is_regular_file(entry))
            {
                std::fstream file(entry.native().c_str(), std::fstream::in);
                file.exceptions(std::fstream::badbit | std::fstream::failbit);
                arch.addFile(entry.generic_string().c_str(), file);
            }
            if(fs::is_directory(entry))
            {
                if(fs::is_empty(entry))
                {
                    arch.addFolder(entry.generic_string().c_str());
                }
                else
                {
                    for(fs::directory_entry& file : fs::recursive_directory_iterator(entry))
                    {
                        fs::path file_path = file.path();
                        if(fs::is_regular_file(file_path))
                        {
                            std::fstream new_file(file_path.native().c_str(), std::fstream::in | std::fstream::binary);
                            new_file.exceptions(std::fstream::badbit | std::fstream::failbit);
                            arch.addFile(file_path.generic_string().c_str(), new_file);
                        }
                        else if(fs::is_empty(file_path))
                        {
                            arch.addFolder(file_path.generic_string().c_str());
                        }
                    }
                }
            }

        }
    } catch (...)
    {
        fs::remove(archive_path);
        throw;
    }
}

static void parse_command_info (std::istream &ins, std::ostream &outs, std::ostream &errs)
{
    (void) errs;
    std::string archive_path;
    ins >> archive_path;
    outs << "Contents of the archive are:\n";
    ArchiveParser arch(archive_path.c_str());
    for(const ArchiveParser::value_type &file : arch)
    {
        outs << "Name: " << file.getFileName() 
             << "\t\tType: " << (file.getFileType() == ArchiveParser::fileType::file ? "file" : "folder") 
             << "\tCompressed size: " << file.getCompressedFileSize() 
             << " ComprAlg: " << file.getCompressionStrg().getAlgStr() << "-" 
             << static_cast<short>(file.getCompressionStrg().getAlgOptionsVal()) <<'\n';
    }
}

static bool path_is_base_of (const fs::path &fromp, const fs::path &ofp)
{
    fs::path::const_iterator from_it = fromp.begin();
    fs::path::const_iterator of_it = ofp.begin();
    while(from_it != fromp.end() && of_it != ofp.end())
    {
        if(from_it->filename() != of_it->filename())
        {
            return false;
        }
        ++from_it; ++of_it;
    }

    return from_it == fromp.end();
}

static void parse_command_unzip (std::istream &ins, std::ostream &outs, std::ostream &errs)
{
    (void) outs;
    std::string archive_path;
    ins >> archive_path;
    std::string dest_path_str;
    ins >> dest_path_str;

    fs::path dest_path(dest_path_str);
    if(!fs::is_directory(dest_path))
    {
        throw std::runtime_error(dest_path_str + "is not a directory!");
    }

    std::string other_args_str;
    std::getline(ins, other_args_str, '\n');
    std::istringstream other_args(other_args_str);


    std::string entry_str;
    ArchiveParser arch(archive_path.c_str());
    std::size_t args = 0;
    while(other_args >> entry_str)
    {
        ++args;
        fs::path entry(entry_str);
        for(const ArchiveParser::value_type &file : arch)
        {
            if(path_is_base_of(entry, file.getFileName()))
            {
                fs::path cur_path(dest_path);
                cur_path /= file.getFileName();
                if(fs::exists(cur_path))
                {
                    errs << "File " << cur_path.c_str() << " already exists!\n";
                    continue;
                }
                if(file.getFileType() == ArchiveParser::fileType::folder)
                {
                    fs::create_directories(cur_path);
                }
                else
                {
                    fs::path parentPath = cur_path.parent_path();
                    if(!parentPath.empty())
                    {
                        fs::create_directories(parentPath);
                    }
                    std::fstream new_file(cur_path.native().c_str(), std::fstream::out | std::fstream::trunc | std::fstream::binary);
                    new_file.exceptions(std::fstream::badbit | std::fstream::failbit);
                    file.readFile(new_file);
                }
            }
        }
    }
    if(args == 0)
    {
        for(const ArchiveParser::value_type &file : arch)
        {
            fs::path cur_path(dest_path);
            cur_path /= file.getFileName();
            if(fs::exists(cur_path))
            {
                errs << "File " << cur_path.c_str() << " already exists!\n";
                continue;
            }
            if(file.getFileType() == ArchiveParser::fileType::folder)
            {
                fs::create_directories(cur_path);
            }
            else
            {
                fs::path parentPath = cur_path.parent_path();
                if(!parentPath.empty())
                {
                    fs::create_directories(parentPath);
                }
                std::fstream new_file(cur_path.native().c_str(), std::fstream::out | std::fstream::trunc | std::fstream::binary);
                new_file.exceptions(std::fstream::badbit | std::fstream::failbit);
                file.readFile(new_file);
            }
        }
    }
}

static void parse_command_ec (std::istream &ins, std::ostream &outs, std::ostream &errs)
{
    (void) errs;
    std::string archive_path;
    ins >> archive_path;
    ArchiveParser arch(archive_path.c_str());
    bool res = arch.verify();
    if(res)
    {
        outs << "Archive is OK!\n";
    }
    else
    {
        outs << "Archive is CORRUPTED! :(\n";
    }
}

static void parse_command_refresh (std::istream &ins, std::ostream &outs, std::ostream &errs)
{
    (void) outs;
    (void) errs;
    std::string archive_path;
    ins >> archive_path;
    ArchiveParser arch(archive_path.c_str());
    // NOTE!!!: това задава каква да е компресията и какъв алгоритъм да е. Не съм го извел навън през командния ред
    arch.setDefaultCompressionStrategy(ArchiveParser::CompressionStrategy("LZW", 3));
    std::string old_file;
    ins >> old_file;
    std::string replaced_file_content;
    ins >> replaced_file_content;

    if(arch.getFileType(old_file.c_str()) != ArchiveParser::fileType::file)
    {
        throw std::runtime_error(old_file + " is not a folder in the archive!");
    }

    arch.deleteFile(old_file.c_str());
    std::fstream new_file(replaced_file_content, std::fstream::in | std::fstream::binary);
    new_file.exceptions(std::fstream::badbit | std::fstream::failbit);
    arch.addFile(old_file.c_str(), new_file);
}

static void parse_commands (std::istream &ins, std::ostream &outs, std::ostream &errs)
{
    std::string comm;
    while(comm != "EXIT")
    {
        try {
            ins >> comm;
            bool comm_unknown = false;
            if(comm == "ZIP")
            {
                parse_command_zip(ins, outs, errs);
            }
            else if(comm == "INFO")
            {
                parse_command_info(ins, outs, errs);
            }
            else if(comm == "UNZIP")
            {
                parse_command_unzip(ins, outs, errs);
            }
            else if(comm == "EC")
            {
                parse_command_ec(ins, outs, errs);
            }
            else if(comm == "REFRESH")
            {
                parse_command_refresh(ins, outs, errs);
            }
            else if(comm == "EXIT")
            {

            }
            else
            {
                comm_unknown = true;
            }

            if(comm_unknown)
            {
                outs << "Unknown command!" << std::endl;
            }
            else 
            {
                outs << "Command executed successfully!" << std::endl;
            }
        } catch (std::exception &exc)
        {
            errs << "Operation FAILED! Reason:\n";
            errs << exc.what() << '\n';
        }

    }
}

int main()
{
    std::cin.exceptions(std::istream::badbit | std::istream::failbit);
    parse_commands(std::cin, std::cout, std::clog);

    return 0;
}
