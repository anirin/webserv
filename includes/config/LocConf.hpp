#pragma once

#include <cstring>
#include <iostream>
#include <map>

#include "BaseConf.hpp"

class LocConf : public BaseConf
{
private:
	LocationType _type;
	std::string _path;
	std::vector<std::string> _limit_except;
	std::vector<std::string> _return;
	bool _autoindex;
	std::vector<std::string> _index;
	std::string _root;
	size_t _client_max_body_size;
	std::vector<LocConf> _locations;

	// handler
	typedef void (LocConf::*handler_directive_t)(std::vector<std::string>);
	std::map<std::string, handler_directive_t> _handler_directive;

public:
	LocConf();
	LocConf(std::string content, std::string path, LocationType type);
	~LocConf();

	virtual void param(std::string content);

	// set methods
	void set_limit_except(std::vector<std::string> tokens);
	void set_return(std::vector<std::string> tokens);
	void set_autoindex(std::vector<std::string> tokens);
	void set_index(std::vector<std::string> tokens);
	void set_root(std::vector<std::string> tokens);
	void set_client_max_body_size(std::vector<std::string> tokens);
	void handle_location_block(std::vector<std::string> tokens);

	// getter
	std::string get_path();
	LocationType get_type();

	// get conf_value_t
	void getConfValue(std::string path, conf_value_t &conf_value);

	// debug
	void debug_print();
};
