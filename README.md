# VocLyrics

VocLyrics is a standalone desktop app that keeps lyrics and a setlist and serves a lyrics-only browser view to devices on the same local network. Choose **Desktop host** or any connected device in **Shared scroll controller**; scrolling from the selected controller is synchronized to every connected screen. The browser resources are embedded in the app. Internet access is used only by the GitHub Actions workflow to fetch pinned build dependencies.

## Features

- Stereo input/output is passed through without changing samples. The audio callback only reads JUCE playhead values and writes scalar atomics; networking and model locks stay on other threads.
- Plain lyrics and section tags with simple import/export and manual shared scrolling; timecode controls are hidden from the standalone workflow.
- Multiple songs, selection, duplicate, delete, rename (double-click a row), drag reorder, previous/next, active and next song display.
- Project state contains setlist, lyrics, song marker metadata and server port.
- Starts an embedded HTTP/WebSocket server on the LAN, searches subsequent ports when a port is busy, embeds the remote web app, and limits connections to eight.
- Per-device output queues replace stale messages. A slow client has an independent sender thread and cannot block the audio callback or other device queues.
- Each remote device can independently select an integer semitone shift from -12 to +12. Its screen shows `TRANSPOSED BY: N` and immediately rewrites the displayed chord names for that device; the setting stays on that device and does not change other screens. The desktop host has its own transpose selector, chord rendering and status.
- The host and each remote viewer choose a band role: Main Singer, Backup Singer, Main Guitar, Rythm Guitar, Drummer, Percussionist, Bass or Keys. Remote roles identify connected screens in the host device list.
- Viewer screens use a modern light chord-sheet card with monospaced, position-aligned chord rows above exactly two lyric rows, a section badge such as `CHORUS`, and two additional lines visible in **Up next**. Large arrow buttons provide smooth shared navigation for the selected controller.
- Browser display modes, automatic phone-width chord-sheet fitting, fullscreen, local clock, proportional text, supported screen wake lock, reconnect, latency estimation and per-device sync nudge are available.
- A ninth device is refused and shown a friendly maximum-device message by the browser view.
- The host editor switches to a large lyrics display while the DAW transport is playing.
- The standalone desktop app runs without a DAW; the host chooses **Desktop host** or any connected device as the single shared-scroll controller. Only that selected device can scroll, and all connected displays follow. Remote devices use a small visible lyric window to limit page memory use.

Transpose changes the chord names shown on that screen and does not pitch-shift audio. Each screen can select an integer shift from -12 to +12 semitones, and the visible value uses the exact `TRANSPOSED BY: N` format. The host's transpose choice is stored in the project, while each remote device stores its own choice locally.

For chords, put a chord-only line immediately above its lyric line. VocLyrics groups that pair into one display line, for example:

```text
[CHORUS]
A                         E
Ratakin ehaa agilee, thaniwenna bea
                         A
Sithakin nowe kisida, haravanne maa
```

The host plug-in API exposes playhead position to effects, but JUCE does not provide a portable command for a plug-in to seek the host. Selecting a setlist row selects the song; hosts that expose a supported marker-jump integration can be connected later. `Song::startSeconds` is retained in project state for that integration.

## Build and install from GitHub Actions

1. Create a new GitHub repository. Unzip this project and upload the files at the ZIP root to the repository root (including `.github/workflows/build.yml`).
2. Open the repository's **Actions** tab. Select **Build** and choose **Run workflow**, or push a commit to start it automatically.
3. When the job finishes, open the workflow run and download `VocLyrics-Standalone-Windows` or `VocLyrics-Standalone-macOS-universal` from **Artifacts**.
4. Unzip the download and launch `VocLyrics.exe` on Windows or `VocLyrics.app` on macOS. If macOS blocks it, run `xattr -dr com.apple.quarantine /path/to/VocLyrics.app` in Terminal.
5. Allow VocLyrics through the operating-system firewall on first run. Phones and tablets must be on the same Wi-Fi/LAN as the host. Open the address or QR code shown in the app on the connected devices.

Allow VocLyrics through the firewall when prompted. Open the address/QR code shown in the app on the connected devices. In **Shared scroll controller**, select **Desktop host** or one of the connected devices; then enable **Share scroll** and use the shared-line buttons or click a lyric line on desktop, or swipe/scroll on the selected remote. All connected lyric screens follow that controller.

The workflow uses JUCE 9.0.3, CivetWeb v1.16 at a pinned upstream commit containing its URI parser security fix, and Nayuki QR Code generator v1.8.0, pinned with CMake FetchContent. Windows builds use the Visual Studio toolchain on `windows-latest`; macOS builds are universal arm64 and x86_64 on `macos-latest`. The workflow runs the parser, tagging, interpolation, and eight-client WebSocket tests before packaging the standalone app for each OS.

## Project layout

```text
CMakeLists.txt
Source/
  PluginProcessor.*  PluginEditor.*
  Lyrics/            LRC parser, song/setlist models, live tagger
  Network/           LAN server, address helpers, QR, sync clock
  UI/                Setlist, lyric editor, display and remote panels
WebRemote/           Embedded browser client
Tests/               Parser, tagging, sync and WebSocket tests
.github/workflows/   Windows and universal macOS builds
```

## If an Actions build fails

Copy the failing job's full error log (including the first compiler or CMake error) into the VocLyrics chat. The source can then be corrected against the actual Windows or macOS toolchain diagnostic, and the workflow can be rerun until it is green.
