import json
from copy import deepcopy
from pathlib import Path
from typing import Any

from config import DEFAULT_ROOT_PATH, SETTINGS_PATH


def _default_settings() -> dict[str, Any]:
    roots = [DEFAULT_ROOT_PATH] if DEFAULT_ROOT_PATH else []
    return {
        "root_paths": roots,
        "active_root_path": roots[0] if roots else "",
        "last_index_stats": {},
    }


def load_settings() -> dict[str, Any]:
    defaults = _default_settings()
    if not SETTINGS_PATH.exists():
        return defaults

    try:
        with SETTINGS_PATH.open("r", encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, json.JSONDecodeError):
        return defaults

    # Migration from old single-folder format.
    if "root_paths" not in data:
        old_root = str(data.get("root_path") or data.get("active_root_path") or DEFAULT_ROOT_PATH or "")
        data["root_paths"] = [old_root] if old_root else []

    root_paths: list[str] = []
    for path in data.get("root_paths", []):
        path_str = str(path).strip()
        if path_str and path_str not in root_paths:
            root_paths.append(path_str)

    active_root_path = str(data.get("active_root_path") or data.get("root_path") or "")
    if active_root_path and active_root_path not in root_paths:
        root_paths.append(active_root_path)
    if not active_root_path and root_paths:
        active_root_path = root_paths[0]

    return {
        "root_paths": root_paths,
        "active_root_path": active_root_path,
        "last_index_stats": data.get("last_index_stats") or {},
    }


def save_settings_data(settings: dict[str, Any]) -> None:
    SETTINGS_PATH.parent.mkdir(parents=True, exist_ok=True)
    data = deepcopy(settings)
    data.pop("root_path", None)
    with SETTINGS_PATH.open("w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)


def save_root_paths(root_paths: list[str], active_root_path: str = "") -> None:
    settings = load_settings()
    unique_paths: list[str] = []
    for path in root_paths:
        path_str = str(path).strip()
        if path_str and path_str not in unique_paths:
            unique_paths.append(path_str)

    if active_root_path and active_root_path not in unique_paths:
        unique_paths.append(active_root_path)
    if not active_root_path and unique_paths:
        active_root_path = unique_paths[0]

    settings["root_paths"] = unique_paths
    settings["active_root_path"] = active_root_path
    save_settings_data(settings)


def add_root_path(root_path: str) -> dict[str, Any]:
    settings = load_settings()
    root_path = str(Path(root_path)) if root_path else ""
    if root_path and root_path not in settings["root_paths"]:
        settings["root_paths"].append(root_path)
    settings["active_root_path"] = root_path
    save_settings_data(settings)
    return settings


def remove_root_path(root_path: str) -> dict[str, Any]:
    settings = load_settings()
    settings["root_paths"] = [p for p in settings["root_paths"] if p != root_path]
    if settings.get("active_root_path") == root_path:
        settings["active_root_path"] = settings["root_paths"][0] if settings["root_paths"] else ""
    save_settings_data(settings)
    return settings


def save_index_stats(root_path: str, stats: dict[str, Any]) -> None:
    settings = load_settings()
    settings.setdefault("last_index_stats", {})[root_path] = stats
    save_settings_data(settings)
