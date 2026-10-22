(() => {
  'use strict';
  const $ = id => document.getElementById(id);
  const screen = $('screen'), lyrics = $('lyrics'), upNext = $('up-next'), settings = $('settings');
  const roles = ['Main Singer','Backup Singer','Main Guitar','Rythm Guitar','Drummer','Percussionist','Bass','Keys'];
  const key = 'voclyrics.remote.v2';
  const defaults = {proportional:true, clock:false, nudge:0, transpose:0, role:''};
  let prefs = {...defaults};
  try { prefs = {...defaults, ...JSON.parse(localStorage.getItem(key) || '{}')}; } catch (_) {}
  prefs.transpose = Number.isInteger(prefs.transpose) ? Math.max(-12, Math.min(12, prefs.transpose)) : 0;
  if (!roles.includes(prefs.role)) prefs.role = '';
  let socket, state = null, rows = [], renderedLyrics = null, hostOffset = 0, retry = 0, wakeLock = null, clientId = 0, pointerY = null, lastActive = -2;
  const save = () => localStorage.setItem(key, JSON.stringify(prefs));
  const parse = source => source.split(/\r?\n/).map(line => {
    const m = line.match(/^\[(\d+):(\d{1,2})(?:[.,](\d{1,3}))?\](.*)$/);
    if (m) return {time:Number(m[1])*60+Number(m[2])+Number(('0.'+(m[3]||'0')).slice(0,5)), text:m[4], section:false};
    const section = line.match(/^\[([^\]]+)\]$/);
    return line.trim() ? {time:-1,text:section?section[1]:line,section:!!section} : null;
  }).filter(Boolean);
  const roleOptions = roles.map(role => `<option value="${role}">${role}</option>`).join('');
  $('role').innerHTML = roleOptions;
  $('role-first').innerHTML = `<option value="" disabled selected>Select your role</option>${roleOptions}`;
  $('transpose').innerHTML = Array.from({length:25}, (_,i) => i - 12).map(value => `<option value="${value}">${value}</option>`).join('');
  const identify = () => { if (socket && socket.readyState === WebSocket.OPEN && prefs.role) socket.send(JSON.stringify({type:'hello',name:prefs.role})); };
  const chooseRole = role => { if (!roles.includes(role)) return; prefs.role=role; save(); applyPreferences(); $('role-setup').classList.add('hidden'); identify(); };
  const applyPreferences = () => {
    $('proportional').checked=prefs.proportional; $('show-clock').checked=prefs.clock; $('clock').style.display=prefs.clock?'':'none';
    $('nudge-value').textContent=`${prefs.nudge} ms`; $('transpose').value=String(prefs.transpose); $('transpose-status').textContent=`TRANSPOSED BY: ${prefs.transpose}`;
    $('role').value=prefs.role; $('role-badge').textContent=(prefs.role||'Choose role').toUpperCase();
    screen.classList.toggle('fit-text',prefs.proportional);
  };
  const position = () => !state ? 0 : state.position + (state.playing ? Math.max(0,performance.now()+hostOffset+prefs.nudge-state.hostMs)/1000 : 0);
  const isChordOnly = text => {
    const tokens=text.trim().split(/\s+/).filter(Boolean); if(!tokens.length)return false;
    let chords=0;
    const valid=tokens.every(token=>{
      const cleaned=token.replace(/^[|:]+|[|:]+$/g,'');
      if(!cleaned)return true;
      const chord=/^[A-G](?:#|b)?(?:m|maj|min|sus|dim|aug|add)?\d*(?:[+\-()]|\/[A-G](?:#|b)?)*$/i.test(cleaned);
      if(chord)chords+=1;
      return chord;
    });
    return valid&&chords>0;
  };
  const isDivider = text => /^\s*[-_=—–]{3,}\s*$/.test(text);
  const lyricBlocks = () => {
    const result=[]; let section='';
    for(let index=0;index<rows.length;index+=1){
      if(rows[index].section){section=rows[index].text;continue;}
      if(isDivider(rows[index].text))continue;
      if(isChordOnly(rows[index].text)&&index+1<rows.length&&!rows[index+1].section){
        result.push({index,chord:rows[index].text,text:rows[index+1].text,section}); index+=1;
      }else result.push({index,chord:'',text:rows[index].text,section});
    }
    return result;
  };
  const transposeRoot = (root,accidental,amount) => {
    const natural={C:0,D:2,E:4,F:5,G:7,A:9,B:11};
    const names=['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
    let pitch=natural[root]+(accidental==='#'?1:accidental==='b'?-1:0)+amount;
    return names[((pitch%12)+12)%12];
  };
  const transposeChord = (token,amount) => {
    if(!amount)return token;
    let changed=token.replace(/^([|:]*)([A-G])([#b]?)/,(match,bars,root,accidental)=>`${bars}${transposeRoot(root,accidental,amount)}`);
    changed=changed.replace(/\/([A-G])([#b]?)/g,(match,root,accidental)=>`/${transposeRoot(root,accidental,amount)}`);
    return changed;
  };
  const transposeChordLine = line => line.split(/(\s+)/).map(part=>/^\s+$/.test(part)?part:transposeChord(part,prefs.transpose)).join('');
  const appendBlock = (parent,block,preview) => {
    const wrapper=document.createElement('div'); wrapper.className=preview?'lyric-block preview-block':'lyric-block';
    const chord=document.createElement('div'); chord.className='chord-row'; chord.textContent=block.chord?transposeChordLine(block.chord):'\u00a0';
    const text=document.createElement('div'); text.className=preview?'lyric-row preview-line':'lyric-row'; text.textContent=block.text;
    wrapper.append(chord,text); parent.appendChild(wrapper);
  };
  const fitSheet = () => {
    const card=document.querySelector('.now-card'); if(!card)return;
    const lines=[...lyrics.querySelectorAll('.chord-row,.lyric-row')]; if(!lines.length)return;
    const longest=Math.max(8,...lines.map(line=>line.textContent.length));
    const width=Math.max(180,card.clientWidth-48),height=Math.max(150,card.clientHeight-100);
    const size=Math.max(14,Math.min(54,Math.floor(width/(longest*.61)),Math.floor(height/6.2)));
    lyrics.style.fontSize=`${size}px`;
  };
  const renderRows = active => {
    lyrics.replaceChildren(); upNext.replaceChildren();
    const blocks=lyricBlocks(); if (!blocks.length) return;
    let start=blocks.findIndex(block=>block.index>=active); if(start<0) start=Math.max(0,blocks.length-1);
    if(start>0&&blocks[start].index>active)start-=1;
    $('section-label').textContent=(blocks[start].section||'SONG').toUpperCase();
    blocks.slice(start,start+2).forEach(block=>appendBlock(lyrics,block,false));
    blocks.slice(start+2,start+4).forEach(block=>appendBlock(upNext,block,true));
    requestAnimationFrame(fitSheet);
  };
  const draw = () => {
    if(state){
      let active=-1,best=-1; rows.forEach((r,i)=>{if(!r.section&&r.time>=0&&r.time<=position()&&r.time>=best){active=i;best=r.time;}});
      if(Number.isInteger(state.scrollLine)&&state.scrollLine>=0) active=Math.min(state.scrollLine,Math.max(0,rows.length-1));
      if(active<0) active=rows.findIndex(r=>!r.section);
      $('song').textContent=state.title||'VocLyrics'; $('next-song').textContent=state.next?`Next song: ${state.next}`:'';
      const canControl=state.controllerId===clientId&&clientId>0;
      $('scroll-up').disabled=!canControl; $('scroll-down').disabled=!canControl;
      if(active!==lastActive){renderRows(active);lastActive=active;}
    }
    if(prefs.clock)$('clock').textContent=new Date().toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'});
    setTimeout(draw,33);
  };
  const setConnection=(ok,message)=>{$('connection').classList.toggle('connected',ok);$('connection span').textContent=message;};
  const connect=()=>{
    socket=new WebSocket(`${location.protocol==='https:'?'wss:':'ws:'}//${location.host}/ws`);
    socket.onopen=()=>{retry=0;setConnection(true,'Connected');identify();ping();};
    socket.onmessage=event=>{let data;try{data=JSON.parse(event.data);}catch(_){return;}
      if(data.type==='hello')clientId=Number(data.clientId)||0;
      else if(data.type==='state'){state=data;if(renderedLyrics!==data.lyrics){renderedLyrics=data.lyrics;rows=parse(data.lyrics||'');lastActive=-2;}}
      else if(data.type==='pong'){const sent=Number(data.clientSent),received=performance.now(),measured=Number(data.hostMs)-(sent+received)/2;hostOffset+=(measured-hostOffset)*.2;}
      else if(data.type==='nudge'){prefs.nudge+=Number(data.delta)||0;save();applyPreferences();}
      else if(data.type==='reset'){prefs.nudge=0;save();applyPreferences();}};
    socket.onclose=async()=>{setConnection(false,'Reconnecting');try{const response=await fetch('/capacity',{cache:'no-store'});if(await response.text()==='full'){showFull();return;}}catch(_){}setTimeout(connect,Math.min(6000,500*(2**Math.min(retry++,4))));};
    socket.onerror=()=>setConnection(false,'Connection interrupted');
  };
  const ping=()=>{if(socket&&socket.readyState===WebSocket.OPEN){socket.send(JSON.stringify({type:'ping',clientSent:performance.now()}));setTimeout(ping,1000);}};
  const controlScroll=delta=>{if(!state||state.controllerId!==clientId||clientId<=0)return;const indices=lyricBlocks().map(block=>block.index);if(!indices.length)return;let pos=indices.findIndex(index=>index>=(state.scrollLine>=0?state.scrollLine:indices[0]));if(pos<0)pos=0;pos=Math.max(0,Math.min(indices.length-1,pos+delta));state.scrollLine=indices[pos];socket.send(JSON.stringify({type:'scroll',line:state.scrollLine}));lastActive=-2;};
  const showFull=()=>{const full=document.createElement('div');full.id='full';full.textContent='VocLyrics is already serving 8 devices. Ask the host to disconnect one, then refresh.';document.body.appendChild(full);};
  const requestWakeLock=async()=>{try{if('wakeLock'in navigator)wakeLock=await navigator.wakeLock.request('screen');}catch(_){}};
  $('gear').addEventListener('click',()=>settings.classList.toggle('hidden'));
  $('fullscreen').addEventListener('click',async()=>{try{if(!document.fullscreenElement)await document.documentElement.requestFullscreen();else await document.exitFullscreen();}catch(_){}});
  document.addEventListener('fullscreenchange',()=>{$('fullscreen').textContent=document.fullscreenElement?'×':'⛶';$('fullscreen').setAttribute('aria-label',document.fullscreenElement?'Exit fullscreen':'Enter fullscreen');setTimeout(fitSheet,80);});
  window.addEventListener('resize',()=>setTimeout(fitSheet,60));
  $('scroll-up').addEventListener('click',()=>controlScroll(-1)); $('scroll-down').addEventListener('click',()=>controlScroll(1));
  screen.addEventListener('pointerdown',event=>{pointerY=event.clientY;});
  screen.addEventListener('pointerup',event=>{if(pointerY===null)return;const distance=pointerY-event.clientY;if(Math.abs(distance)>22)controlScroll(distance>0?1:-1);pointerY=null;});
  screen.addEventListener('wheel',event=>{if(Math.abs(event.deltaY)>0)controlScroll(event.deltaY>0?1:-1);},{passive:true});
  $('role-confirm').addEventListener('click',()=>chooseRole($('role-first').value)); $('role').addEventListener('change',e=>chooseRole(e.target.value));
  $('transpose').addEventListener('change',e=>{prefs.transpose=Math.max(-12,Math.min(12,Number.parseInt(e.target.value,10)||0));save();applyPreferences();lastActive=-2;});
  $('proportional').addEventListener('change',e=>{prefs.proportional=e.target.checked;save();applyPreferences();}); $('show-clock').addEventListener('change',e=>{prefs.clock=e.target.checked;save();applyPreferences();});
  document.querySelectorAll('[data-nudge]').forEach(button=>button.addEventListener('click',()=>{prefs.nudge=button.dataset.nudge==='reset'?0:prefs.nudge+Number(button.dataset.nudge);save();applyPreferences();}));
  $('keep-awake').addEventListener('click',requestWakeLock); document.addEventListener('visibilitychange',()=>{if(document.visibilityState==='visible')requestWakeLock();});
  applyPreferences(); if(!prefs.role)$('role-setup').classList.remove('hidden'); connect(); requestWakeLock(); draw();
})();
