# Privacy-preserving child-test protocol

**Status:** procedure only; child testing is not yet authorized

This procedure is for the two initial testers described in the design. Use the
role labels **younger** and **older** in commands and notes; do not add names,
exact ages, account identifiers, or other personal data.

Completing this procedure does not authorize a child session. Testing remains
blocked until every item in issue #15 is closed and the independent review in
issue #14 records a go decision for an exact prototype commit. A go decision
would authorize only the agreed observation of the disposable local prototype,
not maintained implementation, packaging, publication, or release.

## Before any session

An adult facilitator must:

1. Record the exact prototype commit approved by the final readiness review.
2. Obtain guardian consent and the child's assent. Explain that stopping is
   always allowed and has no penalty.
3. Use the reviewed machine, display scaling, pointer, and audio setup. Disable
   networking and close notifications, file managers, terminals, and unrelated
   applications before the child arrives.
4. Confirm that no AI agent will interact with the child unsupervised.
5. Prepare the child's isolated data slot as described below. Never copy a song
   between slots.
6. Prepare paper or a local plain-text note with only the fields in the note
   template. Do not enable audio, video, screen, telemetry, or keystroke
   recording. A screenshot is also a recording and is not part of a child
   session by default.

## Clean local data and launch

Run these commands as the adult from the repository root. `CHILD_SLOT` must be
exactly `younger` or `older`.

```sh
umask 077
STUDY_ROOT="$HOME/.local/share/cairn-music-child-test"
CHILD_SLOT="younger" # change to "older" for the other child
SLOT_ROOT="$STUDY_ROOT/$CHILD_SLOT"

install -d -m 700 "$STUDY_ROOT" "$SLOT_ROOT"
if [ -e "$SLOT_ROOT/current" ]; then
    mv -- "$SLOT_ROOT/current" \
        "$SLOT_ROOT/archive-before-$(date +%Y%m%dT%H%M%S)"
fi
install -d -m 700 \
    "$SLOT_ROOT/current/data" \
    "$SLOT_ROOT/current/config" \
    "$SLOT_ROOT/current/cache"

env \
    XDG_DATA_HOME="$SLOT_ROOT/current/data" \
    XDG_CONFIG_HOME="$SLOT_ROOT/current/config" \
    XDG_CACHE_HOME="$SLOT_ROOT/current/cache" \
    ./build/cairn-music
```

This reset is performed once before that child's first observed session, or
only after an adult explicitly abandons an invalid run. Do not reset between
that child's initial session and return occasions: automatic reopen must use the
same `current` slot. Launch every later occasion with the same three `XDG_*`
values. The separate slots prevent sibling project sharing; mode `0700` keeps
other local users out. The child remains in the prototype and is not given a
shell, file manager, file picker, or access to these paths.

Before handing over the pointer, verify that the intended slot is selected and
that the prototype opens a blank two-measure song. If it does not, stop and fix
the setup without the child present. Never delete an old `current` directory;
move it aside as shown so unexpected data can be inspected.

## Facilitation rule

Start with one neutral sentence: **"You can make music here. Tell me when you're
done."** Then observe quietly.

During the observed first minute, do not demonstrate controls, point to a sound,
read labels as instructions, give a sequence of steps, or turn the session into
a tutorial. A subtle application-provided inactivity hint may appear normally.
The adult may intervene only for safety, to stop the session, or to correct a
technical setup problem.

After the first minute, answer a direct request only with the least-leading
response that keeps the child in control, such as **"What would you like to try?"**
Record any assistance. If the adult identifies a control or supplies a step,
the affected action is not independent and cannot satisfy an independence
condition in that session.

## Initial observation

For the **younger** flow, observe whether the child independently:

1. discovers how to select and place sounds;
2. creates and plays a song;
3. finishes without a required filename or Save action; and
4. closes and later reopens the same song from the isolated slot.

For the **older** flow, observe the same sequence and whether the child also:

1. extends the song by adding content or measures; and
2. edits existing work using replacement, erase, or Undo.

Do not direct the child through this list. It is an observer checklist, not a
script for the child. First-note and first-playback time may be noted as rough
diagnostics, but neither is a pass condition.

## Return occasions and pass conditions

A return counts only when it occurs on a later occasion and the child either
initiates the request or freely chooses the prototype without a leading prompt.
Do not remind, reward, persuade, schedule a mandatory session, or say that the
child needs to help finish a test. An adult-manufactured return does not count.
Use the same data slot so the child can encounter prior work.

The gate passes only when all of these are true:

- both children have used the prototype;
- each child voluntarily returns on two later occasions;
- the younger child independently creates, plays, autosaves, and reopens a song;
- the older child independently creates, plays, autosaves, reopens, extends, and
  edits a song; and
- no required tutorial or adult step-by-step coaching manufactured those
  outcomes.

A failure to meet the gate is evidence, not a reason to coach or repeat an
action until it passes.

## Stop conditions

Stop immediately if the child withdraws assent, shows distress or sustained
frustration, or asks to stop. Also stop for repeated technical failure, a crash,
a save warning, unexpected data from the other slot, or accidental exposure of
unrelated files or applications.

Move the child away from the machine before diagnosis. Do not ask the child to
repeat a failed action. Record only what was already observed. Quarantine the
slot and treat any unrelated-file exposure as a privacy incident; do not resume
until the cause is understood and the readiness gate is reviewed.

## Preserve a failed autosave

If **"Your song is here, but it is not saved yet."** appears:

1. Stop interaction immediately. Do not make another edit, reset the slot, or
   ask the child to reproduce the failure.
2. Keep the application open if practical; the newest unsaved state exists only
   in memory. Move the child away from the machine.
3. In a separate adult terminal, set the same slot label and preserve the entire
   slot before diagnosis:

   ```sh
   umask 077
   STUDY_ROOT="$HOME/.local/share/cairn-music-child-test"
   CHILD_SLOT="younger" # use the slot from this session
   SLOT_ROOT="$STUDY_ROOT/$CHILD_SLOT"
   SNAPSHOT="$SLOT_ROOT/failure-$(date +%Y%m%dT%H%M%S)"
   cp -a -- "$SLOT_ROOT/current" "$SNAPSHOT"
   chmod -R a-w "$SNAPSHOT"
   printf '%s\n' "$SNAPSHOT"
   ```

4. Note the last observed action, the visible warning, and whether the app
   remained open. Do not include the child's name or ask for a reenactment.
5. Preserve the running process for live diagnosis when feasible. The snapshot
   contains the last committed autosave and any filesystem artifacts, but it
   cannot capture newer in-memory edits. Closing the app can therefore lose the
   unsaved delta.

If startup reports that a song needs help, archive the slot before choosing
**Keep it safe and start a new song**. The prototype normally creates a verified
`prototype-autosave.json.recovery-<id>` sibling, but the whole-slot archive is
the study record. Do not resume the child session until an adult verifies that
the original or recovery copy is preserved.

## Minimal observation note

Create one note per child slot and use only these fields:

```text
slot: younger | older
occasion: initial | return-1 | return-2 | other
voluntary return: yes | no | not applicable
observed actions: [short action list]
confusion points: [short functional description or none]
assistance required: [none or the minimal help given]
reopened prior song: yes | no | not observed
extended song: yes | no | not observed
edited existing work: yes | no | not observed
stop condition: [none or category only]
outcome: pass evidence | incomplete evidence | stopped
```

Do not record names, exact ages, birthdays, account names, free-form profiles,
verbatim conversation, or unrelated family details. Keep the local notes with
the adult-controlled study data, disclose them only to the readiness reviewers,
and delete them when the prototype decision no longer needs the evidence. Any
exception to no-recording or this minimal retention requires a new explicit
owner decision and fresh guardian consent before a session; it is not granted by
this protocol.
