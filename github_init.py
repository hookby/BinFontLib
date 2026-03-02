# -*- coding: utf-8 -*-
"""
Git 初始化窗口工具（tkinter）
- 内置 GitHub / Gitee 两套默认 URL
- 支持自定义 URL
- user.name / user.email 提供默认值并支持修改
"""

import os
import subprocess
import platform
import tkinter as tk
from tkinter import ttk, messagebox

DEFAULT_USER_NAME = "hookby"
DEFAULT_USER_EMAIL = "hookby@gmail.com"

DEFAULT_URLS = {
    "github": "https://github.com/hookby",
    "gitee": "https://gitee.com/hookby",
}

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


class GitInitWindow:
    def __init__(self, root):
        self.root = root
        self.root.title("GitHub/Gitee 初始化工具")
        self.root.geometry("860x560")

        self.project_name_var = tk.StringVar()
        self.project_dir_var = tk.StringVar(value=os.getcwd())
        self.user_name_var = tk.StringVar(value=DEFAULT_USER_NAME)
        self.user_email_var = tk.StringVar(value=DEFAULT_USER_EMAIL)

        self.repo_type_var = tk.StringVar(value="github")
        self.base_url_var = tk.StringVar(value=DEFAULT_URLS["github"])

        self._build_ui()

    def _build_ui(self):
        container = ttk.Frame(self.root, padding=12)
        container.pack(fill=tk.BOTH, expand=True)

        form = ttk.LabelFrame(container, text="配置", padding=10)
        form.pack(fill=tk.X)

        self._add_row(form, 0, "项目名", self.project_name_var)
        self._add_row(form, 1, "项目目录", self.project_dir_var)
        self._add_row(form, 2, "Git 用户名", self.user_name_var)
        self._add_row(form, 3, "Git 邮箱", self.user_email_var)

        ttk.Label(form, text="仓库类型").grid(row=4, column=0, sticky=tk.W, pady=6)
        type_box = ttk.Combobox(
            form,
            textvariable=self.repo_type_var,
            values=["github", "gitee", "custom"],
            state="readonly",
            width=22,
        )
        type_box.grid(row=4, column=1, sticky=tk.W)
        type_box.bind("<<ComboboxSelected>>", self.on_repo_type_change)

        ttk.Label(form, text="仓库基地址").grid(row=5, column=0, sticky=tk.W, pady=6)
        ttk.Entry(form, textvariable=self.base_url_var, width=72).grid(
            row=5, column=1, sticky=tk.W
        )
        ttk.Label(
            form,
            text="示例：https://github.com/hookby 或 https://gitee.com/hookby",
            foreground="#666",
        ).grid(row=6, column=1, sticky=tk.W)

        button_bar = ttk.Frame(container)
        button_bar.pack(fill=tk.X, pady=(10, 8))

        ttk.Button(button_bar, text="初始化本地仓库", command=self.init_local).pack(
            side=tk.LEFT, padx=(0, 8)
        )
        ttk.Button(button_bar, text="设置远程并推送", command=self.setup_remote).pack(
            side=tk.LEFT, padx=(0, 8)
        )
        ttk.Button(button_bar, text="完整执行", command=self.full_init).pack(
            side=tk.LEFT, padx=(0, 8)
        )
        ttk.Button(button_bar, text="清空日志", command=self.clear_log).pack(side=tk.LEFT)

        log_frame = ttk.LabelFrame(container, text="执行日志", padding=8)
        log_frame.pack(fill=tk.BOTH, expand=True)
        self.log_text = tk.Text(log_frame, wrap=tk.WORD, height=18)
        self.log_text.pack(fill=tk.BOTH, expand=True)

    def _add_row(self, parent, row, label, var):
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky=tk.W, pady=6)
        ttk.Entry(parent, textvariable=var, width=72).grid(row=row, column=1, sticky=tk.W)

    def on_repo_type_change(self, _event=None):
        repo_type = self.repo_type_var.get()
        if repo_type in DEFAULT_URLS:
            self.base_url_var.set(DEFAULT_URLS[repo_type])

    def log(self, text):
        self.log_text.insert(tk.END, text + "\n")
        self.log_text.see(tk.END)
        self.root.update_idletasks()

    def clear_log(self):
        self.log_text.delete("1.0", tk.END)

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

        return {
            "project_name": project_name,
            "project_dir": project_dir,
            "user_name": user_name,
            "user_email": user_email,
            "base_url": base_url,
            "remote_url": f"{base_url}/{project_name}.git",
        }

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
            "git push -u origin main",
        ]

        self.log("[远程设置] 开始...")
        for command in commands:
            success, stdout, stderr = run_command(command, project_dir)
            self.log(f"$ {command}")
            if stdout.strip():
                self.log(stdout.strip())
            if not success:
                if stderr.strip():
                    self.log(stderr.strip())
                messagebox.showerror(
                    "执行失败",
                    "远程设置失败，请确认远程仓库已创建并且账号有权限。",
                )
                return

        self.log("[远程设置] 完成")
        messagebox.showinfo("完成", "远程仓库配置并推送成功")

    def full_init(self):
        self.init_local()
        self.setup_remote()


def main():
    root = tk.Tk()
    style = ttk.Style()
    if "vista" in style.theme_names():
        style.theme_use("vista")
    GitInitWindow(root)
    root.mainloop()


if __name__ == "__main__":
    main()
