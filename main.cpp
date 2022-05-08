#include "stdafx.h"

namespace po = boost::program_options;
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

    walker_file(const string& f_name,
        const size_t block_size,
        shared_ptr<walker_hash> hash_alg) : m_file_name(f_name), m_block_size(block_size), m_hash_alg(hash_alg)
    {
        //Check - if file opened
        m_ifs.open(f_name, m_ifs.binary);

        //Get file size
        m_ifs.seekg(0, m_ifs.end);
        m_file_size = m_ifs.tellg();
        m_ifs.seekg(0, m_ifs.beg);
        m_ifs.close();

        m_file_size_in_block = m_file_size / m_block_size;
        //If file size is not a multiple of block size - add 1
        m_file_size_in_block += m_file_size % m_block_size == 0 ? 0 : 1;
    };

    walker_file(walker_file& other)
    {
        m_hash_alg = other.m_hash_alg;
        m_block_size = other.m_block_size;
        m_file_name = other.m_file_name;
        m_file_size = other.m_file_size;
        m_file_size_in_block = other.m_file_size_in_block;
    }

    walker_file(walker_file&& other) noexcept
    {
        m_hash_alg = other.m_hash_alg;
        m_block_size = other.m_block_size;
        m_file_name = other.m_file_name;
        m_file_size = other.m_file_size;
        m_file_size_in_block = other.m_file_size_in_block;
    }

    ~walker_file()
    {
        if (m_ifs.is_open())
            m_ifs.close();
    }

    friend bool operator==(walker_file& lhs, walker_file& rhs)
    {
        if (lhs.m_file_size_in_block != rhs.m_file_size_in_block)
            return false;

        if (lhs.m_file_size == -1 || rhs.m_file_size == -1)
            return false;

        if (!lhs.m_ifs.is_open())
        {
            lhs.m_ifs.open(lhs.m_file_name, lhs.m_ifs.binary);

            if (!lhs.m_ifs.is_open())
                return false;
        }

        if (!rhs.m_ifs.is_open())
        {
            rhs.m_ifs.open(rhs.m_file_name, rhs.m_ifs.binary);

            if (!lhs.m_ifs.is_open())
                return false;
        }

        char* tmp_buf = new char[lhs.m_block_size];

        bool status = true;

        for (size_t i = 0; i < lhs.m_file_size_in_block; i++)
        {
            if (!lhs.m_hash_map.contains(i))
            {
                memset(tmp_buf, 0x0, lhs.m_block_size);
                lhs.m_ifs.read(tmp_buf, lhs.m_block_size);
                vector<char> cur_hash = lhs.m_hash_alg->calculate(tmp_buf, lhs.m_block_size);
                lhs.m_hash_map[i] = cur_hash;
            }

            if (!rhs.m_hash_map.contains(i))
            {
                memset(tmp_buf, 0x0, rhs.m_block_size);
                rhs.m_ifs.read(tmp_buf, rhs.m_block_size);
                vector<char> cur_hash = rhs.m_hash_alg->calculate(tmp_buf, rhs.m_block_size);
                rhs.m_hash_map[i] = cur_hash;
            }

            if (lhs.m_hash_map[i] != rhs.m_hash_map[i])
            {
                status = false;
                break;
            }
        }
        
        if (lhs.m_ifs.is_open())
            lhs.m_ifs.close();

        if (rhs.m_ifs.is_open())
            rhs.m_ifs.close();

        delete[] tmp_buf;
        return status;
    }
};

class johnny_walker
{
    logger m_log_inst;

    size_t m_level = 0;

    bool apply_mask(const string& file_name, const string& file_mask)
    {
        auto file_it = file_name.cbegin();
        auto file_temp_it = file_name.cbegin();

        auto mask_it = file_mask.cbegin();
        auto mask_temp_it = file_mask.cbegin();

        while (1)
        {
            //We at the and of mask
            if (mask_it == file_mask.cend())
            {
                //File name end too?
                if (file_temp_it == file_name.cend())
                    return true;
                else
                    return false;//Nope - we have some more symbols
            }

            if (*mask_it == '*')
            {
                mask_it++;
                //* - last mask character
                if (mask_it == file_mask.cend())
                    return true;

                mask_temp_it = mask_it;
            }

            if (file_temp_it == file_name.cend())
                return false;

            if (*mask_it == *file_temp_it || *mask_it == '?')
            {
                mask_it++;
                file_temp_it++;
            }
            else
            {
                //First iteration failed - skip
                if (mask_it == file_mask.cbegin())
                    return false;

                file_it++;
                file_temp_it = file_it;
                mask_it = mask_temp_it;
            }
        }

        return false;
    }

    bool rwalk(const string& dir,
        const vector<string>& exclude_dirs,
        const size_t level,
        const function<void(size_t, const filesystem::path&)>& file_predicat = nullptr)
    {
        if (level)
        {
            m_level++;

            //check level
            if (m_level > level)
                return false;
        }

        m_log_inst << "================================================" << "\r\n";
        m_log_inst << "walk (" << dir << ")" << "\r\n";

        error_code ec;

        for (const auto& d_it : filesystem::directory_iterator{ dir, ec })
        {
            try
            {
                if (filesystem::is_regular_file(d_it.status()))
                {
                    size_t f_size = filesystem::file_size(d_it);
                    string f_path = d_it.path().string();

                    //Call predicat if we have
                    if (file_predicat)
                        file_predicat(f_size, d_it.path());
                }
                else if (filesystem::is_directory(d_it.status()))
                {
                    bool skip = false;

                    //check exclude dir
                    for (auto exclude_d : exclude_dirs)
                    {
                        if (exclude_d == d_it.path().filename())
                        {
                            m_log_inst << "================================================" << "\r\n";
                            m_log_inst << "skip (" << d_it.path().string() << ")\r\n";
                            skip = true;
                            break;
                        }
                    }

                    //Skip this folder - go to the next
                    if (skip)
                        continue;

                    //We need to go deeper�
                    rwalk(d_it.path().string(), exclude_dirs, level, file_predicat);
                }
            }
            catch (const std::exception& e)
            {
                m_log_inst << e.what() << "\r\n";
                continue;
            }
        }

        return true;
    }

public:
    johnny_walker() {};
    ~johnny_walker() {};

    void set_verbose_out(bool verb)
    {
        if (verb)
            m_log_inst.enable();
        else
            m_log_inst.disable();
    };

    using walker_file_map = map<size_t, list<shared_ptr<walker_file>>>;

    walker_file_map get_scoped_files(   const vector<string>& dirs,
                                        const vector<string>& exclude_dirs,
                                        const size_t level,
                                        const string& file_mask,
                                        const size_t min_file_size,
                                        shared_ptr<walker_hash> hash_alg,
                                        const size_t block_size)
    {
        walker_file_map scoped_files;

        m_log_inst << "================================================" << "\r\n";

        for (const auto& d : dirs)
        {
            //Walk recursively for every file
            //but exclude dirs nad check for level
            rwalk(d, exclude_dirs, level,
                [=, &scoped_files](size_t f_size, const filesystem::path& f_path)
                {
                    //check file size
                    if (f_size < min_file_size)
                    {
                        m_log_inst << " skiped by size" << "\t\t";
                    }//check mask
                    else if (!apply_mask(f_path.filename().string(), file_mask))
                    {
                        m_log_inst << " skiped by mask" << "\t\t";
                    }
                    else
                    {
                        m_log_inst << " add to list" << "\t\t";
                        
                        walker_file wlkr_file(f_path.string(), block_size, hash_alg);
                        scoped_files[f_size].push_back(make_shared<walker_file>(wlkr_file));
                    }
                    m_log_inst << " [" << f_size << "bytes]\t" << f_path << "\r\n";
                }
            );
        }

        return scoped_files;
    }

    /*
    void rget_duplicates(   const list<shared_ptr<walker_file>>& wlkr_file_list,
                            list<list<string>>& root,
                            shared_ptr<walker_hash> hash_alg,
                            char* tmp_buf)
    {
        map<vector<char>, list<shared_ptr<walker_file>>> wlkr_file_map;

        list<string> duplicate;
        bool file_end = false;

        //Calculate hash for current block for every file
        for (auto wlkr_file : wlkr_file_list)
        {
            //Check - is opened
            if (!wlkr_file->m_ifs.is_open())
                continue;

            //if (wlkr_file->m_ifs.eof())
            //    return;

            streamoff sp = wlkr_file->m_ifs.tellg();
            if (sp == wlkr_file->m_file_size || sp == -1)//-1 - error?
            {
                duplicate.push_back(wlkr_file->m_file_name);
                file_end = true;
                continue;
            }

            wlkr_file->m_ifs.read(tmp_buf, wlkr_file->m_block_size);
            //wlkr_file->m_ifs.seekg(wlkr_file->m_block_size, wlkr_file->m_ifs.cur);//Seek to next block
            vector<char> cur_hash = hash_alg->calculate(tmp_buf, wlkr_file->m_block_size);
            memset(tmp_buf, 0x0, wlkr_file->m_block_size);

            wlkr_file_map[cur_hash].push_back(wlkr_file);
        }

        if (file_end)
        {
            root.push_back(duplicate);
            return;
        }

        //Go deeper
        for (auto wlkr_file : wlkr_file_map)
        {
            vector<char> cur_hash;
            list<shared_ptr<walker_file>> next_wlkr_file_list;

            tie(cur_hash, next_wlkr_file_list) = wlkr_file;

            //Start calculate hash for next block
            if(next_wlkr_file_list.size() != 1)
                rget_duplicates(next_wlkr_file_list, root, hash_alg, tmp_buf);
        }
    }
    */

    list<list<string>> get_duplicates(const walker_file_map& scoped_files)
    {
        list<list<string>> result;

        //char* tmp_buf = new char[block_size];
        //memset(tmp_buf, 0x0, block_size);

        size_t scoped_files_count = scoped_files.size();
        size_t processed_files_count = 0;

        m_log_inst << "================================================\r\n";

        for (auto& files : scoped_files)
        {
            processed_files_count++;

            m_log_inst << "Processed " << processed_files_count << "\\" << scoped_files_count
                << " " << processed_files_count * 100 / scoped_files_count << "%" << "\r";

            size_t file_size;
            list<shared_ptr<walker_file>> wlkr_file_list;
            tie(file_size, wlkr_file_list) = files;

            //And what we must do with that???
            if (file_size == 0)
                continue;

            //File with size (file_size) one in list
            //Files with different size are different
            //Captian Obvious
            if (wlkr_file_list.size() == 1)
                continue;

            list<shared_ptr<walker_file>> wlk_tmp;
            list<string> file_name_tmp;

            //Search for duplicates
            while (wlkr_file_list.size() != 0)
            {
                wlk_tmp.clear();
                file_name_tmp.clear();

                auto prev_wlkr_file = wlkr_file_list.cbegin();

                wlk_tmp.push_back(*prev_wlkr_file);
                file_name_tmp.push_back((*prev_wlkr_file)->m_file_name);

                //Compare every file with each other
                for (auto wlkr_file = wlkr_file_list.cbegin(); wlkr_file != wlkr_file_list.cend(); wlkr_file++)
                {
                    //compare iterator's
                    if (prev_wlkr_file == wlkr_file)
                        continue;

                    //compare walker files
                    //* - iterator
                    //* - shared_ptr
                    if (**prev_wlkr_file == **wlkr_file)
                    {
                        wlk_tmp.push_back(*wlkr_file);
                        file_name_tmp.push_back((*wlkr_file)->m_file_name);
                    }
                }

                if (file_name_tmp.size() > 1)
                    result.push_back(file_name_tmp);

                for (auto wlkr_tmp_file : wlk_tmp)
                    wlkr_file_list.remove(wlkr_tmp_file);

            }
            //else case
            //rget_duplicates(wlkr_file_list, result, hash_alg, tmp_buf);
        }

        //delete[] tmp_buf;

        return result;
    }
};

void set_scan_dir(const vector<string>& dirs)
{
    for (auto& d : dirs)
    {
        filesystem::directory_entry dir = filesystem::directory_entry(d);
        if (!dir.exists())
        {
            string err = "Directory does not exist: " + d;
            throw runtime_error::exception(err.c_str());
        }
    }
}

int main(int argc, const char* argv[])
{
    map<string, shared_ptr<walker_hash>> hash_map = { {"crc32", make_shared<crc32_walker_hash>()},
                                                        {"md5", make_shared<md5_walker_hash>()} };

    po::variables_map vm;

    try
    {
        po::options_description desc{ "Options" };
        desc.add_options()
            ("help,h", "This message")
            ("verbose,v", "Verbose output")
            ("scan_dir,d", po::value<vector<string>>()->notifier(set_scan_dir), "scan directory list")
            ("exclude_dir,e", po::value<vector<string>>()->default_value(vector<string>(), ""), "exclude directory list")
            ("scan_lvl,l", po::value<size_t>()->default_value(0), "scan level")
            ("hash,a", po::value<string>()->default_value("crc32"), "hash algorithm [crc32, md5]")
            ("mask,m", po::value<string>()->default_value("*"), "file mask")
            ("file_size,s", po::value<size_t>()->default_value(1), "minimal file size")
            ("block_size,b", po::value<size_t>()->default_value(1000), "read block size");

        po::store(parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help"))
        {
            cout << desc << "\r\n";;
            return 0;
        }

        if (vm.count("scan_dir"))
        {
            for (auto d : vm["scan_dir"].as<vector<string>>())
                cout << "scan_dir " << d << "\r\n";
        }
        else
        {
            string err = "Scan dir empty";
            throw runtime_error::exception(err.c_str());
        }

        if (!hash_map.contains(vm["hash"].as<string>()))
        {
            string err = "Unknown hash alg " + vm["hash"].as<string>();
            throw runtime_error::exception(err.c_str());
        }

        if (vm.count("exclude_dir"))
            for (auto e : vm["exclude_dir"].as<vector<string>>())
                cout << "exclude_dir " << e << "\r\n";
        if (vm.count("level"))
            cout << "level: " << vm["scan_lvl"].as<size_t>() << "\r\n";
        if (vm.count("hash"))
            cout << "hash: " << vm["hash"].as<string>() << "\r\n";
        if (vm.count("mask"))
            cout << "mask: " << vm["mask"].as<string>() << "\r\n";
        if (vm.count("file_size"))
            cout << "file_size: " << vm["file_size"].as<size_t>() << "\r\n";
        if (vm.count("block_size"))
            cout << "block_size: " << vm["block_size"].as<size_t>() << "\r\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    johnny_walker wlkr;

    if (vm.count("verbose"))
        wlkr.set_verbose_out(true);

    //Get scoped files for directory
    //map with walker_files. Key - file size
    johnny_walker::walker_file_map scoped_files = wlkr.get_scoped_files(vm["scan_dir"].as<vector<string>>(),
        vm["exclude_dir"].as<vector<string>>(),
        vm["scan_lvl"].as<size_t>(),
        vm["mask"].as<string>(),
        vm["file_size"].as<size_t>(),
        hash_map[vm["hash"].as<string>()],
        vm["block_size"].as<size_t>());

    //Get duplicate list
    list<list<string>> diplicate_list = wlkr.get_duplicates(scoped_files);

    size_t counter = 1;

    for (auto& dl : diplicate_list)
    {
        cout << "=====Duplicate files group " << counter << "=====" << endl;

        for (auto& df : dl)
        {
            cout << df << endl;
        }

        counter++;
    }

    return 0;
}
