// CRC32 from here:
// https://gist.github.com/timepp/1f678e200d9e0f2a043a9ec6b3690635
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <istream>

class CRC32
{
    std::array<std::uint32_t, 256> prcTable;
    std::uint32_t curCrc = 0;

    void precomputeTable()
    {
        std::uint32_t polynomial = 0xEDB88320; // NOLINT
		for (std::uint32_t i = 0; i < prcTable.size(); i++) 
		{
            std::uint32_t curr = i;
			for (std::size_t j = 0; j < 8; j++)  // NOLINT
            {
				if ((curr & 1U) != 0U) {
					curr = polynomial ^ (curr >> 1U);
				}
				else {
					curr >>= 1U;
				}
			}
			prcTable[i] = curr; // NOLINT
		}
    }

public:

    CRC32() 
    {
        precomputeTable();
    }
    explicit CRC32(std::uint32_t initial) : curCrc(initial)
    {
        precomputeTable();
    }
    
    std::uint32_t getResult() const
    {
        return curCrc;
    }

    void operator() (const std::uint8_t* buf, std::size_t len)
	{
        std::uint32_t newCrc = ~curCrc;
		for (std::size_t i = 0; i < len; ++i) 
		{
			newCrc = prcTable[(newCrc ^ buf[i]) & 0xFFU] ^ (newCrc >> 8U); //NOLINT
		}
        curCrc = ~newCrc;
	}

    template<class T>
    void operator() (const T &data)
    {
        this->operator() (reinterpret_cast<const std::uint8_t*>(&data), sizeof(T)); // NOLINT
    }

    template<class It>
    void operator() (It begin, It end)
    {
        for(; begin != end; ++begin)
        {
            this->operator() (*begin); // NOLINT
        }
    }

    void operator() (std::istream &file, std::size_t file_size)
    {
        constexpr unsigned BUFFER_SIZE = 1024;
        std::array<std::uint8_t, BUFFER_SIZE> buf; // NOLINT
        while(file_size>=BUFFER_SIZE)
        {
            file.read(reinterpret_cast<char*>(buf.data()), buf.size()); // NOLINT
            this->operator() (buf.data(), buf.size());
            file_size -= BUFFER_SIZE;
        }

        file.read(reinterpret_cast<char*>(buf.data()), file_size); // NOLINT
        this->operator() (buf.data(), file_size);
        //file_size -= file_size;
    }
};
