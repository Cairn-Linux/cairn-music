# Cairn Music design

**Status:** proposal 0.3; bounded disposable prototype authorized
**Repository:** `Cairn-Linux/cairn-music`
**Working name:** Cairn Music; final child-facing name remains open
**Relationship to Cairn:** separate application in the Cairn Linux GitHub
organization; not a Cairn v1 release requirement

## 1. Product promise

Cairn Music is a fun-first creative instrument for children approximately five
to eight years old. A child can place playful sound pictures and hear something
recognizably musical in the first minute without reading notation.

It is not a lesson, quiz, reward system, simplified digital audio workstation,
or professional notation editor. Learning can emerge naturally as children
explore pitch, time, duration, and later notation. Making music remains the
point.

The first maintained release, if later authorized, contains only the Pictures
composer. Treble and Grand staff are later releases, in that order. Showing a
staff does not by itself establish a high educational ceiling.

## 2. Project and authorization boundary

Cairn Music lives in its own repository inside the Cairn Linux GitHub
organization. Cairn may later consume a tagged package; the operating-system
repository does not vendor this source.

ADR-0001 authorizes design, research, and disposable local prototypes. Mason's
2026-10-02 instruction authorized starting the first such prototype. It does
not authorize a maintained application, package, release workflow, deployment,
public availability claim, or a change to Cairn's v1 scope.

Cairn's login, containment, recovery, and real-child Phase 0 work remains the
operating-system release priority. A follow-up implementation ADR is required
before prototype code becomes a maintained product.

## 3. Product principles

### 3.1 Fun before instruction

The first action makes sound. The application has no grades, streaks,
achievements, unlock economy, required lessons, applause machinery, or mascot
praise. Completing and hearing a song is the reward.

### 3.2 Make before theory

A child does not need to understand a staff, key signature, file picker, or
music-theory explanation before placing a note. Real musical terms appear
beside interactions the child already understands.

### 3.3 Honest musical structure

Use **note**, **beat**, **measure**, **tempo**, **instrument**, **treble**, and
**bass** when those concepts appear. Do not invent substitute vocabulary that
must later be unlearned.

Pictures, Treble, and Grand staff eventually present the same canonical
composition. A view never silently drops, respells, retimes, or misrepresents
an event. Unsupported content blocks editing in that view with a useful
explanation.

### 3.4 Immediate feedback

Selecting a sound previews it once. Placing, moving, or changing a note
previews the result. Whole-song playback is always easy to reach.

### 3.5 Offline, private, and calm

There are no accounts, network requests, telemetry, advertisements, cloud
libraries, generative services, or manipulative engagement loops. Projects
remain within the child's local profile unless a later Guardian-mediated flow
is explicitly designed.

Motion is brief and functional. No constant animation competes with composing.

## 4. Audience and validation

The anchor audience is ages five to eight, including pre-readers and children
without prior music training. Simplicity wins when it conflicts with advanced
capability, provided the application does not teach a false musical model.

The first prototype testers are:

- **Zelda**, almost five, taking basic music lessons through Let's Play Music;
- **Shepard**, almost seven, taking piano through Let's Play Music and playing
  with both hands.

Zelda is the discoverability gate. Shepard tests whether editing and extending
a song remains interesting after the first interaction.

Evidence from these sessions validates only this prototype with these children.
It does not establish universal usability, accessibility, or learning claims.

## 5. Delivery boundaries

### 5.1 Disposable prototype

The authorized prototype is local, disposable, and evidence-seeking:

- Pictures only;
- mouse and touchpad primary input;
- seven natural-note pitch rows;
- four pitched sound tokens and two percussion tokens;
- two measures initially, expandable to eight;
- 4/4 meter;
- place, preview, erase, undo, play, stop, and whole-song loop;
- local autosave and automatic reopen;
- no app tutorial, only a subtle inactivity pulse; and
- enough visual and sonic polish to judge enjoyment rather than a grey-box
  wireframe.

Prototype code is not presumed to survive into maintained implementation.

### 5.2 First maintained release

If separately authorized, the first release contains:

- Pictures only;
- seven natural-note rows by default;
- an optional **More notes** control for five accidental rows;
- one visible octave at a time;
- 4/4 meter and whole-beat placement by default;
- an optional **Split beat** control exposing eighth-note positions;
- **Short**, **1 beat**, **2 beats**, and **4 beats** durations;
- no note crossing a measure boundary;
- two measures initially, expandable to sixteen;
- eight pitched tokens: four familiar instruments and four original playful
  object or animal sounds;
- four percussion sounds in a separate lane;
- at most three pitched sounds and two percussion hits sounding at once;
- Slow, Steady, and Fast tempo choices;
- place, select, move, change duration, change sound, erase, undo, redo, play,
  stop, and whole-song loop;
- autosaved visual song cards with optional titles; and
- confirmed deletion, immediate undo, and a later Guardian recovery path.

### 5.3 Later releases

Later work may add, in order:

1. a synchronized Pictures-to-Treble bridge;
2. Treble notation;
3. Grand staff; and
4. separately justified composition or interchange capabilities.

Treble and Grand staff are not first-release acceptance criteria.

### 5.4 Explicit first-release exclusions

The first release excludes:

- bass or grand-staff notation;
- meters other than 4/4;
- cross-measure ties, dots, tuplets, pickups, or independent voices;
- dynamics, articulation, pedal, and child-editable note loudness;
- microphone recording, arbitrary audio import, or custom sounds;
- MIDI input, MIDI export, WAV export, MusicXML export, and printing;
- sibling-to-sibling sharing or a shared family library;
- measure duplication, copy/repeat tools, or arbitrary loop ranges;
- plugins, external editors, arbitrary commands, or URLs;
- generative composition; and
- Cairn creations-gallery integration.

## 6. First-minute experience

1. The child opens the app into a blank two-measure Pictures song.
2. Four large pitched sound tokens and two drum tokens are visible.
3. Selecting a token previews it once and leaves that stamp selected.
4. Clicking a pitch/beat position places the token and immediately plays it.
5. A large **Play** control starts the whole song once.
6. An obvious **Loop** toggle repeats the whole song when enabled.
7. A placed note can be selected, moved, erased, or resized using visible
   pointer controls.
8. **Undo** remains visible.
9. The song autosaves without a filename or Save dialog.
10. After brief inactivity, one available action may pulse subtly.

There is no tutorial carousel, directive overlay, mascot instruction, setup
dialog, file picker, or required reading. A separate future Cairn-wide system
orientation application is outside this project's scope.

After first launch the app reopens the last song. Library and **New song** are
one click away.

## 7. Pictures composition

### 7.1 Pitch surface

Time runs left to right. Pitch runs low to high. The default surface shows one
octave's seven natural notes. It is an explicitly diatonic ladder, not fake
piano-key geometry and not a claim that the collection establishes a major or
minor tonic.

**More notes** is visible from the first day as a secondary control. Enabling it
smoothly inserts five smaller accidental rows between the natural rows. The
transition preserves every event. Pictures uses neutral in-between-note symbols
rather than requiring a pre-reader to parse enharmonic labels.

A later octave control changes the visible editing octave. It does not transpose
the song. Off-screen events remain intact and receive clear edge indicators.

An optional **Show note names** control is available at any time.

### 7.2 Time and duration

The meter is 4/4. Whole beats are the default targets. **Split beat** reveals
eighth-note positions for the selected measure.

Duration choices are:

- **Short:** one eighth-note subdivision;
- **1 beat:** one quarter note;
- **2 beats:** one half note; and
- **4 beats:** one whole note.

A token grows a clear horizontal tail across its duration. Only durations that
fit before the current measure's bar line are offered. The app does not clip a
choice silently or create ties in Pictures.

A new project contains two measures. **Add measure** is explicit and disappears
or disables at the applicable eight- or sixteen-measure cap.

### 7.3 Sound-token model

The child selects a sound stamp. Every placed pitched event stores its own
sound-token identifier. Changing the selected palette item never changes
existing events. The first release does not expose tracks or track-level
instrument controls.

Placing another event in an occupied pitch/time cell replaces it through an
undoable operation. Pitched events may overlap subject to the limit of three
sounding simultaneously. An edit that would exceed the limit is rejected with
clear visual feedback; undocumented voice stealing is forbidden.

### 7.4 Percussion

Percussion lives in a separate lane beneath the pitch grid. Its vertical rows
select kit pieces rather than high or low pitch. Percussion events store an
unpitched sound identifier and are never transposed with pitched notes.

At most two percussion events may sound simultaneously. An edit that exceeds
the cap is rejected deterministically.

### 7.5 Editing and playback

The selected stamp remains active for repeated placement. Selecting an event
reveals direct controls for move, duration, sound, and erase. Dragging may be a
shortcut but is not the only way to resize or move an event.

Every edit is undoable during the current session. Whole-song playback runs
once by default. The sole first-release loop behavior repeats the entire song.
There are no measure or arbitrary selection loops.

Slow, Steady, and Fast are the only first-release tempo choices. Their exact BPM
values remain an audio/user-test decision.

## 8. Sound and visual identity

The first release contains eight pitched sounds:

- four recognizable musical instruments; and
- four original playful object or animal sounds.

The prototype contains two of each. It also contains two of the eventual four
percussion sounds.

A picture corresponds honestly to what is heard: a bell token sounds like a
bell, and an animal token makes an original pitched animal-like sound. Each
sounding token may make one brief, non-blocking motion when placed or reached by
the playhead.

The app may use a broader and brighter game-like palette than Cairn's shell.
Shape, silhouette, position, text, and motion must still communicate meaning
without relying on color alone. The workspace must preserve contrast, visible
selection, reduced-motion behavior, and large pointer targets.

Small discoverable Easter eggs are allowed when they neither interrupt music
making nor form a reward economy.

No token, sound mapping, character, animation, layout, sample, or trade dress
may copy Nintendo, Mario Paint, or a fan clone.

## 9. Progressive disclosure and Cairn levels

Cairn levels describe trust and independence, not musical skill. They establish
first-use defaults only and never lock musical capabilities.

- **Level 1:** natural-note Pictures starts by default. More notes remains
  available. Once Treble ships, it is accessible through a secondary View
  control and may be suggested after three saved songs across at least three
  sessions.
- **Level 2:** Pictures remains the starting creative surface, More notes starts
  enabled, and Treble is visibly available once shipped.

A Treble suggestion is rare, dismissible, and includes **Don't show again**.
Opening Treble early is never treated as cheating or bypassing a level.

A generic Linux build with no trusted Cairn level asks once between **Simple**
and **More**. Those choices map to the Level 1- and Level 2-style defaults
without exposing Cairn account terminology.

After first use, child and project preferences win. A later Cairn account-level
change does not overwrite choices already made inside Music.

## 10. Later notation views

### 10.1 Pictures-to-Treble bridge

Switching views alone is not instruction. The later bridge uses a synchronized
split view. Selecting a picture event highlights the corresponding staff note;
a brief transition may show one selected event moving between representations.

### 10.2 Treble

Treble uses standard note heads, stems, rests, and bar lines for its declared
subset. A selected note may show its letter name and corresponding keyboard key.
Picture tokens may remain as small sound badges but never replace notation.

Treble is editable only when it can faithfully represent the document.
Unsupported content remains intact and directs the child back to Pictures.
Accidentals default to sharp spelling, with an explicit enharmonic alternative
when that capability is implemented.

### 10.3 Grand staff

Grand staff is later than Treble. Treble and bass clefs form one staff system,
with Middle C visible between them. Staff assignment and suggested hand remain
separate: bass does not automatically mean left hand, nor treble right hand.

Grand staff does not by itself authorize larger voices, advanced rhythm,
printing, MIDI performance, or a professional notation claim.

## 11. Projects, autosave, and deletion

### 11.1 Child-facing behavior

Songs autosave and appear as visual cards with generated thumbnails. Titles are
optional. No filename, explicit Save action, or filesystem navigation is
required.

Selecting a card reopens that song. The prototype may use one autosave card
while testing the interaction, but its format must support safe automatic
reopen.

Deleting a song card:

1. asks for confirmation;
2. offers an immediate child-facing Undo; and
3. retains a Guardian-accessible recovery path after ordinary Undo expires.

The exact recovery retention and permanent-removal policy must be decided before
maintained implementation. Note erasure remains ordinary undoable editing and
does not invoke Guardian recovery.

### 11.2 Prototype persistence

The prototype stores bounded, versioned JSON under `QStandardPaths` using
`QSaveFile`. It automatically loads on startup. A malformed or unsupported file
leaves the editor usable with a new composition and is not silently overwritten.

The prototype format is disposable and is not the product's schema v1.

### 11.3 Maintained persistence requirements

A maintained implementation requires:

- an accepted versioned project schema;
- atomic save and checked commit results;
- bounded file, string, event, measure, pitch, tempo, and nesting limits;
- generation/recovery behavior for interrupted writes;
- safe handling of concurrent instances;
- migration tests; and
- confinement to the child's configured project root.

Unsupported major schemas open safely and are never destructively rewritten.

## 12. Accessibility and input

Mouse and touchpad are the primary prototype and first-release inputs. All
required composition actions need visible pointer controls and targets sized for
children.

The app still provides semantic roles, names, descriptions, selected state,
change events, logical focus order, high contrast, reduced motion, scalable
text, and non-color cues from the beginning. Those foundations must not be
confused with a claim of complete sequential-keyboard or nonvisual score
editing.

Keyboard completion, switch-input support, and nonvisual composition require
separate requirements and testing before public accessibility claims are made.

## 13. Technical direction

The maintained direction remains C++20, Qt 6, and QML:

- QML draws and forwards intent.
- C++ owns musical validation, composition state, editing, playback, and
  persistence.
- Musical rules and JSON construction do not live in QML JavaScript.
- Core logic is independently testable with Qt Test.
- User-facing strings use `tr()` and `qsTr()`.

The disposable prototype may use a smaller architecture while preserving this
boundary. It may procedurally synthesize temporary sounds to avoid premature
asset-licensing decisions.

Qt Multimedia remains a separately measured build/runtime dependency. FluidSynth,
soundfonts, sample libraries, and other dependencies each require their own
footprint, latency, maintenance, and licence decision.

### 13.1 Audio prototype

The initial prototype may pre-render its bounded composition to PCM and stream
it through `QAudioSink`. This avoids inventing a production real-time engine
before the interaction is validated.

The renderer must:

- use each event's own sound identifier;
- map onset and duration deterministically;
- mix in a wider accumulator and clamp safely;
- render reproducible procedural percussion;
- operate without a physical audio device in automated tests; and
- leave editing and persistence usable if audio initialization fails.

Warm and cold preview latency must be measured before a maintained audio
architecture is accepted.

## 14. Test strategy

### 14.1 Prototype model

Automated tests cover:

- two initial measures and the eight-measure cap;
- valid and invalid pitched/percussion placement;
- per-event sound identity after palette changes;
- occupied-cell replacement;
- separate pitched and percussion semantics;
- three-pitched/two-percussion concurrency limits;
- erase and undo behavior;
- deterministic serialization and round trip;
- failed-load atomicity; and
- autosave and automatic reopen.

### 14.2 Audio

Tests cover:

- bounded, non-silent output for each temporary sound;
- distinguishable pitched timbres;
- pitch rising with grid row;
- deterministic percussion;
- exact onset/duration mapping;
- safe simultaneous mixing; and
- play, stop, end, and whole-song loop state through a fake output boundary.

### 14.3 QML and manual acceptance

An offscreen smoke test loads the QML surface and verifies the expected rows,
measures, and controls. Manual acceptance verifies:

- place and hear all prototype sounds;
- palette changes do not alter existing notes;
- extend from two to eight measures;
- erase and undo;
- play, stop, and loop;
- close and reopen with work restored; and
- edit and save successfully with no audio device.

## 15. Child research gate

Before each child session:

- Guardian consent and child assent are explicit;
- the child may stop at any time;
- no AI agent interacts with the child unsupervised;
- direct observation and brief non-identifying notes are preferred; and
- audio or video is not recorded by default.

First-note and first-playback time remain useful diagnostics, but they do not
pass the prototype.

The prototype advances only if:

- Zelda and Shepard each use it;
- each voluntarily returns on two later occasions;
- Zelda independently creates, plays, saves, and reopens a song;
- Shepard also extends and edits a song; and
- an adult prompt does not manufacture the return behavior.

A scheduled mandatory session is not a voluntary return. Adult reviewers may
find defects but cannot pass this child-engagement gate.

## 16. Originality and licensing

Cairn may study general outcomes such as direct placement and immediate audible
feedback. It must not copy source, samples, characters, icons, animation,
tutorial text, distinctive screen composition, sound mappings, or proprietary
formats from Mario Paint or a fan clone.

Every distributed asset requires a manifest recording origin, author,
acquisition date, immutable hash, complete license, attribution, modification
history, redistribution permission, and any obligation affecting child-created
renders.

For Cairn-created audio, record who performed or synthesized it, the source
instrument or patch, processing history, and confirmation that no restricted
sample, commercial library, copied game audio, or unapproved generated audio was
incorporated.

Prototype procedural sounds are temporary evidence tools, not approved release
assets.

## 17. Remaining decisions

These decisions are intentionally deferred to spikes or later ADRs:

1. exact identities of the familiar and playful sounds;
2. synthesis, recorded samples, or a hybrid maintained audio strategy;
3. asset provenance and redistribution licences;
4. measured audio latency and Minimum-tier resource use;
5. exact Slow, Steady, and Fast BPM values;
6. final octave viewport and edge-indicator treatment;
7. exact animation and spacing when accidental rows appear;
8. versioned maintained project schema and migrations;
9. Guardian recovery retention and permanent-removal policy;
10. trusted Cairn account-level API;
11. keyboard, switch, and nonvisual-composition scope;
12. final child-facing name and icon;
13. music-educator and accessibility reviewers; and
14. maintained implementation, packaging, release, and Cairn integration.

None of these authorizes expanding the disposable prototype into a maintained
product without the required follow-up decision.

## 18. Starting evidence

The proposal treats these as hypotheses, not proof:

- creativity tools benefit from a low threshold and room to grow;
- immediate audible feedback and frequent playback can support revision; and
- direct manipulation can connect time and pitch to visible structure.

Starting references:

- Ford et al., *Identifying Engagement in Children's Interaction whilst
  Composing Digital Music at Home* (2022):
  <https://qmro.qmul.ac.uk/xmlui/handle/123456789/79025>
- Tuuri et al., *Affordances of music composing software for learning
  mathematics at primary schools* (2020):
  <https://journal.alt.ac.uk/index.php/rlt/article/view/2259>
- MusicXML 4.0 notation basics:
  <https://www.w3.org/2021/06/musicxml40/tutorial/notation-basics/>
- Qt audio overview:
  <https://doc.qt.io/qt-6/audiooverview.html>
- Qt `QSaveFile`:
  <https://doc.qt.io/qt-6/qsavefile.html>
- U.S. Copyright Office, *Games*:
  <https://copyright.gov/register/tx-games.html>

Only observation with children, beginning musicians, teachers, and
accessibility reviewers can validate Cairn Music itself.
