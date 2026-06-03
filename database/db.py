import sqlite3
from config import DB_PATH


def get_conn():
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(str(DB_PATH))
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS files(
            id INTEGER PRIMARY KEY,
            filename TEXT NOT NULL,
            full_path TEXT NOT NULL UNIQUE,
            root_path TEXT,
            modified_at REAL,
            size_bytes INTEGER
        )
        """
    )

    columns = {row[1] for row in conn.execute("PRAGMA table_info(files)").fetchall()}
    if "root_path" not in columns:
        conn.execute("ALTER TABLE files ADD COLUMN root_path TEXT")
    if "modified_at" not in columns:
        conn.execute("ALTER TABLE files ADD COLUMN modified_at REAL")
    if "size_bytes" not in columns:
        conn.execute("ALTER TABLE files ADD COLUMN size_bytes INTEGER")

    conn.execute("CREATE INDEX IF NOT EXISTS idx_filename ON files(filename)")
    conn.execute("CREATE INDEX IF NOT EXISTS idx_full_path ON files(full_path)")
    conn.execute("CREATE INDEX IF NOT EXISTS idx_root_path ON files(root_path)")
    conn.commit()
    return conn


def clear_index(conn) -> None:
    conn.execute("DELETE FROM files")
    conn.commit()


def clear_root_index(conn, root_path: str) -> None:
    conn.execute("DELETE FROM files WHERE root_path = ?", (root_path,))
    conn.commit()
