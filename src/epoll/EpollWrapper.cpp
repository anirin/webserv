/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   EpollWrapper.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: atsu <atsu@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/13 14:29:33 by rmatsuba          #+#    #+#             */
/*   Updated: 2025/04/04 02:40:50 by atsu             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "EpollWrapper.hpp"

EpollWrapper::EpollWrapper() {}

/* Epoll constructor with max_events */
EpollWrapper::EpollWrapper(int max_events) {
	epfd_ = epoll_create(max_events);
	if(epfd_ == -1)
		throw std::runtime_error("Failed to create epoll instance");
	events_list_.resize(max_events);
	max_events_ = max_events;
}

EpollWrapper::~EpollWrapper() {
	/* close(epfd_); */
}

int EpollWrapper::getEpfd() const {
	return epfd_;
}

std::vector<struct epoll_event> EpollWrapper::getEventsList() const {
	return events_list_;
}

/* Registerd given file descripter to epoll instance */
void EpollWrapper::addEvent(int fd) {
	struct epoll_event new_event;
	new_event.events = EPOLLIN;
	new_event.data.fd = fd;
	if(epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &new_event) == -1)
		throw std::runtime_error("[epoll wrapper] epoll_ctl failed: " + std::string(strerror(errno)));
}

/* Delete given file descripter from epoll instance */
void EpollWrapper::deleteEvent(int fd) {
	if(epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, NULL) == -1)
		throw std::runtime_error("[epoll wrapper] Failed to delete event from epoll instance");
}

/* Wait for event */
int EpollWrapper::epwait() {
	int nfds = epoll_wait(epfd_, events_list_.data(), events_list_.size(), 0);
	if(nfds == -1)
		throw std::runtime_error("epoll_wait failed");
	return nfds;
}

/* Operator overloading of [] operator */
struct epoll_event &EpollWrapper::operator[](size_t index) {
	return events_list_[index];
}

/* Modify event of given file descripter */
void EpollWrapper::setEvent(int fd, uint32_t events) {
	struct epoll_event new_event;
	new_event.events = events;
	new_event.data.fd = fd;
	if(epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &new_event) == -1)
		throw std::runtime_error("Failed to modify event in epoll instance");
}
