// Copyright 2018 Tomoaki Yoshida<yoshida@furo.org>
/*
This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/

#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>
#include<iomanip>
#include <signal.h>
#include"cageclient.hh"

namespace bo = boost::program_options;
static bool sTerminated = false;

void sig_handler(int s)
{
  sTerminated = true;
}

int main(int argc, char* argv[]) {
  signal(SIGINT, sig_handler);

  std::string server;
  std::vector<std::string> command;
  bo::options_description options;
  options.add_options()
    ("help,h", "Print description")
    ("server,s", bo::value<std::string>(), "Server address and port (e.g. 127.0.0.1:54321");

  try {
    bo::variables_map values;
    bo::parsed_options parsed = bo::command_line_parser(argc, argv).options(options).allow_unregistered().run();
    bo::store(parsed, values);
    bo::notify(values);

    if (values.count("server")) {
      server = values["server"].as<std::string>();
    }
    if (values.count("help")) {
      std::cout << "usage: sampleSubscriber [options]" << std::endl;
      std::cout << options << std::endl;
      return 0;
    }
  }
  catch (std::exception &e) {
    std::cout << e.what() << std::endl;
    return -1;
  }

  if (server.size() == 0) {
    std::cerr << "No server specified : " << server << std::endl;
    std::cout << "usage: sampleSubscriber [options]" << std::endl;
    std::cout << options << std::endl;
    exit(1);
  }

  CageAPI cage(server);

  std::cout<<"connecting to: "<<server<<std::endl;
  cage.connect();

  std::cout<<"Waiting for message"<<std::endl;
  while (!sTerminated) {
    cage.poll();
    auto res=cage.getSubscriber().recvOne();
    std::cout << "------------------------" << std::endl;
    std::cout<<std::setw(4)<<res<<std::endl;
  }
  std::cout << "terminated" << std::endl;
}
