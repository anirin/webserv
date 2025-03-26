#pragma once

#include <map>
#include <sstream>

#include "BaseConf.hpp"
#include "ServConf.hpp"

class MainConf : public BaseConf
{
private:
	// server
	std::vector<ServConf> _servers;

	// handler
	typedef void (MainConf::*handler_directive_t)(std::vector<std::string>);
	std::map<std::string, handler_directive_t> _handler_directive;

public:
	MainConf();
	MainConf(std::string content);
	~MainConf();
	MainConf& operator=(const MainConf& other);

	// setter
	virtual void param(std::string conf_content);

	// getter
	conf_value_t getConfValue(std::string port, std::string server_name, std::string path); // throw
	std::vector<std::pair<std::string, int> > get_listens(); // throw

	// handler
	void handle_server_block(std::vector<std::string> tokens);

	// debug
	void debug_print();
	void debug_print_conf_value(conf_value_t conf_value);
};
