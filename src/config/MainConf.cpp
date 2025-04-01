#include "MainConf.hpp"

// =============================== constructor ===============================

MainConf::MainConf() : _servers() {
	// init
	_servers.clear();
	_handler_directive["server"] = &MainConf::handle_server_block;
}

MainConf::MainConf(std::string conf_content) : _servers() { // throw
	if(conf_content.length() == 0) {
		throw std::runtime_error("MainConf: empty conf_content");
	}

	// init
	_servers.clear();
	_handler_directive["server"] = &MainConf::handle_server_block;

	param(conf_content);
}

MainConf::~MainConf() {}

MainConf &MainConf::operator=(const MainConf &other) {
	if(this == &other) {
		return *this;
	}

	BaseConf::operator=(other);

	_servers = other._servers;
	_handler_directive = other._handler_directive;

	return *this;
}

// =============================== setter ===============================
void MainConf::param(std::string conf_content) { // throw
	size_t pos = 0;

	while(1) {
		std::vector<std::string> tokens;

		int result = BaseConf::parse_token(conf_content, tokens, pos);
		if(result == CONF_ERROR)
			throw std::runtime_error("token error");
		if(result == CONF_EOF) {
			break;
		}

		if(!tokens.empty() && _handler_directive.find(tokens[0]) != _handler_directive.end()) {
			try {
				(this->*_handler_directive[tokens[0]])(tokens);
			} catch(std::runtime_error &e) { throw std::runtime_error("MainConf: " + std::string(e.what())); }
		} else {
			throw std::runtime_error("unknown directive");
		}
	}

	if(_servers.empty()) {
		throw std::runtime_error("MainConf: no server block");
	}
}

void MainConf::handle_server_block(std::vector<std::string> tokens) {
	if(tokens.size() != 2) {
		throw std::runtime_error("server block syntax error");
	}
	// tokens[0] = server
	// tokens[1] = { ... }

	// trim { }
	tokens[1].erase(0, 1);
	tokens[1].erase(tokens[1].size() - 1, 1);

	std::vector<std::string> server_tokens;

	ServConf servConf;

	try {
		servConf = ServConf(tokens[1]);
	} catch(std::runtime_error &e) { throw std::runtime_error(std::string(e.what())); }

	_servers.push_back(servConf);
}

// Getter
conf_value_t MainConf::getConfValue(std::string port, std::string host, std::string path) {
	conf_value_t conf_value;

	// conf_value init
	conf_value._path = "";
	conf_value._listen = std::make_pair("0.0.0.0", 80);
	conf_value._server_name = "";
	conf_value._error_page.clear();
	conf_value._limit_except.clear();
	conf_value._return = std::make_pair(0, "");
	conf_value._autoindex = false;
	conf_value._index.clear();
	conf_value._root = "";
	conf_value._client_max_body_size = 1024 * 1024;

	for(std::vector<ServConf>::iterator it = _servers.begin(); it != _servers.end(); it++) {
		ServConf serv_conf = *it;

		int server_port = serv_conf.get_listen().second;
		std::string server_name = serv_conf.get_server_name();

		std::stringstream ss(port);
		int port_num;
		ss >> port_num;

		/* std::cout << "server_name: " << server_name << std::endl; */
		/* std::cout << "server_port: " << server_port << std::endl; */
		/* std::cout << "host: " << host << std::endl; */
		/* std::cout << "port: " << port << std::endl; */

		if((server_name == "" && server_port == port_num) || (server_port == port_num && server_name == host)) {
			try {
				conf_value = serv_conf.getConfValue(path);
			} catch(std::runtime_error &e) { throw std::runtime_error(std::string(e.what())); }

			return conf_value;
		}
	}

	throw std::runtime_error("[ServConf] server not found");
}

std::vector<std::pair<std::string, int> > MainConf::get_listens() {
	std::vector<std::pair<std::string, int> > listens;

	for(size_t i = 0; i < _servers.size(); i++) {
		listens.push_back(_servers[i].get_listen());
	}

	if(listens.empty()) {
		throw std::runtime_error("listen not found");
	}

	return listens;
}

// Debug
void MainConf::debug_print() {
	for(size_t i = 0; i < _servers.size(); i++) {
		std::cout << "=========================== server " << i << ":" << std::endl;
		_servers[i].debug_print();
	}
}

void MainConf::debug_print_conf_value(conf_value_t conf_value) {
	std::cout << "listen: " << conf_value._listen.first << ":" << conf_value._listen.second << std::endl;
	std::cout << "server_name: " << conf_value._server_name << std::endl;
	std::cout << "error_page: " << std::endl;
	for(std::map<int, std::string>::iterator it = conf_value._error_page.begin(); it != conf_value._error_page.end();
		it++) {
		std::cout << it->first << " : " << it->second << std::endl;
	}
	std::cout << "path: " << conf_value._path << std::endl;
	std::cout << "limit_except: ";
	for(size_t i = 0; i < conf_value._limit_except.size(); i++) {
		std::cout << conf_value._limit_except[i] << " ";
	}
	std::cout << std::endl;
	if(conf_value._return.first != 0 && conf_value._return.second != "") {
		std::cout << "return: " << conf_value._return.first << " : " << conf_value._return.second << std::endl;
	}
	std::cout << "autoindex: " << conf_value._autoindex << std::endl;
	std::cout << "index: ";
	for(size_t i = 0; i < conf_value._index.size(); i++) {
		std::cout << conf_value._index[i] << " ";
	}
	std::cout << std::endl;
	std::cout << "root: " << conf_value._root << std::endl;
	std::cout << "client_max_body_size: " << conf_value._client_max_body_size << std::endl;
}
