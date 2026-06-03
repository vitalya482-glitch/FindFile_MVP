import os
import sys
from PySide6.QtCore import Qt, QUrl
from PySide6.QtGui import QDesktopServices, QFont
from PySide6.QtWidgets import (
    QApplication,
    QWidget,
    QVBoxLayout,
    QHBoxLayout,
    QPushButton,
    QLineEdit,
    QMessageBox,
    QLabel,
    QFileDialog,
    QComboBox,
    QTabWidget,
    QTextEdit,
    QFrame,
    QTableWidget,
    QTableWidgetItem,
    QHeaderView,
    QFileIconProvider,
    QStyle,
)
from PySide6.QtCore import QFileInfo

from search.service import ALL_ROOTS, search_files
from indexer.scanner import format_bytes, format_duration, reindex
from settings import add_root_path, load_settings, remove_root_path, save_index_stats
from config import DB_PATH, SETTINGS_PATH


APP_STYLE = """
QWidget {
    background: #f4f7fb;
    color: #243447;
    font-size: 10.5pt;
}
QTabWidget::pane {
    border: 1px solid #d7e0ea;
    border-radius: 10px;
    background: #ffffff;
    padding: 6px;
}
QTabBar::tab {
    background: #e8eef7;
    border: 1px solid #d7e0ea;
    border-bottom: none;
    padding: 9px 18px;
    border-top-left-radius: 9px;
    border-top-right-radius: 9px;
    margin-right: 4px;
}
QTabBar::tab:selected {
    background: #ffffff;
    color: #0f5fa8;
    font-weight: 600;
}
QFrame#card {
    background: #ffffff;
    border: 1px solid #dbe5ef;
    border-radius: 12px;
}
QLabel#titleLabel {
    background: transparent;
    color: #12344d;
    font-size: 16pt;
    font-weight: 700;
}
QLabel#subtitleLabel {
    background: transparent;
    color: #6b7c8f;
}
QLabel#pillLabel {
    background: #eef6ff;
    color: #195b8f;
    border: 1px solid #cfe5fa;
    border-radius: 10px;
    padding: 6px 10px;
}
QLineEdit, QComboBox, QTextEdit {
    background: #ffffff;
    border: 1px solid #cfd9e5;
    border-radius: 8px;
    padding: 7px 9px;
    selection-background-color: #2f80ed;
}
QLineEdit:focus, QComboBox:focus, QTextEdit:focus {
    border: 1px solid #2f80ed;
}
QPushButton {
    background: #e9f1fb;
    color: #174a76;
    border: 1px solid #c9dced;
    border-radius: 9px;
    padding: 8px 12px;
    font-weight: 600;
}
QPushButton:hover {
    background: #d9ebff;
}
QPushButton#primaryButton {
    background: #2f80ed;
    color: #ffffff;
    border: 1px solid #2f80ed;
}
QPushButton#primaryButton:hover {
    background: #1f6fd0;
}
QPushButton#dangerButton {
    background: #fff0f0;
    color: #a33a3a;
    border: 1px solid #f0caca;
}
QPushButton#dangerButton:hover {
    background: #ffe1e1;
}
QTableWidget {
    background: #ffffff;
    border: 1px solid #dbe5ef;
    border-radius: 10px;
    gridline-color: #edf2f7;
    alternate-background-color: #f8fbff;
}
QHeaderView::section {
    background: #edf4fb;
    color: #24435c;
    border: none;
    border-right: 1px solid #dbe5ef;
    padding: 8px;
    font-weight: 700;
}
QTableWidget::item {
    padding: 6px;
}
QTableWidget::item:selected {
    background: #d9ebff;
    color: #12344d;
}
"""


class MainWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("FindFile")
        self.resize(1120, 760)
        self.icon_provider = QFileIconProvider()

        self.settings = load_settings()
        self.root_paths = self.settings.get("root_paths", [])
        self.active_root_path = self.settings.get("active_root_path", "")

        self.setStyleSheet(APP_STYLE)
        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(14, 14, 14, 14)
        main_layout.setSpacing(12)

        header = QFrame()
        header.setObjectName("card")
        header_layout = QVBoxLayout(header)
        title = QLabel("FindFile")
        title.setObjectName("titleLabel")
        subtitle = QLabel("Локальный быстрый поиск по независимым индексам выбранных папок")
        subtitle.setObjectName("subtitleLabel")
        header_layout.addWidget(title)
        header_layout.addWidget(subtitle)
        main_layout.addWidget(header)

        self.tabs = QTabWidget()
        main_layout.addWidget(self.tabs, 1)

        self.search_tab = QWidget()
        self.stats_tab = QWidget()
        self.tabs.addTab(self.search_tab, "🔎 Поиск")
        self.tabs.addTab(self.stats_tab, "📊 Статистика")

        self.build_search_tab()
        self.build_stats_tab()
        self.refresh_all()

    def build_search_tab(self):
        layout = QVBoxLayout(self.search_tab)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(10)

        controls_card = QFrame()
        controls_card.setObjectName("card")
        controls_layout = QVBoxLayout(controls_card)
        controls_layout.setSpacing(10)

        self.path_label = QLabel()
        self.path_label.setObjectName("pillLabel")
        self.path_label.setTextInteractionFlags(Qt.TextSelectableByMouse)

        path_layout = QHBoxLayout()
        self.root_combo = QComboBox()
        self.root_combo.setMinimumWidth(450)
        self.choose_btn = QPushButton("➕ Добавить папку")
        self.remove_btn = QPushButton("🗑 Удалить из списка")
        self.remove_btn.setObjectName("dangerButton")
        self.index_btn = QPushButton("🔄 Обновить индекс")
        self.index_btn.setObjectName("primaryButton")
        path_layout.addWidget(QLabel("Папка для обновления:"))
        path_layout.addWidget(self.root_combo, 1)
        path_layout.addWidget(self.choose_btn)
        path_layout.addWidget(self.remove_btn)
        path_layout.addWidget(self.index_btn)

        search_filter_layout = QHBoxLayout()
        self.search_scope_combo = QComboBox()
        self.search_scope_combo.setMinimumWidth(420)
        self.search = QLineEdit()
        self.search.setPlaceholderText("Введите часть имени файла или пути...")
        search_filter_layout.addWidget(QLabel("Искать в:"))
        search_filter_layout.addWidget(self.search_scope_combo)
        search_filter_layout.addWidget(self.search, 1)

        self.info_label = QLabel()
        self.info_label.setObjectName("subtitleLabel")
        self.info_label.setTextInteractionFlags(Qt.TextSelectableByMouse)

        controls_layout.addWidget(self.path_label)
        controls_layout.addLayout(path_layout)
        controls_layout.addLayout(search_filter_layout)
        controls_layout.addWidget(self.info_label)

        self.table = QTableWidget(0, 5)
        self.table.setHorizontalHeaderLabels(["Имя", "Источник", "Размер", "Изменён", "Путь"])
        self.table.setAlternatingRowColors(True)
        self.table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
        self.table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        self.table.setSortingEnabled(True)
        self.table.verticalHeader().setVisible(False)
        self.table.verticalHeader().setDefaultSectionSize(34)
        self.table.horizontalHeader().setStretchLastSection(True)
        self.table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        self.table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        self.table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeMode.ResizeToContents)
        self.table.horizontalHeader().setSectionResizeMode(3, QHeaderView.ResizeMode.ResizeToContents)
        self.table.horizontalHeader().setSectionResizeMode(4, QHeaderView.ResizeMode.Stretch)

        layout.addWidget(controls_card)
        layout.addWidget(self.table, 1)

        self.root_combo.currentTextChanged.connect(self.on_active_root_changed)
        self.search_scope_combo.currentTextChanged.connect(lambda _: self.update_results(self.search.text()))
        self.choose_btn.clicked.connect(self.choose_folder)
        self.remove_btn.clicked.connect(self.remove_selected_folder)
        self.index_btn.clicked.connect(self.reindex_files)
        self.search.textChanged.connect(self.update_results)
        self.table.itemDoubleClicked.connect(self.open_item)

    def build_stats_tab(self):
        layout = QVBoxLayout(self.stats_tab)
        layout.setContentsMargins(10, 10, 10, 10)
        self.stats_text = QTextEdit()
        self.stats_text.setReadOnly(True)
        font = QFont("Consolas")
        font.setStyleHint(QFont.StyleHint.Monospace)
        self.stats_text.setFont(font)
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
        db_status = "готов" if DB_PATH.exists() else "не создан"
        db_size = format_bytes(DB_PATH.stat().st_size) if DB_PATH.exists() else "0 Б"
        roots_count = len(self.root_paths)
        self.info_label.setText(
            f"Индекс: {db_status}, {db_size} | Папок в списке: {roots_count} | Настройки: {SETTINGS_PATH}"
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
            "Папка добавлена. Чтобы создать или обновить её независимый индекс, нажмите «Обновить индекс».",
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

    def icon_for_path(self, path: str):
        if os.path.isdir(path):
            return self.style().standardIcon(QStyle.StandardPixmap.SP_DirIcon)
        if os.path.exists(path):
            return self.icon_provider.icon(QFileInfo(path))

        ext = os.path.splitext(path)[1].lower()
        if ext in {".doc", ".docx", ".rtf"}:
            return self.style().standardIcon(QStyle.StandardPixmap.SP_FileDialogDetailedView)
        if ext in {".xls", ".xlsx", ".xlsm", ".csv"}:
            return self.style().standardIcon(QStyle.StandardPixmap.SP_FileDialogListView)
        if ext in {".pdf", ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".mp4", ".avi", ".mov"}:
            return self.style().standardIcon(QStyle.StandardPixmap.SP_FileIcon)
        return self.style().standardIcon(QStyle.StandardPixmap.SP_FileIcon)

    def update_results(self, text):
        self.table.setSortingEnabled(False)
        self.table.setRowCount(0)
        if not text:
            self.table.setSortingEnabled(True)
            return

        scope = self.search_scope_combo.currentText().strip()
        rows = search_files(text, scope)
        self.table.setRowCount(len(rows))

        for row, (name, path, root_path, size_bytes, modified_at) in enumerate(rows):
            size_text = format_bytes(size_bytes or 0)
            modified_text = "-"
            if modified_at:
                try:
                    from datetime import datetime
                    modified_text = datetime.fromtimestamp(float(modified_at)).strftime("%d.%m.%Y %H:%M")
                except Exception:
                    modified_text = "-"

            name_item = QTableWidgetItem(self.icon_for_path(path), name)
            name_item.setData(Qt.UserRole, path)
            root_item = QTableWidgetItem(root_path or "-")
            size_item = QTableWidgetItem(size_text)
            modified_item = QTableWidgetItem(modified_text)
            path_item = QTableWidgetItem(path)
            path_item.setToolTip(path)

            self.table.setItem(row, 0, name_item)
            self.table.setItem(row, 1, root_item)
            self.table.setItem(row, 2, size_item)
            self.table.setItem(row, 3, modified_item)
            self.table.setItem(row, 4, path_item)

        self.table.setSortingEnabled(True)

    def open_item(self, item):
        row = item.row()
        name_item = self.table.item(row, 0)
        path = name_item.data(Qt.UserRole) if name_item else None
        if not path or not os.path.exists(path):
            QMessageBox.warning(self, "Файл не найден", "Файл недоступен или был перемещён.")
            return
        QDesktopServices.openUrl(QUrl.fromLocalFile(path))


def run_app():
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    w = MainWindow()
    w.show()
    sys.exit(app.exec())
