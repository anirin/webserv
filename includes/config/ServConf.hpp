#pragma once

#include <iostream>
#include <map>
#include <string>
#include <sstream>

#include "BaseConf.hpp"
#include "LocConf.hpp"

class ServConf : public BaseConf
{
private:
	// value
	std::pair<std::string, int> _listen;
	std::string _server_name;
	std::map<int, std::string> _error_page;
	size_t _client_max_body_size;
	std::string _root;

	// location
	std::vector<LocConf> _locations;

	// handler
	std::map<std::string, void (ServConf::*)(std::vector<std::string>)> _handler_directive;

public:
	ServConf();
	ServConf(std::string content);
	~ServConf();
	ServConf &operator=(const ServConf &other);

	virtual void param(std::string content);

	// set methods
	void set_listen(std::vector<std::string> tokens);
	void set_server_name(std::vector<std::string> tokens);
	void set_error_page(std::vector<std::string> tokens);
	void set_client_max_body_size(std::vector<std::string> tokens);
	void set_root(std::vector<std::string> tokens);
	void handle_location_block(std::vector<std::string> tokens);

	// gettter
	std::pair<std::string, int> get_listen();
	std::string get_server_name();

	// get conf_value_t
	conf_value_t getConfValue(std::string path);

	// debug
	void debug_print();
};
