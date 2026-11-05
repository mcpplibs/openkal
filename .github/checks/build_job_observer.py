"""Build the real backend and C ABI consumer with the runner's native compiler."""
import concurrent.futures
import os
from pathlib import Path
import subprocess
import sys

root = Path.cwd()
backend = root / 'backend'
include = root / 'include'
out = root / 'unit-api-build'
out.mkdir(exist_ok=True)
windows = sys.platform == 'win32'

def compile_backend(source):
    obj = out / (source.stem + ('.obj' if windows else '.o'))
    if windows:
        command = ['cl', '/nologo', '/std:c++20', '/c', '/O2', '/GR-', '/EHs-c-', '/GS-',
                   '/I' + str(include), '/Fo' + str(obj), str(source)]
    else:
        command = ['c++', '-std=c++23', '-O2', '-ffreestanding', '-fno-exceptions', '-fno-rtti',
                   '-fno-stack-protector', '-I' + str(include), '-c', str(source), '-o', str(obj)]
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    (out / (source.stem + '.log')).write_text(result.stdout)
    if result.returncode:
        raise RuntimeError(str(source.name) + '\n' + result.stdout)
    return str(obj)

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    objects = list(pool.map(compile_backend, sorted((backend / 'src').glob('*.cpp'))))
consumer = root / '.github/checks/job_observer.cpp'
binary = out / ('job-observer.exe' if windows else 'job-observer')
if windows:
    command = ['cl', '/nologo', '/std:c++17', '/EHsc', '/O2', '/I' + str(include), str(consumer),
               *objects, '/Fe:' + str(binary), '/link', 'ntdll.lib', 'synchronization.lib',
               'shell32.lib', 'kernel32.lib', 'bcrypt.lib']
else:
    command = ['c++', '-std=c++17', '-O2', '-pthread', '-I' + str(include), str(consumer),
               *objects, '-o', str(binary)]
subprocess.run(command, check=True)
with (out / 'job-observer.jsonl').open('w') as log:
    result = subprocess.run([str(binary)], stdout=log, stderr=subprocess.STDOUT, timeout=30)
print((out / 'job-observer.jsonl').read_text())
raise SystemExit(result.returncode)
