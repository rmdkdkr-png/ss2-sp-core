# 도구 모음

엔진을 고칠 때 쓰는 검증 도구들이다. **전부 세이브스테이트와 롬이 있어야 돌아간다** —
둘 다 저장소에 없으니 본인 것을 쓰면 된다.

## harness/ — 사무라이 쇼다운!2

엔진(`src/ss2comm.c`)을 에뮬레이터 없이 직접 링크해 화면을 뽑는 도구들이다.
기둥·해설·오버레이를 고칠 때 실기 없이 눈으로 확인하려고 만들었다.

| 파일 | 무엇 |
|---|---|
| `promoshot.c` | 실플레이 화면(ppm)+실램(bin)으로 **앱 화면 전체**를 합성. 기둥·해설띠·오버레이까지 |
| `artview.c` | 일러 주소표를 구워 기둥 그림만 확인 |
| `sideview.c` | 기둥 렌더 단독 |
| `gdview.c` | 간다라 이중층(몸+얼굴) 확인 |
| `effview.c`·`fxview.c` | 전황 연출(흔들림·백섬광·틴트·흑백) 시험 |
| `ovview.c` | 빠른 설정 오버레이 미리보기 |
| `swview.c`·`tiletest.c` | 타일·팔레트 확인 |
| `audit.c` | 대사표 점검 |
| `ss2comm_dbg.c` | 디버그 출력을 넣은 엔진 사본 |

빌드 예:
```bash
gcc -O1 -DSS2SP_RAM_POINTER -DSS2COMM_TEST -I../../src \
    -o promoshot promoshot.c ../../src/ss2comm.c ../../src/ss2sp.c -lm
SS2_ROM=/경로/ss2.ngc ./promoshot 화면.ppm 램.bin fight 결과.ppm
```
`-DSS2COMM_TEST` 를 빼면 `CPUExRAM` 을 못 찾아 링크가 깨진다.

## svc/ — SNK vs Capcom MotM 정찰

libretro 코어를 통째로 열어 **입력을 프레임 단위로 주입**하고 램을 관찰한다.
SS2 하네스와 달리 게임 전체가 실제로 돌아간다.

| 파일 | 무엇 |
|---|---|
| `svcrun.c` | 입력 스크립트 실행 + 램/화면 덤프 + 상태 저장·복원 + CSV 관찰 |
| `probe.py` | 커맨드 후보 84종을 전부 넣어 보고 **지문이 같으면 같은 기술로 묶는** 실측기 |
| `SVC_MEMO.md` | 알아낸 것 전부 — 램 오프셋, 캐릭터 ID, 입력 규칙, 가로채기 시험 |
| `SWEEP.txt` | 18명 전수 실측 (1차) |
| `SWEEP2.txt` | 18명 전수 실측 (공중 보정판) |
| `chars.txt`·`scan.csv` | 캐릭터 ID ↔ 선택 커서 위치 지도 |

```bash
gcc -O1 -std=gnu99 -o svcrun svcrun.c -ldl
python3 probe.py <상태파일> [방향간격]      # 기본 간격 3프레임
```

`svcrun` 스크립트 문법:
```
120 -            프레임 수 + 누를 버튼 (- 는 없음)
5 D R            아래+오른쪽을 5프레임
!태그            램·화면 덤프
!w 태그          관찰 오프셋만 CSV 로 (가볍다)
!save 파일       상태 저장
!load 파일       상태 복원
```
버튼: `U D L R A B X Y ST SE` — **NGP A(펀치) = 레트로 B**, NGP B(킥) = 레트로 A.

## overlay-editor.html

레트로아크 온스크린 오버레이 배치 편집기. 브라우저로 열면 된다.
버튼을 끌어다 놓으면 `.cfg` 와 오버레이 그림이 나온다.
