"""Serve the exact dashboard against temporary files/a fake printer for UI QA."""
from pathlib import Path
import json,signal,socketserver,subprocess,tempfile,threading,time,http.server,io
from PIL import Image,ImageDraw
MODE=Path('/work/research/web-service/fixture-mode.json')
class Printer(socketserver.BaseRequestHandler):
 def handle(self):
  mode=json.loads(MODE.read_text());cmd=self.request.recv(4096).decode()
  if not mode.get('connected',True):return
  if 'M119' in cmd:
   reply='MachineStatus: '+('BUILDING_FROM_SD' if mode.get('printing') else 'READY')+'\r\nMoveMode: '+('MOVING' if mode.get('moving') else 'READY')+'\r\nok\r\n'
  elif 'M27' in cmd:reply='SD printing byte 374/1000\r\nok\r\n'
  elif 'M115' in cmd:reply='Machine Name: Fixture Finder\r\nMachine Type: Finder\r\nFirmware: v2.8.4 TEST\r\nSN: FIXTURE-ONLY\r\nok\r\n'
  elif 'M105' in cmd:reply='T0: 24 /0\r\nok\r\n'
  elif 'M23' in cmd:reply='File opened: fixture.gx\r\nFile selected\r\nok\r\n'
  else:reply='Control Success\r\nok\r\n'
  self.request.sendall(reply.encode())
class Server(socketserver.ThreadingTCPServer):allow_reuse_address=True;daemon_threads=True
class Camera(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):
  if not json.loads(MODE.read_text()).get('camera',False):self.send_error(503);return
  image=Image.new('RGB',(640,480),'#121820');draw=ImageDraw.Draw(image)
  draw.rectangle((80,100,300,370),fill='#078ac0');draw.rectangle((340,100,560,370),fill='#ffad60')
  draw.text((20,25),'CAMERA TEST FIXTURE - NOT A LIVE PRINTER',fill='white')
  out=io.BytesIO();image.save(out,format='JPEG');data=out.getvalue()
  stream='action=stream' in self.path
  self.send_response(200);self.send_header('Content-Type','multipart/x-mixed-replace; boundary=frame' if stream else 'image/jpeg')
  if not stream:self.send_header('Content-Length',str(len(data)))
  self.end_headers()
  if not stream:self.wfile.write(data);return
  try:
   while json.loads(MODE.read_text()).get('camera',False):
    self.wfile.write(b'--frame\r\nContent-Type: image/jpeg\r\nContent-Length: '+str(len(data)).encode()+b'\r\n\r\n'+data+b'\r\n');self.wfile.flush();time.sleep(.2)
  except (BrokenPipeError,ConnectionResetError):pass
def stop(*_):raise SystemExit
signal.signal(signal.SIGTERM,stop)
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);data=p/'files';data.mkdir();state=p/'state';state.mkdir()
 out=data/'Fixture preview.gx'
 subprocess.run(['/work/web-service/build/revival-web-host','--convert','/work/web-service/tests/preview-check.gcode',str(out)],check=True)
 (data/('Fixture '+'long_filename_'*9+'.gx')).write_bytes(out.read_bytes())
 (data/'<img src=x onerror=alert(1)>.gx').write_bytes(out.read_bytes())
 MODE.write_text(json.dumps({'connected':True,'moving':False,'printing':True,'camera':True}))
 fake=Server(('0.0.0.0',18899),Printer);threading.Thread(target=fake.serve_forever,daemon=True).start()
 camera=http.server.ThreadingHTTPServer(('127.0.0.1',18090),Camera);threading.Thread(target=camera.serve_forever,daemon=True).start()
 child=subprocess.Popen(['/work/web-service/build/revival-web-host','--root',str(data),'--www','/work/web-service/www','--state',str(state),'--key-file',str(p/'key'),'--port','18080','--printer-port','18899','--camera-port','18090'])
 try:
  while True:time.sleep(1)
 finally:
  child.terminate();child.wait(timeout=5);fake.shutdown();fake.server_close();camera.shutdown();camera.server_close()
