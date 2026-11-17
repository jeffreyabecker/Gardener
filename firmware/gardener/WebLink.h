#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <esp_http_server.h>
#include <stdint.h>

#include "core/WifiControl.h"

namespace gardener {
class Logger;
class SystemControl;
}  // namespace gardener

namespace gardener::runtime {

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
// Every machine call goes through the same SystemControl the serial grammar uses; the results are
// this link's own wording, form-encoded, because a form body is what a browser page reads back
// without a parser.
//
// It owns its beat: the join's timeout and retries and the portal's DNS are pumped by tick() on the
// task begin() starts, because DNSServer has no task of its own. The HTTP handlers run on the
// httpd's, and only ever read the control or post into its queue.
//
// The network half of that control is WifiControl, which the serial link holds as well: one radio,
// reached from the page and from the wire.
class WebLink : public WifiControl {
 public:
  WebLink(SystemControl& control, Logger& logger);

  // Loads the stored network, starts the radio and its task, then the server.
  void begin();
  void tick(uint32_t nowMs);

  // --- the network, as a front end asks it ---------------------------------
  WifiStatus status() const override;
  int scan(WifiNetwork* networks, int capacity) override;
  void setSsid(const String& ssid) override;
  void setPass(const String& pass) override;
  void setKeepPortal(bool keepPortal) override;
  void join() override;
  void forget() override;

 private:
  // --- the radio -----------------------------------------------------------
  void startJoin();
  void startPortal();
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
  String portalSsid_;
  DNSServer dns_;
  bool mounted_ = false;

  httpd_handle_t server_ = nullptr;
};

}  // namespace gardener::runtime
