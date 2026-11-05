"""Trace Linux x86-64 setpgid entry/exit; no implementation fault hooks."""
import os,signal,subprocess,ctypes,json,time,sys,platform
if sys.platform != "linux" or platform.machine() != "x86_64":
 raise SystemExit("retirement syscall boundary control requires Linux x86-64")
from pathlib import Path
binary=str(Path(sys.argv[1]).resolve())
libc=ctypes.CDLL(None,use_errno=True);libc.ptrace.restype=ctypes.c_long
fields='r15 r14 r13 r12 rbp rbx r11 r10 r9 r8 rax rcx rdx rsi rdi orig_rax rip cs eflags rsp ss fs_base gs_base ds es fs gs'.split()
class Registers(ctypes.Structure):_fields_=[(n,ctypes.c_ulonglong) for n in fields]
def ptrace(op,pid,address=0,data=0):
 r=libc.ptrace(op,pid,ctypes.c_void_p(address),data if isinstance(data,ctypes.c_void_p) else ctypes.c_void_p(data))
 if r==-1:raise OSError(ctypes.get_errno(),str((op,pid)))
 return r
rows=[]
for mode in ['live','wrong-birth','dead','cross-session','before-migration','after-migration']:
 anchor=subprocess.Popen(['python3','-c','import time;time.sleep(20)'],preexec_fn=os.setsid if mode=='cross-session' else lambda:os.setpgid(0,0))
 helper=None
 try:
  who=subprocess.check_output([binary,'identity',str(anchor.pid)],text=True).split();assert int(who[0])==anchor.pid
  if mode=='wrong-birth':who[1]=str(int(who[1])+1)
  if mode=='dead':anchor.kill();anchor.wait()
  helper=subprocess.Popen([binary,*who],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
  assert helper.stdout.readline().strip()=='READY'
  if mode.endswith('migration'):
   ptrace(16,helper.pid);os.waitpid(helper.pid,0);ptrace(0x4200,helper.pid,0,1)
  helper.stdin.write('A');helper.stdin.flush()
  if mode.endswith('migration'):
   at_exit=False;hit=False
   for step in range(500):
    ptrace(24,helper.pid);pid,status=os.waitpid(helper.pid,0)
    if not os.WIFSTOPPED(status):raise RuntimeError(('unexpected_exit',status))
    regs=Registers();ptrace(12,helper.pid,0,ctypes.cast(ctypes.byref(regs),ctypes.c_void_p))
    if regs.orig_rax==109:
     if not at_exit:
      hit=True
      if mode=='before-migration':anchor.kill();anchor.wait();ptrace(17,helper.pid);break
      at_exit=True
     else:
      assert regs.rax==0,regs.rax
      anchor.kill();anchor.wait();ptrace(17,helper.pid);break
   else:raise RuntimeError('retirement syscall boundary not reached')
   assert hit
  helper.stdin.close();output=helper.stdout.read();error=helper.stderr.read();code=helper.wait(timeout=5)
  expected=2 if mode=='live' else 0
  assert code==0 and 'RESULT 0 '+str(expected) in output,(mode,code,output,error)
  if mode=='live':assert anchor.poll() is None and 'OLD_UNIT_END ' in output
  rows.append({'case':mode,'state':'empty' if expected==2 else 'unknown','exit':code,'anchor_preserved':anchor.poll() is None if mode in ['live','wrong-birth','cross-session'] else None,'actual_native_boundary':mode if mode.endswith('migration') else None})
 finally:
  if helper and helper.poll() is None:helper.kill();helper.wait()
  if anchor.poll() is None:anchor.kill();anchor.wait()
(Path(binary).parent/'retire-boundaries.json').write_text(json.dumps(rows,indent=2)+'\n')
print(json.dumps(rows,indent=2))
