﻿// Copyright 2018 Tomoaki Yoshida<yoshida@furo.org>
/*
This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/

#include "zmq_nt.hpp"

#include <iostream>
#include <memory>
#include <sstream>
#include <set>
#include <string>

#include "nlohmann/json.hpp"


class simSubscriber{
public:
  simSubscriber(zmq::context_t &ctx, std::string server="tcp://127.0.0.1:54321");
  ~simSubscriber(){close();}
  bool connect();
  void close();
  bool isValid(){return Sock && Sock->isValid();}
  std::string getLastError(){return lastErr;}
  void addTargetActor(std::string actor);
  nlohmann::json recvOne();
  bool waitFor(int timeout_ms);

protected:
  std::unique_ptr<zmq::socket_t> Sock;
  std::string Server;
  std::string lastErr;
  std::set<std::string> Actors;
};

// -----------------------------------------------

simSubscriber::simSubscriber(zmq::context_t &ctx, std::string server):
  Sock(new zmq::socket_t(ctx, ZMQ_SUB)), Server(server){
  int timeout=1000;
  Sock->setsockopt(ZMQ_RCVTIMEO, timeout);
  Sock->setsockopt(ZMQ_SNDTIMEO, timeout);
}

bool simSubscriber::connect(){
  if (!isValid() || Sock->connect(Server)!=0) {
    std::ostringstream os;
    os << "Cannot create client socket: " << zmq_strerror(zmq_errno());
    lastErr=os.str();
    return false;
  }
  lastErr.clear();
  Sock->setsockopt(ZMQ_SUBSCRIBE, nullptr, 0);
  return true;
}

void simSubscriber::close(){
  Sock->close();
}

void simSubscriber::addTargetActor(std::string actor){
  Actors.insert(actor);
}

nlohmann::json simSubscriber::recvOne(){
  zmq::message_t msg;
  auto err = Sock->recv(&msg);
  if (err < 0) {
    std::ostringstream os;
    os << " Possible reason: " << zmq_strerror(err) << std::endl;
    lastErr=os.str();
    return nlohmann::json();
  }
  lastErr.clear();
  std::string res(msg.data<char>(), msg.size());
  // 受信JSONをパース
  nlohmann::json j = nlohmann::json::parse(res);

  if(j.find("Report")==j.end()){
    std::ostringstream os;
    os<<"Unexpected data received(No Repot field):"<<j<<std::endl;
    lastErr=os.str();
    return nlohmann::json();
  }
  auto j2=j["Report"];
  if(j2.find("Name")==j2.end()){
    std::ostringstream os;
    os<<"Unexpected data received(No Name field):"<<j2<<std::endl;
    lastErr=os.str();
    return nlohmann::json();
  }

  std::string name = j2["Name"];

  if(Actors.size()){
    decltype(Actors)::iterator it=Actors.find(name);
    if(it==Actors.end()) return nlohmann::json();

  }
  return j2;
}
bool simSubscriber::waitFor(int timeout_ms){
  uint32_t optval=Sock->getsockopt<uint32_t>(ZMQ_EVENTS);
  if(optval&ZMQ_POLLIN){
    std::cerr<<"waitFor: ZMQ_POLLIN success:"<<optval<<std::endl;
    return true;
  }

  zmq_pollitem_t pollitem;
  pollitem.socket=static_cast<void*>(*Sock);
  pollitem.events=ZMQ_POLLIN;
  if(zmq::poll(&pollitem,1,timeout_ms) &&
    pollitem.revents&ZMQ_POLLIN)
      return true;
  return false;
}
