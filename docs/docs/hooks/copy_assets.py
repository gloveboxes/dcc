from pathlib import Path
import shutil


def on_pre_build(config):
    config_path = Path(config.config_file_path).resolve()
    repo_root = config_path.parents[2]
    source = repo_root / "images" / "table.jpg"
    target = config_path.parent / "en" / "images" / "table.jpg"

    if not source.exists():
        raise FileNotFoundError(f"benchmark image not found: {source}")

    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)
