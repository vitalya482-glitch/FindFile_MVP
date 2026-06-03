from pathlib import Path

ROOT_PATH = r"\\Diskstationnew\Exchange"
BASE_DIR = Path(__file__).resolve().parent
DATA_DIR = BASE_DIR / "data"
DB_PATH = DATA_DIR / "file_index.db"
