#pragma once

#include "esphome/core/defines.h"
#if defined(USE_NETWORK) && !defined(USE_ZEPHYR)

#include "esphome/components/amg8833/amg8833.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/component.h"

namespace esphome::amg8833 {

// Serves a live 8x8 heatmap page for an AMG8833Component:
//   GET /grid       -> self-contained HTML/CSS/JS page
//   GET /grid.json  -> the cached frame as JSON, polled by that page
//
// Both run in the async web server task, so they only ever read the snapshot
// the component publishes under its frame lock -- no I2C from here. That also
// means the page can never be fresher than the component's update_interval.
class AMG8833WebHandler : public AsyncWebHandler, public Component {
 public:
  AMG8833WebHandler(web_server_base::WebServerBase *base, AMG8833Component *parent) : base_(base), parent_(parent) {}

  bool canHandle(AsyncWebServerRequest *request) const override {
    if (request->method() != HTTP_GET)
      return false;
#ifdef USE_ESP32
    char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
    auto url = request->url_to(url_buf);
    return url == "/grid" || url == "/grid.json";
#else
    return request->url() == "/grid" || request->url() == "/grid.json";
#endif
  }

  void handleRequest(AsyncWebServerRequest *req) override;

  void setup() override {
    this->base_->init();
    this->base_->add_handler(this);
  }

  void dump_config() override;

  float get_setup_priority() const override {
    // After WiFi, same as the other web handlers
    return setup_priority::WIFI - 1.0f;
  }

 protected:
  void handle_json_(AsyncWebServerRequest *req);

  web_server_base::WebServerBase *base_;
  AMG8833Component *parent_;
};

}  // namespace esphome::amg8833

#endif  // USE_NETWORK && !USE_ZEPHYR
