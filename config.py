import sys
from pathlib import Path


def get_app_dir() -> Path:
    """Directory where FindFile stores user data.

    In a PyInstaller EXE this is the folder with FindFile.exe.
    In source mode this is the project root.
    """
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parent


APP_DIR = get_app_dir()
DB_PATH = APP_DIR / "file_index.db"
SETTINGS_PATH = APP_DIR / "settings.json"

# Used only as an initial value if settings.json does not exist yet.
DEFAULT_ROOT_PATH = r"\\Diskstationnew\Exchange"
