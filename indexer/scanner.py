import os
from database.db import get_conn
from config import ROOT_PATH


def reindex():
    if not os.path.exists(ROOT_PATH):
        raise FileNotFoundError(f"Путь для индексации недоступен: {ROOT_PATH}")

    conn = get_conn()
    count = 0
    try:
        conn.execute("DELETE FROM files")
        for root, _, files in os.walk(ROOT_PATH):
            for filename in files:
                conn.execute(
                    "INSERT OR IGNORE INTO files(filename, full_path) VALUES (?, ?)",
                    (filename, os.path.join(root, filename)),
                )
                count += 1
        conn.commit()
        return count
    finally:
        conn.close()
