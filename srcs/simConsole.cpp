// Copyright 2018 Tomoaki Yoshida<yoshida@furo.org>
/*
This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

#include "console.hh"
#include "zmq_nt.hpp"


namespace bo = boost::program_options;

int main(int argc, char *argv[]) {
  std::string              server;
  std::vector<std::string> command;
  bo::options_description  options;
  enum {
    CONSOLE,
    LIST_ENDPOINT,
  } Mode = CONSOLE;

  options.add_options()("help,h", "Print description")(
      "endpoint,e", "Query endpoint tagged with specified sring")(
      "server,s", bo::value<std::string>(),
      "Server address and port (e.g. 127.0.0.1:54323");

  try {
    bo::variables_map  values;
    bo::parsed_options parsed = bo::command_line_parser(argc, argv)
                                    .options(options)
                                    .allow_unregistered()
                                    .run();
    bo::store(parsed, values);
    bo::notify(values);
    command = bo::collect_unrecognized(parsed.options, bo::include_positional);

    if (values.count("server")) {
      server = values["server"].as<std::string>();
    }
    if (values.count("endpoint")) {
      Mode = LIST_ENDPOINT;
    }
    if (values.count("help")) {
      std::cout << "usage: simconsole [options] [console command to exec]"
                << std::endl;
      std::cout << options << std::endl;
      return 0;
    }
  } catch (std::exception &e) {
    std::cout << e.what() << std::endl;
    return -1;
  }

  if (server.size() == 0) {
    std::cerr << "No server specified : " << server << std::endl;
    exit(1);
  }

  std::unique_ptr<zmq::context_t> ctx(new zmq::context_t(1));
  if (!ctx) {
    std::cerr << "Cannot create zcontext:" << zmq_strerror(zmq_errno())
              << std::endl;
    exit(1);
  }
  if (!ctx->isValid()) {
    std::cerr << "Cannot create zcontext:" << zmq_strerror(zmq_errno())
              << std::endl;
    exit(1);
  }

  server = "tcp://" + server + ":54323";

  simConsole con(*ctx, server);
  if (!con.connect()) {
    std::cerr << con.getLastError() << std::endl;
    exit(1);
  }

  std::string cmdline;
  for (const auto &arg : command) {
    if (cmdline.size()) cmdline += " ";
    cmdline = cmdline + arg;
  }
  switch (Mode) {
    case CONSOLE: {
      std::string res;
      if (!con.execConsoleCommand(cmdline, res)) {
        std::cerr << "Request Failed: " << con.getLastError() << std::endl;
        exit(1);
      }
      std::cout << "Result: " << res << std::endl;
    } break;
    case LIST_ENDPOINT: {
      std::vector<std::string> res;
      if (!con.listEndpoints(cmdline, res)) {
        std::cerr << "Request Failed: " << con.getLastError() << std::endl;
        exit(1);
      }
      std::cout << "Result: " << std::endl;
      for (const auto &e : res) {
        std::cout << " [" << e << "]" << std::endl;
        Json meta;
        if (con.getActorMetadata(e, meta)) {
          std::cout << std::setw(4) << meta << std::endl;
        }
      }
    } break;
  }
}
