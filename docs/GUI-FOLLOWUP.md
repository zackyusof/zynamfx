# Free GUI resizing

The editor window accepts independent width and height changes. The fixed-layout interface scales independently on each axis to fill the editor bounds, so changing the window aspect ratio also changes the content aspect ratio. Minimum dimensions keep the existing layout legible; the selected dimensions are saved with plug-in state and restored when the editor reopens.

The local build helper produces the standalone app and AU component only. It does not enable VST3, AAX, LV2, or CLAP formats. The install option preserves the previous app and user AU bundles as timestamped backups (only the newest backup is kept).

## Build record — 2026-10-12

Standalone and AU Release bundles built and installed on macOS arm64. The AU passed `auval -v aumf ZyFX ZYUS`. Both bundles passed strict ad-hoc signature verification. The generated Release directory contains only `Standalone` and `AU` artifacts.
