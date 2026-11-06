"""Audit candidate Git files, optionally with an ignored local identifier denylist."""
from pathlib import Path,PurePosixPath
import argparse,io,json,re,subprocess,sys,zipfile
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--private-identifiers',type=Path,default=ROOT/'.sessions/publication-denylist.json');args=p.parse_args()
    raw=subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard','-z'],cwd=ROOT)
    paths=sorted(set(x.decode() for x in raw.split(b'\0') if x));issues=[]
    tokens=json.loads(args.private_identifiers.read_text()) if args.private_identifiers.exists() else []
    private_key=re.compile(rb'-----BEGIN (?:OPENSSH|RSA|EC|DSA|ENCRYPTED)? ?PRIVATE KEY-----')
    owner_serial=re.compile(rb'\bSNFFF[0-9]{5,}\b')
    binary_suffixes={'.png','.ttf','.woff2','.stl','.orca_printer'}
    forbidden={'research','backups','firmware','.sessions','dist','usb-access','usb-backup','usb-diagnostic','node_modules','build','reports','private'}
    for path in paths:
        file=ROOT/path;parts=PurePosixPath(path).parts
        if any(part in forbidden or part.startswith(('FFINDER-BACKUP-','FFINDER-DIAG-')) for part in parts):issues.append((path,'private/generated path'))
        if not file.is_file():issues.append((path,'missing file'));continue
        data=file.read_bytes()
        if len(data)>2*1024*1024:issues.append((path,'file exceeds 2 MiB source-tree ceiling'))
        if file.suffix=='.orca_printer':
            try:
                with zipfile.ZipFile(io.BytesIO(data)) as archive:
                    entries=archive.infolist()
                    if len(entries)>64 or sum(e.file_size for e in entries)>2*1024*1024:
                        raise ValueError('oversized preset archive')
                    for entry in entries:
                        name=PurePosixPath(entry.filename.replace('\\','/'))
                        if name.is_absolute() or '..' in name.parts or not entry.filename.endswith('.json'):
                            raise ValueError('unexpected preset archive entry')
                        raw=archive.read(entry);preset=json.loads(raw)
                        for key in ('print_host','print_host_webui','printhost_apikey','printhost_user','printhost_password','flashforge_serial_number','user_id','bundle_id'):
                            if preset.get(key) not in (None,'',[],['']):
                                issues.append((path,'private preset field: '+key))
                        data+=b'\n'+raw
            except (ValueError,KeyError,zipfile.BadZipFile) as error:
                issues.append((path,'invalid Orca archive: '+str(error)))
        if private_key.search(data) or owner_serial.search(data):issues.append((path,'credential/device identifier pattern'))
        for index,token in enumerate(tokens):
            if token.encode() in data:issues.append((path,f'local private identifier #{index+1}'))
        if file.suffix in binary_suffixes:continue
        try:text=data.decode('utf-8-sig')
        except UnicodeDecodeError:issues.append((path,'unexpected binary'));continue
        if file.suffix=='.md':
            for target in re.findall(r'!?\[[^\]]*\]\(([^)]+)\)',text):
                target=target.strip('<>').split('#',1)[0]
                if not target or re.match(r'[A-Za-z][\w+.-]*:',target):continue
                from urllib.parse import unquote
                resolved=(file.parent/unquote(target)).resolve()
                if not resolved.exists():issues.append((path,'broken local link: '+target))
    for path,reason in issues:print(path+': '+reason)
    print(f'Audited {len(paths)} candidate source files; {len(issues)} finding(s).')
    return bool(issues)
if __name__=='__main__':sys.exit(main())
