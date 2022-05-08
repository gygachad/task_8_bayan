#pragma once

#include "stdafx.h"

using namespace std;

class walker_hash
{
protected:
    vector<char> m_hash = { 0 };

public:
    virtual vector<char> calculate(const char* buffer, size_t buffer_size) = 0;
};

class md5_walker_hash : public walker_hash
{

public:
    md5_walker_hash();

    vector<char> calculate(const char* buffer, size_t buffer_size) override;
};

class crc32_walker_hash : public walker_hash
{
    uint32_t m_table[256];

    void generate_table();

public:
    crc32_walker_hash();
    vector<char> calculate(const char* buffer, size_t buffer_size) override;
};
