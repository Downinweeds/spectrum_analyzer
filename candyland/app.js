(() => {
  "use strict";

  const COLORS = {
    red: { name: "Red", hex: "#e84855" },
    purple: { name: "Purple", hex: "#7b61ff" },
    yellow: { name: "Yellow", hex: "#ffd166" },
    blue: { name: "Blue", hex: "#4cc9f0" },
    orange: { name: "Orange", hex: "#ff9f1c" },
    green: { name: "Green", hex: "#2ec4b6" },
  };

  const TREATS = {
    peppermint: {
      name: "Peppermint Forest",
      hint: "Move to the Peppermint Forest space.",
      art: "peppermint",
    },
    peanut: {
      name: "Peanut Brittle House",
      hint: "Move to the Peanut Brittle House space.",
      art: "peanut",
    },
    lollipop: {
      name: "Lollipop Woods",
      hint: "Move to the Lollipop Woods space.",
      art: "lollipop",
    },
    icecream: {
      name: "Ice Cream Sea",
      hint: "Move to the Ice Cream Sea space.",
      art: "icecream",
    },
    gumdrop: {
      name: "Gumdrop Mountains",
      hint: "Move to the Gumdrop Mountains space.",
      art: "gumdrop",
    },
    gingerbread: {
      name: "Gingerbread Tree",
      hint: "Move to the Gingerbread Tree space.",
      art: "gingerbread",
    },
    candyhearts: {
      name: "Candy Hearts",
      hint: "Move to the Candy Hearts space.",
      art: "candyhearts",
    },
  };

  /** Classic / modern / vintage anniversary-style decks. */
  const DECK_SPECS = {
    classic64: {
      label: "Classic (64)",
      singles: { red: 8, purple: 8, yellow: 8, blue: 8, orange: 7, green: 7 },
      doubles: { red: 2, purple: 2, yellow: 2, blue: 2, orange: 2, green: 2 },
      treats: ["peppermint", "peanut", "lollipop", "icecream", "gumdrop", "gingerbread"],
    },
    vintage64: {
      label: "Vintage (64)",
      // Anniversary-style boards include Candy Hearts as a 7th picture space.
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

  const STORAGE_KEY = "candyland-drawer-v1";

  const els = {
    remaining: document.getElementById("remaining"),
    drawnCount: document.getElementById("drawn-count"),
    progress: document.getElementById("progress-bar"),
    deckStack: document.getElementById("deck-stack"),
    placeholder: document.getElementById("placeholder"),
    playCard: document.getElementById("play-card"),
    drawBtn: document.getElementById("draw-btn"),
    resetBtn: document.getElementById("reset-btn"),
    deckSelect: document.getElementById("deck-select"),
    history: document.getElementById("history"),
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
          hint: `Move to the next ${COLORS[color].name.toLowerCase()} space.`,
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
          hint: `Move to the second ${COLORS[color].name.toLowerCase()} space.`,
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
      /* ignore quota / private mode */
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

  function vibrate() {
    if (navigator.vibrate) navigator.vibrate(12);
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

  function escapeHtml(str) {
    return String(str)
      .replaceAll("&", "&amp;")
      .replaceAll("<", "&lt;")
      .replaceAll(">", "&gt;")
      .replaceAll('"', "&quot;");
  }

  function renderHistory() {
    const recent = [...state.discard, ...(state.current ? [state.current] : [])]
      .slice(-12)
      .reverse();
    if (!recent.length) {
      els.history.innerHTML = `<span class="chip">No cards drawn yet</span>`;
      return;
    }
    els.history.innerHTML = recent
      .map((card) => {
        if (card.kind === "treat") {
          return `<span class="chip"><span class="swatch treat" style="background:#ff5c8a"></span>${escapeHtml(card.label)}</span>`;
        }
        const hex = COLORS[card.color].hex;
        const cls = card.kind === "double" ? "swatch double" : "swatch";
        return `<span class="chip"><span class="${cls}" style="background:${hex};color:${hex}"></span>${escapeHtml(card.label)}</span>`;
      })
      .join("");
  }

  function updateChrome() {
    const left = state.deck.length;
    const drawn = state.discard.length + (state.current ? 1 : 0);
    const total = Math.max(totalSize(), 1);
    els.remaining.textContent = String(left);
    els.drawnCount.textContent = String(drawn);
    els.progress.style.transform = `scaleX(${left / total})`;
    els.deckStack.classList.toggle("empty", left === 0);
    els.drawBtn.disabled = left === 0;
    els.drawBtn.textContent = left === 0 ? "Deck Empty" : "Draw Card";
    els.deckSelect.value = state.deckType;
    renderHistory();
  }

  function drawCard() {
    if (!state.deck.length) {
      showToast("Deck is empty — tap Reset Deck.");
      return;
    }

    if (state.current) {
      state.discard.push(state.current);
    }

    const card = state.deck.pop();
    state.current = card;
    vibrate();
    renderCard(card);
    // restart animation
    void els.playCard.offsetWidth;
    els.playCard.classList.remove("visible");
    void els.playCard.offsetWidth;
    els.playCard.classList.add("visible");
    updateChrome();
    save();

    if (!state.deck.length) {
      showToast("That was the last card. Reset when you’re ready.");
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
    els.placeholder.innerHTML = "Deck shuffled.<br />Draw a card to start.";
    updateChrome();
    save();
    if (announce) showToast("Fresh deck — good luck!");
  }

  function changeDeckType(type) {
    if (!DECK_SPECS[type] || type === state.deckType) {
      els.deckSelect.value = state.deckType;
      return;
    }
    const midGame = state.discard.length > 0 || state.current;
    if (midGame) {
      const ok = window.confirm(
        "Switching deck size reshuffles and puts every card back. Continue?"
      );
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
      els.placeholder.innerHTML = "Deck ready.<br />Draw a card to start.";
    } else {
      updateChrome();
      if (state.current) {
        renderCard(state.current);
      } else {
        els.playCard.hidden = true;
        els.placeholder.classList.remove("hidden");
        els.placeholder.innerHTML =
          state.deck.length === 0
            ? "Deck empty.<br />Tap Reset Deck to shuffle again."
            : "Welcome back.<br />Draw the next card.";
      }
    }

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
