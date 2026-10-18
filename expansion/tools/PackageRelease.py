"""Whitelisted Xsynth 2000 release staging/packaging. GPL-2.0-or-later.
Run only after the release gate approves the exact module. No installation.
"""
from pathlib import Path
import hashlib
import shutil
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT/'artifacts/final-release'
SOURCE = WORK/'source-stage'/'Xsynth-2000-0.1.0-source'
DIST = ROOT/'dist/release-0.1.0'
BINARY = DIST/'Xsynth-2000-0.1.0-Windows-x64'
SDK = ROOT.parent/'external/vst3sdk'
BUNDLE = ROOT/'build-release-0.1.0/VST3/Release/Xsynth 2000.vst3'


def copy(src, dst):
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def tree(src, dst):
    for f in sorted(src.rglob('*')):
        rel = f.relative_to(src)
        if f.is_file() and not any(p.startswith('.') or p == '__pycache__' for p in rel.parts):
            if f.is_symlink():
                raise RuntimeError(f'Unexpected source symlink: {f}')
            copy(f,dst/rel)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def stage_source():
    if SOURCE.exists():
        raise RuntimeError('Source staging exists; inspect it rather than silently replacing it.')
    product=SOURCE/'expansion'
    for name in ['CMakeLists.txt','COPYING','AUTHOR','CREDITS.md','SDK_REVISION.txt']:
        copy(ROOT/name,product/name)
    for f in ROOT.iterdir():
        if f.is_file() and f.suffix in {'.c','.h','.Xpatch'}:
            copy(f,product/f.name)
    tree(ROOT/'port',product/'port')
    tree(ROOT/'factory',product/'factory')
    for name in ['FONT-SOURCE.md','xsynth-title-native.png']:
        copy(ROOT/'assets'/name,product/'assets'/name)
    for name in ['GenerateTitle.py','GenerateFactory.cpp','PackageRelease.py']:
        copy(ROOT/'tools'/name,product/'tools'/name)
    for f in (ROOT/'tests').rglob('*'):
        if f.is_file() and f.suffix in {'.h','.cpp','.cmake'}:
            copy(f,product/'tests'/f.relative_to(ROOT/'tests'))
    for name in ['WordmarkTests.py','EXTERNAL-FIXTURES.md','rebuild-title.ps1']:
        copy(ROOT/'tests'/name,product/'tests'/name)
    sdk=SOURCE/'external/vst3sdk'
    for name in ['CMakeLists.txt','LICENSE.txt','README.md']:
        copy(SDK/name,sdk/name)
    for name in ['base','cmake','pluginterfaces','public.sdk/source']:
        tree(SDK/name,sdk/name)
    for name in ['public.sdk/LICENSE.txt','public.sdk/samples/vst-hosting/CMakeLists.txt',
                 'public.sdk/samples/vst-utilities/CMakeLists.txt']:
        copy(SDK/name,sdk/name)
    for name in ['public.sdk/samples/vst-hosting/validator','public.sdk/samples/vst-utilities/moduleinfotool']:
        tree(SDK/name,sdk/name)
    name='public.sdk/samples/vst-hosting/audiohost/source/media/miditovst.h'
    copy(SDK/name,sdk/name)  # Required by validator's SDK MIDI conversion tests.
    for name in ['README.md','CHANGELOG.md','SOURCE.md','SDK-COMPONENTS.txt']:
        copy(ROOT/'release'/name,SOURCE/name)
    for name in ['COPYING','AUTHOR','CREDITS.md']:
        copy(ROOT/name,SOURCE/name)
    copy(SDK/'LICENSE.txt',SOURCE/'VST3-SDK-LICENSE.txt')
    print(f'Staged {sum(f.is_file() for f in SOURCE.rglob("*"))} source files.')


def archive(directory, output):
    if output.exists():
        raise RuntimeError(f'Refusing to overwrite archive: {output}')
    with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for f in sorted(directory.rglob('*')):
            if f.is_file():
                z.write(f, f.relative_to(directory.parent).as_posix())


def pack():
    if not (WORK/'RELEASE-GATES-PASS.txt').is_file():
        raise RuntimeError('Exact-module release approval evidence is missing.')
    if BINARY.exists():
        raise RuntimeError('Binary staging already exists; do not overwrite final artifacts.')
    tree(BUNDLE,BINARY/'Xsynth 2000.vst3')
    for name in ['README.md','CHANGELOG.md','SOURCE.md','SDK-COMPONENTS.txt']:
        copy(ROOT/'release'/name,BINARY/name)
    for name in ['COPYING','AUTHOR','CREDITS.md']:
        copy(ROOT/name,BINARY/name)
    copy(SDK/'LICENSE.txt',BINARY/'VST3-SDK-LICENSE.txt')
    source_zip=DIST/'Xsynth-2000-0.1.0-source.zip'
    binary_zip=DIST/'Xsynth-2000-0.1.0-Windows-x64.zip'
    archive(SOURCE,source_zip)
    archive(BINARY,binary_zip)
    module=BINARY/'Xsynth 2000.vst3/Contents/x86_64-win/Xsynth 2000.vst3'
    hashes=''.join(f'{sha(f)}  {f.relative_to(DIST).as_posix()}\n' for f in [binary_zip,source_zip,module])
    (DIST/'SHA256SUMS.txt').write_text(hashes,encoding='utf-8')
    print(hashes,end='')


if __name__ == '__main__':
    if len(sys.argv)!=2 or sys.argv[1] not in {'stage-source','pack'}:
        raise SystemExit('Use stage-source or pack')
    (stage_source if sys.argv[1]=='stage-source' else pack)()
