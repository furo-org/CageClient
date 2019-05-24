# CageのCommActorと通信するライブラリ CageClient

## 概要

UE4用移動ロボットシミュレータプラグインであるCageの、通信コンポーネントCommActorと通信するライブラリおよびサンプルです。

 + 通信路のZMQ/JSONを隠蔽し、移動台車のステータス取得とコマンド送信を実装した高レベルAPI _cageclient.hh_
 + CommActorからステータスを受信する機能の実装 _subscriber.hh_
 + CommActorにコマンドを送信する機能の実装 _console.hh_
 + 受信した移動台車のステータス(JSON)を単に画面に表示するサンプル _sampleSubscriber_
 + UE4コンソールコマンド実行をリクエストするサンプル _simConsole_
 + ZMQ/JSONレベルの通信をpythonで実装したサンプル _sample*.py_
 + rosと連携するサンプル _cage_ros_bridge/_

## できること

### cageclient.hh

CommActorに接続し、指定したActorにコマンドを送信し、またステータスを受信する機能をまとめたものです。コマンド送信は次の2つです。

``` c++
  bool setRpm(double rpmL, double rpmR);
  bool setVW(double V, double W);        // [m/s], [rad/s]
```

これらを呼ぶと直ちにコマンドが送信されます。

ステータス受信の主要なインタフェースは次のとおりです。

``` c++
  bool poll(int timeout_us=-1);
  bool getStatusOne(CageAPI::vehicleStatus &vst, int timeout_us = -1);
```

getStatusOneを呼ぶと台車の情報(CageAPI::vehicleStatus)が得られます。

``` c++
  struct vehicleStatus{
    double simClock;   // timestamp in simulated world [s]
    double lrpm, rrpm;
    double ax, ay, az; // accel [m/s^2]
    double rx, ry, rz; // rotational velocity [rad/s]
    // ground truth
    double ox, oy, oz, ow; // orientation
    double wx, wy, wz; // world position
  };
```

なお、UE4の座標系は左手系ですが、vehicleStatusには右手系にしたものが入ります。

### subscriber.hh

CommActorに接続し、指定したActorの情報を受信する手続きをまとめたものです。

### console.hh

CommActorに接続し、各種コマンドを送信する手続きをまとめたものです。

### simConsole

CommActorに接続し、操作可能なActorの列挙もしくはコンソールコマンドの実行をリクエストします。

操作可能な移動体を列挙するには次のように実行します。

```
$ simConsole -s [IP Address] -e Vehicle
Result: 
 [PuffinBP_2]
{
    "ReductionRatio": 15.0,
    "TreadWidth": 38.0,
    "WheelPerimeterL": 62.793972,
    "WheelPerimeterR": 62.793972
}
```

コンソールコマンドを送るには -e オプションをつけずに、実行するコマンドを書きます。

```
$ simConsole -s [IP Address] [console command]
```

例えばTC2018マップを開始するには次のようにします。

```
$ simConsole -s [IP Address] servertravel TC2018
```

その他使えそうなコンソールコマンドの一部を次に列挙します。

#### servertravel [マップ名]

指定したマップに移動

#### stat FPS

現在のFPSと1フレームの処理に要した時間を表示

#### toggleDebugCamera

操作している人から離れ、自由な視点から観測

#### quit

終了

### sampleConsole.py

CommActorにコンソールコマンドを送信する低レベルの送受信をPythonで記述したサンプルです。操作可能な台車を列挙したり台車のパラメータを取得するような機能はありません。

### sampleSubscriber.py, sampleSubscriber.cc

CommActor に接続し、流れてくる情報を画面に出力するサンプルです。
c++バージョンは cageclient API を使って実装しています。それに対しpythonバージョンはzmqpyを使い、低レベルの送受信をそのまま書いています。

### cage_ros_bridge/

rosと接続するサンプル(cage_ros_bridge)です。以下の機能を持ちます。
 
 + cmd_vel/トピックを購読し、setVWコマンドを送信する
 + odom/ トピックに車輪回転速度とyaw軸角速度を積算したオドメトリを配信する
 + odom_gt/ トピックに位置と姿勢を配信する
 + imu/ トピックに加速度と角速度を配信する

## その他補足

### zmq_nt.hpp

[cppzmq](https://github.com/zeromq/cppzmq) の zmq.hpp を、例外を使わないインタフェースに書き換えたものです。
使用上の大きな違いは次のとおりです。

 1. コンストラクタがthrowしない代わりに、isValid()で正当性を確認する必要がある。
 2. recv*/send*は成功時に0を、失敗時にerrnoを返す。
 3. recv*/send*はEAGAIN時にも0を返さずに、EAGAINを返す。
 4. その他エラー時にerrnoが変化するメソッドはerrnoを返す。

ライセンスは元のcppzmqのライセンス(MIT)に従います。
