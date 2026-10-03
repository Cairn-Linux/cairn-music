# AGENTS.md — working in Cairn Music

Cairn Music is a proposed offline music-making application for children. It is
part of the Cairn Linux project but lives in its own repository.

## Source of truth

- `docs/DESIGN.md` is the product and technical proposal.
- `docs/decisions/` records accepted and proposed decisions.
- The main Cairn repository remains authoritative for operating-system levels,
  containment, brand tokens, accessibility scope and release phases.

Do not silently contradict the main Cairn design. Record any deliberate change
in an ADR in both repositories when it crosses their boundary.

## Current authorization boundary

Only documentation, research and disposable local prototypes are authorized.
Do not create a maintained implementation, package, release workflow or public
product claim until a follow-up implementation ADR is accepted.

## Expected implementation direction

If implementation is authorized:

- C++20, Qt 6 and QML;
- QML draws; C++ owns composition, editing, playback and persistence;
- no network access, accounts, telemetry, ads or cloud dependency;
- no plugins, arbitrary command launch or unrestricted file access;
- every user-facing string is translatable;
- every interaction has keyboard and semantic accessibility behavior;
- logic is covered by Qt Test from the first maintained commit;
- dependencies and sound assets require explicit licence/provenance review.

## Originality boundary

Do not copy source, samples, characters, symbols, animation, tutorial text,
screen composition or proprietary formats from Mario Paint or a fan clone.
Public source is not automatically compatible or free of third-party rights.
Study general outcomes and interaction principles, then document the Cairn
requirement in original language before implementation.

## Contribution and sign-off

Future source code is intended to use Apache-2.0. Documentation and original
art are intended to use CC BY-SA 4.0. A human must review and sign off every
contribution under the Developer Certificate of Origin. An AI agent never adds
a human `Signed-off-by` trailer.
