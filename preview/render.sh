#!/bin/sh
# Rendert Screenshots der Vorschau (im Container mit SDL/Xvfb, siehe README).
# Aufruf im Repo-Verzeichnis:  sh preview/render.sh
# Optionen: GIF=1 (animiertes GIF), FPS=false (ohne FPS-Anzeige, für die README)
set -e
OUT=preview/shots
mkdir -p "$OUT"
# Hardware, WLAN und Hintergrundlicht durch SDL ersetzen, Beispielwerte dazu
sed -e 's#^  hardware: !include base/hardware.yml#  hardware: !include preview/sdl.yml\n  demo: !include preview/demo.yml#' \
    -e '/^  network: !include/d' -e '/^  backlight: !include/d' \
    -e 's#name: "energy-dashboard"#name: "energy-dashboard-preview"#' \
    -e "s#^  show_fps: .*#  show_fps: \"${FPS:-true}\"#" main.yml > .preview.yml
esphome compile .preview.yml
BIN=.esphome/build/energy-dashboard-preview/.pioenvs/energy-dashboard-preview/program
Xvfb :99 -screen 0 480x480x24 >/dev/null 2>&1 &
XVFB=$!
export DISPLAY=:99
trap 'kill $XVFB 2>/dev/null || true' EXIT
sleep 1
tap() { xdotool mousemove "$1" "$2" mousedown 1 sleep 0.15 mouseup 1; }
shot() { import -window root "$OUT/$1.png"; }
for SCENE in ${SCENES:-mittag voll abend nacht}; do
  DEMO_SCENE=$SCENE "$BIN" >"$OUT/$SCENE.log" 2>&1 &
  APP=$!
  sleep ${WAIT:-6}
  shot "$SCENE"
  if [ "$SCENE" = mittag ]; then
    tap 240 240
    sleep 1.5
    shot heute
  fi
  kill $APP 2>/dev/null || true
  sleep 0.5
done
# Optional (GIF=1): Startanimation und Fluss als animiertes GIF
if [ -n "$GIF" ]; then
  DEMO_SCENE=${GIF_SCENE:-mittag} "$BIN" >/dev/null 2>&1 &
  APP=$!
  sleep 0.6
  ffmpeg -loglevel error -y -f x11grab -draw_mouse 0 -framerate 25 -video_size 480x480 -i :99 -t 7 "$OUT/demo.mp4"
  kill $APP 2>/dev/null || true
  ffmpeg -loglevel error -y -i "$OUT/demo.mp4" \
    -vf "fps=20,split[a][b];[a]palettegen=max_colors=128:stats_mode=diff[p];[b][p]paletteuse=dither=none:diff_mode=rectangle" \
    "$OUT/demo.gif"
  rm -f "$OUT/demo.mp4"
fi
