# -*- coding: utf-8 -*-
"""
Git 初始化窗口工具（tkinter）
- 内置 GitHub / Gitee 两套默认 URL
- 支持自定义 URL
- user.name / user.email 提供默认值并支持修改
"""

import os
import json
import subprocess
import platform
import socket
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
from urllib.parse import urlparse

DEFAULT_USER_NAME = "hookby"
DEFAULT_USER_EMAIL = "hookby@gmail.com"

DEFAULT_URLS = {
    "github": "https://github.com/hookby",
    "gitee": "https://gitee.com/hookby",
}

STATE_FILE_NAME = ".github_init_state.json"

IS_WINDOWS = platform.system() == "Windows"


def run_command(command, cwd):
    result = subprocess.run(
        command,
        cwd=cwd,
        shell=IS_WINDOWS,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    return result.returncode == 0, (result.stdout or ""), (result.stderr or "")


def ensure_readme(project_dir, project_name):
    readme_path = os.path.join(project_dir, "README.md")
    if os.path.exists(readme_path):
        return True, "README.md 已存在"

    content = f"# {project_name}\n\n## Description\n"
    with open(readme_path, "w", encoding="utf-8") as file:
        file.write(content)
    return True, "README.md 已创建"


def build_remote_url(base_url, project_name):
    base = (base_url or "").strip().rstrip("/")
    if not base:
        return ""

    if base.endswith(".git"):
        return base

    parts = [p for p in base.split("/") if p]
    # 支持直接输入完整仓库地址：
    # https://github.com/hookby/BinFontLib
    # https://gitee.com/hookby/BinFontLib
    if len(parts) >= 4 and parts[-1] and parts[-2]:
        host = parts[-3]
        if host in ("github.com", "gitee.com"):
            return base + ".git"

    return f"{base}/{project_name}.git"


def extract_host(url):
    value = (url or "").strip()
    if not value:
        return ""
    if not value.startswith("http://") and not value.startswith("https://"):
        value = "https://" + value
    parsed = urlparse(value)
    return parsed.hostname or ""


def test_host_443(host, timeout=5):
    if not host:
        return False, "host 为空"
    try:
        with socket.create_connection((host, 443), timeout=timeout):
            return True, "连接成功"
    except Exception as ex:
        return False, str(ex)


class GitInitWindow:
    def __init__(self, root):
        self.root = root
        self.root.title("GitHub/Gitee 初始化工具")
        self.root.geometry("760x500")
        self.root.minsize(720, 460)

        cwd = os.getcwd()
        self.project_dir_var = tk.StringVar(value=cwd)
        self.project_name_var = tk.StringVar(value=os.path.basename(cwd.rstrip("\\/")))
        self.user_name_var = tk.StringVar(value=DEFAULT_USER_NAME)
        self.user_email_var = tk.StringVar(value=DEFAULT_USER_EMAIL)

        self.repo_type_var = tk.StringVar(value="github")
        self.base_url_var = tk.StringVar(value=DEFAULT_URLS["github"])
        self.push_var = tk.BooleanVar(value=True)

        self.status_text_var = tk.StringVar(value="状态：未检查")
        self.status_color = "#cc3333"

        self._load_state()
        self._build_ui()
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)
        self.root.bind("<Configure>", self._on_resize)
        self._draw_decoration()
        self.check_init_status(show_dialog=False)

    def _build_ui(self):
        container = ttk.Frame(self.root, padding=8)
        container.pack(fill=tk.BOTH, expand=True)

        header = ttk.Frame(container)
        header.pack(fill=tk.X, pady=(0, 6))

        title = ttk.Label(header, text="Git 初始化助手", font=("Microsoft YaHei UI", 11, "bold"))
        title.pack(side=tk.LEFT)

        self.status_canvas = tk.Canvas(header, width=14, height=14, highlightthickness=0)
        self.status_canvas.pack(side=tk.RIGHT, padx=(8, 0))
        self.status_label = ttk.Label(header, textvariable=self.status_text_var)
        self.status_label.pack(side=tk.RIGHT)

        self.decor_canvas = tk.Canvas(container, height=18, highlightthickness=0)
        self.decor_canvas.pack(fill=tk.X, pady=(0, 6))

        form = ttk.LabelFrame(container, text="配置", padding=8)
        form.pack(fill=tk.X)
        form.columnconfigure(1, weight=1)

        self._add_row(form, 0, "项目名", self.project_name_var)
        self._add_dir_row(form, 1, "项目目录", self.project_dir_var)
        self._add_row(form, 2, "Git 用户名", self.user_name_var)
        self._add_row(form, 3, "Git 邮箱", self.user_email_var)

        ttk.Label(form, text="仓库类型").grid(row=4, column=0, sticky=tk.W, pady=4)
        type_box = ttk.Combobox(
            form,
            textvariable=self.repo_type_var,
            values=["github", "gitee", "custom"],
            state="readonly",
            width=14,
        )
        type_box.grid(row=4, column=1, sticky=tk.W)
        type_box.bind("<<ComboboxSelected>>", self.on_repo_type_change)

        ttk.Label(form, text="仓库基地址").grid(row=5, column=0, sticky=tk.W, pady=4)
        ttk.Entry(form, textvariable=self.base_url_var).grid(
            row=5, column=1, columnspan=2, sticky=tk.EW
        )
        ttk.Label(
            form,
            text="示例：https://github.com/hookby 或 https://gitee.com/hookby",
            foreground="#666",
        ).grid(row=6, column=1, columnspan=2, sticky=tk.W)

        ttk.Checkbutton(
            form,
            text="设置远程后立即推送 main",
            variable=self.push_var,
        ).grid(row=7, column=1, sticky=tk.W, pady=(4, 0))

        button_bar = ttk.Frame(container)
        button_bar.pack(fill=tk.X, pady=(6, 6))

        buttons = [
            ("初始化本地", self.init_local),
            ("设置远程", self.setup_remote),
            ("完整执行", self.full_init),
            ("检查状态", lambda: self.check_init_status(show_dialog=False)),
            ("网络测试", self.test_network),
            ("拉取并推送", self.sync_all),
            ("清空日志", self.clear_log),
        ]
        for index, (text, callback) in enumerate(buttons):
            row = index // 4
            col = index % 4
            ttk.Button(button_bar, text=text, command=callback, width=14).grid(
                row=row, column=col, padx=4, pady=2, sticky=tk.W
            )

        log_frame = ttk.LabelFrame(container, text="执行日志", padding=6)
        log_frame.pack(fill=tk.BOTH, expand=True)
        self.log_text = tk.Text(log_frame, wrap=tk.WORD, height=14)
        self.log_text.pack(fill=tk.BOTH, expand=True)

        self._update_status_light("#cc3333", "状态：未检查")

    def _add_row(self, parent, row, label, var):
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky=tk.W, pady=4)
        ttk.Entry(parent, textvariable=var).grid(row=row, column=1, columnspan=2, sticky=tk.EW)

    def _add_dir_row(self, parent, row, label, var):
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky=tk.W, pady=4)
        ttk.Entry(parent, textvariable=var).grid(row=row, column=1, sticky=tk.EW)
        ttk.Button(parent, text="选择目录", command=self.select_project_dir, width=12).grid(
            row=row, column=2, sticky=tk.W, padx=(6, 0)
        )

    def _draw_decoration(self):
        if not hasattr(self, "decor_canvas"):
            return
        self.decor_canvas.delete("all")
        width = max(self.decor_canvas.winfo_width(), 720)
        self.decor_canvas.create_line(8, 10, width - 8, 10, fill="#aab6c2", width=1)
        for x in range(20, width - 20, 70):
            self.decor_canvas.create_oval(x - 3, 7, x + 3, 13, fill="#7da2c1", outline="")

    def _on_resize(self, _event=None):
        self._draw_decoration()

    def _update_status_light(self, color, text):
        self.status_color = color
        self.status_text_var.set(text)
        if hasattr(self, "status_canvas"):
            self.status_canvas.delete("all")
            self.status_canvas.create_oval(2, 2, 12, 12, fill=color, outline="")

    def _state_path(self):
        return os.path.join(os.path.dirname(os.path.abspath(__file__)), STATE_FILE_NAME)

    def _load_state(self):
        try:
            path = self._state_path()
            if not os.path.exists(path):
                return
            with open(path, "r", encoding="utf-8") as file:
                data = json.load(file)
            self.project_dir_var.set(data.get("project_dir") or self.project_dir_var.get())
            self.project_name_var.set(data.get("project_name") or self.project_name_var.get())
            self.user_name_var.set(data.get("user_name") or self.user_name_var.get())
            self.user_email_var.set(data.get("user_email") or self.user_email_var.get())
            self.repo_type_var.set(data.get("repo_type") or self.repo_type_var.get())
            self.base_url_var.set(data.get("base_url") or self.base_url_var.get())
            self.push_var.set(bool(data.get("push", True)))
            geometry = data.get("geometry")
            if geometry:
                self.root.geometry(geometry)
        except Exception:
            pass

    def _save_state(self):
        data = {
            "project_dir": self.project_dir_var.get().strip(),
            "project_name": self.project_name_var.get().strip(),
            "user_name": self.user_name_var.get().strip(),
            "user_email": self.user_email_var.get().strip(),
            "repo_type": self.repo_type_var.get().strip(),
            "base_url": self.base_url_var.get().strip(),
            "push": bool(self.push_var.get()),
            "geometry": self.root.geometry(),
        }
        try:
            with open(self._state_path(), "w", encoding="utf-8") as file:
                json.dump(data, file, ensure_ascii=False, indent=2)
        except Exception:
            pass

    def on_close(self):
        self._save_state()
        self.root.destroy()

    def select_project_dir(self):
        current_dir = self.project_dir_var.get().strip() or os.getcwd()
        selected_dir = filedialog.askdirectory(initialdir=current_dir, title="选择项目目录")
        if not selected_dir:
            return

        self.project_dir_var.set(selected_dir)
        project_name = os.path.basename(selected_dir.rstrip("\\/"))
        self.project_name_var.set(project_name)
        self._save_state()
        self.log(f"[目录] 已切换项目目录: {selected_dir}")
        self.log(f"[目录] 项目名自动更新为: {project_name}")
        self.check_init_status(show_dialog=False)

    def on_repo_type_change(self, _event=None):
        repo_type = self.repo_type_var.get()
        if repo_type in DEFAULT_URLS:
            self.base_url_var.set(DEFAULT_URLS[repo_type])
        self._save_state()

    def log(self, text):
        self.log_text.insert(tk.END, text + "\n")
        self.log_text.see(tk.END)
        self.root.update_idletasks()

    def clear_log(self):
        self.log_text.delete("1.0", tk.END)

    def validate_project_dir(self, show_error=True):
        project_dir = self.project_dir_var.get().strip()
        if not project_dir or not os.path.isdir(project_dir):
            if show_error:
                messagebox.showerror("错误", "项目目录不存在")
            return None
        return project_dir

    def validate_form(self):
        project_name = self.project_name_var.get().strip()
        project_dir = self.project_dir_var.get().strip()
        user_name = self.user_name_var.get().strip()
        user_email = self.user_email_var.get().strip()
        base_url = self.base_url_var.get().strip().rstrip("/")

        if not project_name:
            messagebox.showerror("错误", "项目名不能为空")
            return None
        if not project_dir or not os.path.isdir(project_dir):
            messagebox.showerror("错误", "项目目录不存在")
            return None
        if not user_name:
            messagebox.showerror("错误", "Git 用户名不能为空")
            return None
        if not user_email:
            messagebox.showerror("错误", "Git 邮箱不能为空")
            return None
        if not base_url:
            messagebox.showerror("错误", "仓库基地址不能为空")
            return None

        remote_url = build_remote_url(base_url, project_name)
        if not remote_url:
            messagebox.showerror("错误", "无法解析远程仓库地址")
            return None

        result = {
            "project_name": project_name,
            "project_dir": project_dir,
            "user_name": user_name,
            "user_email": user_email,
            "base_url": base_url,
            "remote_url": remote_url,
        }
        self._save_state()
        return result

    def init_local(self):
        params = self.validate_form()
        if not params:
            return

        project_dir = params["project_dir"]
        project_name = params["project_name"]

        ok, msg = ensure_readme(project_dir, project_name)
        self.log(f"[README] {msg}")
        if not ok:
            messagebox.showerror("错误", msg)
            return

        commands = [
            "git init",
            f'git config --local user.name "{params["user_name"]}"',
            f'git config --local user.email "{params["user_email"]}"',
            "git add .",
            'git commit -m "init repository"',
        ]

        self.log("[本地初始化] 开始...")
        for command in commands:
            success, stdout, stderr = run_command(command, project_dir)
            self.log(f"$ {command}")
            if stdout.strip():
                self.log(stdout.strip())
            if not success:
                if stderr.strip():
                    self.log(stderr.strip())
                messagebox.showerror("执行失败", f"命令失败: {command}")
                return
        self.log("[本地初始化] 完成")
        self._save_state()
        self.check_init_status(show_dialog=False)
        messagebox.showinfo("完成", "本地仓库初始化成功")

    def setup_remote(self):
        params = self.validate_form()
        if not params:
            return

        project_dir = params["project_dir"]
        remote_url = params["remote_url"]

        self.log(f"[远程地址] {remote_url}")

        remove_origin_cmd = "git remote remove origin"
        run_command(remove_origin_cmd, project_dir)

        commands = [
            f"git remote add origin {remote_url}",
            "git branch -M main",
        ]

        if self.push_var.get():
            commands.append("git push -u origin main")

        self.log("[远程设置] 开始...")
        for command in commands:
            success, stdout, stderr = run_command(command, project_dir)
            self.log(f"$ {command}")
            if stdout.strip():
                self.log(stdout.strip())
            if not success:
                if stderr.strip():
                    self.log(stderr.strip())
                err_lower = (stderr or "").lower()
                if "failed to connect" in err_lower or "could not connect to server" in err_lower:
                    messagebox.showerror(
                        "网络不可达",
                        "当前环境无法连接 GitHub/Gitee（443）。\n"
                        "可先取消勾选“设置远程后立即推送 main”，仅写入 remote；\n"
                        "网络恢复后再手动执行：git push -u origin main",
                    )
                    return
                messagebox.showerror(
                    "执行失败",
                    "远程设置失败，请确认仓库已创建、地址正确且账号有权限。",
                )
                return

        self.log("[远程设置] 完成")
        self._save_state()
        self.check_init_status(show_dialog=False)
        messagebox.showinfo("完成", "远程仓库配置并推送成功")

    def full_init(self):
        is_complete, _ = self.check_init_status(show_dialog=False)
        if is_complete:
            messagebox.showinfo("提示", "当前项目已完整初始化，无需重复执行。")
            return

        self.init_local()
        self.setup_remote()

    def check_init_status(self, show_dialog=False):
        project_dir = self.validate_project_dir(show_error=show_dialog)
        if not project_dir:
            self._update_status_light("#cc3333", "状态：目录无效")
            return False, "项目目录无效"

        self.log("[检查] 开始检查初始化完整性...")

        git_dir = os.path.join(project_dir, ".git")
        if not os.path.isdir(git_dir):
            msg = "未检测到 .git，尚未初始化本地仓库"
            self.log(f"✗ {msg}")
            self._update_status_light("#cc3333", "状态：未初始化本地仓库")
            if show_dialog:
                messagebox.showwarning("检查结果", msg)
            return False, msg
        self.log("✓ 已检测到 .git")

        ok_remote, stdout_remote, stderr_remote = run_command("git remote get-url origin", project_dir)
        remote_url = (stdout_remote or "").strip()
        if not ok_remote or not remote_url:
            msg = "未配置 origin 远程仓库"
            self.log(f"✗ {msg}")
            self._update_status_light("#cc3333", "状态：缺少 origin")
            if stderr_remote.strip():
                self.log(stderr_remote.strip())
            if show_dialog:
                messagebox.showwarning("检查结果", msg)
            return False, msg
        self.log(f"✓ origin: {remote_url}")

        ok_fetch, stdout_fetch, stderr_fetch = run_command("git fetch --all --prune", project_dir)
        if not ok_fetch:
            msg = "远程拉取检查失败（fetch 未通过）"
            self.log(f"✗ {msg}")
            self._update_status_light("#cc3333", "状态：远程不可拉取")
            if stdout_fetch.strip():
                self.log(stdout_fetch.strip())
            if stderr_fetch.strip():
                self.log(stderr_fetch.strip())
            if show_dialog:
                messagebox.showwarning("检查结果", msg)
            return False, msg

        if stdout_fetch.strip():
            self.log(stdout_fetch.strip())
        self.log("✓ fetch 检查通过")

        msg = "初始化完整：本地 .git、origin、拉取检查均通过"
        self.log(f"✓ {msg}")
        self._update_status_light("#2fa14f", "状态：初始化完整")
        if show_dialog:
            messagebox.showinfo("检查结果", msg)
        return True, msg

    def test_network(self):
        base_url = self.base_url_var.get().strip()
        custom_host = extract_host(base_url)
        hosts = ["github.com", "gitee.com"]
        if custom_host and custom_host not in hosts:
            hosts.append(custom_host)

        self.log("[网络测试] 开始（TCP 443）...")
        all_ok = True
        for host in hosts:
            ok, detail = test_host_443(host, timeout=5)
            if ok:
                self.log(f"✓ {host}:443 可达")
            else:
                all_ok = False
                self.log(f"✗ {host}:443 不可达 -> {detail}")

        if all_ok:
            messagebox.showinfo("网络测试", "网络连通正常，GitHub/Gitee 443 均可达")
        else:
            messagebox.showwarning("网络测试", "存在不可达主机，请查看日志")

    def sync_all(self):
        project_dir = self.validate_project_dir()
        if not project_dir:
            return

        commands = [
            "git fetch --all --prune",
            "git pull --all",
            "git push --all",
            "git push --tags",
        ]

        self.log("[同步] 开始拉取全部并推送...")
        for command in commands:
            success, stdout, stderr = run_command(command, project_dir)
            self.log(f"$ {command}")
            if stdout.strip():
                self.log(stdout.strip())
            if not success:
                if stderr.strip():
                    self.log(stderr.strip())
                messagebox.showerror("同步失败", f"命令失败: {command}")
                return

        self.log("[同步] 完成")
        self._save_state()
        self.check_init_status(show_dialog=False)
        messagebox.showinfo("完成", "拉取全部提交并推送完成")


def main():
    root = tk.Tk()
    style = ttk.Style()
    if "vista" in style.theme_names():
        style.theme_use("vista")
    GitInitWindow(root)
    root.mainloop()


if __name__ == "__main__":
    main()
