# ADR-0002: Fail closed during prototype autosave recovery

**Status:** accepted for the bounded prototype phase
**Date:** 2026-10-07

## Context

The prototype can encounter a malformed autosave or a schema version it cannot
read. Treating that song as blank would make later automatic saves destructive.
The existing prototype therefore blocks editing and preserves the original
bytes, but the design still described an immediately usable blank editor. The
recovery action also gave no visible feedback when copying, verification, or
saving failed.

## Decision

The bounded prototype uses a fail-closed recovery popup. A malformed or
unsupported autosave keeps the editor blocked until an adult chooses to keep it
safe and start a new song. Before replacement, the prototype creates and
verifies a sibling recovery copy.

If preservation, verification, or the fresh save fails, the editor remains
blocked and the original autosave remains untouched. The existing popup shows a
short, calm, translatable failure message without file paths or technical
detail. Retrying runs the real recovery action again; a successful retry clears
the message and closes the popup.

## Consequences

- An unreadable song is not silently destroyed by later editing or autosave.
- Recovery requires an explicit adult action instead of dropping a child into a
  blank composition.
- Failures are visible and accessible without exposing local paths.
- This remains prototype behavior, not the final Guardian recovery policy or a
  commitment to the disposable file format.