#pragma once

using namespace std;

class logger
{
    bool m_enabled = false;

public:
    logger() {};

    void enable() { m_enabled = true; };
    void disable() { m_enabled = false; };

    logger& operator<<(const auto& data)
    {
        if (m_enabled)
            cout << data;

        return *this;
    }
};

struct walker_file
{
    streamoff   m_file_size;
    size_t      m_file_size_in_block;
    string      m_file_name;
    ifstream    m_ifs;
    size_t      m_block_size;

    shared_ptr<walker_hash>     m_hash_alg;
    map<size_t, vector<char>>   m_hash_map;

    walker_file(const string& f_name, const size_t block_size, shared_ptr<walker_hash> hash_alg);
    walker_file(walker_file& other);
    walker_file(walker_file&& other) noexcept;

    ~walker_file();

    friend bool operator==(walker_file& lhs, walker_file& rhs);
};

class johnny_walker
{
    logger m_log_inst;

    size_t m_level = 0;

    bool apply_mask(const string& file_name, const string& file_mask);

    bool rwalk(const string& dir,
        const vector<string>& exclude_dirs,
        const size_t level,
        const function<void(size_t, const filesystem::path&)>& file_predicat = nullptr);

public:
    johnny_walker();
    ~johnny_walker();

    void set_verbose_out(bool verb);

    using walker_file_map = map<size_t, list<shared_ptr<walker_file>>>;

    walker_file_map get_scoped_files(   const vector<string>& dirs,
                                        const vector<string>& exclude_dirs,
                                        const size_t level,
                                        const string& file_mask,
                                        const size_t min_file_size,
                                        shared_ptr<walker_hash> hash_alg,
                                        const size_t block_size);

    list<list<string>> get_duplicates(const walker_file_map& scoped_files);
};