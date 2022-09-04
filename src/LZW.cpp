#include "LZW.hpp"

#include <cassert>
#include <memory>
#include <ostream>

std::unique_ptr<Compressor> makeLZWCompressor(unsigned dictSize, std::ostream &out)
{
    assert(9<= dictSize && dictSize <= 27);
    switch (dictSize) {
        case 9:
            return std::make_unique<LZWCompressor<9>>(out);
        case 10:
            return std::make_unique<LZWCompressor<10>>(out);
        case 11:
            return std::make_unique<LZWCompressor<11>>(out);
        case 12:
            return std::make_unique<LZWCompressor<12>>(out);
        case 13:
            return std::make_unique<LZWCompressor<13>>(out);
        case 14:
            return std::make_unique<LZWCompressor<14>>(out);
        case 15:
            return std::make_unique<LZWCompressor<15>>(out);
        case 16:
            return std::make_unique<LZWCompressor<16>>(out);
        case 17:
            return std::make_unique<LZWCompressor<17>>(out);
        case 18:
            return std::make_unique<LZWCompressor<18>>(out);
        case 19:
            return std::make_unique<LZWCompressor<19>>(out);
        case 20:
            return std::make_unique<LZWCompressor<20>>(out);
        case 21:
            return std::make_unique<LZWCompressor<21>>(out);
        case 22:
            return std::make_unique<LZWCompressor<22>>(out);
        case 23:
            return std::make_unique<LZWCompressor<23>>(out);
        case 24:
            return std::make_unique<LZWCompressor<24>>(out);
        case 25:
            return std::make_unique<LZWCompressor<25>>(out);
        case 26:
            return std::make_unique<LZWCompressor<26>>(out);
        case 27:
            return std::make_unique<LZWCompressor<27>>(out);
        /*case 28:
            return std::make_unique<LZWCompressor<28>>(out);
        case 29:
            return std::make_unique<LZWCompressor<29>>(out);
        case 30:
            return std::make_unique<LZWCompressor<30>>(out);
        case 31:
            return std::make_unique<LZWCompressor<31>>(out);
        case 32:
            return std::make_unique<LZWCompressor<32>>(out);*/
    }
    return nullptr;
}

std::unique_ptr<Decompressor> makeLZWDecompressor(unsigned dictSize, std::ostream &out)
{
    assert(9<= dictSize && dictSize <= 27);
    switch (dictSize) {
        case 9:
            return std::make_unique<LZWDecompressor<9>>(out);
        case 10:
            return std::make_unique<LZWDecompressor<10>>(out);
        case 11:
            return std::make_unique<LZWDecompressor<11>>(out);
        case 12:
            return std::make_unique<LZWDecompressor<12>>(out);
        case 13:
            return std::make_unique<LZWDecompressor<13>>(out);
        case 14:
            return std::make_unique<LZWDecompressor<14>>(out);
        case 15:
            return std::make_unique<LZWDecompressor<15>>(out);
        case 16:
            return std::make_unique<LZWDecompressor<16>>(out);
        case 17:
            return std::make_unique<LZWDecompressor<17>>(out);
        case 18:
            return std::make_unique<LZWDecompressor<18>>(out);
        case 19:
            return std::make_unique<LZWDecompressor<19>>(out);
        case 20:
            return std::make_unique<LZWDecompressor<20>>(out);
        case 21:
            return std::make_unique<LZWDecompressor<21>>(out);
        case 22:
            return std::make_unique<LZWDecompressor<22>>(out);
        case 23:
            return std::make_unique<LZWDecompressor<23>>(out);
        case 24:
            return std::make_unique<LZWDecompressor<24>>(out);
        case 25:
            return std::make_unique<LZWDecompressor<25>>(out);
        case 26:
            return std::make_unique<LZWDecompressor<26>>(out);
        case 27:
            return std::make_unique<LZWDecompressor<27>>(out);
        /*case 28:
            return std::make_unique<LZWDecompressor<28>>(out);
        case 29:
            return std::make_unique<LZWDecompressor<29>>(out);
        case 30:
            return std::make_unique<LZWDecompressor<30>>(out);
        case 31:
            return std::make_unique<LZWDecompressor<31>>(out);
        case 32:
            return std::make_unique<LZWDecompressor<32>>(out);*/
    }
    return nullptr;
}
