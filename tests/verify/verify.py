"""これまでの検証をまとめて実行し、結果を build/verify/report.md に書く。

    python tests/verify/verify.py                 # すべて（3〜5 分）
    python tests/verify/verify.py --quick         # 描画速度と Stellarium を省く（1〜2 分）
    python tests/verify/verify.py --only keys,motion

MSYS2 Bash でも PowerShell でも同じコマンドで動く（Python の標準ライブラリだけを使う）。
各検証は PASS / FAIL / SKIP を返し、1 つでも FAIL なら終了コード 1。
目視で確かめる画像（シーン・モードの一覧）も build/verify/ に作る。手順の説明は tests/verify/README.md。
"""

import argparse
import ctypes
import math
import os
import re
import shutil
import subprocess
import sys
import time
from ctypes import wintypes
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pngtools  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build' / 'verify'
MINGW = Path('C:/msys64/mingw64/bin')
ENV = dict(os.environ, PATH=f'{MINGW};{os.environ.get("PATH", "")}')
CHECKS = ['build', 'unit', 'keys', 'motion', 'benchmark', 'stellarium', 'screenshots']


class Result:
    def __init__(self, name, title):
        self.name, self.title = name, title
        self.status, self.lines, self.images = 'PASS', [], []

    def fail(self, msg):
        self.status = 'FAIL'
        self.lines.append(f'**FAIL** {msg}')

    def skip(self, msg):
        self.status = 'SKIP'
        self.lines.append(msg)

    def info(self, msg):
        self.lines.append(msg)


def run(cmd, timeout=600, **kw):
    return subprocess.run([str(c) for c in cmd], cwd=ROOT, env=ENV, timeout=timeout, capture_output=True,
                          text=True, encoding='utf-8', errors='replace', **kw)


def exe(kind='debug'):
    return ROOT / 'build' / kind / 'planetarium.exe'


def app(args, kind='debug', timeout=120):
    """アプリを起動して終了まで待つ（--screenshot / --record / --benchmark / --dump-positions 用）"""
    return run([exe(kind)] + [str(a) for a in args], timeout=timeout)


def write_config(name, width=None, height=None, scenario=None, projection=None):
    """config/projector.yaml を元に、解像度・シナリオ・投影を差し替えた設定を build/verify に作る"""
    text = (ROOT / 'config' / 'projector.yaml').read_text(encoding='utf-8')
    if width:
        text = re.sub(r'^  width: \d+', f'  width: {width}', text, flags=re.M)
        text = re.sub(r'^  height: \d+', f'  height: {height}', text, flags=re.M)
    if projection:
        for key, value in projection.items():
            text = re.sub(rf'^  {key}: \S+', f'  {key}: {value}', text, count=1, flags=re.M)
    if scenario:
        text = text[:text.index('\nscenario:')] + '\n' + scenario
    path = OUT / name
    path.write_text(text, encoding='utf-8')
    return path


# ---------------------------------------------------------------- ビルド
def check_build(r):
    for preset in ('mingw-debug', 'mingw-release'):
        for cmd in (['cmake', '--preset', preset], ['cmake', '--build', '--preset', preset]):
            p = run(cmd)
            if p.returncode != 0:
                r.fail(f'`{" ".join(cmd)}` failed')
                r.info('```\n' + (p.stdout + p.stderr)[-3000:] + '\n```')
                return
        warnings = [l for l in (p.stdout + p.stderr).splitlines() if 'warning' in l and 'third_party' not in l]
        if warnings:
            r.fail(f'{preset}: {len(warnings)} warnings')
            r.info('```\n' + '\n'.join(warnings[:20]) + '\n```')
        else:
            r.info(f'{preset}: ビルド成功、警告なし')


# ---------------------------------------------------------------- 自動テスト
def check_unit(r):
    p = run(['ctest', '--preset', 'mingw-debug', '--output-on-failure'])
    summary = [l for l in p.stdout.splitlines() if 'tests passed' in l]
    r.info(summary[0] if summary else '(ctest の結果が読めない)')
    for name in ('astro', 'clock', 'controls'):
        out = run([ROOT / 'build' / 'debug' / f'{name}_test.exe']).stdout
        ok, ng = out.count('[ OK ]'), out.count('[FAIL]')
        r.info(f'- {name}: {ok} 項目合格、{ng} 項目失敗')
        if ng:
            r.info('```\n' + '\n'.join(l for l in out.splitlines() if '[FAIL]' in l) + '\n```')
    if p.returncode != 0:
        r.fail('ctest に失敗したテストがある')


# ---------------------------------------------------------------- キー操作（実際のウィンドウ）
user32 = ctypes.WinDLL('user32', use_last_error=True)
user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
user32.MapVirtualKeyW.argtypes = [wintypes.UINT, wintypes.UINT]
user32.MapVirtualKeyW.restype = wintypes.UINT
EnumProc = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)


def find_window(pid):
    found = []

    def callback(hwnd, _):
        owner = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        name = ctypes.create_unicode_buffer(64)
        user32.GetClassNameW(hwnd, name, 64)
        if owner.value == pid and name.value.startswith('GLFW') and user32.IsWindowVisible(hwnd):
            found.append(hwnd)
        return True

    user32.EnumWindows(EnumProc(callback), 0)
    return found[0] if found else None


def press(hwnd, vk, extended=False):
    """WM_KEYDOWN / WM_KEYUP を送る。GLFW は lParam のスキャンコードからキーを判定する"""
    scan = user32.MapVirtualKeyW(vk, 0)
    down = 1 | (scan << 16) | ((1 << 24) if extended else 0)
    up = down | (1 << 30) | (1 << 31)
    user32.PostMessageW(hwnd, 0x0100, vk, down)
    user32.PostMessageW(hwnd, 0x0101, vk, up)


# (表示名, 仮想キー, 拡張キー, ログに出るはずの文字列)
KEY_STEPS = [
    ('3', 0x33, False, '[mode] 3: 流星群'), ('0', 0x30, False, '[mode] 0: リアル'),
    ('M', 0x4D, False, '[mode] 1: オーロラ'), ('L', 0x4C, False, '[location]'), ('S', 0x53, False, '[season]'),
    ('F3', 0x72, False, '[toast] 季節: 夏'), ('T', 0x54, False, '[toast] 季節の自動巡回 ON'),
    ('T', 0x54, False, '[toast] 季節の自動巡回 OFF'), ('Up', 0x26, True, '[toast] 時間 x10'),
    ('Down', 0x28, True, '[toast] 時間 x1'), ('Space', 0x20, False, '（一時停止）'), ('R', 0x52, False, '（逆再生）'),
    ('N', 0x4E, False, '[time] now, x1'), ('K', 0x4B, False, '[toast] 星座線 ON'), ('J', 0x4A, False, '[toast] 名前 ON'),
    ('C', 0x43, False, '[toast] 方位 ON'), ('G', 0x47, False, '[toast] グリッド ON'), ('I', 0x49, False, '[view] info ON'),
    ('V', 0x56, False, '[view] perspective (south)'), ('V', 0x56, False, '[view] fisheye (zenith)'),
    ('P', 0x50, False, '[scenario] start'), ('P', 0x50, False, '[scenario] stop'),
    ('F11', 0x7A, False, '[view] fullscreen toggled'), ('F11', 0x7A, False, '[view] fullscreen toggled'),
    ('H', 0x48, False, '---- keys ----'), ('Esc', 0x1B, False, '[quit]'),
]


def check_keys(r):
    log = OUT / 'keys.log'
    with open(log, 'wb') as f:
        proc = subprocess.Popen([str(exe()), '--location', 'osaka'], cwd=ROOT, env=ENV, stdout=f, stderr=subprocess.STDOUT)
    read = lambda: log.read_text(encoding='utf-8', errors='replace')  # noqa: E731
    try:
        hwnd = None
        for _ in range(150):  # 起動の最後（キー一覧）とウィンドウを待つ
            hwnd = hwnd or find_window(proc.pid)
            if hwnd and 'Esc quit' in read():
                break
            time.sleep(0.1)
        if not hwnd:
            r.fail('ウィンドウが見つからない')
            return
        time.sleep(0.3)
        rows = []
        for name, vk, ext, expect in KEY_STEPS:
            before = len(read())
            press(hwnd, vk, ext)
            ok = False
            for _ in range(30):  # 最大 3 秒（全画面の切り替えは時間がかかる）
                time.sleep(0.1)
                if expect in read()[before:]:
                    ok = True
                    break
            rows.append(f'| {name} | `{expect}` | {"OK" if ok else "**FAIL**"} |')
            if not ok:
                r.fail(f'{name} キーで `{expect}` が出ない')
        try:
            proc.wait(5)
        except subprocess.TimeoutExpired:
            r.fail('Esc で終了しない')
        r.info(f'実際のウィンドウに {len(KEY_STEPS)} 回キー入力を送った（Shift の組み合わせは unit の controls で確認）')
        r.info('| キー | ログに出るはずの行 | 結果 |\n|---|---|---|\n' + '\n'.join(rows))
    finally:
        if proc.poll() is None:
            proc.kill()


# ---------------------------------------------------------------- 動き
def record(r, name, args, count, interval, wait=0.0, config=None):
    prefix = OUT / 'motion' / name
    cmd = ([config] if config else []) + args + ['--record', prefix, '--count', count, '--interval', interval]
    if wait:
        cmd += ['--wait', wait]
    app(cmd)
    files = sorted((OUT / 'motion').glob(f'{name}_*.png'))
    if len(files) != count:
        r.fail(f'{name}: {count} 枚のはずが {len(files)} 枚')
        return None, files
    return [pngtools.read_png(f)[2] for f in files], files


def check_motion(r):
    (OUT / 'motion').mkdir(exist_ok=True)
    small = write_config('small.yaml', 640, 400)
    fade = write_config('fade.yaml', 640, 400, scenario='''scenario:
  loop: false
  steps:
    - { duration_sec: 1.0, location: achi, time: "2026-12-15 22:00", speed: 1, mode: real }
    - { duration_sec: 5.0, mode: jewel, message: "fade" }
''')
    base = ['--location', 'achi', '--time', '2026-12-15 22:00']
    rows = []

    def diffs(frames):
        return [pngtools.frame_diff(frames[i - 1], frames[i]) for i in range(1, len(frames))]

    # 瞬き（宝石箱）: 毎回はっきり変わる画素がある
    frames, files = record(r, 'twinkle_jewel', base + ['--mode', 'jewel'], 4, 0.15, config=small)
    if frames:
        d = diffs(frames)
        ok = all(c >= 1.0 for _, c in d)
        rows.append(('瞬き（宝石箱）', ', '.join(f'{c:.2f}%' for _, c in d), '変化した画素 ≥ 1%', ok))
    # 瞬き（リアル）: 控えめだが変化している
    frames, _ = record(r, 'twinkle_real', base + ['--mode', 'real'], 3, 0.15, config=small)
    if frames:
        d = diffs(frames)
        ok = all(m >= 0.05 for m, _ in d)
        rows.append(('瞬き（リアル）', ', '.join(f'平均差 {m:.3f}' for m, _ in d), '平均差 ≥ 0.05', ok))
    # オーロラ: 1 秒ごとに形が変わる
    frames, aurora_files = record(r, 'aurora', base + ['--mode', 'aurora'], 3, 1.0, config=small)
    if frames:
        d = diffs(frames)
        ok = all(c >= 0.2 for _, c in d)
        rows.append(('オーロラ', ', '.join(f'{c:.2f}%' for _, c in d), '変化した画素 ≥ 0.2%', ok))
        r.images.append(('motion_aurora.png', aurora_files, 3, 'オーロラが 1 秒ごとに形を変えている（カーテンの縦の筋）'))
    # 流れ星: 多くの間隔で何かが動いている
    frames, meteor_files = record(r, 'meteor', base + ['--mode', 'meteor'], 6, 0.2, wait=1.0, config=small)
    if frames:
        d = diffs(frames)
        moving = sum(1 for _, c in d if c > 0.005)
        ok = moving >= 3
        rows.append(('流れ星', ', '.join(f'{c:.3f}%' for _, c in d), '5 間隔のうち 3 以上で変化', ok))
        r.images.append(('motion_meteor.png', meteor_files, 3, '流れ星がフレームごとに現れて進む（細い光の線）'))
    # フェード: 明るさが 1 秒かけて段階的に変わる（一気に飛ばない）
    frames, fade_files = record(r, 'fade', ['--play'], 6, 0.25, wait=0.8, config=fade)
    if frames:
        b = [pngtools.mean_brightness(f) for f in frames]
        lo, hi = b[0], b[-1]
        middle = [x for x in b[1:-1] if lo + 0.1 * (hi - lo) < x < hi - 0.1 * (hi - lo)]
        ok = hi > lo * 1.3 and len(middle) >= 2 and all(b[i] <= b[i + 1] + 0.3 for i in range(len(b) - 1))
        rows.append(('フェード', ' → '.join(f'{x:.1f}' for x in b), '中間の明るさのフレームが 2 枚以上', ok))
        r.images.append(('motion_fade.png', fade_files, 3, 'リアルから宝石箱へ 1 秒かけて切り替わる（途中のフレームは中間の明るさ）'))
    r.info('| 動き | 測った値 | 合格の条件 | 結果 |\n|---|---|---|---|\n' +
           '\n'.join(f'| {a} | {b} | {c} | {"OK" if ok else "**FAIL**"} |' for a, b, c, ok in rows))
    for a, _, _, ok in rows:
        if not ok:
            r.fail(f'{a} の動きが基準に届かない')


# ---------------------------------------------------------------- 描画速度
BENCH = [('リアル（阿智村）', ['--mode', 'real']), ('リアル（大阪）', ['--mode', 'real', '--location', 'osaka']),
         ('リアル + 星座線・名前・方位・画面表示', ['--mode', 'real', '--constellations', '--labels', '--compass', '--info']),
         ('オーロラ', ['--mode', 'aurora']), ('星雲', ['--mode', 'nebula']), ('流星群', ['--mode', 'meteor']),
         ('宝石箱', ['--mode', 'jewel']), ('ワープ航行', ['--mode', 'warp']), ('天の川', ['--mode', 'milkyway']),
         ('水鏡', ['--mode', 'mirror'])]


def check_benchmark(r, seconds=3.0, minimum=60.0):
    rows = []
    for title, args in BENCH:
        p = app(['--location', 'achi', '--time', '2026-12-15 22:00', '--benchmark', seconds] + args, kind='release')
        m = re.search(r'= ([\d.]+) fps \(([\d.]+) ms/frame\)', p.stdout)
        if not m:
            r.fail(f'{title}: 計測できない')
            continue
        fps = float(m.group(1))
        rows.append(f'| {title} | {fps:.0f} | {m.group(2)} ms | {"OK" if fps >= minimum else "**FAIL**"} |')
        if fps < minimum:
            r.fail(f'{title}: {fps:.0f} fps < {minimum:.0f} fps')
    r.info(f'Release ビルド、1280×800、垂直同期なしで {seconds:.0f} 秒ずつ。合格は {minimum:.0f} fps 以上')
    r.info('| 条件 | fps | 1 フレーム | 結果 |\n|---|---|---|---|\n' + '\n'.join(rows))


# ---------------------------------------------------------------- Stellarium との比較
STEL_CASES = [('osaka', 135.52, 34.82, 50, '2026-12-15T13:00:00', '2026-12-15 22:00'),
              ('achi', 137.63, 35.47, 1400, '2027-08-10T12:00:00', '2027-08-10 21:00')]
STEL_NAMES = ['Sirius', 'Betelgeuse', 'Rigel', 'Capella', 'Vega', 'Polaris', 'Aldebaran', 'Deneb',
              'Sun', 'Moon', 'Mercury', 'Venus', 'Mars', 'Jupiter', 'Saturn', 'Uranus']
PLANETS = {'Sun', 'Mercury', 'Venus', 'Mars', 'Jupiter', 'Saturn', 'Uranus'}
LIMITS = {'恒星': 60.0, '太陽・惑星': 30.0, '月': 30.0}  # 秒角。魚眼の 1 画素 ≒ 810″


def stellarium_exe():
    for base in (Path(os.environ.get('LOCALAPPDATA', '')) / 'Programs', Path('C:/Program Files')):
        p = base / 'Stellarium' / 'stellarium.exe'
        if p.exists():
            return p
    return None


def check_stellarium(r):
    stel = stellarium_exe()
    if not stel:
        r.skip('Stellarium が入っていない（`winget install Stellarium.Stellarium` で入れると比較できる）')
        return
    user_dir = OUT / 'stellarium'
    (user_dir / 'scripts').mkdir(parents=True, exist_ok=True)
    output = user_dir / 'output.txt'
    output.unlink(missing_ok=True)
    cases = ',\n'.join(f'  {{tag: "{t}", lon: {lon}, lat: {lat}, alt: {alt}, utc: "{utc}"}}'
                       for t, lon, lat, alt, utc, _ in STEL_CASES)
    (user_dir / 'scripts' / 'compare.ssc').write_text(f'''// 天体の位置（大気差なし）を output.txt に書いて終了する（tests/verify/verify.py が作る）
var names = {STEL_NAMES};
var cases = [
{cases}
];
for (var c = 0; c < cases.length; c++) {{
  var k = cases[c];
  core.setObserverLocation(k.lon, k.lat, k.alt, 0, k.tag, "Earth");
  core.setDate(k.utc, "utc");
  core.setTimeRate(0);
  core.wait(2);
  core.output("# " + k.tag);
  for (var i = 0; i < names.length; i++) {{
    var info = core.getObjectInfo(names[i]);
    core.output(names[i] + " " + info["altitude-geometric"] + " " + info["azimuth-geometric"]);
  }}
}}
core.quitStellarium();
'''.replace("'", '"'), encoding='utf-8')
    try:
        subprocess.run([str(stel), '--user-dir', str(user_dir), '--startup-script', 'compare.ssc',
                        '--full-screen', 'no'], timeout=240)
    except subprocess.TimeoutExpired:
        r.fail('Stellarium が終了しない')
        return
    if not output.exists():
        r.fail('Stellarium の出力（output.txt）が無い')
        return
    theirs, tag = {}, None
    for line in output.read_text(encoding='utf-8').splitlines():
        parts = line.split()
        if parts and parts[0] == '#':
            tag = parts[1]
        elif len(parts) >= 3:
            theirs[(tag, parts[0])] = (float(parts[1]), float(parts[2]))

    rows, worst = [], {k: 0.0 for k in LIMITS}
    for tag, _, _, _, _, local in STEL_CASES:
        p = app(['--location', tag, '--time', local, '--dump-positions'])
        for line in p.stdout.splitlines():
            parts = line.split()
            if line.startswith('#') or len(parts) != 3 or (tag, parts[0]) not in theirs:
                continue
            name, a, z = parts[0], float(parts[1]), float(parts[2])
            sa, sz = theirs[(tag, name)]
            rad = math.radians
            cosd = math.sin(rad(a)) * math.sin(rad(sa)) + math.cos(rad(a)) * math.cos(rad(sa)) * math.cos(rad(z - sz))
            sep = math.degrees(math.acos(max(-1.0, min(1.0, cosd)))) * 3600
            kind = '月' if name == 'Moon' else '太陽・惑星' if name in PLANETS else '恒星'
            worst[kind] = max(worst[kind], sep)
            rows.append(f'| {tag} | {name} | {a:.3f} | {sa:.3f} | {z:.3f} | {sz:.3f} | {sep:.1f}″ |')
    if len(rows) != len(STEL_CASES) * len(STEL_NAMES):
        r.fail(f'比較できた天体が {len(rows)} 個しかない')
    for kind, limit in LIMITS.items():
        r.info(f'- {kind}: 最大の差 {worst[kind]:.1f}″（合格 ≤ {limit:.0f}″）')
        if worst[kind] > limit:
            r.fail(f'{kind}の差 {worst[kind]:.1f}″ が {limit:.0f}″ を超えた')
    r.info('| 場所 | 天体 | 高度（本） | 高度（St） | 方位（本） | 方位（St） | 角距離 |\n|---|---|---|---|---|---|---|\n' +
           '\n'.join(rows))


# ---------------------------------------------------------------- 目視確認用の画像
SCENES = [
    ('constellations', ['--location', 'achi', '--time', '2026-12-15 22:00', '--constellations', '--labels', '--compass', '--info'],
     '北が上・東が左。オリオン座・シリウスが南東（左下）。名前が重なっていない。左下に日時'),
    ('osaka', ['--location', 'osaka', '--time', '2026-12-15 22:00', '--compass'],
     '大阪: 空がオレンジ色に明るく、縁に街並み（窓明かり）、星が少ない、天の川なし'),
    ('achi_summer', ['--location', 'achi', '--time', '2026-08-10 21:00', '--compass'],
     '阿智村の夏: 天の川が南（下）で太く明るく、暗黒帯で分かれる。縁に山並み'),
    ('dusk', ['--location', 'achi', '--time', '2026-12-15 17:10'],
     '夕方: 南西（右下）の地平線が明るく、明るい星と三日月だけが見える'),
    ('full_moon', ['--location', 'achi', '--time', '2026-12-24 21:00', '--labels'],
     '満月: ふたご座の月が丸く光り、空が青白く明るく、星が減る'),
    ('perspective', ['PERSPECTIVE', '--location', 'achi', '--time', '2026-12-15 22:00', '--grid', '--compass'],
     '透視（南向き）: 下に山並みと「南」、左にオリオン座とシリウス、グリッドは山の手前で止まる'),
]
MODES = [('aurora', 0, '北の空に緑のカーテン'), ('nebula', 0, '星と一緒に動く色の雲'),
         ('meteor', 2.0, '流れ星（細い線）'), ('constellation', 0, '金色の星座線と星座名'),
         ('jewel', 0, '色鮮やかで大きな星、十字の光条'), ('warp', 2.0, '中心から外へ流れる光跡'),
         ('milkyway', 0, '明るい天の川'), ('mirror', 0, '外側の輪が水面で、星空が映る')]


def check_screenshots(r):
    shots = OUT / 'screenshots'
    shots.mkdir(exist_ok=True)
    perspective = write_config('perspective.yaml', projection={'type': 'perspective', 'fov_deg': 70, 'center_alt_deg': 25})
    scene_files, notes = [], []
    for name, args, note in SCENES:
        args = [perspective if a == 'PERSPECTIVE' else a for a in args]
        path = shots / f'{name}.png'
        app(args + ['--screenshot', path])
        if not path.exists():
            r.fail(f'{name}.png が作れない')
            continue
        scene_files.append(path)
        notes.append(f'{len(notes) + 1}. `{name}` — {note}')
    mode_files, mode_notes = [], []
    for mode, wait, note in MODES:
        path = shots / f'mode_{mode}.png'
        app(['--location', 'achi', '--time', '2026-12-15 22:00', '--mode', mode, '--screenshot', path] +
            (['--wait', wait] if wait else []))
        if not path.exists():
            r.fail(f'mode_{mode}.png が作れない')
            continue
        mode_files.append(path)
        mode_notes.append(f'{len(mode_notes) + 1}. `{mode}` — {note}')
    r.images.append(('sheet_scenes.png', scene_files, 3, '左上から順に:\n' + '\n'.join(notes)))
    r.images.append(('sheet_modes.png', mode_files, 3, '左上から順に:\n' + '\n'.join(mode_notes)))
    r.info(f'{len(scene_files) + len(mode_files)} 枚を `build/verify/screenshots/` に保存（1 枚ずつ見るとき用）')


# ---------------------------------------------------------------- まとめ
def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--only', help='実行する検証（カンマ区切り）: ' + ','.join(CHECKS))
    parser.add_argument('--quick', action='store_true', help='描画速度と Stellarium を省く')
    parser.add_argument('--no-build', action='store_true', help='ビルドを省く（ビルド済みのものを使う）')
    args = parser.parse_args()
    selected = args.only.split(',') if args.only else list(CHECKS)
    if args.quick:
        selected = [c for c in selected if c not in ('benchmark', 'stellarium')]
    if args.no_build:
        selected = [c for c in selected if c != 'build']

    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True)

    titles = {'build': 'ビルド（Debug・Release、警告なし）', 'unit': '自動テスト（ctest）',
              'keys': 'キー操作（実際のウィンドウにキー入力を送る）', 'motion': '動き（連続撮影してフレームを比べる）',
              'benchmark': '描画速度', 'stellarium': 'Stellarium との天体位置の比較', 'screenshots': '目視確認用の画像'}
    funcs = {'build': check_build, 'unit': check_unit, 'keys': check_keys, 'motion': check_motion,
             'benchmark': check_benchmark, 'stellarium': check_stellarium, 'screenshots': check_screenshots}
    results = []
    for name in CHECKS:
        if name not in selected:
            continue
        r = Result(name, titles[name])
        print(f'== {r.title} ...', flush=True)
        start = time.time()
        try:
            funcs[name](r)
        except Exception as e:  # 1 つの検証が壊れても残りは続ける
            r.fail(f'例外: {e!r}')
        print(f'   {r.status} ({time.time() - start:.0f} s)', flush=True)
        results.append(r)
        if name == 'build' and r.status == 'FAIL':
            print('ビルドに失敗したので中止')
            break

    # 一覧画像を作る
    sheets = []
    for r in results:
        for file, images, cols, note in r.images:
            if images:
                pngtools.contact_sheet(OUT / file, images, cols)
                sheets.append((file, note))

    lines = ['# 検証レポート', '', f'実行: {time.strftime("%Y-%m-%d %H:%M:%S")}　`python tests/verify/verify.py {" ".join(sys.argv[1:])}`', '',
             '| 検証 | 結果 |', '|---|---|']
    lines += [f'| {r.title} | {"✅" if r.status == "PASS" else "❌" if r.status == "FAIL" else "⏭"} {r.status} |' for r in results]
    for r in results:
        lines += ['', f'## {r.title}: {r.status}', ''] + r.lines
    if sheets:
        lines += ['', '## 目で確かめる画像', '',
                  '数値では確かめにくい見た目は、次の一覧画像を開いて、書いてあるとおりに見えるかを確かめる。', '']
        for file, note in sheets:
            lines += [f'### {file}', '', f'![{file}]({file})', '', note, '']
    (OUT / 'report.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')

    failed = [r for r in results if r.status == 'FAIL']
    print()
    print(f'report: {OUT / "report.md"}')
    for file, _ in sheets:
        print(f'image:  {OUT / file}')
    print('RESULT:', 'FAILED (' + ', '.join(r.name for r in failed) + ')' if failed else 'PASSED')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
