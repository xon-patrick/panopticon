'use strict';
const $ = id => document.getElementById(id);
const client = crypto.randomUUID();
let state = null, selected = '', token = '', seq = 0, drive = null, sending = false;
let lastPoll = 0, yawZero = 0, samples = [], lastImuTime = null;
const text = (id, value) => { $(id).textContent = value; };
const fmt = (v, digits = 1) => Number.isFinite(v) ? v.toFixed(digits) : '—';
const fresh = (topic, limit = 1) => topic && topic.age < limit;
const current = () => state?.robots[selected];
function calibration() {
  try { const saved = JSON.parse(localStorage.getItem('calibration:' + selected) || '{}'); $('diameter').value = saved.diameter || '0.100'; $('resolution').value = saved.resolution || ''; } catch { $('diameter').value = '0.100'; $('resolution').value = ''; }
}
for (const id of ['diameter', 'resolution']) $(id).addEventListener('input', () => {
  try { localStorage.setItem('calibration:' + selected, JSON.stringify({diameter: $('diameter').value, resolution: $('resolution').value})); } catch { /* Storage may be disabled. */ }
});
function canDrive() { return selected && current()?.online && $('armed').checked && !state?.preview && performance.now() - lastPoll < 700; }
async function command(robot, linear, angular) {
  if (!token || !robot || state?.preview) return;
  const response = await fetch('/api/command', {method: 'POST', headers: {'Content-Type': 'application/json', 'X-Control-Token': token}, body: JSON.stringify({robot, client, seq: ++seq, linear, angular}), signal: AbortSignal.timeout(600)});
  const result = await response.json();
  if (!response.ok) throw new Error(result.error || 'Comanda a fost refuzată');
}
function stop(disarm = false) {
  drive = null;
  document.querySelectorAll('[data-drive]').forEach(b => b.classList.remove('active'));
  if (disarm) $('armed').checked = false;
  command(selected, 0, 0).catch(() => {});
  text('controlState', $('armed').checked ? 'PREGĂTIT' : 'DEZARMAT');
}
async function sendDrive() {
  if (!drive || sending) return;
  if (!canDrive()) { stop(true); return; }
  const v = Number($('linear').value), w = Number($('angular').value);
  const values = {forward: [v, 0], back: [-v, 0], left: [0, w], right: [0, -w]}[drive];
  sending = true;
  try { await command(selected, ...values); }
  catch (error) { text('commandError', error.message); stop(true); }
  finally { sending = false; }
}
function start(direction) {
  if (!canDrive()) return;
  drive = direction;
  text('commandError', ''); text('controlState', 'CONTROL ACTIV');
  document.querySelectorAll('[data-drive]').forEach(b => b.classList.toggle('active', b.dataset.drive === drive));
  sendDrive();
}
setInterval(sendDrive, 100);
for (const button of document.querySelectorAll('[data-drive]')) {
  button.addEventListener('pointerdown', event => { if (event.button !== 0) return; event.preventDefault(); button.setPointerCapture(event.pointerId); start(button.dataset.drive); });
  for (const event of ['pointerup', 'pointercancel', 'lostpointercapture']) button.addEventListener(event, () => { if (drive) stop(); });
  button.addEventListener('contextmenu', event => event.preventDefault());
}
const keys = {w:'forward', ArrowUp:'forward', s:'back', ArrowDown:'back', a:'left', ArrowLeft:'left', d:'right', ArrowRight:'right'};
window.addEventListener('keydown', event => {
  if (event.code === 'Space' || event.key === 'Escape') { event.preventDefault(); stop(true); return; }
  if (['INPUT', 'SELECT', 'TEXTAREA'].includes(event.target.tagName)) return;
  if (keys[event.key]) { event.preventDefault(); if (!event.repeat) start(keys[event.key]); }
});
window.addEventListener('keyup', event => { if (keys[event.key] && drive) stop(); });
window.addEventListener('blur', () => stop(true));
document.addEventListener('visibilitychange', () => { if (document.hidden) stop(true); });
window.addEventListener('pagehide', () => stop(true));
$('stop').onclick = () => stop(true);
$('armed').onchange = () => { if (!$('armed').checked) stop(true); else text('controlState', 'PREGĂTIT'); };
$('robot').onchange = () => { stop(true); selected = $('robot').value; samples = []; lastImuTime = null; yawZero = current()?.yaw || 0; calibration(); render(); };
$('zero').onclick = () => { yawZero = current()?.yaw || 0; render(); };
for (const id of ['linear', 'angular']) $(id).oninput = () => text(id + 'Label', Number($(id).value).toFixed(2) + (id === 'linear' ? ' m/s' : ' rad/s'));
function plot(id, field) {
  const canvas = $(id), ctx = canvas.getContext('2d'), width = canvas.width, height = canvas.height;
  ctx.clearRect(0, 0, width, height);
  ctx.strokeStyle = '#304137'; ctx.lineWidth = 1;
  for (let y = 15; y < height; y += 25) { ctx.beginPath(); ctx.moveTo(0,y); ctx.lineTo(width,y); ctx.stroke(); }
  const scale = Math.max(.1, ...samples.flatMap(s => s ? Object.values(s[field]).map(Math.abs) : []));
  ['x', 'y', 'z'].forEach((axis, i) => {
    ctx.strokeStyle = ['#ffbe55','#9ee6c2','#e1adfc'][i]; ctx.lineWidth = 2; ctx.beginPath();
    let gap = true;
    samples.forEach((s, n) => { if (!s || !Number.isFinite(s[field][axis])) { gap = true; return; } const x = n / 99 * width, y = height / 2 - s[field][axis] / scale * (height / 2 - 15); if (gap) ctx.moveTo(x,y); else ctx.lineTo(x,y); gap = false; }); ctx.stroke();
  });
  ctx.fillStyle = '#95a8a0'; ctx.font = '12px monospace'; ctx.fillText('±' + fmt(scale,2), 5,12);
}
function render() {
  const robot = current(), topics = robot?.topics || {}, imu = fresh(topics['imu/data_raw']) ? topics['imu/data_raw'].value : null;
  const enc = fresh(topics.encoder_ticks) ? topics.encoder_ticks : null, rates = enc?.rate;
  const online = !!robot?.online;
  text('online', online ? '● Conectat' : '○ Offline');
  text('lastSeen', topics.encoder_ticks ? `Ultimul mesaj: ${fmt(topics.encoder_ticks.age)} s` : 'Fără telemetrie');
  $('armed').disabled = !online || state?.preview;
  document.querySelectorAll('[data-drive]').forEach(b => b.disabled = !online || !$('armed').checked || state?.preview);
  if (!online && $('armed').checked) stop(true);
  text('turn', imu ? `${fmt(imu.angular_velocity.z * 180 / Math.PI)} °/s` : '— °/s');
  text('battery', fresh(topics.battery_voltage, 3) ? `${fmt(topics.battery_voltage.value, 2)} V` : '— V');
  text('batteryHint', !topics.battery_voltage ? 'Senzor oprit sau fără date' : fresh(topics.battery_voltage, 3) ? 'Tensiune măsurată' : 'Date expirate');
  const diameter = Number($('diameter').value), resolution = Number($('resolution').value), calibrated = diameter > 0 && resolution >= 1;
  text('speed', rates && calibrated ? `${fmt((rates[0] + rates[1]) / 2 * Math.PI * diameter / resolution, 3)} m/s` : '— m/s');
  text('speedHint', calibrated ? 'Estimare din roți · poate exista alunecare' : 'Necesită calibrarea encoderelor');
  text('leftRate', rates ? `${fmt(rates[0],0)} ticks/s` : '— ticks/s'); text('rightRate', rates ? `${fmt(rates[1],0)} ticks/s` : '— ticks/s');
  // SVG/DOM attributes avoid HTML injection from ROS message text.
  const maxRate = Math.max(100, ...(rates || []).map(Math.abs));
  $('leftBar').style.width = rates ? `${Math.abs(rates[0]) / maxRate * 100}%` : '0%';
  $('rightBar').style.width = rates ? `${Math.abs(rates[1]) / maxRate * 100}%` : '0%';
  text('ticks', enc ? enc.value.join(' / ') : '— / —');
  let direction = 'Fără date';
  if (rates) { const [l,r] = rates; direction = Math.abs(l) + Math.abs(r) < 2 ? 'Staționar' : l * r < 0 ? (r > l ? 'Rotire stânga' : 'Rotire dreapta') : (l + r > 0 ? 'Înainte' : 'Înapoi') + (Math.abs(r-l) > Math.max(5, Math.abs(r+l)*.15) ? (r>l ? ' · curbă stânga' : ' · curbă dreapta') : ''); }
  text('direction', direction);
  const yaw = robot ? (robot.yaw - yawZero) * 180 / Math.PI : 0;
  $('robotShape').setAttribute('transform', `rotate(${-yaw} 150 150)`);
  text('heading', imu ? `${fmt(((yaw % 360) + 360) % 360,0)}°` : '—°');
  const a = imu?.linear_acceleration;
  text('roll', a ? `${fmt(Math.atan2(a.y,a.z)*180/Math.PI)}°` : '—°');
  text('pitch', a ? `${fmt(Math.atan2(-a.x,Math.hypot(a.y,a.z))*180/Math.PI)}°` : '—°');
  text('accelValues', a ? ['x','y','z'].map(k => `${k.toUpperCase()} ${fmt(a[k],2)}`).join('   ') : 'Fără date IMU recente');
  text('gyroValues', imu ? ['x','y','z'].map(k => `${k.toUpperCase()} ${fmt(imu.angular_velocity[k],2)}`).join('   ') : 'Fără date IMU recente');
  const list = $('topicList'), names = ['encoder_ticks','imu/data_raw','battery_voltage','status'];
  if (!list.querySelector('details')) { list.replaceChildren(); for (const name of names) { const detail = document.createElement('details'); detail.dataset.topic = name; const summary = document.createElement('summary'); const label = document.createElement('b'); label.textContent = name; summary.append(label,document.createElement('span')); detail.append(summary,document.createElement('pre')); list.append(detail); } }
  let count = 0;
  for (const detail of list.children) { const t = topics[detail.dataset.topic], valid = fresh(t, detail.dataset.topic === 'status' ? 5 : detail.dataset.topic === 'battery_voltage' ? 3 : 1); if(valid) count++; const badge = detail.querySelector('span'); badge.className = valid ? '' : 'stale'; badge.textContent = t ? (valid ? `${fmt(t.hz)} Hz` : `vechi · ${fmt(t.age)} s`) : 'fără date'; detail.querySelector('pre').textContent = t ? JSON.stringify(t.value,null,2) : 'Niciun mesaj primit.'; }
  text('topicCount', `${count} ACTIVE`);
  text('catalog', state?.catalog.map(t => `${t.name}\n  ${t.types.join(', ')}`).join('\n\n') || 'Niciun topic descoperit.');
  plot('accelPlot','linear_acceleration'); plot('gyroPlot','angular_velocity');
}
async function poll() {
  try {
    const response = await fetch('/api/state', {signal: AbortSignal.timeout(1500)});
    if (!response.ok) throw new Error('Bridge indisponibil');
    const next = await response.json();
    if (token && token !== next.token) stop(true);
    state = next; token = state.token; lastPoll = performance.now();
    text('bridge', state.preview ? '◌ Preview · fără ROS' : '● Bridge ROS conectat');
    const names = Object.keys(state.robots).sort();
    if (Array.from($('robot').options).map(o => o.value).filter(Boolean).join('|') !== names.join('|')) {
      $('robot').replaceChildren();
      if (!names.length) $('robot').add(new Option('Aștept robotul…', ''));
      for(const name of names) $('robot').add(new Option(name.replace(/^\//,''),name));
      if (!names.includes(selected)) { stop(true); selected = names[0] || ''; samples = []; lastImuTime = null; yawZero = 0; calibration(); }
      $('robot').value = selected;
    }
    $('notice').hidden = !!current()?.online;
    text('notice', state.preview ? 'Mod preview: interfața este disponibilă, fără date simulate și fără controlul motoarelor.' : names.length ? 'Robotul selectat nu mai publică encodere. Controlul este dezactivat până revine telemetria.' : 'Pornește agentul micro-ROS și robotul. Roboții apar automat când publică telemetrie.');
    const t = current()?.topics['imu/data_raw'];
    samples.push(fresh(t) && t.time !== lastImuTime ? t.value : null); if(samples.length > 100) samples.shift(); lastImuTime = t?.time;
    render();
  } catch {
    stop(true); state = null; samples = []; render();
    text('bridge','○ Bridge deconectat'); $('notice').hidden = false; text('notice','Conexiunea cu serverul s-a pierdut. Teleoperarea a fost dezactivată.');
  } finally { setTimeout(poll,200); }
}
$('armed').addEventListener('change', render);
poll();
