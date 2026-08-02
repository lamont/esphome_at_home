#include "amg8833_web.h"
#if defined(USE_NETWORK) && !defined(USE_ZEPHYR)

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::amg8833 {

static const char *const TAG = "amg8833_web";

// Self-contained: no CDN, no external assets, so the page works on an isolated
// network. Auto-scales the color ramp to the current frame's min/max.
static const char GRID_PAGE[] = R"GRID(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Grid-EYE</title>
<style>
 body{margin:0;padding:1.5rem;background:#12141a;color:#e6e8ee;
      font:14px/1.4 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
      display:flex;flex-direction:column;align-items:center;gap:1rem}
 h1{font-size:1rem;font-weight:600;letter-spacing:.08em;text-transform:uppercase;
    color:#8b93a7;margin:0}
 #g{display:grid;grid-template-columns:repeat(8,1fr);gap:3px;
    width:min(92vw,520px);aspect-ratio:1}
 .c{display:flex;align-items:center;justify-content:center;border-radius:4px;
    font-size:clamp(9px,2.2vw,13px);font-variant-numeric:tabular-nums;
    color:#fff;text-shadow:0 1px 2px rgba(0,0,0,.65);
    transition:background .25s linear;outline:2px solid transparent}
 .c.hot{outline-color:#fff}
 .c.cold{outline-color:rgba(255,255,255,.35)}
 #stats{color:#c3c9d8;font-variant-numeric:tabular-nums}
 #scale{color:#6c7488;font-size:12px}
 #err{color:#ff6b6b;min-height:1.2em;font-size:12px}
 body.stale #g{opacity:.35}
</style>
</head>
<body>
<h1>AMG8833 Thermal Grid</h1>
<div id="g"></div>
<div id="stats">waiting for first frame&hellip;</div>
<div id="scale"></div>
<div id="err"></div>
<script>
const g=document.getElementById('g'),stats=document.getElementById('stats'),
      scale=document.getElementById('scale'),err=document.getElementById('err'),cells=[];
for(let i=0;i<64;i++){const d=document.createElement('div');d.className='c';g.appendChild(d);cells.push(d);}
const color=v=>`hsl(${240-240*v},95%,${28+22*v}%)`;
async function tick(){
  try{
    const r=await fetch('/grid.json',{cache:'no-store'});
    if(!r.ok)throw new Error('HTTP '+r.status);
    const d=await r.json();
    let lo=d.min,hi=d.max;
    if(hi-lo<2){const m=(hi+lo)/2;lo=m-1;hi=m+1;}
    for(let i=0;i<64;i++){
      const t=d.t[i],v=Math.min(1,Math.max(0,(t-lo)/(hi-lo))),c=cells[i];
      c.style.background=color(v);
      c.textContent=t.toFixed(1);
      c.classList.toggle('hot',i===d.maxIdx);
      c.classList.toggle('cold',i===d.minIdx);
    }
    stats.textContent=`min ${d.min.toFixed(2)}  max ${d.max.toFixed(2)}  `+
                      `avg ${d.avg.toFixed(2)}  chip ${d.device.toFixed(2)} °C`;
    scale.textContent=`color scale ${lo.toFixed(1)} – ${hi.toFixed(1)} °C (auto)`;
    err.textContent='';
    document.body.classList.remove('stale');
  }catch(e){
    err.textContent='disconnected: '+e.message;
    document.body.classList.add('stale');
  }
}
setInterval(tick,1000);tick();
</script>
</body>
</html>
)GRID";

void AMG8833WebHandler::dump_config() {
  ESP_LOGCONFIG(TAG, "AMG8833 Web:");
  ESP_LOGCONFIG(TAG, "  Page: /grid");
  ESP_LOGCONFIG(TAG, "  Data: /grid.json");
}

void AMG8833WebHandler::handleRequest(AsyncWebServerRequest *req) {
#ifdef USE_ESP32
  char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
  bool is_json = req->url_to(url_buf) == "/grid.json";
#else
  bool is_json = req->url() == "/grid.json";
#endif
  if (is_json) {
    this->handle_json_(req);
    return;
  }
  req->send(200, "text/html", GRID_PAGE);
}

void AMG8833WebHandler::handle_json_(AsyncWebServerRequest *req) {
  GridFrame frame;
  if (!this->parent_->get_frame(frame)) {
    req->send(503, "application/json", "{\"error\":\"no frame available\"}");
    return;
  }

  AsyncResponseStream *stream = req->beginResponseStream("application/json");
  stream->print("{\"t\":[");
  for (uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++) {
    if (i != 0)
      stream->print(",");
    if (std::isfinite(frame.pixels[i])) {
      stream->printf("%.2f", frame.pixels[i]);
    } else {
      stream->print("null");
    }
  }
  stream->printf("],\"min\":%.2f,\"max\":%.2f,\"avg\":%.2f,\"device\":%.2f,\"minIdx\":%u,\"maxIdx\":%u}",
                 frame.min_temp, frame.max_temp, frame.avg_temp, frame.device_temp,
                 static_cast<unsigned>(frame.min_index), static_cast<unsigned>(frame.max_index));
  req->send(stream);
}

}  // namespace esphome::amg8833

#endif  // USE_NETWORK && !USE_ZEPHYR
