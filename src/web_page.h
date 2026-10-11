/**
 * The control page served by the board, stored in flash.
 */
#pragma once

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>LED Control</title>
  <style>
    body { font-family: system-ui, sans-serif; max-width: 420px; margin: 0 auto;
           padding: 24px 16px; background: #111; color: #eee; text-align: center; }
    h1 { font-size: 1.4rem; }
    .bulb { width: 90px; height: 90px; border-radius: 50%; margin: 20px auto;
            background: #ffd54a; opacity: 0.1; box-shadow: 0 0 40px #ffd54a; }
    .buttons { display: flex; gap: 10px; }
    button { flex: 1; padding: 16px 0; font-size: 1.1rem; border: 0;
             border-radius: 10px; background: #2d6cdf; color: #fff; }
    button:active { background: #1d4fa8; }
    input[type=range] { width: 100%; margin: 28px 0 8px; }
    #status { color: #999; }
  </style>
</head>
<body>
  <h1>LED Control</h1>
  <div class="bulb" id="bulb"></div>
  <div class="buttons">
    <button onclick="send('on')">On</button>
    <button onclick="send('off')">Off</button>
    <button onclick="send('fade')">Fade</button>
  </div>
  <input type="range" id="slider" min="0" max="255" value="0">
  <div id="status">connecting...</div>
<script>
  const slider = document.getElementById('slider');
  const bulb = document.getElementById('bulb');
  const status = document.getElementById('status');
  let dragging = false;

  function show(s) {
    bulb.style.opacity = 0.1 + 0.9 * s.brightness / 255;
    status.textContent = (s.fading ? 'Fading' : 'Manual') + ' - brightness ' + s.brightness;
    if (!dragging) slider.value = s.brightness;
  }
  function call(url) {
    return fetch(url).then(r => r.json()).then(show).catch(() => status.textContent = 'disconnected');
  }
  function send(cmd) { call('/' + cmd); }

  slider.addEventListener('input', () => { dragging = true; call('/brightness?value=' + slider.value); });
  slider.addEventListener('change', () => { dragging = false; });

  setInterval(() => { if (!dragging) call('/status'); }, 500);
  call('/status');
</script>
</body>
</html>
)HTML";
