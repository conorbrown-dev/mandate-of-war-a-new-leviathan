# Accepted Visual Baselines

Only `tools/validate <scenario> --rendered --update-baseline --visual-threshold <calibrated-value>` may create or replace references here. Review repeated rendered checkpoints before accepting them; the command records the explicit scenario/checkpoint threshold in `manifest.json`.

No baseline is accepted yet because the initial implementation environment did not provide a render-capable connection inside the normal filesystem/process sandbox. Headless state validation remains functional and reports screenshot checkpoints as skipped rather than claiming nonexistent images.
