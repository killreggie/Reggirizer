// 07BASED: connects the TV interface (index.html) to the plugin.
//
// UI -> plugin: index.html calls BASED.emit(type, ...) whenever the user changes something.
// Plugin -> UI: parameter changes from the host (automation, presets, session load) call
//               the BASED.api.* setters, which update the screen without echoing back.
import * as Juce from "./juce/index.js";

const B = window.BASED, api = B.api;
if (window.__JUCE__) {
  B.plugin = true;
  B.bridged = true; // index.html shows a warning if this never gets set

  const prefix = { t2: "rz_", mx: "bp_", hr: "gc_", td: "tc_" };
  const paramOf = (uiId) => { const [sec, k] = uiId.split(":"); return prefix[sec] + k.replace(/\s+/g, ""); };
  const cache = {};
  const slider = (id) => cache[id] || (cache[id] = Juce.getSliderState(id));
  const toggle = (id) => cache[id] || (cache[id] = Juce.getToggleState(id));
  const combo = (id) => cache[id] || (cache[id] = Juce.getComboBoxState(id));
  const call = (name, ...args) => Juce.getNativeFunction(name)(...args);
  call("uiLog", "UI connected to the audio engine");
  window.addEventListener("error", (e) => call("uiLog", `error: ${e.message} (${(e.filename || "").split("/").pop()}:${e.lineno})`));
  window.addEventListener("unhandledrejection", (e) => call("uiLog", `error: ${e.reason}`));

  const gcToggles = ["gc_aa", "gc_dc", "gc_soft"];
  const fxOn = { t2: "on_rz", mx: "on_bp", hr: "on_gc", td: "on_tc" };
  let saveTimer = 0;

  // ---------- UI -> plugin ----------
  B.emit = (type, ...a) => {
    switch (type) {
      case "knob": slider(paramOf(a[0])).setNormalisedValue(a[1]); break;
      case "knobStart": slider(paramOf(a[0])).sliderDragStarted(); break;
      case "knobEnd": slider(paramOf(a[0])).sliderDragEnded(); break;
      case "mode": combo(a[0] === "hr" ? "gc_mode" : "tc_mode").setChoiceIndex(a[1]); break;
      case "toggle": toggle(gcToggles[a[1]]).setValue(a[2]); break;
      case "power": toggle("on_gc").setValue(a[1]); break;
      case "bus": call("setOrder", a[0].map((f) => f.id)); a[0].forEach((f) => toggle(fxOn[f.id]).setValue(f.on)); break;
      case "slot": { const s = slider(a[0] === "time" ? "bp_tslot" : "bp_vslot"); s.setNormalisedValue(a[1] / 31); break; }
      case "bpEdits": clearTimeout(saveTimer); saveTimer = setTimeout(() => call("saveBluEdits", JSON.stringify(a[0])), 120); break;
      case "hold": toggle("bp_hold").setValue(a[0]); break;
      case "notes": a[0].forEach((on, i) => toggle("rz_n" + i).setValue(on)); break;
      case "scale": combo("rz_key").setChoiceIndex(a[0]); combo("rz_scale").setChoiceIndex(a[1]); break;
      case "stab": combo("rz_stab").setChoiceIndex(a[0]); break;
      case "blast": call("blast", a[0] || 0); break;
      case "tcPreset": call("setTcPreset", a[0], a[1]); break;
      default: break; // "env" is for the mockup only: the plugin rebuilds envelopes itself
    }
  };

  // ---------- plugin -> UI ----------
  // Knobs: every [data-k] dial on any channel.
  document.querySelectorAll(".ch [data-k]").forEach((el) => {
    const uiId = `${el.closest(".ch").id}:${el.dataset.k}`, s = slider(paramOf(uiId));
    s.valueChangedEvent.addListener(() => api.knob(uiId, s.getNormalisedValue()));
  });

  combo("gc_mode").valueChangedEvent.addListener(() => api.gcMode(combo("gc_mode").getChoiceIndex()));
  combo("tc_mode").valueChangedEvent.addListener(() => api.tcMode(combo("tc_mode").getChoiceIndex()));
  gcToggles.forEach((id, i) => toggle(id).valueChangedEvent.addListener(() => api.gcToggle(i, toggle(id).getValue())));
  Object.entries(fxOn).forEach(([fx, id]) => toggle(id).valueChangedEvent.addListener(() => {
    const on = toggle(id).getValue();
    api.busOn(fx, on);
    if (fx === "hr") api.gcPower(on);
  }));

  // Reggirizer keyboard + key/scale/stabilizer: batch the 15 updates into one redraw.
  let rzQueued = false;
  const rzSync = () => {
    if (rzQueued) return;
    rzQueued = true;
    queueMicrotask(() => {
      rzQueued = false;
      const notes = Array.from({ length: 12 }, (_, i) => toggle("rz_n" + i).getValue());
      api.rz.set(notes, combo("rz_key").getChoiceIndex(), combo("rz_scale").getChoiceIndex(), combo("rz_stab").getChoiceIndex());
    });
  };
  for (let i = 0; i < 12; i++) toggle("rz_n" + i).valueChangedEvent.addListener(rzSync);
  ["rz_key", "rz_scale", "rz_stab"].forEach((id) => combo(id).valueChangedEvent.addListener(rzSync));

  // Blu Pill slots + hold.
  let bpQueued = false;
  const bpSync = () => {
    if (bpQueued) return;
    bpQueued = true;
    queueMicrotask(() => {
      bpQueued = false;
      api.bp.load(null, { time: Math.round(slider("bp_tslot").getScaledValue()), vol: Math.round(slider("bp_vslot").getScaledValue()) }, toggle("bp_hold").getValue());
    });
  };
  ["bp_tslot", "bp_vslot"].forEach((id) => slider(id).valueChangedEvent.addListener(bpSync));
  toggle("bp_hold").valueChangedEvent.addListener(bpSync);

  // Saved UI-only state: bus order, edited Blu Pill slots, Trap City track preset.
  call("getState").then((st) => {
    api.bus(st.order.map((id) => ({ id, on: toggle(fxOn[id]).getValue() })));
    try { api.bp.load(JSON.parse(st.bluEdits || "{}"), null, toggle("bp_hold").getValue()); } catch (e) { /* bad JSON: keep presets */ }
    const [act, track] = String(st.tcPreset || "0,6").split(",").map(Number);
    api.tcPreset(act, track);
  });

  // Live readouts from the audio engine (~30 per second).
  window.__JUCE__.backend.addEventListener("status", (s) => {
    api.live = s;
    api.rz.pitch({ det: s.det, tgt: s.tgt, cents: s.cents, corr: s.corr });
    api.bp.phase = s.phase >= 0 ? s.phase : null;
  });
}
