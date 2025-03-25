#include "ServConf.hpp"

// Constructor
ServConf::ServConf(std::string content) {
	// init
	_locations.clear();
	_handler_directive["location"] = &ServConf::handle_location_block;
	_handler_directive["listen"] = &ServConf::set_listen;
	_handler_directive["server_name"] = &ServConf::set_server_name;
	_handler_directive["error_page"] = &ServConf::set_error_page;
	_handler_directive["client_max_body_size"] = &ServConf::set_client_max_body_size;
	_handler_directive["root"] = &ServConf::set_root;

	_server_name.clear();
	_error_page.clear();
	_client_max_body_size = 1024 * 1024; // default 1MB
	_locations.clear();

	param(content);
}

// Destructor
ServConf::~ServConf() {}

// Setter
void ServConf::param(std::string content) {
	size_t pos = 0;

	while(1) {
		std::vector<std::string> tokens;

		try {
			int result = BaseConf::parse_token(content, tokens, pos);
			if(result == CONF_ERROR) {
				throw std::runtime_error("Error: invalid token");
			}
			if(result == CONF_EOF) {
				break;
			}
		} catch(std::runtime_error &e) { throw std::runtime_error("Error: invaid token"); }

		if(tokens.empty()) {
			continue;
		}

		if(_handler_directive.find(tokens[0]) != _handler_directive.end()) {
			(this->*_handler_directive[tokens[0]])(tokens);
		}
	}
}

void ServConf::set_listen(std::vector<std::string> tokens) {
	if(_listen.first != "") {
		throw std::runtime_error("listen is duplicated");
	}
	if(tokens.size() != 2) {
		throw std::runtime_error("listen value required one");
	}

	// default 0.0.0.0:80
	std::string address = "0.0.0.0";
	int port = 80;
	int value;

	std::string::size_type pos = tokens[1].find(":");
	if(pos != std::string::npos) {
		address = tokens[1].substr(0, pos);
		std::stringstream ss(tokens[1].substr(pos + 1));
		ss >> value;
		if(ss.fail())
			throw std::runtime_error("listen port is not number");
		if(ss.eof() == false)
			throw std::runtime_error("listen port require (0~65535)");
		if(port < 0 || port > 65535)
			throw std::runtime_error("listen port require (0~65535)");
		port = value;
	} else {
		std::stringstream ss(tokens[1]);
		ss >> value;
		if(!ss.fail() && (ss.eof() != false)) {
			if(port < 0 || port > 65535)
				throw std::runtime_error("listen port require (0~65535)");
			port = value;
		} else {
			address = tokens[1];
		}
	}
	_listen = std::make_pair(address, port);
}

void ServConf::set_server_name(std::vector<std::string> tokens) {
	if(tokens.size() != 2) {
		throw std::runtime_error("server_name value required one");
	}

	if(_server_name.empty()) {
		_server_name = tokens[1];
	} else {
		throw std::runtime_error("server_name is duplicated");
	}
}

void ServConf::set_error_page(std::vector<std::string> tokens) {
	if(tokens.size() < 2) {
		throw std::runtime_error("error_page args required two or more");
	}
	for(size_t i = 1; i < tokens.size() - 1; i++) {
		std::stringstream ss(tokens[i]);
		int status_code;

		ss >> status_code;
		if(ss.fail())
			throw std::runtime_error("error_page status_code is not number");
		if(ss.eof() == false)
			throw std::runtime_error("error_page status_code require (100~599)");
		if(status_code < 100 || status_code > 599)
			throw std::runtime_error("error_page status_code require (100~599)");
		_error_page[status_code] = tokens[tokens.size() - 1];
	}
}

void ServConf::set_client_max_body_size(std::vector<std::string> tokens) {
	if(_client_max_body_size != 1024 * 1024) {
		throw std::runtime_error("client_max_body_size already set");
	}
	if(tokens.size() < 2) {
		throw std::runtime_error("client_max_body_size syntax error");
	}

	// todo エラーハンドリングの強化
	if(tokens[1].find("k") != std::string::npos) {
		tokens[1].erase(tokens[1].size() - 1, 1);
		_client_max_body_size = my_stoul(tokens[1]) * 1024;
	} else if(tokens[1].find("m") != std::string::npos) {
		tokens[1].erase(tokens[1].size() - 1, 1);
		_client_max_body_size = my_stoul(tokens[1]) * 1024 * 1024;
	} else {
		_client_max_body_size = my_stoul(tokens[1]);
	}
}

void ServConf::set_root(std::vector<std::string> tokens) {
	if(!_root.empty())
		throw std::runtime_error("root is duplicated");
	if(tokens.size() != 2)
		throw std::runtime_error("root value required one");
	_root = tokens[1];
}

// handle block
void ServConf::handle_location_block(std::vector<std::string> tokens) {
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
			throw std::runtime_error("[ServConf] location block type is invalid");
	}

	// trim { }
	tokens[token_size - 1].erase(0, 1);
	tokens[token_size - 1].erase(tokens[token_size - 1].size() - 1, 1);

	// location block, path
	_locations.push_back(LocConf(tokens[token_size - 1], tokens[token_size - 2], type));
}

// Getter
std::pair<std::string, int> ServConf::get_listen() {
	return _listen;
}

std::string ServConf::get_server_name() {
	return _server_name;
}

LocConf select_location(std::string path, std::vector<LocConf> locations) {
	LocConf tmp_conf;
	std::string tmp_path;
	LocationType type;

	LocConf normal_conf;
	size_t normal_max_len = 0;
	LocConf tilde_conf;
	bool tilde_flag = false;
	LocConf caret_tilde_conf;
	size_t caret_max_len = 0;

	for(std::vector<LocConf>::iterator it = locations.begin(); it != locations.end(); it++) {
		tmp_conf = *it;
		tmp_path = tmp_conf.get_path();
		type = tmp_conf.get_type();

		if(type == EQUAL && tmp_path == path) // =
			return tmp_conf;
		else if(tilde_flag == false && type == TILDE) // ~
		{
			// memo ~ \.php$ \と$は未対応
			if(path.find(tmp_path) != std::string::npos) {
				tilde_conf = tmp_conf;
				tilde_flag = true;
				continue;
			}
		} else if(tilde_flag == false && type == TILDE_STAR) // ~*
		{
			std::string big_path = path;
			for(std::string::size_type i = 0; i < tmp_path.length(); ++i) {
				tmp_path[i] = std::toupper(static_cast<unsigned char>(tmp_path[i]));
			}
			for(std::string::size_type i = 0; i < path.length(); ++i) {
				big_path[i] = std::toupper(static_cast<unsigned char>(path[i]));
			}

			if(big_path.find(tmp_path) != std::string::npos) {
				tilde_conf = tmp_conf;
				tilde_flag = true;
				continue;
			}
		}

		std::string dir_path;
		dir_path = path.substr(0, path.find_last_of("/") + 1);

		size_t path_len = tmp_path.length();
		if(type == NON) // 標準
		{
			if(dir_path.find(tmp_path) == 0 && (path_len > normal_max_len)) {
				normal_conf = tmp_conf;
				normal_max_len = path_len;
				continue;
			}
		} else if(type == CARET_TILDE) // ^~
		{
			if(dir_path.find(tmp_path) == 0 && (path_len > caret_max_len)) {
				caret_tilde_conf = tmp_conf;
				caret_max_len = path_len;
				continue;
			}
		}
	}

	if(tilde_flag == true && caret_max_len == 0)
		return tilde_conf;
	else if(tilde_flag == true && caret_max_len != 0)
		return caret_tilde_conf;
	else {
		if(normal_max_len == 0 && caret_max_len == 0)
			throw std::runtime_error("[server] location not found");
		if(normal_max_len >= caret_max_len)
			return normal_conf;
		else
			return caret_tilde_conf;
	}
}

conf_value_t ServConf::getConfValue(std::string path) {
	conf_value_t conf_value;
	LocConf locConf;

	conf_value._listen = _listen;
	conf_value._server_name = _server_name;
	conf_value._error_page = _error_page;
	conf_value._client_max_body_size = _client_max_body_size;
	conf_value._root = _root;
	conf_value._autoindex = false;

	try {
		locConf = select_location(path, _locations);
	} catch(std::runtime_error &e) { throw std::runtime_error("[server] location not found"); }

	locConf.getConfValue(path, conf_value);
	return conf_value;
}

// debug
void ServConf::debug_print() {
	std::cout << "listen: " << _listen.first << ":" << _listen.second << std::endl;
	std::cout << "server_name: " << _server_name << std::endl;
	std::cout << "root: " << _root << std::endl;
	std::cout << "error_page: " << std::endl;
	for(std::map<int, std::string>::iterator it = _error_page.begin(); it != _error_page.end(); it++) {
		std::cout << it->first << " : " << it->second << std::endl;
	}
	std::cout << "client_max_body_size: " << _client_max_body_size << std::endl;

	for(size_t i = 0; i < _locations.size(); i++) {
		_locations[i].debug_print();
	}

	std::cout << std::endl;
}
