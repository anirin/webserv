/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/13 13:49:54 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/03/30 15:31:13 by atsu             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"
#include "ConnectionWrapper.hpp"
#include "EpollWrapper.hpp"
#include "Listener.hpp"
#include "includes/socket/Connection.hpp"

std::string getConfContent(char *confPath) {
	if(confPath) {
		std::ifstream ifs(confPath);
		if(!ifs) {
			throw std::runtime_error("[main.cpp] Failed to open configuration file");
		}
		std::stringstream buffer;
		buffer << ifs.rdbuf();
		std::string content = buffer.str();
		std::cout << "[main.cpp] confPath: " << confPath << std::endl;
		return content;
	}

	// todo 以下の項目を削除
	std::string defaultPath = "configs/normal_case/default.conf";
	std::ifstream ifs(defaultPath.c_str());
	if(!ifs) {
		throw std::runtime_error("[main.cpp] Failed to open configuration file");
	}
	std::stringstream buffer;
	buffer << ifs.rdbuf();
	std::string content = buffer.str();
	std::cout << "[main.cpp] confPath: " << defaultPath << std::endl;
	return content;
}

int main(int argc, char **argv) {
	if(argc != 1 && argc != 2) {
		std::cerr << "./webserv {your conf path} or ./webserv" << std::endl;
		return 1;
	}

	char *confPath = NULL;
	if(argc == 2) {
		confPath = argv[1];
	}

	// configの読み込み
	std::string content = getConfContent(confPath);
	MainConf mainConf;
	try {
		mainConf = MainConf(content);
	} catch(const std::exception &e) {
		std::cerr << "[main.cpp] Error: " << e.what() << std::endl;
		return 1;
	}

	// サーバーの設定
	EpollWrapper epollWrapper(100);

	// ポート番号の重複を取り除く
	std::vector<std::pair<std::string, int> > ports = mainConf.get_listens();
	std::set<int> unique_ports;
	for(size_t i = 0; i < ports.size(); i++) {
		unique_ports.insert(ports[i].second);
	}

	// リスナーの設定
	std::vector<int> listen_fds;
	for(std::set<int>::iterator it = unique_ports.begin(); it != unique_ports.end(); ++it) {
		Listener newl;
		try {
			newl = Listener(*it);
		} catch(const std::exception &e) {
			std::cerr << "[main.cpp] Error: Listen failed: " << e.what() << std::endl;
			return 1;
		}
		epollWrapper.addEvent(newl.getFd());
		listen_fds.push_back(newl.getFd());
	}

	ConnectionWrapper connections;

	/* Main loop */
	while(true) {
		FileStatus file_status;

		int nfds = epollWrapper.epwait();
		for(int i = 0; i < nfds; ++i) {
			std::cout << std::endl << "[main.cpp] epoll for-loop ===[" << i << "]===" << std::endl;
			struct epoll_event current_event = epollWrapper[i];
			int target_fd = current_event.data.fd;
			if(std::find(listen_fds.begin(), listen_fds.end(), target_fd) != listen_fds.end()) {
				try {
					Connection *newConn = new Connection(target_fd);
					epollWrapper.addEvent(newConn->getFd());
					connections.addConnection(newConn);
				} catch(const std::exception &e) {
					std::cerr << "[main.cpp] Error: Accept failed: " << e.what() << std::endl;
				}
			} else {
				Connection *conn = connections.getConnection(target_fd);

				if(!conn) {
					std::cerr << "[main.cpp] Error: Connection not found" << std::endl;
					continue;
				}
				if(conn->isTimedOut(&mainConf)) {
					epollWrapper.deleteEvent(target_fd);
					connections.removeConnection(target_fd);
					close(target_fd);
					continue;
				}
				FileTypes type = conn->getFdType(target_fd);
				switch(type) {
					case SOCKET:
						if(current_event.events & EPOLLIN) {
							file_status = conn->readSocket(&mainConf);
							if(file_status == ERROR || file_status == CLOSED) {
								epollWrapper.deleteEvent(target_fd);
								connections.removeConnection(target_fd);
								close(target_fd);
								std::cout << "[main.cpp] Error: connection closed" << std::endl;
							}
							if(file_status == SUCCESS_STATIC) {
								epollWrapper.setEvent(target_fd, EPOLLOUT);
								std::cout << "[main.cpp] connection event set to EPOLLOUT" << std::endl;
							}
							if(file_status == SUCCESS_CGI) {
								epollWrapper.setEvent(target_fd, EPOLLOUT);
								std::cout << "[main.cpp] connection event set to EPOLLOUT" << std::endl;
							}
						} else if(current_event.events & EPOLLOUT) {
							file_status = conn->writeSocket();
							if(file_status == ERROR) {
								epollWrapper.deleteEvent(target_fd);
								connections.removeConnection(target_fd);
								close(target_fd);
								std::cout << "[main.cpp] Error: connection closed" << std::endl;
							}
							if(file_status == SUCCESS) {
								epollWrapper.setEvent(target_fd, EPOLLIN);
								std::cout << "[main.cpp] connection event set to EPOLLIN" << std::endl;
								// todo 初期化すべき部分は別にあるかも
								conn->clearValue();
							}
						}
						break;
					case PIPE:
						std::cout << "[main.cpp] PIPE case should not be used with new CGI implementation" << std::endl;
						break;
					default:
						break;
				}
			}
		}
	}
}
