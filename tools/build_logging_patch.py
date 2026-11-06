"""Compile logging-only adapter against the printer's legacy runtime ABI."""
from pathlib import Path
import hashlib,json,subprocess
from elftools.elf.elffile import ELFFile
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'logging/build';OUT.mkdir(exist_ok=True)
prefix=['docker','run','--rm','--entrypoint','/bin/sh','--mount',f'type=bind,source={ROOT},target=/work','ffinder-web-tools:local','-c']
import shlex
def run(args): subprocess.run(prefix+[shlex.join(args)],check=True)
run(['gcc','-O2','-Wall','-Wextra','-Werror','-DLOG_TEST','logging/revival_log.c','-o','logging/build/test-filter','-ldl','-pthread'])
run(['logging/build/test-filter'])
run(['arm-linux-gnueabi-gcc','-std=gnu11','-O2','-Wall','-Wextra','-Werror','-march=armv5te','-mfloat-abi=soft','-fPIC','-shared','-nostdlib','-fno-stack-protector','-fno-builtin','-o','logging/build/librevival_log.so','logging/revival_log.c'])
binary=OUT/'librevival_log.so'
with binary.open('rb') as f:
    elf=ELFFile(f);assert elf.header['e_machine']=='EM_ARM' and elf.elfclass==32
    symbols=[s.name for s in elf.get_section_by_name('.dynsym').iter_symbols() if s['st_shndx']=='SHN_UNDEF' and s.name]
    assert not any('isoc23' in s or 'time64' in s for s in symbols)
    exports={s.name for s in elf.get_section_by_name('.dynsym').iter_symbols() if s['st_shndx']!='SHN_UNDEF'}
    assert 'open' in exports and 'open64' not in exports
manifest={'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'bytes':binary.stat().st_size,'source_sha256':hashlib.sha256((ROOT/'logging/revival_log.c').read_bytes()).hexdigest(),'undefined':symbols}
(OUT/'build.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest,indent=2))
