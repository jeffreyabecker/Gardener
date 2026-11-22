#include "WebLink.h"

#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/sockets.h>
#include <stdint.h>

#include "Form.h"
#include "core/Knob.h"
#include "core/Logger.h"
#include "core/RunState.h"
#include "machine/Machine.h"
#include "machine/SystemControl.h"

namespace gardener::runtime {
namespace {

GARDENER_KNOB_VALID(const char*, WifiApPrefix, GARDENER_WIFI_AP_PREFIX, "Gardener", true,
                    "the setup network's name");
GARDENER_KNOB_VALID(const char*, WifiApPassword, GARDENER_WIFI_AP_PASSWORD, "", true,
                    "the setup network's password, empty for an open network");
GARDENER_KNOB_VALID(uint32_t, WifiConnectTimeoutMs, GARDENER_WIFI_CONNECT_TIMEOUT_MS, 15000,
                    GARDENER_WIFI_CONNECT_TIMEOUT_MS > 0,
                    "how long a join gets before the portal comes up, in milliseconds");
GARDENER_KNOB_VALID(uint32_t, WifiRetryMs, GARDENER_WIFI_RETRY_MS, 5000, GARDENER_WIFI_RETRY_MS > 0,
                    "the wait between join retries, in milliseconds");
GARDENER_KNOB_VALID(uint32_t, WifiTaskStackWords, GARDENER_WIFI_TASK_STACK_WORDS, 4096,
                    GARDENER_WIFI_TASK_STACK_WORDS >= 2048, "the wifi task's stack, in words");
GARDENER_KNOB_VALID(uint16_t, HttpPort, GARDENER_HTTP_PORT, 80, GARDENER_HTTP_PORT > 0,
                    "the port the page and the API are served on");
GARDENER_KNOB_VALID(uint32_t, HttpStackBytes, GARDENER_HTTP_STACK_BYTES, 8192,
                    GARDENER_HTTP_STACK_BYTES >= 4096, "the httpd task's stack, in bytes");
GARDENER_KNOB_VALID(UBaseType_t, WifiTaskPriority, GARDENER_WIFI_TASK_PRIORITY, 1,
                    GARDENER_WIFI_TASK_PRIORITY <= 24, "a task priority below the config ceiling");
GARDENER_KNOB_VALID(UBaseType_t, WifiTaskCore, GARDENER_WIFI_TASK_CORE, 0,
                    GARDENER_WIFI_TASK_CORE < portNUM_PROCESSORS, "a core this part has");

constexpr uint32_t TickMs = 20;
constexpr const char* WwwRoot = "/www";
constexpr const char* PagePath = "/www/index.html";
constexpr const char* ConfigPath = "/wifi.conf";
constexpr uint8_t MaxRoutes = 12;
constexpr uint32_t MaxFormBytes = 256;
constexpr const char* FormType = "application/x-www-form-urlencoded";
constexpr int MaxScannedNetworks = 16;

// What an access point's name and key may be: 32 bytes of SSID, and WPA2's 8 to 63 characters — or
// nothing at all, which is an open network.
constexpr unsigned int MaxSetupNameChars = 32;
constexpr unsigned int MinSetupPassChars = 8;
constexpr unsigned int MaxSetupPassChars = 63;

void wifiTask(void* arg) {
  WebLink& link = *static_cast<WebLink*>(arg);
  for (;;) {
    link.tick(millis());
    vTaskDelay(pdMS_TO_TICKS(TickMs));
  }
}

}  // namespace

WebLink::WebLink(SystemControl& control, Logger& logger) : control_(control), logger_(logger) {}

void WebLink::begin() {
  // No format on a failed mount: the config lives here, and a partition that will not mount is a
  // board to look at over the cable rather than one to wipe.
  mounted_ = LittleFS.begin(false);
  if (mounted_) {
    int pages = 0;
    File tree = LittleFS.open(WwwRoot);
    for (File entry = tree.openNextFile(); entry; entry = tree.openNextFile())
      pages++;
    File page = LittleFS.open(PagePath, "r");
    String line = "page: " + String(WwwRoot) + ", " + String(pages) + " files, index.html ";
    if (page) {
      line += String((uint32_t)page.size());
      line += " bytes";
      page.close();
    } else {
      line += "missing";
    }
    logger_.info(line.c_str());
  } else {
    logger_.warn("page: no filesystem, the page will not serve");
  }
  load();

  // The suffix tells two boards on the same bench apart in a WiFi list, and it is what the setup
  // network is called until the board's file names it.
  String suffix((uint16_t)(ESP.getEfuseMac() & 0xFFFF), HEX);
  suffix.toUpperCase();
  while (suffix.length() < 4)
    suffix = "0" + suffix;

  // What the radio will take, checked here rather than at softAP: a name the radio truncates is one
  // nobody finds in a list, and a key outside WPA2's range makes startPortal fail silently.
  if (setupName_.length() > MaxSetupNameChars) {
    logger_.warn("wifi: the setup name is longer than 32 characters, using the board's own");
    setupName_ = "";
  }
  if (!setupPass_.isEmpty() &&
      (setupPass_.length() < MinSetupPassChars || setupPass_.length() > MaxSetupPassChars)) {
    logger_.warn("wifi: the setup password is not 8 to 63 characters, the portal comes up open");
    setupPass_ = "";
  }
  portalSsid_ = setupName_.isEmpty() ? String(WifiApPrefix) + "-" + suffix : setupName_;
  portalPass_ = setupPass_.isEmpty() ? String(WifiApPassword) : setupPass_;

  // The credentials are this link's, not the core's: persistent(false) stops WiFi writing a second
  // copy into NVS that could then disagree with the page, and the retries below own the schedule.
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);

  if (ssid_.isEmpty()) {
    startPortal();
  } else {
    startJoin();
  }
  xTaskCreatePinnedToCore(wifiTask, "wifi", WifiTaskStackWords, this, WifiTaskPriority, nullptr,
                          WifiTaskCore);

  // The server starts on whichever interface the radio has: the portal when there is no network,
  // the client address once there is one.
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = HttpPort;
  config.max_uri_handlers = MaxRoutes;
  config.stack_size = HttpStackBytes;
  config.lru_purge_enable = true;
  config.uri_match_fn = httpd_uri_match_wildcard;
  if (httpd_start(&server_, &config) != ESP_OK) {
    logger_.warn("http: server did not start");
    return;
  }

  route("/api/status", HTTP_GET, onStatus);
  route("/api/set", HTTP_POST, onSet);
  route("/api/vent", HTTP_POST, onVent);
  route("/api/reset", HTTP_POST, onReset);
  route("/api/wifi", HTTP_GET, onWifi);
  route("/api/wifi", HTTP_POST, onWifiSave);
  route("/api/wifi/scan", HTTP_GET, onScan);
  route("/api/wifi/forget", HTTP_POST, onWifiForget);
  // The page tree last, so it can only take what no route above did. The root is registered on its
  // own as well as under the wildcard, because the matcher's treatment of the bare slash is not
  // something to bet the page on.
  route("/", HTTP_GET, onStatic);
  route("/*", HTTP_GET, onStatic);

  String line = "http: server on port ";
  line += String(HttpPort);
  logger_.info(line.c_str());

  startMdns();
}

void WebLink::startMdns() {
  // A host label is lower case, so it is the setup network's own name lowered: one word for the
  // board whether the client reached it through the access point or the network it joined.
  String host = portalSsid_;
  host.toLowerCase();
  // A label's alphabet is narrower than an SSID's, so the prefix's other characters become hyphens
  // rather than a host name the responder refuses.
  for (size_t i = 0; i < host.length(); i++) {
    const char c = host[i];
    if ((c < 'a' || c > 'z') && (c < '0' || c > '9') && c != '-') host[i] = '-';
  }
  if (!MDNS.begin(host.c_str())) {
    logger_.warn("mdns: responder did not start");
    return;
  }
  // The page has a name a browser and a service browser each know; the API gets one of its own so
  // a client that finds it knows where the calls start without reading the page.
  MDNS.addService("http", "tcp", HttpPort);
  MDNS.addServiceTxt("http", "tcp", "path", "/");
  MDNS.addService("gardener", "tcp", HttpPort);
  MDNS.addServiceTxt("gardener", "tcp", "api", "/api");

  String line = "mdns: " + host + ".local, _http._tcp at /, _gardener._tcp at /api";
  logger_.info(line.c_str());
}

void WebLink::tick(uint32_t nowMs) {
  if (portalUp_) dns_.processNextRequest();
  if (ssid_.isEmpty()) return;

  if (WiFi.status() == WL_CONNECTED) {
    joining_ = false;
    portalMandatory_ = false;
    if (!joinedReported_) {
      joinedReported_ = true;
      String line = "wifi: joined " + ssid_ + " at " + WiFi.localIP().toString();
      logger_.info(line.c_str());
    }
    if (!keepPortal_ && portalUp_) stopPortal();
    return;
  }

  if (joinedReported_) {
    joinedReported_ = false;
    joining_ = true;
    joinStartMs_ = nowMs;
    lastRetryMs_ = nowMs;
    String line = "wifi: lost " + ssid_;
    logger_.warn(line.c_str());
    return;
  }

  if (!joining_) {
    joining_ = true;
    joinStartMs_ = nowMs;
    lastRetryMs_ = nowMs;
  }
  if (!portalMandatory_ && nowMs - joinStartMs_ >= WifiConnectTimeoutMs) {
    portalMandatory_ = true;
    if (startPortal()) {
      logger_.warn("wifi: join failed, the portal is up");
    } else {
      logger_.warn("wifi: join failed, and the portal did not start");
    }
  }
  if (nowMs - lastRetryMs_ >= WifiRetryMs) {
    lastRetryMs_ = nowMs;
    WiFi.reconnect();
  }
}

// --- the radio -------------------------------------------------------------

void WebLink::startJoin() {
  joinedReported_ = false;
  portalMandatory_ = false;
  joining_ = true;
  joinStartMs_ = millis();
  lastRetryMs_ = joinStartMs_;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid_.c_str(), pass_.c_str());
  String line = "wifi: joining " + ssid_;
  logger_.info(line.c_str());
}

void WebLink::join() {
  // Nothing to join is the portal, not a radio left in a client mode with no network.
  if (ssid_.isEmpty()) {
    startPortal();
    return;
  }

  joinedReported_ = false;
  portalMandatory_ = false;
  joining_ = true;
  joinStartMs_ = millis();
  lastRetryMs_ = joinStartMs_;

  WiFi.mode(portalUp_ ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(ssid_.c_str(), pass_.c_str());
  String line = "wifi: joining " + ssid_;
  logger_.info(line.c_str());
}

void WebLink::setSsid(const String& ssid) {
  ssid_ = ssid;
  save();
}

void WebLink::setPass(const String& pass) {
  pass_ = pass;
  save();
}

void WebLink::setKeepPortal(bool keepPortal) {
  keepPortal_ = keepPortal;
  save();
}

WifiStatus WebLink::status() const {
  WifiStatus now;
  now.ssid = ssid_;
  now.address = address();
  now.mode = modeName();
  now.keepPortal = keepPortal_;
  return now;
}

void WebLink::forget() {
  ssid_ = "";
  pass_ = "";
  keepPortal_ = false;
  save();

  joinedReported_ = false;
  joining_ = false;
  portalMandatory_ = true;
  WiFi.disconnect(true);
  if (portalUp_) {
    // The portal outlives the forgotten network; only its client side goes.
    WiFi.mode(WIFI_AP);
  } else {
    startPortal();
  }
  logger_.info("wifi: network forgotten");
}

bool WebLink::startPortal() {
  if (portalUp_) return true;

  // softAP's result is the only word on whether there is a portal at all: it fails when the radio
  // will not run one, and the log below would otherwise promise an access point nobody can see.
  WiFi.mode(ssid_.isEmpty() ? WIFI_AP : WIFI_AP_STA);
  if (!WiFi.softAP(portalSsid_.c_str(), portalPass_.c_str())) {
    logger_.warn("wifi: the access point did not start");
    return false;
  }
  dns_.start(53, "*", WiFi.softAPIP());
  portalUp_ = true;

  String line = "wifi: portal " + portalSsid_ + " at " + WiFi.softAPIP().toString();
  if (!portalPass_.isEmpty()) line += " keyed";
  logger_.info(line.c_str());
  return true;
}

void WebLink::stopPortal() {
  dns_.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  portalUp_ = false;
  logger_.info("wifi: portal closed");
}

void WebLink::save() const {
  String line = "ssid=";
  line += formEncoded(ssid_);
  line += "&pass=";
  line += formEncoded(pass_);
  line += "&keep_ap=";
  line += keepPortal_ ? "1" : "0";
  // The setup network's own two fields go back exactly as the file gave them, empty when it did not:
  // a save stores a choice, and never turns the board's default name into one.
  line += "&ap_ssid=";
  line += formEncoded(setupName_);
  line += "&ap_pass=";
  line += formEncoded(setupPass_);

  File file = LittleFS.open(ConfigPath, "w");
  if (!file) {
    logger_.warn("wifi: the network could not be stored");
    return;
  }
  file.print(line);
  file.close();
}

void WebLink::load() {
  File file = LittleFS.open(ConfigPath, "r");
  if (!file) return;

  const String line = file.readString();
  file.close();
  ssid_ = formField(line, "ssid");
  pass_ = formField(line, "pass");
  keepPortal_ = formField(line, "keep_ap") == "1";
  setupName_ = formField(line, "ap_ssid");
  setupPass_ = formField(line, "ap_pass");
}

String WebLink::address() const {
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP().toString();
  return portalUp_ ? WiFi.softAPIP().toString() : String("-");
}

const char* WebLink::modeName() const {
  if (portalUp_ && WiFi.status() == WL_CONNECTED) return "ap+sta";
  if (portalUp_) return "ap";
  return "sta";
}

int WebLink::scan(WifiNetwork* networks, int capacity) {
  const int found = WiFi.scanNetworks();
  int stored = 0;
  for (int i = 0; i < found && stored < capacity; i++) {
    networks[stored].ssid = WiFi.SSID(i);
    networks[stored].rssi = WiFi.RSSI(i);
    stored++;
  }
  WiFi.scanDelete();
  return stored;
}

// --- the server ------------------------------------------------------------

void WebLink::route(const char* uri, http_method method, esp_err_t (*handler)(httpd_req_t*)) {
  httpd_uri_t definition = {};
  definition.uri = uri;
  definition.method = method;
  definition.handler = handler;
  definition.user_ctx = this;
  httpd_register_uri_handler(server_, &definition);
}

WebLink* WebLink::from(httpd_req_t* request) { return static_cast<WebLink*>(request->user_ctx); }

esp_err_t WebLink::reply(httpd_req_t* request, const char* code, const char* type,
                         const String& body) {
  httpd_resp_set_status(request, code);
  httpd_resp_set_type(request, type);
  return httpd_resp_send(request, body.c_str(), body.length());
}

esp_err_t WebLink::onStatic(httpd_req_t* request) {
  // A GET is a path inside the document root and nothing else: no climbing out of it, and no
  // reaching the config that sits beside it.
  String path = WwwRoot;
  path += request->uri;
  if (path.indexOf("..") >= 0) return miss(request);
  if (path.endsWith("/")) path += "index.html";

  File file;
  if (from(request)->mounted_) file = LittleFS.open(path, "r");
  if (!file) return miss(request);

  const String body = file.readString();
  file.close();
  return reply(request, "200 OK", typeOf(path), body);
}

// A path nothing serves. On the setup access point that is the portal's landing — the OS probes ask
// for URLs of their own, and a redirect to the page is what opens the sheet — but only while there
// is a page to land on: a redirect with no index behind it would just loop.
esp_err_t WebLink::miss(httpd_req_t* request) {
  WebLink* self = from(request);
  if (!self->mounted_ || !LittleFS.exists(PagePath)) {
    return reply(request, "404 Not Found", FormType, "ok=0&err=no_page");
  }
  if (self->portalUp_ && onPortal(request)) {
    httpd_resp_set_status(request, "302 Found");
    httpd_resp_set_hdr(request, "Location", "/");
    return httpd_resp_send(request, nullptr, 0);
  }
  return reply(request, "404 Not Found", "text/plain", "not found\n");
}

const char* WebLink::typeOf(const String& path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".js")) return "text/javascript";
  if (path.endsWith(".svg")) return "image/svg+xml";
  if (path.endsWith(".png")) return "image/png";
  if (path.endsWith(".ico")) return "image/x-icon";
  return "application/octet-stream";
}

esp_err_t WebLink::onStatus(httpd_req_t* request) {
  const SystemStatus now = from(request)->control_.status();

  String body = "ok=1&uptime_ms=";
  body += String(now.uptimeMs);
  body += "&pressure_kpa=";
  body += String(now.pressureKpa, 2u);
  body += "&target=";
  body += String(now.targetKpa, 1u);
  body += "&state=";
  body += runStateName(now.state);
  body += "&duty=";
  body += String(now.dutyPercent, 1u);
  body += "&fault=";
  body += (now.fault[0] ? now.fault : "none");
  return reply(request, "200 OK", FormType, body);
}

esp_err_t WebLink::onSet(httpd_req_t* request) {
  WebLink* self = from(request);
  const String token = formField(bodyOf(request), "target");
  if (token.isEmpty()) {
    return reply(request, "400 Bad Request", FormType, "ok=0&err=missing_target");
  }

  // `-` stays as the shorthand for the ambient target, which is what 0 means as well.
  float targetKpa = 0.0f;
  if (token != "-") targetKpa = token.toFloat();

  switch (self->control_.set(targetKpa)) {
    case ServeResult::Ok: {
      String body = "ok=1&target=";
      body += String(targetKpa, 1u);
      body += "&state=PULL";
      return reply(request, "200 OK", FormType, body);
    }
    case ServeResult::Released:
      return reply(request, "200 OK", FormType, "ok=1&target=0.0&state=VENT");
    case ServeResult::BelowRange: {
      String body = "ok=0&err=out_of_range&field=target&value=";
      body += formEncoded(token);
      body += "&min=0.0";
      return reply(request, "400 Bad Request", FormType, body);
    }
    case ServeResult::AboveRange: {
      String body = "ok=0&err=out_of_range&field=target&value=";
      body += formEncoded(token);
      body += "&max=";
      body += String(ControlFloorKpa, 1u);
      return reply(request, "400 Bad Request", FormType, body);
    }
    case ServeResult::NotReady: {
      String body = "ok=0&err=not_ready&reason=";
      body += self->control_.refusal();
      return reply(request, "409 Conflict", FormType, body);
    }
  }

  return reply(request, "400 Bad Request", FormType, "ok=0&err=unknown");
}

esp_err_t WebLink::onVent(httpd_req_t* request) {
  from(request)->control_.vent();
  return reply(request, "200 OK", FormType, "ok=1&state=VENT");
}

esp_err_t WebLink::onReset(httpd_req_t* request) {
  from(request)->control_.reset();
  return reply(request, "200 OK", FormType, "ok=1&state=IDLE");
}

esp_err_t WebLink::onWifi(httpd_req_t* request) {
  const WebLink* self = from(request);

  String body = "ok=1&ssid=";
  body += formEncoded(self->ssid_);
  body += "&mode=";
  body += self->modeName();
  body += "&ip=";
  body += self->address();
  body += "&keep_ap=";
  body += self->keepPortal_ ? "1" : "0";
  return reply(request, "200 OK", FormType, body);
}

esp_err_t WebLink::onScan(httpd_req_t* request) {
  WifiNetwork networks[MaxScannedNetworks];
  const int count = from(request)->scan(networks, MaxScannedNetworks);

  String body = "ok=1&count=";
  body += String(count);
  for (int i = 0; i < count; i++) {
    body += "&ssid";
    body += String(i);
    body += "=";
    body += formEncoded(networks[i].ssid);
    body += "&rssi";
    body += String(i);
    body += "=";
    body += String(networks[i].rssi);
  }
  return reply(request, "200 OK", FormType, body);
}

esp_err_t WebLink::onWifiSave(httpd_req_t* request) {
  const String body = bodyOf(request);
  const String ssid = formField(body, "ssid");
  const String pass = formField(body, "pass");
  const bool keepPortal = formField(body, "keep_ap") == "1";

  if (ssid.isEmpty()) {
    return reply(request, "400 Bad Request", FormType, "ok=0&err=missing_ssid");
  }

  from(request)->setSsid(ssid);
  from(request)->setPass(pass);
  from(request)->setKeepPortal(keepPortal);
  from(request)->join();
  return reply(request, "200 OK", FormType, "ok=1");
}

esp_err_t WebLink::onWifiForget(httpd_req_t* request) {
  from(request)->forget();
  return reply(request, "200 OK", FormType, "ok=1");
}

bool WebLink::onPortal(httpd_req_t* request) {
  struct sockaddr_in peer;
  socklen_t length = sizeof(peer);
  const int socket = httpd_req_to_sockfd(request);
  if (socket < 0 || getpeername(socket, (struct sockaddr*)&peer, &length) != 0) return false;

  // lwIP holds the peer in network order, and IPAddress's uint32 is the same byte order on this
  // part, so the two compare directly over the /24 the access point hands out.
  const uint32_t client = peer.sin_addr.s_addr;
  const uint32_t accessPoint = static_cast<uint32_t>(WiFi.softAPIP());
  return (client & 0x00FFFFFFu) == (accessPoint & 0x00FFFFFFu);
}

// --- the request body ------------------------------------------------------

String WebLink::bodyOf(httpd_req_t* request) {
  if (request->content_len == 0 || request->content_len > MaxFormBytes) return String();

  char buffer[MaxFormBytes + 1];
  size_t total = 0;
  while (total < request->content_len) {
    const int read = httpd_req_recv(request, buffer + total, request->content_len - total);
    if (read <= 0) {
      if (read == HTTPD_SOCK_ERR_TIMEOUT) continue;
      return String();
    }
    total += read;
  }
  buffer[total] = '\0';
  return String(buffer);
}

}  // namespace gardener::runtime
