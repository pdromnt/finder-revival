"""Exercise the native service against disposable storage and a fake printer."""
from pathlib import Path
import base64,hashlib,http.client,http.server,io,json,os,socket,socketserver,struct,subprocess,tempfile,threading,time,unittest,zlib,fcntl
from PIL import Image

BIN=Path(os.environ.get('REVIVAL_TEST_BINARY','/work/web-service/build/revival-web-host'))
BUSY=False
MOVE_BUSY=False
FAIL_START=False
UNAVAILABLE=False
COMMANDS=[]
PROGRESS='250/1000'
CAMERA=True
class Printer(socketserver.BaseRequestHandler):
 def handle(self):
  cmd=self.request.recv(4096).decode();COMMANDS.append(cmd)
  if UNAVAILABLE:return
  if 'M119' in cmd:
   reply='MachineStatus: '+('BUILDING_FROM_SD' if BUSY else 'READY')+'\r\nMoveMode: '+('MOVING' if MOVE_BUSY else 'READY')+'\r\nok\r\n'
  elif 'M105' in cmd:reply='T0: 23 /0\r\nok\r\n'
  elif 'M27' in cmd:reply='SD printing byte '+PROGRESS+'\r\nok\r\n'
  elif 'M115' in cmd:reply='Machine Name: Fixture Finder\r\nMachine Type: Finder\r\nFirmware: v2.8.4\r\nSN: TEST-ONLY\r\nok\r\n'
  elif 'M23 ' in cmd:reply=('Error: File selection failed' if FAIL_START else 'File opened: test.gx\r\nFile selected')+'\r\nok\r\n'
  else:reply='Control Success\r\nok\r\n'
  # Deliberately split the trailing acknowledgement to exercise framing.
  self.request.sendall(reply[:-3].encode());time.sleep(.005);self.request.sendall(reply[-3:].encode())
class FakePrinter(socketserver.ThreadingTCPServer):allow_reuse_address=True;daemon_threads=True
class Camera(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):
  if not CAMERA:self.send_error(503);return
  image=Image.new('RGB',(80,60),'#078ac0');data=io.BytesIO();image.save(data,format='JPEG');jpeg=data.getvalue()
  stream=self.path=='/?action=stream'
  self.send_response(200);self.send_header('Content-Type','multipart/x-mixed-replace; boundary=frame' if stream else 'image/jpeg')
  payload=b'--frame\r\nContent-Type: image/jpeg\r\nContent-Length: '+str(len(jpeg)).encode()+b'\r\n\r\n'+jpeg+b'\r\n' if stream else jpeg
  if stream:self.send_header('Content-Length',str(len(payload)))
  self.end_headers();self.wfile.write(payload)
class Integration(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory();cls.path=Path(cls.tmp.name);cls.storage=cls.path/'files';cls.storage.mkdir();cls.state=cls.path/'state';cls.state.mkdir()
  cls.fake=FakePrinter(('127.0.0.1',18899),Printer);threading.Thread(target=cls.fake.serve_forever,daemon=True).start()
  cls.camera=http.server.ThreadingHTTPServer(('127.0.0.1',18090),Camera);threading.Thread(target=cls.camera.serve_forever,daemon=True).start()
  cls.start()
 @classmethod
 def start(cls):
  cls.proc=subprocess.Popen([str(BIN),'--root',str(cls.storage),'--www','/work/web-service/www','--state',str(cls.state),'--key-file',str(cls.path/'key'),'--port','18080','--bind','127.0.0.1','--printer-port','18899','--camera-port','18090'],stdout=subprocess.DEVNULL)
  for _ in range(50):
   try:
    conn=http.client.HTTPConnection('127.0.0.1',18080,timeout=2);conn.request('GET','/api/session');r=conn.getresponse();cls.key=json.loads(r.read())['apiKey'];conn.close();return
   except OSError:time.sleep(.05)
  raise RuntimeError('Server did not start')
 @classmethod
 def tearDownClass(cls):cls.proc.terminate();cls.proc.wait();cls.fake.shutdown();cls.fake.server_close();cls.camera.shutdown();cls.camera.server_close();cls.tmp.cleanup()
 def request(self,method,path,data=None,headers=None):
  c=http.client.HTTPConnection('127.0.0.1',18080,timeout=8);c.request(method,path,body=data,headers=headers or {});r=c.getresponse();status=r.status;body=r.read();c.close();return status,body
 def upload(self,name,code,print_=False):
  boundary='test-multipart-boundary'
  body=(f'--{boundary}\r\nContent-Disposition: form-data; name="file"; filename="{name}"\r\nContent-Type: application/octet-stream\r\n\r\n').encode()+code+(f'\r\n--{boundary}\r\nContent-Disposition: form-data; name="print"\r\n\r\n'+('true' if print_ else 'false')+f'\r\n--{boundary}--\r\n').encode()
  return self.request('POST','/api/files/local',body,{'Content-Type':'multipart/form-data; boundary='+boundary,'X-Api-Key':self.key})
 def source(self,kind='PNG',two=False):
  image=Image.new('RGBA',(160,80),(200,60,20,255));b=io.BytesIO();image.convert('RGB').save(b,format=kind)
  encoded=base64.b64encode(b.getvalue()).decode();tag='thumbnail' if kind=='PNG' else 'thumbnail_JPG'
  return ('; estimated printing time (normal mode) = 1h 2m 3s\n; filament used [mm] = 1234.5\n; layer_height = 0.2\n; nozzle_temperature = 205\n; first_layer_bed_temperature = 0\n; outer_wall_speed = 60\n; wall_loops = 2\n'+f'; {tag} begin 160x80 {len(encoded)}\n; '+encoded+f'\n; {tag} end\n'+ ('; thumbnail begin 1x1 4\n; BAD!\n; thumbnail end\n' if two else '')+'G90\nG1 X1 Y1\n').encode()
 def test_01_conversion_metadata_and_source_cleanup(self):
  source=self.source(two=True);s,b=self.upload('cube.gcode',source);self.assertEqual(s,201,b)
  data=(self.storage/'cube.gx').read_bytes();self.assertFalse((self.storage/'cube.gcode').exists())
  self.assertEqual(data[:12],b'xgcode 1.0\n\0');offset=struct.unpack_from('<I',data,20)[0]
  self.assertEqual(data[offset:],source);self.assertEqual(data[58:60],b'BM');self.assertEqual(struct.unpack_from('<II',data,58+18),(80,60))
  self.assertEqual(struct.unpack_from('<II',data,28),(3723,1234));self.assertEqual(struct.unpack_from('<H',data,42)[0],200)
  self.assertEqual(struct.unpack_from('<HHHHHHHH',data,42),(200,0,2,60,0,205,0,1))
  self.assertEqual(list((self.storage/'.revival-web').iterdir()),[])
 def test_02_listing_preview_and_identity_delete(self):
  s,b=self.request('GET','/api/files');self.assertEqual(s,200);item=next(x for x in json.loads(b)['files'] if x['name']=='cube.gx');self.assertTrue(item['thumbnail'])
  s,b=self.request('GET','/api/preview/cube.gx');self.assertEqual(s,200);self.assertEqual(b[:2],b'BM')
  s,b=self.request('DELETE','/api/files/local/cube.gx',headers={'X-Api-Key':self.key,'If-Match':'stale'});self.assertEqual(s,409)
  s,b=self.request('DELETE','/api/files/local/cube.gx',headers={'X-Api-Key':self.key,'If-Match':item['etag']});self.assertEqual(s,204,b)
 def test_03_failure_keeps_existing_file_and_cleans_input(self):
  (self.storage/'existing.gx').write_bytes(b'old file')
  s,b=self.upload('existing.gcode',b'; thumbnail begin 1x1 4\n; BAD!\n; thumbnail end\nG90\n');self.assertEqual(s,400,b)
  self.assertEqual((self.storage/'existing.gx').read_bytes(),b'old file');self.assertFalse((self.storage/'existing.gcode').exists());self.assertEqual(list((self.storage/'.revival-web').iterdir()),[])
 def test_04_busy_and_unavailable_are_not_destructive(self):
  global BUSY,UNAVAILABLE
  BUSY=True
  try:s,b=self.upload('busy.gcode',b'G90\n');self.assertEqual(s,409,b);self.assertFalse((self.storage/'busy.gx').exists())
  finally:BUSY=False
  UNAVAILABLE=True
  try:
   s,b=self.upload('unavailable.gcode',b'G90\n');self.assertEqual(s,409,b);self.assertFalse((self.storage/'unavailable.gx').exists())
   s,b=self.request('GET','/api/status');self.assertFalse(json.loads(b)['connected']);self.assertFalse(json.loads(b)['ready'])
   _,listing=self.request('GET','/api/files');item=next(f for f in json.loads(listing)['files'] if f['name']=='existing.gx')
   s,b=self.request('DELETE','/api/files/local/existing.gx',headers={'X-Api-Key':self.key,'If-Match':item['etag']});self.assertEqual(s,409,b);self.assertEqual((self.storage/'existing.gx').read_bytes(),b'old file')
  finally:UNAVAILABLE=False
 def test_05_path_auth_origin_and_symlink(self):
  for name in ['../escape.gcode','x\\escape.gcode','.hidden.gcode','bad\nname.gcode']:
   s,b=self.upload(name,b'G90\n');self.assertEqual(s,400,(name,s,b))
  s,b=self.request('POST','/api/files/local',b'',{'Origin':'http://evil.invalid','X-Api-Key':self.key});self.assertEqual(s,403)
  s,b=self.request('DELETE','/api/files/local/existing.gx');self.assertEqual(s,403)
  (self.storage/'link.gx').symlink_to('/etc/passwd');s,b=self.request('GET','/api/files');self.assertNotIn('link.gx',b.decode())
  s,b=self.upload('link.gcode',b'G90\n');self.assertEqual(s,409,b);self.assertTrue((self.storage/'link.gx').is_symlink())
 def test_06_interrupted_upload_and_anonymous_source(self):
  c=socket.create_connection(('127.0.0.1',18080));c.sendall((f'POST /api/files/local HTTP/1.1\r\nHost: 127.0.0.1:18080\r\nX-Api-Key: {self.key}\r\nContent-Type: multipart/form-data; boundary=x\r\nContent-Length: 1000000\r\n\r\n--x\r\n').encode());time.sleep(.15)
  self.assertEqual(list((self.storage/'.revival-web').iterdir()),[]);c.close();time.sleep(.2);self.assertEqual(list((self.storage/'.revival-web').iterdir()),[])
 def test_07_gx_passthrough_and_explicit_print_only(self):
  before=len([x for x in COMMANDS if 'M23' in x]);s,b=self.upload('photo.gcode',self.source('JPEG'));self.assertEqual(s,201,b)
  gx=(self.storage/'photo.gx').read_bytes();s,b=self.upload('copy.gx',gx);self.assertEqual(s,201,b);self.assertEqual(gx,(self.storage/'copy.gx').read_bytes());self.assertEqual(before,len([x for x in COMMANDS if 'M23' in x]))
  s,b=self.upload('explicit.gcode',b'G90\n',True);self.assertEqual(s,201,b);self.assertEqual(before+1,len([x for x in COMMANDS if 'M23' in x]))
 def test_08_malformed_and_empty_do_not_publish(self):
  for name,source in [('bad.gcode',b'\0binary'),('empty.gcode',b'\n '),('invalid.gx',b'not GX')]:
   s,b=self.upload(name,source);self.assertEqual(s,400,(name,b))
  self.assertFalse((self.storage/'bad.gx').exists());self.assertEqual(list((self.storage/'.revival-web').iterdir()),[])
  for data in [b'xgcode 1.0\n\0'+b'\0'*100]:
   s,b=self.upload('bad-header.gx',data);self.assertEqual(s,400,b)
 def test_09_status_framing_and_compatibility(self):
  s,b=self.request('GET','/api/status');self.assertEqual(s,200);self.assertTrue(json.loads(b)['ready']);self.assertIn('T0:',json.loads(b)['temperature'])
  s,b=self.request('GET','/api/version');self.assertEqual(s,200);self.assertIn('OctoPrint',json.loads(b)['text'])
 def test_10_restart_cleans_only_private_remnants(self):
  stage=self.storage/'.revival-web';(stage/'output-interrupted.part').write_bytes(b'partial');(stage/'input-crash').write_bytes(b'raw')
  self.proc.terminate();self.proc.wait();self.__class__.start();self.assertFalse((stage/'output-interrupted.part').exists());self.assertFalse((stage/'input-crash').exists());self.assertEqual((self.storage/'existing.gx').read_bytes(),b'old file')
 def test_11_movement_state_blocks_ui_and_mutation(self):
  global MOVE_BUSY
  MOVE_BUSY=True
  try:
   s,b=self.request('GET','/api/status');self.assertEqual(s,200);self.assertFalse(json.loads(b)['ready']);self.assertEqual(json.loads(b)['state'],'Busy')
   s,b=self.upload('moving.gcode',b'G90\n');self.assertEqual(s,409,b)
  finally:MOVE_BUSY=False
 def test_12_safe_stock_names_and_utf8(self):
  s,b=self.upload('Caf\u00e9 cube $(test).gcode',b'G90\n');self.assertEqual(s,201,b);self.assertEqual(json.loads(b)['files']['local']['name'],'Caf_cube_test_.gx')
  self.assertTrue((self.storage/'Caf_cube_test_.gx').exists())
 def test_13_failed_print_is_not_reported_as_started(self):
  global FAIL_START
  FAIL_START=True
  try:
   s,b=self.upload('start-fails.gcode',b'G90\n',True);self.assertEqual(s,502,b);self.assertFalse(json.loads(b)['printed']);self.assertTrue((self.storage/'start-fails.gx').exists())
  finally:FAIL_START=False
 def test_14_oversized_deflate_is_rejected_without_publishing(self):
  def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
  png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',1,1,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b'\0'*(16*1024*1024)))+chunk(b'IEND',b'')
  source=b'; thumbnail begin 1x1 1000\n; '+base64.b64encode(png)+b'\n; thumbnail end\nG90\n'
  s,b=self.upload('deflate-bomb.gcode',source);self.assertEqual(s,400,b);self.assertFalse((self.storage/'deflate-bomb.gx').exists());self.assertEqual(list((self.storage/'.revival-web').iterdir()),[])
 def test_15_progress_and_device_information(self):
  global BUSY,PROGRESS
  BUSY=True
  try:
   s,b=self.request('GET','/api/status');d=json.loads(b);self.assertEqual(s,200);self.assertEqual(d['state'],'Printing');self.assertEqual(d['progress']['percent'],25.0)
   for invalid in ['0/0','1001/1000','invalid']:
    PROGRESS=invalid;s,b=self.request('GET','/api/status');self.assertIsNone(json.loads(b)['progress'])
  finally:BUSY=False;PROGRESS='250/1000'
  s,b=self.request('GET','/api/printer');self.assertEqual(s,200);self.assertEqual(json.loads(b)['serialNumber'],'TEST-ONLY');self.assertEqual(json.loads(b)['machineName'],'Fixture Finder')
 def test_16_explicit_print_checks_identity_readiness_and_ack(self):
  global BUSY,FAIL_START
  s,b=self.upload('confirmation.gcode',b'G90\n');self.assertEqual(s,201,b)
  f=next(f for f in json.loads(self.request('GET','/api/files')[1])['files'] if f['name']=='confirmation.gx')
  path='/api/print/confirmation.gx';h={'X-Api-Key':self.key,'If-Match':f['etag']}
  before=len([x for x in COMMANDS if 'M23' in x])
  self.assertEqual(self.request('POST',path)[0],403)
  self.assertEqual(self.request('POST',path,headers={**h,'If-Match':'old'})[0],409)
  BUSY=True
  try:self.assertEqual(self.request('POST',path,headers=h)[0],409)
  finally:BUSY=False
  self.assertEqual(before,len([x for x in COMMANDS if 'M23' in x]))
  s,b=self.request('POST',path,headers=h);self.assertEqual(s,200,b);self.assertTrue(json.loads(b)['started'])
  FAIL_START=True
  try:self.assertEqual(self.request('POST',path,headers=h)[0],502)
  finally:FAIL_START=False
  (self.storage/'unsafe;name.gx').write_bytes((self.storage/'confirmation.gx').read_bytes())
  self.assertEqual(self.request('POST','/api/print/unsafe;name.gx',headers=h)[0],400)
 def test_17_camera_proxy_and_unavailable(self):
  global CAMERA
  s,b=self.request('GET','/api/camera/snapshot');self.assertEqual(s,200,b);self.assertEqual(b[:2],b'\xff\xd8')
  s,b=self.request('GET','/api/camera/stream');self.assertEqual(s,200,b);self.assertIn(b'Content-Type: image/jpeg',b)
  with (self.state/'camera-stream.lock').open('w') as lock:
   fcntl.flock(lock,fcntl.LOCK_EX)
   self.assertEqual(self.request('GET','/api/camera/stream')[0],409)
   self.assertEqual(self.request('GET','/api/status')[0],200)
  CAMERA=False
  try:self.assertEqual(self.request('GET','/api/camera/snapshot')[0],503)
  finally:CAMERA=True
 def test_18_upload_notification_and_dismiss_identity(self):
  s,b=self.upload('external-slicer.gcode',b'G90\n');self.assertEqual(s,201,b)
  s,b=self.request('GET','/api/notifications');d=json.loads(b);self.assertEqual(d['name'],'external-slicer.gx')
  path='/api/notifications/dismiss';h={'X-Api-Key':self.key,'If-Match':d['etag']}
  self.assertEqual(self.request('POST',path)[0],403)
  self.assertEqual(self.request('POST',path,headers={**h,'If-Match':'old'})[0],409)
  self.assertEqual(self.request('POST',path,headers=h)[0],204)
  self.assertIsNone(json.loads(self.request('GET','/api/notifications')[1]))
  self.upload('external-slicer.gcode',b'G90\nG1 X1\n')
  self.assertEqual(self.request('POST',path,headers=h)[0],409)
  f=next(f for f in json.loads(self.request('GET','/api/files')[1])['files'] if f['name']=='external-slicer.gx')
  self.assertEqual(self.request('POST','/api/print/external-slicer.gx',headers={**h,'If-Match':f['etag']})[0],200)
  self.assertIsNone(json.loads(self.request('GET','/api/notifications')[1]))
if __name__=='__main__':unittest.main(verbosity=2)
