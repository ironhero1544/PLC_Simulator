# FX3U/FX3UC 명령 구현 대조표

공식 응용 명령 219개 중 실행 경로가 등록된 항목: 219개.
등록은 전체 피연산자·하드웨어 동작 검증 완료를 뜻하지 않는다.
D/P 변형, 디바이스 범위 및 장치 연결은 별도 검증 대상이다.
미등록 명령을 빈 동작으로 성공 처리하지 않는다.

출처: [Mitsubishi Electric 응용 명령 목록](https://www.mitsubishielectric.com/fa/products/cnt/plc_fx/pmerit/contents/plc/instruction.html)

`scripts/update_fx_instruction_coverage.py`로 재생성한다.

| FNC | 명령 | 기능 | 실행 경로 |
| --- | --- | --- | --- |
| 0 | CJ | Conditional Jump | 등록됨 |
| 1 | CALL | Call Subroutine | 등록됨 |
| 2 | SRET | Subroutine Return | 등록됨 |
| 3 | IRET | Interrupt Return | 등록됨 |
| 4 | EI | Enable Interrupt | 등록됨 |
| 5 | DI | Disable Interrupt | 등록됨 |
| 6 | FEND | Main Routine Program End | 등록됨 |
| 7 | WDT | Watchdog Timer Refresh | 등록됨 |
| 8 | FOR | Start a FOR/NEXT Loop | 등록됨 |
| 9 | NEXT | End a FOR/NEXT Loop | 등록됨 |
| 10 | CMP | Compare | 등록됨 |
| 11 | ZCP | Zone Compare | 등록됨 |
| 12 | MOV | Move | 등록됨 |
| 13 | SMOV | Shift Move | 등록됨 |
| 14 | CML | Complement | 등록됨 |
| 15 | BMOV | Block Move | 등록됨 |
| 16 | FMOV | Fill Move | 등록됨 |
| 17 | XCH | Exchange | 등록됨 |
| 18 | BCD | Conversion to Binary Coded Decimal | 등록됨 |
| 19 | BIN | Conversion to Binary | 등록됨 |
| 20 | ADD | Addition | 등록됨 |
| 21 | SUB | Subtraction | 등록됨 |
| 22 | MUL | Multiplication | 등록됨 |
| 23 | DIV | Division | 등록됨 |
| 24 | INC | Increment | 등록됨 |
| 25 | DEC | Decrement | 등록됨 |
| 26 | WAND | Logical Word AND | 등록됨 |
| 27 | WOR | Logical Word OR | 등록됨 |
| 28 | WXOR | Logical Exclusive OR | 등록됨 |
| 29 | NEG | Negation | 등록됨 |
| 30 | ROR | Rotation Right | 등록됨 |
| 31 | ROL | Rotation Left | 등록됨 |
| 32 | RCR | Rotation Right with Carry | 등록됨 |
| 33 | RCL | Rotation Left with Carry | 등록됨 |
| 34 | SFTR | Bit Shift Right | 등록됨 |
| 35 | SFTL | Bit Shift Left | 등록됨 |
| 36 | WSFR | Word Shift Right | 등록됨 |
| 37 | WSFL | Word Shift Left | 등록됨 |
| 38 | SFWR | Shift write [FIFO/FILO control] | 등록됨 |
| 39 | SFRD | Shift read [FIFO control] | 등록됨 |
| 40 | ZRST | Zone Reset | 등록됨 |
| 41 | DECO | Decode | 등록됨 |
| 42 | ENCO | Encode | 등록됨 |
| 43 | SUM | Sum of Active Bits | 등록됨 |
| 44 | BON | Check Specified Bit Status | 등록됨 |
| 45 | MEAN | Mean | 등록됨 |
| 46 | ANS | Timed Annunciator Set | 등록됨 |
| 47 | ANR | Annunciator Reset | 등록됨 |
| 48 | SQR | Square Root | 등록됨 |
| 49 | FLT | Conversion to Floating Point | 등록됨 |
| 50 | REF | Refresh | 등록됨 |
| 51 | REFF | Refresh and filter adjust | 등록됨 |
| 52 | MTR | Input Matrix | 등록됨 |
| 53 | HSCS | High-speed counter set | 등록됨 |
| 54 | HSCR | High-speed counter reset | 등록됨 |
| 55 | HSZ | High-speed counter zone compare | 등록됨 |
| 56 | SPD | Speed Detection | 등록됨 |
| 57 | PLSY | Pulse Y Output | 등록됨 |
| 58 | PWM | Pulse Width Modulation | 등록됨 |
| 59 | PLSR | Acceleration/deceleration setup | 등록됨 |
| 60 | IST | Initial State | 등록됨 |
| 61 | SER | Search a Data Stack | 등록됨 |
| 62 | ABSD | Absolute drum sequencer | 등록됨 |
| 63 | INCD | Incremental drum sequencer | 등록됨 |
| 64 | TTMR | Teaching Timer | 등록됨 |
| 65 | STMR | Special Timer | 등록됨 |
| 66 | ALT | Alternate State | 등록됨 |
| 67 | RAMP | Ramp Variable Value | 등록됨 |
| 68 | ROTC | Rotary Table Control | 등록됨 |
| 69 | SORT | SORT Tabulated Data | 등록됨 |
| 70 | TKY | Ten Key Input | 등록됨 |
| 71 | HKY | Hexadecimal Input | 등록됨 |
| 72 | DSW | Digital switch (thumbwheel input) | 등록됨 |
| 73 | SEGD | Seven Segment Decoder | 등록됨 |
| 74 | SEGL | Seven Segment With Latch | 등록됨 |
| 75 | ARWS | Arrow Switch | 등록됨 |
| 76 | ASC | ASCII code data input | 등록됨 |
| 77 | PR | Print (ASCII Code) | 등록됨 |
| 78 | FROM | Read From A Special Function Block | 등록됨 |
| 79 | TO | Write To A Special Function Block | 등록됨 |
| 80 | RS | Serial Communication | 등록됨 |
| 81 | PRUN | Parallel Run (Octal Mode) | 등록됨 |
| 82 | ASCI | Hexadecimal to ASCII Conversion | 등록됨 |
| 83 | HEX | ASCII to Hexadecimal Conversion | 등록됨 |
| 84 | CCD | Check Code | 등록됨 |
| 85 | VRRD | Volume Read | 등록됨 |
| 86 | VRSC | Volume Scale | 등록됨 |
| 87 | RS2 | Serial Communication 2 | 등록됨 |
| 88 | PID | PID Control Loop | 등록됨 |
| 102 | ZPUSH | Batch Store of Index Register | 등록됨 |
| 103 | ZPOP | Batch POP of Index Register | 등록됨 |
| 110 | ECMP | Floating Point Compare | 등록됨 |
| 111 | EZCP | Floating Point Zone Compare | 등록됨 |
| 112 | EMOV | Floating Point Move | 등록됨 |
| 116 | ESTR | Floating Point to Character String Conversion | 등록됨 |
| 117 | EVAL | Character String to Floating Point Conversion | 등록됨 |
| 118 | EBCD | Floating Point to Scientific Notation Conversion | 등록됨 |
| 119 | EBIN | Scientific Notation to Floating Point Conversion | 등록됨 |
| 120 | EADD | Floating Point Addition | 등록됨 |
| 121 | ESUB | Floating Point Subtraction | 등록됨 |
| 122 | EMUL | Floating Point Multiplication | 등록됨 |
| 123 | EDIV | Floating Point Division | 등록됨 |
| 124 | EXP | Floating Point Exponent | 등록됨 |
| 125 | LOGE | Floating Point Natural Logarithm | 등록됨 |
| 126 | LOG10 | Floating Point Common Logarithm | 등록됨 |
| 127 | ESQR | Floating Point Square Root | 등록됨 |
| 128 | ENEG | Floating Point Negation | 등록됨 |
| 129 | INT | Floating Point to Integer Conversion | 등록됨 |
| 130 | SIN | Floating Point Sine | 등록됨 |
| 131 | COS | Floating Point Cosine | 등록됨 |
| 132 | TAN | Floating Point Tangent | 등록됨 |
| 133 | ASIN | Floating Point Arc Sine -1 | 등록됨 |
| 134 | ACOS | Floating Point Arc Cosine -1 | 등록됨 |
| 135 | ATAN | Floating Point Arc Tangent -1 | 등록됨 |
| 136 | RAD | Floating Point Degrees to Radians Conversion | 등록됨 |
| 137 | DEG | Floating Point Radians to Degrees Conversion | 등록됨 |
| 140 | WSUM | Sum of Word Data | 등록됨 |
| 141 | WTOB | WORD to BYTE | 등록됨 |
| 142 | BTOW | BYTE to WORD | 등록됨 |
| 143 | UNI | 4-bit Linking of Word Data | 등록됨 |
| 144 | DIS | 4-bit Grouping of Word Data | 등록됨 |
| 147 | SWAP | Byte Swap | 등록됨 |
| 149 | SORT2 | Sort Tabulated Data 2 | 등록됨 |
| 150 | DSZR | DOG Search Zero Return | 등록됨 |
| 151 | DVIT | Interrupt Positioning | 등록됨 |
| 152 | TBL | Batch Data Positioning Mode | 등록됨 |
| 155 | ABS | Absolute Current Value Read | 등록됨 |
| 156 | ZRN | Zero Return | 등록됨 |
| 157 | PLSV | Variable Speed Pulse Output | 등록됨 |
| 158 | DRVI | Drive to Increment | 등록됨 |
| 159 | DRVA | Drive to Absolute | 등록됨 |
| 160 | TCMP | RTC data compare | 등록됨 |
| 161 | TZCP | RTC data zone compare | 등록됨 |
| 162 | TADD | RTC data addition | 등록됨 |
| 163 | TSUB | RTC data subtraction | 등록됨 |
| 164 | HTOS | Hour to second conversion | 등록됨 |
| 165 | STOH | Second to hour conversion | 등록됨 |
| 166 | TRD | Read RTC data | 등록됨 |
| 167 | TWR | Set RTC data | 등록됨 |
| 169 | HOUR | Hour Meter | 등록됨 |
| 170 | GRY | Decimal to Gray Code Conversion | 등록됨 |
| 171 | GBIN | Gray Code to Decimal Conversion | 등록됨 |
| 176 | RD3A | Read form Dedicated Analog Block | 등록됨 |
| 177 | WR3A | Write to Dedicated Analog Block | 등록됨 |
| 182 | COMRD | Read device comment data | 등록됨 |
| 184 | RND | Random Number Generation | 등록됨 |
| 186 | DUTY | Timing pulse generation | 등록됨 |
| 188 | CRC | Cyclic Redundancy Check | 등록됨 |
| 189 | HCMOV | High-speed counter move | 등록됨 |
| 192 | BK+ | Block Data Addition | 등록됨 |
| 193 | BK- | Block Data Subtraction | 등록됨 |
| 194 | BKCMP= | Block Data Compare (S1) = (S2) | 등록됨 |
| 195 | BKCMP> | Block Data Compare (S1) > (S2) | 등록됨 |
| 196 | BKCMP< | Block Data Compare (S1) < (S2) | 등록됨 |
| 197 | BKCMP<> | Block Data Compare (S1) ≠ (S2) | 등록됨 |
| 198 | BKCMP<= | Block Data Compare (S1) ≦ (S2) | 등록됨 |
| 199 | BKCMP>= | Block Data Compare (S1) ≧ (S2) | 등록됨 |
| 200 | STR | BIN to Character String Conversion | 등록됨 |
| 201 | VAL | Character String to BIN Conversion | 등록됨 |
| 202 | $+ | Link Character Strings | 등록됨 |
| 203 | LEN | Character String Length Detection | 등록됨 |
| 204 | RIGHT | Extracting Character String Data from the Right | 등록됨 |
| 205 | LEFT | Extracting Character String Data from the Left | 등록됨 |
| 206 | MIDR | Random Selection of Character Strings | 등록됨 |
| 207 | MIDW | Random Replacement of Character Strings | 등록됨 |
| 208 | INSTR | Character string search | 등록됨 |
| 209 | $MOV | Character String Transfer | 등록됨 |
| 210 | FDEL | Deleting Data from Tables | 등록됨 |
| 211 | FINS | Inserting Data to Tables | 등록됨 |
| 212 | POP | Shift Last Data Read [FILO Control] | 등록됨 |
| 213 | SFR | 16-bit data n Bit Shift Right with Carry | 등록됨 |
| 214 | SFL | 16-bit data n Bit Shift Left with Carry | 등록됨 |
| 224 | LD= | Load Compare (S1) = (S2) | 등록됨 |
| 225 | LD> | Load Compare (S1) > (S2) | 등록됨 |
| 226 | LD< | Load Compare (S1) < (S2) | 등록됨 |
| 228 | LD<> | Load Compare (S1) ≠ (S2) | 등록됨 |
| 229 | LD<= | Load Compare (S1) ≦ (S2) | 등록됨 |
| 230 | LD>= | Load Compare (S1) ≧ (S2) | 등록됨 |
| 232 | AND= | AND Compare (S1) = (S2) | 등록됨 |
| 233 | AND> | AND Compare (S1) > (S2) | 등록됨 |
| 234 | AND< | AND Compare (S1) < (S2) | 등록됨 |
| 236 | AND<> | AND Compare (S1) ≠ (S2) | 등록됨 |
| 237 | AND<= | AND Compare (S1) ≦ (S2) | 등록됨 |
| 238 | AND>= | AND Compare (S1) ≧ (S2) | 등록됨 |
| 240 | OR= | OR Compare (S1) = (S2) | 등록됨 |
| 241 | OR> | OR Compare (S1) > (S2) | 등록됨 |
| 242 | OR< | OR Compare (S1) < (S2) | 등록됨 |
| 244 | OR<> | OR Compare (S1) ≠ (S2) | 등록됨 |
| 245 | OR<= | OR Compare (S1) ≦ (S2) | 등록됨 |
| 246 | OR>= | OR Compare (S1) ≧ (S2) | 등록됨 |
| 256 | LIMIT | Limit Control | 등록됨 |
| 257 | BAND | Dead Band Control | 등록됨 |
| 258 | ZONE | Zone Control | 등록됨 |
| 259 | SCL | Scaling (Coordinate by Point Data) | 등록됨 |
| 260 | DABIN | Decimal ASCII to BIN Conversion | 등록됨 |
| 261 | BINDA | BIN to Decimal ASCII Conversion | 등록됨 |
| 269 | SCL2 | Scaling 2 (Coordinate by X/Y Data) | 등록됨 |
| 270 | IVCK | Inverter Status Check | 등록됨 |
| 271 | IVDR | Inverter Drive | 등록됨 |
| 272 | IVRD | Inverter Parameter Read | 등록됨 |
| 273 | IVWR | Inverter Parameter Write | 등록됨 |
| 274 | IVBWR | Inverter Parameter Block Write | 등록됨 |
| 275 | IVMC | Inverter Multi Command | 등록됨 |
| 276 | ADPRW | MODBUS Read / Write | 등록됨 |
| 278 | RBFM | Divided BFM Read | 등록됨 |
| 279 | WBFM | Divided BFM Write | 등록됨 |
| 280 | HSCT | High-Speed Counter Compare With Data Table | 등록됨 |
| 290 | LOADR | Load From ER | 등록됨 |
| 291 | SAVER | Save to ER | 등록됨 |
| 292 | INITR | Initialize R and ER | 등록됨 |
| 293 | LOGR | Logging R and ER | 등록됨 |
| 294 | RWER | Rewrite to ER | 등록됨 |
| 295 | INITER | Initialize ER | 등록됨 |
| 300 | FLCRT | File create / check | 등록됨 |
| 301 | FLDEL | File delete / CF card format | 등록됨 |
| 302 | FLWR | Data write | 등록됨 |
| 303 | FLRD | Data read | 등록됨 |
| 304 | FLCMD | FX3U-CF-ADP command | 등록됨 |
| 305 | FLSTRD | FX3U-CF-ADP status read | 등록됨 |

## 목록 외 기본 명령 및 기반 기능

- LD/LDI/LDP/LDF, AND/ANI/ANDP/ANDF, OR/ORI/ORP/ORF, ANB/ORB, MPS/MRD/MPP, INV, OUT/SET/RST, END: 구현됨.
- MC/MCR, PLS/PLF, NOP, STL/RET 및 IST 초기 상태 제어: 구현됨.
- P/I 포인터 및 5중 CALL/FOR, 입력·타이머 인터럽트: 구현됨.
- packed-bit 스칼라, 인덱스 V/Z, 특수 D, 파일 R/ER, D 비트 지정: 구현됨. 전체 피연산자 조합 검증 필요.
- 가상 BFM·아날로그·고속 카운터·물리 I/O·통신·축·인버터·MODBUS·CF: 실행 모델 구현됨.
- 실제 GX Works2 가져오기 및 대상 CPU/모듈의 전체 피연산자·타이밍 조합 대조: 검증 필요.
