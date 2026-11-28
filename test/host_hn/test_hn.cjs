/* SPDX-License-Identifier: MIT */
// AI noise reduction: the page's resampler (filter weights cached per fractional position) gives the same
// samples as HamNoise's per-sample formula, and AUTO picks the CW model only in CW.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const page = fs.readFileSync(require('node:path').join(__dirname, '../../main/index.html'), 'utf8');
const section = (from, to) => {
  const a = page.indexOf(from);
  const b = page.indexOf(to, a);
  assert.ok(a >= 0 && b > a, 'section ' + from);
  return page.slice(a, b);
};

// 1. The resampler, against the original per-sample weights (HamNoise denoise-worklet.js)
const ctx = vm.createContext({});
vm.runInContext(section('class SincResampler {', 'class DenoiseWorklet') + 'this.SincResampler = SincResampler;', ctx);
function originalWeightAt(r, position, buffer, baseIndex) {
  const center = Math.floor(position);
  let acc = 0, weightSum = 0;
  for (let index = center - r.radius; index <= center + r.radius; index += 1) {
    const sample = buffer[index - baseIndex] ?? 0;
    const distance = position - index, absDistance = Math.abs(distance);
    if (absDistance > r.radius) continue;
    const sincArg = 2 * r.cutoffNorm * distance;
    const sinc = Math.abs(sincArg) < 1e-8 ? 1 : Math.sin(Math.PI * sincArg) / (Math.PI * sincArg);
    const x = absDistance / r.radius;
    const window = 0.42 + 0.5 * Math.cos(Math.PI * x) + 0.08 * Math.cos(2 * Math.PI * x);
    const weight = 2 * r.cutoffNorm * sinc * window;
    acc += sample * weight;
    weightSum += weight;
  }
  return Math.abs(weightSum) > 1e-8 ? acc / weightSum : 0;
}
for (const [from, to, radius] of [[48000, 9600, 128], [16000, 9600, 128], [44100, 9600, 128], [9600, 48000, 16], [9600, 16000, 16]]) {
  const got = [];
  const r = new ctx.SincResampler(from, to, (v) => got.push(v), radius);
  // the reference recomputes each output with the original formula on the same buffer state
  const want = [];
  const origSampleAt = r.sampleAt.bind(r);
  r.sampleAt = (position) => { want.push(originalWeightAt(r, position, r.buffer, r.baseIndex)); return origSampleAt(position); };
  let seed = 11;
  const rnd = () => { seed = (seed * 1103515245 + 12345) >>> 0; return seed / 4294967296 - 0.5; };
  for (let block = 0; block < 40; block++) r.process(Float32Array.from({ length: 128 }, rnd));
  assert.ok(got.length > 100, 'outputs ' + from + '->' + to);
  let maxErr = 0;
  for (let i = 0; i < got.length; i++) maxErr = Math.max(maxErr, Math.abs(got[i] - want[i]));
  // positions are keyed to a millionth of a sample: the difference stays far below float32 audio (6e-8)
  assert.ok(maxErr < 1e-7, `${from} -> ${to}: same samples as the original (max error ${maxErr})`);
  assert.ok(r.weights.size <= 256, 'a bounded weight cache');
}

// 2. AUTO: the CW model in CW only
const sel = vm.createContext({ hnSettings: { model: 'auto' }, hnMode: ['', ''] });
vm.runInContext(section('function hnTargetFor(i) {', 'function hnParams(') + 'this.hnTargetFor = hnTargetFor;', sel);
const pick = (mode, model = 'auto') => { sel.hnSettings.model = model; sel.hnMode[0] = mode; return sel.hnTargetFor(0); };
assert.equal(pick('CW-U'), 'cw');
assert.equal(pick('CW-L'), 'cw');
for (const m of ['USB', 'LSB', 'AM', 'FM', 'DATA-USB', 'RTTY-LSB', 'C4FM-DN', '']) assert.equal(pick(m), 'voice', m);
assert.equal(pick('USB', 'cw'), 'cw');
assert.equal(pick('CW-U', 'voice'), 'voice');

console.log('AI noise reduction: resampler matches HamNoise (5 rate pairs), AUTO model choice');
