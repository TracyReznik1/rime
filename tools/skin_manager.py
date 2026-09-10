"""Lightweight on-demand skin manager; packaged separately from the IME."""
import argparse
import queue
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from PIL import Image, ImageTk
from skin_store import SkinStore, user_directory


class Manager:
    def __init__(self, root, directory):
        self.root, self.store = root, SkinStore(directory)
        self.events = queue.Queue()
        self.items = []
        root.title("小狼毫 · 皮肤管理")
        root.geometry("820x560")
        root.minsize(700, 500)
        root.columnconfigure(0, weight=1)
        root.rowconfigure(1, weight=1)
        top = ttk.Frame(root, padding=(20, 16))
        top.grid(row=0, column=0, sticky="ew")
        ttk.Label(top, text="皮肤管理", font=("Microsoft YaHei UI", 19, "bold")).pack(anchor="w")
        ttk.Label(top, text="导入搜狗静态横版皮肤，随时切换或恢复原配色。").pack(anchor="w", pady=(6, 0))
        body = ttk.Frame(root, padding=(20, 0, 20, 0))
        body.grid(row=1, column=0, sticky="nsew")
        body.columnconfigure(1, weight=1)
        body.rowconfigure(0, weight=1)
        self.listbox = tk.Listbox(body, width=24, exportselection=False, borderwidth=1, relief="solid", font=("Microsoft YaHei UI", 11))
        self.listbox.grid(row=0, column=0, sticky="nsew", padx=(0, 16))
        self.listbox.bind("<<ListboxSelect>>", self.preview)
        right = ttk.Frame(body)
        right.grid(row=0, column=1, sticky="nsew")
        right.columnconfigure(0, weight=1)
        right.rowconfigure(1, weight=1)
        self.heading = ttk.Label(right, text="选择一款皮肤", font=("Microsoft YaHei UI", 13, "bold"))
        self.heading.grid(row=0, column=0, sticky="w", pady=(0, 8))
        self.canvas = tk.Canvas(right, background="#f3f4f6", highlightthickness=0)
        self.canvas.grid(row=1, column=0, sticky="nsew")
        self.canvas.bind("<Configure>", self.preview)
        ttk.Label(right, text="背景预览 · 文字布局以实际输入为准", foreground="#666666").grid(row=2, column=0, sticky="w", pady=8)
        buttons = ttk.Frame(root, padding=(20, 14))
        buttons.grid(row=2, column=0, sticky="ew")
        self.import_button = ttk.Button(buttons, text="导入 .ssf…", command=self.import_file)
        self.import_button.pack(side="left")
        ttk.Button(buttons, text="应用皮肤", command=lambda: self.action(self.apply)).pack(side="left", padx=8)
        ttk.Button(buttons, text="恢复默认", command=lambda: self.action(self.store.default)).pack(side="left")
        ttk.Button(buttons, text="撤销切换", command=lambda: self.action(self.store.undo)).pack(side="left", padx=8)
        ttk.Button(buttons, text="移除…", command=self.remove).pack(side="right")
        self.status = tk.StringVar(value="仅支持静态 H1 拉伸布局；暂不支持动画、平铺和竖版。")
        ttk.Label(root, textvariable=self.status, wraplength=770, padding=(20, 0, 20, 16)).grid(row=3, column=0, sticky="ew")
        self.refresh()
        root.after(100, self.poll)

    def refresh(self, selected=None):
        self.items = self.store.entries()
        active = self.store.current_manifest()
        self.listbox.delete(0, tk.END)
        for i, item in enumerate(self.items):
            suffix = "  · 当前" if str(item["path"] / "skin.ini") == active else ""
            self.listbox.insert(tk.END, item["name"] + suffix)
            if item["id"] == selected:
                self.listbox.selection_set(i)
        if self.items and not self.listbox.curselection():
            self.listbox.selection_set(0)
        self.preview()

    def selected(self):
        selection = self.listbox.curselection()
        if not selection:
            raise ValueError("请先选择或导入一款皮肤。")
        return self.items[selection[0]]

    def preview(self, event=None):
        self.canvas.delete("all")
        if not self.listbox.curselection():
            self.heading.config(text="还没有导入皮肤")
            self.canvas.create_text(max(100, self.canvas.winfo_width()//2), 110, text="点击「导入 .ssf…」开始", fill="#666666", font=("Microsoft YaHei UI", 12))
            return
        item = self.selected()
        self.heading.config(text=item["name"])
        try:
            with Image.open(item["path"] / "background.png") as source:
                bitmap = source.convert("RGBA")
            bitmap.thumbnail((max(100, self.canvas.winfo_width()-24), max(100, self.canvas.winfo_height()-24)))
            self.photo = ImageTk.PhotoImage(bitmap)
            self.canvas.create_image(self.canvas.winfo_width()//2, self.canvas.winfo_height()//2, image=self.photo)
        except (OSError, ValueError) as error:
            self.status.set(f"预览失败：{error}")

    def apply(self):
        self.store.apply(self.selected()["id"])

    def action(self, callback):
        try:
            callback()
            selected = self.selected()["id"] if self.listbox.curselection() else None
            self.refresh(selected)
            self.status.set("已应用。请在输入框重新组词查看效果；首次安装新版输入法后需重开应用。")
        except Exception as error:
            messagebox.showerror("未完成", str(error), parent=self.root)

    def remove(self):
        try:
            item = self.selected()
            if messagebox.askyesno("移除皮肤", f"将「{item['name']}」移到皮肤库的 .trash 目录？", parent=self.root):
                self.store.remove(item["id"])
                self.refresh()
        except Exception as error:
            messagebox.showerror("未完成", str(error), parent=self.root)

    def import_file(self):
        source = filedialog.askopenfilename(parent=self.root, title="导入搜狗皮肤", filetypes=[("搜狗皮肤", "*.ssf")])
        if not source:
            return
        self.import_button.state(["disabled"])
        self.status.set("正在检查并导入皮肤…")
        def worker():
            try:
                self.events.put((True, self.store.import_file(source)))
            except Exception as error:
                self.events.put((False, str(error)))
        threading.Thread(target=worker, daemon=True).start()

    def poll(self):
        try:
            ok, result = self.events.get_nowait()
            self.import_button.state(["!disabled"])
            if ok:
                self.refresh(result[0]["id"])
                self.status.set("已导入静态横版背景。确认预览后点击「应用皮肤」。动画、竖版与独立状态栏未导入。")
            else:
                self.status.set("导入未完成，原皮肤未改变。")
                messagebox.showerror("无法导入此皮肤", result, parent=self.root)
        except queue.Empty:
            pass
        self.root.after(100, self.poll)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--user-dir", help="Use an isolated library for testing")
    args = parser.parse_args()
    root = tk.Tk()
    ttk.Style(root).theme_use("vista")
    Manager(root, args.user_dir or user_directory())
    root.mainloop()


if __name__ == "__main__":
    main()
