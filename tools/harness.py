#!/usr/bin/env python3
"""Drive build/harness (tools/c/harness.c): the real game, stepped by commands.

One running game, kept by a small server, so separate shell calls can play it:

    python3 tools/harness.py start [--modern] [--seed N] [--size WxH]
    python3 tools/harness.py key 1             # any harness command (see harness.c)
    python3 tools/harness.py wait
    python3 tools/harness.py state             # JSON: the frame, request, game, scene
    python3 tools/harness.py snap /tmp/a.png 384 252
    python3 tools/harness.py stop

Each command prints the harness's answer (one JSON line, after any event lines
when "watch on" is set). Everything the harness says also goes to the log,
/tmp/opentrail-harness.log, so `tail -f` on it is a live stream of the game.
A script of commands, one per line: `python3 tools/harness.py run FILE`.

As a library: Harness(modern=True).cmd("wait") -> the answer as a dict.
"""
import json, os, socket, subprocess, sys, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOCK = '/tmp/opentrail-harness.sock'
LOG = '/tmp/opentrail-harness.log'


class Harness:
    """The harness as a subprocess in this process."""

    def __init__(self, modern=False, seed=None, size=None, log=None, touch=False):
        args = [os.path.join(ROOT, 'build', 'harness')]
        if modern: args.append('--modern')
        if touch: args.append('--touch')
        if seed is not None: args += ['--seed', str(seed)]
        if size: args += ['--size', size]
        self.log = log
        self.p = subprocess.Popen(args, cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.DEVNULL, text=True, bufsize=1)
        self.ready = self._answer()[-1]

    def _answer(self):
        """Lines up to and including the one with "ok"."""
        lines = []
        while True:
            line = self.p.stdout.readline()
            if not line: raise RuntimeError('the harness ended')
            if self.log: self.log.write(line); self.log.flush()
            d = json.loads(line)
            lines.append(d)
            if 'ok' in d: return lines

    def cmd_all(self, line):
        """All the lines the command produced: events first, the answer last."""
        if self.log: self.log.write('> ' + line + '\n')
        self.p.stdin.write(line + '\n'); self.p.stdin.flush()
        return self._answer()

    def cmd(self, line):
        return self.cmd_all(line)[-1]

    def close(self):
        try: self.cmd('quit')
        except Exception: pass
        self.p.wait(timeout=10)


def serve(argv):
    """The server behind `start`: one Harness, commands from the socket."""
    if os.path.exists(SOCK): os.unlink(SOCK)
    log = open(LOG, 'w')
    h = Harness(modern='--modern' in argv,
                seed=argv[argv.index('--seed') + 1] if '--seed' in argv else None,
                size=argv[argv.index('--size') + 1] if '--size' in argv else None, log=log)
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.bind(SOCK); s.listen(4)
    while True:
        c, _ = s.accept()
        with c, c.makefile('rw') as f:
            line = f.readline().strip()
            try:
                out = h.cmd_all(line)
            except Exception as e:
                out = [{'ok': False, 'error': str(e)}]
            f.write(''.join(json.dumps(d) + '\n' for d in out)); f.flush()
            if line == 'quit' or not out[-1].get('ok', True) and 'ended' in out[-1].get('error', ''):
                break
    s.close(); os.unlink(SOCK)


def send(line):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(SOCK)
    with s, s.makefile('rw') as f:
        f.write(line + '\n'); f.flush()
        return f.read()


def main():
    a = sys.argv[1:]
    if not a: print(__doc__); return 2
    if a[0] == '_serve': serve(a[1:]); return 0
    if a[0] == 'start':
        if os.path.exists(SOCK):
            try: send('state'); print('already running'); return 1
            except OSError: os.unlink(SOCK)
        subprocess.Popen([sys.executable, os.path.abspath(__file__), '_serve'] + a[1:], cwd=ROOT,
                         stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                         start_new_session=True)
        for _ in range(100):
            if os.path.exists(SOCK): print('{"ok": true, "started": true}'); return 0
            time.sleep(0.1)
        print('the harness did not start (is build/harness built? a display needed for its hidden window)')
        return 1
    if not os.path.exists(SOCK): print('not running: harness.py start'); return 1
    if a[0] == 'stop': sys.stdout.write(send('quit')); return 0
    if a[0] == 'run':
        for line in open(a[1]):
            line = line.strip()
            if line and not line.startswith('#'): sys.stdout.write(send(line))
        return 0
    sys.stdout.write(send(' '.join(a)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
