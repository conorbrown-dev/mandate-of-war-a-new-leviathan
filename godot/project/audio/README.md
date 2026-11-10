# Sound library

382 unmodified source clips, acquired 2026-11-08 (about 23.5 MB). The three Kenney
packs and rubberduck destruction pack are complete. The firearm library is a
three-file selection: AK-47 C_28P, AR-15 D_24P, Carl Gustav M45 G_24P. These WAVs
are source recordings and may contain multiple takes; edit/trim before combat use.
The heavy engine is engine_heavy_loop_1.ogg, modified from a car loop by its author.

`manifest.json` records every audio file's creator, original filename, source URL,
license, SHA-256 and edit status. Source licenses/notices live in each pack folder.
Kenney's original license files are preserved verbatim. The 382 originals are
unchanged. Two derived mono PCM16/24 kHz cues
under edited/ are reproducible with tools/prepare_audio.py; their source paths,
trim/fade/normalization details and hashes are included in the manifest.

CC0: Kenney, rubberduck, and the Free Firearm Sound Library by Ben Jaszczak,
Brian Nelson, Kevin Heras, Matthew Nanney. Optional credit is appreciated.

Required credit for the engine, distributed under its CC BY 3.0 option:

Engine-loop heavy vehicle/tank by Nayckron, based on qubodup.
https://opengameart.org/content/engine-loop-heavy-vehicletank
Licensed under CC BY 3.0: https://creativecommons.org/licenses/by/3.0/
Source recording unchanged. In game: looping, gain and speed-based pitch.
This attribution is also available to players under F2 / Audio & Credits.

`ui_audio.gd`: four quiet UI voices on the UI bus. `combat_effects.gd`: 12
positional Combat voices, visible native events and a 4.5 km cutoff. The palette
selects distinct cannon/artillery, missile, rifle and burst cues by unit type;
recon and cargo aircraft have no firing sound. Impacts/destruction retain Kenney
layers. `engine_audio.gd`: eight nearest moving ground/airborne aircraft loops,
1.8 km cutoff, current-visibility checks and no idle ground rumble. Aircraft use
Kenney spaceEngineLarge_000 as a stylized turbine substitute, 6 dB above the ground
loop's voice gain to account for its quieter steady source level. Sources are
duplicated
in memory, leaving original OGGs unchanged; gain fades in over wall time and
speed-based pitch is independent of game-speed controls.

F2 / Audio & Credits is reachable from briefing and during/after the match.
Master, UI, Combat and Engines levels persist in user://audio.cfg. Defaults are
80/80/80/60 percent; zero mutes. A Master limiter provides a -1 dBFS ceiling.
The audio modal leaves the match running and blocks tactical hotkeys while open.

The source library remains packaged with notices and provenance. Sonniss assets
are not included. Aircraft, cannon and missile sounds are stylized, not authentic
recordings. Measured mix levels and a reproducible listening reel accompany the
Slice 4 validation artifacts; human audition and full-match hand play are pending.
