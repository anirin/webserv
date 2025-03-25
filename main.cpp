/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+           */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/13 13:49:54 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/02/24 11:44:23by atsu             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"
#include "ConnectionWrapper.hpp"
#include "EpollWrapper.hpp"
#include "Listener.hpp"

std::string getConfContent() {
	std::string confPath = "src/config/sample/test.conf";
	std::ifstream ifs(confPath.c_str());
	if(!ifs) {
		throw std::runtime_error("[main.cpp] Failed to open configuration file");
	}
	std::stringstream buffer;
	buffer << ifs.rdbuf();
	std::string content = buffer.str();
	return content;
}

int main() {
	std::string content = getConfContent();
	MainConf mainConf(content);

	Listener listener(8080);
	EpollWrapper epollWrapper(100);
	ConnectionWrapper connections;
	epollWrapper.addEvent(listener.getFd());

	std::map<int, int> event_count_per_iteration;
	std::set<int> processed_fds; // Track FDs that have been deleted in this iteration

	while(true) {
		FileStatus file_status;

		int nfds = epollWrapper.epwait();
		event_count_per_iteration.clear(); // Reset counter each iteration
		processed_fds.clear(); // Reset the tracking set

		for(int i = 0; i < nfds; ++i) {
			std::cout << std::endl << "[main.cpp] epoll for-loop ===[" << i << "]===" << std::endl;
			struct epoll_event current_event = epollWrapper[i];
			int target_fd = current_event.data.fd;

			// Check if we've processed this FD too many times
			if (event_count_per_iteration.find(target_fd) != event_count_per_iteration.end()) {
				event_count_per_iteration[target_fd]++;
				
				if (event_count_per_iteration[target_fd] > 5) {
					std::cout << "[main.cpp] Warning: Skipping fd " << target_fd 
							<< " to prevent infinite loop" << std::endl;
					continue; // Skip this FD for this iteration
				}
			} else {
				event_count_per_iteration[target_fd] = 1;
			}

			if(target_fd == listener.getFd()) {
				try {
					Connection *newConn = new Connection(listener.getFd());
					epollWrapper.addEvent(newConn->getFd());
					connections.addConnection(newConn);
				} catch(const std::exception &e) {
					std::cerr << "[main.cpp] Error: Accept failed: " << e.what() << std::endl;
				}
			} else {
				// Check if this FD has already been processed and removed
                if(processed_fds.find(target_fd) != processed_fds.end()) {
                    std::cout << "[main.cpp] Skipping already processed fd " << target_fd << std::endl;
                    continue;
                }

				Connection *conn = connections.getConnection(target_fd);

				if(!conn) {
					std::cerr << "[main.cpp] Error: Connection not found" << std::endl;
					// Add to processed list to avoid repeated errors
                    processed_fds.insert(target_fd);
                    
                    // Try to clean up this stray FD
                    try {
                        epollWrapper.deleteEvent(target_fd);
                        close(target_fd);
                    } catch(const std::exception &e) {
                        // Just trying to clean up, so ignore errors
                    }
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
								processed_fds.insert(target_fd);
							}
							if(file_status == SUCCESS_STATIC) {
								epollWrapper.setEvent(target_fd, EPOLLOUT);
								std::cout << "[main.cpp] connection event set to EPOLLOUT" << std::endl;
							}
							if(file_status == SUCCESS_CGI) {
								epollWrapper.addEvent(conn->getCGI()->getFd());
								std::cout << "[main.cpp] CGI event add to epoll" << std::endl;
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
								processed_fds.insert(target_fd);
							}
							if(file_status == SUCCESS) {
								epollWrapper.setEvent(target_fd, EPOLLIN | EPOLLONESHOT);
								std::cout << "[main.cpp] connection event set to EPOLLIN|EPOLLONESHOT" << std::endl;
								conn->clearValue();
							}
						}
						break;
					case PIPE:
						std::cout << "[main.cpp] Processing CGI pipe" << std::endl;
						file_status = conn->readCGI();
						if(file_status == ERROR) {
							epollWrapper.deleteEvent(target_fd);
							close(target_fd);
							epollWrapper.deleteEvent(conn->getFd());
							connections.removeConnection(conn->getFd());
							close(conn->getFd());
							std::cout << "[main.cpp] connection closed due to CGI error" << std::endl;
							processed_fds.insert(target_fd);
						} else if(file_status == SUCCESS) {
							try {
								epollWrapper.deleteEvent(target_fd);
								close(target_fd);
								
								 // Use EPOLLONESHOT to prevent immediate re-triggering
								epollWrapper.setEvent(conn->getFd(), EPOLLOUT | EPOLLONESHOT);
								std::cout << "[main.cpp] connection event set to EPOLLOUT|EPOLLONESHOT after CGI success" << std::endl;
							} catch(const std::exception &e) {
								std::cerr << "[main.cpp] Error modifying socket: " << e.what() << std::endl;
							}
						}
						break;
					default:
						break;
			}
			}
		}
	}
}
