
import os
from PySide6.QtWidgets import QApplication, QWidget, QVBoxLayout, QPushButton, QLineEdit, QListWidget, QListWidgetItem
from search.service import search_files
from indexer.scanner import reindex

class MainWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("FindFile")
        self.resize(900,600)
        layout = QVBoxLayout(self)
        self.btn = QPushButton("Переиндексировать")
        self.search = QLineEdit()
        self.search.setPlaceholderText("Поиск по имени или пути")
        self.list = QListWidget()
        layout.addWidget(self.btn)
        layout.addWidget(self.search)
        layout.addWidget(self.list)
        self.btn.clicked.connect(reindex)
        self.search.textChanged.connect(self.update_results)
        self.list.itemDoubleClicked.connect(self.open_item)

    def update_results(self, text):
        self.list.clear()
        if not text:
            return
        for name, path in search_files(text):
            item = QListWidgetItem(f"{name} | {path}")
            item.setData(256, path)
            self.list.addItem(item)

    def open_item(self, item):
        path = item.data(256)
        if os.path.exists(path):
            os.startfile(path)

def run_app():
    app = QApplication([])
    w = MainWindow()
    w.show()
    app.exec()
