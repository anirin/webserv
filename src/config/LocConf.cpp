#include "LocConf.hpp"

// Constructor
LocConf::LocConf() : _path("") {}

LocConf::LocConf(std::string content, std::string path, LocationType type) : _type(type), _path(path) {
	// init
	_locations.clear();
	_limit_except.clear();
	_autoindex = false;
	_index.clear();
	_root.clear();
	_client_max_body_size = 1024 * 1024; // default 1MB

	// init handler directive
	_handler_directive["limit_except"] = &LocConf::set_limit_except;
	_handler_directive["return"] = &LocConf::set_return;
	_handler_directive["autoindex"] = &LocConf::set_autoindex;
	_handler_directive["index"] = &LocConf::set_index;
	_handler_directive["root"] = &LocConf::set_root;
	_handler_directive["client_max_body_size"] = &LocConf::set_client_max_body_size;
	_handler_directive["location"] = &LocConf::handle_location_block;

	param(content);
}

// Destructor
LocConf::~LocConf() {}

// Setter
void LocConf::param(std::string content) {
	size_t pos = 0;

	while(1) {
		std::vector<std::string> tokens;

		try {
			int result = BaseConf::parse_token(content, tokens, pos);
			if(result == CONF_ERROR) {
				throw std::runtime_error("token error");
			}
			if(result == CONF_EOF) {
				break;
			}
		} catch(std::runtime_error &e) { throw std::runtime_error("token error"); }

		if(tokens.empty()) {
			continue;
		}

		if(_handler_directive.find(tokens[0]) != _handler_directive.end()) {
			(this->*_handler_directive[tokens[0]])(tokens);
		} else {
			throw std::runtime_error("unknown directive");
		}
	}
}

void LocConf::handle_location_block(std::vector<std::string> tokens) {
	// tokens[0] = location
	// tokens[1] = path
	// tokens[2] = { ... }
	int token_size = tokens.size();
	LocationType type;
	type = NON;

	if(token_size < 3 || token_size > 4) {
		throw std::runtime_error("handle_location_block args required three or four");
	}

	if(token_size == 4) {
		if(tokens[1] == "=")
			type = EQUAL;
		else if(tokens[1] == "~")
			type = TILDE;
		else if(tokens[1] == "~*")
			type = TILDE_STAR;
		else if(tokens[1] == "^~")
			type = CARET_TILDE;
		else
			throw std::runtime_error("[LocConf] location block type is invalid");
	}

	// trim { }
	tokens[token_size - 1].erase(0, 1);
	tokens[token_size - 1].erase(tokens[token_size - 1].size() - 1, 1);

	// location block, path
	_locations.push_back(LocConf(tokens[token_size - 1], tokens[token_size - 2], type));
}

void LocConf::set_limit_except(std::vector<std::string> tokens) {
	if(_limit_except.size() > 0) {
		throw std::runtime_error("limit_except already set");
	}
	if(tokens.size() < 2) {
		throw std::runtime_error("limit_except syntax error");
	}

	size_t i = 1;
	while(tokens.size() > i) {
		std::string s = tokens[i];
		for(std::size_t i = 0; i < s.length(); i++) {
			s[i] = std::toupper(static_cast<unsigned char>(s[i]));
		}
		if(s == "GET" || s == "POST" || s == "DELETE" || s == "PUT")
			_limit_except.push_back(s);
		else {
			std::cout << "limit_except: " << s << std::endl;
			throw std::runtime_error("limit_except syntax error");
		}
		i++;
	}
}

void LocConf::set_return(std::vector<std::string> tokens) {
	if(_return.second.size() > 0) {
		throw std::runtime_error("return already set");
	}

	if(tokens.size() != 3) {
		throw std::runtime_error("return syntax error");
	}

	// std::pair<int, std::string> _return;
	// first token は 200 <= x <= 599
	int status_code;
	try {
		status_code = my_stoul(tokens[1]);
	} catch(std::invalid_argument &e) {
		throw std::runtime_error("return status code is invalid");
	} catch(std::out_of_range &e) { throw std::runtime_error("return status code is invalid"); }

	if(status_code < 200 || status_code > 599) {
		throw std::runtime_error("return status code is invalid");
	}

	if(tokens[2].find("http://") == 0 || tokens[2].find("https://") == 0) {
		_return = std::make_pair(status_code, tokens[2]);
	} else {
		throw std::runtime_error("return url is invalid");
	}
}

void LocConf::set_autoindex(std::vector<std::string> tokens) {
	if(_autoindex) {
		throw std::runtime_error("autoindex already set");
	}
	if(tokens.size() < 2) {
		throw std::runtime_error("autoindex syntax error");
	}

	if(tokens[1] == "on") {
		_autoindex = true;
	} else if(tokens[1] == "off") {
		_autoindex = false;
	} else {
		throw std::runtime_error("autoindex syntax error");
	}
}

void LocConf::set_index(std::vector<std::string> tokens) {
	if(_index.size() > 0) {
		throw std::runtime_error("index already set");
	}
	if(tokens.size() < 2) {
		throw std::runtime_error("index syntax error");
	}

	size_t i = 1;
	while(tokens.size() > i) {
		_index.push_back(tokens[i]);
		i++;
	}
}

void LocConf::set_root(std::vector<std::string> tokens) {
	if(_root.size() > 0) {
		throw std::runtime_error("root already set");
	}
	if(tokens.size() < 2) {
		throw std::runtime_error("root syntax error");
	}

	_root = tokens[1];
}

void LocConf::set_client_max_body_size(std::vector<std::string> tokens) {
	if(_client_max_body_size != 1024 * 1024) {
		throw std::runtime_error("client_max_body_size already set");
	}
	if(tokens.size() < 2) {
		throw std::runtime_error("client_max_body_size syntax error");
	}

	if(tokens[1].find("k") != std::string::npos || tokens[1].find("K") != std::string::npos) {
		tokens[1].erase(tokens[1].size() - 1, 1);
		try {
			_client_max_body_size = my_stoul(tokens[1]) * 1024;
		} catch(std::exception &e) { throw std::runtime_error("client_max_body_size syntax error"); }
	} else if(tokens[1].find("m") != std::string::npos || tokens[1].find("M") != std::string::npos) {
		tokens[1].erase(tokens[1].size() - 1, 1);
		try {
			_client_max_body_size = my_stoul(tokens[1]) * 1024 * 1024;
		} catch(std::exception &e) { throw std::runtime_error("client_max_body_size syntax error"); }
	} else {
		try {
			_client_max_body_size = my_stoul(tokens[1]);
		} catch(std::exception &e) { throw std::runtime_error("client_max_body_size syntax error"); }
	}
}

// Getter
std::string LocConf::get_path() {
	return _path;
}

LocationType LocConf::get_type() {
	return _type;
}

// Get conf_value_t
LocConf get_location(std::string path, std::vector<LocConf> locations) {
	LocConf locConf;

	for(size_t i = 0; i < locations.size(); i++) {
		if(path.find(locations[i].get_path()) == 0) {
			locConf = locations[i];
		}
	}

	return locConf;
}

void LocConf::getConfValue(std::string path, conf_value_t &conf_value) {
	conf_value._path = _path;
	if(_limit_except.size() > 0)
		conf_value._limit_except = _limit_except;
	if(_return.second.size() > 0)
		conf_value._return = _return;
	conf_value._autoindex = _autoindex;
	if(_index.size() > 0)
		conf_value._index = _index;
	if(_root.size() > 0)
		conf_value._root = _root;
	if(_client_max_body_size != 1024 * 1024)
		conf_value._client_max_body_size = _client_max_body_size;

	// std::cout << "loc get conf ok" << std::endl;

	LocConf locConf = get_location(path, _locations);
	if(locConf.get_path().empty()) {
		return;
	}
	for(size_t i = 0; i < _locations.size(); i++) {
		if(_locations[i].get_path() == path) {
			_locations[i].getConfValue(path, conf_value);
		}
	}

	return;
}

// debug
void LocConf::debug_print() {
	std::cout << std::endl;
	std::cout << "=========================== location block:" << std::endl;
	std::cout << "path: " << _path << std::endl;
	std::cout << "limit_except: ";
	if(_return.first != 0 && _return.second != "") {
		std::cout << "return: " << _return.first << " : " << _return.second << std::endl;
	}
	std::cout << std::endl;
	std::cout << "autoindex: " << _autoindex << std::endl;
	std::cout << "index: ";
	for(size_t i = 0; i < _index.size(); i++) {
		std::cout << _index[i] << " ";
	}
	std::cout << std::endl;
	std::cout << "root: " << _root << std::endl;
	std::cout << "client_max_body_size: " << _client_max_body_size << std::endl;
	std::cout << "locations: " << _locations.size() << std::endl;
	for(size_t i = 0; i < _locations.size(); i++) {
		std::cout << "location " << i << ":" << std::endl;
		_locations[i].debug_print();
	}
	std::cout << "=================================" << std::endl;
	std::cout << std::endl << std::endl;
}