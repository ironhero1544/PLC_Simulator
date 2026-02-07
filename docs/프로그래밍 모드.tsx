import React, { useState, useRef, useCallback, useEffect } from 'react';
import { Play, Pause, Upload, Save, FolderOpen, Settings, Monitor, Edit3, AlertCircle, CheckCircle, Info, X, Plus, Trash2 } from 'lucide-react';

const PLCSimulator = () => {
  // 프로그래밍 모드 상태
  const [currentMode, setCurrentMode] = useState('edit'); // 'edit', 'monitor'
  const [isRunning, setIsRunning] = useState(false);
  const [isOnline, setIsOnline] = useState(false);
  
  // 커서 및 편집 상태
  const [cursorPosition, setCursorPosition] = useState({ rung: 0, x: 0 });
  const [showAddressDialog, setShowAddressDialog] = useState(false);
  const [showVerticalDialog, setShowVerticalDialog] = useState(false);
  const [verticalConnectionData, setVerticalConnectionData] = useState({ startRung: 0, lineCount: 1, x: 0 });
  const [tempAddress, setTempAddress] = useState('');
  const [tempInstructionType, setTempInstructionType] = useState('');
  const [tempPreset, setTempPreset] = useState('');
  const [showHelp, setShowHelp] = useState(false);
  
  // 실제 GX Works 2 스타일 래더 프로그램 구조 - 각 룽은 1줄만
  const [ladderProgram, setLadderProgram] = useState({
    rungs: [
      {
        id: 'rung_0',
        number: 0,
        comment: '',
        cells: Array(12).fill(null), // 각 룽은 1줄 12칸
      },
      {
        id: 'end',
        number: 'END',
        isEnd: true,
        cells: Array(12).fill(null),
      }
    ],
    // 룽간 세로선 연결 정보
    verticalConnections: [] // { x: 0, rungs: [0, 3, 7] } 형태
  });
  
  // 실시간 디바이스 상태 - X0~X15, Y0~Y15로 확장
  const [deviceStates, setDeviceStates] = useState(() => {
    const initialStates = {};
    
    // X0~X15 입력
    for (let i = 0; i <= 15; i++) {
      initialStates[`X${i}`] = false;
    }
    
    // Y0~Y15 출력
    for (let i = 0; i <= 15; i++) {
      initialStates[`Y${i}`] = false;
    }
    
    return initialStates;
  });
  
  // 전력 흐름 상태
  const [powerFlowStates, setPowerFlowStates] = useState({});
  
  // 컴파일 상태
  const [compileStatus, setCompileStatus] = useState({
    hasErrors: false,
    errors: [],
    warnings: [],
    lastCompiled: null,
    isCompiling: false
  });

  // 래더 프로그램에서 사용된 디바이스 추출
  const getUsedDevices = useCallback(() => {
    const usedDevices = { M: new Set(), T: new Set(), C: new Set() };
    
    ladderProgram.rungs.forEach(rung => {
      if (rung.isEnd) return;
      
      rung.cells.forEach(cell => {
        if (cell && cell.address) {
          const address = cell.address;
          if (address.startsWith('M')) {
            usedDevices.M.add(address);
          } else if (address.startsWith('T')) {
            usedDevices.T.add(address);
          } else if (address.startsWith('C')) {
            usedDevices.C.add(address);
          }
        }
      });
    });
    
    return {
      M: Array.from(usedDevices.M).sort((a, b) => {
        const numA = parseInt(a.substring(1));
        const numB = parseInt(b.substring(1));
        return numA - numB;
      }),
      T: Array.from(usedDevices.T).sort((a, b) => {
        const numA = parseInt(a.substring(1));
        const numB = parseInt(b.substring(1));
        return numA - numB;
      }),
      C: Array.from(usedDevices.C).sort((a, b) => {
        const numA = parseInt(a.substring(1));
        const numB = parseInt(b.substring(1));
        return numA - numB;
      })
    };
  }, [ladderProgram]);

  // 새 디바이스가 추가될 때 deviceStates 업데이트
  useEffect(() => {
    const usedDevices = getUsedDevices();
    
    setDeviceStates(prev => {
      const newStates = { ...prev };
      
      // M 디바이스 추가
      usedDevices.M.forEach(address => {
        if (!(address in newStates)) {
          newStates[address] = false;
        }
      });
      
      // T 디바이스 추가
      usedDevices.T.forEach(address => {
        if (!(address in newStates)) {
          // 래더 프로그램에서 해당 디바이스의 preset 값 찾기
          let presetValue = 50; // 기본값
          ladderProgram.rungs.forEach(rung => {
            rung.cells.forEach(cell => {
              if (cell && cell.address === address && cell.preset) {
                presetValue = parseInt(cell.preset.replace('K', '') || '50');
              }
            });
          });
          newStates[address] = { value: 0, done: false, preset: presetValue, enabled: false };
        }
      });
      
      // C 디바이스 추가
      usedDevices.C.forEach(address => {
        if (!(address in newStates)) {
          // 래더 프로그램에서 해당 디바이스의 preset 값 찾기
          let presetValue = 10; // 기본값
          ladderProgram.rungs.forEach(rung => {
            rung.cells.forEach(cell => {
              if (cell && cell.address === address && cell.preset) {
                presetValue = parseInt(cell.preset.replace('K', '') || '10');
              }
            });
          });
          newStates[address] = { value: 0, done: false, preset: presetValue, enabled: false, lastPower: false };
        }
      });
      
      return newStates;
    });
  }, [getUsedDevices, ladderProgram]);

  // 다이얼로그 포커스 관리
  useEffect(() => {
    if (showAddressDialog) {
      // 다이얼로그가 열릴 때 즉시 포커스 설정
      const focusDialog = () => {
        const dialog = document.querySelector('.address-dialog');
        if (dialog) {
          dialog.focus();
          // 입력 필드에도 포커스
          const input = dialog.querySelector('input');
          if (input) {
            input.focus();
          }
        }
      };
      
      // 다음 프레임에서 포커스 설정
      requestAnimationFrame(focusDialog);
    }
  }, [showAddressDialog]);

  useEffect(() => {
    if (showVerticalDialog) {
      // 세로선 다이얼로그가 열릴 때 즉시 포커스 설정
      const focusDialog = () => {
        const dialog = document.querySelector('.vertical-dialog');
        if (dialog) {
          dialog.focus();
          // 입력 필드에도 포커스
          const input = dialog.querySelector('input');
          if (input) {
            input.focus();
          }
        }
      };
      
      // 다음 프레임에서 포커스 설정
      requestAnimationFrame(focusDialog);
    }
  }, [showVerticalDialog]);

  const MAX_X = 11; // 0~11 (12칸)

  // 명령어 타입 정의
  const getInstructionSymbol = (type) => {
    switch (type) {
      case 'contact_no': return '─┤ ├─';
      case 'contact_nc': return '─┤/├─';
      case 'coil': return '─( )─';
      case 'coil_set': return '─(S)─';
      case 'coil_reset': return '─(R)─';
      case 'timer': return '[TON]';
      case 'counter': return '[CTU]';
      case 'reset': return '[RST]';
      case 'h_line': return '━━━━━';
      default: return '─??─';
    }
  };

  // 룽에 명령어 추가
  const addInstructionToRung = useCallback((rungIndex, x, instruction) => {
    setLadderProgram(prev => ({
      ...prev,
      rungs: prev.rungs.map((rung, idx) => {
        if (idx !== rungIndex || rung.isEnd) return rung;
        
        const newCells = [...rung.cells];
        
        // 출력 명령어는 자동으로 맨 오른쪽으로
        if (['coil', 'coil_set', 'coil_reset', 'timer', 'counter', 'reset'].includes(instruction.type)) {
          x = MAX_X;
        }
        
        newCells[x] = instruction;
        
        return {
          ...rung,
          cells: newCells
        };
      })
    }));
  }, []);

  // 가로선 자동 생성
  const generateAutoHorizontalLines = useCallback((rungIndex) => {
    setLadderProgram(prev => ({
      ...prev,
      rungs: prev.rungs.map((rung, idx) => {
        if (idx !== rungIndex || rung.isEnd) return rung;
        
        const newCells = [...rung.cells];
        const instructions = [];
        
        // 실제 명령어들만 찾기 (가로선 제외)
        for (let x = 0; x < newCells.length; x++) {
          const cell = newCells[x];
          if (cell && !cell.auto && cell.type !== 'h_line') {
            instructions.push({ ...cell, x });
          }
        }
        
        // 출력이 있으면 가로선 생성
        const hasOutput = instructions.some(inst => 
          ['coil', 'coil_set', 'coil_reset', 'timer', 'counter', 'reset'].includes(inst.type) && inst.x === MAX_X
        );
        
        if (hasOutput && instructions.length > 0) {
          // 기존 auto 가로선 제거
          for (let x = 0; x < newCells.length; x++) {
            if (newCells[x] && newCells[x].auto && newCells[x].type === 'h_line') {
              newCells[x] = null;
            }
          }
          
          // 첫 번째 명령어부터 출력까지 가로선 생성
          const firstX = Math.min(...instructions.map(inst => inst.x));
          for (let x = firstX + 1; x <= MAX_X; x++) {
            if (!newCells[x]) {
              newCells[x] = {
                id: `auto_h_${rungIndex}_${x}`,
                type: 'h_line',
                address: '',
                auto: true
              };
            }
          }
        }
        
        return {
          ...rung,
          cells: newCells
        };
      })
    }));
  }, []);

  // 키보드 단축키 처리
  useEffect(() => {
    const handleKeyDown = (e) => {
      if (showAddressDialog || showVerticalDialog) return;
      
      // 전역 단축키
      if (e.key === 'F2') {
        e.preventDefault();
        setCurrentMode('monitor');
        return;
      }
      if (e.key === 'F3') {
        e.preventDefault();
        setCurrentMode('edit');
        return;
      }
      if (e.key === 'F1') {
        e.preventDefault();
        setShowHelp(!showHelp);
        return;
      }
      
      if (currentMode !== 'edit') return;
      
      // 커서 이동
      if (e.key === 'ArrowUp') {
        e.preventDefault();
        moveCursor('up');
      } else if (e.key === 'ArrowDown') {
        e.preventDefault();
        moveCursor('down');
      } else if (e.key === 'ArrowLeft') {
        e.preventDefault();
        moveCursor('left');
      } else if (e.key === 'ArrowRight') {
        e.preventDefault();
        moveCursor('right');
      }
      
      // 명령어 삽입
      else if (e.key === 'F5') {
        e.preventDefault();
        insertInstruction('contact_no');
      } else if (e.key === 'F6') {
        e.preventDefault();
        insertInstruction('contact_nc');
      } else if (e.key === 'F7') {
        e.preventDefault();
        insertInstruction('coil');
      } else if (e.key === 'F9') {
        e.preventDefault();
        if (e.shiftKey) {
          addVerticalConnection(); // 세로선 연결 (룽간 연결)
        } else {
          insertInstruction('h_line');
        }
      }
      
      // 기타
      else if (e.key === 'Delete') {
        e.preventDefault();
        deleteInstructionAtCursor();
      } else if (e.key === 'Enter') {
        e.preventDefault();
        editInstructionAtCursor();
      } else if (e.key === 'Insert') {
        e.preventDefault();
        if (e.shiftKey) {
          addNewRung();
        }
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [currentMode, cursorPosition, showAddressDialog, showVerticalDialog, showHelp]);

  // 커서 이동 함수
  const moveCursor = useCallback((direction) => {
    setCursorPosition(prev => {
      const maxRung = ladderProgram.rungs.length - 1;
      
      switch (direction) {
        case 'up':
          return { ...prev, rung: Math.max(0, prev.rung - 1) };
        case 'down':
          return { ...prev, rung: Math.min(maxRung, prev.rung + 1) };
        case 'left':
          return { ...prev, x: Math.max(0, prev.x - 1) };
        case 'right':
          return { ...prev, x: Math.min(MAX_X, prev.x + 1) };
        default:
          return prev;
      }
    });
  }, [ladderProgram.rungs.length]);

  // 새 룽 추가
  const addNewRung = useCallback(() => {
    const newRung = {
      id: `rung_${Date.now()}`,
      number: ladderProgram.rungs.length - 1, // END 앞에 삽입
      comment: '',
      cells: Array(12).fill(null), // 1줄 12칸
    };
    
    setLadderProgram(prev => ({
      ...prev,
      rungs: [
        ...prev.rungs.slice(0, -1), // END 제외
        newRung,
        ...prev.rungs.slice(-1).map(r => ({ ...r, number: r.isEnd ? 'END' : r.number + 1 }))
      ]
    }));
    
    setCursorPosition({ rung: ladderProgram.rungs.length - 1, x: 0 });
  }, [ladderProgram.rungs]);

  // 명령어 삽입
  const insertInstruction = useCallback((type) => {
    // END 룽에서 명령어 추가 시 새 룽 생성
    const currentRung = ladderProgram.rungs[cursorPosition.rung];
    if (currentRung && currentRung.isEnd) {
      // 새 룽 생성
      const newRung = {
        id: `rung_${Date.now()}`,
        number: ladderProgram.rungs.length - 1, // END 앞에 삽입
        comment: '',
        cells: Array(12).fill(null), // 1줄 12칸
      };
      
      setLadderProgram(prev => ({
        ...prev,
        rungs: [
          ...prev.rungs.slice(0, -1), // END 제외
          newRung,
          ...prev.rungs.slice(-1) // END 추가
        ]
      }));
      
      // 커서를 새 룽으로 이동
      const newRungIndex = ladderProgram.rungs.length - 1;
      setCursorPosition({ rung: newRungIndex, x: 0 });
      
      // 가로선 생성
      setTimeout(() => {
        generateAutoHorizontalLines(newRungIndex);
      }, 10);
      
      // 다음 프레임에서 명령어 삽입 (상태 업데이트 완료 후)
      setTimeout(() => {
        setTempInstructionType(type);
        setTempAddress('');
        setTempPreset('');
        
        if (['h_line'].includes(type)) {
          const newInstruction = {
            id: `inst_${Date.now()}`,
            type: type,
            address: '',
            comment: ''
          };
          
          setLadderProgram(prev => ({
            ...prev,
            rungs: prev.rungs.map((rung, idx) => {
              if (idx !== newRungIndex || rung.isEnd) return rung;
              
              const newCells = [...rung.cells];
              newCells[0] = newInstruction;
              
              return {
                ...rung,
                cells: newCells
              };
            })
          }));
          
          setCursorPosition(prev => ({ 
            ...prev, 
            x: Math.min(MAX_X, prev.x + 1) 
          }));
          
          // 가로선 자동 생성
          setTimeout(() => {
            generateAutoHorizontalLines(newRungIndex);
          }, 20);
        } else {
          setShowAddressDialog(true);
        }
      }, 10);
      
      return;
    }
    
    setTempInstructionType(type);
    setTempAddress('');
    setTempPreset('');
    
    if (['h_line'].includes(type)) {
      const newInstruction = {
        id: `inst_${Date.now()}`,
        type: type,
        address: '',
        comment: ''
      };
      
      addInstructionToRung(cursorPosition.rung, cursorPosition.x, newInstruction);
      generateAutoHorizontalLines(cursorPosition.rung);
      
      setCursorPosition(prev => ({ 
        ...prev, 
        x: Math.min(MAX_X, prev.x + 1) 
      }));
    } else {
      setShowAddressDialog(true);
    }
  }, [cursorPosition, ladderProgram.rungs]);

  // 세로선 연결 추가 (커서 칸과 왼쪽 칸 사이)
  const addVerticalConnection = useCallback(() => {
    const x = cursorPosition.x;
    const currentRung = cursorPosition.rung;
    
    if (x === 0) {
      alert('첫 번째 칸에서는 세로선을 그을 수 없습니다.');
      return;
    }
    
    if (ladderProgram.rungs[currentRung]?.isEnd) {
      alert('END 룽에서는 세로선을 그을 수 없습니다.');
      return;
    }
    
    // 세로선 다이얼로그 열기 - 현재 커서 위치를 시작점으로 설정
    setVerticalConnectionData({
      startRung: currentRung,
      lineCount: 1,
      x: x // 이것은 실제로는 x-1과 x 사이를 의미
    });
    setShowVerticalDialog(true);
  }, [cursorPosition, ladderProgram.rungs]);

  // 세로선 연결 확정
  const confirmVerticalConnection = useCallback(() => {
    const { startRung, lineCount, x } = verticalConnectionData;
    const endRung = startRung + lineCount; // 현재 줄에서 아래로 lineCount만큼
    
    if (lineCount < 1) {
      alert('연결할 줄 수는 1 이상이어야 합니다.');
      return;
    }
    
    const maxRung = ladderProgram.rungs.length - 2; // END 제외
    if (startRung < 0 || endRung > maxRung) {
      alert(`룽 번호는 0부터 ${maxRung}까지 입력 가능합니다. (시작: ${startRung}, 끝: ${endRung})`);
      return;
    }
    
    setLadderProgram(prev => {
      const newVerticalConnections = [...prev.verticalConnections];
      
      // 새로운 세로선 연결 추가 (중복 허용)
      const rungs = [];
      for (let i = startRung; i <= endRung; i++) {
        rungs.push(i);
      }
      
      newVerticalConnections.push({
        x: x, // x-1과 x 칸 사이를 의미
        rungs: rungs
      });
      
      return {
        ...prev,
        verticalConnections: newVerticalConnections
      };
    });
    
    setShowVerticalDialog(false);
  }, [verticalConnectionData, ladderProgram.rungs.length]);

  const confirmAddInstruction = useCallback(() => {
    if (!tempAddress.trim()) return;
    
    // 입력된 명령어 파싱
    const input = tempAddress.trim().toUpperCase();
    let instructionType = tempInstructionType;
    let deviceAddress = input;
    let presetValue = tempPreset;
    
    // F7 출력에서 특수 명령어 자동 인식
    if (tempInstructionType === 'coil') {
      // 타이머 패턴: T숫자 K숫자
      const timerMatch = input.match(/^T(\d+)\s+K(\d+)$/);
      if (timerMatch) {
        instructionType = 'timer';
        deviceAddress = `T${timerMatch[1]}`;
        presetValue = `K${timerMatch[2]}`;
      }
      
      // 카운터 패턴: C숫자 K숫자
      const counterMatch = input.match(/^C(\d+)\s+K(\d+)$/);
      if (counterMatch) {
        instructionType = 'counter';
        deviceAddress = `C${counterMatch[1]}`;
        presetValue = `K${counterMatch[2]}`;
      }
      
      // 리셋 패턴: RST 주소
      const resetMatch = input.match(/^RST\s+([TC]\d+)$/);
      if (resetMatch) {
        instructionType = 'reset';
        deviceAddress = resetMatch[1];
        presetValue = undefined;
      }
    }
    
    const newInstruction = {
      id: `inst_${Date.now()}`,
      type: instructionType,
      address: deviceAddress,
      preset: ['timer', 'counter'].includes(instructionType) ? presetValue || 'K10' : undefined,
      comment: '',
      originalInput: tempInstructionType === 'coil' ? input : undefined // 디버깅용
    };
    
    // END 룽에서 명령어 추가 시 새 룽 생성
    const currentRung = ladderProgram.rungs[cursorPosition.rung];
    if (currentRung && currentRung.isEnd) {
      // 새 룽 생성
      const newRung = {
        id: `rung_${Date.now()}`,
        number: ladderProgram.rungs.length - 1, // END 앞에 삽입
        comment: '',
        cells: Array(12).fill(null), // 1줄 12칸
      };
      
      setLadderProgram(prev => {
        const updatedRungs = [
          ...prev.rungs.slice(0, -1), // END 제외
          newRung,
          ...prev.rungs.slice(-1) // END 추가
        ];
        
        // 새 룽에 명령어 추가
        const newRungIndex = updatedRungs.length - 2; // END 전 마지막 룽
        let targetX = 0;
        
        // 출력 명령어는 자동으로 맨 오른쪽으로
        if (['coil', 'coil_set', 'coil_reset', 'timer', 'counter', 'reset'].includes(newInstruction.type)) {
          targetX = MAX_X;
        }
        
        updatedRungs[newRungIndex].cells[targetX] = newInstruction;
        
        return {
          ...prev,
          rungs: updatedRungs
        };
      });
      
      // 커서를 새 룽으로 이동
      const newRungIndex = ladderProgram.rungs.length - 1;
      setCursorPosition({ rung: newRungIndex, x: 0 });
    } else {
      // 일반적인 경우
      addInstructionToRung(cursorPosition.rung, cursorPosition.x, newInstruction);
      generateAutoHorizontalLines(cursorPosition.rung);
      
      const isOutput = ['coil', 'coil_set', 'coil_reset', 'timer', 'counter', 'reset'].includes(instructionType);
      if (!isOutput) {
        setCursorPosition(prev => ({ 
          ...prev, 
          x: Math.min(MAX_X, prev.x + 1) 
        }));
      }
    }
    
    setShowAddressDialog(false);
    setTempInstructionType('');
  }, [tempAddress, tempPreset, tempInstructionType, cursorPosition, ladderProgram.rungs]);

  // 커서 위치의 명령어 삭제
  const deleteInstructionAtCursor = useCallback(() => {
    const rung = ladderProgram.rungs[cursorPosition.rung];
    if (!rung || rung.isEnd) return;
    
    setLadderProgram(prev => ({
      ...prev,
      rungs: prev.rungs.map((r, idx) => {
        if (idx !== cursorPosition.rung) return r;
        
        const newCells = [...r.cells];
        newCells[cursorPosition.x] = null;
        
        return {
          ...r,
          cells: newCells
        };
      })
    }));
    
    generateAutoHorizontalLines(cursorPosition.rung);
  }, [cursorPosition, ladderProgram.rungs]);

  // 커서 위치의 명령어 편집
  const editInstructionAtCursor = useCallback(() => {
    const rung = ladderProgram.rungs[cursorPosition.rung];
    if (!rung || rung.isEnd) return;
    
    const instruction = rung.cells[cursorPosition.x];
    
    if (instruction && !['h_line'].includes(instruction.type) && !instruction.auto) {
      setTempAddress(instruction.address);
      setTempInstructionType(instruction.type);
      setTempPreset(instruction.preset || '');
      setShowAddressDialog(true);
    }
  }, [cursorPosition, ladderProgram.rungs]);

  // 세로선 렌더링 (보이지 않는 세로선 전용 그리드 사용)
  const renderVerticalConnections = () => {
    const isMonitoring = currentMode === 'monitor';
    
    return ladderProgram.verticalConnections.map((connection, index) => {
      if (connection.rungs.length < 1) return null;
      
      const sortedRungs = [...connection.rungs].sort((a, b) => a - b);
      const startRung = sortedRungs[0];
      const endRung = sortedRungs[sortedRungs.length - 1];
      
      // 전력 흐름 확인 (x-1 칸의 전력을 x 칸으로 전달)
      let hasAnyPower = false;
      if (isMonitoring) {
        connection.rungs.forEach(rungIndex => {
          const powerFlow = powerFlowStates[rungIndex];
          if (powerFlow && powerFlow[connection.x - 1]) { // 왼쪽 칸의 전력 확인
            hasAnyPower = true;
          }
        });
      }
      
      return (
        <div
          key={`vconn_${index}`}
          className="absolute pointer-events-none"
          style={{
            // connection.x번째 그리드 칸의 정확한 시작점 (x-1과 x 사이)
            left: `${(connection.x / 12) * 100}%`,
            top: `${startRung * 40 + 20}px`, // 룽 중앙에 맞춤
            width: '2px',
            height: `${(endRung - startRung) * 40 + 1}px`, // 정확한 높이 계산
            backgroundColor: hasAnyPower ? '#6B7280' : '#9CA3AF',
            zIndex: 30
          }}
        />
      );
    });
  };
  
  const calculatePowerFlow = useCallback((states) => {
    const powerFlows = {};
    
    // 각 룽별로 전력 흐름 계산
    ladderProgram.rungs.forEach((rung, rungIndex) => {
      if (rung.isEnd) return;
      
      let currentPower = true; // 좌측 레일에서 시작
      const rungFlow = {};
      
      for (let x = 0; x <= MAX_X; x++) {
        const cell = rung.cells[x];
        
        if (cell) {
          switch (cell.type) {
            case 'contact_no':
              // 타이머/카운터 접점은 done 상태 확인
              if (cell.address.startsWith('T') || cell.address.startsWith('C')) {
                const deviceState = states[cell.address];
                currentPower = currentPower && (deviceState?.done || false);
              } else {
                currentPower = currentPower && (states[cell.address] || false);
              }
              break;
            case 'contact_nc':
              // 타이머/카운터 접점은 done 상태 확인
              if (cell.address.startsWith('T') || cell.address.startsWith('C')) {
                const deviceState = states[cell.address];
                currentPower = currentPower && !(deviceState?.done || false);
              } else {
                currentPower = currentPower && !(states[cell.address] || false);
              }
              break;
            case 'coil':
              if (currentPower) {
                states[cell.address] = true;
              }
              break;
            case 'timer':
              const timerState = states[cell.address];
              if (timerState && currentPower && !timerState.enabled) {
                timerState.enabled = true;
                timerState.value = 0;
              } else if (timerState && !currentPower) {
                timerState.enabled = false;
                timerState.value = 0;
                timerState.done = false;
              }
              if (timerState && timerState.enabled) {
                const presetValue = parseInt(cell.preset?.replace('K', '') || '10');
                if (timerState.value < presetValue) {
                  timerState.value++;
                } else {
                  timerState.done = true;
                }
              }
              break;
            case 'counter':
              const counterState = states[cell.address];
              if (counterState && currentPower && !counterState.lastPower) {
                // 상승 엣지에서만 카운트
                counterState.value++;
                const presetValue = parseInt(cell.preset?.replace('K', '') || '10');
                if (counterState.value >= presetValue) {
                  counterState.done = true;
                }
              }
              if (counterState) {
                counterState.lastPower = currentPower;
              }
              break;
            case 'reset':
              if (currentPower) {
                // 리셋 대상 디바이스 찾기
                const targetState = states[cell.address];
                if (targetState && typeof targetState === 'object') {
                  // 타이머/카운터 리셋
                  targetState.value = 0;
                  targetState.done = false;
                  targetState.enabled = false;
                }
              }
              break;
            case 'h_line':
              // 가로선은 전력을 그대로 통과
              break;
          }
        }
        
        rungFlow[x] = currentPower;
      }
      
      powerFlows[rungIndex] = rungFlow;
    });
    
    return powerFlows;
  }, [ladderProgram]);

  // 실시간 시뮬레이션 엔진
  useEffect(() => {
    if (!isRunning) return;
    
    const interval = setInterval(() => {
      setDeviceStates(prev => {
        const newStates = { ...prev };
        const newPowerFlows = calculatePowerFlow(newStates);
        setPowerFlowStates(newPowerFlows);
        return newStates;
      });
    }, 100);
    
    return () => clearInterval(interval);
  }, [isRunning, calculatePowerFlow]);

  // 명령어 렌더링
  const renderCell = (cell, x, rungIndex, isMonitoring = false) => {
    const powerFlowData = powerFlowStates[rungIndex] || {};
    const isActive = isMonitoring && powerFlowData[x];
    
    const isCursorHere = currentMode === 'edit' && 
      cursorPosition.rung === rungIndex && 
      cursorPosition.x === x;
    
    if (!cell) {
      // 빈 셀
      return (
        <div 
          key={x}
          className={`h-full flex items-center justify-center cursor-pointer relative
            ${isCursorHere ? 'bg-yellow-200 border-2 border-yellow-500' : 'bg-gray-50'}
            border-r border-gray-200`}
          onClick={() => {
            if (currentMode === 'edit') {
              setCursorPosition({ rung: rungIndex, x });
            }
          }}
        >
          <div className="w-full h-0.5 bg-gray-300"></div>
        </div>
      );
    }
    
    // 디바이스 상태 값 가져오기
    const getDeviceValue = (address) => {
      const state = deviceStates[address];
      if (typeof state === 'object') {
        // 타이머/카운터 - done 상태 표시 (접점에서 사용되는 상태)
        return state.done ? 'ON' : 'OFF';
      }
      return state ? 'ON' : 'OFF';
    };
    
    // 명령어가 있는 셀
    return (
      <div 
        key={x}
        className={`h-full flex flex-col items-center justify-center text-xs font-mono relative cursor-pointer
          ${isCursorHere ? 'bg-yellow-200 border-2 border-yellow-500' : ''}
          ${cell.auto ? 'bg-gray-50' : 'bg-white'}
          ${isActive ? 'bg-gray-200' : ''}
          border-r border-gray-200`}
        onClick={() => {
          if (currentMode === 'edit') {
            setCursorPosition({ rung: rungIndex, x });
          }
        }}
      >
        {cell.type === 'h_line' ? (
          <div className="h-full w-full flex items-center justify-center">
            <div className={`w-full h-1 ${isActive ? 'bg-gray-600' : 'bg-gray-400'}`}></div>
          </div>
        ) : (
          <>
            <div className={`text-blue-800 font-bold ${isActive ? 'text-gray-700' : ''}`}>
              {getInstructionSymbol(cell.type)}
            </div>
            <div className="text-xs">{cell.address}</div>
            {['timer', 'counter'].includes(cell.type) && cell.preset && (
              <div className="text-xs text-gray-600">{cell.preset}</div>
            )}
            {cell.type === 'reset' && (
              <div className="text-xs text-gray-600">RST</div>
            )}
            
            {/* GX Works2 스타일 디바이스 상태 모니터 박스 - 모니터 모드에서만 표시 */}
            {currentMode === 'monitor' && cell.address && cell.type !== 'h_line' && (
              <div 
                className={`absolute -top-1 -right-1 text-xs px-1 rounded shadow-sm border text-white font-bold z-10 ${
                  isActive ? 'bg-gray-700 border-gray-800' : 'bg-gray-500 border-gray-600'
                }`}
                style={{ 
                  fontSize: '8px', 
                  lineHeight: '1.1',
                  minWidth: '20px',
                  textAlign: 'center'
                }}
              >
                <div style={{ fontSize: '7px' }}>{cell.address}</div>
                <div style={{ fontSize: '7px' }}>{getDeviceValue(cell.address)}</div>
              </div>
            )}
          </>
        )}
      </div>
    );
  };

  // 래더 룽 렌더링
  const renderRung = (rung, index) => {
    const isMonitoring = currentMode === 'monitor';
    
    return (
      <div 
        key={rung.id}
        className="relative flex bg-white border-b border-gray-400 hover:bg-gray-50 transition-colors"
      >
        {/* 룽 번호 */}
        <div className={`w-16 flex items-center justify-center border-r-2 border-gray-400 text-sm font-mono h-10
          ${rung.isEnd ? 'bg-gray-300' : 'bg-gray-100'} text-gray-700`}>
          {rung.isEnd ? 'END' : String(rung.number).padStart(4, '0')}
        </div>
        
        {/* 래더 회로 */}
        <div className="flex-1 flex relative">
          {/* 왼쪽 전원 레일 */}
          <div className="w-4 flex items-center justify-center border-r border-gray-300 bg-gray-50 h-10">
            <div className="w-full h-1 bg-black"></div>
          </div>
          
          {/* 그리드 */}
          <div className="flex-1 relative" style={{ 
            display: 'grid', 
            gridTemplateColumns: 'repeat(12, 1fr)',
            height: '40px' // 고정 높이로 변경
          }}>
            {rung.isEnd ? (
              // END 줄
              Array.from({ length: 12 }, (_, x) => {
                const isCursorHere = currentMode === 'edit' && 
                  cursorPosition.rung === index && 
                  cursorPosition.x === x;
                
                return (
                  <div 
                    key={x} 
                    className={`border-r border-gray-300 relative cursor-pointer flex items-center justify-center
                      ${isCursorHere ? 'bg-yellow-100' : 'bg-gray-100'}
                      ${x === MAX_X ? 'border-r-0' : ''}`}
                    onClick={() => {
                      if (currentMode === 'edit') {
                        setCursorPosition({ rung: index, x });
                      }
                    }}
                  >
                    {x === 5 || x === 6 ? (
                      <span className="text-gray-600 font-mono text-sm">{x === 5 ? '[END]' : ''}</span>
                    ) : (
                      <div className="w-full h-1 bg-gray-300"></div>
                    )}
                  </div>
                );
              })
            ) : (
              // 일반 룽
              rung.cells.map((cell, x) => renderCell(cell, x, index, isMonitoring))
            )}
          </div>
          
          {/* 오른쪽 전원 레일 */}
          <div className="w-4 flex items-center justify-center border-l border-gray-300 bg-gray-50 h-10">
            <div className="w-full h-1 bg-black"></div>
          </div>
        </div>
        
        {/* 삭제 버튼 */}
        {!rung.isEnd && currentMode === 'edit' && (
          <div className="w-12 flex items-center justify-center border-l border-gray-300 h-10">
            <button
              onClick={() => {
                if (ladderProgram.rungs.length > 2) {
                  setLadderProgram(prev => ({
                    ...prev,
                    rungs: [
                      ...prev.rungs.slice(0, index),
                      ...prev.rungs.slice(index + 1).map(r => ({ 
                        ...r, 
                        number: r.isEnd ? 'END' : r.number - 1 
                      }))
                    ]
                  }));
                  
                  if (cursorPosition.rung === index) {
                    setCursorPosition(prev => ({ ...prev, rung: Math.max(0, index - 1) }));
                  }
                }
              }}
              className="p-1 text-gray-400 hover:text-red-600 transition-colors"
              title="룽 삭제"
            >
              <Trash2 size={16} />
            </button>
          </div>
        )}
      </div>
    );
  };

  return (
    <div className="w-full h-screen bg-gray-100 flex flex-col">
      {/* 헤더 툴바 */}
      <div className="bg-white border-b px-4 py-2">
        <div className="flex justify-between items-center">
          <div className="flex items-center space-x-4">
            <h1 className="text-xl font-bold text-gray-800">
              Mitsubishi FX3U PLC 시뮬레이터
            </h1>
            
            <div className={`px-3 py-1 rounded text-sm font-medium border ${
              isRunning ? 'bg-gray-600 text-white border-gray-700' : 'bg-gray-100 text-gray-700 border-gray-300'
            }`}>
              {isRunning ? 'RUN' : 'STOP'}
            </div>
          </div>
          
          <div className="flex items-center space-x-2">
            <button 
              onClick={() => setIsRunning(!isRunning)}
              className={`flex items-center gap-2 px-4 py-2 rounded font-medium border transition-colors ${
                isRunning 
                  ? 'bg-gray-600 text-white border-gray-700 hover:bg-gray-700' 
                  : 'bg-white text-gray-700 border-gray-300 hover:bg-gray-100'
              }`}
            >
              {isRunning ? <Pause size={16} /> : <Play size={16} />}
              {isRunning ? 'STOP' : 'RUN'}
            </button>
          </div>
        </div>
      </div>

      {/* 메인 툴바 */}
      <div className="bg-gray-50 border-b px-4 py-2">
        <div className="flex items-center justify-between">
          <div className="flex items-center space-x-4">
            {/* 모드 전환 */}
            <div className="flex items-center space-x-1">
              <button
                onClick={() => setCurrentMode('edit')}
                className={`flex items-center gap-1 px-3 py-1 text-sm rounded border ${
                  currentMode === 'edit' ? 'bg-gray-600 text-white border-gray-700' : 'bg-white text-gray-700 border-gray-300 hover:bg-gray-100'
                }`}
              >
                <Edit3 size={16} />
                쓰기 (F3)
              </button>
              <button
                onClick={() => setCurrentMode('monitor')}
                className={`flex items-center gap-1 px-3 py-1 text-sm rounded border ${
                  currentMode === 'monitor' ? 'bg-gray-600 text-white border-gray-700' : 'bg-white text-gray-700 border-gray-300 hover:bg-gray-100'
                }`}
              >
                <Monitor size={16} />
                모니터 (F2)
              </button>
            </div>
            
            <div className="w-px h-6 bg-gray-300"></div>
            
            {/* 도움말 토글 */}
            <button
              onClick={() => setShowHelp(!showHelp)}
              className={`flex items-center gap-1 px-3 py-1 text-sm rounded border ${
                showHelp ? 'bg-gray-600 text-white border-gray-700' : 'bg-white text-gray-700 border-gray-300 hover:bg-gray-100'
              }`}
            >
              <Info size={16} />
              도움말 (F1)
            </button>
          </div>
        </div>
      </div>

      <div className="flex flex-1 overflow-hidden">
        {/* 메인 영역 - 래더 다이어그램 */}
        <div className="flex-1 overflow-auto bg-gray-50 p-4 relative">
          <div className="bg-white border border-gray-400 shadow-sm relative">
            {/* 컬럼 헤더 */}
            <div className="flex border-b-2 border-gray-400">
              <div className="w-16 h-8 bg-gray-200 border-r-2 border-gray-400"></div>
              <div className="flex-1 flex">
                <div className="w-4 bg-gray-200"></div>
                <div className="flex-1 grid grid-cols-12">
                  {Array.from({ length: 12 }, (_, i) => (
                    <div key={i} className="h-8 flex items-center justify-center border-r border-gray-300 bg-gray-200 text-xs font-mono">
                      {i}
                    </div>
                  ))}
                </div>
                <div className="w-4 bg-gray-200"></div>
              </div>
              {currentMode === 'edit' && <div className="w-12 bg-gray-200 border-l border-gray-300"></div>}
            </div>
            
            {/* 래더 프로그램 */}
            <div className="relative">
              {ladderProgram.rungs.map((rung, index) => renderRung(rung, index))}
              
              {/* 세로선 연결 전용 오버레이 그리드 */}
              <div className="absolute inset-0 pointer-events-none">
                <div className="flex h-full">
                  {/* 룽 번호 공간 */}
                  <div className="w-16"></div>
                  {/* 래더 회로 공간 */}
                  <div className="flex-1 flex">
                    {/* 왼쪽 레일 */}
                    <div className="w-4"></div>
                    {/* 세로선 전용 13칸 그리드 (0-1 사이, 1-2 사이, ..., 11-12 사이) */}
                    <div className="flex-1 relative" style={{ 
                      display: 'grid', 
                      gridTemplateColumns: 'repeat(12, 1fr)',
                      height: `${ladderProgram.rungs.length * 40}px`
                    }}>
                      {/* 세로선 렌더링 */}
                      {renderVerticalConnections()}
                    </div>
                    {/* 오른쪽 레일 */}
                    <div className="w-4"></div>
                  </div>
                  {/* 삭제 버튼 공간 */}
                  {currentMode === 'edit' && <div className="w-12"></div>}
                </div>
              </div>
            </div>
          </div>
        </div>

        {/* 오른쪽 패널 */}
        <div className="w-80 bg-white border-l overflow-y-auto">
          <div className="p-4">
            {/* 키보드 도움말 */}
            {showHelp && currentMode === 'edit' && (
              <div className="mb-6 p-4 bg-gray-100 border border-gray-300 rounded">
                <h4 className="text-sm font-bold text-gray-800 mb-3">키보드 단축키</h4>
                <div className="space-y-2 text-xs">
                  <div className="grid grid-cols-2 gap-1">
                    <span className="font-mono bg-white px-1 rounded border">↑↓←→</span>
                    <span>커서 이동</span>
                    <span className="font-mono bg-white px-1 rounded border">F5</span>
                    <span>-| |- (NO)</span>
                    <span className="font-mono bg-white px-1 rounded border">F6</span>
                    <span>-|/|- (NC)</span>
                    <span className="font-mono bg-white px-1 rounded border">F7</span>
                    <span>출력/명령어</span>
                    <span className="font-mono bg-white px-1 rounded border">F9</span>
                    <span>━━━ 가로선</span>
                    <span className="font-mono bg-white px-1 rounded border">Shift+F9</span>
                    <span>세로선</span>
                    <span className="font-mono bg-white px-1 rounded border">Shift+Insert</span>
                    <span>새 룽</span>
                    <span className="font-mono bg-white px-1 rounded border">Del</span>
                    <span>삭제</span>
                    <span className="font-mono bg-white px-1 rounded border">Enter</span>
                    <span>편집</span>
                  </div>
                  <div className="mt-2 pt-2 border-t border-gray-300 text-gray-700">
                    <div className="text-xs mb-1">• F7에서 다양한 명령어 입력:</div>
                    <div className="text-xs mb-1">  - Y0: 일반 출력</div>
                    <div className="text-xs mb-1">  - T1 K10: 타이머 (1.0초)</div>
                    <div className="text-xs mb-1">  - C1 K5: 카운터 (5회)</div>
                    <div className="text-xs mb-1">  - RST C1: 리셋</div>
                    <div className="text-xs mb-1">• Shift+F9로 현재 커서 위치부터 세로선 연결</div>
                    <div className="text-xs mb-1">• 1 입력 = 현재 줄에서 아래 1줄로 연결</div>
                    <div className="text-xs mb-1">• 세로선으로 룽간 OR 연산 수행</div>
                    <div className="text-xs mb-1">• Shift+Insert 또는 END 칸에서 접점 추가 시 새 룽 생성</div>
                    <div className="text-xs mb-1">• Enter로 명령어 편집, Del로 삭제</div>
                    <div className="text-xs">• F2로 모니터 모드 전환하여 시뮬레이션</div>
                  </div>
                </div>
              </div>
            )}
            
            {/* 커서 위치 정보 */}
            {currentMode === 'edit' && (
              <div className="mb-4 p-3 bg-gray-100 rounded border">
                <h4 className="text-sm font-semibold text-gray-700 mb-2">커서 위치</h4>
                <div className="text-sm font-mono text-gray-800">
                  룽: {ladderProgram.rungs[cursorPosition.rung]?.isEnd ? 'END' : String(cursorPosition.rung).padStart(4, '0')}<br/>
                  X: {cursorPosition.x}
                </div>
              </div>
            )}

            {/* 모니터 모드에서만 시뮬레이션 제어 표시 */}
            {currentMode === 'monitor' && (
              <>
                <h3 className="text-lg font-bold mb-4">시뮬레이션 제어</h3>
                
                {/* 타이머/카운터 섹션 */}
                {(getUsedDevices().T.length > 0 || getUsedDevices().C.length > 0) && (
                  <div className="mb-6">
                    <h4 className="text-sm font-semibold text-gray-700 mb-3">타이머/카운터</h4>
                    
                    {/* 실제 래더에 있는 타이머만 표시 */}
                    {getUsedDevices().T.map(address => {
                      const timerState = deviceStates[address] || { value: 0, done: false, enabled: false };
                      const progressPercent = Math.min(100, (timerState.value / (timerState.preset || 10)) * 100);
                      
                      return (
                        <div key={address} className="mb-3 p-3 bg-gray-50 border border-gray-300 rounded">
                          <div className="flex justify-between items-center mb-2">
                            <span className="font-bold text-gray-800">{address}</span>
                            <span className={`text-sm px-2 py-1 rounded ${
                              timerState.done ? 'bg-gray-600 text-white' : 
                              timerState.enabled ? 'bg-gray-400 text-white' : 'bg-gray-200 text-gray-600'
                            }`}>
                              {timerState.done ? 'DONE' : timerState.enabled ? 'RUN' : 'STOP'}
                            </span>
                          </div>
                          
                          <div className="text-sm text-gray-600 mb-2">
                            현재: {timerState.value} / 설정: {timerState.preset || 10}
                          </div>
                          
                          {/* 간단한 프로그레스 바 */}
                          <div className="w-full bg-gray-200 rounded h-2">
                            <div 
                              className="bg-gray-400 h-2 rounded transition-all duration-300"
                              style={{ width: `${progressPercent}%` }}
                            ></div>
                          </div>
                        </div>
                      );
                    })}

                    {/* 실제 래더에 있는 카운터만 표시 */}
                    {getUsedDevices().C.map(address => {
                      const counterState = deviceStates[address] || { value: 0, done: false };
                      const progressPercent = Math.min(100, (counterState.value / (counterState.preset || 10)) * 100);
                      
                      return (
                        <div key={address} className="mb-3 p-3 bg-gray-50 border border-gray-300 rounded">
                          <div className="flex justify-between items-center mb-2">
                            <span className="font-bold text-gray-800">{address}</span>
                            <span className={`text-sm px-2 py-1 rounded ${
                              counterState.done ? 'bg-gray-600 text-white' : 'bg-gray-200 text-gray-600'
                            }`}>
                              {counterState.done ? 'DONE' : 'STOP'}
                            </span>
                          </div>
                          
                          <div className="text-sm text-gray-600 mb-2">
                            현재: {counterState.value} / 설정: {counterState.preset || 10}
                          </div>
                          
                          {/* 간단한 프로그레스 바 */}
                          <div className="w-full bg-gray-200 rounded h-2">
                            <div 
                              className="bg-gray-400 h-2 rounded transition-all duration-300"
                              style={{ width: `${progressPercent}%` }}
                            ></div>
                          </div>
                        </div>
                      );
                    })}
                  </div>
                )}

                {/* 내부 릴레이 제어 - M0~M15 */}
                {getUsedDevices().M.length > 0 && (
                  <div className="mb-6">
                    <h4 className="text-sm font-semibold text-gray-700 mb-2">내부 릴레이 (M)</h4>
                    <div className="text-xs text-gray-500 mb-2">래더 프로그램에 의해 제어</div>
                    <div className="grid grid-cols-4 gap-1">
                      {getUsedDevices().M.map(address => {
                        return (
                          <div 
                            key={address} 
                            className={`flex flex-col items-center p-1 rounded text-xs border ${
                              deviceStates[address] 
                                ? 'bg-gray-600 text-white border-gray-700' 
                                : 'bg-gray-100 text-gray-600 border-gray-300'
                            }`}
                          >
                            <span className="font-mono text-xs font-bold">{address}</span>
                            <div className={`w-4 h-4 rounded-full border mt-1 ${
                              deviceStates[address] 
                                ? 'bg-white border-gray-200' 
                                : 'bg-gray-300 border-gray-400'
                            }`}></div>
                          </div>
                        );
                      })}
                    </div>
                  </div>
                )}

                {/* 입력 제어 - X0~X15 */}
                <div className="mb-6">
                  <h4 className="text-sm font-semibold text-gray-700 mb-2">입력 제어 (X0~X15)</h4>
                  <div className="text-xs text-gray-500 mb-2">클릭하여 ON/OFF</div>
                  <div className="grid grid-cols-4 gap-1">
                    {Array.from({ length: 16 }, (_, i) => {
                      const address = `X${i}`;
                      return (
                        <div 
                          key={address} 
                          className={`flex flex-col items-center p-1 rounded cursor-pointer transition-colors text-xs border ${
                            deviceStates[address] 
                              ? 'bg-gray-600 text-white border-gray-700 hover:bg-gray-700' 
                              : 'bg-gray-100 text-gray-600 border-gray-300 hover:bg-gray-200'
                          }`}
                          onClick={() => {
                            setDeviceStates(prev => ({
                              ...prev,
                              [address]: !prev[address]
                            }));
                          }}
                        >
                          <span className="font-mono text-xs font-bold">{address}</span>
                          <div className={`w-4 h-4 rounded-full border mt-1 ${
                            deviceStates[address] 
                              ? 'bg-white border-gray-200' 
                              : 'bg-gray-300 border-gray-400'
                          }`}></div>
                        </div>
                      );
                    })}
                  </div>
                </div>
                
                {/* 출력 상태 - Y0~Y15 */}
                <div className="mb-6">
                  <h4 className="text-sm font-semibold text-gray-700 mb-2">출력 상태 (Y0~Y15)</h4>
                  <div className="grid grid-cols-4 gap-1">
                    {Array.from({ length: 16 }, (_, i) => {
                      const address = `Y${i}`;
                      return (
                        <div 
                          key={address} 
                          className={`flex flex-col items-center p-1 rounded text-xs border ${
                            deviceStates[address] 
                              ? 'bg-gray-600 text-white border-gray-700' 
                              : 'bg-gray-100 text-gray-600 border-gray-300'
                          }`}
                        >
                          <span className="font-mono text-xs font-bold">{address}</span>
                          <div className={`w-4 h-4 rounded-full border mt-1 ${
                            deviceStates[address] 
                              ? 'bg-white border-gray-200' 
                              : 'bg-gray-300 border-gray-400'
                          }`}></div>
                        </div>
                      );
                    })}
                  </div>
                </div>
              </>
            )}
          </div>
        </div>
      </div>

      {/* 하단 상태바 */}
      <div className="bg-white border-t px-4 py-2">
        <div className="flex justify-between items-center text-sm">
          <div className="flex items-center space-x-6">
            <span>
              모드: <span className="font-medium">
                {currentMode === 'edit' ? '쓰기' : '모니터'}
              </span>
            </span>
            {currentMode === 'edit' && (
              <span>
                커서: <span className="font-medium">
                  {ladderProgram.rungs[cursorPosition.rung]?.isEnd ? 'END' : `${String(cursorPosition.rung).padStart(4, '0')}`}
                  : X{cursorPosition.x}
                </span>
              </span>
            )}
          </div>
          <div className="flex items-center space-x-6">
            <span>스텝: {ladderProgram.rungs.filter(r => !r.isEnd).length}</span>
            <span>세로선: {ladderProgram.verticalConnections.length}개</span>
            {ladderProgram.verticalConnections.length > 0 && (
              <span className="text-gray-600 text-xs">
                {ladderProgram.verticalConnections.map(conn => 
                  `X${conn.x-1}↔X${conn.x}(R${Math.min(...conn.rungs).toString().padStart(4,'0')}-${Math.max(...conn.rungs).toString().padStart(4,'0')})`
                ).join(', ')}
              </span>
            )}
            <span className={isRunning ? 'text-gray-700 font-medium' : 'text-gray-500'}>
              PLC: {isRunning ? 'RUN' : 'STOP'}
            </span>
          </div>
        </div>
      </div>

      {/* 세로선 연결 다이얼로그 */}
      {showVerticalDialog && (
        <div 
          className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50 vertical-dialog"
          onClick={() => setShowVerticalDialog(false)}
          onKeyDown={(e) => {
            if (e.key === 'Enter') {
              e.preventDefault();
              e.stopPropagation();
              if (verticalConnectionData.lineCount >= 1) {
                confirmVerticalConnection();
              }
            }
            if (e.key === 'Escape') {
              e.preventDefault();
              e.stopPropagation();
              setShowVerticalDialog(false);
            }
          }}
          tabIndex={0}
        >
          <div 
            className="bg-white rounded-lg p-6 w-96"
            onClick={(e) => e.stopPropagation()}
          >
            <h3 className="text-lg font-bold mb-4">
              세로선 연결 (X{verticalConnectionData.x - 1} ↔ X{verticalConnectionData.x} 사이)
            </h3>
            
            <div className="mb-4 p-3 bg-gray-50 rounded">
              <div className="text-sm text-gray-700">
                <div><strong>시작 룽:</strong> {String(verticalConnectionData.startRung).padStart(4, '0')}</div>
                <div><strong>시작 위치:</strong> X{verticalConnectionData.x - 1} ↔ X{verticalConnectionData.x}</div>
              </div>
            </div>
            
            <div className="mb-4">
              <label className="block text-sm font-medium text-gray-700 mb-2">
                현재 룽에서 아래로 연결할 줄 수
              </label>
              <input
                type="number"
                value={verticalConnectionData.lineCount}
                onChange={(e) => setVerticalConnectionData(prev => ({ 
                  ...prev, 
                  lineCount: Math.max(1, parseInt(e.target.value) || 1)
                }))}
                className="w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
                min="1"
                max={ladderProgram.rungs.length - verticalConnectionData.startRung - 1}
                autoFocus
                onKeyDown={(e) => {
                  if (e.key === 'Enter') {
                    e.preventDefault();
                    e.stopPropagation();
                    if (verticalConnectionData.lineCount >= 1) {
                      confirmVerticalConnection();
                    }
                  }
                  if (e.key === 'Escape') {
                    e.preventDefault();
                    e.stopPropagation();
                    setShowVerticalDialog(false);
                  }
                }}
              />
            </div>
            
            <div className="flex justify-end space-x-2">
              <button
                onClick={() => setShowVerticalDialog(false)}
                className="px-4 py-2 text-gray-700 border border-gray-300 rounded hover:bg-gray-50"
              >
                취소
              </button>
              <button
                onClick={confirmVerticalConnection}
                className="px-4 py-2 bg-gray-600 text-white rounded hover:bg-gray-700"
              >
                연결
              </button>
            </div>
          </div>
        </div>
      )}

      {/* 주소 입력 다이얼로그 */}
      {showAddressDialog && (
        <div 
          className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50 address-dialog"
          onClick={() => setShowAddressDialog(false)}
          onKeyDown={(e) => {
            if (e.key === 'Enter') {
              e.preventDefault();
              e.stopPropagation();
              if (tempAddress.trim()) {
                confirmAddInstruction();
              }
            }
            if (e.key === 'Escape') {
              e.preventDefault();
              e.stopPropagation();
              setShowAddressDialog(false);
            }
          }}
          tabIndex={0}
        >
          <div 
            className="bg-white rounded-lg p-6 w-96"
            onClick={(e) => e.stopPropagation()}
          >
            <h3 className="text-lg font-bold mb-4">
              {tempInstructionType === 'contact_no' ? '-| |- (NO 접점)' :
               tempInstructionType === 'contact_nc' ? '-|/|- (NC 접점)' :
               tempInstructionType === 'coil' ? '-( )- (출력/명령어)' :
               tempInstructionType === 'timer' ? '[TON] (타이머)' :
               tempInstructionType === 'counter' ? '[CTU] (카운터)' : '명령어'}
            </h3>
            <div className="mb-4">
              <label className="block text-sm font-medium text-gray-700 mb-2">
                {tempInstructionType === 'coil' ? '명령어' : '디바이스'}
              </label>
              <input
                type="text"
                value={tempAddress}
                onChange={(e) => setTempAddress(e.target.value.toUpperCase())}
                className="w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
                placeholder={
                  tempInstructionType === 'coil' ? 'Y0 또는 T1 K10 또는 C1 K5 또는 RST C1' :
                  tempInstructionType === 'timer' ? 'T0' :
                  tempInstructionType === 'counter' ? 'C0' : 'X0'
                }
                autoFocus
              />
              {tempInstructionType === 'coil' && (
                <div className="mt-2 text-xs text-gray-600">
                  <div>• 일반 출력: Y0, Y1...</div>
                  <div>• 타이머: T1 K10 (T1번 1.0초)</div>
                  <div>• 카운터: C1 K5 (C1번 5회)</div>
                  <div>• 리셋: RST C1 (C1 리셋)</div>
                </div>
              )}
            </div>
            
            <div className="flex justify-end space-x-2">
              <button
                onClick={() => setShowAddressDialog(false)}
                className="px-4 py-2 text-gray-700 border border-gray-300 rounded hover:bg-gray-50"
              >
                취소
              </button>
              <button
                onClick={confirmAddInstruction}
                className="px-4 py-2 bg-gray-600 text-white rounded hover:bg-gray-700 disabled:bg-gray-400 disabled:cursor-not-allowed"
                disabled={!tempAddress.trim()}
              >
                입력
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default PLCSimulator;