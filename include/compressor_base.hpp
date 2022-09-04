#pragma once

#include <istream>

class Compressor
{
public:
    virtual ~Compressor() = default;

    // calling this function can improve compression
    virtual void prepare(std::istream &ins, std::size_t read_size) = 0;

    virtual void operator() (std::istream &ins, std::size_t read_size) = 0;

    virtual void finish() = 0;
};

class Decompressor
{
public:
    virtual ~Decompressor() = default;

    virtual void operator() (std::istream &ins, std::size_t read_size) = 0;

    virtual void finish() = 0;
};
