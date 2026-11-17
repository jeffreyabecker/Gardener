// The network's commands: the status the wire answers with, the scan list and how a name with a
// space or a quote survives it, the stored choice a field at a time, the join that applies it, the
// forget that clears it, and the refusals. The control is fake, because what a case asserts is what
// the wire asked and what it printed, not what a radio did.

#include <doctest.h>

#include <Arduino.h>

#include <string>
#include <vector>

#include "Clock.h"
#include "TextStream.h"
#include "fakes/Parts.h"
#include "fakes/Wifi.h"
#include "machine/CommandQueue.h"
#include "machine/Machine.h"
#include "machine/SystemControl.h"
#include "serial/SerialLink.h"

namespace {

using gardener::CommandQueue;
using gardener::Machine;
using gardener::MachineCommand;
using gardener::SerialLink;
using gardener::SystemControl;
using gardener::WifiNetwork;
using gardener::host::TextStream;
using gardener::test::FakePump;
using gardener::test::FakeSensor;
using gardener::test::FakeValve;
using gardener::test::FakeWifi;
using gardener::test::RecordingLogger;

// The queue the link posts into: everything fits. The machine's commands are not what these cases
// are about, but the link needs somewhere to post them.
class OpenQueue : public CommandQueue {
 public:
  bool post(const MachineCommand& command) override {
    commands_.push_back(command);
    return true;
  }

  bool take(MachineCommand& command) override {
    if (commands_.empty()) return false;
    command = commands_.front();
    commands_.erase(commands_.begin());
    return true;
  }

 private:
  std::vector<MachineCommand> commands_;
};

struct Bench {
  Bench()
      : machine(logger, &pump, &sensor, &valve), control(machine.vacuum(), queue),
        link(io, nullptr, logger, control, wifi) {
    gardener::host::reset();
    machine.boot();
    link.begin();
  }

  void send(const char* text) {
    io.clearPrinted();
    io.type(text);
    link.poll(gardener::host::nowMs());
  }

  std::string reply() const { return io.printed(); }

  FakePump pump;
  FakeSensor sensor;
  FakeValve valve;
  FakeWifi wifi;
  RecordingLogger logger;
  OpenQueue queue;
  Machine machine;
  SystemControl control;
  TextStream io;
  SerialLink link;
};

}  // namespace

TEST_CASE("wifi answers the stored choice and the radio's state") {
  Bench bench;
  bench.send("wifi\n");
  CHECK_EQ(bench.reply(), std::string("ok ssid=\"\" mode=ap ip=- keep_ap=0\n"));
}

TEST_CASE("an uppercase wifi verb and sub-verb are the same command") {
  Bench bench;
  bench.send("WIFI KEEP ON\n");
  CHECK_EQ(bench.reply(), std::string("ok keep_ap=1\n"));
}

TEST_CASE("wifi ssid stores the rest of the line, spaces and all") {
  Bench bench;
  bench.send("wifi ssid My Net\n");
  CHECK_EQ(bench.reply(), std::string("ok ssid=\"My Net\"\n"));
  CHECK_EQ(std::string(bench.wifi.ssid().c_str()), std::string("My Net"));
}

TEST_CASE("a quoted name and a bare one are the same name") {
  Bench bench;
  bench.send("wifi ssid \"My Net\"\n");
  CHECK_EQ(bench.reply(), std::string("ok ssid=\"My Net\"\n"));
  CHECK_EQ(std::string(bench.wifi.ssid().c_str()), std::string("My Net"));
}

TEST_CASE("a quote inside a quoted name is escaped and read back") {
  Bench bench;
  bench.send("wifi ssid \"Bob\\\"s Net\"\n");
  CHECK_EQ(bench.reply(), std::string("ok ssid=\"Bob\\\"s Net\"\n"));
  CHECK_EQ(std::string(bench.wifi.ssid().c_str()), std::string("Bob\"s Net"));
}

TEST_CASE("a name that needs no quotes is answered bare") {
  Bench bench;
  bench.send("wifi ssid Home\n");
  CHECK_EQ(bench.reply(), std::string("ok ssid=Home\n"));
}

TEST_CASE("wifi ssid with nothing after it is refused") {
  Bench bench;
  bench.send("wifi ssid\n");
  CHECK_EQ(bench.reply(), std::string("err bad_argument ssid\n"));
}

TEST_CASE("wifi pass stores the password and never repeats it") {
  Bench bench;
  bench.send("wifi pass hunter two\n");
  CHECK_EQ(bench.reply(), std::string("ok\n"));
  CHECK_EQ(std::string(bench.wifi.pass().c_str()), std::string("hunter two"));
}

TEST_CASE("wifi pass with nothing after it clears the password") {
  Bench bench;
  bench.send("wifi pass secret\n");
  bench.send("wifi pass\n");
  CHECK_EQ(bench.reply(), std::string("ok\n"));
  CHECK(bench.wifi.pass().isEmpty());
}

TEST_CASE("wifi keep takes on and off and refuses anything else") {
  Bench bench;
  bench.send("wifi keep on\n");
  CHECK_EQ(bench.reply(), std::string("ok keep_ap=1\n"));
  CHECK(bench.wifi.keepPortal());

  bench.send("wifi keep off\n");
  CHECK_EQ(bench.reply(), std::string("ok keep_ap=0\n"));
  CHECK(!bench.wifi.keepPortal());

  bench.send("wifi keep maybe\n");
  CHECK_EQ(bench.reply(), std::string("err bad_argument maybe\n"));
}

TEST_CASE("a join with nothing stored is its own refusal") {
  Bench bench;
  bench.send("wifi join\n");
  CHECK_EQ(bench.reply(), std::string("err no_network\n"));
  CHECK(!bench.wifi.joined());
}

TEST_CASE("a join applies the stored choice and answers the new state") {
  Bench bench;
  bench.send("wifi ssid Home\n");
  bench.send("wifi pass secret\n");
  bench.send("wifi join\n");
  CHECK_EQ(bench.reply(), std::string("ok ssid=Home mode=sta ip=10.0.0.9 keep_ap=0\n"));
  CHECK(bench.wifi.joined());
  CHECK_EQ(std::string(bench.wifi.ssid().c_str()), std::string("Home"));
}

TEST_CASE("a scan lists the networks one per line and quotes the names that need it") {
  Bench bench;
  bench.wifi.visible({{"Home", -55}, {"Net Two", -71}, {"Bob\"s", -80}});
  bench.send("wifi scan\n");
  CHECK_EQ(bench.reply(), std::string("ok count=3\n"
                                      "ssid0=Home rssi0=-55\n"
                                      "ssid1=\"Net Two\" rssi1=-71\n"
                                      "ssid2=\"Bob\\\"s\" rssi2=-80\n"));
}

TEST_CASE("a scan that finds nothing says so") {
  Bench bench;
  bench.send("wifi scan\n");
  CHECK_EQ(bench.reply(), std::string("ok count=0\n"));
}

TEST_CASE("a scan with an argument is refused") {
  Bench bench;
  bench.send("wifi scan now\n");
  CHECK_EQ(bench.reply(), std::string("err bad_argument now\n"));
}

TEST_CASE("forget clears the choice and answers the portal's state") {
  Bench bench;
  bench.send("wifi ssid Home\n");
  bench.send("wifi join\n");
  bench.send("wifi forget\n");
  CHECK_EQ(bench.reply(), std::string("ok ssid=\"\" mode=ap ip=- keep_ap=0\n"));
  CHECK(bench.wifi.ssid().isEmpty());
  CHECK(!bench.wifi.joined());
}

TEST_CASE("a sub-verb nothing serves is refused by name") {
  Bench bench;
  bench.send("wifi bogus\n");
  CHECK_EQ(bench.reply(), std::string("err bad_argument bogus\n"));
}
