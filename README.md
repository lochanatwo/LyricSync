# VocLyrics

VocLyrics is a standalone desktop app that keeps lyrics and a setlist and serves a lyrics-only browser view to devices on the same local network. Choose up to two people in **Shared scroll controllers**; scrolling from either selected controller is synchronized to every connected screen. The browser resources are embedded in the app. Internet access is used only by the GitHub Actions workflow to fetch pinned build dependencies.

## Features

- Stereo input/output is passed through without changing samples. The audio callback only reads JUCE playhead values and writes scalar atomics; networking and model locks stay on other threads.
- Plain lyrics and section tags with simple import/export and manual shared scrolling; timecode controls are hidden from the standalone workflow.
- Multiple songs, selection, duplicate, delete, rename (double-click a row), drag reorder, previous/next, active and next song display. Press **Ctrl+F** on Windows/Linux or **Command+F** on macOS to search song titles and lyric text across the setlist. A lyric hit appears beneath its song title; selecting it highlights the matching words in the editor. Press Enter to open the first match.
- Project state contains setlist, lyrics, song marker metadata and server port.
- Starts an embedded HTTP/WebSocket server on the LAN, searches subsequent ports when a port is busy, embeds the remote web app, and limits connections to eight.
- Viewer assets are served with no-cache headers and a versioned URL so phones cannot mix old HTML with new JavaScript. A low-latency HTTP state fallback keeps lyrics updating when a browser or network blocks WebSockets.
- Per-device output queues replace stale messages. A slow client has an independent sender thread and cannot block the audio callback or other device queues.
- Put a song-key line such as `Key- A#` anywhere in the song. `Key- A# | TRANSPOSED BY: 0` is also accepted. VocLyrics recognizes sharp or flat keys, keeps that metadata out of the performance lyrics, and uses it as the source key for transposition.
- Each remote device independently chooses its target from all 12 chromatic keys: C, C#, D, D#, E, F, F#, G, G#, A, A# and B. The target choice is remembered separately for each song and device, immediately rewrites that device's chord names, and does not change other screens. The desktop host's target key is also remembered per song in the project. If the file key is missing or not recognized, choose the original key with **Source** first; the desktop Source choice is saved in the project and shared with all viewers, while a viewer can set its own Source choice locally.
- The host and each remote viewer choose a band role: Main Singer, Backup Singer, Main Guitar, Rythm Guitar, Drummer, Percussionist, Bass or Keys. Remote roles identify connected screens in the host device list.
- Viewer screens use one large modern chord-sheet card with monospaced, position-aligned chords above five lyric rows and a section badge such as `CHORUS`. The **Up next** panel has been removed so the sheet fills the available screen, and each large arrow moves it by two lyric rows.
- Each viewer has its original key and target-key selector at the top of the chord sheet. The exact signed integer shift is always shown as `TRANSPOSED BY: N`, with no need to open display settings.
- Desktop performance view uses the same chord-sheet layout with five visible chord and lyric rows. Its **Performance view** button always opens and closes the view, and its buttons or keyboard **Up/Down Arrow** keys move exactly two rows.
- Host-controlled automatic scrolling is available in both the editor and performance view. Set a speed from 1 to 60 lyric rows per minute and press **Auto scroll**; every connected display follows the same row, and scrolling stops at the end of the song. Chord-sheet rows glide upward during forward movement and reverse smoothly when moving back.
- **Tap tempo**: while the desktop host has shared-scroll permission, press **Shift+Space** four times on the beat or click **Tap tempo** four times. The first four quarter-note taps set the BPM; one chord/lyric row then advances per four-beat bar for every connected screen. You can keep tapping to refine the BPM. Slow tempos below 50 BPM are supported. **Stop** halts it; **Auto scroll** uses the separate lines-per-minute speed.
- Browser display modes, automatic phone-width chord-sheet fitting, Android fullscreen, iPhone Home Screen fullscreen mode, local clock, proportional text, supported screen wake lock, reconnect, latency estimation and per-device sync nudge are available.
- A ninth device is refused and shown a friendly maximum-device message by the browser view.
- The host publishes one authoritative active lyric row with every state update, so all WebSocket and HTTP-fallback viewers choose the same row instead of calculating it independently.
- The standalone desktop app runs without a DAW; the host chooses a primary controller and an optional second controller from **Desktop host** and the connected devices. Either selected person can scroll and all connected displays follow. Laptop browser viewers can use their keyboard **Up/Down Arrow** keys when selected. Selected phone viewers can drag the chord sheet continuously with one finger; movement is sent while the finger is moving for smooth low-latency shared scrolling. Remote devices use a small visible lyric window to limit page memory use.

Transpose changes the chord names shown on that screen and does not pitch-shift audio. Add a line such as `Key- A#` to identify the original song key, then choose the target key directly. If the file key is absent or was not detected, set **Source** to the actual original key before choosing **Target**. The visible amount uses the exact `TRANSPOSED BY: N` format. The host's per-song choice is stored in the project, while each remote device stores its own target for each song locally.

For chords, put a chord-only line immediately above its lyric line. VocLyrics groups that pair into one display line, for example:

```text
Key- A#
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

Allow VocLyrics through the firewall when prompted. Open the address/QR code shown in the app on the connected devices. In **Shared scroll controllers**, select the first controller and optionally a second person. Then enable **Share scroll** and use the shared-line buttons or click a lyric line on desktop, use the keyboard arrows on a selected laptop, or drag the chord sheet on a selected phone. All connected lyric screens follow either controller.

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
