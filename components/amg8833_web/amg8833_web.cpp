#include "amg8833_web.h"
#if defined(USE_NETWORK) && !defined(USE_ZEPHYR)

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::amg8833 {

static const char *const TAG = "amg8833_web";

// Self-contained: no CDN, no external assets, so the page works on an isolated
// network. Auto-scales the color ramp to the current frame's min/max.
//
// One page serves both routes: "/" is the dashboard (heatmap + every entity,
// streamed from web_server's /events) and "/grid" is the heatmap alone. The
// script branches on location.pathname so only one copy lives in flash.
//
// The °C/°F switch is display-only and remembered in localStorage. Every wire
// format stays Celsius: /grid.json, /metrics, the API and MQTT are untouched.
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
 header{display:flex;align-items:center;gap:1rem;flex-wrap:wrap;justify-content:center}
 h1{font-size:1rem;font-weight:600;letter-spacing:.08em;text-transform:uppercase;
    color:#8b93a7;margin:0}
 #unit{display:flex;border:1px solid #2b3040;border-radius:6px;overflow:hidden}
 #unit button{background:#1a1d26;color:#8b93a7;border:0;padding:.3rem .7rem;
    font:inherit;font-size:12px;cursor:pointer}
 #unit button.on{background:#2f6feb;color:#fff}
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
 table{border-collapse:collapse;width:min(92vw,520px);font-size:13px}
 td{padding:.3rem .5rem;border-top:1px solid #23273200}
 tr:nth-child(odd){background:#171a22}
 td.v{text-align:right;font-variant-numeric:tabular-nums;color:#c3c9d8;
      max-width:24ch;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
 a{color:#6c7488;font-size:12px}
</style>
</head>
<body>
<header>
 <h1>AMG8833 Thermal Grid</h1>
 <div id="unit"><button data-u="C">°C</button><button data-u="F">°F</button></div>
</header>
<div id="g"></div>
<div id="stats">waiting for first frame&hellip;</div>
<div id="scale"></div>
<div id="err"></div>
<table id="ents"></table>
<a id="alt" href="/grid">heatmap only &rarr;</a>
<script>
const isGrid=location.pathname==='/grid';
const g=document.getElementById('g'),stats=document.getElementById('stats'),
      scale=document.getElementById('scale'),err=document.getElementById('err'),
      ents=document.getElementById('ents'),alt=document.getElementById('alt'),cells=[];
for(let i=0;i<64;i++){const d=document.createElement('div');d.className='c';g.appendChild(d);cells.push(d);}
if(isGrid){ents.remove();alt.href='/';alt.textContent='← all readings';}

// Display-only unit conversion. Everything on the wire stays Celsius.
let unit=localStorage.getItem('gridUnit')==='F'?'F':'C';
const cv=t=>unit==='F'?t*9/5+32:t, U=()=>unit==='F'?'°F':'°C';
document.querySelectorAll('#unit button').forEach(b=>{
  b.onclick=()=>{unit=b.dataset.u;localStorage.setItem('gridUnit',unit);paint();};
});
function marks(){document.querySelectorAll('#unit button').forEach(b=>
  b.classList.toggle('on',b.dataset.u===unit));}

let frame=null;
const rows=new Map();

function paint(){
  marks();
  if(frame){
    let lo=frame.min,hi=frame.max;
    if(hi-lo<2){const m=(hi+lo)/2;lo=m-1;hi=m+1;}
    for(let i=0;i<64;i++){
      const t=frame.t[i],v=Math.min(1,Math.max(0,(t-lo)/(hi-lo))),c=cells[i];
      c.style.background=`hsl(${240-240*v},95%,${28+22*v}%)`;
      c.textContent=cv(t).toFixed(1);
      c.classList.toggle('hot',i===frame.maxIdx);
      c.classList.toggle('cold',i===frame.minIdx);
    }
    stats.textContent=`min ${cv(frame.min).toFixed(2)}  max ${cv(frame.max).toFixed(2)}  `+
                      `avg ${cv(frame.avg).toFixed(2)}  chip ${cv(frame.device).toFixed(2)} ${U()}`;
    scale.textContent=`color scale ${cv(lo).toFixed(1)} – ${cv(hi).toFixed(1)} ${U()} (auto)`;
  }
  if(!isGrid)paintRows();
}

function paintRows(){
  const esc=s=>String(s).replace(/[<>&"]/g,c=>({'<':'&lt;','>':'&gt;','&':'&amp;','"':'&quot;'}[c]));
  ents.innerHTML=[...rows.values()].sort((a,b)=>a.name.localeCompare(b.name)).map(e=>{
    // Convert only entities the device itself reports in Celsius.
    const f=(unit==='F'&&typeof e.state==='string'&&e.state.includes('°C')&&isFinite(e.value))
      ?(e.value*9/5+32).toFixed(2)+' °F':e.state;
    return `<tr><td>${esc(e.name)}</td><td class="v">${esc(f)}</td></tr>`;
  }).join('');
}

async function tick(){
  try{
    const r=await fetch('/grid.json',{cache:'no-store'});
    if(!r.ok)throw new Error('HTTP '+r.status);
    frame=await r.json();
    err.textContent='';
    document.body.classList.remove('stale');
  }catch(e){
    err.textContent='disconnected: '+e.message;
    document.body.classList.add('stale');
  }
  paint();
}
setInterval(tick,1000);tick();

if(!isGrid){
  // web_server still owns /events even though it no longer serves "/".
  let pending=null;
  const es=new EventSource('/events');
  es.addEventListener('state',ev=>{
    try{
      const d=JSON.parse(ev.data);
      if(!d.id)return;
      rows.set(d.id,{name:d.name||d.id,state:d.state,value:d.value});
      if(!pending)pending=setTimeout(()=>{pending=null;paintRows();},50);
    }catch(e){}
  });
}
</script>
</body>
</html>
)GRID";

void AMG8833WebHandler::dump_config() {
  ESP_LOGCONFIG(TAG, "AMG8833 Web:");
  ESP_LOGCONFIG(TAG, "  Dashboard: / (shadows the web_server index page)");
  ESP_LOGCONFIG(TAG, "  Heatmap:   /grid");
  ESP_LOGCONFIG(TAG, "  Data:      /grid.json");
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
