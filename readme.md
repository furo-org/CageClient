# Cage PluginのCommActorと通信するライブラリ CageClient

## 概要

UE4用移動ロボットシミュレータプラグインであるCageの、通信コンポーネントCommActorと通信するライブラリおよびサンプルです。

 + 通信路のZMQ/JSONを隠蔽し、移動台車のステータス取得とコマンド送信を実装した高レベルAPI _cageclient.hh_
 + CommActorからステータスを受信する機能の実装 _subscriber.hh_
 + CommActorにコマンドを送信する機能の実装 _console.hh_
 + 受信した移動台車のステータス(JSON)を単に画面に表示するサンプル _sampleSubscriber_
 + UE4コンソールコマンド実行をリクエストするサンプル _simConsole_
 + ZMQ/JSONレベルの通信をpythonで実装したサンプル _sample*.py_
 + rosと連携するサンプル [cage_ros_stack](https://github.com/furo-org/cage_ros_stack)

ライブラリとしてのCageClientはheaderonlyなので、アプリケーションは_cageclient.hh_をincludeして依存ライブラリ(zeromq)をリンクすれば利用できます。

### 動作環境および依存ライブラリ

実際にビルドを確認している環境は Ubuntu 18.04および20.04です。
標準C++ライブラリのほか、[ZeroMQ](http://zeromq.org)と[nlohmann::json](https://github.com/nlohmann/json)を使用しています。これらのライブラリが動作する環境であればUbuntu 18.04/20.04以外でも多くの環境でビルドできることが期待できます。

なお、ROSのサンプルは Ubuntu 20.04 上の [ROS Noetic Ninjemys](http://wiki.ros.org/noetic/Installation) での動作を確認しています。また、Pythonのサンプルには [pyzmq](https://pyzmq.readthedocs.io/en/latest/) が必要です。

### License

This software is available under the [MIT License](https://opensource.org/licenses/mit-license.php).

## Quick Start

まずCage Pluginを導入したシミュレータを用意してください。ちょっと試してみるだけならば[VTC](https://github.com/furo-org/VTC)を[パッケージしたバイナリ(64bit Windows版)](https://github.com/furo-org/VTC/releases)を使ってみてください。zipを展開してVTC2018.exeを起動するだけです。全画面とウィンドウモードの切り替えはAlt-Enterで、終了はAlt-F4です。
なお、パッケージ版はUnreal Editorとは違い世界に干渉する手段がかなり限られますのでその点は注意が必要です。

ROSで動くプログラムと接続して使う場合には[cage_ros_stack](https://github.com/furo-org/cage_ros_stack)を(必要に応じて修正して)使うと良いでしょう。ROSで駆動されるもの以外のロボットのインタフェースを使う場合cage_ros_bridgeと同等のプログラムを用意する必要があります。cageclient.hhにCageに実装してあるロボットPuffinを動かすためのインタフェースを用意してあります。これを利用して既存の実機用フレームワークに適合するプログラムを作ってください。

シミュレータと通信して簡単な動きを指示する例をsampleRun.ccにざっと実装してありますので、まずはこれを眺めてみてください。sampleRun.ccは車輪の回転速度から並進移動量を計算し、10m直進したら止まって終了します。より詳しい使い方の例はROSのサンプルcage_ros_bridgeを参照してください。

ビルドはCMakeを使います。

```
mkdir build
cd build
cmake .. -DBUILD_CAGE_EXAMPLES=ON -DBUILD_CAGE_CLI=ON
make
./examples/sampleRun [シミュレータが動作するPCのIP]
```

sampleRunサンプルでは、以下のようにしてシミュレータに接続しています。

``` c++
  CageAPI cage(server);
  cage.connect();
```

その後は

``` c++
    CageAPI::vehicleStatus vst;
    cage.getStatusOne(vst);
```

としてステータスを受信し、

``` c++
  cage.setVW(0.20,0);
```

などとして車輪を回転させるコマンドを送信しています。その後ループで並進移動距離を積算し、10mに達したら停止するコマンドを送信するようにしています。

ユーザプログラムからCageClientを使う場合はCageClientを(git submoduleなどで)サブディレクトリに配置し、CMake で add_subdirectory してください。その後cageClientIFターゲットをリンクすれば必要な設定が行われます。

``` cmake
 add_subdirectory(CageClient)
 add_executable(UserCode usercode.cc)
 target_link_libraries(UserCode cageClientIF)
```

----

## 簡易リファレンス

### cageclient.hh

CommActorに接続し、指定したActorにコマンドを送信し、またステータスを受信する機能をまとめたものです。シミュレータに接続するにはCageAPIのインスタンスを作り、connect()を呼びます。接続先アドレスpeerAddrは省略できませんが、対象とするロボット名targetVehicleは省略でき、その場合最初に見つかったものを使います。

``` c++
 class CageAPI{
  CageAPI(std::string peerAddr);
  CageAPI(std::string peerAddr, std::string targetVehicle);
  bool connect();
```

コマンド送信は次の3つです。

``` c++
  bool setRpm(double rpmL, double rpmR);
  bool setVW(double V, double W);        // [m/s], [rad/s]
  bool setFLW(double F, double L, double W);        // [m/s], [rad/s]
```

これらを呼ぶと直ちにコマンドが送信されます。setRpmもsetVWもどちらも車輪の回転数を指示するコマンドで、setVWの場合はシミュレータ側で支持された速度を達成する左右の目標回転速度を計算します。setRpmはこれをバイパスして直接目標回転速度を与えることができます。setFLWは二自由度の並進移動を支持できるコマンドで、前後方向をFに、左を正とした左右方向をLに与えます。Puffinのような一自由度の並進移動しかできないロボットにsetFLWでコマンドを送った場合左右方向は無視されます。
通常はsetRpm, setVW, setFLWのどれか一つを使います。


適切な回転速度を求めるには車輪の大きさと配置を知る必要がありますが、これはconnect()時にシミュレータから値を取得し、CageAPI::VehicleInfo に格納されます。

``` c++
  struct vehicleInfo{
    std::string name;
    double WheelPerimeterL; // [m]
    double WheelPerimeterR; // [m]
    double TreadWidth;      // [m]
    double ReductionRatio;
  } VehicleInfo;
```

また、シミュレータ側が対応している場合世界の緯度経度基準点の情報がWorldInfoに入ります。
``` c++
  struct worldInfo{
    bool valid;
    double Latitude0;
    double Longitude0;
    std::array<double, 3> ReferenceLocation;
    std::array<double, 4> ReferenceRotation;
  } WorldInfo;
```

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
    double latitude, longitude;
  };
```

なお、UE4の座標系は左手系ですが、vehicleStatusには(Y軸を反転することで)右手系にしたものが入ります。

latitudeとlongitudeはシミュレータ側が報告できた場合に値が入ります。

### subscriber.hh

CommActorに接続し、指定したActorの情報を受信する手続きをまとめたものです。

### console.hh

CommActorに接続し、各種コマンドを送信する手続きをまとめたものです。

### simConsole

CommActorに接続し、操作可能なActorの列挙もしくはコンソールコマンドの実行をリクエストするプログラムです。CMakeのconfigure時にBUILD_CAGE_CLIスイッチをONにしている場合にビルドされます。

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

例えばVTCマップを開始するには次のようにします。

```
$ simConsole -s [IP Address] servertravel VTC
```

その他使えそうなコンソールコマンドの一部を次に列挙します。

#### servertravel [マップ名]

指定したマップに移動

#### quit

終了

### sampleConsole.py

CommActorにコンソールコマンドを送信する低レベルの送受信をPythonで記述したサンプルです。操作可能な台車を列挙したり台車のパラメータを取得するような機能はありません。

### sampleSubscriber.py, sampleSubscriber.cc

CommActor に接続し、流れてくる情報を画面に出力するサンプルです。
c++バージョンは cageclient API を使って実装しています。それに対しpythonバージョンはzmqpyを使い、低レベルの送受信をそのまま書いています。

### ~~cage_ros_bridge/~~

rosと接続するサンプル(cage_ros_bridge)です。[cage_ros_stack](https://github.com/furo-org/cage_ros_stack) リポジトリに移動しました。

### sampleRun

Quick Start で説明した、10m走行して止まるサンプルプログラムです。

## その他補足

### zmq_nt.hpp

[cppzmq](https://github.com/zeromq/cppzmq) の zmq.hpp を、例外を使わないインタフェースに書き換えたものです。
使用上の大きな違いは次のとおりです。

 1. コンストラクタがthrowしない代わりに、isValid()で正当性を確認する必要がある。
 2. recv*/send*は成功時に0を、失敗時にerrnoを返す。
 3. recv*/send*はEAGAIN時にも0を返さずに、EAGAINを返す。
 4. その他エラー時にerrnoが変化するメソッドはerrnoを返す。

ライセンスは元のcppzmqのライセンス(MIT)に従います。
