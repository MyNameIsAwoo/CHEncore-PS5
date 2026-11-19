#!/usr/bin/env python3
# Copyright (C) 2026 HOWL
# SPDX-License-Identifier: MIT
"""CH: Encore personal install builder; GUI by default, optional command line."""
import argparse
import os
from pathlib import Path
import queue
import subprocess
import sys
import threading
from build_core import build, Cancelled, detect_installations, HERE


def gui():
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox
    root = tk.Tk(); root.title('CH: Encore — Install Builder')
    root.geometry('860x760'); root.minsize(760, 740); root.configure(bg='#081c3a')
    style = ttk.Style(); style.theme_use('clam')
    style.configure('.', background='#081c3a', foreground='#ecf0f5', font=('Segoe UI', 11))
    style.configure('TEntry', fieldbackground='#102d50', foreground='#ffffff', padding=7)
    style.configure('TCombobox', fieldbackground='#102d50', background='#102d50', foreground='#ffffff', padding=7)
    style.map('TCombobox', fieldbackground=[('readonly', '#102d50')])
    style.configure('TButton', background='#194671', padding=(16, 9))
    style.map('TButton', background=[('active', '#245d91'), ('disabled', '#14263c')],
              foreground=[('disabled', '#8591a0'), ('!disabled', '#ecf0f5')])
    style.configure('Title.TLabel', font=('Segoe UI', 25, 'bold'))
    style.configure('Sub.TLabel', foreground='#aebac9')
    style.configure('TCheckbutton', background='#081c3a')
    style.map('TCheckbutton', background=[('active', '#081c3a')])
    style.configure('TRadiobutton', padding=(0, 7), background='#081c3a')
    style.map('TRadiobutton', background=[('active', '#081c3a')])
    style.configure('TProgressbar', background='#58b38b', troughcolor='#102d50')
    pane = ttk.Frame(root, padding=26); pane.pack(fill='both', expand=True)
    ttk.Label(pane, text='CH: ENCORE', style='Title.TLabel').pack(anchor='w')
    ttk.Label(pane, text='Create your personal PS5 install', style='Sub.TLabel').pack(anchor='w', pady=(2, 18))
    ttk.Label(pane, text='PC game installation').pack(anchor='w')
    source = tk.StringVar(); line = ttk.Frame(pane); line.pack(fill='x', pady=(6, 3))
    combo = ttk.Combobox(line, textvariable=source); combo.pack(side='left', fill='x', expand=True)
    browse = ttk.Button(line, text='Browse…', command=lambda: choose_source()); browse.pack(side='left', padx=(8, 0))
    detect = ttk.Button(line, text='Detect', command=lambda: scan()); detect.pack(side='left', padx=(8, 0))
    hint = tk.StringVar(value='Standalone and launcher-managed installs are supported when their asset version matches.')
    ttk.Label(pane, textvariable=hint, style='Sub.TLabel', wraplength=790).pack(anchor='w', pady=(0, 17))
    ttk.Label(pane, text='Output folder').pack(anchor='w')
    output = tk.StringVar(value=str(Path.home() / 'Desktop' / 'CH Encore Builds'))
    line = ttk.Frame(pane); line.pack(fill='x', pady=(6, 16))
    entry = ttk.Entry(line, textvariable=output); entry.pack(side='left', fill='x', expand=True)
    output_button = ttk.Button(line, text='Browse…', command=lambda: choose_output()); output_button.pack(side='left', padx=(8, 0))
    ttk.Label(pane, text='Install format').pack(anchor='w')
    kind = tk.StringVar(value='folder'); formats = ttk.Frame(pane); formats.pack(fill='x', pady=(3, 9))
    radios = []
    for value, text in [('folder', 'Game folder'), ('exfat', 'exFAT image'), ('pkg', 'Native homebrew PKG / fPKG')]:
        radio = ttk.Radiobutton(formats, text=text, variable=kind, value=value)
        radio.pack(side='left', padx=(0, 24)); radios.append(radio)
    format_hint = tk.StringVar(value='Your assets are added on this PC. No separate asset file or import selection on the console.')
    ttk.Label(pane, textvariable=format_hint, style='Sub.TLabel', wraplength=790).pack(anchor='w', pady=(0, 8))
    def update_format_hint(*_):
        format_hint.set('PKG: first use automatically downloads a verified tool; later builds use its cache.' if kind.get() == 'pkg' else
                        'Your assets are added on this PC. No separate asset file or import selection on the console.')
    kind.trace_add('write', update_format_hint)
    acknowledge = tk.BooleanVar()
    consent = ttk.Checkbutton(pane, text='I have permission to extract and use these local PC resources.', variable=acknowledge)
    consent.pack(anchor='w', pady=(3, 9))
    logbox = tk.Text(pane, height=5, bg='#06142b', fg='#c9d6e4', font=('Consolas', 10),
                     relief='flat', wrap='word', padx=12, pady=10, state='disabled')
    logbox.pack(fill='both', expand=True, pady=(0, 10))
    status = tk.StringVar(value='Ready')
    ttk.Label(pane, textvariable=status).pack(anchor='w')
    progress = ttk.Progressbar(pane, mode='indeterminate'); progress.pack(fill='x', pady=(7, 12))
    buttons = ttk.Frame(pane); buttons.pack(fill='x')
    create = ttk.Button(buttons, text='Build install', command=lambda: start()); create.pack(side='right')
    cancel_button = ttk.Button(buttons, text='Cancel', command=lambda: cancel.set(), state='disabled'); cancel_button.pack(side='right', padx=(0, 8))
    open_button = ttk.Button(buttons, text='Open output', command=lambda: open_output(), state='disabled'); open_button.pack(side='left')
    events = queue.Queue(); cancel = threading.Event(); active = False; last_output = None; closing = False; detecting = False
    controls = [combo, browse, detect, entry, output_button, consent, *radios]

    def choose_source():
        folder = filedialog.askdirectory(title='Select your installed PC game (or *_Data folder)')
        if folder: source.set(folder)

    def choose_output():
        folder = filedialog.askdirectory(title='Select output location')
        if folder: output.set(folder)

    def write(text):
        logbox.configure(state='normal'); logbox.insert('end', str(text) + '\n'); logbox.see('end'); logbox.configure(state='disabled')

    def scan():
        nonlocal detecting
        if detecting or active: return
        detecting = True; detect.configure(state='disabled'); hint.set('Looking in common install and launcher locations…')
        def work():
            try: events.put(('found', detect_installations()))
            except Exception as exc: events.put(('scan-error', str(exc)))
        threading.Thread(target=work, daemon=True).start()

    def start():
        nonlocal active
        if active: return
        if not acknowledge.get():
            messagebox.showinfo('Local resources', 'Only use resources you have permission to extract and use.'); return
        if not source.get().strip() or not output.get().strip():
            messagebox.showinfo('Choose folders', 'Select your PC installation and output folder first.'); return
        active = True; cancel.clear()
        for control in controls: control.configure(state='disabled')
        create.configure(state='disabled'); open_button.configure(state='disabled'); cancel_button.configure(state='normal')
        progress.start(14); status.set('Building…'); write('Starting personal build. Your PC installation is read only.')
        game, parent, target = source.get(), output.get(), kind.get()
        def work():
            try:
                result = build(game, parent, target, log=lambda text: events.put(('log', text)), cancel=cancel)
                events.put(('complete', result))
            except Cancelled as exc: events.put(('cancelled', str(exc)))
            except Exception as exc: events.put(('error', str(exc)))
        threading.Thread(target=work, daemon=True).start()

    def open_output():
        if last_output:
            if os.name == 'nt': os.startfile(str(last_output))
            else: subprocess.Popen(['open' if sys.platform == 'darwin' else 'xdg-open', str(last_output)])

    def close():
        nonlocal closing
        if active:
            if not messagebox.askyesno('Cancel build?', 'Cancel the active build and close when cleanup finishes?'): return
            closing = True; cancel.set(); status.set('Cancelling and cleaning up…')
        else: root.destroy()

    def poll():
        nonlocal active, last_output, detecting
        try:
            while True:
                event, value = events.get_nowait()
                if event in ('found', 'scan-error'):
                    detecting = False
                    if not active: detect.configure(state='normal')
                    if event == 'found':
                        combo['values'] = [str(p) for p in value]
                        if value and not source.get(): source.set(str(value[0]))
                        hint.set(f'Found {len(value)} installation(s). Choose one or Browse for a custom location.' if value else 'No installation found in common locations. Use Browse for your standalone or custom launcher folder.')
                    else: hint.set('Detection unavailable. Use Browse.'); write(value)
                elif event == 'log': write(value)
                else:
                    active = False; progress.stop()
                    for control in controls: control.configure(state='normal')
                    create.configure(state='normal'); cancel_button.configure(state='disabled')
                    if event == 'complete':
                        last_output = value; open_button.configure(state='normal'); status.set('Your install is ready'); write('Ready to install: ' + str(value))
                    elif event == 'cancelled': status.set('Cancelled'); write(value)
                    else: status.set('Build did not complete'); write(value); messagebox.showerror('Build did not complete', str(value))
                    if closing: root.destroy(); return
        except queue.Empty: pass
        root.after(100, poll)
    root.protocol('WM_DELETE_WINDOW', close)
    root.after(150, scan); root.after(100, poll); root.mainloop()


def main():
    # A frozen executable is its own helper host; it cannot launch .py files
    # with sys.executable as a normal installed Python interpreter would.
    if len(sys.argv) > 1 and sys.argv[1] == '--internal-exfat':
        from exfat_backend import main as helper
        helper(sys.argv[2:]); return 0
    if len(sys.argv) > 1 and sys.argv[1] == '--internal-validate-pkg':
        from validate_pkg import main as helper
        helper(sys.argv[2:]); return 0
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game', type=Path)
    p.add_argument('--output', type=Path)
    p.add_argument('--format', choices=('folder', 'exfat', 'pkg'))
    p.add_argument('--bundle', type=Path, default=HERE)
    p.add_argument('--pkg-tool', type=Path)
    p.add_argument('--cache', type=Path)
    p.add_argument('--detect', action='store_true')
    p.add_argument('--self-check', action='store_true', help='Verify the bundled game and packaging tools without PC assets')
    p.add_argument('--offline', action='store_true', help='Disallow Python network downloads during this run')
    p.add_argument('--log-file', type=Path, help='Write command-line diagnostics to this file')
    p.add_argument('--authorized', action='store_true', help='Confirm permission to extract/use your selected local resources')
    args = p.parse_args()
    if args.log_file:
        stream = args.log_file.open('w', encoding='utf-8', buffering=1)
        sys.stdout = sys.stderr = stream
    if args.offline:
        import urllib.request
        def no_download(*_args, **_kwargs):
            raise OSError('Offline test: a network download was requested.')
        urllib.request.urlopen = no_download
    if args.self_check:
        import tempfile
        from build_core import template_config, unpack_template
        from backends import prepare_backend
        config = template_config(args.bundle)
        with tempfile.TemporaryDirectory(prefix='encore-self-check-') as directory:
            unpack_template(args.bundle, Path(directory), config, threading.Event())
        backend = prepare_backend('exfat', None, None, threading.Event(), print)
        if not isinstance(backend, dict): raise ValueError('This executable needs its embedded packaging backend.')
        print('PASS bundled game template and offline exFAT tools; Python ' + sys.version.split()[0])
        print('PKG uses an automatically verified first-use tool download, then its cache.')
        return 0
    if args.detect:
        for folder in detect_installations(): print(folder)
        return 0
    if args.game is None and args.output is None and args.format is None:
        gui(); return 0
    if args.game is None or args.output is None or args.format is None or not args.authorized:
        p.error('--game, --output, --format and --authorized are required for a command-line build')
    try:
        build(args.game, args.output, args.format, args.bundle, pkg_tool=args.pkg_tool, cache=args.cache)
    except (ValueError, OSError, Cancelled) as exc:
        print('Build failed: ' + str(exc), file=sys.stderr); return 1
    return 0

if __name__ == '__main__':
    import multiprocessing
    multiprocessing.freeze_support()
    if getattr(sys, 'frozen', False):
        # --windowed builds do not necessarily have console streams. The GUI
        # has its own log, and CLI users can explicitly select --log-file.
        if sys.stdout is None: sys.stdout = open(os.devnull, 'w')
        if sys.stderr is None: sys.stderr = open(os.devnull, 'w')
    sys.exit(main())
