# Build channels: development (Gitea) vs public (GitHub)

zynamfx is developed on Gitea and published to GitHub when a release is ready. A
build belongs to exactly one channel, and the channel is baked into the app: the
header logo link and the Settings footer both read it. A development build never
points at GitHub; a public build never points at Gitea.

| Channel | Source of truth | In-app repo link | Flag |
| --- | --- | --- | --- |
| development | `git.zyusof.net/zack/zynamfx` | `https://git.zyusof.net/zack/zynamfx` | (default) |
| public | `github.com/zackyusof/zynamfx` | `https://github.com/zackyusof/zynamfx` | `--public` |

## Building

```sh
# development (Gitea) — the default
python3 script/build-stadium-prototype.py [--install]

# public (GitHub) — the logo and Settings point at github.com/zackyusof/zynamfx
python3 script/build-stadium-prototype.py --public [--install]
```

The channel is passed to CMake as `T3K_FORK_REPO_URL` and
`T3K_FORK_BUILD_CHANNEL` (see `cmake/ForkIdentity.cmake`), which become the
`T3K_REPO_URL` / `T3K_BUILD_CHANNEL` compile definitions consumed by
`plugin/ui/core/BuildIdentity.h`. The Settings version line reads e.g.
`zynamfx v0.0.14 - development build (git.zyusof.net/zack/zynamfx)`.

Do **not** edit the repository URL in source — it is a build input. Publishing is
a clone of the satisfied development state into the public repository with the
public identity (this URL, and the other per-tree identity values such as
`VERSION` and the AU plug-in code); the two repositories have separate histories
and must never be merged.
