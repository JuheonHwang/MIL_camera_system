# 11. Driving the trigger generator from the program

The capture pipeline of chapter 07, with the external trigger generator switched on and off over a serial port, so one key press produces a fixed, repeatable burst of frames.

## What you learn

- How to open and configure a COM port with the Win32 API.
- Why the generator is silenced before `MdigProcess` starts: every camera then sees the same first pulse.
- How to time the save backlog (from the last pulse until every frame is on disk).

## Key functions

Nothing here is new on the MIL side. The new calls are Win32:

| Function | Why it is used here |
|---|---|
| `CreateFileA("COM4", ...)` | Opens the serial port of the trigger generator |
| `SetCommTimeouts` | Blocking writes, non-blocking reads |
| `GetCommState` / `SetCommState` | 115200 baud, 8 data bits, one stop bit |
| `SetupComm`, `PurgeComm` | Buffer sizes; discard anything left in the buffers |
| `WriteFile` | Sends one ASCII command |
| `CloseHandle` | Releases the port |

## How it works

```
PC --serial--> trigger generator --pulses--> AUX input of every grabber
                                             --CoaXPress link trigger--> cameras
```

1. Allocate and configure the cameras for external trigger (chapters 05 to 07).
2. Open the port and send `chmd 0`, `rfq 30`, `ode`: mode, 30 Hz, output off.
3. Start the save workers and `MdigProcess`. Nothing is grabbed yet, because no pulse arrives.
4. On a key press send `oen`, wait `PULSE_TIME_MS`, send `ode`.
5. Stop `MdigProcess`, close the queue, join the workers, print the elapsed time and statistics.

With 30 Hz and 1000 ms you should get about 30 frames per camera.

## Run

Set `TRIGGER_PORT`, `REMOTE_PC` and the camera table at the top of the file. Start the program, wait for the prompt, press a key. Files are written to `C:/output_image/FFFF_CC.tiff` on the PC that owns each camera; this example does not move them to the host (see chapter 08).

## Notes

- The command set (`chmd`, `rfq`, `oen`, `ode`) belongs to the generator used in this project. The meanings given in the source are inferred from how the original program used them. Check the manual of your device.
- The original compared the result of `CreateFile` with `NULL`. A failed `CreateFile` returns `INVALID_HANDLE_VALUE`, so that check never fired. It is corrected here.
- `rfq` must match what the cameras can do: the pulse period has to be longer than the exposure time.
- The original of this program did not set `M_MODIFIED_BUFFER_HOOK_MODE` to `M_MULTI_THREAD`, and started `MdigProcess` with `M_DEFAULT` instead of `M_TRIGGER_FOR_FIRST_GRAB`. Both are kept.

---

## 한국어 요약

07장의 캡처 파이프라인에 **시리얼 포트로 외부 트리거 발생기를 제어**하는 부분을 더한 예제입니다. 키를 한 번 누르면 정해진 시간 동안만 펄스가 나가므로 매번 같은 수의 프레임을 얻을 수 있습니다.

- MIL 쪽에 새로운 내용은 없고, Win32 시리얼 API(`CreateFileA`, `SetCommState`, `WriteFile` 등)가 새로 나옵니다.
- 순서: 카메라 설정 → 포트 열고 `chmd 0`, `rfq 30`, `ode`(출력 끔) 전송 → `MdigProcess` 시작(펄스가 없으므로 대기) → 키 입력 시 `oen` → `PULSE_TIME_MS` 대기 → `ode` → 정지, 저장 완료까지 걸린 시간 출력.
- 발생기를 먼저 꺼 두고 `MdigProcess`를 시작하는 이유는 모든 카메라가 같은 첫 펄스를 보게 하기 위해서입니다.
- 명령어(`chmd`, `rfq`, `oen`, `ode`)는 이 프로젝트에서 쓴 발생기의 것이고, 의미는 원본 코드의 사용 방식에서 추정한 것입니다. 사용하는 장비의 매뉴얼을 확인하세요.
- 원본은 `CreateFile` 실패를 `NULL`과 비교했는데 실제 실패 값은 `INVALID_HANDLE_VALUE`라서 수정했습니다.
- 파일은 각 카메라가 연결된 PC의 `C:/output_image/`에 저장되며, 호스트로 옮기지는 않습니다(08장 참고).
