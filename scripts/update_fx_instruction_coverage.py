"""Regenerate the FX instruction audit from the official catalog and registry."""

import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
catalog = json.loads((ROOT / "docs/FX_INSTRUCTION_CATALOG.json").read_text(
    encoding="utf-8"))
registry = (ROOT / "src/programming/execution_program.cpp").read_text(
    encoding="utf-8")
registered = set(re.findall(r'\{Opcode::\w+,\s*"([^"]+)"', registry))
comparisons = {prefix + suffix for prefix in ("LD", "AND", "OR")
               for suffix in ("=", ">", "<", "<>", "<=", ">=")}
implemented = registered | comparisons
items = catalog["instructions"]
count = sum(item["mnemonic"] in implemented for item in items)
lines = ["# FX3U/FX3UC 명령 구현 대조표", "",
         f"공식 응용 명령 {len(items)}개 중 실행 경로가 등록된 항목: {count}개.",
         "등록은 전체 피연산자·하드웨어 동작 검증 완료를 뜻하지 않는다.",
         "D/P 변형, 디바이스 범위 및 장치 연결은 별도 검증 대상이다.",
         "미등록 명령을 빈 동작으로 성공 처리하지 않는다.", "",
         f'출처: [Mitsubishi Electric 응용 명령 목록]({catalog["source"]})', "",
         "`scripts/update_fx_instruction_coverage.py`로 재생성한다.", "",
         "| FNC | 명령 | 기능 | 실행 경로 |", "| --- | --- | --- | --- |"]
for item in items:
    status = "등록됨" if item["mnemonic"] in implemented else "미구현"
    lines.append(f'| {item["fnc"]} | {item["mnemonic"]} | '
                 f'{item["function"]} | {status} |')
lines += ["", "## 목록 외 기본 명령 및 기반 기능", "",
          "- LD/LDI/LDP/LDF, AND/ANI/ANDP/ANDF, OR/ORI/ORP/ORF, "
          "ANB/ORB, MPS/MRD/MPP, INV, OUT/SET/RST, END: 구현됨.",
          "- MC/MCR, PLS/PLF, NOP, STL/RET 및 IST 초기 상태 제어: 구현됨.",
          "- P/I 포인터 및 5중 CALL/FOR, 입력·타이머 인터럽트: 구현됨.",
          "- packed-bit 스칼라, 인덱스 V/Z, 특수 D, 파일 R/ER, D 비트 지정: 구현됨. 전체 피연산자 조합 검증 필요.",
          "- 가상 BFM·아날로그·고속 카운터·물리 I/O·통신·축·인버터·MODBUS·CF: 실행 모델 구현됨.",
          "- 실제 GX Works2 가져오기 및 대상 CPU/모듈의 전체 피연산자·타이밍 조합 대조: 검증 필요.", ""]
(ROOT / "docs/FX_INSTRUCTION_COVERAGE.md").write_text(
    "\n".join(lines), encoding="utf-8")
print(f"{count}/{len(items)} official applied instructions have execution paths")
