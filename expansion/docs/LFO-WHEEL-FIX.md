# LFO Wave selector fix — v0.1.1

The popup presented eight shapes using two established parameters: the six-step
classic Wave parameter and the CLASSIC / S&H / SMOOTH digital-wave parameter.
The wheel only incremented the classic Wave parameter, so it could not enter
S&H or SMOOTH and could not leave a digital shape already selected by menu.

The editor now shares its current-shape decoder and shape-selection writer
between popup and wheel. Wheel up moves one position forward and wheel down
moves one position backward, clamping at the endpoints as other wheel selectors
already do. The complete unchanged menu order is:

SINE → TRI → SAW UP → SAW DN → SQUARE → PULSE → S&H → SMOOTH

Classic shapes retain Wave = index / 5 and digital-wave = 0. S&H retains
digital-wave = 0.5; SMOOTH retains digital-wave = 1. Selecting either digital
shape preserves the latent classic Wave value, exactly as the popup already did.
Each editor tab resolves these writes to that LFO's existing parameter IDs.

Only Editor.cpp changes interaction behavior. Version.h changes product metadata
to 0.1.1. No DSP, parameter definitions, IDs, state migration or automation
encoding changed for this fix. Historical audio and native-patch references were
not regenerated. Earlier development changes since v0.1.0 remain documented
separately in VALIDATION-CHECKPOINT.md and the changelog.

InteractionTests.cpp exercises the real editor messages and popup fixture from
every starting shape, in both directions, on all four LFOs at 100%, 125%, 150%
and 200%. PickerFixtures.h checks the actual menu order, command IDs and checked
item after wheel operations. The tests verify all eight shapes, both clamped
endpoints, exact normalized encoding, menu-to-wheel and wheel-to-menu coherence,
and unchanged unrelated parameters. This covers 4,096 wheel events and 512
checked menus. Existing four-LFO UI, DSP/state/automation and regression suites
remain part of the release gate.

Validation on the final v0.1.1 candidate: focused UI/interaction tests 2/2;
full regression suite 24/24 (257.61 seconds); Steinberg validator 47 passed,
0 failed. Linked-DLL validation passed identity/version, audio/automation,
six-theme pixel comparisons, About links, native file operations, external
import and instance independence. Original audio/native fixtures remained
byte-identical. Actual DAW listening is still a separate user check.
