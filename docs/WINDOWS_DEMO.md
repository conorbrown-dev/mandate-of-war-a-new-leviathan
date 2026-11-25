# Windows Demo Build

The `Windows Demo Package` GitHub Actions workflow builds the Windows x86_64
GDExtension, exports the Godot project with Godot 4.7.2, and runs a headless
skirmish-start check through the exported console wrapper. The package contains
`MandateOfWarDemo.exe`, `bin/rts_gdextension.dll`, and a sibling `data/`
directory used by the native simulation and unit catalog. The runtime selects
that sibling `data/` directory when it contains `unit_faction_stats.json`.

Download the `MandateOfWarDemo-Windows-x86_64` artifact from the successful
workflow run. Extract its ZIP and run `MandateOfWarDemo.exe` from the extracted
folder. The game requires the entire folder so the extension DLL and `data/`
directory stay beside the executable.

## Startup-path fix (2026-11-11)

The first published ZIP used a `standalone` feature check that selected
`../../data` on an exported Windows run. A user log showed JSON parsing fail
and `Unit data could not be read from ../../data`, leaving the startup screen
at `LOADING SCENARIO`. The game now checks for the unit catalog in the
executable-adjacent `data/` folder before using the development checkout path.

Local validation: Release native build passed. Godot 4.7.2, launched from a
copied executable with a sibling runtime `data/` folder, printed that folder
as `Runtime data root` and started Broken Strait with one Field Engineer per
side. A PowerShell check asserted both lines and rejected script or unit-data
errors. The exported ZIP itself still needs a new package run; the workflow now
checks for the skirmish-start line through the console wrapper from Godot's
Windows release export templates. Godot 4.7.2 did not emit that wrapper during
the first fixed-path package run, so the workflow copies it beside the game.
