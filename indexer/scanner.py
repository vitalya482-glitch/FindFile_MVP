import os
import time
from dataclasses import asdict, dataclass, field
from datetime import datetime
from database.db import clear_root_index, get_conn
from config import DB_PATH


WORD_EXTENSIONS = {".doc", ".docx", ".docm", ".rtf", ".odt"}
EXCEL_EXTENSIONS = {".xls", ".xlsx", ".xlsm", ".xlsb", ".csv", ".ods"}
IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".gif", ".bmp", ".tif", ".tiff", ".webp", ".heic"}
VIDEO_EXTENSIONS = {".mp4", ".mov", ".avi", ".mkv", ".wmv", ".m4v", ".flv", ".webm"}
PDF_EXTENSIONS = {".pdf"}


@dataclass
class IndexStats:
    files_count: int = 0
    folders_count: int = 0
    word_count: int = 0
    pdf_count: int = 0
    excel_count: int = 0
    image_count: int = 0
    video_count: int = 0
    other_count: int = 0
    elapsed_seconds: float = 0.0
    errors_count: int = 0
    root_path: str = ""
    indexed_size_bytes: int = 0
    db_size_bytes: int = 0
    indexed_at: str = ""
    skipped_paths: list[str] = field(default_factory=list)

    def to_dict(self) -> dict:
        return asdict(self)

    def as_message(self) -> str:
        elapsed = format_duration(self.elapsed_seconds)
        lines = [
            "Индексация завершена.",
            "",
            f"Папка: {self.root_path}",
            f"Дата индексации: {self.indexed_at}",
            f"Время индексации: {elapsed}",
            f"Размер индекса: {format_bytes(self.db_size_bytes)}",
            f"Размер проиндексированных данных: {format_bytes(self.indexed_size_bytes)}",
            f"Файлов проиндексировано: {self.files_count:,}".replace(",", " "),
            f"Папок просканировано: {self.folders_count:,}".replace(",", " "),
            "",
            "По типам файлов:",
            f"Word: {self.word_count:,}".replace(",", " "),
            f"PDF: {self.pdf_count:,}".replace(",", " "),
            f"Excel/CSV: {self.excel_count:,}".replace(",", " "),
            f"Изображения: {self.image_count:,}".replace(",", " "),
            f"Видео: {self.video_count:,}".replace(",", " "),
            f"Прочие файлы: {self.other_count:,}".replace(",", " "),
        ]

        if self.errors_count:
            lines.extend([
                "",
                f"Ошибок доступа/чтения: {self.errors_count:,}".replace(",", " "),
            ])
            if self.skipped_paths:
                lines.append("Первые проблемные пути:")
                lines.extend(self.skipped_paths[:5])

        return "\n".join(lines)


def format_duration(seconds: float) -> str:
    total_seconds = int(round(seconds))
    hours, remainder = divmod(total_seconds, 3600)
    minutes, secs = divmod(remainder, 60)

    if hours:
        return f"{hours} ч {minutes} мин {secs} сек"
    if minutes:
        return f"{minutes} мин {secs} сек"
    return f"{secs} сек"


def format_bytes(value: int | None) -> str:
    if not value:
        return "0 Б"
    units = ["Б", "КБ", "МБ", "ГБ", "ТБ"]
    size = float(value)
    unit_index = 0
    while size >= 1024 and unit_index < len(units) - 1:
        size /= 1024
        unit_index += 1
    return f"{size:.2f} {units[unit_index]}"


def add_file_to_stats(stats: IndexStats, filename: str) -> None:
    ext = os.path.splitext(filename)[1].lower()
    if ext in WORD_EXTENSIONS:
        stats.word_count += 1
    elif ext in PDF_EXTENSIONS:
        stats.pdf_count += 1
    elif ext in EXCEL_EXTENSIONS:
        stats.excel_count += 1
    elif ext in IMAGE_EXTENSIONS:
        stats.image_count += 1
    elif ext in VIDEO_EXTENSIONS:
        stats.video_count += 1
    else:
        stats.other_count += 1


def reindex(root_path: str) -> IndexStats:
    if not root_path:
        raise ValueError("Выберите папку для индексации.")

    if not os.path.exists(root_path):
        raise FileNotFoundError(f"Путь для индексации недоступен: {root_path}")

    started_at = time.perf_counter()
    stats = IndexStats(root_path=root_path)
    conn = get_conn()

    def on_walk_error(error: OSError) -> None:
        stats.errors_count += 1
        path = getattr(error, "filename", None)
        if path and len(stats.skipped_paths) < 5:
            stats.skipped_paths.append(str(path))

    try:
        # Independent indexes: refresh only the selected root folder.
        clear_root_index(conn, root_path)
        for root, dirs, files in os.walk(root_path, onerror=on_walk_error):
            stats.folders_count += 1

            for filename in files:
                full_path = os.path.join(root, filename)
                try:
                    stat = os.stat(full_path)
                    modified_at = stat.st_mtime
                    size_bytes = stat.st_size
                    stats.indexed_size_bytes += int(size_bytes)
                except OSError:
                    stats.errors_count += 1
                    if len(stats.skipped_paths) < 5:
                        stats.skipped_paths.append(full_path)
                    modified_at = None
                    size_bytes = None

                conn.execute(
                    """
                    INSERT OR REPLACE INTO files(
                        filename, full_path, root_path, modified_at, size_bytes
                    ) VALUES (?, ?, ?, ?, ?)
                    """,
                    (filename, full_path, root_path, modified_at, size_bytes),
                )
                stats.files_count += 1
                add_file_to_stats(stats, filename)

                if stats.files_count % 5000 == 0:
                    conn.commit()

        conn.commit()
        stats.elapsed_seconds = time.perf_counter() - started_at
        stats.indexed_at = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        stats.db_size_bytes = DB_PATH.stat().st_size if DB_PATH.exists() else 0
        return stats
    finally:
        conn.close()
