(() => {
  'use strict';
  const $ = id => document.getElementById(id);
  const screen = $('screen'), lyrics = $('lyrics'), settings = $('settings');
  const key = 'voclyrics.remote.v1';
  const defaults = {style:'scrolling', proportional:false, clock:false, nudge:0, transpose:0};
  let prefs = {...defaults};
  try { prefs = {...defaults, ...JSON.parse(localStorage.getItem(key) || '{}')}; } catch (_) {}
  prefs.transpose = Number.isInteger(prefs.transpose) ? Math.max(-12, Math.min(12, prefs.transpose)) : 0;
  let socket, state = null, rows = [], renderedLyrics = null, hostOffset = 0, retry = 0, wakeLock = null, hideTimer, lastActive = -2, clientId = 0, pointerY = null;
  const save = () => localStorage.setItem(key, JSON.stringify(prefs));
  const showControls = () => { screen.classList.add('controls-visible'); clearTimeout(hideTimer); hideTimer = setTimeout(() => { if (settings.classList.contains('hidden')) screen.classList.remove('controls-visible'); }, 3500); };
  const parse = source => source.split(/\r?\n/).map(line => {
    const m = line.match(/^\[(\d+):(\d{1,2})(?:[.,](\d{1,3}))?\](.*)$/);
    if (m) return {time:Number(m[1])*60+Number(m[2])+Number(('0.'+(m[3]||'0')).slice(0,5)), text:m[4], section:false};
    const section = line.match(/^\[([^\]]+)\]$/);
    return line.trim() ? {time:-1,text:section?section[1]:line,section:!!section} : null;
  }).filter(Boolean);
  const applyPreferences = () => {
    lyrics.className = prefs.style;
    lyrics.classList.toggle('proportional', prefs.proportional);
    $('style').value = prefs.style; $('proportional').checked = prefs.proportional;
    $('show-clock').checked = prefs.clock; $('clock').style.display = prefs.clock ? '' : 'none';
    $('nudge-value').textContent = `${prefs.nudge} ms`;
    $('transpose').value = String(prefs.transpose);
    $('transpose-status').textContent = `TRANSPOSED BY: ${prefs.transpose}`;
    lastActive = -2;
  };
  const position = () => {
    if (!state) return 0;
    const now = performance.now() + hostOffset + prefs.nudge;
    const elapsed = state.playing ? Math.max(0, now - state.hostMs) / 1000 : 0;
    return state.position + elapsed;
  };
  const renderRows = active => {
    lyrics.replaceChildren();
    if (active < 0 || active >= rows.length) return;
    const count = prefs.style === 'single' ? 1 : prefs.style === 'teleprompter' ? 9 : 5;
    const start = Math.max(0, active - Math.floor(count / 2));
    const end = Math.min(rows.length, start + count);
    for (let i = start; i < end; ++i) {
      const row = rows[i];
      if (row.section) continue;
      const el = document.createElement('div');
      el.className = `line ${i < active ? 'past' : i === active ? 'active' : 'future'}`;
      el.textContent = row.text;
      el.dataset.index = i;
      lyrics.appendChild(el);
    }
  };
  const draw = () => {
    if (state) {
      const pos = position();
      let active = -1;
      let bestTime = -1;
      rows.forEach((r,i) => { if (!r.section && r.time >= 0 && r.time <= pos && r.time >= bestTime) { active = i; bestTime = r.time; } });
      if (Number.isInteger(state.scrollLine) && state.scrollLine >= 0)
        active = Math.min(state.scrollLine, rows.length - 1);
      if (active < 0) active = rows.findIndex(r => !r.section);
      $('song').textContent = state.title || 'VocLyrics';
      $('next-song').textContent = state.next ? `Next: ${state.next}` : '';
      if (active !== lastActive) {
        renderRows(active);
        lastActive = active;
      }
      if (prefs.proportional) {
        const activeRow = rows[active];
        const length = Math.max(10, activeRow ? activeRow.text.length : 10);
        lyrics.style.fontSize = `clamp(1.5rem, ${Math.max(3.5, Math.min(9, 75 / length))}vw, 6.5rem)`;
      } else lyrics.style.fontSize = '';
    }
    if (prefs.clock) $('clock').textContent = new Date().toLocaleTimeString([], {hour:'2-digit',minute:'2-digit'});
    setTimeout(draw, 100);
  };
  const setConnection = (ok, message) => { $('connection').classList.toggle('connected', ok); $('connection span').textContent = message; };
  const connect = () => {
    const scheme = location.protocol === 'https:' ? 'wss:' : 'ws:';
    socket = new WebSocket(`${scheme}//${location.host}/ws`);
    socket.onopen = () => { retry = 0; setConnection(true,'Connected'); socket.send(JSON.stringify({type:'hello',name:`${navigator.platform || 'Remote'} display`})); ping(); };
    socket.onmessage = event => {
      let data; try { data = JSON.parse(event.data); } catch (_) { return; }
      if (data.type === 'hello') { clientId = Number(data.clientId) || 0; }
      else if (data.type === 'state') { state = data; if (renderedLyrics !== data.lyrics) { renderedLyrics=data.lyrics; rows=parse(data.lyrics || ''); renderRows(); } }
      else if (data.type === 'pong') { const sent = Number(data.clientSent); const received = performance.now(); const measured = Number(data.hostMs) - (sent + received) / 2; hostOffset += (measured - hostOffset) * .2; }
      else if (data.type === 'nudge') { prefs.nudge += Number(data.delta) || 0; save(); applyPreferences(); }
      else if (data.type === 'reset') { prefs.nudge = 0; save(); applyPreferences(); }
    };
    socket.onclose = async () => {
      setConnection(false,'Reconnecting');
      try { const response = await fetch('/capacity',{cache:'no-store'}); if ((await response.text()) === 'full') { showFull(); return; } } catch (_) {}
      setTimeout(connect, Math.min(6000, 500 * (2 ** Math.min(retry++,4))));
    };
    socket.onerror = () => setConnection(false,'Connection interrupted');
  };
  const ping = () => { if (socket && socket.readyState === WebSocket.OPEN) { socket.send(JSON.stringify({type:'ping',clientSent:performance.now()})); setTimeout(ping,1500); } };
  const controlScroll = delta => {
    if (!state || state.controllerId !== clientId || clientId <= 0) return;
    let current = Number.isInteger(state.scrollLine) && state.scrollLine >= 0 ? state.scrollLine : rows.findIndex(row => !row.section);
    const last = Math.max(0, rows.length - 1);
    current = Math.max(0, Math.min(last, current + delta));
    state.scrollLine = current;
    if (socket && socket.readyState === WebSocket.OPEN) socket.send(JSON.stringify({type:'scroll',line:current}));
    lastActive = -2;
  };
  const showFull = () => { const full = document.createElement('div'); full.id='full'; full.textContent='VocLyrics is already serving 8 devices. Ask the host to disconnect one device, then refresh this page.'; document.body.appendChild(full); };
  const requestWakeLock = async () => {
    try {
      if ('wakeLock' in navigator) wakeLock = await navigator.wakeLock.request('screen');
    } catch (_) {}
  };
  $('gear').addEventListener('click', () => { settings.classList.toggle('hidden'); showControls(); });
  screen.addEventListener('pointerdown', event => { pointerY=event.clientY; showControls(); });
  screen.addEventListener('pointerup', event => {
    if (pointerY === null) return;
    const distance=pointerY-event.clientY;
    if (Math.abs(distance) > 22) controlScroll(distance > 0 ? 1 : -1);
    pointerY=null;
  });
  screen.addEventListener('wheel', event => { if (Math.abs(event.deltaY) > 0) controlScroll(event.deltaY > 0 ? 1 : -1); }, {passive:true});
  $('transpose').innerHTML = Array.from({length:25}, (_,i) => i - 12)
    .map(value => `<option value="${value}">${value}</option>`).join('');
  $('transpose').addEventListener('change', e => {
    const value = Number.parseInt(e.target.value, 10);
    prefs.transpose = Number.isInteger(value) ? Math.max(-12, Math.min(12, value)) : 0;
    save(); applyPreferences();
  });
  $('style').addEventListener('change', e => { prefs.style=e.target.value; save(); applyPreferences(); });
  $('proportional').addEventListener('change', e => { prefs.proportional=e.target.checked; save(); applyPreferences(); });
  $('show-clock').addEventListener('change', e => { prefs.clock=e.target.checked; save(); applyPreferences(); });
  document.querySelectorAll('[data-nudge]').forEach(button => button.addEventListener('click', () => {
    prefs.nudge = button.dataset.nudge === 'reset' ? 0 : prefs.nudge + Number(button.dataset.nudge); save(); applyPreferences(); showControls();
  }));
  $('keep-awake').addEventListener('click', requestWakeLock);
  document.addEventListener('visibilitychange', () => { if (document.visibilityState === 'visible') requestWakeLock(); });
  applyPreferences(); connect(); requestWakeLock(); draw();
})();

