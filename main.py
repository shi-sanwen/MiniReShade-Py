"""
版权声明：
    本作品（包括但不限于文字、图片、音频、视频、设计元素等）的版权归作者“迷影、大喵工作室、Qichee”及“创梦星际”共同所有。未经作者及工作室明确书面授权，任何个人、组织或机构不得以任何形式复制、分发、传播、展示、修改、改编或以其他任何方式使用本作品的全部或部分内容。
作者及工作室保留对本作品的全部权利，包括但不限于著作权、商标权、专利权等。任何未经授权的使用行为均构成侵权，作者及工作室将依法追究侵权者的法律责任，并要求侵权者承担相应的赔偿责任。
如需获取本作品的授权使用，请与“迷影、大喵工作室、Qichee、创梦星际”工作室联系，工作室将根据具体情况决定是否授权以及授权的条件和范围。作者及工作室保留随时修改、更新或终止本版权声明的权利，而无需事先通知任何第三方。
作者及工作室对本版权声明拥有最终解释权。

联系方式：
工作室名称：创梦星际
地址：https://www.scmgzs.top/
QQ：2196634956
联系邮箱：2196634956@qq.com
"""


import os
import sys
import winreg
import subprocess
import logging
import traceback
import requests
import time
import win32com.client
import psutil
import ctypes
import shutil
from datetime import datetime
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
                             QPushButton, QLabel, QTextEdit, QProgressBar, QRadioButton,
                             QButtonGroup, QCheckBox, QFileDialog, QMessageBox)
from PyQt5.QtCore import Qt, QThread, pyqtSignal
from PyQt5.QtGui import QFont, QPalette, QColor


def setup_logging():
    # 获取脚本所在目录
    script_dir = os.path.dirname(os.path.abspath(__file__))

    # 创建 'logs' 目录（如果不存在）
    logs_dir = os.path.join(script_dir, 'logs')
    os.makedirs(logs_dir, exist_ok=True)

    # 生成带时间戳的唯一日志文件名
    current_time = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_file = os.path.join(logs_dir, f'installer_log_{current_time}.txt')

    # 配置日志记录
    logging.basicConfig(
        filename=log_file,
        level=logging.DEBUG,
        format='%(asctime)s - %(levelname)s - %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S'
    )

    # 添加控制台处理器以同时输出日志到控制台
    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.INFO)
    formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
    console_handler.setFormatter(formatter)
    logging.getLogger('').addHandler(console_handler)

    return log_file


def is_admin():
    try:
        return ctypes.windll.shell32.IsUserAnAdmin() != 0
    except:
        return False


if getattr(sys, 'frozen', False):
    application_path = sys._MEIPASS
else:
    application_path = os.path.dirname(os.path.abspath(__file__))
# UI相关常量定义
WINDOW_WIDTH = 800
WINDOW_HEIGHT = 600
BUTTON_HEIGHT = 30
PADDING = 10
FONT_SIZE = 10

# UI主题颜色
COLORS = {
    'primary': '#2196F3',
    'secondary': '#757575',
    'background': '#FFFFFF',
    'text': '#000000',
    'success': '#4CAF50',
    'error': '#F44336'
}

# UI样式表
STYLE_SHEET = f"""
QPushButton {{
    background-color: {COLORS['primary']};
    color: white;
    border: none;
    padding: 5px;
    min-height: {BUTTON_HEIGHT}px;
}}

QPushButton:hover {{
    background-color: {COLORS['secondary']};
}}

QProgressBar {{
    border: 2px solid {COLORS['secondary']};
    border-radius: 5px;
    text-align: center;
}}

QProgressBar::chunk {{
    background-color: {COLORS['primary']};
}}
"""

# UI工具函数
def create_button(text, parent=None):
    """创建统一样式的按钮"""
    button = QPushButton(text, parent)
    button.setFont(QFont('楷体', FONT_SIZE))
    return button

def create_label(text, parent=None):
    """创建统一样式的标签"""
    label = QLabel(text, parent)
    label.setFont(QFont('楷体', FONT_SIZE))
    return label

def show_message(parent, title, text, icon=QMessageBox.Information):
    """显示统一样式的消息框"""
    msg = QMessageBox(parent)
    msg.setWindowTitle(title)
    msg.setText(text)
    msg.setIcon(icon)
    msg.setFont(QFont('楷体', FONT_SIZE))
    return msg.exec_()


class InstallerWorker(QThread):
    progress_update = pyqtSignal(str)
    installation_complete = pyqtSignal(bool)

    def __init__(self, install_path, zip_path):
        super().__init__()
        self.install_path = install_path
        self.zip_path = zip_path

    def run(self):
        self.progress_update.emit("开始安装...")
        logging.info("开始安装...")
        if self.extract_zip_with_7z(self.zip_path, self.install_path):
            self.progress_update.emit("安装完成")
            logging.info("安装完成")
            self.installation_complete.emit(True)
        else:
            self.progress_update.emit("安装失败")
            logging.error("安装失败")
            self.installation_complete.emit(False)

    def extract_zip_with_7z(self, zip_path, extract_path):
        self.progress_update.emit(f"开始解压缩 {zip_path} 到 {extract_path}...")
        logging.info(f"开始解压缩 {zip_path} 到 {extract_path}...")
        if not os.path.exists(zip_path):
            self.progress_update.emit(f"ZIP文件不存在: {zip_path}")
            logging.error(f"ZIP文件不存在: {zip_path}")
            return False

        if not os.path.exists(extract_path):
            os.makedirs(extract_path)

        seven_zip_path = os.path.join(application_path, '7z', '7z.exe')
        if not os.path.exists(seven_zip_path):
            self.progress_update.emit(f"7z.exe not found at {seven_zip_path}")
            logging.error(f"7z.exe not found at {seven_zip_path}")
            return False

        max_attempts = 3
        for attempt in range(max_attempts):
            try:
                process = subprocess.Popen([seven_zip_path, 'x', zip_path, f'-o{extract_path}', '-y'],
                                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)

                while True:
                    output = process.stdout.readline()
                    if output == '' and process.poll() is not None:
                        break
                    if output:
                        self.progress_update.emit(output.strip())
                        logging.debug(output.strip())

                returncode = process.wait()
                if returncode != 0:
                    self.progress_update.emit(f"7z extraction failed with return code {returncode}")
                    logging.error(f"7z extraction failed with return code {returncode}")
                    if attempt < max_attempts - 1:
                        self.progress_update.emit(f"等待3秒后重试... (尝试 {attempt + 2}/{max_attempts})")
                        logging.info(f"等待3秒后重试... (尝试 {attempt + 2}/{max_attempts})")
                        time.sleep(3)
                    else:
                        return False
                else:
                    self.progress_update.emit("解压缩完成！")
                    logging.info("解压缩完成！")
                    return True
            except Exception as e:
                self.progress_update.emit(f"解压缩过程中出错: {str(e)}")
                logging.exception(f"解压缩过程中出错: {str(e)}")
                if attempt < max_attempts - 1:
                    self.progress_update.emit(f"等待3秒后重试... (尝试 {attempt + 2}/{max_attempts})")
                    logging.info(f"等待3秒后重试... (尝试 {attempt + 2}/{max_attempts})")
                    time.sleep(3)
                else:
                    return False

    def verify_installation(self, path):
        required_files = ['reshade-shaders']  # 只检查必要的着色器文件
        for file in required_files:
            if not os.path.exists(os.path.join(path, file)):
                return False
        return True


class ScanWorker(QThread):
    progress_update = pyqtSignal(str)
    version_found = pyqtSignal(str, str)
    scan_complete = pyqtSignal()

    def __init__(self):
        super().__init__()
        self.scanning = True
        self.scan_paused = False
        self.versions_found = []

    def run(self):
        self.scan_for_micromininew()

    def scan_for_micromininew(self):
        # 首先尝试使用Windows索引搜索
        if self.windows_index_search():
            self.scan_complete.emit()
            return

        # 如果Windows索引搜索失败或没有找到结果，进行全盘扫描
        self.full_disk_scan()

        if not self.versions_found:
            self.progress_update.emit("未找到MicroMiniNew.exe")
            logging.info("未找到MicroMiniNew.exe")

        self.scan_complete.emit()

    def windows_index_search(self):
        try:
            connection = win32com.client.Dispatch("ADODB.Connection")
            connection.Open("Provider=Search.CollatorDSO;Extended Properties='Application=Windows';")
            recordset = win32com.client.Dispatch("ADODB.Recordset")
            recordset.Open("SELECT System.ItemPathDisplay FROM SystemIndex WHERE System.FileName = '启动器.exe'",
                           connection)

            while not recordset.EOF:
                path = recordset.Fields("System.ItemPathDisplay").Value
                version = f"未知版本 (路径: {os.path.dirname(path)})"
                self.versions_found.append((version, os.path.dirname(path)))
                self.version_found.emit(version, os.path.dirname(path))
                self.progress_update.emit(f"检测到: {version}")
                logging.info(f"检测到: {version}")
                recordset.MoveNext()

            recordset.Close()
            connection.Close()

            return bool(self.versions_found)
        except Exception as e:
            self.progress_update.emit(f"Windows索引搜索失败: {str(e)}")
            logging.exception(f"Windows索引搜索失败: {str(e)}")
            return False

    def full_disk_scan(self):
        self.progress_update.emit("开始全盘扫描...")
        logging.info("开始全盘扫描...")
        excluded_folders = ['$Recycle.Bin', 'System Volume Information', 'Windows', 'Program Files',
                            'Program Files (x86)', 'ProgramData', 'temp', 'tmp']
        self.progress_update.emit(f"排除的文件夹: {', '.join(excluded_folders)}，以及路径中包含 'Temp' 的文件夹")
        logging.info(f"排除的文件夹: {', '.join(excluded_folders)}，以及路径中包含 'Temp' 的文件夹")
        for drive in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ':
            drive_path = f"{drive}:\\"
            if os.path.exists(drive_path):
                self.progress_update.emit(f"正在扫描驱动器 {drive}...")
                logging.info(f"正在扫描驱动器 {drive}...")
                for root, dirs, files in os.walk(drive_path):
                    while self.scan_paused:
                        time.sleep(0.1)
                    if not self.scanning:
                        return

                    dirs[:] = [d for d in dirs if d not in excluded_folders and not d.startswith('$')]

                    if "启动器.exe" in files:
                        path = root
                        version = f"未知版本 (路径: {path})"
                        self.versions_found.append((version, path))
                        self.version_found.emit(version, path)
                        self.progress_update.emit(f"检测到: {version}")
                        logging.info(f"检测到: {version}")


class InstallerGUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("迷你世界光影包安装器V2.6.31")
        self.setGeometry(100, 100, 1200, 700)
        self.setup_ui()

        self.detected_versions = {}
        self.scanning = False
        self.scan_paused = False
        self.scan_start_time = None
        self.failed_installations = []
        self.retry_count = 0
        self.windows_version = self.get_windows_version()

        # 优化系统检测提示文字
        if self.windows_version < 6.1:
            QMessageBox.warning(
                self,
                "系统兼容性提示",
                "检测到您正在使用较早版本的Windows系统。\n"
                "为了确保最佳体验，部分高级功能将自动调整为兼容模式。\n"
                "基础功能不受影响，请放心使用。"
            )
            logging.info("系统版本较低，已启用兼容模式")

    def setup_ui(self):
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        main_layout = QHBoxLayout()
        main_widget.setLayout(main_layout)

        left_panel = QWidget()
        left_layout = QVBoxLayout()
        left_panel.setLayout(left_layout)
        main_layout.addWidget(left_panel)

        right_panel = QWidget()
        right_layout = QVBoxLayout()
        right_panel.setLayout(right_layout)
        main_layout.addWidget(right_panel)

        self.create_copyright_notice(left_layout)
        self.create_update_log(left_layout)
        self.create_versions_frame(left_layout)
        self.create_preset_frame(left_layout)
        self.create_scan_controls(left_layout)

        self.create_output_text(right_layout)
        self.create_progress_bar(right_layout)

        self.set_water_ink_theme()

    def set_water_ink_theme(self):
        palette = self.palette()
        palette.setColor(QPalette.Window, QColor("#F0F8FF"))
        palette.setColor(QPalette.WindowText, QColor("#000080"))
        palette.setColor(QPalette.Button, QColor("#E6F3FF"))
        palette.setColor(QPalette.ButtonText, QColor("#000000"))
        self.setPalette(palette)

        # 将默认字体改为楷体
        font = QFont("楷体", 10)
        QApplication.setFont(font)

        self.setStyleSheet("""
            QWidget {
                font-family: 楷体;
            }
            QPushButton {
                background-color: #E6F3FF;
                color: black;
                border: 1px solid #4682B4;
                padding: 5px 10px;
                border-radius: 3px;
                font-family: 楷体;
            }
            QPushButton:hover {
                background-color: #D0E8FF;
                border: 1px solid #5c9fd6;
            }
            QPushButton:pressed {
                background-color: #B8DCFF;
                border: 1px solid #37698e;
            }
            QPushButton:disabled {
                background-color: #F0F0F0;
                color: #A0A0A0;
                border: 1px solid #C0C0C0;
            }
            QLabel {
                font-family: 楷体;
            }
            QTextEdit {
                font-family: 楷体;
            }
        """)

    def create_copyright_notice(self, layout):
        copyright_label = QLabel("版权声明")
        copyright_label.setStyleSheet("font-weight: bold; color: #000080; font-size: 12px;")
        layout.addWidget(copyright_label)

        copyright_text = QTextEdit()
        copyright_text.setPlainText(
            "版权声明： \n \n本作品（包括但不限于文字、图片、音频、视频、设计元素等）的版权归作者“迷影、大喵工作室、Qichee”及“创梦星际”共同所有。未经作者及工作室明确书面授权，任何个人、组织或机构不得以任何形式复制、分发、传播、展示、修改、改编或以其他任何方式使用本作品的全部或部分内容。作者及工作室保留对本作品的全部权利，包括但不限于著作权、商标权、专利权等。任何未经授权的使用行为均构成侵权，作者及工作室将依法追究侵权者的法律责任，并要求侵权者承担相应的赔偿责任。如需获取本作品的授权使用，请与“迷影、大喵工作室、Qichee、创梦星际”工作室联系，工作室将根据具体情况决定是否授权以及授权的条件和范围。作者及工作室保留随时修改、更新或终止本版权声明的权利，而无需事先通知任何第三方。作者及工作室对本版权声明拥有最终解释权。\n \n联系方式：\n工作室名称：创梦星际\n地址：https://www.scmgzs.top/\n联系邮箱：ssw2196634956@outlook.com")
        copyright_text.setReadOnly(True)
        copyright_text.setMaximumHeight(100)
        copyright_text.setStyleSheet("background-color: #E6F3FF; color: #000080; border: 1px solid #4682B4;")
        layout.addWidget(copyright_text)

    def create_update_log(self, layout):
        update_label = QLabel("更新日志")
        update_label.setStyleSheet("font-weight: bold; color: #000080; font-size: 12px;")
        layout.addWidget(update_label)

        update_text = QTextEdit()
        update_text.setPlainText(
            "## 更新日志\n \n### 版本 2.6.31（2025年1月）\n- **优化**:\n - 优化代码架构，更好的兼容win7系统运行。\n - 优化UI界面，使用pyqt5开发。\n- **修复**：\n - 修复过ACE，使用天梦零惜大佬制作。\n - 修复扫盘寻找问题。\n### 版本 2.6.24 (2023年10月)\n- **新增功能**:\n - 添加了对新游戏版本的支持。\n  - 增加了用户反馈功能，用户可以直接在应用内提交反馈。\n \n- **优化**:\n  - 优化了安装过程中的文件解压速度。\n  - 改进了UI界面，使其更加友好和直观。\n \n- **修复**:\n  - 修复了在某些情况下快捷方式创建失败的问题。\n  - 修复了安装过程中可能出现的崩溃问题。\n \n### 版本 2.6.23 (2023年9月)\n- **更新**:\n  - 更新了UI，界面更加简洁。\n  - 优化了运行架构，提升了性能。\n \n- **功能增强**:\n  - 支持4399和7k7k的反ACE。\n  - 添加了安装失败检测和自动重试功能。\n \n- **修复**:\n  - 修复了过ACE启动器闪退问题。\n  - 改进了全盘扫描功能。\n \n### 版本 2.6.22 (2023年8月)\n- **优化**:\n  - 优化了日志记录功能，增加了更多的调试信息。\n  - 改进了用户界面，提升了用户体验。\n \n- **修复**:\n  - 修复了某些情况下无法检测到游戏版本的问题。")
        update_text.setReadOnly(True)
        update_text.setMaximumHeight(100)
        update_text.setStyleSheet("background-color: #E6F3FF; color: #000080; border: 1px solid #4682B4;")
        layout.addWidget(update_text)

    def create_versions_frame(self, layout):
        versions_label = QLabel("检测到的游戏版本")
        versions_label.setStyleSheet("font-weight: bold; color: #000080; font-size: 12px;")
        layout.addWidget(versions_label)

        self.versions_group = QButtonGroup(self)
        self.versions_layout = QVBoxLayout()
        versions_widget = QWidget()
        versions_widget.setLayout(self.versions_layout)
        layout.addWidget(versions_widget)

        add_custom_button = QPushButton("添加自定义安装")
        add_custom_button.clicked.connect(self.add_custom_path)
        layout.addWidget(add_custom_button)

    def create_preset_frame(self, layout):
        preset_label = QLabel("预设下载选项")
        preset_label.setStyleSheet("font-weight: bold; color: #000080; font-size: 12px;")
        layout.addWidget(preset_label)

        self.yirixc_checkbox = QCheckBox("预设制作：YiRixc")
        self.yirixc_checkbox.setStyleSheet("color: #000080;")
        layout.addWidget(self.yirixc_checkbox)

        self.tianxingdashe_checkbox = QCheckBox("预设制作：天星大蛇")
        self.tianxingdashe_checkbox.setStyleSheet("color: #000080;")
        layout.addWidget(self.tianxingdashe_checkbox)

        download_preset_button = QPushButton("下载预设")
        download_preset_button.clicked.connect(self.download_presets)
        layout.addWidget(download_preset_button)

    def create_scan_controls(self, layout):
        scan_layout = QHBoxLayout()

        self.start_scan_button = QPushButton("开始扫描")
        self.start_scan_button.clicked.connect(self.start_scan)
        scan_layout.addWidget(self.start_scan_button)

        self.pause_scan_button = QPushButton("暂停扫描")
        self.pause_scan_button.clicked.connect(self.pause_scan)
        self.pause_scan_button.setEnabled(False)
        scan_layout.addWidget(self.pause_scan_button)

        self.stop_scan_button = QPushButton("停止扫描")
        self.stop_scan_button.clicked.connect(self.stop_scan)
        self.stop_scan_button.setEnabled(False)
        scan_layout.addWidget(self.stop_scan_button)

        self.scan_progress_label = QLabel("")
        self.scan_progress_label.setStyleSheet("color: #000080;")
        scan_layout.addWidget(self.scan_progress_label)

        self.install_button = QPushButton("安装")
        self.install_button.clicked.connect(self.install_action)
        scan_layout.addWidget(self.install_button)

        layout.addLayout(scan_layout)

    def create_output_text(self, layout):
        output_label = QLabel("输出日志")
        output_label.setStyleSheet("font-weight: bold; color: #000080; font-size: 12px;")
        layout.addWidget(output_label)

        self.output_text = QTextEdit()
        self.output_text.setReadOnly(True)
        self.output_text.setStyleSheet("background-color: #E6F3FF; color: #000080; border: 1px solid #4682B4;")
        layout.addWidget(self.output_text)

    def create_progress_bar(self, layout):
        self.progress_bar = QProgressBar()
        self.progress_bar.setStyleSheet("""
            QProgressBar {
                border: 1px solid #4682B4;
                border-radius: 5px;
                text-align: center;
            }
            QProgressBar::chunk {
                background-color: #4682B4;
            }
        """)
        layout.addWidget(self.progress_bar)

    def add_custom_path(self):
        path = QFileDialog.getExistingDirectory(self, "选择安装目录")
        if path:
            version = f"自定义路径: {path}"
            self.detected_versions[version] = path
            self.populate_options()
        logging.info(f"添加自定义路径: {path}")

    def populate_options(self):
        for i in reversed(range(self.versions_layout.count())):
            self.versions_layout.itemAt(i).widget().setParent(None)

        for version, path in self.detected_versions.items():
            radio_button = QRadioButton(version)
            radio_button.setStyleSheet("color: #000080;")
            self.versions_group.addButton(radio_button)
            self.versions_layout.addWidget(radio_button)

        if self.detected_versions:
            self.versions_group.buttons()[0].setChecked(True)

        logging.info(f"更新检测到的版本列表: {self.detected_versions}")

    def download_presets(self):
        selected_button = self.versions_group.checkedButton()
        if not selected_button:
            QMessageBox.warning(self, "警告", "请先选择一个游戏版本")
            logging.warning("尝试下载预设时未选择游戏版本")
            return

        version = selected_button.text()
        install_path = self.detected_versions[version]

        if self.yirixc_checkbox.isChecked():
            self.download_yirixc_presets(install_path)

        if self.tianxingdashe_checkbox.isChecked():
            self.download_tianxingdashe_presets(install_path)

    def download_yirixc_presets(self, install_path):
        url = "https://scm.aug.lol/preset/YiRixc/YiRixc.zip"
        self.download_and_extract_preset(url, install_path, "YiRixc")

    def download_tianxingdashe_presets(self, install_path):
        url = "https://scm.aug.lol/preset/StarSerpent/StarSerpent.zip"
        self.download_and_extract_preset(url, install_path, "StarSerpent")

    def download_and_extract_preset(self, url, install_path, preset_name):
        preset_path = os.path.join(install_path, "presets")
        os.makedirs(preset_path, exist_ok=True)
        zip_path = os.path.join(preset_path, f"{preset_name}.zip")

        try:
            response = requests.get(url, stream=True)
            total_size = int(response.headers.get('content-length', 0))
            block_size = 1024
            downloaded = 0

            with open(zip_path, 'wb') as file:
                for data in response.iter_content(block_size):
                    size = file.write(data)
                    downloaded += size
                    self.progress_bar.setValue(int(downloaded / total_size * 100))

            if self.extract_zip_with_7z(zip_path, preset_path):
                os.remove(zip_path)
                self.log_output(f"{preset_name} 预设下载并解压完成")
                logging.info(f"{preset_name} 预设下载并解压完成")
            else:
                self.log_output(f"{preset_name} 预设解压失败")
                logging.error(f"{preset_name} 预设解压失败")
        except Exception as e:
            self.log_output(f"下载 {preset_name} 预设时出错: {str(e)}")
            logging.exception(f"下载 {preset_name} 预设时出错: {str(e)}")

    def extract_zip_with_7z(self, zip_path, extract_path):
        self.log_output(f"开始解压缩 {zip_path} 到 {extract_path}...")
        logging.info(f"开始解压缩 {zip_path} 到 {extract_path}...")
        if not os.path.exists(zip_path):
            self.log_output(f"ZIP文件不存在: {zip_path}")
            logging.error(f"ZIP文件不存在: {zip_path}")
            return False

        if not os.path.exists(extract_path):
            os.makedirs(extract_path)

        seven_zip_path = os.path.join(application_path, '7z', '7z.exe')
        if not os.path.exists(seven_zip_path):
            self.log_output(f"7z.exe not found at {seven_zip_path}")
            logging.error(f"7z.exe not found at {seven_zip_path}")
            return False

        max_attempts = 3
        for attempt in range(max_attempts):
            try:
                process = subprocess.Popen([seven_zip_path, 'x', zip_path, f'-o{extract_path}', '-y'],
                                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)

                while True:
                    output = process.stdout.readline()
                    if output == '' and process.poll() is not None:
                        break
                    if output:
                        self.log_output(output.strip())
                        logging.debug(output.strip())

                returncode = process.wait()
                if returncode != 0:
                    self.log_output(f"7z extraction failed with return code {returncode}")
                    logging.error(f"7z extraction failed with return code {returncode}")
                    if attempt < max_attempts - 1:
                        self.log_output(f"等待3秒后重试... (尝试 {attempt + 2}/{max_attempts})")
                        logging.info(f"等待3秒后重试... (尝试 {attempt + 2}/{max_attempts})")
                        time.sleep(3)
                    else:
                        return False
                else:
                    self.log_output("解压缩完成！")
                    logging.info("解压缩完成！")
                    return True
            except Exception as e:
                self.log_output(f"解压缩过程中出错: {str(e)}")
                logging.exception(f"解压缩过程中出错: {str(e)}")
                if attempt < max_attempts - 1:
                    self.log_output(f"等待3秒后重试... (尝试 {attempt + 2}/{max_attempts})")
                    logging.info(f"等待3秒后重试... (尝试 {attempt + 2}/{max_attempts})")
                    time.sleep(3)
                else:
                    return False

    def start_scan(self):
        self.scanning = True
        self.scan_paused = False
        self.start_scan_button.setEnabled(False)
        self.pause_scan_button.setEnabled(True)
        self.stop_scan_button.setEnabled(True)
        self.scan_start_time = time.time()

        self.scan_worker = ScanWorker()
        self.scan_worker.progress_update.connect(self.log_output)
        self.scan_worker.version_found.connect(self.add_detected_version)
        self.scan_worker.scan_complete.connect(self.on_scan_complete)

        try:
            self.scan_worker.start()
            logging.info("开始扫描")
        except Exception as e:
            self.log_output(f"扫描启动失败: {str(e)}")
            logging.exception(f"扫描启动失败: {str(e)}")
            self.on_scan_complete()

    def pause_scan(self):
        if not self.scanning:
            return
        self.scan_paused = not self.scan_paused
        self.scan_worker.scan_paused = self.scan_paused
        if self.scan_paused:
            self.pause_scan_button.setText("继续扫描")
            self.log_output("扫描已暂停")
            logging.info("扫描已暂停")
        else:
            self.pause_scan_button.setText("暂停扫描")
            self.log_output("扫描已继续")
            logging.info("扫描已继续")

    def stop_scan(self):
        if not self.scanning:
            return
        self.scanning = False
        self.scan_worker.scanning = False
        self.start_scan_button.setEnabled(True)
        self.pause_scan_button.setEnabled(False)
        self.stop_scan_button.setEnabled(False)
        if self.scan_start_time:
            elapsed_time = time.time() - self.scan_start_time
            hours, rem = divmod(elapsed_time, 3600)
            minutes, seconds = divmod(rem, 60)
            time_str = f"{int(hours):02d}:{int(minutes):02d}:{int(seconds):02d}"
            self.scan_progress_label.setText(f"用户终止扫描，总用时: {time_str}")
            self.log_output(f"扫描已终止，总用时: {time_str}")
            logging.info(f"扫描已终止，总用时: {time_str}")
        else:
            self.scan_progress_label.setText("扫描未开始")
            self.log_output("扫描未开始就被终止")
            logging.info("扫描未开始就被终止")

    def on_scan_complete(self):
        self.scanning = False
        self.start_scan_button.setEnabled(True)
        self.pause_scan_button.setEnabled(False)
        self.stop_scan_button.setEnabled(False)
        if self.scan_start_time:
            elapsed_time = time.time() - self.scan_start_time
            hours, rem = divmod(elapsed_time, 3600)
            minutes, seconds = divmod(rem, 60)
            time_str = f"{int(hours):02d}:{int(minutes):02d}:{int(seconds):02d}"
            self.scan_progress_label.setText(f"扫描完成，总用时: {time_str}")
            self.log_output(f"扫描完成，总用时: {time_str}")
            logging.info(f"扫描完成，总用时: {time_str}")
        else:
            self.scan_progress_label.setText("扫描完成")
            self.log_output("扫描完成")
            logging.info("扫描完成")
        self.populate_options()

    def add_detected_version(self, version, path):
        self.detected_versions[version] = path
        self.log_output(f"添加检测到的版本: {version} 在 {path}")
        logging.info(f"添加检测到的版本: {version} 在 {path}")

    def install_action(self):
        selected_button = self.versions_group.checkedButton()
        if not selected_button:
            QMessageBox.warning(self, "警告", "请先选择一个游戏版本")
            logging.warning("尝试安装时未选择游戏版本")
            return

        version = selected_button.text()
        install_path = self.detected_versions[version]

        running_processes = self.check_running_processes(install_path)
        if running_processes:
            process_list = ", ".join(running_processes)
            reply = QMessageBox.question(self, "警告",
                                         f"以下进程正在运行：{process_list}\n是否结束这些进程并继续安装？",
                                         QMessageBox.Yes | QMessageBox.No)
            if reply == QMessageBox.Yes:
                for process in psutil.process_iter(['name', 'exe']):
                    try:
                        if process.info['exe'] and process.info['exe'].startswith(install_path):
                            process.terminate()
                    except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
                        pass
                self.log_output("等待3秒以确保进程完全终止...")
                time.sleep(3)
            else:
                return

        self.start_installation(install_path)

    def check_running_processes(self, install_path):
        running_processes = []
        for process in psutil.process_iter(['name', 'exe']):
            try:
                if process.info['exe'] and process.info['exe'].startswith(install_path):
                    running_processes.append(process.info['name'])
            except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
                pass
        return running_processes

    def start_installation(self, install_path):
        zip_path = os.path.join(application_path, 'reshade', 'MiNi-reshade.zip')
        self.log_output(f"设置 zip 文件路径为: {zip_path}")

        if not os.path.exists(zip_path):
            self.log_output(f"ZIP文件不存在: {zip_path}")
            QMessageBox.critical(self, "错误", f"ZIP文件不存在: {zip_path}")
            return

        self.install_worker = InstallerWorker(install_path, zip_path)
        self.install_worker.progress_update.connect(self.log_output)
        self.install_worker.installation_complete.connect(self.on_installation_complete)
        self.install_worker.start()

    def on_installation_complete(self, success):
        if success:
            self.log_output("安装完成")
            QMessageBox.information(self, "完成", "安装完成")
        else:
            self.log_output("安装失败")
            reply = QMessageBox.question(self, "安装失败",
                                         "安装失败。是否要重启系统并尝试重新安装？",
                                         QMessageBox.Yes | QMessageBox.No)
            if reply == QMessageBox.Yes:
                self.schedule_reboot_and_install()

    def schedule_reboot_and_install(self):
        key = winreg.CreateKey(winreg.HKEY_CURRENT_USER, r"Software\Microsoft\Windows\CurrentVersion\RunOnce")
        winreg.SetValueEx(key, "MiniWorldInstaller", 0, winreg.REG_SZ, sys.executable)
        os.system("shutdown /r /t 1")

    def log_output(self, message):
        self.output_text.append(message)

    def run(self):
        if not is_admin():
            self.log_output("不是管理员，尝试以管理员身份重新启动...")
            ctypes.windll.shell32.ShellExecuteW(None, "runas", sys.executable, " ".join(sys.argv), None, 1)
            sys.exit(0)

        self.log_output("检查是否以管理员身份运行...")
        self.log_output("开始检测游戏版本...")
        self.check_all_versions()
        self.show()

    def check_all_versions(self):
        self.detected_versions = {}
        users_folder = os.path.expanduser("~") 

        registry_path = r"SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\官方版迷你世界"
        official_path = self.check_registry_and_extract(registry_path)
        if official_path:
            self.detected_versions["官方版迷你世界"] = official_path
            self.log_output("检测到: 官方版迷你世界")

        for user in os.listdir(users_folder):
            user_folder_path = os.path.join(users_folder, user)
            paths = {
                "4399版迷你世界": os.path.join(user_folder_path, "AppData", "Roaming", "miniworldgame4399"),
                "7k7k版迷你世界": os.path.join(user_folder_path, "AppData", "Roaming", "miniworldgame7k7k"),
                "迷你世界国际版": os.path.join(user_folder_path, "AppData", "Roaming", "miniworldOverseasgame"),
            }

            for version, path in paths.items():
                if os.path.exists(path):
                    self.detected_versions[f"{version} (用户: {user})"] = path
                    self.log_output(f"检测到: {version} 在用户 {user} 的文件夹中")

        if not self.detected_versions:
            reply = QMessageBox.question(self, "未检测到安装路径",
                                         "是否进行全盘扫描寻找迷你世界？",
                                         QMessageBox.Yes | QMessageBox.No)
            if reply == QMessageBox.Yes:
                self.start_scan()

        self.log_output(f"检测到的版本: {self.detected_versions}")
        self.populate_options()

    def check_registry_and_extract(self, registry_key_path):
        try:
            if self.windows_version <= 6.1:
                access_flag = winreg.KEY_READ
            else:
                access_flag = winreg.KEY_READ | winreg.KEY_WOW64_32KEY

            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, registry_key_path, 0, access_flag) as key:
                publisher = winreg.QueryValueEx(key, "Publisher")[0]
                folder_path = publisher.replace("迷你玩科技有限公司", "").strip()
                if folder_path:
                    return folder_path
                else:
                    self.log_output("发布者路径无效。")
        except FileNotFoundError:
            pass
        except Exception as e:
            self.log_output(f"访问注册表时出错: {e}")
        return None

    def get_windows_version(self):
        try:
            import platform
            return float(platform.version().split('.')[0])
        except:
            return 6.1  # Windows 7 默认版本号


if __name__ == "__main__":
    try:
        desktop_path = win32com.client.Dispatch("WScript.Shell").SpecialFolders("Desktop")
        log_path = os.path.join(desktop_path, "installer_log.txt")
        logging.basicConfig(filename=log_path, level=logging.DEBUG,
                            format='%(asctime)s - %(levelname)s - %(message)s')
        app = QApplication(sys.argv)
        installer = InstallerGUI()
        installer.run()
        sys.exit(app.exec())
    except Exception as e:
        error_msg = f"程序发生未预期的错误: {str(e)}\n{traceback.format_exc()}"
        print(error_msg)
        logging.error(error_msg)
        QMessageBox.critical(None, "错误", error_msg)



# PyInstaller --onefile --debug=all --windowed --clean --noconsole --name 光影包安装器x86x64 --icon="D:\programming\一键安装包\main-ico.ico" --add-data "reshade\MiNi-reshade.zip;reshade" --add-data "7z\7z.exe;7z" --add-data "7z\7z.dll;7z" --hidden-import tqdm --hidden-import win32com.client --version-file main.txt main.py

