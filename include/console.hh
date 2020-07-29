// Copyright 2018-2020 Tomoaki Yoshida<yoshida@furo.org>
/*
This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/
#pragma once
#include "zmq_nt.hpp"
#include "json.hh"
#include <sstream>

#include<iostream>
#include<iomanip>

class simConsole{
public:
  simConsole(zmq::context_t &ctx, std::string server="tcp://127.0.0.1:54323");
  ~simConsole(){close();}
  bool connect();
  void close();
  bool isValid(){return Sock && Sock->isValid();}
  std::string getLastError(){return lastErr;}
  bool submitRequest(std::string req,std::string &res);
  bool submitRequest(std::vector<std::string> req, std::string &res);

  bool listEndpoints(std::string tag, std::vector<std::string> &res);
  bool getActorMetadata(std::string actor, Json &res);
  bool execConsoleCommand(std::string command, std::string &res);
  bool sendActorMessage(std::string endpoint, std::string command, std::string &res);

protected:
  std::unique_ptr<zmq::socket_t> Sock;
  std::string Server;
  std::string lastErr;
};


simConsole::simConsole(zmq::context_t &ctx, std::string server):
  Sock(new zmq::socket_t(ctx, ZMQ_REQ)), Server(server){
  int on=1;
  Sock->setsockopt(ZMQ_REQ_CORRELATE, on);
  Sock->setsockopt(ZMQ_REQ_RELAXED, on);
  int timeout = 1000;
  Sock->setsockopt(ZMQ_RCVTIMEO, timeout);
  Sock->setsockopt(ZMQ_SNDTIMEO, timeout);
}

bool simConsole::connect(){
  if (!isValid() || Sock->connect(Server)!=0) {
    std::ostringstream os;
    os << "Cannot create client socket: " << zmq_strerror(zmq_errno());
    lastErr=os.str();
    return false;
  }
  lastErr.clear();
  return true;
}

void simConsole::close(){
  Sock->close();
}

bool simConsole::execConsoleCommand(std::string command, std::string &res){
  std::ostringstream os;
  os
    << "{\n"
    << "\"Type\" :  \"Console\",\n"
    << "\"Input\" : \"" << command << "\"\n"
    << "}";
  std::string r;
  if(!submitRequest(os.str(),r))
    return false;

  auto rj=Json::parse(r);
  if(rj.count("Result") || rj["Result"].is_string()){
    res=rj["Result"];
    lastErr.clear();
    return true;
  }
  lastErr="Unexpected response:"+r;
  return false;
}
bool simConsole::sendActorMessage(std::string endpoint, std::string command, std::string &res){
  std::ostringstream os;
  os
    << "{\n"
    << "\"Type\" :  \"ActorMsg\",\n"
    << "\"Endpoint\" : \"" << endpoint << "\"\n"
    << "}";
  std::string r;
  if(!submitRequest(std::vector<std::string>{os.str(),command},r))
    return false;

  auto rj=Json::parse(r);
  if(rj.count("Result") || rj["Result"].is_string()){
    res=rj["Result"];
    lastErr.clear();
    return true;
  }
  lastErr="Unexpected response:"+r;
  return false;
}

bool simConsole::listEndpoints(std::string tag, std::vector<std::string> &res){
  std::ostringstream os;
  os
    << "{\n"
    << "\"Type\" :  \"ListEndpoint\",\n"
    << "\"Tag\" : \"" << tag << "\"\n"
    << "}";
  std::string r;
  if(!submitRequest(os.str(),r))
      return false;

  auto rj=Json::parse(r);
  if(rj.count("Result") || rj["Result"].is_array()){
    for (Json::iterator it = rj["Result"].begin(),ec=rj["Result"].end();
         it != ec; ++it) {
      res.push_back(*it);
    }
    lastErr.clear();
    return true;
  }
  lastErr="Unexpected response:"+r;
  return false;
}
bool simConsole::getActorMetadata(std::string actor, Json &res){
  std::ostringstream os;
  os << "{\n"
     << "\"Type\" :  \"GetActorMeta\",\n"
     << "\"Endpoint\" : \"" << actor << "\"\n"
     << "}";
  std::string r;
  if (!submitRequest(os.str(), r))
    return false;

  auto rj = Json::parse(r);
  if (rj.count("Result")) {
    res=rj["Result"];
    lastErr.clear();
    return true;
  }
  lastErr = "Unexpected response:" + r;
  return false;
}

bool simConsole::submitRequest(std::vector<std::string> req, std::string &res){
  for(int i=0;i<req.size();++i){
    int flags=0;
    if(i!=req.size()-1)flags=ZMQ_SNDMORE;
    auto err=Sock->send(req[i].begin(), req[i].end(),flags);
    if (err < 0) {
      std::ostringstream os;
      os << "Could not send command to [" << Server << "] :"<< zmq_strerror(err);
      lastErr=os.str();
      return false;
    }
  }
  zmq::message_t msg;
  auto err=Sock->recv(&msg);
  if (err < 0) {
    std::ostringstream os;
    os << "Could not receive response from [" << Server << "] :"<< zmq_strerror(err);
    lastErr=os.str();
    return false;
  }
  lastErr.clear();
  res=std::string(msg.data<char>(), msg.size());
  return true;
}

bool simConsole::submitRequest(std::string req, std::string &res){
  auto err=Sock->send(req.begin(), req.end());
  if (err < 0) {
    std::ostringstream os;
    os << "Could not send command to [" << Server << "] :"<< zmq_strerror(err);
    lastErr=os.str();
    return false;
  }
  zmq::message_t msg;
  err=Sock->recv(&msg);
  if (err < 0) {
    std::ostringstream os;
    os << "Could not receive response from [" << Server << "] :"<< zmq_strerror(err);
    lastErr=os.str();
    return false;
  }
  lastErr.clear();
  res=std::string(msg.data<char>(), msg.size());
  return true;
}
