const assert=require('node:assert/strict');
const test=require('node:test');
const keys=require('../WebRemote/key-logic.js');

test('per-device target key changes the displayed chord row',()=>{
  const source=keys.pitchForKey('A','#');
  const target=keys.pitchForKey('C');
  const shift=keys.transposeBetween(source,target);
  assert.equal(shift,2);
  assert.equal(keys.transposeChordLine('A#     F/A',shift),'C     G/B');
});

test('flat source and slash chords transpose while keeping spacing',()=>{
  const shift=keys.transposeBetween(keys.pitchForKey('B','b'),keys.pitchForKey('D'));
  assert.equal(shift,4);
  assert.equal(keys.transposeChordLine('Bb   F/A',shift),'D   A/C#');
});

test('Unicode sharp and flat key symbols map to the same pitches',()=>{
  assert.equal(keys.pitchForKey('A','♯'),10);
  assert.equal(keys.pitchForKey('B','♭'),10);
});
