# ADR-0001: Separate repository and bounded prototype phase

**Status:** accepted for the bounded prototype phase
**Date:** 2026-10-01
**Accepted:** 2026-10-02 by Mason's direct authorization to begin initial
development

## Context

Cairn Linux's accepted ADR-0009 says no first-party make-app ships in v1 and
that existing maintained software should be surveyed first. The survey has not
yet found an offline, packaged music composer that fits both a pre-reader and a
child progressing into real notation.

The maintainer wants to define an original, fun-first Cairn music application
for children approximately five to eight years old, beginning with
picture-based composition and potentially progressing through Treble to Grand
staff in later releases. This product has its own document
model, audio engine, assets, tests, packaging and release lifecycle. Putting it
inside the operating-system repository would couple two different products and
make the OS release carry application-development history.

Design work must not silently turn into a second maintained product while
Cairn's current safety and real-child release gates remain open.

## Decision

Cairn Music lives in its own `Cairn-Linux/cairn-music` repository within the
Cairn Linux GitHub organization.

This ADR authorizes only:

- product and interaction design;
- licence, dependency and prior-art research;
- disposable, local-only interaction, audio and data-model prototypes; and
- small research sessions conducted under the consent and privacy rules in the
  design proposal.

It does not authorize a maintained application, package, release workflow or
public availability claim. Every delivery slice beyond the disposable
prototype is blocked until a follow-up implementation ADR is accepted here and
the main Cairn repository explicitly amends ADR-0009 and its roadmap.

## Consequences

- The app can have a focused history and release lifecycle without bloating the
  operating-system repository.
- Cairn can later consume a tagged package rather than vendoring application
  source into the image.
- Brand tokens and OS trust boundaries remain upstream contracts rather than
  copied authority.
- The repository may exist before the product does; public communication must
  preserve that distinction.
- A second repository adds maintenance, packaging, CI and release work if a
  maintained implementation is later accepted.
