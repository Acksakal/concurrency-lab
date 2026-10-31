#!/usr/bin/env python3
"""Check that each .c file in c/ is identical to the code shown in docs/.

The .c files are the source of truth. If this fails, a page and its .c file
have drifted apart: update whichever one is stale.
Run from the repository root:  python3 scripts/check_sync.py
"""
import html
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PAGES = ROOT / "docs"

def page(name):
    return (PAGES / name).read_text(encoding="utf-8")

def plain_block(text, block_id, strip_tags=False):
    m = re.search(r'<script type="text/plain" id="%s">(.*?)</script>' % block_id, text, re.S)
    if not m:
        return None
    code = m.group(1)
    if strip_tags:                       # thread-paths.html marks lines with @@tags
        code = re.sub(r"@@\w+", "", code)
    return code

def threads_block(text, filename):
    m = re.search(r'<div class="fn">%s</div><pre class="cb"><code>(.*?)</code></pre>'
                  % re.escape(filename), text, re.S)
    return html.unescape(m.group(1)) if m else None

def norm(s):
    return s.strip().replace("\r\n", "\n") + "\n" if s is not None else None

threads = page("threads.html")
paths = page("thread-paths.html")
CHECKS = [
    ("c/01-processes/fork.c",            plain_block(page("concurrency.html"), "src-c")),
    ("c/02-threads/hello_threads.c",     threads_block(threads, "hello_threads.c")),
    ("c/03-race/race.c",                 threads_block(threads, "race.c")),
    ("c/03-race/mutex.c",                threads_block(threads, "mutex.c")),
    ("c/04-deadlock/deadlock.c",         threads_block(threads, "deadlock.c")),
    ("c/05-condvar/condvar.c",           threads_block(threads, "condvar.c")),
    ("c/05-condvar/counter_original.c",  plain_block(paths, "code-L4", True)),
    ("c/05-condvar/counter_fixed.c",     plain_block(paths, "code-L5", True)),
    ("c/06-hardware/ryzen_lab.c",        plain_block(page("thread-ryzen.html"), "csrc")),
]
# Not checked (no full copy in any page): single_count.c, deadlock_fixed.c (shown as a diff).

bad = 0
for rel, shown in CHECKS:
    local = norm((ROOT / rel).read_text(encoding="utf-8"))
    if shown is None:
        print("MISSING in page:", rel); bad += 1
    elif norm(shown) != local:
        print("DIFFERS:", rel); bad += 1
    else:
        print("ok     ", rel)
sys.exit(1 if bad else 0)
