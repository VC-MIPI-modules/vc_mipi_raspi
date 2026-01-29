vc-config builder

Purpose
- Assemble SOC-specific `vc-config` scripts from reusable parts placed in `scripts/parts/`.

How it works
- `vc-config.manifest` maps each SOC directory name (e.g. `bcm2712`) to an ordered list of part files (in `scripts/parts/`).
- `vc-config-builder.sh` concatenates the listed parts for each manifest line and writes `scripts/generated/<soc>/vc-config`.
- Run with `--deploy` to copy generated files into their target directories, backing up existing files with a timestamped `.orig`.

Quick start
1. Create shared/common parts in `scripts/parts/` (e.g. `header`, `common`).
2. Create per-SOC parts that contain only the SOC-specific lines (e.g. `bcm2712`).
3. Edit `scripts/vc-config.manifest` to list parts in the desired order for each SOC.
4. Run:

```bash
cd scripts
./vc-config-builder.sh
# to assemble and deploy to scripts/<soc>/vc-config (backups created):
./vc-config-builder.sh --deploy
```

Notes
- This builder is intentionally simple: it concatenates parts in order. To migrate an existing `vc-config` into this system, split the file into logical parts (header, shared functions, SOC-specific variables, per-SOC overrides) and add them to `scripts/parts/`.
- Review generated files before deploying to avoid overwriting custom edits.
