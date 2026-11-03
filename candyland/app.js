(() => {
  "use strict";

  /** Bump this on every Candyland release so phones show the loaded build. */
  const APP_REV = 12;

  const COLORS = {
    red: { name: "Red", hex: "#ff3b4e" },
    purple: { name: "Purple", hex: "#7b61ff" },
    yellow: { name: "Yellow", hex: "#ffd000" },
    blue: { name: "Blue", hex: "#2eb7ff" },
    orange: { name: "Orange", hex: "#ff8a1f" },
    green: { name: "Green", hex: "#2ad17a" },
  };

  const TREATS = {
    peppermint: {
      name: "Peppermint Forest",
      hint: "Go to Peppermint Forest!",
      art: "peppermint",
    },
    peanut: {
      name: "Peanut Brittle House",
      hint: "Go to Peanut Brittle House!",
      art: "peanut",
    },
    lollipop: {
      name: "Lollipop Woods",
      hint: "Go to Lollipop Woods!",
      art: "lollipop",
    },
    icecream: {
      name: "Ice Cream Sea",
      hint: "Go to Ice Cream Sea!",
      art: "icecream",
    },
    gumdrop: {
      name: "Gumdrop Mountains",
      hint: "Go to Gumdrop Mountains!",
      art: "gumdrop",
    },
    gingerbread: {
      name: "Gingerbread Tree",
      hint: "Go to Gingerbread Tree!",
      art: "gingerbread",
    },
    candyhearts: {
      name: "Candy Hearts",
      hint: "Go to Candy Hearts!",
      art: "candyhearts",
    },
  };

  const DECK_SPECS = {
    classic64: {
      label: "Classic (64)",
      singles: { red: 8, purple: 8, yellow: 8, blue: 8, orange: 7, green: 7 },
      doubles: { red: 2, purple: 2, yellow: 2, blue: 2, orange: 2, green: 2 },
      treats: ["peppermint", "peanut", "lollipop", "icecream", "gumdrop", "gingerbread"],
    },
    vintage64: {
      label: "Vintage (64)",
      singles: { red: 7, purple: 8, yellow: 8, blue: 8, orange: 7, green: 7 },
      doubles: { red: 2, purple: 2, yellow: 2, blue: 2, orange: 2, green: 2 },
      treats: [
        "candyhearts",
        "peppermint",
        "peanut",
        "lollipop",
        "icecream",
        "gumdrop",
        "gingerbread",
      ],
    },
    modern44: {
      label: "Modern (44)",
      singles: { red: 3, purple: 3, yellow: 4, blue: 3, orange: 4, green: 4 },
      doubles: { red: 3, purple: 3, yellow: 3, blue: 3, orange: 3, green: 3 },
      treats: ["peppermint", "peanut", "lollipop", "icecream", "gumdrop"],
    },
  };

  const STORAGE_KEY = "candyland-drawer-v2";

  const els = {
    progress: document.getElementById("progress-bar"),
    deckStack: document.getElementById("deck-stack"),
    placeholder: document.getElementById("placeholder"),
    playCard: document.getElementById("play-card"),
    drawBtn: document.getElementById("draw-btn"),
    resetBtn: document.getElementById("reset-btn"),
    deckSelect: document.getElementById("deck-select"),
    toast: document.getElementById("toast"),
    modal: document.getElementById("reset-modal"),
    resetCancel: document.getElementById("reset-cancel"),
    resetConfirm: document.getElementById("reset-confirm"),
  };

  let state = {
    deckType: "classic64",
    deck: [],
    discard: [],
    current: null,
  };

  /**
   * Play chimes with HTMLAudioElement + WAV blobs (media playback).
   * iPhone Silent mode mutes many Web Audio beeps, but media audio
   * (same path as audiobooks) still plays. Never await play().
   */
  const SAMPLE_RATE = 22050;
  let soundUrls = null;
  /**
   * Two players per sound (ping-pong). Reusing one Audio and pause/seek/play
   * is flaky on iOS 15; alternating avoids that without allocating every draw.
   * Always call play() directly from the tap handler (no setTimeout) or iOS
   * drops the user-gesture and sound fails ~randomly.
   */
  const soundPlayers = {
    draw: [null, null],
    empty: [null, null],
    treat: [null, null],
  };
  const soundSlot = { draw: 0, empty: 0, treat: 0 };

  function writeAscii(view, offset, text) {
    for (let i = 0; i < text.length; i += 1) {
      view.setUint8(offset + i, text.charCodeAt(i));
    }
  }

  function samplesToWavUrl(samples) {
    const bytes = samples.length * 2;
    const buffer = new ArrayBuffer(44 + bytes);
    const view = new DataView(buffer);
    writeAscii(view, 0, "RIFF");
    view.setUint32(4, 36 + bytes, true);
    writeAscii(view, 8, "WAVE");
    writeAscii(view, 12, "fmt ");
    view.setUint32(16, 16, true);
    view.setUint16(20, 1, true);
    view.setUint16(22, 1, true);
    view.setUint32(24, SAMPLE_RATE, true);
    view.setUint32(28, SAMPLE_RATE * 2, true);
    view.setUint16(32, 2, true);
    view.setUint16(34, 16, true);
    writeAscii(view, 36, "data");
    view.setUint32(40, bytes, true);
    let o = 44;
    for (let i = 0; i < samples.length; i += 1) {
      const clipped = Math.max(-1, Math.min(1, samples[i]));
      view.setInt16(o, clipped < 0 ? clipped * 0x8000 : clipped * 0x7fff, true);
      o += 2;
    }
    return URL.createObjectURL(new Blob([buffer], { type: "audio/wav" }));
  }

  function mixTone(samples, freq, startSec, durSec, amp, waveType) {
    const start = Math.floor(startSec * SAMPLE_RATE);
    const n = Math.floor(durSec * SAMPLE_RATE);
    const attack = 0.02;
    const release = Math.min(0.22, durSec * 0.45);
    for (let i = 0; i < n; i += 1) {
      const t = i / SAMPLE_RATE;
      let gain = amp;
      if (t < attack) gain *= t / attack;
      else if (t > durSec - release) gain *= Math.max(0, (durSec - t) / release);
      const phase = 2 * Math.PI * freq * t;
      let wave = Math.sin(phase);
      if (waveType === "triangle") {
        const x = (freq * t) % 1;
        wave = 4 * Math.abs(x - 0.5) - 1;
      }
      const idx = start + i;
      if (idx < samples.length) samples[idx] += wave * gain;
    }
  }

  function renderClip(durationSec, buildFn) {
    const samples = new Float32Array(Math.ceil(durationSec * SAMPLE_RATE));
    buildFn(samples);
    let peak = 0.001;
    for (let i = 0; i < samples.length; i += 1) {
      peak = Math.max(peak, Math.abs(samples[i]));
    }
    const norm = Math.min(1, 0.92 / peak);
    for (let i = 0; i < samples.length; i += 1) samples[i] *= norm;
    return samplesToWavUrl(samples);
  }

  function ensureSounds() {
    if (soundUrls) return soundUrls;
    soundUrls = {
      draw: renderClip(0.55, (s) => {
        mixTone(s, 1046.5, 0, 0.45, 0.55);
        mixTone(s, 1568, 0.06, 0.5, 0.35);
        mixTone(s, 2093, 0.1, 0.35, 0.2);
      }),
      empty: renderClip(0.95, (s) => {
        mixTone(s, 880, 0, 0.35, 0.5);
        mixTone(s, 1318.5, 0.07, 0.4, 0.32);
        mixTone(s, 880, 0.42, 0.35, 0.5);
        mixTone(s, 1318.5, 0.5, 0.45, 0.32);
      }),
      treat: renderClip(0.85, (s) => {
        mixTone(s, 523.25, 0, 0.22, 0.42, "triangle");
        mixTone(s, 659.25, 0.1, 0.22, 0.42, "triangle");
        mixTone(s, 783.99, 0.2, 0.24, 0.42, "triangle");
        mixTone(s, 1046.5, 0.32, 0.5, 0.48);
        mixTone(s, 1568, 0.4, 0.45, 0.28);
      }),
    };
    return soundUrls;
  }

  function createPlayer(kind) {
    const url = ensureSounds()[kind];
    if (!url) return null;
    const audio = new Audio(url);
    audio.setAttribute("playsinline", "true");
    audio.preload = "auto";
    audio.volume = 1;
    return audio;
  }

  function nextPlayer(kind) {
    const slot = soundSlot[kind];
    soundSlot[kind] = slot ^ 1;
    if (!soundPlayers[kind][slot]) {
      soundPlayers[kind][slot] = createPlayer(kind);
    }
    return { audio: soundPlayers[kind][slot], slot };
  }

  function armSoundPlayers() {
    ensureSounds();
    ["draw", "empty", "treat"].forEach((kind) => {
      soundPlayers[kind][0] = soundPlayers[kind][0] || createPlayer(kind);
      soundPlayers[kind][1] = soundPlayers[kind][1] || createPlayer(kind);
    });
  }

  function playSound(kind) {
    try {
      const picked = nextPlayer(kind);
      const audio = picked.audio;
      if (!audio) return;
      try {
        if (!audio.paused) audio.pause();
      } catch (_) {
        /* ignore */
      }
      try {
        audio.currentTime = 0;
      } catch (_) {
        /* ignore seek errors on cold start */
      }
      const result = audio.play();
      if (result && typeof result.catch === "function") {
        result.catch(() => {
          soundPlayers[kind][picked.slot] = createPlayer(kind);
          const retry = soundPlayers[kind][picked.slot];
          if (!retry) return;
          try {
            retry.currentTime = 0;
          } catch (_) {
            /* ignore */
          }
          const again = retry.play();
          if (again && typeof again.catch === "function") again.catch(() => {});
        });
      }
    } catch (_) {
      /* never block drawing on sound errors */
    }
  }

  function playDrawChime() {
    playSound("draw");
  }

  function playEmptyChime() {
    playSound("empty");
  }

  function playTreatSound() {
    playSound("treat");
  }

  function buildDeck(type) {
    const spec = DECK_SPECS[type];
    const cards = [];
    let id = 0;

    for (const [color, count] of Object.entries(spec.singles)) {
      for (let i = 0; i < count; i += 1) {
        cards.push({
          id: `${type}-s-${color}-${id++}`,
          kind: "single",
          color,
          label: COLORS[color].name,
          hint: `Go to the next ${COLORS[color].name}!`,
        });
      }
    }

    for (const [color, count] of Object.entries(spec.doubles)) {
      for (let i = 0; i < count; i += 1) {
        cards.push({
          id: `${type}-d-${color}-${id++}`,
          kind: "double",
          color,
          label: `Double ${COLORS[color].name}`,
          hint: `Skip to the second ${COLORS[color].name}!`,
        });
      }
    }

    for (const key of spec.treats) {
      const treat = TREATS[key];
      cards.push({
        id: `${type}-t-${key}-${id++}`,
        kind: "treat",
        treat: key,
        label: treat.name,
        hint: treat.hint,
        art: treat.art,
      });
    }

    return shuffle(cards);
  }

  function shuffle(arr) {
    const a = arr.slice();
    for (let i = a.length - 1; i > 0; i -= 1) {
      const j = Math.floor(Math.random() * (i + 1));
      [a[i], a[j]] = [a[j], a[i]];
    }
    return a;
  }

  function save() {
    try {
      localStorage.setItem(
        STORAGE_KEY,
        JSON.stringify({
          deckType: state.deckType,
          deck: state.deck,
          discard: state.discard,
          current: state.current,
        })
      );
    } catch (_) {
      /* ignore */
    }
  }

  function load() {
    try {
      const raw = localStorage.getItem(STORAGE_KEY);
      if (!raw) return false;
      const data = JSON.parse(raw);
      if (!data || !DECK_SPECS[data.deckType] || !Array.isArray(data.deck)) return false;
      state.deckType = data.deckType;
      state.deck = data.deck;
      state.discard = Array.isArray(data.discard) ? data.discard : [];
      state.current = data.current || null;
      return true;
    } catch (_) {
      return false;
    }
  }

  function totalSize() {
    return state.deck.length + state.discard.length + (state.current ? 1 : 0);
  }

  function showToast(message) {
    els.toast.textContent = message;
    els.toast.classList.add("show");
    clearTimeout(showToast._t);
    showToast._t = setTimeout(() => els.toast.classList.remove("show"), 2200);
  }

  function vibrate(pattern = 12) {
    if (navigator.vibrate) navigator.vibrate(pattern);
  }

  function escapeHtml(str) {
    return String(str)
      .replaceAll("&", "&amp;")
      .replaceAll("<", "&lt;")
      .replaceAll(">", "&gt;")
      .replaceAll('"', "&quot;");
  }

  function renderCard(card) {
    els.playCard.hidden = false;
    els.playCard.className = "play-card visible" + (card.kind === "treat" ? " character" : "");
    els.placeholder.classList.add("hidden");

    if (card.kind === "treat") {
      els.playCard.innerHTML = `
        <div class="treat-art treat-${escapeHtml(card.art)}" aria-hidden="true"><span></span></div>
        <div class="card-label">${escapeHtml(card.label)}</div>
        <div class="card-hint">${escapeHtml(card.hint)}</div>
      `;
      return;
    }

    const hex = COLORS[card.color].hex;
    const blocks =
      card.kind === "double"
        ? `<div class="color-block" style="background:${hex}"></div>
           <div class="color-block" style="background:${hex}"></div>`
        : `<div class="color-block" style="background:${hex}"></div>`;

    els.playCard.innerHTML = `
      <div class="color-blocks">${blocks}</div>
      <div class="card-label">${escapeHtml(card.label)}</div>
      <div class="card-hint">${escapeHtml(card.hint)}</div>
    `;
  }

  function updateChrome() {
    const left = state.deck.length;
    const total = Math.max(totalSize(), 1);
    els.progress.style.transform = `scaleX(${left / total})`;
    els.deckStack.classList.toggle("empty", left === 0);
    els.drawBtn.disabled = left === 0;
    els.drawBtn.textContent = left === 0 ? "EMPTY!" : "DRAW!";
    els.deckSelect.value = state.deckType;
  }

  function drawCard() {
    if (!state.deck.length) {
      playEmptyChime();
      showToast("No cards left — ask a grown-up to Reset.");
      return;
    }

    if (state.current) {
      state.discard.push(state.current);
    }

    const card = state.deck.pop();
    state.current = card;
    vibrate(card.kind === "treat" ? [12, 40, 18] : 12);
    renderCard(card);
    void els.playCard.offsetWidth;
    els.playCard.classList.remove("visible");
    void els.playCard.offsetWidth;
    els.playCard.classList.add("visible");
    updateChrome();
    save();

    if (!state.deck.length) {
      playEmptyChime();
      showToast("Last card! Ask a grown-up to Reset.");
    } else if (card.kind === "treat") {
      playTreatSound();
    } else {
      playDrawChime();
    }
  }

  function resetDeck(announce = true) {
    state.deck = buildDeck(state.deckType);
    state.discard = [];
    state.current = null;
    els.playCard.hidden = true;
    els.playCard.className = "play-card";
    els.playCard.innerHTML = "";
    els.placeholder.classList.remove("hidden");
    els.placeholder.innerHTML = "Tap DRAW!<br />to start";
    updateChrome();
    save();
    if (announce) showToast("Fresh deck — let's play!");
  }

  function changeDeckType(type) {
    if (!DECK_SPECS[type] || type === state.deckType) {
      els.deckSelect.value = state.deckType;
      return;
    }
    const midGame = state.discard.length > 0 || state.current;
    if (midGame) {
      const ok = window.confirm("Switching decks puts all cards back. Continue?");
      if (!ok) {
        els.deckSelect.value = state.deckType;
        return;
      }
    }
    state.deckType = type;
    resetDeck(true);
  }

  function openResetModal() {
    els.modal.classList.add("open");
  }

  function closeResetModal() {
    els.modal.classList.remove("open");
  }

  function init() {
    const revEl = document.getElementById("app-rev");
    if (revEl) revEl.textContent = `Rev. ${APP_REV}`;

    const restored = load();
    if (!restored) {
      resetDeck(false);
      els.placeholder.innerHTML = "Tap DRAW!<br />to start";
    } else {
      updateChrome();
      if (state.current) {
        renderCard(state.current);
      } else {
        els.playCard.hidden = true;
        els.placeholder.classList.remove("hidden");
        els.placeholder.innerHTML =
          state.deck.length === 0
            ? "Deck empty.<br />Ask a grown-up to Reset."
            : "Tap DRAW!<br />for the next card";
      }
    }

    // Prebuild WAV blobs + reusable Audio players on first touch.
    const armAudio = () => {
      armSoundPlayers();
    };
    document.addEventListener("pointerdown", armAudio, { once: true, passive: true });
    document.addEventListener("touchstart", armAudio, { once: true, passive: true });

    els.drawBtn.addEventListener("click", drawCard);
    els.resetBtn.addEventListener("click", openResetModal);
    els.resetCancel.addEventListener("click", closeResetModal);
    els.resetConfirm.addEventListener("click", () => {
      closeResetModal();
      resetDeck(true);
    });
    els.modal.addEventListener("click", (e) => {
      if (e.target === els.modal) closeResetModal();
    });
    els.deckSelect.addEventListener("change", (e) => changeDeckType(e.target.value));

    document.addEventListener("keydown", (e) => {
      if (e.code === "Space" && document.activeElement?.tagName !== "SELECT") {
        e.preventDefault();
        drawCard();
      }
    });

    if ("serviceWorker" in navigator) {
      navigator.serviceWorker.register("./sw.js").catch(() => {});
    }
  }

  init();
})();
