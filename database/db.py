import sqlite3
from config import DATA_DIR, DB_PATH


def get_conn():
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(str(DB_PATH))
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS files(
            id INTEGER PRIMARY KEY,
            filename TEXT NOT NULL,
            full_path TEXT NOT NULL UNIQUE
        )
        """
    )
    conn.execute("CREATE INDEX IF NOT EXISTS idx_filename ON files(filename)")
    conn.execute("CREATE INDEX IF NOT EXISTS idx_full_path ON files(full_path)")
    return conn
