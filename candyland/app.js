(() => {
  "use strict";

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

  let audioCtx = null;
  let audioReady = false;

  function getAudioContext() {
    const Ctx = window.AudioContext || window.webkitAudioContext;
    if (!Ctx) return null;
    if (!audioCtx) audioCtx = new Ctx();
    return audioCtx;
  }

  /** iPhone Safari needs resume() + a real buffer kick during a user gesture. */
  async function unlockAudio() {
    const ctx = getAudioContext();
    if (!ctx) return null;
    try {
      if (ctx.state === "suspended") {
        await ctx.resume();
      }
      if (!audioReady) {
        const buffer = ctx.createBuffer(1, 1, 22050);
        const source = ctx.createBufferSource();
        source.buffer = buffer;
        source.connect(ctx.destination);
        source.start(0);
        audioReady = true;
      }
    } catch (_) {
      /* ignore unlock failures; tone() may still work later */
    }
    return ctx;
  }

  function tone(ctx, freq, start, dur, type, gainValue) {
    if (!ctx) return;
    const t0 = ctx.currentTime + start;
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = type;
    osc.frequency.setValueAtTime(freq, t0);
    gain.gain.setValueAtTime(0, t0);
    gain.gain.linearRampToValueAtTime(gainValue, t0 + 0.02);
    gain.gain.linearRampToValueAtTime(gainValue * 0.55, t0 + dur * 0.45);
    gain.gain.linearRampToValueAtTime(0, t0 + dur);
    osc.connect(gain);
    gain.connect(ctx.destination);
    osc.start(t0);
    osc.stop(t0 + dur + 0.05);
  }

  /** Clear bell chime for a normal draw. */
  function playDrawChime(ctx) {
    tone(ctx, 1046.5, 0, 0.45, "sine", 0.45);
    tone(ctx, 1568, 0.06, 0.5, "sine", 0.28);
    tone(ctx, 2093, 0.1, 0.35, "sine", 0.12);
  }

  /** Two chimes when the deck runs out. */
  function playEmptyChime(ctx) {
    tone(ctx, 880, 0, 0.35, "sine", 0.42);
    tone(ctx, 1318.5, 0.07, 0.4, "sine", 0.26);
    tone(ctx, 880, 0.42, 0.35, "sine", 0.42);
    tone(ctx, 1318.5, 0.5, 0.45, "sine", 0.26);
  }

  /** Playful sparkle for special/picture cards. */
  function playTreatSound(ctx) {
    tone(ctx, 523.25, 0, 0.22, "triangle", 0.35);
    tone(ctx, 659.25, 0.1, 0.22, "triangle", 0.35);
    tone(ctx, 783.99, 0.2, 0.24, "triangle", 0.35);
    tone(ctx, 1046.5, 0.32, 0.5, "sine", 0.4);
    tone(ctx, 1568, 0.4, 0.45, "sine", 0.22);
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

  async function drawCard() {
    const ctx = await unlockAudio();

    if (!state.deck.length) {
      playEmptyChime(ctx);
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
      playEmptyChime(ctx);
      showToast("Last card! Ask a grown-up to Reset.");
    } else if (card.kind === "treat") {
      playTreatSound(ctx);
    } else {
      playDrawChime(ctx);
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

    // Warm up audio on first touch (helps iPhone home-screen / Safari).
    const armAudio = () => {
      unlockAudio();
    };
    document.addEventListener("pointerdown", armAudio, { once: true, passive: true });
    document.addEventListener("touchstart", armAudio, { once: true, passive: true });

    els.drawBtn.addEventListener("click", () => {
      drawCard();
    });
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
