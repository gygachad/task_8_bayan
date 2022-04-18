#include <iostream>
#include <boost/program_options.hpp>
#include <boost/uuid/detail/md5.hpp>
#include <boost/crc.hpp>
#include <boost/regex/v5/regex.hpp>
#include <boost/regex/v5/regex_match.hpp>
#include <filesystem>
#include <list>
#include <map>

#include "version.h"

namespace po = boost::program_options;
using namespace std;

enum class walker_hash_e { crc32, md5 };

struct walker_hash
{
    map<string, walker_hash_e> m_hash_map =    {    {"crc32", walker_hash_e::crc32},
                                                    {"md5", walker_hash_e::md5} };
    void calculate(walker_hash_e hash_type) 
    {
        //switch(hash_type)
    };

    //operator ==
    //operator!=
};

struct walker_options
{    

    list<filesystem::path> m_dirs;
    list<filesystem::path> m_exclude_dirs;
    walker_hash_e m_hash_alg;
    const size_t m_level;
    const string& m_file_mask;
    const size_t m_min_file_size;
    const size_t m_block_size;

    bool m_mask_start_with = false;
    bool m_mask_end_with = false;
    vector<string> m_mask_vector;

    walker_options() = delete;

    walker_options( const vector<string>& dirs,
                    const vector<string>& exclude_dirs,
                    const size_t level,
                    const string& hash_alg,
                    const string& file_mask,
                    const size_t min_file_size,
                    const size_t block_size) :
                    m_level{ level },
                    m_hash_alg{ walker_hash_e::crc32 },
                    m_file_mask{ file_mask },
                    m_min_file_size{ min_file_size },
                    m_block_size{ block_size }
    {
        for (auto d : dirs)
        {
            filesystem::path p = filesystem::path(d);
            if (filesystem::is_directory(p))
                m_dirs.push_back(p);
        }

        for (auto d : exclude_dirs)
        {
            filesystem::path p = filesystem::path(d);
            if (filesystem::is_directory(p))
                m_exclude_dirs.push_back(p);
        }

        walker_hash wh;
        if(wh.m_hash_map.contains(hash_alg))
            m_hash_alg = wh.m_hash_map[hash_alg];

        m_mask_vector = split(m_file_mask, '*');
        if (m_file_mask[0] == '*')
            m_mask_start_with = true;
        if (m_file_mask[m_file_mask.length() - 1] == '*')
            m_mask_end_with = true;
    }

    // ("",  '.') -> [""]
    // ("11", '.') -> ["11"]
    // ("..", '.') -> ["", "", ""]
    // ("11.", '.') -> ["11", ""]
    // (".11", '.') -> ["", "11"]
    // ("11.22", '.') -> ["11", "22"]
    vector<std::string> split(const string& str, char d)
    {
        vector<std::string> r;

        string::size_type start = 0;
        string::size_type stop = str.find_first_of(d);
        while (stop != string::npos)
        {
            string sub = str.substr(start, stop - start);
            if(sub != "")
                r.push_back(sub);

            start = stop + 1;
            stop = str.find_first_of(d, start);
        }

        string sub = str.substr(start);

        if (sub != "")
            r.push_back(sub);

        return r;
    }
};


class johnny_walker
{
    const walker_options& m_options;
    map<size_t, list<string>> m_scan_files;

    bool apply_mask(const string& file_name)
    {
        size_t start = 0;
        size_t counter = 0;

        if (m_options.m_mask_vector.size() == 0)
            return true;


        //check file mask
        for (auto sub_mask : m_options.m_mask_vector)
        {
            size_t offset = file_name.find(sub_mask, start);

            if (offset == string::npos)
                return false;

            //mask not start with * - file_name must conatin sub_mask at start
            if (!m_options.m_mask_start_with && counter == 0)
            {
                if (offset != start)
                    return false;
            }
            else
            {
                if (offset == start)
                    return false;
            }

            if (counter == m_options.m_mask_vector.size() - 1)
            {
                //mask end with * - after last symbol must be another symbols
                if (m_options.m_mask_end_with)
                {
                    if (offset + sub_mask.length() == file_name.length())
                        return false;
                    else
                        return true;
                }
                else
                {
                    if (offset + sub_mask.length() != file_name.length())
                        return false;
                    else
                        return true;
                }
            }

            start = offset + sub_mask.length();
            counter++;
        }

        return true;
    }

public:
    johnny_walker(const walker_options& options) : m_options{ options } {};
    ~johnny_walker() {};

    void walk(const filesystem::path& p)
    {
        cout << "walk (" << p.filename() << ")" << endl;

        for (const auto& d_it : filesystem::directory_iterator{ p })
        {
            if (filesystem::is_regular_file(d_it.status()))
            {
                size_t f_size = filesystem::file_size(d_it);
                string f_path = d_it.path().string();

                cout << "  " << f_path << " [" << f_size << " bytes] ";

                if (!apply_mask(d_it.path().filename().string()))
                {
                    cout << " skiped by mask" << endl;
                    continue;
                }
                
                /*
                boost::basic_regex my_filter(".*");
                boost::smatch what;

                // Skip if no match for V2:
                if (!boost::regex_match(e.path().string(), my_filter))
                {
                    cout << "skiped by mask" << endl;
                    continue;
                }
                */
                // For V3:
                //if( !boost::regex_match( i->path().filename().string(), what, my_filter ) ) continue;
                
                //check file size
                if (f_size < m_options.m_min_file_size)
                {
                    cout << " skiped by size" << endl;
                    continue;
                }
                cout << " add to list" << endl;
                m_scan_files[f_size].push_back(f_path);
            }
            else if (filesystem::is_directory(d_it.status()))
            {
                //check exclude dir
                walk(d_it.path());
            }
        }
    }
};

void set_scan_dir(const vector<string>& dirs) 
{
    for (auto d : dirs)
    {
        filesystem::directory_entry dir = filesystem::directory_entry(d);
        if (!dir.exists())
        {
            string err = "Directory does not exist: " + d;
            throw runtime_error::exception(err.c_str());
        }
    }
}

void set_hash(string hash)
{
    
    walker_hash wh;
    //check - if hash in list
    if (!wh.m_hash_map.contains(hash))
    {
        string err = "Unknown hash alg " + hash;
        throw runtime_error::exception(err.c_str());
    }
}

int main(int argc, const char* argv[])
{
    po::variables_map vm;

    try 
    {
        po::options_description desc{ "Options" };
        desc.add_options()
            ("help,h", "This message")
            ("scan_dir,d", po::value<vector<string>>()->notifier(set_scan_dir), "scan directory list")
            ("exclude_dir,e", po::value<vector<string>>()->default_value(vector<string>(), ""), "exclude directory list")
            ("scan_lvl,l", po::value<size_t>()->default_value(0), "scan level")
            ("hash,a", po::value<string>()->default_value("crc32")->notifier(set_hash), "hash algorithm [crc32, md5]")
            ("mask,m", po::value<string>()->default_value("*"), "file mask")
            ("file_size,s", po::value<size_t>()->default_value(1), "minimal file size")
            ("block_size,b", po::value<size_t>()->default_value(1000), "read block size");

        po::store(parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help"))
        {
            cout << desc << std::endl;
            return 0;
        }

        if (vm.count("scan_dir"))
        {
            for (auto d : vm["scan_dir"].as<vector<string>>())
                cout << d << endl;
        }
        else
        {
            string err = "Scan dir empty";
            throw runtime_error::exception(err.c_str());
        }
            
        if (vm.count("exclude_dir"))
            for (auto e : vm["exclude_dir"].as<vector<string>>())
                cout << e << endl;
        if (vm.count("level"))
            cout << "level: " << vm["scan_lvl"].as<size_t>() << std::endl;
        if (vm.count("hash"))
            cout << "hash: " << vm["hash"].as<string>() << std::endl;
        if (vm.count("mask"))
            cout << "mask: " << vm["mask"].as<string>() << std::endl;
        if (vm.count("file_size"))
            cout << "file_size: " << vm["file_size"].as<size_t>() << std::endl;
        if (vm.count("block_size"))
            cout << "block_size: " << vm["block_size"].as<size_t>() << std::endl;
    }
    catch (const std::exception& e) 
    {
        std::cerr << e.what() << std::endl;
    }

    walker_options opt( vm["scan_dir"].as<vector<string>>(),
                        vm["exclude_dir"].as<vector<string>>(),
                        vm["scan_lvl"].as<size_t>(),
                        vm["hash"].as<string>(),
                        vm["mask"].as<string>(),
                        vm["file_size"].as<size_t>(),
                        vm["block_size"].as<size_t>()
                        );

    johnny_walker wlkr(opt);

    for (const auto& d : vm["scan_dir"].as<vector<string>>())
    {
        wlkr.walk(d);
    }

	return 0;
}
