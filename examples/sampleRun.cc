// Copyright 2018 Tomoaki Yoshida<yoshida@furo.org>
/*
This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/

#include <signal.h>
#include"cageclient.hh"

static bool sTerminated = false;

void sig_handler(int s)
{
  sTerminated = true;
}

int main(int argc, char* argv[]) {
  signal(SIGINT, sig_handler);

  std::string server;
  std::vector<std::string> command;

  if(argc<1){
    std::cerr << "No server specified : "<<std::endl;
    std::cout << "usage: sampleRun [address]" << std::endl;
    exit(0);
  }
  server=std::string(argv[1]);

  CageAPI cage(server);

  std::cout<<"connecting to: "<<server<<std::endl;
  cage.connect();
  double odo = 0;
  double simClock=0;
  const double perimeter = (cage.VehicleInfo.WheelPerimeterL + cage.VehicleInfo.WheelPerimeterR)/2.;

  std::cout << "Waiting for message" << std::endl;
  { // initialize clock
    CageAPI::vehicleStatus vst;
    cage.getStatusOne(vst);
    simClock=vst.simClock;
  }
  // 0.2m/s
  cage.setVW(0.20,0);

  // wait for traveling 10[m]
  while (!sTerminated) {
    CageAPI::vehicleStatus vst;
    cage.poll();
    if(!cage.getStatusOne(vst))continue;
    std::cout << "------------------------" << std::endl;
    double dt=vst.simClock-simClock;
    simClock=vst.simClock;

    // simple odometry (no rotation)
    // rpm -> m/s
    double dx = (vst.lrpm - vst.rrpm) /60. / 2.
     /cage.VehicleInfo.ReductionRatio * perimeter * dt;
    odo+=dx;

    // 10[m] from start point
    if(odo>10)
    break;

    std::cout << "odo: " << odo << "  dt: " << dt << "  dx: " << dx << " rpmL:" << vst.lrpm << " rpmR:" << vst.rrpm << std::endl;
  }
  // stop
  cage.setVW(0,0);
}
