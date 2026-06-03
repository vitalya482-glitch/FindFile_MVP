import os
import sys
from PySide6.QtCore import Qt, QUrl
from PySide6.QtGui import QDesktopServices
from PySide6.QtWidgets import (
    QApplication,
    QWidget,
    QVBoxLayout,
    QHBoxLayout,
    QPushButton,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QMessageBox,
    QLabel,
    QFileDialog,
    QComboBox,
    QTabWidget,
    QTextEdit,
)
from search.service import ALL_ROOTS, search_files
from indexer.scanner import format_bytes, format_duration, reindex
from settings import add_root_path, load_settings, remove_root_path, save_index_stats
from config import DB_PATH, SETTINGS_PATH


class MainWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("FindFile")
        self.resize(1000, 700)

        self.settings = load_settings()
        self.root_paths = self.settings.get("root_paths", [])
        self.active_root_path = self.settings.get("active_root_path", "")

        main_layout = QVBoxLayout(self)

        self.tabs = QTabWidget()
        main_layout.addWidget(self.tabs)

        self.search_tab = QWidget()
        self.stats_tab = QWidget()
        self.tabs.addTab(self.search_tab, "Поиск")
        self.tabs.addTab(self.stats_tab, "Статистика")

        self.build_search_tab()
        self.build_stats_tab()
        self.refresh_all()

    def build_search_tab(self):
        layout = QVBoxLayout(self.search_tab)

        self.path_label = QLabel()
        self.path_label.setTextInteractionFlags(Qt.TextSelectableByMouse)

        path_layout = QHBoxLayout()
        self.root_combo = QComboBox()
        self.root_combo.setMinimumWidth(450)
        self.choose_btn = QPushButton("Добавить папку")
        self.remove_btn = QPushButton("Удалить папку из списка")
        self.index_btn = QPushButton("Обновить индекс выбранной папки")
        path_layout.addWidget(self.root_combo, 1)
        path_layout.addWidget(self.choose_btn)
        path_layout.addWidget(self.remove_btn)
        path_layout.addWidget(self.index_btn)

        search_filter_layout = QHBoxLayout()
        self.search_scope_combo = QComboBox()
        self.search_scope_combo.setMinimumWidth(450)
        self.search = QLineEdit()
        self.search.setPlaceholderText("Поиск по имени или пути")
        search_filter_layout.addWidget(QLabel("Искать в:"))
        search_filter_layout.addWidget(self.search_scope_combo)
        search_filter_layout.addWidget(self.search, 1)

        self.info_label = QLabel()
        self.info_label.setTextInteractionFlags(Qt.TextSelectableByMouse)

        self.list = QListWidget()

        layout.addWidget(self.path_label)
        layout.addLayout(path_layout)
        layout.addWidget(self.info_label)
        layout.addLayout(search_filter_layout)
        layout.addWidget(self.list)

        self.root_combo.currentTextChanged.connect(self.on_active_root_changed)
        self.search_scope_combo.currentTextChanged.connect(lambda _: self.update_results(self.search.text()))
        self.choose_btn.clicked.connect(self.choose_folder)
        self.remove_btn.clicked.connect(self.remove_selected_folder)
        self.index_btn.clicked.connect(self.reindex_files)
        self.search.textChanged.connect(self.update_results)
        self.list.itemDoubleClicked.connect(self.open_item)

    def build_stats_tab(self):
        layout = QVBoxLayout(self.stats_tab)
        self.stats_text = QTextEdit()
        self.stats_text.setReadOnly(True)
        layout.addWidget(self.stats_text)

    def refresh_all(self):
        self.settings = load_settings()
        self.root_paths = self.settings.get("root_paths", [])
        self.active_root_path = self.settings.get("active_root_path", "")
        self.refresh_combos()
        self.refresh_labels()
        self.refresh_stats()

    def refresh_combos(self):
        self.root_combo.blockSignals(True)
        self.search_scope_combo.blockSignals(True)

        self.root_combo.clear()
        self.root_combo.addItems(self.root_paths)
        if self.active_root_path and self.active_root_path in self.root_paths:
            self.root_combo.setCurrentText(self.active_root_path)

        self.search_scope_combo.clear()
        self.search_scope_combo.addItem(ALL_ROOTS)
        self.search_scope_combo.addItems(self.root_paths)

        self.root_combo.blockSignals(False)
        self.search_scope_combo.blockSignals(False)

    def refresh_labels(self):
        active = self.active_root_path or "не выбрана"
        self.path_label.setText(f"Выбранная папка для обновления индекса: {active}")
        db_status = "есть" if DB_PATH.exists() else "нет"
        db_size = format_bytes(DB_PATH.stat().st_size) if DB_PATH.exists() else "0 Б"
        self.info_label.setText(
            f"Индекс: {DB_PATH} ({db_status}, {db_size}) | Настройки: {SETTINGS_PATH}"
        )

    def refresh_stats(self):
        stats_by_root = self.settings.get("last_index_stats", {}) or {}
        lines = ["СТАТИСТИКА ИНДЕКСА", ""]
        lines.append(f"Файл индекса: {DB_PATH}")
        lines.append(f"Размер файла индекса: {format_bytes(DB_PATH.stat().st_size) if DB_PATH.exists() else '0 Б'}")
        lines.append("")

        if not self.root_paths:
            lines.append("Папки для индексации пока не добавлены.")
            self.stats_text.setPlainText("\n".join(lines))
            return

        for root in self.root_paths:
            stats = stats_by_root.get(root)
            lines.append("=" * 70)
            lines.append(f"Папка: {root}")
            if not stats:
                lines.append("Статус: ещё не индексировалась")
                lines.append("")
                continue

            lines.append(f"Последняя индексация: {stats.get('indexed_at', '-')}")
            lines.append(f"Время индексации: {format_duration(float(stats.get('elapsed_seconds') or 0))}")
            lines.append(f"Размер проиндексированных данных: {format_bytes(int(stats.get('indexed_size_bytes') or 0))}")
            lines.append(f"Файлов: {int(stats.get('files_count') or 0):,}".replace(",", " "))
            lines.append(f"Папок: {int(stats.get('folders_count') or 0):,}".replace(",", " "))
            lines.append(f"Word: {int(stats.get('word_count') or 0):,}".replace(",", " "))
            lines.append(f"PDF: {int(stats.get('pdf_count') or 0):,}".replace(",", " "))
            lines.append(f"Excel/CSV: {int(stats.get('excel_count') or 0):,}".replace(",", " "))
            lines.append(f"Изображения: {int(stats.get('image_count') or 0):,}".replace(",", " "))
            lines.append(f"Видео: {int(stats.get('video_count') or 0):,}".replace(",", " "))
            lines.append(f"Прочие: {int(stats.get('other_count') or 0):,}".replace(",", " "))
            lines.append(f"Ошибок доступа/чтения: {int(stats.get('errors_count') or 0):,}".replace(",", " "))
            lines.append("")

        self.stats_text.setPlainText("\n".join(lines))

    def on_active_root_changed(self, root_path: str):
        if not root_path:
            return
        self.active_root_path = root_path
        # Persist only the active choice, without changing the folder list.
        from settings import save_root_paths
        save_root_paths(self.root_paths, self.active_root_path)
        self.refresh_all()

    def choose_folder(self):
        folder = QFileDialog.getExistingDirectory(
            self,
            "Выберите папку для индексации",
            self.active_root_path if self.active_root_path and os.path.exists(self.active_root_path) else "",
        )
        if not folder:
            return

        self.settings = add_root_path(folder)
        self.refresh_all()
        QMessageBox.information(
            self,
            "FindFile",
            "Папка добавлена. Чтобы создать или обновить её независимый индекс, нажмите «Обновить индекс выбранной папки».",
        )

    def remove_selected_folder(self):
        root_path = self.root_combo.currentText().strip()
        if not root_path:
            return
        answer = QMessageBox.question(
            self,
            "FindFile",
            "Удалить папку из списка?\n\nЗаписи этой папки в индексе останутся до следующей очистки базы, но в фильтрах папка больше не будет отображаться.",
        )
        if answer != QMessageBox.StandardButton.Yes:
            return
        self.settings = remove_root_path(root_path)
        self.refresh_all()
        self.update_results(self.search.text())

    def reindex_files(self):
        root_path = self.root_combo.currentText().strip()
        if not root_path:
            QMessageBox.warning(self, "FindFile", "Сначала добавьте папку для индексации.")
            return
        try:
            stats = reindex(root_path)
            save_index_stats(root_path, stats.to_dict())
            QMessageBox.information(self, "FindFile", stats.as_message())
            self.refresh_all()
            self.update_results(self.search.text())
        except Exception as exc:
            QMessageBox.critical(self, "Ошибка индексации", str(exc))

    def update_results(self, text):
        self.list.clear()
        if not text:
            return
        scope = self.search_scope_combo.currentText().strip()
        for name, path, root_path, size_bytes, modified_at in search_files(text, scope):
            size_text = format_bytes(size_bytes or 0)
            item = QListWidgetItem(f"{name} | {root_path} | {size_text} | {path}")
            item.setData(Qt.UserRole, path)
            self.list.addItem(item)

    def open_item(self, item):
        path = item.data(Qt.UserRole)
        if not path or not os.path.exists(path):
            QMessageBox.warning(self, "Файл не найден", "Файл недоступен или был перемещён.")
            return
        QDesktopServices.openUrl(QUrl.fromLocalFile(path))


def run_app():
    app = QApplication(sys.argv)
    w = MainWindow()
    w.show()
    sys.exit(app.exec())
