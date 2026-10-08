# Cairn Music

A design-first, offline music-making application for Cairn Linux.

**Status:** bounded disposable prototype in progress; no maintained
implementation is authorized yet.

Cairn Music aims to be a fun-first creative instrument for children around five
to eight years old. The first release boundary is picture-based composition;
Treble and Grand staff are later possibilities, not prototype requirements.

The repository is intentionally separate from the Cairn Linux operating-system
repository. The child-facing product name and icon remain open design decisions.

Read:

- [`docs/DESIGN.md`](docs/DESIGN.md) — product, interaction, music and technical
  design;
- [`docs/CHILD_TEST_PROTOCOL.md`](docs/CHILD_TEST_PROTOCOL.md) — the
  privacy-preserving observation and clean-data procedure; and
- [Repository decision] — why this is a separate repository and what work is
  currently allowed.

[Repository decision]: docs/decisions/0001-separate-repository-and-prototype-boundary.md

## Current boundary

Design, research and disposable local prototypes are allowed. A maintained
application, package or release requires a separate accepted implementation
decision in both this repository and the main Cairn repository.

Cairn's current login, containment, recovery and real-child Phase 0 work remains
the release priority. This repository is not evidence that Cairn Music ships or
is scheduled for v1.

## Intended licence

Future source code is intended to use Apache-2.0. Documentation and original
brand/art assets are intended to use CC BY-SA 4.0, matching the main Cairn
project. Every dependency, sound, soundfont, icon and contributed recording
requires separate provenance and licence review before distribution.

## Disposable prototype development

The current local prototype uses C++20, Qt 6.8 or newer, QML and Qt Test. It is
research code, not an authorized package or release.

Configure and build with a Qt 6.8-or-newer installation:

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

Run the prototype from the repository root:

```sh
./build/cairn-music
```

The prototype autosaves under Qt's per-user application-data location. It does
not use networking, a file picker, export, plugins or external commands.

Undo history is session-only and retains the 100 most recent successful edits.
Undoing and then making a new edit preserves the earlier undo chain; the new edit
itself becomes the newest undo point. Loading a composition starts a fresh
session history. The prototype does not provide redo.

### Prototype autosave recovery

If the autosave is malformed or uses an unsupported schema, the editor blocks
changes and shows a short recovery message instead of treating the song as
blank. It immediately copies the original bytes beside the autosave as
`prototype-autosave.json.recovery-<id>`. Choosing **Keep it safe and start a new
song** verifies that recovery copy before replacing the invalid autosave with a
fresh composition. A grown-up can recover or inspect the sibling file manually.
If preservation, verification, or the new save fails, the editor remains blocked
and the original autosave is not overwritten. This is a prototype recovery path,
not the maintained product's final Guardian recovery policy.
