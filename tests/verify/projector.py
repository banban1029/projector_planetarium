"""プロジェクタ（Optoma ML1050ST+）をつないだときの動作確認。結果は build/verify/projector_report.md。

    python tests/verify/projector.py            # 自動の確認（約 1 分。プロジェクタに数回映像が出る）
    python tests/verify/projector.py --manual   # 続けて、人が投影を見て確かめる手順（テストパターン・星空・上映）

MSYS2 Bash でも PowerShell でも同じコマンド。プロジェクタをつなぎ、Windows の表示を「拡張」（Win + P）にしてから実行する。
自動の確認は PASS / FAIL / WARN を出し、1 つでも FAIL なら終了コード 1。
"""

import argparse
import ctypes
import re
import subprocess
import sys
import time
from ctypes import wintypes
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pngtools  # noqa: E402
import verify as v  # noqa: E402  ビルド・アプリの起動・キー入力の道具を使い回す

OUT = v.OUT / 'projector'
WIDTH, HEIGHT, REFRESH = 1280, 800, 60  # ML1050ST+ のネイティブ

# 画面の大きさを実際の画素で受け取る（ノート PC は 200% 表示なので、これが無いと縮んだ値になる）
try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except (AttributeError, OSError):
    pass

user32 = v.user32


class RECT(ctypes.Structure):
    _fields_ = [('left', ctypes.c_long), ('top', ctypes.c_long), ('right', ctypes.c_long), ('bottom', ctypes.c_long)]


class MONITORINFOEX(ctypes.Structure):
    _fields_ = [('cbSize', wintypes.DWORD), ('rcMonitor', RECT), ('rcWork', RECT), ('dwFlags', wintypes.DWORD),
                ('szDevice', wintypes.WCHAR * 32)]


user32.MonitorFromWindow.restype = wintypes.HMONITOR
user32.MonitorFromWindow.argtypes = [wintypes.HWND, wintypes.DWORD]
user32.GetMonitorInfoW.argtypes = [wintypes.HMONITOR, ctypes.POINTER(MONITORINFOEX)]


def windows_of(pid):
    """そのプロセスの GLFW ウィンドウ: [(hwnd, タイトル, ウィンドウの矩形, モニタの矩形, 主モニタか)]"""
    found = []

    def callback(hwnd, _):
        owner = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        cls = ctypes.create_unicode_buffer(64)
        user32.GetClassNameW(hwnd, cls, 64)
        if owner.value == pid and cls.value.startswith('GLFW') and user32.IsWindowVisible(hwnd):
            title = ctypes.create_unicode_buffer(256)
            user32.GetWindowTextW(hwnd, title, 256)
            rect = RECT()
            user32.GetWindowRect(hwnd, ctypes.byref(rect))
            info = MONITORINFOEX()
            info.cbSize = ctypes.sizeof(MONITORINFOEX)
            user32.GetMonitorInfoW(user32.MonitorFromWindow(hwnd, 2), ctypes.byref(info))
            m = info.rcMonitor
            found.append((hwnd, title.value, (rect.left, rect.top, rect.right, rect.bottom),
                          (m.left, m.top, m.right, m.bottom), bool(info.dwFlags & 1)))
        return True

    user32.EnumWindows(v.EnumProc(callback), 0)
    return found


def size(r):
    return r[2] - r[0], r[3] - r[1]


# ---------------------------------------------------------------- 1. Windows から見えるか
def check_windows(r):
    p = subprocess.run(['powershell.exe', '-NoProfile', '-Command',
                        "Get-CimInstance -Namespace root\\wmi -ClassName WmiMonitorID | ForEach-Object { "
                        "(($_.UserFriendlyName | Where-Object {$_ -ne 0} | ForEach-Object {[char]$_}) -join '') }"],
                       capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=60)
    names = [n.strip() for n in p.stdout.splitlines() if n.strip()]
    r.info('Windows が認識しているモニタ: ' + (', '.join(names) if names else '(名前なし)'))
    if not any('optoma' in n.lower() for n in names):
        r.warn('モニタ名に "Optoma" が無い（別のプロジェクタか、名前を返さない接続）。以降の確認で解像度から判定する')


# ---------------------------------------------------------------- 2. アプリから見えるか
def check_monitors(r):
    out = v.app(['--list-monitors'], kind='release').stdout
    lines = [l for l in out.splitlines() if l.startswith('monitor[')]
    r.info('```\n' + '\n'.join(lines) + '\n```')
    if len(lines) < 2:
        r.fail('モニタが 1 台しか見えない。Win + P で「拡張」にする（「複製」だと別の画面を出せない）')
        return
    proj = [l for l in lines if ' projector' in l]
    if not proj:
        r.fail('プロジェクタのモニタが見つからない（config の monitor_name / monitor_index を確認）')
        return
    m = re.search(r'(\d+)x(\d+)@(\d+)Hz', proj[0])
    w, h, hz = int(m.group(1)), int(m.group(2)), int(m.group(3))
    r.info(f'プロジェクタ: {w}×{h} @ {hz} Hz')
    if (w, h) != (WIDTH, HEIGHT):
        r.fail(f'解像度が {w}×{h}。ネイティブの {WIDTH}×{HEIGHT} にする（設定 → ディスプレイ → 解像度）。'
               '違う解像度だと縮小・拡大で星がぼける')
    if hz < 59:
        r.warn(f'リフレッシュレートが {hz} Hz（60 Hz が望ましい）')


# ---------------------------------------------------------------- 3. 全画面でプロジェクタに出るか
def check_output(r):
    shots = OUT
    log = OUT / 'output.log'
    path = shots / 'output.png'
    with open(log, 'wb') as f:
        proc = subprocess.Popen([str(v.exe('release')), '--output', 'projector', '--operator', '--location', 'achi',
                                 '--time', '2026-11-02 22:00', '--compass', '--info', '--screenshot', str(path),
                                 '--wait', '4'], cwd=v.ROOT, env=v.ENV, stdout=f, stderr=subprocess.STDOUT)
    try:
        wins = []
        for _ in range(60):  # ウィンドウが 2 つ出るまで待つ
            time.sleep(0.1)
            wins = windows_of(proc.pid)
            if len(wins) >= 2:
                time.sleep(0.8)  # 全画面への切り替えを待つ
                wins = windows_of(proc.pid)
                break
        proc.wait(30)
    finally:
        if proc.poll() is None:
            proc.kill()
    text = log.read_text(encoding='utf-8', errors='replace')
    line = next((l for l in text.splitlines() if l.startswith('[output]')), '(ログなし)')
    r.info(f'起動ログ: `{line}`')
    if 'fullscreen on monitor' not in line:
        r.fail('プロジェクタに全画面で出せていない')
    output = [w for w in wins if not w[1].startswith('操作パネル')]
    panel = [w for w in wins if w[1].startswith('操作パネル')]
    if not output:
        r.fail('出力ウィンドウが見つからない')
    else:
        _, _, rect, mon, primary = output[0]
        r.info(f'出力ウィンドウ: {size(rect)[0]}×{size(rect)[1]}、載っているモニタ {size(mon)[0]}×{size(mon)[1]}'
               f'（{"主モニタ" if primary else "プロジェクタ側"}）')
        if primary:
            r.fail('出力ウィンドウが PC 本体の画面に出ている')
        if rect != mon:
            r.fail('出力ウィンドウがモニタ全体を覆っていない（全画面になっていない）')
        if size(mon) != (WIDTH, HEIGHT):
            r.fail(f'出力先のモニタが {size(mon)[0]}×{size(mon)[1]}（{WIDTH}×{HEIGHT} のはず）')
    if not panel:
        r.fail('操作パネルが出ていない')
    else:
        r.info(f'操作パネル: {"主モニタ（PC の画面）" if panel[0][4] else "プロジェクタ側（PC の画面に出すべき）"}')
        if not panel[0][4]:
            r.fail('操作パネルがプロジェクタ側に出ている')
    if path.exists():
        w, h, _ = pngtools.read_png(path)
        r.info(f'出力画像: {w}×{h}')
        if (w, h) != (WIDTH, HEIGHT):
            r.fail(f'出力画像が {w}×{h}（{WIDTH}×{HEIGHT} のはず）')
        r.images.append(('output.png', path, '11/2 22:00 阿智村の星空（方位・画面表示つき）。プロジェクタに出したものと同じ画像'))
    else:
        r.fail('出力画像が撮れない')
    panel_png = OUT / 'output_operator.png'
    if panel_png.exists():
        r.images.append(('output_operator.png', panel_png, '操作パネル（PC の画面）。上にプレビュー、下に状態とキー一覧'))


# ---------------------------------------------------------------- 4. 電源と速さ
def on_battery():
    """バッテリーで動いているか（Win32_Battery.BatteryStatus = 1 は放電中）。電池の無い PC は False"""
    p = subprocess.run(['powershell.exe', '-NoProfile', '-Command', '(Get-CimInstance Win32_Battery).BatteryStatus'],
                       capture_output=True, text=True, timeout=60)
    return p.stdout.strip() == '1'


def check_power(r):
    raw = subprocess.run(['powercfg', '/getactivescheme'], capture_output=True, timeout=60).stdout
    try:  # コンソールの設定により UTF-8 か Shift_JIS で返る
        text = raw.decode('utf-8')
    except UnicodeDecodeError:
        text = raw.decode('cp932', errors='replace')
    r.info('電源プラン: ' + text.strip().split('(')[-1].rstrip(')'))
    if on_battery():
        r.warn('バッテリーで動いている。内蔵 GPU は電池だと性能を落とし、映像がカクつくことがある。上映は充電器につないで行う')
    else:
        r.info('電源: AC（充電器につながっている）')


def measure(args, times):
    fps = []
    for _ in range(times):
        p = v.app(['--output', 'projector', '--location', 'achi', '--time', '2026-11-02 22:00', '--constellations',
                   '--constellation-names', '--labels', '--benchmark', 4] + args, kind='release')
        m = re.search(r'= ([\d.]+) fps', p.stdout)
        if m:
            fps.append(float(m.group(1)))
    return fps


def check_speed(r):
    battery = on_battery()
    rows = []
    # 垂直同期あり（上映と同じ）は 3 回測って真ん中の値。プロジェクタの 60 Hz に毎回間に合っているか
    for title, extra, times, minimum in (('垂直同期あり（上映と同じ）', ['--keep-vsync'], 3, REFRESH - 5),
                                         ('垂直同期なし（余裕）', [], 1, REFRESH)):
        fps = measure(extra, times)
        if not fps:
            r.fail(f'{title}: 計測できない')
            continue
        median = sorted(fps)[len(fps) // 2]
        ok = median >= minimum
        rows.append(f'| {title} | {median:.0f}（{", ".join(f"{x:.0f}" for x in fps)}） | {"OK" if ok else "**不足**"} |')
        if not ok:
            message = f'{title}: {median:.0f} fps（{minimum} fps 以上のはず）'
            if battery:
                r.warn(message + '。バッテリー駆動のため。充電器につないでもう一度確かめる')
            else:
                r.fail(message)
    r.info('プロジェクタに全画面（1280×800）、星座線・名前つき、Release ビルドで 4 秒ずつ'
           + ('。**バッテリー駆動中**' if battery else '') +
           '\n\n| 条件 | fps（中央値、各回） | 結果 |\n|---|---|---|\n' + '\n'.join(rows))


# ---------------------------------------------------------------- 5. キー操作（全画面のプロジェクタ出力で）
KEYS = [('X', 0x58, False, '[toast] テストパターン ON'), ('X', 0x58, False, '[toast] テストパターン OFF'),
        ('K', 0x4B, False, '[toast] 星座: 線'), ('F12', 0x7B, False, '[toast] クレジット ON'),
        ('F12', 0x7B, False, '[toast] クレジット OFF'), ('F11', 0x7A, False, '[view] fullscreen toggled'),
        ('F11', 0x7A, False, '[view] fullscreen toggled'), ('Esc', 0x1B, False, '[quit]')]


def check_keys(r):
    log = OUT / 'keys.log'
    with open(log, 'wb') as f:
        proc = subprocess.Popen([str(v.exe('release')), '--output', 'projector', '--location', 'achi'], cwd=v.ROOT, env=v.ENV,
                                stdout=f, stderr=subprocess.STDOUT)
    read = lambda: log.read_text(encoding='utf-8', errors='replace')  # noqa: E731
    try:
        hwnd = None
        for _ in range(150):
            outs = [w for w in windows_of(proc.pid) if not w[1].startswith('操作パネル')]
            hwnd = outs[0][0] if outs else None
            if hwnd and 'Esc quit' in read():
                break
            time.sleep(0.1)
        if not hwnd:
            r.fail('出力ウィンドウが見つからない')
            return
        rows = []
        for name, vk, ext, expect in KEYS:
            before = len(read())
            v.press(hwnd, vk, ext)
            ok = False
            for _ in range(40):
                time.sleep(0.1)
                if expect in read()[before:]:
                    ok = True
                    break
            rows.append(f'| {name} | `{expect}` | {"OK" if ok else "**FAIL**"} |')
            if not ok:
                r.fail(f'{name} キーで `{expect}` が出ない')
        r.info('全画面のプロジェクタ出力にキー入力を送った\n\n'
               '| キー | ログに出るはずの行 | 結果 |\n|---|---|---|\n' + '\n'.join(rows))
    finally:
        try:
            proc.wait(5)
        except subprocess.TimeoutExpired:
            proc.kill()


# ---------------------------------------------------------------- 6. 投影する画像（テストパターン・上映）
def check_images(r):
    for name, args, note in (
            ('test_pattern.png', ['--test-pattern'], 'テストパターン（灰色 17 段・暗部 9 段・市松模様・格子・方位）'),
            ('show.png', ['--scenario', v.SHOW, '--play', '--seek', 85], '上映 1:25（夏の大三角とベガ・アルタイル）')):
        path = OUT / name
        v.app(['--output', 'projector'] + args + ['--screenshot', path], kind='release')
        if path.exists():
            r.images.append((name, path, note))
        else:
            r.fail(f'{name} が撮れない')


# ---------------------------------------------------------------- 7. 人が投影を見て確かめる（--manual）
MANUAL = [
    ('テストパターン', ['--test-pattern'], [
        '下端の灰色 17 段が、いちばん暗い段からいちばん明るい段まで全部見分けられる（だめなら render.gamma を変える）',
        '上端の暗い 9 段のうち、何段目から見えるか（2〜3 段目から見えれば暗い星も見える）',
        '中央と四隅の市松模様が、灰色に溶けずに白黒の細かい模様に見える（だめならピントを合わせる）',
        '格子がまっすぐで等間隔（台形になっていたら投影の角度を直す）',
        '赤い線（北）が部屋の北を向いている（違えば projection.rotation_deg で回す）',
    ]),
    ('星空（阿智村 11/2 22:00）', ['--location', 'achi', '--time', '2026-11-02 22:00', '--compass'], [
        '星が「点」に見え、明るい星と暗い星の差が分かる',
        '天の川がうっすら見える（部屋を暗くした状態で）',
        '北・東・南・西の文字が読める',
        '空の背景が十分に暗い（黒浮きが気にならない）',
    ]),
    ('上映（最初の 1 分）', ['--scenario', v.SHOW, '--play'], [
        'ナレーションと BGM が聞こえ、音量のバランスがよい',
        '字幕が読める大きさ・位置',
        '大阪 → 阿智村の切り替えと、星が増えていく様子が自然',
    ]),
]


def run_manual(r):
    print('\nこれから順にプロジェクタへ映します。投影を見て、各項目に y（はい）/ n（いいえ）で答えてください。')
    print('次へ進むと、映していたアプリは自動で終了します。')
    for title, args, items in MANUAL:
        input(f'\n[{title}] Enter で投影を始めます...')
        proc = subprocess.Popen([str(v.exe('release')), '--output', 'projector', '--operator'] + args, cwd=v.ROOT, env=v.ENV,
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        rows = []
        try:
            for item in items:
                answer = ''
                while answer not in ('y', 'n'):
                    answer = input(f'  {item} (y/n): ').strip().lower()
                rows.append(f'| {item} | {"はい" if answer == "y" else "**いいえ**"} |')
                if answer == 'n':
                    r.warn(f'{title}: {item} → いいえ')
        finally:
            proc.kill()
        r.info(f'**{title}**\n\n| 確かめたこと | 結果 |\n|---|---|\n' + '\n'.join(rows))


# ---------------------------------------------------------------- まとめ
def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--manual', action='store_true', help='続けて、人が投影を見て確かめる手順を行う')
    parser.add_argument('--no-build', action='store_true', help='ビルドを省く')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    for old in OUT.glob('*'):
        if old.is_file():
            old.unlink()

    checks = [('windows', 'Windows がプロジェクタを認識しているか', check_windows),
              ('monitors', 'アプリから見たモニタ（拡張表示・解像度）', check_monitors),
              ('output', 'プロジェクタに全画面で出る・操作パネルは PC 側', check_output),
              ('power', '電源（上映は充電器につなぐ）', check_power),
              ('speed', 'プロジェクタ出力での描画速度', check_speed),
              ('keys', '全画面のプロジェクタ出力でのキー操作', check_keys),
              ('images', 'プロジェクタに出す画像（テストパターン・上映）', check_images)]
    if args.manual:
        checks.append(('manual', '人が投影を見て確かめた結果', run_manual))
    results = []
    if not args.no_build:
        r = v.Result('build', 'ビルド（プロジェクタの確認は Release ビルドを使う）')
        print('== ビルド ...', flush=True)
        v.check_build(r)
        print(f'   {r.status}', flush=True)
        results.append(r)
        if r.status == 'FAIL':
            checks = []
    for name, title, func in checks:
        r = v.Result(name, title)
        print(f'== {title} ...', flush=True)
        try:
            func(r)
        except Exception as e:  # 1 つ壊れても残りは続ける
            r.fail(f'例外: {e!r}')
        print(f'   {r.status}', flush=True)
        results.append(r)
        if name == 'monitors' and r.status == 'FAIL':
            print('プロジェクタが使えないので中止')
            break

    images = [img for r in results for img in r.images]
    if images:
        pngtools.contact_sheet(OUT / 'sheet.png', [p for _, p, _ in images], 2)
    icons = {'PASS': '✅', 'FAIL': '❌', 'WARN': '⚠️', 'SKIP': '⏭'}
    lines = ['# プロジェクタの動作確認', '', f'実行: {time.strftime("%Y-%m-%d %H:%M:%S")}　'
             f'`python tests/verify/projector.py {" ".join(sys.argv[1:])}`', '', '| 確認 | 結果 |', '|---|---|']
    lines += [f'| {r.title} | {icons[r.status]} {r.status} |' for r in results]
    for r in results:
        lines += ['', f'## {r.title}: {r.status}', ''] + r.lines
    if images:
        lines += ['', '## 目で確かめる画像', '', '![sheet](projector/sheet.png)', '', '左上から順に:', '']
        lines += [f'{i + 1}. `{name}` — {note}' for i, (name, _, note) in enumerate(images)]
    report = v.OUT / 'projector_report.md'
    report.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    failed = [r.name for r in results if r.status == 'FAIL']
    warned = [r.name for r in results if r.status == 'WARN']
    print(f'\nreport: {report}\nimage:  {OUT / "sheet.png"}')
    print('RESULT:', f'FAILED ({", ".join(failed)})' if failed else
          'PASSED' + (f' with warnings ({", ".join(warned)})' if warned else ''))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
