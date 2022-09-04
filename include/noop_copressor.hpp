#pragma once

#include "compressor_base.hpp"
#include <istream>
#include <ostream>
#include <array>

class NoneCompressor final : public Compressor
{
private:
    std::ostream &out;
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

public:
    explicit NoneCompressor(std::ostream &_out) : out(_out) {}

    void prepare(std::istream &ins, std::size_t ins_size) override
    {
        (void)ins;
        (void)ins_size;
    }


    void operator() (std::istream &ins, std::size_t ins_size) override
    {
        stream_dd(ins, ins_size, out);
    }

    void finish() override 
    { }
};

class NoneDecompressor final : public Decompressor
{
private:
    std::ostream &out;
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

public:
    explicit NoneDecompressor(std::ostream &_out) : out(_out) {}

    void operator() (std::istream &ins, std::size_t ins_size) override
    {
        stream_dd(ins, ins_size, out);
    }

    void finish() override 
    { }
};
