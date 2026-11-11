# Windows Demo Build

The `Windows Demo Package` GitHub Actions workflow builds the Windows x86_64
GDExtension, exports the Godot project with Godot 4.7.2, and runs a headless
startup check against the exported game. The package contains
`MandateOfWarDemo.exe`, `bin/rts_gdextension.dll`, and a sibling `data/`
directory used by the native simulation and unit catalog.

Download the `MandateOfWarDemo-Windows-x86_64` artifact from the successful
workflow run. Extract its ZIP and run `MandateOfWarDemo.exe` from the extracted
folder. The game requires the entire folder so the extension DLL and `data/`
directory stay beside the executable.
