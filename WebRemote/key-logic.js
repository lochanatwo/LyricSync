((root) => {
  'use strict';
  const names=['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
  const pitchForKey=(rootNote,accidental='')=>{
    const natural={C:0,D:2,E:4,F:5,G:7,A:9,B:11};
    const base=natural[String(rootNote).toUpperCase()];
    if(!Number.isInteger(base))return -1;
    return (base+(['#','♯'].includes(accidental)?1:['b','♭'].includes(accidental)?-1:0)+12)%12;
  };
  const transposeBetween=(source,target)=>{
    let amount=(target-source+12)%12;
    if(amount>6)amount-=12;
    return amount;
  };
  const transposeRoot=(rootNote,accidental,amount)=>
    names[(pitchForKey(rootNote,accidental)+amount+24)%12];
  const transposeChord=(token,amount)=>{
    if(!amount)return token;
    let changed=token.replace(/^([|:]*)([A-G])([#b]?)/,(match,bars,note,accidental)=>
      `${bars}${transposeRoot(note,accidental,amount)}`);
    changed=changed.replace(/\/([A-G])([#b]?)/g,(match,note,accidental)=>
      `/${transposeRoot(note,accidental,amount)}`);
    return changed;
  };
  const transposeChordLine=(line,amount)=>line.split(/(\s+)/).map(part=>
    /^\s+$/.test(part)?part:transposeChord(part,amount)).join('');
  const api={pitchForKey,transposeBetween,transposeChordLine};
  if(typeof module!=='undefined'&&module.exports)module.exports=api;
  else root.VocLyricsKeyLogic=api;
})(typeof window!=='undefined'?window:globalThis);
