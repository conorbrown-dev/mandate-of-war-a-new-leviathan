# CLAUDE.md

Read `AGENTS.md` first, then `docs/HANDOFF.md` and `docs/STATUS.md`.

## Worktrees

Do all work in a dedicated git worktree on its own task branch, never in the primary checkout, which holds the user's in-progress changes. Build and test inside the worktree, report its path and branch when finished, and remove only worktrees you created. See "Use Git Worktrees" in `AGENTS.md` for the full rules.

## Sourced models and assets

All sourced models must be royalty free for commercial use, and each one must be given credit in the game.
