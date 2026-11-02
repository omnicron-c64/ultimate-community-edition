#!/usr/bin/env python3
"""Exercise the real ftp_client.cc over loopback using scripted FTP servers."""
import os,socket,threading,subprocess,tempfile,pathlib,time
ROOT=pathlib.Path(__file__).resolve().parents[3]
MODES=['list','fallback','large-list','small-buffer','bad-epsv','bad-pasv','listing-fail','download','write-fail','sync-fail','transfer-fail','upload','read-fail','login-fail','greeting-fail','greeting-timeout','injection']
def serve(listener,mode,errors):
 try:
  conn,_=listener.accept();conn.settimeout(15)
  with conn,conn.makefile('rb') as stream:
   def reply(s):conn.sendall(s.encode())
   if mode=='greeting-fail':reply('421 Unavailable\r\n');return
   if mode=='greeting-timeout':time.sleep(12);return
   reply('220-Welcome\r\n123 Incidental numeric line\r\n220 Ready\r\n')
   data=None
   try:
    while True:
     raw=stream.readline()
     if not raw:break
     cmd,_,arg=raw.decode().strip('\r\n').partition(' ')
     if cmd=='USER':reply('331 Password required\r\n')
     elif cmd=='PASS':reply('530 Login rejected\r\n' if mode=='login-fail' else '230 Logged in\r\n')
     elif cmd=='TYPE':reply('200 Binary\r\n')
     elif cmd=='CWD':reply('250 Directory changed\r\n')
     elif cmd=='PWD':reply('257 "/"\r\n')
     elif cmd in ('EPSV','PASV'):
      if cmd=='EPSV' and mode in ('fallback','bad-pasv'):reply('502 Unsupported\r\n');continue
      if mode=='bad-epsv':reply('229 Bad (|||99999|)\r\n');continue
      if mode=='bad-pasv':reply('227 Bad (1,2,3,4,999,1)\r\n');continue
      if data:data.close()
      data=socket.socket();data.bind(('127.0.0.1',0));data.listen();data.settimeout(15);port=data.getsockname()[1]
      # Deliberately wrong PASV address: client should use the control peer.
      reply(f'229 Passive (|||{port}|)\r\n' if cmd=='EPSV' else f'227 Passive (10,99,88,77,{port//256},{port%256})\r\n')
     elif cmd in ('MLSD','LIST','RETR','STOR'):
      if cmd=='MLSD' and mode=='fallback':reply('502 Unsupported\r\n');continue
      if mode=='listing-fail':reply('550 Access denied\r\n');data.close();data=None;continue
      channel,_=data.accept();data.close();data=None;channel.settimeout(15);reply('150 Opening data\r\n')
      try:
       if cmd=='STOR':
        content=b''
        while True:
         b=channel.recv(4096)
         if not b:break
         content+=b
        if mode=='upload' and content!=b'X'*12000:raise AssertionError('wrong uploaded payload')
       elif cmd=='RETR':channel.sendall(bytes(i%251 for i in range(12000)))
       else:
        rows=1000 if mode=='large-list' else 2
        body=''.join((f'-rw-r--r-- 1 user group 12 Jan 01 12:00 file {i}.d64\r\n' if cmd=='LIST' else f'type=file;size=12; file {i}.d64\r\n') for i in range(rows)).encode()
        # Fragment CRLF and record boundaries.
        for i in range(0,len(body),137):channel.sendall(body[i:i+137])
      except (BrokenPipeError,ConnectionResetError):pass
      finally:channel.close()
      try:reply('426 Transfer aborted\r\n' if mode=='transfer-fail' else '226 Completed\r\n')
      except (BrokenPipeError,ConnectionResetError):break
     else:raise AssertionError('unexpected command '+cmd)
   except (ConnectionResetError,BrokenPipeError):pass
   finally:
    if data:data.close()
 except Exception as e:errors.append(repr(e))
 finally:listener.close()
def main():
 with tempfile.TemporaryDirectory() as temp:
  binary=temp+'/ftp-test'
  subprocess.run(['g++','-std=c++11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(ROOT/'software/test/ftp_client/stubs'),'-I'+str(ROOT/'software/network'),str(ROOT/'software/network/ftp_client.cc'),str(ROOT/'software/test/ftp_client/client_test.cc'),'-o',binary],check=True)
  for source in ['listing_test.cc','lifecycle_test.cc']:
   executable=temp+'/'+source
   subprocess.run(['g++','-std=c++11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(ROOT/'software/filesystem'),'-I'+str(ROOT/'software/filemanager'),str(ROOT/'software/test/ftp_client'/source),'-o',executable],check=True)
   env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0'
   subprocess.run([executable],env=env,check=True)
  for mode in MODES:
   listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen();listener.settimeout(15);port=listener.getsockname()[1]
   errors=[];thread=threading.Thread(target=serve,args=(listener,mode,errors),daemon=True);thread.start()
   env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0'
   p=subprocess.run([binary,str(port),mode],env=env,capture_output=True,text=True,timeout=35);thread.join(16)
   if p.returncode or errors or thread.is_alive():raise AssertionError((mode,p.returncode,p.stdout,p.stderr,errors))
   print(p.stdout.strip())
 print('FTP loopback: all scenarios passed')
if __name__=='__main__':main()
