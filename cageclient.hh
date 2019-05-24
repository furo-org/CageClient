#pragma once

#include "console.hh"
#include "subscriber.hh"
#include <string>
#include <cmath>

class CageAPI{
  std::unique_ptr<zmq::context_t> ZCtx;
  std::string Endpoint;
  std::string VehicleName;
  std::string ReporterAddr, ConsoleAddr;
  std::string ErrorString;
  //std::thread Thread;
  //std::mutex Mutex;
  //bool isTerminated=false;

  float RpmLeft=0, RpmRight=0;

  std::unique_ptr<simSubscriber> Subscriber;
  std::unique_ptr<simConsole> Console;

public:
  struct vehicleStatus{
    double simClock;   // timestamp in simulated world [s]
    double lrpm, rrpm;
    double ax, ay, az; // accel [m/s^2]
    double rx, ry, rz; // rotational velocity [rad/s]
    // ground truth
    double ox, oy, oz, ow; // orientation
    double wx, wy, wz; // world position
  };
  struct vehicleInfo{
    std::string name;
    double WheelPerimeterL;
    double WheelPerimeterR;
    double TreadWidth;
    double ReductionRatio;
  } VehicleInfo;

  // construct CageAPI object. peer format: "SimulatorAddress/VehicleName"
  //   note: '/VehicleName' can be omitted.
  CageAPI(std::string peer);

  // construct CageAPI object. targetVehicle can be empty string
  CageAPI(std::string peerAddr, std::string targetVehicle);
  ~CageAPI();
  bool connect();
  std::string getErrorString(){return ErrorString;}

  bool isValid(){return ZCtx && Console && Subscriber;}
  simConsole& getConsole(){return *Console;}
  simSubscriber& getSubscriber(){return *Subscriber;};
  bool poll(int timeout_us=-1);
  bool getStatusOne(CageAPI::vehicleStatus &vst, int timeout_us = -1);
  bool setRpm(double rpmL, double rpmR);
  bool setVW(double V, double W);        // [m/s], [rad/s]

  std::string getError(){return ErrorString;}

private:
 template <typename F>
 void setErrorStrm(F f)
  {
    std::ostringstream ost;
    f(ost);
    ErrorString=ost.str();
  }
  void setError(std::string s){
    ErrorString=s;
  }

  void clearError(){
    ErrorString.clear();
  }
};

// ----------------------------------------------------------------

CageAPI::CageAPI(std::string peerAddr){
  auto sep=peerAddr.find_last_of('/');
  std::string vtarget;
  if(sep!=std::string::npos){
    VehicleName=peerAddr.substr(sep+1);
    peerAddr=peerAddr.substr(0,sep);
  }

  ReporterAddr="tcp://"+peerAddr+":54321";
  ConsoleAddr="tcp://"+peerAddr+":54323";
}

CageAPI::CageAPI(std::string peerAddr, std::string targetVehicle){
  ReporterAddr="tcp://"+peerAddr+":54321";
  ConsoleAddr="tcp://"+peerAddr+":54323";
  VehicleName=targetVehicle;
}

CageAPI::~CageAPI(){
  Subscriber.reset();
  Console.reset();
}

bool CageAPI::connect(){
  std::ostringstream ost;
  // ZMQ Context
  ZCtx.reset(new zmq::context_t(1));
  if (!ZCtx || !ZCtx->isValid()) {
    setErrorStrm([](auto &s) {
      s << "Cannot create zcontext:" << zmq_strerror(zmq_errno());
    });
    return false;
  }
  // Command Socket
  Console.reset(new simConsole(*ZCtx,ConsoleAddr));
  if(!Console || !Console->connect()){
    setError(Console->getLastError());
    Console.reset();
    ZCtx.reset();
    return false;
  }

  // List endpoint
  std::vector<std::string> targets;
  if(!Console->listEndpoints("Vehicle",targets)){
    setErrorStrm([&](auto &s){s<<"Unable to get endpoint list: "<<Console->getLastError();});
    Console.reset();
    ZCtx.reset();
    return false;
  }

  std::string endpoint;
  for(const auto &e:targets){
    std::cerr<<"Vehicle :"<<e;
    if(endpoint.size()==0){
      if(VehicleName.size()==0 || e.find(VehicleName)!=std::string::npos){
        endpoint=e;
        std::cerr<<" <- target selected";
      }
    }
    std::cerr<<std::endl;
  }
  if(endpoint.size()==0){
    setError("No matching vehicle found.");
    Console.reset();
    ZCtx.reset();
    return false;
  }

  // Reporter Socket
  Subscriber.reset(new simSubscriber(*ZCtx, ReporterAddr));
  if (!Subscriber || !Subscriber->connect()){
    setError(Subscriber->getLastError());
    Subscriber.reset();
    Console.reset();
    ZCtx.reset();
    return false;
  }

  Subscriber->addTargetActor(endpoint);
  Endpoint=endpoint;
  nlohmann::json meta;
  if(!Console->getActorMetadata(endpoint, meta)){
    setError("Failed to fetch vehicle metadata");
    return false;
  }
  VehicleInfo.name=endpoint;
  VehicleInfo.TreadWidth = static_cast<double>(meta["TreadWidth"]) / 100.;
  VehicleInfo.WheelPerimeterL = static_cast<double>(meta["WheelPerimeterL"]) / 100.;
  VehicleInfo.WheelPerimeterR = static_cast<double>(meta["WheelPerimeterR"]) / 100.;
  VehicleInfo.ReductionRatio = static_cast<double>(meta["ReductionRatio"]);
  ErrorString = "";
  return true;
}
bool CageAPI::poll(int timeout_us){
  return Subscriber->waitFor(timeout_us);
}
bool CageAPI::getStatusOne(CageAPI::vehicleStatus &vst, int timeout_us)
{
  if(!poll(timeout_us))return false;
  auto j=Subscriber->recvOne();
  if (j.find("Data") == j.end()) {
    setErrorStrm([](auto &ost) { ost << "Unexpected json structure."; });
    return false;
  }
  if (j.find("Time") == j.end()) {
    setErrorStrm([](auto &ost) { ost << "Unexpected json structure."; });
    return false;
  }
  vst.simClock=static_cast<double>(j["Time"]);
  auto r=j["Data"];
  if(r.find("LeftRpm")!=r.end())
    vst.lrpm=static_cast<double>(r["LeftRpm"]);
  if(r.find("RightRpm")!=r.end())
    vst.rrpm=static_cast<double>(r["RightRpm"]);
  auto accel=r.find("Accel");
  if(accel!=r.end()){
    nlohmann::json a=*accel;
    // cm/s^2 -> m/s^2
    vst.ax=static_cast<double>(a["X"])/100.;
    vst.ay=static_cast<double>(a["Y"])/100. * -1.;
    vst.az=static_cast<double>(a["Z"])/100.;
  }
  // [deg/s] -> [rad/s]
  auto angVel=r.find("AngVel");
  if(angVel!=r.end()){
    nlohmann::json av=*angVel;
    vst.rx=static_cast<double>(av["X"])*M_PI/180.;
    vst.ry=static_cast<double>(av["Y"])*M_PI/180.*-1.;
    vst.rz=static_cast<double>(av["Z"])*M_PI/180.*-1.;
  }
  auto pose=r.find("Pose");
  if(pose!=r.end()){
    nlohmann::json p=*pose;
    vst.ox=static_cast<double>(p["X"]);
    vst.oy=static_cast<double>(p["Y"]) * -1.;
    vst.oz=static_cast<double>(p["Z"]);
    vst.ow=static_cast<double>(p["W"]) * -1.;
  }
  // location  +X +Y +Z [cm]  -> +X -Y +Z [m]
  auto loc=r.find("Position");
  if(pose!=r.end()){
    nlohmann::json l=*loc;
    vst.wx = static_cast<double>(l["X"])/ 100.;
    vst.wy = static_cast<double>(l["Y"])/ 100. * -1.;
    vst.wz = static_cast<double>(l["Z"])/ 100.;
  }
  return true;
}
bool CageAPI::setRpm(double rpmL, double rpmR){
  std::ostringstream os;
  std::string res;
  os << "{\"CmdType\":\"RPM\",\"R\":" << rpmR 
  << ",\"L\":" << rpmL << "}" << std::endl;
  if (!Console->sendActorMessage(Endpoint, os.str(), res))
  {
    setErrorStrm([&](auto &ost) { ost << " Failed to send actor command to " << Endpoint
     << " : " << Console->getLastError(); });
    return false;
  }
  return true;
}

bool CageAPI::setVW(double V, double W){
  std::ostringstream os;
  std::string res;
  // m/s -> cm/s  deg/s -> rad/s
  os << "{\"CmdType\":\"VW\",\"V\":" << V*100
     << ",\"W\":" << W*180./M_PI << "}" << std::endl;
  if (!Console->sendActorMessage(Endpoint, os.str(), res))
  {
    setErrorStrm([&](auto &ost) { ost << " Failed to send actor command to " << Endpoint
                                 << " : " << Console->getLastError(); });
    return false;
  }
  return true;
}

