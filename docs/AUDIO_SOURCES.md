# Slice 4 sound sources

Researched 2026-11-08. The table retains the original sourcing shortlist; current playback is described below.
Acquired 2026-11-08: 382 unmodified audio files now live in `godot/project/audio/`,
with a SHA-256/source/license manifest and pack license notices. The current Kenney
page uses signed download links; those worked after the old links returned 403.
Godot imports and decodes every file. Four UI cues are wired into Strategic Command.
Distinct cannon/artillery, missile, rifle and burst cues now supplement positional
impact/destruction sounds. Moving vehicles and airborne aircraft have bounded loops;
F2 supplies persistent channel controls and player-accessible attribution. Two edited
firearm cues are reproducible with tools/prepare_audio.py and manifest their original
source, trims, mono/downsampling, normalization/fades and new checksums. Original
source bytes remain unchanged. The measured mix/listening reel does not constitute
human audition; listening and match feel still need human review.

## First-choice free sources

| Source and author | Proposed use (subject to audition) | Published license | Acquisition |
|---|---|---|---|
| [Interface Sounds — Kenney](https://kenney.nl/assets/interface-sounds) | UI click, panel open/close, order acceptance/refusal | CC0; 100 sounds | Acquired via current signed download link |
| [Impact Sounds — Kenney](https://kenney.nl/assets/impact-sounds) | Armor hits, metal debris, construction details | CC0; 130 sounds | Acquired via current signed download link |
| [Sci-fi Sounds — Kenney](https://kenney.nl/assets/sci-fi-sounds) | Explosion/rocket layers and alerts; audition for restrained near-future tone | CC0; 70 sounds | Acquired via current signed download link |
| [The Free Firearm Sound Library — Ben Jaszczak, Brian Nelson, Kevin Heras, Matthew Nanney](https://opengameart.org/node/21826) | Real firearm layers for machine guns; these are not tank-cannon recordings | CC0; page explicitly allows professional use without royalties/credit | Acquired; three original WAVs selected (AK-47 C_28P, AR-15 D_24P, Carl Gustav M45 G_24P) |
| [75 CC0 breaking / falling / hit sfx — rubberduck](https://opengameart.org/content/75-cc0-breaking-falling-hit-sfx) | Metal hits, collapsing buildings, rock/glass debris | CC0 | Acquired complete (75 OGG files) |
| [Explosions — EZduzziteh](https://opengameart.org/content/explosions-4) | Alternate destruction/explosion layers | CC0 | Four individual OGG files; not downloaded |

Kenney confirms commercial use and no mandatory attribution on its
[support page](https://kenney.nl/support). Preserve original licenses and source
records even where credit is optional. The proposed uses above are recommendations,
not listening results or claims of authentic military recordings.

## Vehicle candidate requiring credit

[Engine-loop heavy vehicle/tank — Nayckron, based on qubodup](https://opengameart.org/content/engine-loop-heavy-vehicletank)
offers CC BY 3.0 and GPL 3.0 alternatives. Select the **CC BY 3.0** option for this
asset if used, retain attribution and the license link, and identify edits.
The author says it is modified from a car loop, not a recording of a specific tank.
WAV/MP3/OGG and a pack with speed variations are listed. Acquired the unchanged
`engine_heavy_loop_1.ogg`; now looped on moving vehicles, with runtime gain and
speed-based pitch. Aircraft use a separate, stylized Kenney engine loop.
Credit recorded in `audio/README.md` and the source license notice:

> Engine-loop heavy vehicle/tank by Nayckron, based on qubodup,
> https://opengameart.org/content/engine-loop-heavy-vehicletank,
> CC BY 3.0, https://creativecommons.org/licenses/by/3.0/.
> Source unchanged; runtime looping, gain and speed-based pitch.

The same credit, source URL and CC BY 3.0 link are visible to players in F2 /
Audio & Credits. No derived CC BY source file was written.

## Broader recording library

[Sonniss GameAudioGDC](https://sonniss.com/gameaudiogdc/) is a free archive worth
searching for machinery, aircraft and environmental recordings. Specific suitable
clips have **not** been identified. Do not assume the paid catalog's recordings
are included in a free bundle.

The [GDC bundle license](https://sonniss.com/gdc-bundle-license/) permits commercial
game synchronization and editing without mandatory attribution, and restricts
standalone redistribution and AI/ML training. It is not CC0. Before acquisition,
retain the applicable license and assess the repository distribution route: do not
add these raw files to a publicly accessible source tree. No bundle acquired.

## Intended first audition set

Start with 2 UI clicks, 1 accepted-order cue, 1 refused-order cue, 3 armor hits,
3 destruction variants, 3 cannon/artillery candidates, 1 machine-gun burst and
1 rocket launch. Prefer dry sounds with no baked-in music or other battle noise.
Real cannon/artillery, jet, helicopter and ground-engine recordings remain gaps;
do not call a layered substitute an authentic recording.

For each selected file, record source URL, creator, license, original filename,
SHA-256, edits and intended event. Retain original license text beside acquired
assets. Audition in context before selecting final clips. UI playback uses four bounded voices at -18 dB through UI. Combat uses 12
bounded positional voices at -16 dB through Combat and native visible events with
current-sight checks. Engines use eight nearest visible moving/airborne sources
through Engines (ground -26 dB, stylized aircraft -20 dB), with a 1.8 km cutoff.
Master/UI/Combat/Engines controls and a Master limiter are implemented. These are
measured initial mix choices; authentic aircraft/heavy-weapon recordings remain
optional replacements, not a claim about the current synthesized layers.
