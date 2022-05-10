#include "stdafx.h"

namespace po = boost::program_options;
using namespace std;

void set_scan_dir(const vector<string>& dirs)
{
    for (auto& d : dirs)
    {
        filesystem::directory_entry dir = filesystem::directory_entry(d);
        if (!dir.exists())
        {
            string err = "Directory does not exist: " + d;
            throw runtime_error(err.c_str());
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
            throw runtime_error(err.c_str());
        }

        if (!hash_map.contains(vm["hash"].as<string>()))
        {
            string err = "Unknown hash alg " + vm["hash"].as<string>();
            throw runtime_error(err.c_str());
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
