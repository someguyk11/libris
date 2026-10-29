"""AUDIT ONLY: execute AST-extracted backend functions with in-memory doubles.

No backend imports, application startup, environment/config/credential access,
HTTP requests, database connections, or real stop/command requests occur.
"""
import ast
import copy
from pathlib import Path
from types import SimpleNamespace as NS
import threading

root=Path(__file__).resolve().parents[4]
tree=ast.parse((root/'backend/guidance.py').read_text())
functions=[]
for name in ('fail','replay_result'):
    node=copy.deepcopy(next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name==name))
    node.decorator_list=[]
    if name=='replay_result':
        node.args.defaults=[ast.Constant(None)]
        for arg in node.args.args: arg.annotation=None
    functions.append(node)
module=ast.fix_missing_locations(ast.Module(body=functions,type_ignores=[]))
class Task: pass
class Robot: pass
task=NS(id='audit-task',robot_id='audit-robot',session_id='audit-session',status='running',failure_reason=None)
robot=NS(guidance_state='running')
class Database:
    commits=0
    def get(self,kind,identifier): return task if kind is Task else robot
    def commit(self): self.commits+=1
db=Database(); state={}; submitted=[]
runtime=NS(lock=threading.RLock(),state=lambda robot_id:state,
           submit=lambda *args,**kwargs:submitted.append((args,kwargs)))
scope={'runtime':runtime,'available':lambda:None,'sweep':lambda db:None,
       'GuidanceTask':Task,'RobotModel':Robot,'ACTIVE':{'running'},'CLAIMED':{'running'},
       'now':lambda:123,'clear_otp':lambda task:None,'deliveries':{},'arrivals':{},
       'current_route':lambda db,task:NS(id='audit-route'),'command_id':lambda task,route,sequence:'audit-command'}
exec(compile(module,'<audit-only extracted guidance functions>','exec'),scope)
reason='Atlas navigation confidence lost; human assistance required; pose uncertainty exceeded'
body=NS(task_id=task.id,session_id=task.session_id,route_id='audit-route',command_id='audit-command',sequence=1,status='failed',error=reason)
response=scope['replay_result']('audit-robot',body,db)
print('In-memory response:',response)
print('In-memory persistent task failure_reason:',task.failure_reason)
print('Fake commits:',db.commits,'Fake stop submissions:',len(submitted))
if task.failure_reason!=reason:
    print('FAIL guidance preserves detailed CCPS failure: supplied error discarded')
    raise SystemExit(1)
print('PASS detailed failure preserved')
