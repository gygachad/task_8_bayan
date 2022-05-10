#include "stdafx.h"

using namespace std;

/*md5 Walker hash*/
md5_walker_hash::md5_walker_hash() { m_hash.reserve(16); }

vector<char> md5_walker_hash::calculate(const char* buffer, size_t buffer_size)
{
    m_hash.clear();

    boost::uuids::detail::md5 hash;
    boost::uuids::detail::md5::digest_type digest;

    hash.process_bytes(buffer, buffer_size);
    hash.get_digest(digest);

    const auto charDigest = reinterpret_cast<const char*>(&digest);

    for (size_t i = 0; i < sizeof(digest); i++)
        m_hash.push_back(charDigest[i]);

    return m_hash;
}
/********************/

/*crc32 walker hash*/
crc32_walker_hash::crc32_walker_hash()
{
    generate_table();
    m_hash.reserve(4);
}

void crc32_walker_hash::generate_table()
{
    uint32_t polynomial = 0xEDB88320;
    for (uint32_t i = 0; i < 256; i++)
    {
        uint32_t c = i;
        for (size_t j = 0; j < 8; j++)
        {
            if (c & 1) 
                c = polynomial ^ (c >> 1);
            else
                c >>= 1;
        }
        m_table[i] = c;
    }
}

vector<char> crc32_walker_hash::calculate(const char* buffer, size_t buffer_size)
{
    uint32_t c = 0x12345678;

    const char* u = buffer;
    for (size_t i = 0; i < buffer_size; ++i)
        c = m_table[(c ^ u[i]) & 0xFF] ^ (c >> 8);

    m_hash.clear();

    m_hash.push_back((c >> 24) & 0xFF);
    m_hash.push_back((c >> 16) & 0xFF);
    m_hash.push_back((c >> 8) & 0xFF);
    m_hash.push_back((c) & 0xFF);

    return m_hash;
}
/********************/
