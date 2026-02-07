import React, { useState, useRef, useCallback, useEffect } from 'react';
import { Play, Edit, Cable, MousePointer, Zap } from 'lucide-react';

const PLCSimulator = () => {
  // 기본 상태들
  const [currentMode, setCurrentMode] = useState('wiring');
  const [selectedTool, setSelectedTool] = useState('select');
  const [plcRunning, setPlcRunning] = useState(false);
  const [airPressure, setAirPressure] = useState(6.0);
  const [isDraggingPressure, setIsDraggingPressure] = useState(false);
  const [pressureDragStart, setPressureDragStart] = useState({ angle: 0, pressure: 0 });
  
  // 컴포넌트 관련 상태들
  const [placedComponents, setPlacedComponents] = useState([]);
  const [selectedComponent, setSelectedComponent] = useState(null);
  const [isDraggingComponent, setIsDraggingComponent] = useState(false);
  const [dragOffset, setDragOffset] = useState({ x: 0, y: 0 });
  const [draggedComponent, setDraggedComponent] = useState(null);
  
  // 배선 관련 상태들
  const [wires, setWires] = useState([]);
  const [isConnecting, setIsConnecting] = useState(false);
  const [connectionStart, setConnectionStart] = useState(null);
  const [wireControlPoints, setWireControlPoints] = useState([]);
  const [currentMousePos, setCurrentMousePos] = useState({ x: 0, y: 0 });
  const [snappedPos, setSnappedPos] = useState({ x: 0, y: 0 });
  const [snapGuides, setSnapGuides] = useState({ horizontal: null, vertical: null });
  
  // 포트 호버 상태
  const [hoveredPort, setHoveredPort] = useState(null);
  const [hoveredComponent, setHoveredComponent] = useState(null);
  const [hoveredFRLInstance, setHoveredFRLInstance] = useState(null);
  
  // 버튼/LED 상태
  const [buttonStates, setButtonStates] = useState({});
  
  // 실린더 시스템 상태
  const [cylinderStates, setCylinderStates] = useState({});
  const [sensorStates, setSensorStates] = useState({});
  const [valveStates, setValveStates] = useState({});
  const [isDraggingPiston, setIsDraggingPiston] = useState(false);
  const [draggedPiston, setDraggedPiston] = useState(null);
  
  // 레퍼런스
  const canvasRef = useRef(null);

  // PLC 부품 데이터
  const plcComponents = [
    { 
      id: 'plc', 
      name: 'FX3U-32M PLC', 
      desc: '입력 16개, 출력 16개',
      type: 'plc',
      width: 320,
      height: 180,
      ports: {
        inputs: [
          ...Array.from({length: 8}, (_, i) => ({id: `X${i}`, type: 'input', x: 30 + i * 25, y: 25})),
          ...Array.from({length: 8}, (_, i) => ({id: `X${i+10}`, type: 'input', x: 30 + i * 25, y: 45}))
        ],
        outputs: [
          ...Array.from({length: 8}, (_, i) => ({id: `Y${i}`, type: 'output', x: 30 + i * 25, y: 135})),
          ...Array.from({length: 8}, (_, i) => ({id: `Y${i+10}`, type: 'output', x: 30 + i * 25, y: 155}))
        ]
      }
    },
    { 
      id: 'frl', 
      name: '에어 서비스 유닛', 
      desc: '공압 공급원 (압력 조절 가능)',
      type: 'pneumatic',
      width: 80,
      height: 100,
      ports: {
        airOut: [{id: 'OUT', type: 'air', x: 40, y: 90}]
      }
    },
    { 
      id: 'manifold', 
      name: '에어 메니폴드', 
      desc: '에어 서비스 유닛과 연결',
      type: 'pneumatic',
      width: 120,
      height: 60,
      ports: {
        airIn: [{id: 'IN', type: 'air', x: 10, y: 30}],
        airOut: Array.from({length: 4}, (_, i) => ({id: `OUT${i+1}`, type: 'air', x: 30 + i * 20, y: 50}))
      }
    },
    { 
      id: 'limit', 
      name: '리밋 스위치', 
      desc: '실린더의 피스톤에 반응',
      type: 'sensor',
      width: 60,
      height: 60,
      ports: {
        contacts: [
          {id: 'NO', type: 'contact', x: 18, y: 55}, 
          {id: 'COM', type: 'contact', x: 30, y: 55}, 
          {id: 'NC', type: 'contact', x: 42, y: 55}
        ]
      }
    },
    { 
      id: 'sensor', 
      name: '정전 용량형 센서', 
      desc: '실린더의 피스톤에 반응',
      type: 'sensor',
      width: 60,
      height: 80,
      ports: {
        power: [{id: '+24V', type: 'power', x: 15, y: 75}, {id: '0V', type: 'power', x: 45, y: 75}],
        output: [{id: 'OUT', type: 'output', x: 30, y: 75}]
      }
    },
    { 
      id: 'cylinder', 
      name: '복동 공압 실린더', 
      desc: '여기 피스톤이 있음',
      type: 'cylinder',
      width: 140,
      height: 40,
      ports: {
        airPorts: [{id: 'A', type: 'air', x: 10, y: 20}, {id: 'B', type: 'air', x: 130, y: 20}]
      }
    },
    { 
      id: 'solenoid1', 
      name: '단측 솔레노이드 밸브 (3/2way)', 
      desc: '전자석 1개, 스프링 리턴',
      type: 'valve',
      width: 80,
      height: 100,
      ports: {
        coil: [{id: '+24V', type: 'coil', x: 15, y: 15}, {id: '0V', type: 'coil', x: 15, y: 30}],
        airPorts: [
          {id: 'P', type: 'air', x: 40, y: 90}, 
          {id: 'A', type: 'air', x: 25, y: 55}, 
          {id: 'B', type: 'air', x: 55, y: 55}
        ]
      }
    },
    { 
      id: 'solenoid2', 
      name: '양측 솔레노이드 밸브 (3/2way)', 
      desc: '전자석 2개, 양방향 제어',
      type: 'valve',
      width: 100,
      height: 100,
      ports: {
        coilA: [{id: 'A+24V', type: 'coil', x: 15, y: 15}, {id: 'A0V', type: 'coil', x: 15, y: 30}],
        coilB: [{id: 'B+24V', type: 'coil', x: 85, y: 15}, {id: 'B0V', type: 'coil', x: 85, y: 30}],
        airPorts: [
          {id: 'P', type: 'air', x: 50, y: 90}, 
          {id: 'A', type: 'air', x: 30, y: 55}, 
          {id: 'B', type: 'air', x: 70, y: 55}
        ]
      }
    },
    { 
      id: 'button', 
      name: '푸시버튼 유닛', 
      desc: '푸시버튼이 LED도 함',
      type: 'control',
      width: 200,
      height: 100,
      ports: {
        button1: [
          {id: 'BTN1', type: 'contact', x: 80, y: 20},
          {id: 'BTN1_NO', type: 'contact', x: 100, y: 20},
          {id: 'BTN1_NC', type: 'contact', x: 120, y: 20},
          {id: 'LED1_+', type: 'led', x: 140, y: 20},
          {id: 'LED1_-', type: 'led', x: 160, y: 20}
        ],
        button2: [
          {id: 'BTN2', type: 'contact', x: 80, y: 50},
          {id: 'BTN2_NO', type: 'contact', x: 100, y: 50},
          {id: 'BTN2_NC', type: 'contact', x: 120, y: 50},
          {id: 'LED2_+', type: 'led', x: 140, y: 50},
          {id: 'LED2_-', type: 'led', x: 160, y: 50}
        ],
        button3: [
          {id: 'BTN3', type: 'contact', x: 80, y: 80},
          {id: 'BTN3_NO', type: 'contact', x: 100, y: 80},
          {id: 'BTN3_NC', type: 'contact', x: 120, y: 80},
          {id: 'LED3_+', type: 'led', x: 140, y: 80},
          {id: 'LED3_-', type: 'led', x: 160, y: 80}
        ]
      }
    },
    { 
      id: 'power', 
      name: '파워 서플라이', 
      desc: '전원 공급원',
      type: 'power',
      width: 100,
      height: 80,
      ports: {
        dc: [{id: '+24V', type: 'dc', x: 85, y: 20}, {id: '0V', type: 'dc', x: 85, y: 40}]
      }
    }
  ];

  // 유틸리티 함수들
  const isOutOfBounds = useCallback((x, y, width, height) => {
    const rect = canvasRef.current?.getBoundingClientRect();
    if (!rect) return false;
    return x + width < 0 || y + height < 0 || x > rect.width || y > rect.height;
  }, []);

  const getPortColor = (type) => {
    const colors = {
      'input': '#4CAF50',
      'output': '#FF9800', 
      'power': '#F44336',
      'air': '#2196F3',
      'contact': '#9E9E9E',
      'coil': '#FF0000',
      'led': '#FFEB3B',
      'ac': '#800080',
      'dc': '#FF0000'
    };
    return colors[type] || '#999';
  };

  // 각도 계산 함수
  const calculateAngle = useCallback((mouseX, mouseY, centerX, centerY) => {
    const deltaX = mouseX - centerX;
    const deltaY = mouseY - centerY;
    let angle = Math.atan2(deltaY, deltaX) * (180 / Math.PI);
    if (angle < 0) angle += 360;
    return angle;
  }, []);

  // FRL 더블클릭 처리 함수
  const handleFRLDoubleClick = useCallback((componentId) => {
    const newPressure = prompt('압력을 입력하세요 (0.0 ~ 10.0 bar):', airPressure.toFixed(1));
    if (newPressure !== null) {
      const pressure = parseFloat(newPressure);
      if (!isNaN(pressure) && pressure >= 0 && pressure <= 10) {
        setAirPressure(pressure);
      } else {
        alert('올바른 압력 값을 입력하세요 (0.0 ~ 10.0 bar)');
      }
    }
  }, [airPressure]);

  // 다이얼 클릭 처리 함수
  const handleDialMouseDown = useCallback((e, componentId) => {
    e.stopPropagation();
    
    const rect = canvasRef.current?.getBoundingClientRect();
    if (!rect) return;
    
    const component = placedComponents.find(c => c.instanceId === componentId);
    if (!component) return;
    
    const mouseX = e.clientX - rect.left;
    const mouseY = e.clientY - rect.top;
    const centerX = component.x + 40;
    const centerY = component.y + 55;
    
    const startAngle = calculateAngle(mouseX, mouseY, centerX, centerY);
    
    setIsDraggingPressure(true);
    setPressureDragStart({ 
      angle: startAngle, 
      pressure: airPressure 
    });
  }, [placedComponents, calculateAngle, airPressure]);

  // 버튼 마우스 다운 핸들러 (모멘터리)
  const handleButtonMouseDown = useCallback((e, componentId, buttonNumber) => {
    e.stopPropagation();
    
    setButtonStates(prev => ({
      ...prev,
      [componentId]: {
        ...prev[componentId],
        [`btn${buttonNumber}_pressed`]: true
      }
    }));
  }, []);

  // 버튼 마우스 업 핸들러 (모멘터리)
  const handleButtonMouseUp = useCallback((e, componentId, buttonNumber) => {
    e.stopPropagation();
    
    setButtonStates(prev => ({
      ...prev,
      [componentId]: {
        ...prev[componentId],
        [`btn${buttonNumber}_pressed`]: false
      }
    }));
  }, []);

  // 컴포넌트 삭제 함수
  const deleteComponent = useCallback((componentId) => {
    setPlacedComponents(prev => prev.filter(c => c.instanceId !== componentId));
    setWires(prev => prev.filter(w => 
      w.from?.componentId !== componentId && w.to?.componentId !== componentId
    ));
    setSelectedComponent(null);
    
    setCylinderStates(prev => {
      const newStates = { ...prev };
      delete newStates[componentId];
      return newStates;
    });
    
    setSensorStates(prev => {
      const newStates = { ...prev };
      delete newStates[componentId];
      return newStates;
    });
    
    setValveStates(prev => {
      const newStates = { ...prev };
      delete newStates[componentId];
      return newStates;
    });
    
    setButtonStates(prev => {
      const newStates = { ...prev };
      delete newStates[componentId];
      return newStates;
    });
    
    if (connectionStart?.componentId === componentId) {
      setIsConnecting(false);
      setConnectionStart(null);
      setWireControlPoints([]);
    }
  }, [connectionStart]);

  // 컴포넌트 초기화
  const initializeComponent = useCallback((component) => {
    if (component.type === 'cylinder') {
      setCylinderStates(prev => {
        if (prev[component.instanceId]) {
          return prev;
        }
        
        return {
          ...prev,
          [component.instanceId]: {
            pistonPosition: 0,
            isExtended: false,
            isRetracting: false,
            isExtending: false
          }
        };
      });
    } else if (component.type === 'valve') {
      setValveStates(prev => {
        if (prev[component.instanceId]) {
          return prev;
        }
        
        const newState = component.id === 'solenoid1' 
          ? { type: 'single', coilA: false, coilB: false, position: 'B' }
          : { type: 'double', coilA: false, coilB: false, position: 'B' };
        
        return {
          ...prev,
          [component.instanceId]: newState
        };
      });
    } else if (component.type === 'control') {
      setButtonStates(prev => {
        if (prev[component.instanceId]) {
          return prev;
        }
        
        return {
          ...prev,
          [component.instanceId]: {
            btn1_pressed: false,
            btn1_led: false,
            btn2_pressed: false,
            btn2_led: false,
            btn3_pressed: false,
            btn3_led: false
          }
        };
      });
    }
  }, []);

  // 센서 상태 업데이트
  const updateSensorStates = useCallback(() => {
    const newSensorStates = {};
    
    placedComponents.forEach(component => {
      if (component.type === 'cylinder') {
        const cylinderState = cylinderStates[component.instanceId];
        if (!cylinderState) return;
        
        const pistonHeadX = component.x + (-10 - cylinderState.pistonPosition) + 6;
        const pistonHeadY = component.y + 20;
        
        placedComponents.forEach(sensor => {
          if ((sensor.type === 'sensor' && sensor.id === 'limit')) {
            const sensorX = sensor.x + 30;
            const sensorY = sensor.y + 0;
            const distance = Math.sqrt(Math.pow(pistonHeadX - sensorX, 2) + Math.pow(pistonHeadY - sensorY, 2));
            
            if (distance < 25) {
              newSensorStates[sensor.instanceId] = { activated: true, type: 'limit' };
            } else if (!newSensorStates[sensor.instanceId]) {
              newSensorStates[sensor.instanceId] = { activated: false, type: 'limit' };
            }
          }
          
          if ((sensor.type === 'sensor' && sensor.id === 'sensor')) {
            const sensorX = sensor.x + 30;
            const sensorY = sensor.y + 5;
            const distance = Math.sqrt(Math.pow(pistonHeadX - sensorX, 2) + Math.pow(pistonHeadY - sensorY, 2));
            
            if (distance < 30) {
              newSensorStates[sensor.instanceId] = { activated: true, type: 'proximity' };
            } else if (!newSensorStates[sensor.instanceId]) {
              newSensorStates[sensor.instanceId] = { activated: false, type: 'proximity' };
            }
          }
        });
      }
    });
    
    setSensorStates(newSensorStates);
  }, [placedComponents, cylinderStates]);

  // 휠 스크롤로 FRL 압력 조절
  const handleWheel = useCallback((e) => {
    const rect = canvasRef.current?.getBoundingClientRect();
    if (!rect) return;
    
    const mouseX = e.clientX - rect.left;
    const mouseY = e.clientY - rect.top;
    
    let isNearFRLDial = false;
    placedComponents.forEach(component => {
      if (component.id === 'frl') {
        const dialCenterX = component.x + 40;
        const dialCenterY = component.y + 55;
        const distance = Math.sqrt(Math.pow(mouseX - dialCenterX, 2) + Math.pow(mouseY - dialCenterY, 2));
        
        if (distance <= 20) {
          isNearFRLDial = true;
        }
      }
    });
    
    if (isNearFRLDial) {
      e.preventDefault();
      const delta = e.deltaY > 0 ? -0.3 : 0.3;
      const newPressure = Math.max(0, Math.min(10, airPressure + delta));
      setAirPressure(newPressure);
    }
  }, [airPressure, placedComponents]);

  const handlePistonMouseDown = useCallback((e, componentId) => {
    if (selectedTool !== 'select') return;
    
    e.stopPropagation();
    setIsDraggingPiston(true);
    setDraggedPiston(componentId);
  }, [selectedTool]);

  // 스냅 기능
  const snapToGrid = useCallback((x, y, snapDistance = 15) => {
    const gridSize = 20;
    let snappedX = x;
    let snappedY = y;
    let guides = { horizontal: null, vertical: null };

    const gridX = Math.round(x / gridSize) * gridSize;
    const gridY = Math.round(y / gridSize) * gridSize;
    
    if (Math.abs(x - gridX) < snapDistance) {
      snappedX = gridX;
      guides.vertical = gridX;
    }
    
    if (Math.abs(y - gridY) < snapDistance) {
      snappedY = gridY;
      guides.horizontal = gridY;
    }

    if (isConnecting) {
      const allPoints = [connectionStart, ...wireControlPoints];
      
      for (const point of allPoints) {
        if (Math.abs(y - point.y) < snapDistance) {
          snappedY = point.y;
          guides.horizontal = point.y;
          break;
        }
        
        if (Math.abs(x - point.x) < snapDistance) {
          snappedX = point.x;
          guides.vertical = point.x;
          break;
        }
      }
    }

    placedComponents.forEach(component => {
      const allPorts = Object.values(component.ports).flat();
      allPorts.forEach(port => {
        const portX = component.x + port.x;
        const portY = component.y + port.y;
        
        if (Math.abs(y - portY) < snapDistance) {
          snappedY = portY;
          guides.horizontal = portY;
        }
        
        if (Math.abs(x - portX) < snapDistance) {
          snappedX = portX;
          guides.vertical = portX;
        }
      });
    });

    return { x: snappedX, y: snappedY, guides };
  }, [isConnecting, connectionStart, wireControlPoints, placedComponents]);

  // 이벤트 핸들러들
  const handleComponentMouseDown = useCallback((e, componentId) => {
    if (selectedTool !== 'select') return;
    
    e.stopPropagation();
    const component = placedComponents.find(c => c.instanceId === componentId);
    if (!component) return;

    setSelectedComponent(componentId);
    setIsDraggingComponent(true);
    
    const rect = canvasRef.current.getBoundingClientRect();
    const offsetX = e.clientX - rect.left - component.x;
    const offsetY = e.clientY - rect.top - component.y;
    setDragOffset({ x: offsetX, y: offsetY });
  }, [selectedTool, placedComponents]);

  const handleMouseMove = useCallback((e) => {
    const rect = canvasRef.current?.getBoundingClientRect();
    if (!rect) return;
    
    const mouseX = e.clientX - rect.left;
    const mouseY = e.clientY - rect.top;
    setCurrentMousePos({ x: mouseX, y: mouseY });

    const snapResult = snapToGrid(mouseX, mouseY);
    setSnappedPos({ x: snapResult.x, y: snapResult.y });
    setSnapGuides(snapResult.guides);

    let isHoveringFRLDial = false;
    placedComponents.forEach(component => {
      if (component.id === 'frl') {
        const frlX = mouseX - component.x - 40;
        const frlY = mouseY - component.y - 55;
        const distance = Math.sqrt(frlX * frlX + frlY * frlY);
        if (distance <= 15) {
          isHoveringFRLDial = true;
        }
      }
    });
    
    if (isHoveringFRLDial && !hoveredComponent) {
      setHoveredComponent('frl_dial');
    } else if (!isHoveringFRLDial && hoveredComponent === 'frl_dial') {
      setHoveredComponent(null);
    }

    if (isDraggingPressure) {
      let currentComponent = null;
      placedComponents.forEach(component => {
        if (component.id === 'frl') {
          const centerX = component.x + 40;
          const centerY = component.y + 55;
          const distance = Math.sqrt(Math.pow(mouseX - centerX, 2) + Math.pow(mouseY - centerY, 2));
          if (distance <= 30) {
            currentComponent = component;
          }
        }
      });
      
      if (currentComponent) {
        const centerX = currentComponent.x + 40;
        const centerY = currentComponent.y + 55;
        const currentAngle = calculateAngle(mouseX, mouseY, centerX, centerY);
        
        let angleDiff = currentAngle - pressureDragStart.angle;
        
        if (angleDiff > 180) angleDiff -= 360;
        if (angleDiff < -180) angleDiff += 360;
        
        const pressureChange = (angleDiff / 180) * 10;
        const newPressure = Math.max(0, Math.min(10, pressureDragStart.pressure + pressureChange));
        
        setAirPressure(newPressure);
      }
      return;
    }

    if (isDraggingComponent && selectedComponent) {
      const newX = Math.max(-200, mouseX - dragOffset.x);
      const newY = Math.max(-200, mouseY - dragOffset.y);
      
      setPlacedComponents(prev => prev.map(comp => 
        comp.instanceId === selectedComponent 
          ? { ...comp, x: newX, y: newY }
          : comp
      ));
    }

    if (isDraggingPiston && draggedPiston) {
      const component = placedComponents.find(c => c.instanceId === draggedPiston);
      if (component && cylinderStates[draggedPiston]) {
        const targetHeadX = mouseX - component.x - 6;
        const newPosition = -10 - targetHeadX;
        const constrainedPosition = Math.max(0, Math.min(80, newPosition));
        
        setCylinderStates(prev => ({
          ...prev,
          [draggedPiston]: {
            ...prev[draggedPiston],
            pistonPosition: constrainedPosition
          }
        }));
      }
    }
  }, [snapToGrid, isDraggingComponent, selectedComponent, dragOffset, isDraggingPiston, draggedPiston, placedComponents, isDraggingPressure, pressureDragStart, airPressure, hoveredComponent, calculateAngle]);

  const handleMouseUp = useCallback(() => {
    if (isDraggingComponent && selectedComponent) {
      const component = placedComponents.find(c => c.instanceId === selectedComponent);
      if (component && isOutOfBounds(component.x, component.y, component.width, component.height)) {
        deleteComponent(selectedComponent);
      }
    }
    
    // 모든 버튼 누름 상태 해제
    setButtonStates(prev => {
      const newStates = {};
      let hasChanges = false;
      
      Object.keys(prev).forEach(componentId => {
        const currentState = prev[componentId];
        const newState = { ...currentState };
        
        for (let i = 1; i <= 3; i++) {
          if (newState[`btn${i}_pressed`]) {
            newState[`btn${i}_pressed`] = false;
            hasChanges = true;
          }
        }
        
        newStates[componentId] = newState;
      });
      
      return hasChanges ? newStates : prev;
    });
    
    setIsDraggingComponent(false);
    setIsDraggingPiston(false);
    setDraggedPiston(null);
    setIsDraggingPressure(false);
  }, [isDraggingComponent, selectedComponent, placedComponents, isOutOfBounds, deleteComponent]);

  const handleMouseLeave = useCallback(() => {
    setSnapGuides({ horizontal: null, vertical: null });
    setHoveredComponent(null);
    setHoveredFRLInstance(null);
    setIsDraggingPressure(false);
  }, []);

  const handleCanvasClick = useCallback((e) => {
    if (selectedTool === 'select') {
      setSelectedComponent(null);
    } else if (isConnecting && (selectedTool === 'electric' || selectedTool === 'pneumatic')) {
      setWireControlPoints(prev => [...prev, { x: snappedPos.x, y: snappedPos.y }]);
    }
  }, [selectedTool, isConnecting, snappedPos]);

  const handleCanvasDoubleClick = useCallback((e) => {
    if (isConnecting) {
      setIsConnecting(false);
      setConnectionStart(null);
      setWireControlPoints([]);
    }
  }, [isConnecting]);

  const handleDragStart = useCallback((e, component) => {
    e.dataTransfer.setData('component', JSON.stringify(component));
    setDraggedComponent(component);
  }, []);

  const handleDrop = useCallback((e) => {
    e.preventDefault();
    const componentData = e.dataTransfer.getData('component');
    if (!componentData) return;

    const component = JSON.parse(componentData);
    const rect = canvasRef.current.getBoundingClientRect();
    const x = e.clientX - rect.left - component.width / 2;
    const y = e.clientY - rect.top - component.height / 2;

    const newComponent = {
      ...component,
      instanceId: `${component.id}_${Date.now()}`,
      x: Math.max(0, x),
      y: Math.max(0, y)
    };

    setPlacedComponents(prev => [...prev, newComponent]);
    
    if (component.type === 'cylinder' || component.type === 'valve' || component.type === 'control') {
      initializeComponent(newComponent);
    }
  }, [initializeComponent]);

  const handlePortClick = useCallback((e, componentId, port) => {
    e.stopPropagation();
    
    if (selectedTool !== 'electric' && selectedTool !== 'pneumatic') return;

    const component = placedComponents.find(c => c.instanceId === componentId);
    if (!component) return;

    const portPosition = Object.values(component.ports).flat().find(p => p.id === port.id);
    if (!portPosition) return;

    const portX = component.x + portPosition.x;
    const portY = component.y + portPosition.y;

    if (!isConnecting) {
      setIsConnecting(true);
      setConnectionStart({ 
        componentId, 
        port, 
        x: portX, 
        y: portY 
      });
      setWireControlPoints([]);
    } else {
      if (connectionStart && connectionStart.componentId !== componentId) {
        const newWire = {
          id: `wire_${Date.now()}`,
          from: connectionStart,
          to: { componentId, port, x: portX, y: portY },
          controlPoints: [...wireControlPoints],
          type: selectedTool
        };
        setWires(prev => [...prev, newWire]);
      }
      setIsConnecting(false);
      setConnectionStart(null);
      setWireControlPoints([]);
    }
  }, [selectedTool, placedComponents, isConnecting, connectionStart, wireControlPoints]);

  // 키보드 이벤트 및 전역 마우스 이벤트
  useEffect(() => {
    const handleKeyDown = (e) => {
      if ((e.key === 'Delete' || e.key === 'Backspace') && selectedComponent && selectedTool === 'select') {
        e.preventDefault();
        deleteComponent(selectedComponent);
      }
      
      if (hoveredComponent === 'frl_dial' && hoveredFRLInstance) {
        if (e.key === 'ArrowUp') {
          e.preventDefault();
          const newPressure = Math.min(10, airPressure + 0.1);
          setAirPressure(newPressure);
        } else if (e.key === 'ArrowDown') {
          e.preventDefault();
          const newPressure = Math.max(0, airPressure - 0.1);
          setAirPressure(newPressure);
        } else if (e.key === 'Enter' || e.key === ' ') {
          e.preventDefault();
          handleFRLDoubleClick(hoveredFRLInstance);
        }
      }
    };

    const handleGlobalMouseUp = () => {
      // 모든 버튼 누름 상태 해제 (전역)
      setButtonStates(prev => {
        const newStates = {};
        let hasChanges = false;
        
        Object.keys(prev).forEach(componentId => {
          const currentState = prev[componentId];
          const newState = { ...currentState };
          
          for (let i = 1; i <= 3; i++) {
            if (newState[`btn${i}_pressed`]) {
              newState[`btn${i}_pressed`] = false;
              hasChanges = true;
            }
          }
          
          newStates[componentId] = newState;
        });
        
        return hasChanges ? newStates : prev;
      });
    };

    window.addEventListener('keydown', handleKeyDown);
    window.addEventListener('mouseup', handleGlobalMouseUp);
    
    return () => {
      window.removeEventListener('keydown', handleKeyDown);
      window.removeEventListener('mouseup', handleGlobalMouseUp);
    };
  }, [selectedComponent, selectedTool, deleteComponent, hoveredComponent, hoveredFRLInstance, airPressure, handleFRLDoubleClick]);

  // 실린더 및 밸브 초기화
  useEffect(() => {
    const cylinderComponents = placedComponents.filter(c => c.type === 'cylinder');
    const valveComponents = placedComponents.filter(c => c.type === 'valve');
    const controlComponents = placedComponents.filter(c => c.type === 'control');
    
    setCylinderStates(prev => {
      const newStates = { ...prev };
      let hasChanges = false;
      
      cylinderComponents.forEach(component => {
        if (!newStates[component.instanceId]) {
          newStates[component.instanceId] = {
            pistonPosition: 0,
            isExtended: false,
            isRetracting: false,
            isExtending: false
          };
          hasChanges = true;
        }
      });
      
      Object.keys(newStates).forEach(cylinderId => {
        if (!cylinderComponents.find(c => c.instanceId === cylinderId)) {
          delete newStates[cylinderId];
          hasChanges = true;
        }
      });
      
      return hasChanges ? newStates : prev;
    });
    
    setValveStates(prev => {
      const newStates = { ...prev };
      let hasChanges = false;
      
      valveComponents.forEach(component => {
        if (!newStates[component.instanceId]) {
          if (component.id === 'solenoid1') {
            newStates[component.instanceId] = {
              type: 'single',
              coilA: false,
              coilB: false,
              position: 'B'
            };
          } else if (component.id === 'solenoid2') {
            newStates[component.instanceId] = {
              type: 'double',
              coilA: false,
              coilB: false,
              position: 'B'
            };
          }
          hasChanges = true;
        }
      });
      
      Object.keys(newStates).forEach(valveId => {
        if (!valveComponents.find(c => c.instanceId === valveId)) {
          delete newStates[valveId];
          hasChanges = true;
        }
      });
      
      return hasChanges ? newStates : prev;
    });

    setButtonStates(prev => {
      const newStates = { ...prev };
      let hasChanges = false;
      
      controlComponents.forEach(component => {
        if (!newStates[component.instanceId]) {
          newStates[component.instanceId] = {
            btn1_pressed: false,
            btn1_led: false,
            btn2_pressed: false,
            btn2_led: false,
            btn3_pressed: false,
            btn3_led: false
          };
          hasChanges = true;
        }
      });
      
      Object.keys(newStates).forEach(buttonId => {
        if (!controlComponents.find(c => c.instanceId === buttonId)) {
          delete newStates[buttonId];
          hasChanges = true;
        }
      });
      
      return hasChanges ? newStates : prev;
    });
  }, [placedComponents]);

  // 실린더 자동 동작
  useEffect(() => {
    const interval = setInterval(() => {
      setCylinderStates(prev => {
        const newStates = {};
        let hasChanges = false;
        
        Object.keys(prev).forEach(cylinderId => {
          const state = prev[cylinderId];
          let newPosition = state.pistonPosition;
          
          if (state.isExtending && state.pistonPosition < 80) {
            newPosition = Math.min(80, state.pistonPosition + 2);
            hasChanges = true;
          } else if (state.isRetracting && state.pistonPosition > 0) {
            newPosition = Math.max(0, state.pistonPosition - 2);
            hasChanges = true;
          }
          
          newStates[cylinderId] = {
            ...state,
            pistonPosition: newPosition
          };
        });
        
        return hasChanges ? newStates : prev;
      });
    }, 50);
    
    return () => clearInterval(interval);
  }, []);

  // LED 제어 업데이트 (전기 배선에 따라)
  useEffect(() => {
    const electricWires = wires.filter(w => w.type === 'electric');
    
    placedComponents.forEach(component => {
      if (component.type === 'control') {
        const componentId = component.instanceId;
        
        setButtonStates(prev => {
          const currentState = prev[componentId] || {};
          const newState = { ...currentState };
          let hasChanges = false;
          
          // 각 LED에 대해 전원 연결 확인
          for (let i = 1; i <= 3; i++) {
            const hasPositive = electricWires.some(wire => 
              wire.to.componentId === componentId && wire.to.port.id === `LED${i}_+`
            );
            const hasNegative = electricWires.some(wire => 
              wire.to.componentId === componentId && wire.to.port.id === `LED${i}_-`
            );
            
            const ledShouldBeOn = hasPositive && hasNegative;
            
            if (newState[`btn${i}_led`] !== ledShouldBeOn) {
              newState[`btn${i}_led`] = ledShouldBeOn;
              hasChanges = true;
            }
          }
          
          return hasChanges ? { ...prev, [componentId]: newState } : prev;
        });
      }
    });
  }, [wires, placedComponents]);

  // 밸브 제어 업데이트
  useEffect(() => {
    const electricWires = wires.filter(w => w.type === 'electric');
    const newValveStates = {};
    
    placedComponents.forEach(component => {
      if (component.type === 'valve') {
        const valveId = component.instanceId;
        
        if (component.id === 'solenoid1') {
          let coilPowered = false;
          
          const hasPositive = electricWires.some(wire => 
            wire.to.componentId === valveId && wire.to.port.id === '+24V'
          );
          const hasNegative = electricWires.some(wire => 
            wire.to.componentId === valveId && wire.to.port.id === '0V'
          );
          
          coilPowered = hasPositive && hasNegative;
          
          newValveStates[valveId] = {
            type: 'single',
            coilA: coilPowered,
            coilB: false,
            position: coilPowered ? 'A' : 'B'
          };
        }
        
        else if (component.id === 'solenoid2') {
          let coilAPowered = false;
          let coilBPowered = false;
          
          const hasPositiveA = electricWires.some(wire => 
            wire.to.componentId === valveId && wire.to.port.id === 'A+24V'
          );
          const hasNegativeA = electricWires.some(wire => 
            wire.to.componentId === valveId && wire.to.port.id === 'A0V'
          );
          
          const hasPositiveB = electricWires.some(wire => 
            wire.to.componentId === valveId && wire.to.port.id === 'B+24V'
          );
          const hasNegativeB = electricWires.some(wire => 
            wire.to.componentId === valveId && wire.to.port.id === 'B0V'
          );
          
          coilAPowered = hasPositiveA && hasNegativeA;
          coilBPowered = hasPositiveB && hasNegativeB;
          
          if (coilAPowered && coilBPowered) {
            coilBPowered = false;
          }
          
          let position = 'B';
          if (coilAPowered || coilBPowered) position = 'A';
          
          newValveStates[valveId] = {
            type: 'double',
            coilA: coilAPowered,
            coilB: coilBPowered,
            position: position
          };
        }
      }
    });
    
    setValveStates(newValveStates);
  }, [wires, placedComponents]);

  // 공압 제어 업데이트
  useEffect(() => {
    const pneumaticWires = wires.filter(w => w.type === 'pneumatic');
    
    pneumaticWires.forEach(wire => {
      const fromComponent = placedComponents.find(c => c.instanceId === wire.from.componentId);
      const toComponent = placedComponents.find(c => c.instanceId === wire.to.componentId);
      
      if (fromComponent && toComponent) {
        if (fromComponent.type === 'valve' && toComponent.type === 'cylinder') {
          const valveState = valveStates[fromComponent.instanceId];
          const cylinderState = cylinderStates[toComponent.instanceId];
          
          if (valveState && cylinderState && airPressure >= 3.0) {
            if (valveState.position === 'A' && wire.from.port.id === 'A') {
              if (wire.to.port.id === 'A') {
                setCylinderStates(prev => ({
                  ...prev,
                  [toComponent.instanceId]: {
                    ...prev[toComponent.instanceId],
                    isExtending: true,
                    isRetracting: false
                  }
                }));
              }
            } else if (valveState.position === 'B' && wire.from.port.id === 'B') {
              if (wire.to.port.id === 'B') {
                setCylinderStates(prev => ({
                  ...prev,
                  [toComponent.instanceId]: {
                    ...prev[toComponent.instanceId],
                    isExtending: false,
                    isRetracting: true
                  }
                }));
              }
            } else {
              setCylinderStates(prev => ({
                ...prev,
                [toComponent.instanceId]: {
                  ...prev[toComponent.instanceId],
                  isExtending: false,
                  isRetracting: false
                }
              }));
            }
          }
        }
      }
    });
  }, [wires, placedComponents, cylinderStates, valveStates, airPressure]);

  // 센서 상태 업데이트
  useEffect(() => {
    updateSensorStates();
  }, [cylinderStates, placedComponents, updateSensorStates]);

  // 직선 경로 생성
  const createStraightPath = (startPoint, endPoint, controlPoints) => {
    const allPoints = [startPoint, ...controlPoints, endPoint];
    let path = `M ${allPoints[0].x} ${allPoints[0].y}`;
    
    for (let i = 1; i < allPoints.length; i++) {
      path += ` L ${allPoints[i].x} ${allPoints[i].y}`;
    }
    
    return path;
  };

  // 배선 렌더링
  const renderWires = () => {
    return (
      <>
        {isConnecting && (snapGuides.horizontal !== null || snapGuides.vertical !== null) && (
          <g className="pointer-events-none">
            {snapGuides.horizontal !== null && (
              <line
                x1="0"
                y1={snapGuides.horizontal}
                x2="100%"
                y2={snapGuides.horizontal}
                stroke="#00ff00"
                strokeWidth="1"
                strokeDasharray="3,3"
                opacity="0.7"
              />
            )}
            {snapGuides.vertical !== null && (
              <line
                x1={snapGuides.vertical}
                y1="0"
                x2={snapGuides.vertical}
                y2="100%"
                stroke="#00ff00"
                strokeWidth="1"
                strokeDasharray="3,3"
                opacity="0.7"
              />
            )}
          </g>
        )}

        {wires.map(wire => {
          const fromComponent = placedComponents.find(c => c.instanceId === wire.from.componentId);
          const toComponent = placedComponents.find(c => c.instanceId === wire.to.componentId);
          
          if (!fromComponent || !toComponent) return null;

          const fromPort = Object.values(fromComponent.ports).flat().find(p => p.id === wire.from.port.id);
          const toPort = Object.values(toComponent.ports).flat().find(p => p.id === wire.to.port.id);

          if (!fromPort || !toPort) return null;

          const startPoint = {
            x: fromComponent.x + fromPort.x,
            y: fromComponent.y + fromPort.y
          };
          
          const endPoint = {
            x: toComponent.x + toPort.x,
            y: toComponent.y + toPort.y
          };

          const pathString = createStraightPath(startPoint, endPoint, wire.controlPoints || []);

          return (
            <g key={wire.id}>
              <path
                d={pathString}
                fill="none"
                stroke={wire.type === 'electric' ? '#FF6B35' : '#4A90E2'}
                strokeWidth="4"
                strokeLinecap="round"
                strokeLinejoin="round"
                className="cursor-pointer hover:stroke-red-500"
                onClick={() => setWires(prev => prev.filter(w => w.id !== wire.id))}
              />
              
              <circle
                cx={startPoint.x}
                cy={startPoint.y}
                r="4"
                fill={wire.type === 'electric' ? '#FF6B35' : '#4A90E2'}
                className="pointer-events-none"
              />
              
              <circle
                cx={endPoint.x}
                cy={endPoint.y}
                r="4"
                fill={wire.type === 'electric' ? '#FF6B35' : '#4A90E2'}
                className="pointer-events-none"
              />

              {wire.controlPoints?.map((point, index) => (
                <circle
                  key={index}
                  cx={point.x}
                  cy={point.y}
                  r="3"
                  fill={wire.type === 'electric' ? '#FF6B35' : '#4A90E2'}
                  stroke="#fff"
                  strokeWidth="2"
                  className="cursor-move"
                />
              ))}
            </g>
          );
        })}

        {isConnecting && connectionStart && (
          <g>
            {wireControlPoints.length > 0 ? (
              <>
                <path
                  d={createStraightPath(
                    connectionStart, 
                    snappedPos, 
                    wireControlPoints
                  )}
                  fill="none"
                  stroke={selectedTool === 'electric' ? '#FF6B35' : '#4A90E2'}
                  strokeWidth="4"
                  strokeLinecap="round"
                  strokeDasharray="8,4"
                  className="pointer-events-none"
                />
                
                {wireControlPoints.map((point, index) => (
                  <circle
                    key={index}
                    cx={point.x}
                    cy={point.y}
                    r="4"
                    fill={selectedTool === 'electric' ? '#FF6B35' : '#4A90E2'}
                    stroke="#fff"
                    strokeWidth="2"
                    className="pointer-events-none"
                  />
                ))}
              </>
            ) : (
              <line
                x1={connectionStart.x}
                y1={connectionStart.y}
                x2={snappedPos.x}
                y2={snappedPos.y}
                stroke={selectedTool === 'electric' ? '#FF6B35' : '#4A90E2'}
                strokeWidth="4"
                strokeLinecap="round"
                strokeDasharray="8,4"
                className="pointer-events-none"
              />
            )}
            
            <circle
              cx={connectionStart.x}
              cy={connectionStart.y}
              r="5"
              fill={selectedTool === 'electric' ? '#FF6B35' : '#4A90E2'}
              stroke="#fff"
              strokeWidth="2"
              className="pointer-events-none"
            />
            
            <circle
              cx={snappedPos.x}
              cy={snappedPos.y}
              r="3"
              fill={selectedTool === 'electric' ? '#FF6B35' : '#4A90E2'}
              className="pointer-events-none"
            />

            {(snapGuides.horizontal !== null || snapGuides.vertical !== null) && (
              <g className="pointer-events-none">
                <circle
                  cx={snappedPos.x}
                  cy={snappedPos.y}
                  r="8"
                  fill="none"
                  stroke="#00ff00"
                  strokeWidth="2"
                />
                <path
                  d={`M${snappedPos.x - 6},${snappedPos.y} L${snappedPos.x + 6},${snappedPos.y} M${snappedPos.x},${snappedPos.y - 6} L${snappedPos.x},${snappedPos.y + 6}`}
                  stroke="#00ff00"
                  strokeWidth="2"
                />
              </g>
            )}
          </g>
        )}
      </>
    );
  };

  // 컴포넌트 렌더링
  const renderComponent = (component, x, y, id) => {
    const allPorts = Object.values(component.ports).flat();
    const isSelected = selectedComponent === id;
    const isOutOfBoundsNow = isOutOfBounds(x, y, component.width, component.height);
    const opacity = isDraggingComponent && selectedComponent === id && isOutOfBoundsNow ? 0.5 : 1;
    
    return (
      <g key={id} transform={`translate(${x}, ${y})`} data-component-id={id} opacity={opacity}>
        {isDraggingComponent && selectedComponent === id && isOutOfBoundsNow && (
          <g>
            <rect
              width={component.width + 20}
              height={component.height + 20}
              x={-10}
              y={-10}
              fill="none"
              stroke="#ff4444"
              strokeWidth="3"
              strokeDasharray="10,5"
              rx="10"
            />
          </g>
        )}

        {isSelected && (
          <rect
            width={component.width + 10}
            height={component.height + 10}
            x={-5}
            y={-5}
            fill="none"
            stroke="#2196F3"
            strokeWidth="2"
            strokeDasharray="5,5"
          />
        )}
        
        <rect
          width={component.width}
          height={component.height}
          fill="#f0f0f0"
          stroke="#333"
          strokeWidth="2"
          rx="5"
          className="cursor-move"
          onMouseDown={(e) => handleComponentMouseDown(e, id)}
        />

        {/* PLC 특별 렌더링 */}
        {component.type === 'plc' && (
          <>
            <rect x="20" y="15" width="210" height="50" fill="#e8e8e8" stroke="#333" strokeWidth="1" rx="3"/>
            <rect x="20" y="115" width="210" height="50" fill="#e8e8e8" stroke="#333" strokeWidth="1" rx="3"/>
            
            {Array.from({length: 16}, (_, i) => (
              <circle 
                key={`input-${i}`}
                cx={30 + (i % 8) * 25} 
                cy={i < 8 ? 25 : 45} 
                r="3" 
                fill="#4CAF50" 
                stroke="#333" 
                strokeWidth="1"
              />
            ))}
            
            {Array.from({length: 16}, (_, i) => (
              <circle 
                key={`output-${i}`}
                cx={30 + (i % 8) * 25} 
                cy={i < 8 ? 135 : 155} 
                r="3" 
                fill="#FF9800" 
                stroke="#333" 
                strokeWidth="1"
              />
            ))}
            
            <rect x="250" y="70" width="50" height="40" fill="#2c3e50" stroke="#333" strokeWidth="2" rx="3"/>
            <circle cx="275" cy="80" r="3" fill={plcRunning ? "#00ff00" : "#666"} stroke="#333"/>
          </>
        )}

        {/* 복동실린더 특별 렌더링 */}
        {component.type === 'cylinder' && (
          <>
            {(() => {
              const cylinderState = cylinderStates[id];
              if (!cylinderState) {
                return (
                  <g>
                    <rect x="25" y="12" width="80" height="16" fill="#ffcccc" stroke="#ff0000" strokeWidth="2" rx="2"/>
                  </g>
                );
              }
              
              const pistonHeadX = -10 - cylinderState.pistonPosition;
              const internalPistonX = 97 - cylinderState.pistonPosition * 0.875;
              const rodStartX = pistonHeadX + 12;
              const rodEndX = internalPistonX;
              const rodLength = rodEndX - rodStartX;
              
              return (
                <>
                  <rect x="25" y="12" width="80" height="16" fill="#e0e0e0" stroke="#333" strokeWidth="2" rx="2"/>
                  <rect x="27" y="14" width="76" height="12" fill="#f5f5f5" stroke="none"/>
                  <rect x="20" y="14" width="7" height="12" fill="#888" stroke="#333" strokeWidth="2" rx="1"/>
                  <circle cx="23.5" cy="20" r="2" fill="#666" stroke="#333"/>
                  <rect x="103" y="14" width="7" height="12" fill="#888" stroke="#333" strokeWidth="2" rx="1"/>
                  
                  <rect 
                    x={internalPistonX} 
                    y="15" 
                    width="6" 
                    height="10" 
                    fill="#666" 
                    stroke="#333" 
                    strokeWidth="1"
                    rx="1"
                  />
                  
                  <rect 
                    x={rodStartX} 
                    y="19" 
                    width={rodLength} 
                    height="2" 
                    fill="#888" 
                    stroke="#333"
                    strokeWidth="1"
                  />
                  
                  <rect 
                    x={pistonHeadX} 
                    y="15" 
                    width="12" 
                    height="10" 
                    fill="#666" 
                    stroke="#333"
                    strokeWidth="2"
                    rx="2"
                    className="cursor-move hover:fill-gray-500"
                    onMouseDown={(e) => handlePistonMouseDown(e, id)}
                  />
                  
                  <circle 
                    cx={pistonHeadX + 6} 
                    cy="20" 
                    r="2" 
                    fill="#888" 
                    stroke="#333"
                    strokeWidth="1"
                  />
                  
                  <rect x="8" y="18" width="4" height="4" fill="#2196F3" stroke="#333" strokeWidth="1"/>
                  <rect x="128" y="18" width="4" height="4" fill="#2196F3" stroke="#333" strokeWidth="1"/>
                  <text x="10" y="16" textAnchor="middle" className="text-xs font-bold" fill="#2196F3">A</text>
                  <text x="130" y="16" textAnchor="middle" className="text-xs font-bold" fill="#2196F3">B</text>
                  
                  <text x="70" y="10" textAnchor="middle" className="text-xs" fill="#666">
                    {Math.round(cylinderState.pistonPosition)}mm
                  </text>
                  
                  {cylinderState.isExtending && (
                    <text x="70" y="35" textAnchor="middle" className="text-xs" fill="#00aa00" fontWeight="bold">
                      전진 중
                    </text>
                  )}
                  {cylinderState.isRetracting && (
                    <text x="70" y="35" textAnchor="middle" className="text-xs" fill="#aa0000" fontWeight="bold">
                      후진 중
                    </text>
                  )}
                  
                  {isSelected && (
                    <>
                      <line x1={-10} y1="8" x2={-10} y2="32" stroke="#ff6600" strokeWidth="1" strokeDasharray="2,2"/>
                      <line x1={-90} y1="8" x2={-90} y2="32" stroke="#ff6600" strokeWidth="1" strokeDasharray="2,2"/>
                      <text x="-50" y="6" textAnchor="middle" className="text-xs" fill="#ff6600">행정: 80mm</text>
                      
                      <circle 
                        cx={pistonHeadX + 6} 
                        cy="20" 
                        r="15" 
                        fill="none" 
                        stroke="#00ff00" 
                        strokeWidth="1" 
                        strokeDasharray="3,3" 
                        opacity="0.7"
                      />
                      <text x={pistonHeadX + 6} y="40" textAnchor="middle" className="text-xs" fill="#00ff00">
                        센서 감지 대상
                      </text>
                    </>
                  )}
                </>
              );
            })()}
          </>
        )}

        {/* 리밋 스위치 렌더링 */}
        {(component.type === 'sensor' && component.id === 'limit') && (
          <>
            {(() => {
              const sensorState = sensorStates[id] || { activated: false };
              const isActivated = sensorState.activated;
              
              return (
                <>
                  <rect x="5" y="10" width="50" height="30" fill="#2c2c2c" stroke="#333" strokeWidth="2" rx="3"/>
                  <rect x="20" y="5" width="20" height="8" fill="#444" stroke="#333" strokeWidth="1" rx="2"/>
                  <rect x="28" y="0" width="4" height="10" fill="#444" stroke="#333" strokeWidth="1"/>
                  <circle cx="30" cy="0" r="3" fill={isActivated ? "#ff6600" : "#888"} stroke="#333" strokeWidth="1"/>
                  <circle cx="50" cy="20" r="2" fill={isActivated ? "#00ff00" : "#ff0000"} stroke="#333"/>
                  <rect x="10" y="42" width="40" height="15" fill="#2d5016" stroke="#333" strokeWidth="1" rx="2"/>
                  <circle cx="18" cy="49" r="2" fill="#c0c0c0" stroke="#333" strokeWidth="1"/>
                  <circle cx="30" cy="49" r="2" fill="#c0c0c0" stroke="#333" strokeWidth="1"/>
                  <circle cx="42" cy="49" r="2" fill="#c0c0c0" stroke="#333" strokeWidth="1"/>
                  <text x="30" y="28" textAnchor="middle" className="text-xs" fill="#fff">LIMIT</text>
                  
                  {isSelected && (
                    <circle cx="30" cy="0" r="25" fill="none" stroke="#ff6600" strokeWidth="1" strokeDasharray="3,3" opacity="0.5"/>
                  )}
                </>
              );
            })()}
          </>
        )}

        {/* 용량형 센서 렌더링 */}
        {(component.type === 'sensor' && component.id === 'sensor') && (
          <>
            {(() => {
              const sensorState = sensorStates[id] || { activated: false };
              const isActivated = sensorState.activated;
              
              return (
                <>
                  <rect x="20" y="5" width="20" height="65" fill="#c0c0c0" stroke="#333" strokeWidth="2" rx="10"/>
                  <rect x="22" y="2" width="16" height="10" fill={isActivated ? "#90EE90" : "#1976D2"} stroke="#333" strokeWidth="2" rx="2"/>
                  <circle cx="30" cy="15" r="3" fill={isActivated ? "#00ff00" : "#ff0000"} stroke="#333" strokeWidth="1"/>
                  
                  {/* 하단 연결부 - 3개 포트 */}
                  <rect x="12" y="68" width="8" height="8" fill="#333" stroke="#333" strokeWidth="1" rx="2"/>
                  <rect x="26" y="68" width="8" height="8" fill="#333" stroke="#333" strokeWidth="1" rx="2"/>
                  <rect x="40" y="68" width="8" height="8" fill="#333" stroke="#333" strokeWidth="1" rx="2"/>
                  
                  {/* 케이블 */}
                  <rect x="14" y="76" width="4" height="5" fill="#333"/>
                  <rect x="28" y="76" width="4" height="5" fill="#333"/>
                  <rect x="42" y="76" width="4" height="5" fill="#333"/>
                  
                  {isSelected && (
                    <circle cx="30" cy="7" r="30" fill="none" stroke="#1976D2" strokeWidth="1" strokeDasharray="3,3" opacity="0.5"/>
                  )}
                </>
              );
            })()}
          </>
        )}

        {/* 솔레노이드 밸브 렌더링 */}
        {component.type === 'valve' && (
          <>
            {component.id === 'solenoid1' && (
              <>
                {(() => {
                  const valveState = valveStates[id] || { position: 'B', coilA: false };
                  
                  return (
                    <>
                      <rect x="25" y="40" width="30" height="50" fill="#e0e0e0" stroke="#333" strokeWidth="2" rx="5"/>
                      
                      <rect 
                        x="5" y="10" width="20" height="35" 
                        fill={valveState.coilA ? "#2ecc71" : "#2c3e50"} 
                        stroke="#333" strokeWidth="2" rx="3"
                      />
                      <rect x="8" y="13" width="14" height="8" fill="#34495e" rx="2"/>
                      {valveState.coilA && (
                        <circle cx="15" cy="35" r="2" fill="#00ff00" className="animate-pulse"/>
                      )}
                      
                      <g stroke="#666" strokeWidth="2" fill="none">
                        <path d="M65,45 Q70,40 65,35 Q60,30 65,25 Q70,20 65,15"/>
                      </g>
                      <rect x="60" y="10" width="15" height="35" fill="#f39c12" stroke="#333" strokeWidth="2" rx="3"/>
                      
                      <rect x="30" y="45" width="20" height="20" fill="#fff" stroke="#333" strokeWidth="2"/>
                      <circle cx="35" cy="55" r="2" fill="#333"/>
                      <circle cx="45" cy="55" r="2" fill="#333"/>
                      
                      {valveState.position === 'A' ? (
                        <path d="M33,50 L47,60" stroke="#00ff00" strokeWidth="3"/>
                      ) : (
                        <path d="M33,60 L47,50" stroke="#2196F3" strokeWidth="3"/>
                      )}
                      
                      <rect x="38" y="87" width="4" height="4" fill="#2196F3" stroke="#333" strokeWidth="1"/>
                      <text x="40" y="85" textAnchor="middle" className="text-xs font-bold" fill="#2196F3">P</text>
                      
                      <rect x="23" y="52" width="4" height="4" fill="#2196F3" stroke="#333" strokeWidth="1"/>
                      <text x="25" y="50" textAnchor="middle" className="text-xs font-bold" fill="#2196F3">A</text>
                      
                      <rect x="53" y="52" width="4" height="4" fill="#2196F3" stroke="#333" strokeWidth="1"/>
                      <text x="55" y="50" textAnchor="middle" className="text-xs font-bold" fill="#2196F3">B</text>
                      
                      <circle cx="15" cy="15" r="2" fill="#ff0000" stroke="#333" strokeWidth="1"/>
                      <circle cx="15" cy="30" r="2" fill="#000000" stroke="#333" strokeWidth="1"/>
                    </>
                  );
                })()}
              </>
            )}
            
            {component.id === 'solenoid2' && (
              <>
                {(() => {
                  const valveState = valveStates[id] || { position: 'center', coilA: false, coilB: false };
                  
                  return (
                    <>
                      <rect x="35" y="40" width="30" height="50" fill="#e0e0e0" stroke="#333" strokeWidth="2" rx="5"/>
                      
                      <rect 
                        x="5" y="10" width="20" height="35" 
                        fill={valveState.coilA ? "#2ecc71" : "#2c3e50"} 
                        stroke="#333" strokeWidth="2" rx="3"
                      />
                      <rect x="8" y="13" width="14" height="8" fill="#34495e" rx="2"/>
                      {valveState.coilA && (
                        <circle cx="15" cy="40" r="2" fill="#00ff00" className="animate-pulse"/>
                      )}
                      
                      <rect 
                        x="75" y="10" width="20" height="35" 
                        fill={valveState.coilB ? "#2ecc71" : "#2c3e50"} 
                        stroke="#333" strokeWidth="2" rx="3"
                      />
                      <rect x="78" y="13" width="14" height="8" fill="#34495e" rx="2"/>
                      {valveState.coilB && (
                        <circle cx="85" cy="40" r="2" fill="#00ff00" className="animate-pulse"/>
                      )}
                      
                      <rect x="40" y="45" width="20" height="20" fill="#fff" stroke="#333" strokeWidth="2"/>
                      <circle cx="45" cy="55" r="2" fill="#333"/>
                      <circle cx="55" cy="55" r="2" fill="#333"/>
                      
                      {valveState.position === 'A' ? (
                        <path d="M43,50 L57,60" stroke="#00ff00" strokeWidth="3"/>
                      ) : (
                        <path d="M43,60 L57,50" stroke="#2196F3" strokeWidth="3"/>
                      )}
                      
                      <rect x="48" y="87" width="4" height="4" fill="#2196F3" stroke="#333" strokeWidth="1"/>
                      <text x="50" y="85" textAnchor="middle" className="text-xs font-bold" fill="#2196F3">P</text>
                      
                      <rect x="28" y="52" width="4" height="4" fill="#2196F3" stroke="#333" strokeWidth="1"/>
                      <text x="30" y="50" textAnchor="middle" className="text-xs font-bold" fill="#2196F3">A</text>
                      
                      <rect x="68" y="52" width="4" height="4" fill="#2196F3" stroke="#333" strokeWidth="1"/>
                      <text x="70" y="50" textAnchor="middle" className="text-xs font-bold" fill="#2196F3">B</text>
                      
                      <circle cx="15" cy="15" r="2" fill="#ff0000" stroke="#333" strokeWidth="1"/>
                      <circle cx="15" cy="30" r="2" fill="#000000" stroke="#333" strokeWidth="1"/>
                      
                      <circle cx="85" cy="15" r="2" fill="#ff0000" stroke="#333" strokeWidth="1"/>
                      <circle cx="85" cy="30" r="2" fill="#000000" stroke="#333" strokeWidth="1"/>
                    </>
                  );
                })()}
              </>
            )}
          </>
        )}

        {component.type === 'control' && (
          <>
            {/* 전체 베이스 */}
            <rect x="5" y="5" width="190" height="90" fill="#f0f0f0" stroke="#333" strokeWidth="2" rx="5"/>
            
            {(() => {
              const buttonState = buttonStates[id] || { 
                btn1_pressed: false, btn1_led: false,
                btn2_pressed: false, btn2_led: false,
                btn3_pressed: false, btn3_led: false
              };
              
              return (
                <>
                  {/* 푸시버튼 1 (빨간색) - 모멘터리 동작 */}
                  <g 
                    onMouseDown={(e) => handleButtonMouseDown(e, id, 1)}
                    onMouseUp={(e) => handleButtonMouseUp(e, id, 1)}
                    onMouseLeave={(e) => handleButtonMouseUp(e, id, 1)}
                  >
                    <circle cx="40" cy="20" r="12" 
                      fill={buttonState.btn1_led ? "#ff4444" : "#800000"} 
                      stroke="#333" strokeWidth="2" 
                      className="cursor-pointer hover:opacity-80"
                    />
                    <circle cx="40" cy="20" r={buttonState.btn1_pressed ? 6 : 8}
                      fill={buttonState.btn1_led ? "#ff8888" : "#400000"} 
                      stroke="#333" strokeWidth="1"
                      className="cursor-pointer"
                    />
                    {buttonState.btn1_pressed && (
                      <circle cx="40" cy="20" r="4" fill="#fff" opacity="0.8"/>
                    )}
                  </g>
                  
                  {/* 푸시버튼 2 (노란색) - 모멘터리 동작 */}
                  <g 
                    onMouseDown={(e) => handleButtonMouseDown(e, id, 2)}
                    onMouseUp={(e) => handleButtonMouseUp(e, id, 2)}
                    onMouseLeave={(e) => handleButtonMouseUp(e, id, 2)}
                  >
                    <circle cx="40" cy="50" r="12" 
                      fill={buttonState.btn2_led ? "#ffff44" : "#808000"} 
                      stroke="#333" strokeWidth="2" 
                      className="cursor-pointer hover:opacity-80"
                    />
                    <circle cx="40" cy="50" r={buttonState.btn2_pressed ? 6 : 8}
                      fill={buttonState.btn2_led ? "#ffff88" : "#404000"} 
                      stroke="#333" strokeWidth="1"
                      className="cursor-pointer"
                    />
                    {buttonState.btn2_pressed && (
                      <circle cx="40" cy="50" r="4" fill="#fff" opacity="0.8"/>
                    )}
                  </g>
                  
                  {/* 푸시버튼 3 (초록색) - 모멘터리 동작 */}
                  <g 
                    onMouseDown={(e) => handleButtonMouseDown(e, id, 3)}
                    onMouseUp={(e) => handleButtonMouseUp(e, id, 3)}
                    onMouseLeave={(e) => handleButtonMouseUp(e, id, 3)}
                  >
                    <circle cx="40" cy="80" r="12" 
                      fill={buttonState.btn3_led ? "#44ff44" : "#008000"} 
                      stroke="#333" strokeWidth="2" 
                      className="cursor-pointer hover:opacity-80"
                    />
                    <circle cx="40" cy="80" r={buttonState.btn3_pressed ? 6 : 8}
                      fill={buttonState.btn3_led ? "#88ff88" : "#004000"} 
                      stroke="#333" strokeWidth="1"
                      className="cursor-pointer"
                    />
                    {buttonState.btn3_pressed && (
                      <circle cx="40" cy="80" r="4" fill="#fff" opacity="0.8"/>
                    )}
                  </g>
                </>
              );
            })()}
            
            {/* 포트 구분선 */}
            <line x1="70" y1="10" x2="70" y2="90" stroke="#666" strokeWidth="1" strokeDasharray="2,2"/>
          </>
        )}

        {component.type === 'pneumatic' && (
          <>
            {component.id === 'frl' && (
              <>
                <rect x="10" y="10" width="60" height="80" fill="#e8f4fd" stroke="#333" strokeWidth="2" rx="5"/>
                <circle cx="40" cy="25" r="12" fill="#fff" stroke="#333" strokeWidth="2"/>
                <circle cx="40" cy="25" r="8" fill="none" stroke="#666" strokeWidth="1"/>
                
                {Array.from({length: 5}, (_, i) => {
                  const angle = (i / 4) * 180 - 90;
                  const radians = (angle * Math.PI) / 180;
                  const x1 = 40 + Math.cos(radians) * 6;
                  const y1 = 25 + Math.sin(radians) * 6;
                  const x2 = 40 + Math.cos(radians) * 8;
                  const y2 = 25 + Math.sin(radians) * 8;
                  return (
                    <line key={i} x1={x1} y1={y1} x2={x2} y2={y2} stroke="#333" strokeWidth="1"/>
                  );
                })}
                
                {(() => {
                  const angle = (airPressure / 10) * 180 - 90;
                  const radians = (angle * Math.PI) / 180;
                  const needleX = 40 + Math.cos(radians) * 6;
                  const needleY = 25 + Math.sin(radians) * 6;
                  return (
                    <line x1="40" y1="25" x2={needleX} y2={needleY} stroke="#ff0000" strokeWidth="2"/>
                  );
                })()}
                
                <circle 
                  cx="40" 
                  cy="55" 
                  r="12" 
                  fill={isDraggingPressure ? "#999" : "#ccc"} 
                  stroke="#333" 
                  strokeWidth="2" 
                  className="cursor-pointer hover:fill-gray-400"
                  onMouseEnter={() => {
                    setHoveredComponent('frl_dial');
                    setHoveredFRLInstance(id);
                  }}
                  onMouseLeave={() => {
                    setHoveredComponent(null);
                    setHoveredFRLInstance(null);
                  }}
                  onMouseDown={(e) => handleDialMouseDown(e, id)}
                />
                
                <circle cx="40" cy="55" r="2" fill="#666"/>
                
                {(() => {
                  const dialAngle = (airPressure / 10) * 270 - 135;
                  const radians = (dialAngle * Math.PI) / 180;
                  const lineX = 40 + Math.cos(radians) * 8;
                  const lineY = 55 + Math.sin(radians) * 8;
                  return (
                    <line x1="40" y1="55" x2={lineX} y2={lineY} stroke="#333" strokeWidth="2"/>
                  );
                })()}
                
                {isDraggingPressure && (
                  <g>
                    <rect
                      x="10"
                      y="30"
                      width="60"
                      height="20"
                      fill="#ff9500"
                      rx="5"
                      opacity="0.9"
                    />
                    <text
                      x="40"
                      y="42"
                      textAnchor="middle"
                      className="text-sm font-bold pointer-events-none"
                      fill="#fff"
                    >
                      {airPressure.toFixed(1)} bar
                    </text>
                  </g>
                )}
              </>
            )}
            
            {component.id === 'manifold' && (
              <>
                <rect x="10" y="10" width="100" height="40" fill="#d0d0d0" stroke="#333" strokeWidth="2" rx="5"/>
                <rect x="15" y="15" width="90" height="30" fill="#e8e8e8" stroke="#333" strokeWidth="1" rx="3"/>
                <text x="60" y="35" textAnchor="middle" className="text-sm font-bold" fill="#333">MANIFOLD</text>
              </>
            )}
          </>
        )}

        {component.type === 'power' && (
          <>
            <rect x="5" y="5" width="90" height="70" fill="#2c3e50" stroke="#333" strokeWidth="2" rx="5"/>
            <text x="50" y="40" textAnchor="middle" className="text-sm font-bold" fill="#fff">24V DC</text>
          </>
        )}

        {/* 포트 렌더링 */}
        {allPorts.map((port) => (
          <g key={`${id}-${port.id}`}>
            <circle
              cx={port.x}
              cy={port.y}
              r="4"
              fill={
                port.id === '+24V' || port.id === 'A+24V' || port.id === 'B+24V' || port.id.includes('LED') && port.id.includes('+') ? '#FF0000' : 
                port.id === '0V' || port.id === 'A0V' || port.id === 'B0V' || port.id.includes('LED') && port.id.includes('-') ? '#000000' : 
                getPortColor(port.type)
              }
              stroke="#333"
              strokeWidth="1"
              className="cursor-crosshair hover:stroke-blue-500 hover:stroke-2"
              onMouseDown={(e) => e.stopPropagation()}
              onMouseEnter={() => setHoveredPort(`${id}-${port.id}`)}
              onMouseLeave={() => setHoveredPort(null)}
              onClick={(e) => handlePortClick(e, id, port)}
            />
            {hoveredPort === `${id}-${port.id}` && (
              <g>
                <rect
                  x={port.x - 15}
                  y={port.y - 25}
                  width="30"
                  height="15"
                  fill="#333"
                  rx="2"
                  opacity="0.9"
                />
                <text
                  x={port.x}
                  y={port.y - 15}
                  textAnchor="middle"
                  className="text-xs pointer-events-none"
                  fill="#fff"
                  fontWeight="bold"
                >
                  {port.id === 'A+24V' ? 'A+' : 
                   port.id === 'A0V' ? 'A-' : 
                   port.id === 'B+24V' ? 'B+' : 
                   port.id === 'B0V' ? 'B-' :
                   port.id.includes('LED') && port.id.includes('+') ? 'LED+' :
                   port.id.includes('LED') && port.id.includes('-') ? 'LED-' :
                   port.id.includes('BTN') && port.id.includes('NO') ? 'NO' :
                   port.id.includes('BTN') && port.id.includes('NC') ? 'NC' :
                   port.id.includes('BTN') && !port.id.includes('_') ? 'BTN' :
                   port.id}
                </text>
              </g>
            )}
          </g>
        ))}

        {/* FRL 다이얼 호버 가이드 */}
        {component.id === 'frl' && hoveredComponent === 'frl_dial' && hoveredFRLInstance === id && (
          <g>
            <circle 
              cx="40" 
              cy="55" 
              r="15" 
              fill="none" 
              stroke="#ff9500" 
              strokeWidth="2" 
              strokeDasharray="3,3"
              opacity="0.8"
              className="pointer-events-none"
            />
            
            <rect
              x="90"
              y="25"
              width="140"
              height="65"
              fill="#000"
              rx="10"
              opacity="0.95"
              stroke="#ff9500"
              strokeWidth="2"
            />
            <text
              x="160"
              y="42"
              textAnchor="middle"
              className="text-xs font-bold"
              fill="#fff"
              fontSize="8"
            >
              압력: {airPressure.toFixed(1)} bar
            </text>
            <text
              x="160"
              y="56"
              textAnchor="middle"
              className="text-xs"
              fill="#ffeb3b"
              fontSize="7"
            >
              드래그: 원형으로 회전
            </text>
            <text
              x="160"
              y="70"
              textAnchor="middle"
              className="text-xs"
              fill="#90EE90"
              fontSize="6"
            >
              휠: 미세조절
            </text>

          </g>
        )}

        {/* 컴포넌트 삭제 버튼 */}
        {isSelected && (
          <g 
            className="cursor-pointer"
            onClick={(e) => {
              e.stopPropagation();
              deleteComponent(id);
            }}
          >
            <circle
              cx={component.width - 10}
              cy={10}
              r="8"
              fill="#ff4444"
              stroke="#fff"
              strokeWidth="2"
            />
            <path
              d="M-3,-3 L3,3 M3,-3 L-3,3"
              transform={`translate(${component.width - 10}, 10)`}
              stroke="#fff"
              strokeWidth="2"
            />
          </g>
        )}
      </g>
    );
  };

  return (
    <div className="w-full h-screen bg-white flex flex-col">
      {/* 헤더 */}
      <div className="flex justify-between items-center p-4 border-b">
        <h1 className="text-3xl font-bold text-gray-800">FX3U PLC 시뮬레이터 - 실배선 모드</h1>
        
        <button 
          onClick={() => setPlcRunning(!plcRunning)}
          className={`flex items-center gap-2 px-4 py-2 border border-gray-300 rounded-full hover:bg-gray-50 ${
            plcRunning ? 'bg-green-100 text-green-800' : 'bg-white'
          }`}
        >
          <Play size={16} fill="currentColor" />
          <span className="text-sm font-medium">
            {plcRunning ? 'PLC Running' : 'PLC Start'}
          </span>
        </button>
      </div>

      <div className="flex flex-1">
        {/* 왼쪽: 배선 작업 공간 */}
        <div className="flex-1 border-r">
          {/* 툴바 */}
          <div className="flex gap-2 p-4 border-b bg-gray-50 flex-wrap">
            <button
              onClick={() => {
                setSelectedTool('select'); 
                setIsConnecting(false); 
                setConnectionStart(null);
                setWireControlPoints([]);
              }}
              className={`flex items-center gap-2 px-3 py-2 rounded-lg text-sm font-bold ${
                selectedTool === 'select' 
                  ? 'bg-blue-200 text-blue-800' 
                  : 'border border-gray-300 hover:bg-gray-100'
              }`}
            >
              <MousePointer size={16} />
              선택도구
            </button>
            <button
              onClick={() => {
                setSelectedTool('pneumatic'); 
                setIsConnecting(false); 
                setConnectionStart(null);
                setSelectedComponent(null);
                setWireControlPoints([]);
              }}
              className={`flex items-center gap-2 px-3 py-2 rounded-lg text-sm font-medium ${
                selectedTool === 'pneumatic' 
                  ? 'bg-blue-200 text-blue-800' 
                  : 'border border-gray-300 hover:bg-gray-100'
              }`}
            >
              <span className="text-xs">⭕</span>
              공압 배선 도구
            </button>
            <button
              onClick={() => {
                setSelectedTool('electric'); 
                setIsConnecting(false); 
                setConnectionStart(null);
                setSelectedComponent(null);
                setWireControlPoints([]);
              }}
              className={`flex items-center gap-2 px-3 py-2 rounded-lg text-sm font-medium ${
                selectedTool === 'electric' 
                  ? 'bg-blue-200 text-blue-800' 
                  : 'border border-gray-300 hover:bg-gray-100'
              }`}
            >
              <Zap size={16} />
              전기 배선 도구
            </button>
            
            {/* 상태 표시 */}
            {isConnecting && (
              <div className="flex items-center gap-2 px-3 py-2 bg-yellow-100 text-yellow-800 rounded-lg">
                <span className="text-sm">
                  {wireControlPoints.length === 0 
                    ? "배선 그리는 중... (포트 클릭: 연결 완료)"
                    : `중간점 ${wireControlPoints.length}개 추가됨`
                  }
                </span>
                <button 
                  onClick={() => {
                    setIsConnecting(false); 
                    setConnectionStart(null);
                    setWireControlPoints([]);
                  }}
                  className="text-yellow-600 hover:text-yellow-800"
                >
                  취소
                </button>
              </div>
            )}
          </div>
          
          {/* 배선 캔버스 */}
          <div className="h-full bg-gray-50 p-4">
            <svg
              ref={canvasRef}
              className="w-full h-full border-2 border-dashed border-gray-300 rounded-lg bg-white"
              onDragOver={(e) => e.preventDefault()}
              onDrop={handleDrop}
              onMouseMove={handleMouseMove}
              onMouseUp={handleMouseUp}
              onMouseLeave={handleMouseLeave}
              onClick={handleCanvasClick}
              onDoubleClick={handleCanvasDoubleClick}
              onWheel={handleWheel}
            >
              <defs>
                <pattern id="grid" width="20" height="20" patternUnits="userSpaceOnUse">
                  <path d="M 20 0 L 0 0 0 20" fill="none" stroke="#e0e0e0" strokeWidth="1"/>
                </pattern>
              </defs>
              <rect width="100%" height="100%" fill="url(#grid)" />
              
              {renderWires()}
              
              {placedComponents.map((component) =>
                renderComponent(component, component.x, component.y, component.instanceId)
              )}
            </svg>
          </div>
        </div>

        {/* 오른쪽: PLC 부품 목록 */}
        <div className="w-80 bg-gray-50 overflow-y-auto">
          <div className="p-4">
            <h3 className="text-lg font-bold mb-4">실배선 오브젝트</h3>
            <div className="space-y-3">
              {plcComponents.map((component) => (
                <div 
                  key={component.id}
                  className="bg-white rounded-lg p-3 border border-gray-200 hover:shadow-md cursor-grab active:cursor-grabbing transition-shadow"
                  draggable
                  onDragStart={(e) => handleDragStart(e, component)}
                >
                  <h4 className="font-bold text-sm mb-1">{component.name}</h4>
                  <p className="text-xs text-gray-600">{component.desc}</p>
                </div>
              ))}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default PLCSimulator;