
import os
from database.db import get_conn
from config import ROOT_PATH

def reindex():
    conn = get_conn()
    conn.execute("DELETE FROM files")
    for r, _, files in os.walk(ROOT_PATH):
        for f in files:
            conn.execute("INSERT OR IGNORE INTO files(filename, full_path) VALUES (?, ?)", (f, os.path.join(r, f)))
    conn.commit()
    conn.close()
