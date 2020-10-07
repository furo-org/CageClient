// Copyright 2018-2020 Tomoaki Yoshida<yoshida@furo.org>
/*
This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <array>
#include <cmath>
#include <sstream>
#include <string>

#include "console.hh"
#include "subscriber.hh"

class CageAPI {
  std::unique_ptr<zmq::context_t> ZCtx;
  std::string                     Endpoint;
  std::string                     VehicleName;
  std::string                     ReporterAddr, ConsoleAddr;
  std::string                     ErrorString;
  // std::thread Thread;
  // std::mutex Mutex;
  // bool isTerminated=false;

  float RpmLeft = 0, RpmRight = 0;

  std::unique_ptr<simSubscriber> Subscriber;
  std::unique_ptr<simConsole>    Console;

public:
  struct vehicleStatus {
    double simClock;  // timestamp in simulated world [s]
    double lrpm, rrpm;
    double ax, ay, az;  // accel [m/s^2]
    double rx, ry, rz;  // rotational velocity [rad/s]
    // ground truth
    double      ox, oy, oz, ow;  // orientation
    double      wx, wy, wz;      // world position
    double      latitude, longitude;
    std::string toString() {
      std::ostringstream os;
      os << "Clock: " << simClock << "\n"
         << "lrpm: " << lrpm << "\t rrpm: " << rrpm << "\n"
         << "accel  x: " << ax << "\t y: " << ay << "\t z: " << az << "\n"
         << "angvel x: " << rx << "\t y: " << ry << "\t z: " << rz << "\n"
         << std::setprecision(10) << "latitude: " << latitude
         << "\t longitude: " << longitude << std::endl;
      return os.str();
    }
  };

  struct Transform {
    std::array<double, 3> trans;  // x, y, z  [m]
    std::array<double, 4> rot;    // w, x, y, z
  };

  struct vehicleInfo {
    std::string                      name;
    double                           WheelPerimeterL;  //  [m]
    double                           WheelPerimeterR;  //  [m]
    double                           TreadWidth;       //  [m]
    double                           ReductionRatio;
    std::map<std::string, Transform> Transforms;
  } VehicleInfo;
  struct worldInfo {
    bool                  valid;
    double                Latitude0;
    double                Longitude0;
    std::array<double, 3> ReferenceLocation;
    std::array<double, 4> ReferenceRotation;
  } WorldInfo;

  // construct CageAPI object. peer format: "SimulatorAddress/VehicleName"
  //   note: '/VehicleName' can be omitted.
  CageAPI(std::string peerAddr);

  // construct CageAPI object. targetVehicle can be empty string
  CageAPI(std::string peerAddr, std::string targetVehicle);
  ~CageAPI();
  bool        connect();
  std::string getErrorString() { return ErrorString; }

  bool           isValid() { return ZCtx && Console && Subscriber; }
  simConsole &   getConsole() { return *Console; }
  simSubscriber &getSubscriber() { return *Subscriber; };
  bool           poll(int timeout_us = -1);
  bool           getStatusOne(CageAPI::vehicleStatus &vst, int timeout_us = -1);

  bool setRpm(double rpmL,
              double rpmR);        // Left wheel and Right wheel speed in [rpm]
  bool setVW(double V, double W);  // Forward, Angvel in [m/s], [rad/s]
  bool setFLW(double F, double L,
              double W);  // Forward, Left, Angvel in  [m/s], [m/s], [rad/s]

  std::string getError() { return ErrorString; }

  void setDefaultTransform(std::string           frameId,
                           std::array<double, 3> translation,
                           std::array<double, 4> rotation);

private:
  template <typename F>
  void setErrorStrm(F f) {
    std::ostringstream ost;
    f(ost);
    ErrorString = ost.str();
  }
  void setError(std::string s) { ErrorString = s; }

  void clearError() { ErrorString.clear(); }

  double decode60(std::array<double, 3> v) {
    double d = v[0], m = v[1], s = v[2];
    return ((s / 60.) + m) / 60. + d;
  }
};

// ----------------------------------------------------------------

CageAPI::CageAPI(std::string peerAddr) {
  auto        sep = peerAddr.find_last_of('/');
  std::string vtarget;
  if (sep != std::string::npos) {
    VehicleName = peerAddr.substr(sep + 1);
    peerAddr    = peerAddr.substr(0, sep);
  }

  ReporterAddr = "tcp://" + peerAddr + ":54321";
  ConsoleAddr  = "tcp://" + peerAddr + ":54323";
}

CageAPI::CageAPI(std::string peerAddr, std::string targetVehicle) {
  ReporterAddr = "tcp://" + peerAddr + ":54321";
  ConsoleAddr  = "tcp://" + peerAddr + ":54323";
  VehicleName  = targetVehicle;
}

CageAPI::~CageAPI()=default;

void CageAPI::setDefaultTransform(std::string           frameId,
                                  std::array<double, 3> translation,
                                  std::array<double, 4> rotation) {
  VehicleInfo.Transforms[frameId] = Transform{translation, rotation};
}

bool CageAPI::connect() {
  std::ostringstream ost;
  // ZMQ Context
  Subscriber.release();
  Console.release();
  ZCtx.reset(new zmq::context_t(1));
  if (!ZCtx || !ZCtx->isValid()) {
    setErrorStrm([](auto &s) {
      s << "Cannot create zcontext:" << zmq_strerror(zmq_errno());
    });
    return false;
  }
  // Command Socket
  Console.reset(new simConsole(*ZCtx, ConsoleAddr));
  if (!Console || !Console->connect()) {
    setError(Console->getLastError());
    Console.release();
    ZCtx.release();
    return false;
  }

  // List endpoint
  std::vector<std::string> targets;
  if (!Console->listEndpoints("Vehicle", targets)) {
    setErrorStrm([&](auto &s) {
      s << "Unable to get endpoint list: " << Console->getLastError();
    });
    Console.release();
    ZCtx.release();
    return false;
  }

  std::string endpoint;
  for (const auto &e : targets) {
    std::cerr << "Vehicle :" << e;
    if (endpoint.size() == 0) {
      if (VehicleName.size() == 0 || e.find(VehicleName) != std::string::npos) {
        endpoint = e;
        std::cerr << " <- target selected";
      }
    }
    std::cerr << std::endl;
  }
  if (endpoint.size() == 0) {
    setError("No matching vehicle found.");
    Console.release();
    ZCtx.release();
    return false;
  }

  // GeoReference
  targets.clear();
  std::string geoReference;
  if (Console->listEndpoints("GeoReference", targets)) {
    if (targets.size() > 1) {
      std::cerr << "Multiple GeoReference reported. Using the first one ["
                << targets[0] << "]." << std::endl;
    }
    geoReference = targets[0];
    Json grMeta;
    if (!Console->getActorMetadata(geoReference, grMeta)) {
      setError("Failed to fetch geo-reference metadata");
    } else {
      std::cerr << "GeoReference Actor [" << geoReference << "]" << std::endl;
      WorldInfo.valid = false;
      if (grMeta.count("GeoLocation")) {
        auto loc             = grMeta["GeoLocation"];
        auto lat             = loc["latitude"];
        auto lon             = loc["longitude"];
        WorldInfo.Latitude0  = decode60({static_cast<double>(lat["x"]),
                                        static_cast<double>(lat["y"]),
                                        static_cast<double>(lat["z"])});
        WorldInfo.Longitude0 = decode60({static_cast<double>(lon["x"]),
                                         static_cast<double>(lon["y"]),
                                         static_cast<double>(lon["z"])});
        if (grMeta.count("Transform")) {
          auto trans                     = grMeta["Transform"];
          auto t                         = trans["translation"];
          WorldInfo.ReferenceLocation[0] = static_cast<double>(t["x"]);
          WorldInfo.ReferenceLocation[1] = static_cast<double>(t["y"]);
          WorldInfo.ReferenceLocation[2] = static_cast<double>(t["z"]);
          auto r                         = trans["rotation"];
          WorldInfo.ReferenceRotation[0] = static_cast<double>(r["w"]);
          WorldInfo.ReferenceRotation[1] = static_cast<double>(r["x"]);
          WorldInfo.ReferenceRotation[2] = static_cast<double>(r["y"]);
          WorldInfo.ReferenceRotation[3] = static_cast<double>(r["z"]);
          WorldInfo.valid                = true;
        }
      }
    }
  }

  // Reporter Socket
  Subscriber.reset(new simSubscriber(*ZCtx, ReporterAddr));
  if (!Subscriber || !Subscriber->connect()) {
    setError(Subscriber->getLastError());
    Subscriber.release();
    Console.release();
    ZCtx.release();
    return false;
  }

  Subscriber->addTargetActor(endpoint);
  Endpoint = endpoint;
  Json meta;
  if (!Console->getActorMetadata(endpoint, meta)) {
    setError("Failed to fetch vehicle metadata");
    return false;
  }
  VehicleInfo.name = endpoint;
  if (meta.count("TreadWidth"))
    VehicleInfo.TreadWidth = static_cast<double>(meta["TreadWidth"]) / 100.;
  if (meta.count("WheelPerimeterL"))
    VehicleInfo.WheelPerimeterL =
        static_cast<double>(meta["WheelPerimeterL"]) / 100.;
  if (meta.count("WheelPerimeterR"))
    VehicleInfo.WheelPerimeterR =
        static_cast<double>(meta["WheelPerimeterR"]) / 100.;
  if (meta.count("ReductionRatio"))
    VehicleInfo.ReductionRatio = static_cast<double>(meta["ReductionRatio"]);
  const std::string transform{"Transform-"};
  for (const auto &kv : meta.items()) {
    const auto &key=kv.key();
    const auto &value = kv.value();
    if (key.compare(0, transform.size(), transform) != 0) continue;
    std::string coord{key.substr(transform.size())};
    std::cout << "Found Transform for : " << coord << std::endl;
    Transform t;
    t.trans[0] = static_cast<double>(value["translation"]["x"]) / 100.;
    t.trans[1] = static_cast<double>(value["translation"]["y"]) / 100. * -1.;
    t.trans[2] = static_cast<double>(value["translation"]["z"]) / 100.;
    t.rot[0]   = static_cast<double>(value["rotation"]["w"]) * -1.;
    t.rot[1]   = static_cast<double>(value["rotation"]["x"]);
    t.rot[2]   = static_cast<double>(value["rotation"]["y"]) * -1.;
    t.rot[3]   = static_cast<double>(value["rotation"]["z"]);
    VehicleInfo.Transforms[coord] = t;
  }
  ErrorString = "";
  return true;
}
bool CageAPI::poll(int timeout_us) { return Subscriber->waitFor(timeout_us); }
bool CageAPI::getStatusOne(CageAPI::vehicleStatus &vst, int timeout_us) {
  if (!poll(timeout_us)) return false;
  auto j = Subscriber->recvOne();
  // std::cout<<"Recv:["<<j<<"]"<<std::endl;
  if (j.find("Data") == j.end()) {
    setErrorStrm([](auto &ost) { ost << "Unexpected json structure."; });
    return false;
  }
  if (j.find("Time") == j.end()) {
    setErrorStrm([](auto &ost) { ost << "Unexpected json structure."; });
    return false;
  }
  vst.simClock = static_cast<double>(j["Time"]);
  auto r       = j["Data"];
  if (r.find("LeftRpm") != r.end())
    vst.lrpm = static_cast<double>(r["LeftRpm"]);
  if (r.find("RightRpm") != r.end())
    vst.rrpm = static_cast<double>(r["RightRpm"]);
  auto accel = r.find("Accel");
  if (accel != r.end()) {
    Json a = *accel;
    // cm/s^2 -> m/s^2
    vst.ax = static_cast<double>(a["X"]) / 100.;
    vst.ay = static_cast<double>(a["Y"]) / 100. * -1.;
    vst.az = static_cast<double>(a["Z"]) / 100.;
  }
  // [deg/s] -> [rad/s]
  auto angVel = r.find("AngVel");
  if (angVel != r.end()) {
    Json av = *angVel;
    vst.rx  = static_cast<double>(av["X"]) * M_PI / 180.;
    vst.ry  = static_cast<double>(av["Y"]) * M_PI / 180. * -1.;
    vst.rz  = static_cast<double>(av["Z"]) * M_PI / 180. * -1.;
  }
  auto pose = r.find("Pose");
  if (pose != r.end()) {
    Json p = *pose;
    vst.ox = static_cast<double>(p["X"]);
    vst.oy = static_cast<double>(p["Y"]) * -1.;
    vst.oz = static_cast<double>(p["Z"]);
    vst.ow = static_cast<double>(p["W"]) * -1.;
  }
  // location  +X +Y +Z [cm]  -> +X -Y +Z [m]
  auto loc = r.find("Position");
  if (loc != r.end()) {
    Json l = *loc;
    vst.wx = static_cast<double>(l["X"]) / 100.;
    vst.wy = static_cast<double>(l["Y"]) / 100. * -1.;
    vst.wz = static_cast<double>(l["Z"]) / 100.;
  }
  auto lat = r.find("lat");
  if (lat != r.end()) {
    Json b = *lat;
    vst.latitude =
        decode60({static_cast<double>(b["X"]), static_cast<double>(b["Y"]),
                  static_cast<double>(b["Z"])});
  }
  auto lon = r.find("lon");
  if (lon != r.end()) {
    Json l = *lon;
    vst.longitude =
        decode60({static_cast<double>(l["X"]), static_cast<double>(l["Y"]),
                  static_cast<double>(l["Z"])});
  }
  return true;
}

bool CageAPI::setRpm(double rpmL, double rpmR) {
  std::ostringstream os;
  std::string        res;
  os << "{\"CmdType\":\"RPM\",\"R\":" << rpmR << ",\"L\":" << rpmL << "}"
     << std::endl;
  if (!Console->sendActorMessage(Endpoint, os.str(), res)) {
    setErrorStrm([&](auto &ost) {
      ost << " Failed to send actor command to " << Endpoint << " : "
          << Console->getLastError();
    });
    return false;
  }
  return true;
}

bool CageAPI::setVW(double V, double W) {
  std::ostringstream os;
  std::string        res;
  // m/s -> cm/s  deg/s -> rad/s
  os << "{\"CmdType\":\"VW\",\"V\":" << V * 100 << ",\"W\":" << W * 180. / M_PI
     << "}" << std::endl;
  if (!Console->sendActorMessage(Endpoint, os.str(), res)) {
    setErrorStrm([&](auto &ost) {
      ost << " Failed to send actor command to " << Endpoint << " : "
          << Console->getLastError();
    });
    return false;
  }
  return true;
}

bool CageAPI::setFLW(double F, double L, double W) {
  std::ostringstream os;
  std::string        res;
  // m/s -> cm/s  deg/s -> rad/s
  os << "{\"CmdType\":\"VW\",\"V\":" << F * 100 << ",\"L\":" << L * 100
     << ",\"W\":" << W * 180. / M_PI << "}" << std::endl;
  if (!Console->sendActorMessage(Endpoint, os.str(), res)) {
    setErrorStrm([&](auto &ost) {
      ost << " Failed to send actor command to " << Endpoint << " : "
          << Console->getLastError();
    });
    return false;
  }
  return true;
}
