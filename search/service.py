
from database.db import get_conn

def search_files(q):
    conn = get_conn()
    rows = conn.execute(
        "SELECT filename, full_path FROM files WHERE filename LIKE ? OR full_path LIKE ? LIMIT 500",
        (f"%{q}%", f"%{q}%")
    ).fetchall()
    conn.close()
    return rows
