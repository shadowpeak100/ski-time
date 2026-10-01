// Phone-side JS companion for Ski Time watch face.
// Fetches the current outdoor temperature (°F) from Open-Meteo and sends it
// to the watch via AppMessage on app launch, then every 30 minutes.

var config = require('./config');

var REFRESH_MS = 30 * 60 * 1000;   // 30 minutes
var REQUEST_TIMEOUT_MS = 15000;

function logErr(prefix, e) {
  console.log('[ski-time] ' + prefix + ': ' + (e && e.message ? e.message : e));
}

function sendWeather(msg) {
  Pebble.sendAppMessage(
    msg,
    function() { console.log('[ski-time] sent ' + JSON.stringify(msg)); },
    function(e) { logErr('sendAppMessage failed', e); }
  );
}

// "YYYY-MM-DDTHH:MM" -> minutes since midnight
function minutesOfDay(s) {
  return +s.substr(11, 2) * 60 + +s.substr(14, 2);
}

// -1 blowing west, +1 blowing east, 0 calm (< 5 km/h); null when unknown.
// Open-Meteo gives the direction the wind comes FROM, so the wind blows toward dir + 180.
function windX(speed, dir) {
  if (typeof speed !== 'number' || !isFinite(speed)) return null;
  if (speed < 5) return 0;
  if (typeof dir !== 'number' || !isFinite(dir)) return null;
  var toward = ((dir + 180) % 360 + 360) % 360;
  return toward >= 180 ? -1 : 1;
}

function fetchWeather() {
  navigator.geolocation.getCurrentPosition(
    function(pos) {
      var lat = pos.coords.latitude.toFixed(4);
      var lon = pos.coords.longitude.toFixed(4);
      var url = 'https://api.open-meteo.com/v1/forecast' +
                '?latitude=' + lat +
                '&longitude=' + lon +
                '&current=temperature_2m,weather_code,wind_speed_10m,wind_direction_10m' +
                '&temperature_unit=fahrenheit' +
                '&daily=sunrise,sunset&timezone=auto&forecast_days=1';
      var req = new XMLHttpRequest();
      req.open('GET', url, true);
      req.timeout = REQUEST_TIMEOUT_MS;
      req.onload = function() {
        if (req.status >= 200 && req.status < 300) {
          try {
            var data = JSON.parse(req.responseText);
            var t = Math.round(data.current.temperature_2m);
            var msg = { TEMP_F: t };
            var code = data.current.weather_code;
            if (typeof code === 'number' && isFinite(code) && code % 1 === 0 && code >= 0 && code <= 99) msg.WX_CODE = code;
            var wind = windX(data.current.wind_speed_10m, data.current.wind_direction_10m);
            if (wind !== null) msg.WIND_X = wind;
            var rise = data.daily && data.daily.sunrise && data.daily.sunrise[0];
            var set = data.daily && data.daily.sunset && data.daily.sunset[0];
            if (rise && set) {
              var sr = minutesOfDay(rise), ss = minutesOfDay(set);
              if (isFinite(sr) && isFinite(ss) && sr >= 0 && sr < 1440 && ss >= 0 && ss < 1440) {
                msg.SUNRISE = sr;
                msg.SUNSET = ss;
              }
            }
            sendWeather(msg);
          } catch (e) { logErr('parse', e); }
        } else {
          logErr('http', req.status);
        }
      };
      req.onerror = function() { logErr('network', null); };
      req.ontimeout = function() { logErr('timeout', null); };
      req.send();
    },
    function(err) { logErr('geo', err); },
    { timeout: REQUEST_TIMEOUT_MS, maximumAge: 10 * 60 * 1000 }
  );
}

Pebble.addEventListener('ready', function() {
  console.log('[ski-time] pkjs ready');
  fetchWeather();
  setInterval(fetchWeather, REFRESH_MS);
});

// ---- Settings page ----

function loadCfg() {
  var cfg = {}, k;
  for (k in config.DEFAULTS) cfg[k] = config.DEFAULTS[k];
  try {
    var saved = JSON.parse(localStorage.getItem('cfg') || '{}');
    for (k in saved) if (saved[k] !== undefined) cfg[k] = saved[k];
  } catch (e) { logErr('loadCfg', e); }
  if (cfg.dayMode === 2) { cfg.dayCycle = false; cfg.dayMode = 0; cfg.dayFixed = 1; }   // the old "Always midday"
  cfg.dayMode = +cfg.dayMode; cfg.dayFixed = +cfg.dayFixed; cfg.wxFixed = +cfg.wxFixed;
  return cfg;
}

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL('data:text/html;charset=utf-8,' +
                 encodeURIComponent(config.buildHtml(loadCfg())));
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) return;                       // dismissed without saving
  var cfg;
  try { cfg = JSON.parse(decodeURIComponent(e.response)); }
  catch (err) { logErr('parse config', err); return; }
  localStorage.setItem('cfg', JSON.stringify(cfg));
  Pebble.sendAppMessage(config.toMessage(cfg),
    function() { console.log('[ski-time] settings sent'); },
    function(err) { logErr('settings send', err); });
});
