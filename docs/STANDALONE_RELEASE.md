# 단일 EXE 배포

`PLCSimulator-2.0.0-win64-standalone.exe` 하나를 전달하고 더블클릭하여 실행한다.
설치 마법사, 관리자 권한, 사용자의 수동 압축 해제가 필요하지 않다.

실행기는 검증된 CPack ZIP의 앱, 런타임 DLL, 폰트, 언어 파일과 리소스를
내장한다. 최초 실행에 `%LOCALAPPDATA%\PLCSimulator\Standalone\<버전>-<패키지 해시>`에
풀고 해당 폴더에서 앱을 실행한다. 이후에는 같은 캐시를 재사용한다.
앱 종료 코드와 명령줄 인자를 전달하며 제거 프로그램이나 바로가기를 만들지 않는다.
배포 파일은 하나지만 실행 중 필요한 파일은 디스크 캐시에 존재하는 방식이다.

UI 설정은 기존 앱과 동일하게 `%LOCALAPPDATA%\PLCSimulator\ui_settings.cfg`에
보관된다. RTL을 사용하려면 기존과 같이 WSL2와 RTL 도구가 별도로 필요하다.
코드 서명은 추가되지 않는다.

## 재생성

먼저 최신 Release를 빌드하고 CPack ZIP을 생성한다. 기존 ZIP을 입력하면
그 ZIP에 포함된 앱 버전이 그대로 사용되므로 최신 ZIP인지 확인해야 한다.

```powershell
./scripts/BuildStandalone.ps1 -PackageZip ./releases/2.0.0-2026-10-09/PLCSimulator-2.0.0-win64.zip
```

기본 NSIS 위치가 다르면 `-MakeNsis`로 `makensis.exe` 경로를 지정한다.
생성된 실행기는 ZIP과 같은 폴더에 `-standalone.exe` 이름으로 저장된다.

## 패키지 검증

`/extract-only=<절대 경로>` 인자로 앱을 실행하지 않고 지정한 폴더에
내용물을 풀 수 있다. 빈 검증용 폴더를 지정하고 원본 ZIP의 모든 파일과
SHA-256을 비교한다. 실행기가 추가하는 `payload.ready`는 패키지 해시이다.
이 검증은 패키지 내용의 일치를 확인하며 GUI 실행 검증을 대신하지 않는다.
