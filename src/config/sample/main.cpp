#include <fstream>

#include "LocConf.hpp"
#include "MainConf.hpp"
#include "ServConf.hpp"

std::string get_file_content(char* filename)
{
    std::ifstream ifs(filename);
    if(!ifs)
    {
        std::cerr << "Error: file not found" << std::endl;
        exit(1);
    }

    std::string content;
    while(!ifs.eof())
    {
        std::string line;
        std::getline(ifs, line);
        content += line + "\n";
    }

    return content;
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
		return 1;
	}

	char *filename = argv[1];

	std::string content = get_file_content(filename);

	// std ::cout << content << std::endl;

	MainConf mainConf;
	try {
		mainConf = MainConf(content);
	} catch(std::exception &e) {
		std::cout << "conf has error" << std::endl;
		std::cerr << e.what() << std::endl;
		return 1;
	}

	// search server && location block
	std::string port = "8080";
	std::string server_name = "localhost";
	std::string path = "/under_construction/index.php";
	// std::string path = "/cgi/";
	conf_value_t conf_value;

	try
	{
		conf_value = mainConf.getConfValue(port, server_name, path);
	}
	catch(std::exception &e)
	{
		std::cout << "get conf has error" << std::endl;
		std::cerr << e.what() << std::endl;
		return 1;
	}

	// debug
	// mainConf.debug_print();
	// mainConf.debug_print_conf_value(conf_value);

	return 0;
}
