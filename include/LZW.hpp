#pragma once

#include "compressor_base.hpp"

#include <boost/unordered/unordered_map.hpp>
#include <climits>
#include <cstdint>
#include <ios>
#include <istream>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

// Алгоритъмът е взаимстван от https://www.cplusplus.com/articles/iL18T05o/

// Work only on little endian
// Probably only if CHAR_BIT == 8
static_assert(CHAR_BIT == 8, ""); // NOLINT

template<unsigned DICT_SIZE_POW>
class LZWCompressor final : public Compressor
{
private:

    // types:
    static_assert(DICT_SIZE_POW > CHAR_BIT, "Dictionary size is too small");
    static_assert(DICT_SIZE_POW <= 32, "Dictionary size is too large"); // NOLINT
    using CodeType = std::conditional_t<DICT_SIZE_POW <= 16, std::uint16_t, std::uint32_t>;
    using KeyType = std::pair<CodeType, uint8_t>;
    // Използвам boost::unordered_map защото се представя доста по-бързо от 
    // колкото std::unordered_map (или поне така беше преди, не знам, може и да 
    // са пренаписали libstdcpp или libc++)
    using DictContainer = boost::unordered_map<KeyType, CodeType>;

    // static constatns
    static constexpr std::size_t DICT_SIZE = (1ULL << DICT_SIZE_POW);
    static constexpr unsigned DICT_MAX_SIZE = DICT_SIZE - 1U;
    static constexpr CodeType INVALID_CODETYPE = DICT_MAX_SIZE;

    // member variables
    DictContainer dict{};
    std::ostream &out;
    std::uint64_t bitsBuffer = 0;
    unsigned bitsBufferSize = 0;
    unsigned dictSizeBits = 0;
    bool codeTypeWriteFinished = false;

    // private functions
    void resetDictionary()
    {
        dict.clear();
        constexpr unsigned minc = std::numeric_limits<uint8_t>::min();
        constexpr unsigned maxc = std::numeric_limits<uint8_t>::max();
        static_assert(minc == 0, "");
        for(unsigned i=minc; i<=maxc; i++)
        {
            CodeType cur_code = static_cast<CodeType>(i);//CodeType cur_code = dict.size();
            dict[{INVALID_CODETYPE, i}] = cur_code;
        }
        dictSizeBits = 9;
    }
    
    void writeBits(std::uint64_t bits, unsigned bitsSize)
    {
        assert(bitsSize < 64-8);
        bitsBuffer |= (bits << bitsBufferSize);
        bitsBufferSize += bitsSize;
        while(bitsBufferSize >= 8)
        {
            unsigned char val = (bitsBuffer & 0xFFU);
            out.write(reinterpret_cast<char*>(&val), 1); // NOLINT
            bitsBuffer >>= 8U; bitsBufferSize -= 8U;
        }
    }

    /*
    template<unsigned DSP = DICT_SIZE_POW, std::enable_if_t<DSP == 16, bool> = true>
    template<unsigned DSP = DICT_SIZE_POW, std::enable_if_t<DSP != 16, bool> = true>
    */

    void finishCodeTypeWrite()
    {
        codeTypeWriteFinished = true;
        assert(bitsBufferSize < 8);
        if(bitsBufferSize == 0)
        {
            return;
        }
        char val = static_cast<char>(bitsBuffer & 0xFFU);
        out.write(&val, 1);
        bitsBuffer = 0; bitsBufferSize = 0;
    }

    void writeCodeType(CodeType data)
    {
        // TODO: variable bit size
        //if(dictSizeBits <= 12)
        {
            // TODO: dynamic bit size
            //writeBits(data, dictSizeBits);
            writeBits(data, DICT_SIZE_POW);
        }
    }
    
public:

    LZWCompressor(const LZWCompressor&) = delete;
    LZWCompressor& operator= (const LZWCompressor&) = delete;

    LZWCompressor(LZWCompressor&&)  noexcept = default;
    LZWCompressor& operator= (LZWCompressor&&)  noexcept = default;
    
    ~LZWCompressor() override
    {
        if(codeTypeWriteFinished)
        {
            return;
        }
        // да скрием проблемите под килима!
        try 
        {
            finishCodeTypeWrite();
        } catch(...)
        {

        }
    }

    void finish() override
    {
        finishCodeTypeWrite();
    }

    explicit LZWCompressor(std::ostream &_out) : 
        out(_out)
    {}

    void prepare(std::istream &ins, std::size_t read_size) override
    {
        (void) ins;
        (void) read_size;
        // TODO: map of used characters
    }

    void operator()(std::istream &ins, std::size_t read_size) override
    {
        assert(codeTypeWriteFinished == false);

        resetDictionary();
        
        CodeType cur_code = INVALID_CODETYPE;
        std::uint8_t chr; // NOLINT
#pragma GCC unroll 4
        for(std::size_t i=0; i<read_size; i++)
        {
            ins.get(reinterpret_cast<char&>(chr)); // NOLINT
            
            if(dict.size() == DICT_MAX_SIZE)
            {
                resetDictionary();
            }
            
            if(dict.count({cur_code, chr}) == 0)
            {
                // not in the dict, so
                // add it to the dictionary:
                CodeType new_code = static_cast<CodeType>(dict.size());
                dict[{cur_code, chr}] = new_code;
                if((1U << dictSizeBits) - 2U <= new_code + 1U)
                {
                    ++dictSizeBits;
                }
                writeCodeType(cur_code);
                cur_code = chr; // cur_code = dict[{INVALID_CODETYPE, chr}];
            }
            else
            {
                // already in the dict
                cur_code = dict[{cur_code, chr}];
            }
        }

        if(cur_code != INVALID_CODETYPE)
        {
            writeCodeType(cur_code);
        }
    }    

};



template<unsigned DICT_SIZE_POW>
class LZWDecompressor final : public Decompressor
{
private:

    // types:
    static_assert(DICT_SIZE_POW > CHAR_BIT, "Dictionary size is too small");
    static_assert(DICT_SIZE_POW <= 32, "Dictionary size is too large"); // NOLINT
    using CodeType = std::conditional_t<DICT_SIZE_POW <= 16, std::uint16_t, std::uint32_t>;
    using KeyType = std::pair<CodeType, uint8_t>;
    using DictContainer = std::vector<KeyType>;

    // static constatns
    static constexpr std::size_t DICT_SIZE = (1ULL << DICT_SIZE_POW);
    static constexpr unsigned DICT_MAX_SIZE = DICT_SIZE - 1;
    static constexpr CodeType INVALID_CODETYPE = DICT_MAX_SIZE;
    static_assert(DICT_SIZE_POW > CHAR_BIT, "Dictionary size is too small");

    // member variables
    DictContainer dict{};
    std::ostream &out;
    std::uint64_t bitsBuffer = 0;
    unsigned bitsBufferSize = 0;
    unsigned dictSizeBits = 0;
    bool codeTypeReadFinished = false;

    // private functions
    CodeType readBits(unsigned bitsSize, std::istream &ins, std::size_t &read_size)
    {
        assert(bitsSize <= sizeof(CodeType)*CHAR_BIT);
        assert(bitsSize < 64-8 && bitsSize > 0);
        while(bitsBufferSize < bitsSize)
        {
            unsigned char val;
            if(read_size == 0)
            {
                throw std::runtime_error("Archive is corrupted!");
            }
            ins.read(reinterpret_cast<char*>(&val), 1); --read_size; // NOLINT
            bitsBuffer |= (static_cast<std::uint64_t>(val) << bitsBufferSize); bitsBufferSize += 8;
        }
        CodeType res = static_cast<CodeType>(bitsBuffer & ((1ULL << bitsSize) - 1U));
        bitsBuffer >>= bitsSize; bitsBufferSize -= bitsSize;

        return res;
    }

    void finishCodeTypeRead()
    {
        codeTypeReadFinished = true;
        if(bitsBuffer != 0)
        {
            throw std::runtime_error("Archive is corrupted!");
        }
    }

    void resetDictionary()
    {
        dict.clear();
        constexpr unsigned minc = std::numeric_limits<uint8_t>::min();
        constexpr unsigned maxc = std::numeric_limits<uint8_t>::max();
        static_assert(minc == 0, "");
        dict.resize(maxc-minc+1);
        for(unsigned i=minc; i<=maxc; i++)
        {
            CodeType cur_code = static_cast<CodeType>(i); //CodeType cur_code = dict.size();
            dict[cur_code] = {INVALID_CODETYPE, i};
        }
        dictSizeBits = 9;
    }

    void codeTypeToStr(CodeType code, std::vector<std::uint8_t> &res)
    {
        res.clear();
        while(code != INVALID_CODETYPE)
        {
            res.push_back(dict[code].second);
            code = dict[code].first;
        }
        std::reverse(res.begin(), res.end());
    }

    std::uint8_t codeTypeToFirstChar(CodeType code)
    {
        assert(code != INVALID_CODETYPE);
        while(code != INVALID_CODETYPE)
        {
            if(dict[code].first == INVALID_CODETYPE) 
            {
                return dict[code].second;
            }
            code = dict[code].first;
        }
        assert(false); // we should not be here!
        return 0;
    }

    bool readCodeType(std::istream &ins, std::size_t &read_size, CodeType &code)
    {
        if(read_size == 0) 
        {
            return false;
        }
        code = readBits(DICT_SIZE_POW, ins, read_size);
        // TODO: dynamic bit size
        //code = readBits(dictSizeBits, ins, read_size);

        return true;
    }

public:

    LZWDecompressor(const LZWDecompressor&) = delete;
    LZWDecompressor& operator= (const LZWDecompressor&) = delete;

    LZWDecompressor(LZWDecompressor&&)  noexcept = default;
    LZWDecompressor& operator= (LZWDecompressor&&)  noexcept = default;
    
    ~LZWDecompressor() override
    {
        try {
            finishCodeTypeRead();
        } catch(...)
        {

        }
    }

    explicit LZWDecompressor(std::ostream &_out) : 
        out(_out)
    {
        dict.reserve(DICT_SIZE);
    }

    void finish() override
    {
        finishCodeTypeRead();
    }

    void operator()(std::istream &ins, std::size_t read_size) override
    {
        resetDictionary();
        
        CodeType cur_code; // NOLINT
        CodeType prev_code = INVALID_CODETYPE;
        std::vector<std::uint8_t> tmps;
        tmps.reserve(64); // NOLINT
        while(readCodeType(ins, read_size, cur_code))
        {
            //ins.get(reinterpret_cast<char&>(chr)); // NOLINT
            
            if(dict.size() == DICT_MAX_SIZE)
            {
                resetDictionary();
            }
            
            if(cur_code > dict.size())
            {
                throw std::runtime_error("Archive is corrupted");
            }

            if(cur_code == dict.size())
            {
                uint8_t lchr = codeTypeToFirstChar(prev_code);
                dict.push_back({prev_code, lchr});
                if(dict.size() >= (1U << dictSizeBits) - 2U)
                {
                    ++dictSizeBits;
                }
                codeTypeToStr(cur_code, tmps);
            }
            else // cur_code < dict.size()
            {
                codeTypeToStr(cur_code, tmps);
                if(prev_code != INVALID_CODETYPE)
                {
                    dict.push_back({prev_code, tmps.front()});
                    if(dict.size() >= (1U << dictSizeBits) - 2U)
                    {
                        ++dictSizeBits;
                    }
                }
            }

            out.write(reinterpret_cast<const char*>(tmps.data()),  // NOLINT
                      static_cast<std::streamsize>(tmps.size())); 
            prev_code = cur_code;
        }
    }

};


std::unique_ptr<Compressor> makeLZWCompressor(unsigned dictSize, std::ostream &out);
std::unique_ptr<Decompressor> makeLZWDecompressor(unsigned dictSize, std::ostream &out);
