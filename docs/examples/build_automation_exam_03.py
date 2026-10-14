"""Build editable wiring/HMI and GX Works CSV artifacts for the exam 3 guide.

This creates configuration and PLC source only, never runtime sensor/plant state.
This is a working answer. Validation must use the original exam conditions.
"""
import csv
import io
import json
import sys
from pathlib import Path

sys.dont_write_bytecode = True

from exam03_render_groups import apply_render_groups

folder = Path(__file__).resolve().parent
parts = []
wires = []

def part(number, kind, x, y, label, **settings):
    item = dict(instance_id=number, type=kind, x=x, y=y,
                custom_label=label, z_order=len(parts), **settings)
    parts.append(item)
    return item

def wire(first, port, second, target, kind='electric', label=''):
    item = dict(id=len(wires), from_component_id=first, from_port_id=port,
                to_component_id=second, to_port_id=target, wire_kind=kind,
                thickness=2, waypoints=[], is_tagged=bool(label), tag_text=label,
                tag_color_index=0,
                color=dict(r=.15 if kind=='pneumatic' else .9,
                           g=.55 if kind=='pneumatic' else .35,
                           b=.9 if kind=='pneumatic' else .1, a=1))
    wires.append(item)

def power(number, plus=0, minus=1):
    wire(1, 0, number, plus)
    wire(1, 1, number, minus)

def input_pin(number, port, address):
    index = int(address[1:], 8)
    wire(number, port, 0, index if index < 16 else index + 17, label=address)

def output_pin(address, number, port):
    wire(0, 16 + int(address[1:], 8), number, port, label=address)

def z(low, high=None, initial=None):
    return dict(min_layer=low, max_layer=high if high is not None else low,
                initial_height=initial if initial is not None else low)

part(0, 'PLC', 240, 40, 'PLC / 3번', z_config=z(1))
part(1, 'POWER_SUPPLY', 40, 120, '24V DC')
hmi = part(2, 'TOUCH_PANEL', 680, 40, 'HMI / 2화면')
part(3, 'SERVO_AMPLIFIER', 300, 290, '리프트 서보앰프')
part(4, 'AC_SUPPLY', 40, 300, '서보 AC 전원')
part(5, 'FRL', 40, 530, '0.5MPa', frl_pressure=5)
part(6, 'VALVE_SINGLE', 180, 530, '운전 공압 공급')
part(7, 'MANIFOLD', 340, 550, '공압 분배')
part(10, 'SENSOR_CYLINDER', 160, 853, '공급', flip_x=True, z_config=z(1))
part(11, 'SENSOR_CYLINDER', 462, 711, '송출', rotation_quadrants=3, z_config=z(1))
part(12, 'PROCESSING_CYLINDER', 445, 806, '가공', z_config=z(1, 2, 2))
part(13, 'STOPPER_CYLINDER', 410, 1200, '스토퍼',
     z_config=z(1, 2, 2))
part(14, 'LIFT_SUCTION', 386, 1061.5, '서보리프트·흡착', z_config=z(1, 3, 3),
     axis_settings=dict(pulses_per_turn=60, lead_mm=1, stroke_mm=100,
                        max_speed=100, acceleration=200))
part(15, 'SENSOR_CYLINDER', 595, 1330, '창고', flip_x=True, z_config=z(1))
part(16, 'WAREHOUSE', 550, 1172.5, '저장창고 2 → 5', z_config=z(1, 3, 1),
     warehouse_drive=dict(cylinder_id=15, travel=70))
part(17, 'CONVEYOR', 480, 1025, '분류 컨베이어', rotation_quadrants=1, z_config=z(1))
part(20, 'BOX', 480, 1400, '비금속 저장박스', z_config=z(1))
part(23, 'RING_SENSOR', 320, 868, '공작물 감지·중력 공급', z_config=z(1,2,1))
part(24, 'SENSOR', 325, 1070, '용량형 재질 감지', rotation_quadrants=1, z_config=z(1))
part(25, 'SENSOR', 340, 1325, '저장박스 입구 통과 확인', rotation_quadrants=1, z_config=z(1))
part(21, 'VACUUM_GENERATOR', 1290, 820, '진공 발생기')
part(22, 'INDUCTIVE_SENSOR', 540, 1070, '유도형 금속 검출', rotation_quadrants=3, z_config=z(1))
part(30, 'VALVE_SINGLE', 40, 690, '공급 솔레노이드')
part(31, 'VALVE_SINGLE', 170, 690, '송출 솔레노이드')
part(32, 'VALVE_DOUBLE', 590, 690, '가공 상승·하강')
part(33, 'VALVE_DOUBLE', 760, 690, '스토퍼 상승·하강')
part(34, 'VALVE_DOUBLE', 940, 690, '흡착실린더 전진·후진')
part(35, 'VALVE_DOUBLE', 1110, 690, '창고 전진·후진')
part(36, 'VALVE_SINGLE', 1300, 690, '진공 ON·OFF')

def fiber(number, x, y_top, y_bottom, amplifier_x, amplifier_y, address, name):
    part(number, 'FIBER_AMPLIFIER', amplifier_x, amplifier_y, name)
    part(number+1, 'FIBER_TRANSMITTER', x-15, y_top-93, name+' TX',
         rotation_quadrants=3, z_config=z(1))
    part(number+2, 'FIBER_RECEIVER', x-15, y_bottom-5, name+' RX',
         rotation_quadrants=1, z_config=z(1))
    power(number)
    input_pin(number, 2, address)
    wire(number, 3, number+1, 0, 'optical')
    wire(number, 4, number+2, 0, 'optical')

fiber(40, 350, 850, 945, 650, 330, 'X7', '공급 공작물')
wires[:] = [w for w in wires if not (w['from_component_id']==40 and w['from_port_id']==2)]
power(23, 0, 2)
input_pin(23, 1, 'X7')
fiber(43, 505, 880, 975, 980, 330, 'X23', '송출 공작물')
part(46, 'FIBER_AMPLIFIER', 1310, 330, '스토퍼 공작물')
power(46)
input_pin(46, 2, 'X16')
part(47, 'FIBER_TRANSMITTER', 420, 1215, '스토퍼 TX', flip_x=True, z_config=z(1))
part(48, 'FIBER_RECEIVER', 570, 1215, '스토퍼 RX', z_config=z(1))
wire(46, 3, 47, 0, 'optical')
wire(46, 4, 48, 0, 'optical')
part(50, 'WORKPIECE_METAL', 335, 881, '첫 금속 공작물', z_config=z(1))

power(2)
power(3, 4, 5)
wire(1, 1, 3, 7)
output_pin('Y17', 3, 9)
wire(1, 0, 3, 10)
output_pin('Y0', 3, 6)
output_pin('Y4', 3, 8)
input_pin(3, 12, 'X17')
for phase in range(4): wire(4, phase, 3, phase)
for phase in range(3): wire(3, 14+phase, 14, 10+phase)
wire(3, 3, 14, 13)
wire(3, 17, 14, 14)

for number, forward, reverse in [(10,'X0','X1'), (11,'X2','X3'),
                                 (14,'X10','X11'), (15,'X12','X13')]:
    power(number, 2, 3)
    input_pin(number, 4, forward)
    input_pin(number, 5, reverse)
power(13, 2, 3)
input_pin(13, 4, 'X6')
input_pin(13, 5, 'X21')
input_pin(14, 7, 'X20')
power(24)
input_pin(24, 2, 'X14')
power(25)
input_pin(25, 2, 'X22')
power(12, 0, 4)
input_pin(12, 1, 'X4')
input_pin(12, 2, 'X5')
output_pin('Y3', 12, 3)
power(22)
input_pin(22, 2, 'X15')
for number in (17,):
    output_pin('Y7', number, 0)
    wire(1, 1, number, 1)
wire(5, 0, 6, 2, 'pneumatic')
output_pin('Y17', 6, 0)
wire(1, 1, 6, 1)
wire(6, 4, 7, 0, 'pneumatic')

for valve, cylinder, address in [(30,10,'Y1'), (31,11,'Y2')]:
    wire(7, 1, valve, 2, 'pneumatic')
    output_pin(address, valve, 0)
    wire(1,1,valve,1)
    wire(valve,3,cylinder,0,'pneumatic')
    wire(valve,4,cylinder,1,'pneumatic')
for valve, cylinder, forward, reverse, first, second in [
    (32,12,'Y5','Y6',6,5), (33,13,'Y10','Y11',1,0),
    (34,14,'Y12','Y13',1,0), (35,15,'Y15','Y16',1,0)]:
    wire(7,2,valve,4,'pneumatic')
    output_pin(forward,valve,0)
    output_pin(reverse,valve,2)
    wire(1,1,valve,1)
    wire(1,1,valve,3)
    wire(valve,5,cylinder,first,'pneumatic')
    wire(valve,6,cylinder,second,'pneumatic')
# Vacuum retains supply during the five-second pause so a carried piece stays held.
wire(5,0,36,2,'pneumatic')
output_pin('Y14',36,0)
wire(1,1,36,1)
wire(36,4,21,0,'pneumatic')
wire(21,1,14,6,'pneumatic')

widgets=[]
def widget(kind, text, address, x, y, width, height, page=0, **extra):
    item = dict(type=kind, text=text, address=address, x=x, y=y, width=width,
                height=height, page=page, target_page=-1, momentary=True,
                unit='', decimals=0, font_size=12, on_color=0xff50c850)
    item.update(extra)
    widgets.append(item)
    return item

widget('text','테스트 동작','',8,4,200,20)
widget('button','창고 전진','M100',8,28,85,28)
widget('button','창고 후진','M101',98,28,85,28)
widget('button','가공모터 5초','M103',188,28,92,28)
widget('button','JOG 상승','M104',8,64,85,28)
widget('button','JOG 하강','M105',98,64,85,28)
widget('button','원점 복귀','M102',188,64,92,28)
widget('button','저속','M106',8,100,85,28)
widget('button','중속','M107',98,100,85,28)
widget('button','고속','M108',188,100,92,28)
widget('number','JOG 속도','D100',8,138,175,42,unit='mm/min')
widget('lamp','원점','M209',188,138,92,42)
widget('button','장비 시운전 ->','',8,185,272,24)['target_page']=1
widget('text','장비 시운전','',8,4,200,20,page=1)
widget('button','단속','M110',8,28,60,30,page=1)
widget('button','연속','M111',78,28,60,30,page=1)
widget('button','종료','M112',148,28,60,30,page=1)
widget('button','일시정지','M113',218,28,62,30,page=1)
widget('message','동작 상태','D103',8,70,272,35,page=1,
       messages=['정지','연속운전','일시정지'])
widget('number','금속 적재','D101',8,115,125,44,page=1,unit='개',
       visibility_address='M210')
widget('lamp','연속','M200',148,115,132,24,page=1)
widget('lamp','서보 동작','M211',148,144,132,24,page=1)
widget('button','<- 테스트 동작','',8,185,272,24,page=1)['target_page']=0
hmi['hmi_settings']=dict(screen_width=288,screen_height=216,
                         pages=['테스트 동작','장비 시운전'],widgets=widgets)
# Rotate the physical assembly as a rigid group: conveyor travels right.
# Controls remain upright in two separate rows above the machine.
from route_automation_exam_03 import SPECS
mechanical = {10,11,12,13,14,15,16,17,20,22,23,24,25,41,42,44,45,47,48,50}
for item in parts:
    if item['instance_id'] not in mechanical:
        continue
    width, height = SPECS[item['type']][2]
    rotation = item.get('rotation_quadrants', 0)
    old_width, old_height = (height,width) if rotation % 2 else (width,height)
    center_x, center_y = item['x']+old_width/2, item['y']+old_height/2
    rotation = (rotation+3) % 4
    new_width, new_height = (height,width) if rotation % 2 else (width,height)
    item.update(x=center_y-250-new_width/2,
                y=1320-center_x-new_height/2, rotation_quadrants=rotation)
control_positions = {
    0:(190,40), 1:(40,60), 2:(1280,40), 3:(550,40), 4:(40,200),
    40:(660,60), 43:(860,60), 46:(1060,60),
    5:(40,380), 6:(150,380), 7:(260,400),
    30:(420,380), 31:(560,380), 32:(700,380), 33:(860,380),
    34:(1020,380), 35:(1180,380), 36:(1360,380), 21:(1500,390),
}
labels = {0:'PLC',2:'HMI',3:'서보앰프',4:'AC 전원',6:'공압 공급',7:'분배',14:'리프트 · 흡착',16:'6칸 창고',17:'컨베이어',20:'저장박스',23:'공작물 감지',24:'용량형',25:'통과 감지',22:'유도형',30:'공급',31:'송출',32:'가공',33:'스토퍼',34:'흡착',35:'창고',36:'진공',40:'공급 감지',43:'송출 감지',46:'스토퍼 감지'}
for item in parts:
    if item['instance_id'] in labels:
        item['custom_label'] = labels[item['instance_id']]
    if item['instance_id'] in control_positions:
        item['x'], item['y'] = control_positions[item['instance_id']]

# Draw moving pieces above the conveyor and fixed mechanical housings.
display_order = {17:0,20:1,23:2,15:5,16:40,12:15,13:20,10:25,11:26,14:30}
for item in parts:
    item['z_order'] = display_order.get(item['instance_id'], 40)
    if item['instance_id'] == 15:
        item.update(x=930.5, y=540.0, custom_label='창고 구동')
    if item['instance_id'] == 50:
        item['z_order'] = 100

# Replace the external proportional linkage with one direct-drive assembly.
parts[:] = [item for item in parts if item['instance_id'] != 15]
warehouse = next(item for item in parts if item['instance_id'] == 16)
warehouse.update(type='DRIVEN_WAREHOUSE', x=667.5, y=605,
                 rotation_quadrants=0,
                 custom_label='6칸 창고')
warehouse.pop('warehouse_drive', None)
next(item for item in parts if item['instance_id'] == 13)['y'] = 760
# Keep the drill center fixed while restoring its upright housing.
processing = next(item for item in parts if item['instance_id'] == 12)
processing.update(x=processing['x'] + 30, y=processing['y'] - 30,
                  rotation_quadrants=0)
for connection in wires:
    for key in ('from_component_id', 'to_component_id'):
        if connection[key] == 15:
            connection[key] = 16

# Separate the feed/processing station from the sorting/lift station.
# A real second conveyor bridges the extra distance, using the same PLC output.
feed_station = {10, 11, 12, 23, 41, 42, 44, 45, 50}
for item in parts:
    if item['instance_id'] in feed_station:
        item['x'] -= 250
part(18, 'CONVEYOR', 375, 760, '이송 컨베이어',
     z_order_override=0, z_config=z(1))
parts[-1]['z_order'] = 0
parts[-1].pop('z_order_override')
output_pin('Y7', 18, 0)
wire(1, 1, 18, 1)

# Mount optical heads beyond the moving envelopes, rather than on top of them.
for item in parts:
    if item['instance_id'] in {41, 44}:
        item['x'] -= 90
    elif item['instance_id'] in {42, 45}:
        item['x'] += 90
    elif item['instance_id'] == 47:
        item['y'] += 130
    if item['instance_id'] in {22,24,25,41,42,44,45,47,48}:
        item['z_order'] = -10
    if item['instance_id'] in mechanical or item['instance_id'] == 18:
        item['y'] += 220
    item['show_custom_label'] = item['instance_id'] not in {10,11,12,13,22,24,25,41,42,44,45,47,48}

# The ring sensor replaces the separate magazine and its supply optical beam.
parts[:] = [item for item in parts if item['instance_id'] not in {40,41,42}]
wires[:] = [connection for connection in wires
            if connection['from_component_id'] not in {40,41,42}
            and connection['to_component_id'] not in {40,41,42}]
for index, connection in enumerate(wires):
    connection['id'] = index

layout=dict(version=3, next_instance_id=51,next_wire_id=len(wires),
            electrical_config=dict(plc_input_mode='sink',plc_output_mode='source'),
            components=parts,wires=wires)
apply_render_groups(layout)
(folder/'automation-exam-03-answer.json').write_text(
    json.dumps(layout,ensure_ascii=False,indent=2),encoding='utf-8')

# Four actual stock pieces in the ring sensor gravity feed, in insertion order.
continuous = json.loads(json.dumps(layout))
first_piece = next(p for p in continuous['components'] if p['instance_id']==50)
first_piece['type']='WORKPIECE_NONMETAL'
first_piece['z_config']=z(1,2,1)
first_piece['custom_label']='1 / 비금속'
for number,kind,label in [
        (51,'WORKPIECE_METAL','2 / 금속'),
        (52,'WORKPIECE_NONMETAL','3 / 비금속'),
        (53,'WORKPIECE_METAL','4 / 금속')]:
    continuous['components'].append(dict(instance_id=number,type=kind,x=first_piece['x'],y=first_piece['y'],
        rotation_quadrants=first_piece.get('rotation_quadrants',0),
        custom_label=label,z_order=100+number,z_config=z(1,2,1)))
continuous['next_instance_id']=54
apply_render_groups(continuous)
(folder/'automation-exam-03-continuous.json').write_text(
    json.dumps(continuous,ensure_ascii=False,indent=2),encoding='utf-8')

# Instruction source is assembled as rungs, with one snapshot of the sequence
# stage per scan. Conditional command bits are evaluated before physical outputs.
statements=[]
def emit(command,*args): statements.append((command,list(args)))
def rung(conditions,command,*args):
    for op,operands in conditions: emit(op,*operands)
    emit(command,*args)
def bit(address,invert=False): return ('LDI' if invert else 'LD',[address])
def stage(number): return ('LD=', ['D11',f'K{number}'])
def step(number,condition,target):
    rung([stage(number),('AND', [condition]),('ANI',['M204']),('ANI',['M205'])],
         'MOV',f'K{target}','D10')
def timer(number,conditions,preset): rung(conditions,'OUT',f'T{number}',f'K{preset}')
def output(address,conditions):
    rung(conditions+[('ANI',['M204'])],'OUT',address)

rung([bit('M8002')],'MOV','K500','D100')
rung([bit('M8002')],'MOV','K0','D10')
rung([bit('M8002')],'MOV','K0','D101')
rung([bit('M8002')],'RST','M200')
rung([bit('M8002')],'RST','M201')
rung([bit('M8002')],'SET','M205')
for button,speed in [('M106',500),('M107',1000),('M108',1500)]:
    rung([bit(button)],'MOV',f'K{speed}','D100')
emit('LD','M8000'); emit('DMOV','D100','D200')
# D100 is a word; D200's high word must stay zero for double precision PLSV.
emit('LD','M8000'); emit('MOV','K0','D201')
emit('LD','M8000'); emit('DMOV','D200','D202')
emit('LD','M8000'); emit('DNEG','D202')
for command,args in [('SET',['M200']),('SET',['M210']),('MOV',['K0','D101']),('SET',['M201'])]:
    rung([('LDP',['M111']),('ANI',['M201']),('ANI',['M205']),
          ('ANI',['M206'])],command,*args)
for command,args in [('RST',['M200']),('RST',['M210']),('SET',['M201'])]:
    rung([('LDP',['M110']),('ANI',['M201']),('ANI',['M205']),
          ('ANI',['M206'])],command,*args)
rung([bit('M112')],'SET','M203')
rung([bit('M113')],'SET','M204')
timer(3,[bit('M204')],50)
rung([bit('T3')],'SET','M205')
rung([bit('T3')],'RST','M201')
rung([bit('T3')],'RST','M200')
rung([bit('T3')],'RST','M204')
rung([bit('T3')],'RST','M206')
rung([bit('T3')],'RST','M212')
emit('LD','M8000'); emit('MOV','D10','D11')
rung([stage(0),('AND',['M201']),('AND',['X7']),('ANI',['M204']),
      ('ANI',['M205']),('DAND=',['D8340','K0']),('AND',['X17']),
      ('ANI',['M8340'])],
     'MOV','K1','D10')
step(1,'X0',2); step(2,'X1',33)
rung([stage(33),('ANI',['M204'])], 'MOV','K3','D10')
step(3,'X4',4)
timer(0,[stage(4),('ANI',['M204'])],30)
step(4,'T0',5); step(5,'X5',6); step(6,'X2',7); step(7,'X3',8)
step(8,'X14',9)
rung([stage(9),('AND',['X15'])],'MOV','K10','D10')
rung([stage(9),('ANI',['X15'])],'MOV','K30','D10')
rung([stage(10),('AND',['X6']),('AND',['X16'])],'MOV','K11','D10')
timer(4,[stage(11)],10); step(11,'T4',12)  # Motion completion is added below.
step(12,'X20',13)
rung([stage(13),('AND=',['D101','K0']),('AND',['X13'])],'MOV','K14','D10')
rung([stage(13),('AND>',['D101','K0']),('AND',['X12'])],'MOV','K14','D10')
timer(6,[stage(14)],10)
rung([stage(14),('AND',['T6']),('AND',['X17']),('ANI',['M8340'])],'MOV','K15','D10')
step(15,'X10',16)
rung([stage(16),('ANI',['X20']),('ANI',['M204'])], 'MOV','K17','D10')
step(17,'X11',18)
timer(8,[stage(18)],10)
rung([stage(18),('AND',['T8']),('AND',['X17']),('ANI',['M8340'])],'MOV','K19','D10')
step(19,'X21',20)
rung([stage(20),('AND',['M200'])],'INC','D101')
rung([stage(20)],'MOV','K0','D10')
step(30,'X22',32)
rung([stage(32),('ANI',['X22']),('ANI',['M204'])], 'MOV','K31','D10')
rung([stage(31)],'MOV','K0','D10')
for finished in (20,31):
    rung([stage(finished),('LDI',['M200']),('OR',['M203']),('OR>=',['D101','K2']),('ORI',['X7']),('ANB',[])],
         'RST','M201')
    rung([stage(finished),('LDI',['M200']),('OR',['M203']),('OR>=',['D101','K2']),('ORI',['X7']),('ANB',[])],
         'RST','M200')
rung([bit('M201',True)],'RST','M203')

# Manual test functions are interlocked with automatic movement.
rung([('LDP',['M103']),('ANI',['M201'])],'SET','M212')
timer(11,[bit('M212'),('ANI',['M204'])],50)
rung([bit('T11')],'RST','M212')
rung([('LDP',['M102']),('ANI',['M201']),('ANI',['M205']),('AND',['X11'])],'SET','M206')
timer(12,[bit('M206')],10)
rung([bit('M206'),('AND',['T12']),('DAND=',['D8340','K0']),
      ('AND',['X17']),('ANI',['M8340'])],'SET','M208')
rung([bit('M206'),('AND',['T12']),('DAND=',['D8340','K0']),
      ('AND',['X17']),('ANI',['M8340'])],'RST','M206')
rung([bit('M104'),('OR',['M105'])],'RST','M208')
rung([bit('M206'),('AND',['M8013']),('OR',['M208'])],'OUT','M209')
rung([bit('M8340'),('ORI',['X17']),('AND',['M200']),('AND',['M8013'])],'OUT','M211')
emit('LD','M8000'); emit('MOV','K0','D103')
rung([bit('M200')],'MOV','K1','D103')
rung([bit('M204')],'MOV','K2','D103')

# Servo commands stay powered throughout each positioning step.
motion_guard=[('ANI',['M204']),('AND',['X11'])]
rung([stage(11)]+motion_guard,'DDRVA','K-6000','K1500','Y0','Y4')
rung([stage(14),('AND=',['D101','K0'])]+motion_guard,'DDRVA','K-6000','K1500','Y0','Y4')
rung([stage(14),('AND>',['D101','K0'])]+motion_guard,'DDRVA','K0','K1500','Y0','Y4')
rung([stage(18)]+motion_guard,'DDRVA','K0','K1500','Y0','Y4')
# Manual JOG preserves the pulse coordinate but may leave the lift off home.
# Restore that coordinate before the first feed operation of either run mode.
rung([stage(0),('AND',['M201']),('ANI',['M205'])]+motion_guard,
     'DDRVA','K0','K1500','Y0','Y4')
rung([bit('M206'),('ANI',['M205'])]+motion_guard,'DDRVA','K0','K1500','Y0','Y4')
rung([bit('M205')]+motion_guard,'DDRVA','K0','K1500','Y0','Y4')
rung([bit('M104'),('ANI',['M201']),('ANI',['M206']),('ANI',['M205']),
      ('DAND<',['D8340','K0'])]+motion_guard,'DPLSV','D200','Y0','Y4')
rung([bit('M105'),('ANI',['M201']),('ANI',['M206']),('ANI',['M205']),
      ('DAND>',['D8340','K-6000'])]+motion_guard,'DPLSV','D202','Y0','Y4')

output('Y17',[bit('M8000')])
output('Y1',[stage(1),('ANI',['M205'])])
output('Y2',[stage(6),('ANI',['M205'])])
output('Y3',[('LD>=',['D11','K3']),('AND<=',['D11','K5']),('OR=',['D11','K33']),('OR',['M212']),('ANI',['M205'])])
output('Y5',[('LD>=',['D11','K3']),('AND<=',['D11','K4']),('ANI',['M205'])])
output('Y6',[stage(5),('OR',['M205'])])
output('Y7',[('LD>=',['D11','K8']),('AND<=',['D11','K10']),('OR=',['D11','K30']),('OR=',['D11','K32']),('ANI',['M205'])])
output('Y10',[('LD>=',['D11','K10']),('AND<=',['D11','K18']),('ANI',['M205'])])
output('Y11',[stage(19),('OR',['M205'])])
output('Y12',[('LD>=',['D11','K15']),('AND<=',['D11','K16']),('ANI',['M205'])])
output('Y13',[stage(17),('OR',['M205'])])
rung([('LD>=',['D11','K12']),('AND<=',['D11','K15']),('ANI',['M205'])],'OUT','Y14')
output('Y15',[stage(13),('AND>',['D101','K0']),('LD',['M100']),('ANI',['M201']),('ORB',[]),('ANI',['M205'])])
output('Y16',[stage(13),('AND=',['D101','K0']),('LD',['M101']),('ANI',['M201']),('ORB',[]),('OR',['M205'])])
timer(13,[bit('M205')],20)
rung([bit('M205'),('AND',['X1']),('AND',['X3']),('AND',['X5']),
      ('AND',['X11']),('AND',['X13']),('AND',['X21']),
      ('DAND=',['D8340','K0']),('AND',['X17']),('ANI',['M8340']),('AND',['T13'])],'MOV','K0','D10')
rung([bit('M205'),('AND',['X1']),('AND',['X3']),('AND',['X5']),
      ('AND',['X11']),('AND',['X13']),('AND',['X21']),
      ('DAND=',['D8340','K0']),('AND',['X17']),('ANI',['M8340']),('AND',['T13'])],'RST','M205')
emit('END')

# Gate stage 11's completion with servo INP as well as the startup delay.
for index,(command,args) in enumerate(statements):
    if command=='MOV' and args==['K12','D10']:
        statements.insert(index,('AND',['X17']))
        statements.insert(index+1,('ANI',['M8340']))
        break
text='\n'.join(command+' '+ ' '.join(args) for command,args in statements)+'\n'
(folder/'automation-exam-03-answer.il').write_text(text,encoding='utf-8')
stream=io.StringIO(newline='')
writer=csv.writer(stream,delimiter='\t',quoting=csv.QUOTE_ALL,lineterminator='\r\n')
writer.writerow(['(Automation exam 3)'])
writer.writerow(['PLC Information:','FXCPU FX3U/FX3UC'])
writer.writerow(['Step No.','Line Statement','Instruction','I/O(Device)','Blank','PI Statement','Note'])
for index,(command,args) in enumerate(statements):
    writer.writerow([index,'',command,args[0] if args else '','','',''])
    for value in args[1:]: writer.writerow(['','','',value,'','',''])
(folder/'automation-exam-03-answer.csv').write_text(stream.getvalue(),encoding='utf-16')
print(f'{len(parts)} parts, {len(wires)} wires, {len(widgets)} widgets, {len(statements)} instructions')


