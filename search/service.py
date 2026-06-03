from database.db import get_conn


def search_files(q):
    q = q.strip()
    if not q:
        return []

    conn = get_conn()
    try:
        return conn.execute(
            """
            SELECT filename, full_path
            FROM files
            WHERE filename LIKE ? OR full_path LIKE ?
            ORDER BY filename
            LIMIT 500
            """,
            (f"%{q}%", f"%{q}%"),
        ).fetchall()
    finally:
        conn.close()
