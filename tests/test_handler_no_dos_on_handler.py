#!/usr/bin/env python3
"""No DOS call runs on the handler process from the packet loop (CON-87000004).

A Lock/Open/Examine on the handler process does DoPkt on pr_MsgPort, the handler's own packet port, and
takes a client packet off it: the boot failed with "Program failed (error #87000004)" (AN_AsyncPkt) once
fonts_ttf_exists (a Lock of FONTS:SymbolsNerdFontMono.ttf) ran in apply_profile for every window open.
Every window, tab and pane is its own handler process, so the rule holds for all of them.

Guard 1 (the first fix): fonts_ttf_exists is called only from config_worker and watch_worker, and
apply_profile / parse_spec / profile_switch call no DOS function.

Guard 2 (HANDLER-DOS-IN-PACKET-LOOP): every DOS call in handler/vtcon_handler.c sits inside a function on
WORKER_SIDE, the functions that run on a worker process (worker_call / start_worker / CreateNewProcTags
entries) or are called only from them. The map is built from the function bodies (comments and strings
stripped), not from a flat grep. A new DOS call in any other function fails here: move it to a worker."""
import os, re, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "handler/vtcon_handler.c")
src = open(SRC).read()


def body(name):
    m = re.search(r"\nstatic [^\n;]*\b%s\([^\n;]*\)\n\{" % name, src)
    assert m, name + " not found"
    return src[m.end():src.find("\n}\n", m.end())]


DOS = r"\b(fonts_ttf_exists|Lock|Open|Examine|ExNext|Read|GetVar|Delay|DeviceProc|DoPkt|SendPkt)\s*\("
for fn in ("apply_profile", "parse_spec", "profile_switch"):
    hit = re.search(DOS, body(fn))
    assert not hit, "%s calls %s on the handler process" % (fn, hit.group(1))

# the whole file: fonts_ttf_exists is called from the two workers only (its definition aside)
callers = set()
for m in re.finditer(r"\bfonts_ttf_exists\(", src):
    if src[max(0, m.start() - 12):m.start()].endswith("static int "):
        continue
    start = src.rfind("\nstatic ", 0, m.start())
    callers.add(re.match(r"\nstatic [^\n(]*?\b(\w+)\(", src[start:]).group(1))
assert callers == {"config_worker", "watch_worker"}, "fonts_ttf_exists called from %s" % sorted(callers)
assert "nerd_ttf" in body("apply_profile"), "apply_profile does not use the worker's nerd_ttf"


# ---- guard 2: the per-function map ----------------------------------------------------------------
def strip(text):
    """comments and string/char literals blanked, newlines kept (so offsets stay)"""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
        elif text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            out.append(c + " " * (j - i - 1) + c)
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


# every DOS entry point whose call is a packet to a file system or handler (or runs a command)
DOS_CALLS = ("Lock Open Examine ExamineFH Read Write Seek Close DeleteFile IsFileSystem ParentDir SameLock "
             "OpenFromLock GetVar SetVar DeviceProc DoPkt SendPkt NameFromLock MatchFirst ExNext CreateDir "
             "Rename SystemTags Execute").split()
CALL = re.compile(r"(?<![\w.>])(%s)\s*\(" % "|".join(DOS_CALLS))

# the functions that run on a worker process, or are called only from one
WORKER_SIDE = {
    "png_worker", "file_write_worker", "kfile_read_job", "kfile_unlink_job", "kfile_lock",
    "fonts_ttf_exists", "read_conf", "read_splits", "read_active_theme", "config_worker", "watch_worker",
    "launcher", "run_async", "tab_prog_script", "tab_spawner",
}

clean = strip(src)
funcs = {}  # name -> (body start, body end) of every function definition: "name(...)\n{" ... "\n}\n"
for m in re.finditer(r"\n(?:static\s+)?[A-Za-z_][^\n;{}()]*?\b(\w+)\([^;{}]*\)\s*\n\{", clean):
    end = clean.find("\n}\n", m.end())
    funcs[m.group(1)] = (m.end(), end)
# counted from the structure: every function body opens with a lone "{" line
assert len(funcs) == clean.count("\n{\n"), "function map has %d of %d bodies: the definition pattern broke" % (
    len(funcs), clean.count("\n{\n"))
owner = {}
for m in CALL.finditer(clean):
    where = [n for n, (a, b) in funcs.items() if a <= m.start() < b]
    line = clean.count("\n", 0, m.start()) + 1
    owner.setdefault(where[0] if where else "<file scope>", []).append("%s@%d" % (m.group(1), line))
bad = {n: v for n, v in owner.items() if n not in WORKER_SIDE}
assert not bad, "DOS calls outside the worker-side functions (move them to a worker): %s" % bad
for name in WORKER_SIDE:
    assert name in funcs, "WORKER_SIDE names %s, which is not a function in the source" % name

used = set(owner)
assert used >= {"png_worker", "file_write_worker", "kfile_read_job", "kfile_unlink_job", "kfile_lock"}, \
    "the new workers no longer make their DOS calls: %s" % sorted(used)
print("ok")
