(() => {
  'use strict';
  const $ = id => document.getElementById(id);
  const screen = $('screen'), lyrics = $('lyrics'), settings = $('settings');
  const isIOS=/iPhone|iPad|iPod/i.test(navigator.userAgent)||(navigator.platform==='MacIntel'&&navigator.maxTouchPoints>1);
  const isStandalone=window.matchMedia('(display-mode: standalone)').matches||navigator.standalone===true;
  if(isStandalone)document.documentElement.classList.add('standalone');
  const roles = ['Main Singer','Backup Singer','Main Guitar','Rythm Guitar','Drummer','Percussionist','Bass','Keys'];
  const keyNames = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
  const {pitchForKey,transposeBetween,transposeChordLine}=window.VocLyricsKeyLogic;
  const key = 'voclyrics.remote.v2';
  const defaults = {proportional:true, clock:false, nudge:0, transpose:0, zoomPercent:100, role:'', targetKeys:{}, sourceKeys:{}};
  let prefs = {...defaults};
  try { prefs = {...defaults, ...JSON.parse(localStorage.getItem(key) || '{}')}; } catch (_) {}
  prefs.transpose = Number.isInteger(prefs.transpose) ? Math.max(-12, Math.min(12, prefs.transpose)) : 0;
  prefs.zoomPercent=Number.isInteger(prefs.zoomPercent)?Math.max(60,Math.min(180,prefs.zoomPercent)):100;
  if(!prefs.targetKeys||typeof prefs.targetKeys!=='object'||Array.isArray(prefs.targetKeys))prefs.targetKeys={};
  if(!prefs.sourceKeys||typeof prefs.sourceKeys!=='object'||Array.isArray(prefs.sourceKeys))prefs.sourceKeys={};
  if (!roles.includes(prefs.role)) prefs.role = '';
  let socket, state = null, rows = [], renderedLyrics = null, renderedSongId = '', originalKeyPitch = -1, reportedKeyPitch = -1, hostOffset = 0, retry = 0, wakeLock = null, clientId = 0, pointerY = null, dragAccumulator = 0, dragging = false, lastActive = -2, lastRenderedActive = -1, renderSequence = 0, polling = false;
  const save = () => { try { localStorage.setItem(key, JSON.stringify(prefs)); } catch (_) {} };
  const parse = source => source.split(/\r?\n/).map(line => {
    const m = line.match(/^\[(\d+):(\d{1,2})(?:[.,](\d{1,3}))?\](.*)$/);
    const time=m?Number(m[1])*60+Number(m[2])+Number(('0.'+(m[3]||'0')).slice(0,5)):-1;
    const text=m?m[4]:line;
    const keyLine=text.match(/^\s*key\s*[-:]\s*([A-G])([#b♯♭]?)(?:\s*(?:\||\()?\s*transposed by:\s*[+-]?\d+\s*\)?)?\s*$/i);
    if(keyLine)return {time,text,section:false,key:true,keyPitch:pitchForKey(keyLine[1],keyLine[2])};
    if(m)return {time,text,section:false};
    const section = text.match(/^\[([^\]]+)\]$/);
    return text.trim() ? {time:-1,text:section?section[1]:text,section:!!section} : null;
  }).filter(Boolean);
  const roleOptions = roles.map(role => `<option value="${role}">${role}</option>`).join('');
  $('role').innerHTML = roleOptions;
  $('role-first').innerHTML = `<option value="" disabled selected>Select your role</option>${roleOptions}`;
  $('target-key').innerHTML=keyNames.map((name,index)=>`<option value="${index}">${name}</option>`).join('');
  $('source-key-choice').innerHTML='<option value="auto">File</option>'+keyNames.map((name,index)=>`<option value="${index}">${name}</option>`).join('');
  const identify = () => { if (socket && socket.readyState === WebSocket.OPEN && prefs.role) socket.send(JSON.stringify({type:'hello',name:prefs.role})); };
  const chooseRole = role => { if (!roles.includes(role)) return; prefs.role=role; save(); applyPreferences(); $('role-setup').classList.add('hidden'); identify(); };
  const applyPreferences = () => {
    $('proportional').checked=prefs.proportional; $('show-clock').checked=prefs.clock; $('clock').style.display=prefs.clock?'':'none';
    $('nudge-value').textContent=`${prefs.nudge} ms`; $('transpose-status').textContent=`TRANSPOSED BY: ${prefs.transpose}`;
    $('role').value=prefs.role; $('role-badge').textContent=(prefs.role||'Choose role').toUpperCase();
    $('zoom-value').textContent=`${prefs.zoomPercent}%`;
    screen.classList.toggle('fit-text',prefs.proportional);
  };
  const songPreferenceKey=()=>state&&state.songId?state.songId:(state&&state.title?`title:${state.title}`:'current');
  const visibleRowCount=()=>Math.max(8,Math.min(25,Math.round(1500/prefs.zoomPercent)));
  const syncKeyControls=()=>{
    const preferenceKey=songPreferenceKey();
    const sourceChoice=$('source-key-choice');
    const sourceOverride=Number(prefs.sourceKeys[preferenceKey]);
    const hasSourceOverride=Object.prototype.hasOwnProperty.call(prefs.sourceKeys,preferenceKey)&&Number.isInteger(sourceOverride)&&sourceOverride>=0&&sourceOverride<12;
    originalKeyPitch=hasSourceOverride?sourceOverride:reportedKeyPitch;
    sourceChoice.value=hasSourceOverride?String(sourceOverride):'auto';
    const select=$('target-key'),source=$('source-key');
    if(originalKeyPitch<0){select.disabled=true;select.value='';source.textContent='Choose source key';prefs.transpose=0;applyPreferences();return;}
    select.disabled=false;source.textContent=`Key- ${keyNames[originalKeyPitch]}`;
    const remembered=Object.prototype.hasOwnProperty.call(prefs.targetKeys,preferenceKey);
    let target=remembered?Number(prefs.targetKeys[preferenceKey]):originalKeyPitch;
    if(!Number.isInteger(target)||target<0||target>11)target=originalKeyPitch;
    if(!remembered){prefs.targetKeys[preferenceKey]=target;save();}
    prefs.transpose=transposeBetween(originalKeyPitch,target);select.value=String(target);applyPreferences();
  };
  const refreshChords=()=>{renderSequence+=1;lyrics.replaceChildren();lastRenderedActive=-1;lastActive=-2;};
  const position = () => !state ? 0 : state.position + (state.playing ? Math.max(0,performance.now()+hostOffset+prefs.nudge-state.hostMs)/1000 : 0);
  const canControlScroll = () => !!state&&clientId>0;
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
      if(rows[index].key)continue;
      if(isDivider(rows[index].text))continue;
      if(isChordOnly(rows[index].text)&&index+1<rows.length&&!rows[index+1].section){
        result.push({index,chord:rows[index].text,text:rows[index+1].text,section}); index+=1;
      }else result.push({index,chord:'',text:rows[index].text,section});
    }
    return result;
  };
  const appendBlock = (parent,block) => {
    const wrapper=document.createElement('div'); wrapper.className='lyric-block';
    const chord=document.createElement('div'); chord.className='chord-row'; chord.textContent=block.chord?transposeChordLine(block.chord,prefs.transpose):'\u00a0';
    const text=document.createElement('div'); text.className='lyric-row'; text.textContent=block.text;
    wrapper.append(chord,text); parent.appendChild(wrapper);
  };
  const fitSheet = () => {
    const card=document.querySelector('.now-card'); if(!card)return;
    const lines=[...lyrics.querySelectorAll('.chord-row,.lyric-row')]; if(!lines.length)return;
    const longest=Math.max(8,...lines.map(line=>line.textContent.length));
    const width=Math.max(120,lyrics.clientWidth-(window.innerWidth<=760?40:8));
    const height=Math.max(100,lyrics.clientHeight);
    const widthFit=Math.floor(width/(longest*.62));
    const size=Math.max(8,Math.min(32,Math.floor(widthFit*prefs.zoomPercent/100),Math.floor(height/(visibleRowCount()*2.04))));
    lyrics.style.fontSize=`${size}px`;
  };
  const renderRows = active => {
    const blocks=lyricBlocks(); if (!blocks.length) return;
    let start=blocks.findIndex(block=>block.index>=active); if(start<0) start=Math.max(0,blocks.length-1);
    if(start>0&&blocks[start].index>active)start-=1;
    $('section-label').textContent=(blocks[start].section||'SONG').toUpperCase();
    const frame=document.createElement('div');frame.className='sheet-frame';
    const shown=visibleRowCount();
    frame.style.setProperty('--visible-rows',String(shown));
    blocks.slice(start,start+shown).forEach(block=>appendBlock(frame,block));
    const oldFrames=[...lyrics.querySelectorAll('.sheet-frame')];
    oldFrames.slice(0,-1).forEach(old=>old.remove());
    const old=oldFrames.length?oldFrames[oldFrames.length-1]:null,direction=lastRenderedActive<0||active>=lastRenderedActive?1:-1;
    lastRenderedActive=active;
    if(!old){lyrics.replaceChildren(frame);requestAnimationFrame(fitSheet);return;}
    const sequence=++renderSequence;
    frame.classList.add(direction>0?'enter-below':'enter-above');lyrics.appendChild(frame);
    requestAnimationFrame(()=>requestAnimationFrame(()=>{old.classList.add(direction>0?'exit-up':'exit-down');frame.classList.add('slide-centre');fitSheet();}));
    setTimeout(()=>{if(sequence!==renderSequence)return;old.remove();frame.className='sheet-frame';fitSheet();},440);
  };
  const draw = () => {
    if(state){
      let active=-1,best=-1; rows.forEach((r,i)=>{if(!r.section&&r.time>=0&&r.time<=position()&&r.time>=best){active=i;best=r.time;}});
      if(Number.isInteger(state.activeLine)&&state.activeLine>=0) active=Math.min(state.activeLine,Math.max(0,rows.length-1));
      if(Number.isInteger(state.scrollLine)&&state.scrollLine>=0) active=Math.min(state.scrollLine,Math.max(0,rows.length-1));
      if(active<0) active=rows.findIndex(r=>!r.section);
      $('song').textContent=state.title||'VocLyrics';
      const canControl=canControlScroll();
      $('scroll-up').disabled=!canControl; $('scroll-down').disabled=!canControl;
      screen.classList.toggle('can-scroll',canControl);
      if(active!==lastActive){renderRows(active);lastActive=active;}
    }
    if(prefs.clock)$('clock').textContent=new Date().toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'});
    setTimeout(draw,33);
  };
  const setConnection=(ok,message)=>{$('connection').classList.toggle('connected',ok);$('connection span').textContent=message;};
  const acceptMessage=data=>{
    if(data.type==='hello')clientId=Number(data.clientId)||0;
    else if(data.type==='state'){
      state=data;
      const incomingKeyPitch=Number.isInteger(data.songKey)&&data.songKey>=0?data.songKey:
        (renderedLyrics===data.lyrics&&renderedSongId===(data.songId||'')?reportedKeyPitch:(parse(data.lyrics||'').find(row=>row.key)?.keyPitch??-1));
      if(renderedLyrics!==data.lyrics||renderedSongId!==(data.songId||'')||reportedKeyPitch!==incomingKeyPitch){
        renderedLyrics=data.lyrics;renderedSongId=data.songId||'';rows=parse(data.lyrics||'');
        reportedKeyPitch=incomingKeyPitch;
        syncKeyControls();refreshChords();
      }
    }
    else if(data.type==='pong'){const sent=Number(data.clientSent),received=performance.now(),measured=Number(data.hostMs)-(sent+received)/2;hostOffset+=(measured-hostOffset)*.2;}
    else if(data.type==='nudge'){prefs.nudge+=Number(data.delta)||0;save();applyPreferences();}
    else if(data.type==='reset'){prefs.nudge=0;save();applyPreferences();}
  };
  const startPolling=()=>{
    if(polling)return; polling=true;
    const poll=async()=>{
      if(socket&&socket.readyState===WebSocket.OPEN){polling=false;return;}
      try{const response=await fetch(`/state?t=${Date.now()}`,{cache:'no-store'});if(response.ok){acceptMessage(await response.json());setConnection(true,'Connected');}}catch(_){setConnection(false,'Reconnecting');}
      if(polling)setTimeout(poll,200);
    };
    poll();
  };
  const connect=()=>{
    try{socket=new WebSocket(`${location.protocol==='https:'?'wss:':'ws:'}//${location.host}/ws`);}catch(_){startPolling();setTimeout(connect,1500);return;}
    socket.onopen=()=>{polling=false;retry=0;setConnection(true,'Connected');identify();ping();};
    socket.onmessage=event=>{let data;try{data=JSON.parse(event.data);}catch(_){return;}acceptMessage(data);};
    socket.onclose=async()=>{setConnection(false,'Reconnecting');startPolling();try{const response=await fetch('/capacity',{cache:'no-store'});if(await response.text()==='full'){showFull();return;}}catch(_){}setTimeout(connect,Math.min(6000,500*(2**Math.min(retry++,4))));};
    socket.onerror=()=>{setConnection(false,'Connection interrupted');startPolling();};
    setTimeout(()=>{if(!socket||socket.readyState!==WebSocket.OPEN)startPolling();},1500);
  };
  const ping=()=>{if(socket&&socket.readyState===WebSocket.OPEN){socket.send(JSON.stringify({type:'ping',clientSent:performance.now()}));setTimeout(ping,1000);}};
  const controlScroll=delta=>{if(!canControlScroll()||!socket||socket.readyState!==WebSocket.OPEN)return;const indices=lyricBlocks().map(block=>block.index);if(!indices.length)return;let pos=indices.findIndex(index=>index>=(state.scrollLine>=0?state.scrollLine:indices[0]));if(pos<0)pos=0;pos=Math.max(0,Math.min(indices.length-1,pos+delta));state.scrollLine=indices[pos];socket.send(JSON.stringify({type:'scroll',line:state.scrollLine}));lastActive=-2;};
  const showFull=()=>{const full=document.createElement('div');full.id='full';full.textContent='VocLyrics is already serving 8 devices. Ask the host to disconnect one, then refresh.';document.body.appendChild(full);};
  const requestWakeLock=async()=>{try{if('wakeLock'in navigator)wakeLock=await navigator.wakeLock.request('screen');}catch(_){}};
  $('gear').addEventListener('click',()=>settings.classList.toggle('hidden'));
  const fullscreenButton=$('fullscreen'),iosHelp=$('ios-fullscreen-help'),iosHelpClose=$('ios-help-close');
  if(fullscreenButton)fullscreenButton.addEventListener('click',async()=>{if(isIOS&&!isStandalone){if(iosHelp)iosHelp.classList.remove('hidden');return;}try{if(!document.fullscreenElement&&document.documentElement.requestFullscreen)await document.documentElement.requestFullscreen();else if(document.fullscreenElement&&document.exitFullscreen)await document.exitFullscreen();}catch(_){if(iosHelp)iosHelp.classList.remove('hidden');}});
  if(iosHelpClose)iosHelpClose.addEventListener('click',()=>iosHelp.classList.add('hidden'));
  document.addEventListener('fullscreenchange',()=>{$('fullscreen').textContent=document.fullscreenElement?'×':'⛶';$('fullscreen').setAttribute('aria-label',document.fullscreenElement?'Exit fullscreen':'Enter fullscreen');setTimeout(fitSheet,80);});
  window.addEventListener('resize',()=>setTimeout(fitSheet,60));
  $('scroll-up').addEventListener('click',()=>controlScroll(-2)); $('scroll-down').addEventListener('click',()=>controlScroll(2));
  const finishDrag=()=>{
    if(!dragging)return;dragging=false;pointerY=null;dragAccumulator=0;
    lyrics.classList.remove('finger-dragging');lyrics.style.transition='transform .2s cubic-bezier(.22,.72,.18,1)';lyrics.style.transform='translate3d(0,0,0)';
    setTimeout(()=>{if(!dragging){lyrics.style.removeProperty('transform');lyrics.style.removeProperty('transition');}},220);
  };
  lyrics.addEventListener('pointerdown',event=>{
    if(!canControlScroll())return;dragging=true;pointerY=event.clientY;dragAccumulator=0;
    lyrics.classList.add('finger-dragging');lyrics.style.transition='none';
    if(lyrics.setPointerCapture)lyrics.setPointerCapture(event.pointerId);
  });
  lyrics.addEventListener('pointermove',event=>{
    if(!dragging||pointerY===null||!canControlScroll())return;
    event.preventDefault();dragAccumulator+=pointerY-event.clientY;pointerY=event.clientY;
    const threshold=Math.max(24,Math.min(48,window.innerHeight*.052));
    const steps=Math.min(3,Math.floor(Math.abs(dragAccumulator)/threshold));
    if(steps>0){const direction=dragAccumulator>0?1:-1;controlScroll(direction*steps);dragAccumulator-=direction*steps*threshold;}
    lyrics.style.transform=`translate3d(0,${-dragAccumulator}px,0)`;
  },{passive:false});
  lyrics.addEventListener('pointerup',finishDrag);lyrics.addEventListener('pointercancel',finishDrag);
  screen.addEventListener('wheel',event=>{if(Math.abs(event.deltaY)>0)controlScroll(event.deltaY>0?1:-1);},{passive:true});
  document.addEventListener('keydown',event=>{
    if(event.key!=='ArrowUp'&&event.key!=='ArrowDown')return;
    const tag=document.activeElement&&document.activeElement.tagName;
    if(tag==='INPUT'||tag==='SELECT'||tag==='TEXTAREA')return;
    if(!canControlScroll())return;
    event.preventDefault();controlScroll(event.key==='ArrowUp'?-2:2);
  });
  $('role-confirm').addEventListener('click',()=>chooseRole($('role-first').value)); $('role').addEventListener('change',e=>chooseRole(e.target.value));
  $('target-key').addEventListener('change',event=>{
    const target=Number(event.target.value);if(originalKeyPitch<0||!Number.isInteger(target))return;
    prefs.targetKeys[songPreferenceKey()]=target;prefs.transpose=transposeBetween(originalKeyPitch,target);
    save();applyPreferences();refreshChords();
  });
  const changeZoom=delta=>{
    prefs.zoomPercent=Math.max(60,Math.min(180,prefs.zoomPercent+delta));
    save();applyPreferences();refreshChords();
  };
  $('zoom-out').addEventListener('click',()=>changeZoom(-10));
  $('zoom-in').addEventListener('click',()=>changeZoom(10));
  $('source-key-choice').addEventListener('change',event=>{
    const value=event.target.value,preferenceKey=songPreferenceKey();
    if(value==='auto')delete prefs.sourceKeys[preferenceKey];
    else {const pitch=Number(value);if(!Number.isInteger(pitch)||pitch<0||pitch>11)return;prefs.sourceKeys[preferenceKey]=pitch;}
    save();syncKeyControls();refreshChords();
  });
  $('proportional').addEventListener('change',e=>{prefs.proportional=e.target.checked;save();applyPreferences();}); $('show-clock').addEventListener('change',e=>{prefs.clock=e.target.checked;save();applyPreferences();});
  document.querySelectorAll('[data-nudge]').forEach(button=>button.addEventListener('click',()=>{prefs.nudge=button.dataset.nudge==='reset'?0:prefs.nudge+Number(button.dataset.nudge);save();applyPreferences();}));
  $('keep-awake').addEventListener('click',requestWakeLock); document.addEventListener('visibilitychange',()=>{if(document.visibilityState==='visible')requestWakeLock();});
  applyPreferences(); if(!prefs.role)$('role-setup').classList.remove('hidden'); connect(); requestWakeLock(); draw();
})();
