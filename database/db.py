
import sqlite3
from config import DB_PATH

def get_conn():
    conn = sqlite3.connect(DB_PATH)
    conn.execute("CREATE TABLE IF NOT EXISTS files(id INTEGER PRIMARY KEY, filename TEXT, full_path TEXT UNIQUE)")
    conn.execute("CREATE INDEX IF NOT EXISTS idx_filename ON files(filename)")
    return conn
