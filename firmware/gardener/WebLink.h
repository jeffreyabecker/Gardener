#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <esp_http_server.h>
#include <stdint.h>

namespace gardener {
class Logger;
class SystemControl;
}  // namespace gardener

namespace gardener::runtime {

// One network a scan found, as the page lists it.
struct WifiNetwork {
  String ssid;
  int32_t rssi = 0;
};

// The device's web link: the page and the API over HTTP, and the radio that carries them. It serves
// on whichever interface the radio has up, and when there is no network to join it is the setup
// access point itself — the DNS server answers every name with that address, so a phone that joins
// it opens the page. It answers mDNS under the setup network's own name, so the page has an address
// before the client knows the board's.
//
// The page is not in the firmware: everything under `data/www` of the sketch becomes the image in
// the board's LittleFS partition, and a GET is the matching path inside that tree, so a stylesheet
// or a second page is a file in the folder rather than a rebuild. The network choice is a file in
// the same partition, at the filesystem root where no URI can reach it.
//
// Every machine call goes through the same SystemControl the Bluetooth link asks; the results are
// this link's own wording, form-encoded, because a form body is what a browser page reads back
// without a parser.
//
// It owns its beat: the join's timeout and retries and the portal's DNS are pumped by tick() on the
// task begin() starts, because DNSServer has no task of its own. The HTTP handlers run on the
// httpd's, and only ever read the control or post into its queue.
class WebLink {
 public:
  WebLink(SystemControl& control, Logger& logger);

  // Loads the stored network, starts the radio and its task, then the server.
  void begin();
  void tick(uint32_t nowMs);

 private:
  // --- the radio -----------------------------------------------------------
  int scan(WifiNetwork* networks, int capacity);
  void setSsid(const String& ssid);
  void setPass(const String& pass);
  void setKeepPortal(bool keepPortal);
  void join();
  void forget();
  void startJoin();
  bool startPortal();
  void stopPortal();
  void save() const;
  void load();
  String address() const;
  const char* modeName() const;

  // --- the server ----------------------------------------------------------
  static esp_err_t onStatus(httpd_req_t* request);
  static esp_err_t onSet(httpd_req_t* request);
  static esp_err_t onVent(httpd_req_t* request);
  static esp_err_t onReset(httpd_req_t* request);
  static esp_err_t onWifi(httpd_req_t* request);
  static esp_err_t onScan(httpd_req_t* request);
  static esp_err_t onWifiSave(httpd_req_t* request);
  static esp_err_t onWifiForget(httpd_req_t* request);
  static esp_err_t onStatic(httpd_req_t* request);
  static esp_err_t miss(httpd_req_t* request);
  static const char* typeOf(const String& path);

  static WebLink* from(httpd_req_t* request);
  void route(const char* uri, http_method method, esp_err_t (*handler)(httpd_req_t*));
  static esp_err_t reply(httpd_req_t* request, const char* code, const char* type,
                         const String& body);
  static String bodyOf(httpd_req_t* request);
  static bool onPortal(httpd_req_t* request);

  // --- the board's name on the network -------------------------------------
  void startMdns();

  SystemControl& control_;
  Logger& logger_;

  // The network and the choice that outlives a power cut.
  String ssid_;
  String pass_;
  bool keepPortal_ = false;
  bool portalUp_ = false;
  bool joining_ = false;
  bool portalMandatory_ = false;
  bool joinedReported_ = false;
  uint32_t joinStartMs_ = 0;
  uint32_t lastRetryMs_ = 0;
  // The setup network's configured pair, kept apart from the effective one so a save writes a choice
  // back and never a default: empty here means the board derives its own name and the knob decides.
  String setupName_;
  String setupPass_;
  String portalSsid_;
  String portalPass_;
  DNSServer dns_;
  bool mounted_ = false;

  httpd_handle_t server_ = nullptr;
};

}  // namespace gardener::runtime
