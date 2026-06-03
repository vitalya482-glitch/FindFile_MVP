from database.db import get_conn


ALL_ROOTS = "Все папки"


def search_files(q: str, root_path: str | None = None):
    q = q.strip()
    if not q:
        return []

    conn = get_conn()
    try:
        params = [f"%{q}%", f"%{q}%"]
        root_filter = ""
        if root_path and root_path != ALL_ROOTS:
            root_filter = "AND root_path = ?"
            params.append(root_path)

        return conn.execute(
            f"""
            SELECT filename, full_path, root_path, size_bytes, modified_at
            FROM files
            WHERE (filename LIKE ? OR full_path LIKE ?)
            {root_filter}
            ORDER BY filename
            LIMIT 500
            """,
            params,
        ).fetchall()
    finally:
        conn.close()
