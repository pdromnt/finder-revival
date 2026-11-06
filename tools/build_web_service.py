"""Build the native HTTP/conversion service for Linux tests and ARMv5 hardware."""
from pathlib import Path
import hashlib,json,subprocess
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'web-service/build';out.mkdir(exist_ok=True)
mount=['--mount',f'type=bind,source={ROOT},target=/work']
compile_flags=['-std=c11','-D_FILE_OFFSET_BITS=64','-Os','-Wall','-Wextra','-Wno-misleading-indentation','-Wno-unused-function']
common=[*compile_flags,'web-service/native/server.c','web-service/native/gx.c','-lm']
for compiler,target,flags in [('gcc','revival-web-host',[]),('arm-linux-gnueabi-gcc','revival-web-arm',['-static','-march=armv5te','-mfloat-abi=soft'])]:
    subprocess.run(['docker','run','--rm',*mount,'ffinder-web-tools:local',compiler,*common,*flags,'-o','web-service/build/'+target],check=True)
inputs={str(p.relative_to(ROOT)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*sorted((ROOT/'web-service/native').glob('*.[ch]')),ROOT/'web-service/vendor/stb_image.h']}
manifest={'inputs':inputs,'arm_compile_flags':[*compile_flags,'-march=armv5te','-mfloat-abi=soft'],'binaries':{p.name:{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in out.glob('revival-web-*')}}
(out/'build.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest['binaries'],indent=2))
