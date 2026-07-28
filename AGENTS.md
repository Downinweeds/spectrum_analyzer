# AGENTS.md

## Cursor Cloud specific instructions

### What this is
A zero-dependency, fully client-side **real-time audio Spectrum Analyzer**. The entire app is a single static file: `index.html` (vanilla JS + Web Audio API + Canvas). There is no backend, database, package manager, build step, or lint/test tooling.

### Run it (development)
Serve the file over HTTP from the repo root and open it in a browser:

```bash
python3 -m http.server 8000
# then open http://localhost:8000/index.html
```

`python3` is pre-installed. Any static server works; `localhost`/HTTPS is required because `getUserMedia` needs a secure context (opening via `file://` may block the microphone).

### Non-obvious gotchas
- **Audio only starts on a user gesture.** The canvas is blank until you click/tap the page (browser autoplay policy). `start()` is bound to a one-time `click`/`touchstart`.
- **Microphone required.** The app calls `navigator.mediaDevices.getUserMedia({ audio: true })`. With no audio input the bars sit at the noise floor.
- **Testing without a real mic:** launch Chrome with fake-media flags so the permission prompt is auto-accepted and a ~1 kHz test tone is fed in, which makes the bars/waveform respond:
  ```bash
  google-chrome --use-fake-ui-for-media-stream --use-fake-device-for-media-stream \
    "http://localhost:8000/index.html"
  ```
  (In this headless VM, `dbus`/`gpu` errors from Chrome are harmless.)

### Lint / test / build
None exist. There is no linter, test suite, or build; the app runs directly from `index.html`.
