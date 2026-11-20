# 02. Two cameras on one PC

Chapter 01 for two cameras: two Rapixo CXP boards, one camera each, written as arrays and loops instead of copy-pasted lines.

## What you learn
- Each board is a separate MIL system; one `MsysAlloc` per board.
- Display, digitizer and buffer belong to one system and are allocated on the system of their camera.
- Describing the cameras in a small table and looping over it.

## Key MIL functions
| Function | Why it is used here |
|---|---|
| `MsysAlloc` | Called once per board (`M_DEV0`, `M_DEV1`). |
| `MdispAlloc`, `MdigAlloc`, `MbufAlloc2d` | Called per camera, on that camera's system. |
| `MdigControlFeature`, `MdigControl` | Same settings as chapter 01, written to every digitizer. |
| `MdigInquireFeature` | Adds `DeviceSerialNumber`, useful to tell the cameras apart. |
| `MsysInquire` (`M_NUMBER`) | Which board a camera is on. |
| `MdigGrabContinuous`, `MdigHalt` | Started for each camera back to back; both run together. |

## How it works
```cpp
static const CameraSource CAMERAS[] = {
	{ M_DEV0, M_DEV3 },   // first board
	{ M_DEV1, M_DEV3 },   // second board
};
```
All MIL ids become arrays of the same length (`MilSystem[i]`, `MilDisplay[i]`, ...). Allocation, configuration, readback, buffer creation and grab each run in a loop over `i`. Only one `MappAlloc` is needed. Free in reverse: buffers, digitizers and displays, systems, application.

## Run
Press any key to stop the live view of both cameras. Two windows open, one per camera. No files are written. Add a line to `CAMERAS` for a third camera; nothing else changes.

## Notes
- In the original, `Height` and `Width` were written as camera features only on the first digitizer; the second one only got `M_SOURCE_SIZE_X/Y`. The loop now writes them to both.
- The original never freed the digitizers; they are freed here.

---

## 한국어 요약
한 PC의 보드 2장(각 보드에 카메라 1대)으로 라이브 영상을 보는 예제입니다. 01장 코드를 배열과 반복문으로 정리했습니다.

- 보드마다 별도의 MIL System이므로 `MsysAlloc`을 보드 수만큼 호출합니다.
- Display, Digitizer, Buffer는 해당 카메라가 연결된 System에서 할당해야 합니다.
- `CAMERAS` 표에 (보드 번호, 디지타이저 번호)를 적고 반복문으로 처리합니다. 카메라를 추가하려면 표에 한 줄만 추가하면 됩니다.
- `MdigGrabContinuous`는 즉시 반환하므로 두 카메라가 동시에 동작합니다.
- 실행: 창 2개가 열리고, 아무 키나 누르면 종료합니다. 파일은 저장하지 않습니다.
- 원본에서 두 번째 카메라에는 Height/Width 카메라 기능이 쓰이지 않았고 Digitizer도 해제되지 않았는데, 둘 다 수정했습니다.
