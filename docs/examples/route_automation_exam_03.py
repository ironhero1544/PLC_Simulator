"""Route answer wires through free canvas space with orthogonal segments."""
import heapq
import json
import math
import re
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
SPECS = {
 'PLC':('plc_def.cpp','kPorts',(320,180)),
 'POWER_SUPPLY':('power_supply_def.cpp','kPorts',(100,80)),
 'TOUCH_PANEL':('storage_sensor_def.cpp','kHmiPorts',(360,270)),
 'SERVO_AMPLIFIER':('servo_def.cpp','kAmpDesignPorts',(152*180/354,180)),
 'AC_SUPPLY':('servo_def.cpp','kAcPorts',(125,80)),
 'FRL':('frl_def.cpp','kPorts',(80,100)),
 'MANIFOLD':('manifold_def.cpp','kPorts',(120,60)),
 'SENSOR_CYLINDER':('sensor_cylinder_def.cpp','kCylinderPorts',(340,84)),
 'STOPPER_CYLINDER':('sensor_cylinder_def.cpp','kStopperPorts',(80,80)),
 'PROCESSING_CYLINDER':('processing_cylinder_def.cpp','kPorts',(120,180)),
 'LIFT_SUCTION':('servo_def.cpp','kIntegratedPorts',(340,200)),
 'WAREHOUSE':(None,None,(160,100)),
 'DRIVEN_WAREHOUSE':('storage_sensor_def.cpp','kDrivenWarehousePorts',(430,100)),
 'CONVEYOR':('conveyor_def.cpp','kPorts',(400,80)),
 'BOX':(None,None,(80,100)), 'MAGAZINE':(None,None,(100,100)),
 'VACUUM_GENERATOR':('sensor_cylinder_def.cpp','kVacuumPorts',(120,80)),
 'RING_SENSOR':('ring_sensor_def.cpp','kPorts',(60,70)),
 'SENSOR':('sensor_def.cpp','kPorts',(100,120)),
 'INDUCTIVE_SENSOR':('inductive_sensor_def.cpp','kPorts',(100,120)),
 'VALVE_SINGLE':('valve_single_def.cpp','kPorts',(80,100)),
 'VALVE_DOUBLE':('valve_double_def.cpp','kPorts',(100,100)),
 'FIBER_AMPLIFIER':('storage_sensor_def.cpp','kFiberPorts',(174,64)),
 'FIBER_TRANSMITTER':('storage_sensor_def.cpp','kTxPorts',(98,30)),
 'FIBER_RECEIVER':('storage_sensor_def.cpp','kRxPorts',(98,30)),
 'WORKPIECE_METAL':(None,None,(30,30)),
 'WORKPIECE_NONMETAL':(None,None,(30,30)),
}
PORTS={}
number=r'(-?\d+(?:\.\d+)?)(?:f)?'
for kind,(file,array,size) in SPECS.items():
 if not file:continue
 source=(ROOT/'src/components'/file).read_text(encoding='utf-8')
 block=source.split(' '+array+'[] = {',1)[1].split('};',1)[0]
 items=re.findall(r'\{\s*(\d+),\s*\{\s*'+number+r',\s*'+number+r'\s*\}',block)
 PORTS[kind]={int(i):(float(x),float(y)) for i,x,y in items}
 if kind=='SERVO_AMPLIFIER':
  PORTS[kind]={i:((x-40)*180/354,(y-8)*180/354) for i,(x,y) in PORTS[kind].items()}

def world(part, port):
 w,h=SPECS[part['type']][2];q=part.get('rotation_quadrants',0)%4
 x,y=PORTS[part['type']][port];x-=w/2;y-=h/2
 if part.get('flip_x'):x=-x
 if part.get('flip_y'):y=-y
 x,y=((x,y),(-y,x),(-x,-y),(y,-x))[q]
 dw,dh=(h,w) if q%2 else (w,h)
 return part['x']+dw/2+x,part['y']+dh/2+y

def route_layout(layout):
 parts={p['instance_id']:p for p in layout['components']}
 bounds={}
 for id,p in parts.items():
  if p['type'].startswith('WORKPIECE'):continue
  w,h=SPECS[p['type']][2]
  if p.get('rotation_quadrants',0)%2:w,h=h,w
  bounds[id]=(p['x'],p['y'],p['x']+w,p['y']+h)
 parent={}
 def root(key):
  parent.setdefault(key,key)
  if parent[key]!=key:parent[key]=root(parent[key])
  return parent[key]
 def ends(w):return (w['from_component_id'],w['from_port_id']),(w['to_component_id'],w['to_port_id'])
 for w in layout['wires']:
  a,b=ends(w);parent[root(a)]=root(b)
 occupied={}
 def grid(x):return int(round(x/10))
 def point(node):return node[0]*10,node[1]*10
 def inside(p,box):return box[0]-4<p[0]<box[2]+4 and box[1]-4<p[1]<box[3]+4
 for wire in layout['wires']:
  a,b=ends(wire);start=world(parts[a[0]],a[1]);end=world(parts[b[0]],b[1]);net=root(a)
  excluded=set()
  # A connector may sit over another mechanical assembly in a top view.
  excluded.update(id for id,box in bounds.items() if id not in (a[0],b[0]) and (inside(start,box) or inside(end,box)))
  obstacles=[box for id,box in bounds.items() if id not in excluded]
  blocked_nodes=set()
  for box in obstacles:
   for gx in range(math.floor((box[0]-4)/10),math.ceil((box[2]+4)/10)+1):
    for gy in range(math.floor((box[1]-4)/10),math.ceil((box[3]+4)/10)+1):
     if inside(point((gx,gy)),box):blocked_nodes.add((gx,gy))
  def blocked(node):return node in blocked_nodes
  def escape(p,id,port):
   box=bounds[id];x,y=p
   candidates=[((math.floor((box[0]-15)/10),grid(y)),[(math.floor((box[0]-15)/10)*10,y)]),
               ((math.ceil((box[2]+15)/10),grid(y)),[(math.ceil((box[2]+15)/10)*10,y)]),
               ((grid(x),math.floor((box[1]-15)/10)),[(x,math.floor((box[1]-15)/10)*10)]),
               ((grid(x),math.ceil((box[3]+15)/10)),[(x,math.ceil((box[3]+15)/10)*10)])]
   if parts[id]['type']=='PLC':candidates=candidates[2:3] if port<16 or port>=32 else candidates[3:]
   # The compact stopper's left gutter is enclosed by the material sensor,
   # warehouse and belt. Take its terminals through the open right gutter.
   if parts[id]['type']=='STOPPER_CYLINDER' and parts[id].get('rotation_quadrants')==3:
    candidates=candidates[1:2]
   candidates=[(n,path) for n,path in candidates if not blocked(n)]
   if not candidates:return (grid(x),grid(y)),[(x,grid(y)*10)]
   return min(candidates,key=lambda c:abs(c[1][0][0]-x)+abs(c[1][0][1]-y))
  first,first_path=escape(start,a[0],a[1]);last,last_path=escape(end,b[0],b[1])
  max_x=180;max_y=160
  queue=[(0,0,first,-1)];cost={(first,-1):0};previous={};goal=None
  while queue:
   _,distance,node,direction=heapq.heappop(queue)
   if distance!=cost.get((node,direction)):continue
   if node==last:goal=(node,direction);break
   for axis,(dx,dy) in enumerate(((1,0),(0,1),(-1,0),(0,-1))):
    next_node=(node[0]+dx,node[1]+dy)
    if not(-4<=next_node[0]<=max_x and -4<=next_node[1]<=max_y) or blocked(next_node):continue
    owners=occupied.get(next_node,set())
    penalty=3 if owners and net not in owners else 0
    candidate=distance+1+(.3 if direction>=0 and axis!=direction else 0)+penalty
    key=(next_node,axis)
    if candidate>=cost.get(key,float('inf')):continue
    cost[key]=candidate;previous[key]=(node,direction)
    heuristic=abs(next_node[0]-last[0])+abs(next_node[1]-last[1])
    heapq.heappush(queue,(candidate+heuristic,candidate,next_node,axis))
  if goal is None:raise RuntimeError(f'Cannot route wire {wire["id"]}: {a} -> {b}')
  path=[];key=goal
  while key in previous:path.append(key[0]);key=previous[key]
  path.append(first);path.reverse()
  for node in path:occupied.setdefault(node,set()).add(net)
  points=[start,*first_path,*map(point,path),*reversed(last_path),end]
  # The terminal-to-grid connector needs an extra bend for non-grid pins.
  orthogonal=[points[0]]
  for p in points[1:]:
   old=orthogonal[-1]
   if abs(old[0]-p[0])>.001 and abs(old[1]-p[1])>.001:orthogonal.append((p[0],old[1]))
   if p!=orthogonal[-1]:orthogonal.append(p)
  reduced=[]
  for p in orthogonal:
   while len(reduced)>=2 and ((reduced[-2][0]==reduced[-1][0]==p[0]) or (reduced[-2][1]==reduced[-1][1]==p[1])):reduced.pop()
   reduced.append(p)
  wire['waypoints']=[{'x':round(x,3),'y':round(y,3)} for x,y in reduced[1:-1]]
  wire['is_tagged']=False
  kind=wire['wire_kind']
  color=(.15,.55,.9) if kind=='pneumatic' else ((.6,.3,.8) if kind=='optical' else (.85,.5,.05))
  if a==(1,0):color=(.85,.12,.12)
  if a==(1,1):color=(.18,.23,.3)
  if b[0]==0:color=(.1,.6,.3)
  wire['color']=dict(zip(('r','g','b','a'),(*color,1)))
 return layout

if __name__=='__main__':
 folder=Path(__file__).parent
 for name in ('answer','continuous'):
  p=folder/f'automation-exam-03-{name}.json';layout=route_layout(json.loads(p.read_text(encoding='utf-8-sig')))
  content=json.dumps(layout,ensure_ascii=False,indent=2)
  p.write_text(content,encoding='utf-8')
 print('Routed every wire with right-angle waypoints and separate signal/power colors.')
