import os
import sys
from PySide6.QtCore import Qt, QUrl
from PySide6.QtGui import QDesktopServices
from PySide6.QtWidgets import (
    QApplication,
    QWidget,
    QVBoxLayout,
    QPushButton,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QMessageBox,
)
from search.service import search_files
from indexer.scanner import reindex


class MainWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("FindFile")
        self.resize(900, 600)

        layout = QVBoxLayout(self)
        self.btn = QPushButton("Переиндексировать")
        self.search = QLineEdit()
        self.search.setPlaceholderText("Поиск по имени или пути")
        self.list = QListWidget()

        layout.addWidget(self.btn)
        layout.addWidget(self.search)
        layout.addWidget(self.list)

        self.btn.clicked.connect(self.reindex_files)
        self.search.textChanged.connect(self.update_results)
        self.list.itemDoubleClicked.connect(self.open_item)

    def reindex_files(self):
        try:
            count = reindex()
            QMessageBox.information(self, "FindFile", f"Индексация завершена. Файлов найдено: {count}")
            self.update_results(self.search.text())
        except Exception as exc:
            QMessageBox.critical(self, "Ошибка индексации", str(exc))

    def update_results(self, text):
        self.list.clear()
        if not text:
            return
        for name, path in search_files(text):
            item = QListWidgetItem(f"{name} | {path}")
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
