# 09. Frame time stamps

Adds a per-frame time stamp, read in the MdigProcess hook and written into the file name, so frames from different cameras and PCs can be matched afterwards.

## What you learn
- Reading the time stamp of a grabbed frame inside the hook.
- Carrying extra data from the hook to the save worker in the queue item.
- Why the CAMERAS table alone is enough to run the same code on one PC or on several.

## Key MIL functions
| Function | Why it is used here |
|---|---|
| `MdigGetHookInfo(HookId, M_TIME_STAMP, &t)` | Time stamp of this frame, as a `MIL_DOUBLE`. |
| `MdigGetHookInfo(HookId, M_MODIFIED_BUFFER + M_BUFFER_ID, &id)` | Buffer that was just grabbed (same as chapter 07). |
| `MbufClone`, `MbufSave`, `MappFileOperation` | Unchanged from chapters 07 and 08. |

## How it works
Baseline = chapters 07/08. The difference is these lines in `timestamp_capture.cpp`:

1. `SaveItem` has a new member `MIL_DOUBLE TimeStamp`.
2. In `ProcessingFunction`: `MdigGetHookInfo(HookId, M_TIME_STAMP, &EventTimeStamp);` and the value is stored in the `SaveItem` pushed on the queue. It has to be read inside the hook, because the hook id is not valid afterwards.
3. `TimeStampedFileName()` builds `<frame>_<camera>_<timestamp>.tiff`, with the time stamp printed with 6 decimals (`0012_03_1234.567890.tiff`). `milcam::FrameFileName` does the first part; the time stamp goes into its `Extension` argument.

What the originals do with the time stamp, and nothing more: it goes into the file name. It is not printed and not compared in code. Matching or timing checks are done by you afterwards, for example by sorting the files of two cameras by time stamp, or subtracting the stamps of frames that belong to the same trigger pulse.

The two originals are the same program except for the camera list: `main_timestamp.cpp` uses two local cameras, the other adds two cameras on a second PC and the remote save and move steps of chapter 08. Here both are one program: for rows with a remote system the worker saves with the `remote:///` prefix and queues the file for `MappFileOperation(..., M_FILE_MOVE, ...)`, which runs after all saves finish.

Shutdown order: `MdigProcess(M_STOP)` on every camera, `SaveQueue.Close()`, join the save threads, then close the move queue and run the move threads.

## Run
Edit `REMOTE_PC` (or delete the remote rows). Build `09_timestamp_capture`, start it, wait for "Camera Ready Done!", feed the external trigger, press a key to stop. Files appear in `C:/output_image/` (remote ones are moved there from the other PC). Per-camera frame statistics are printed at the end.

## Notes
- The original does not say in which unit or relative to which clock the stamp is; it only formats the `double`. The stamps of cameras on different PCs may come from different clocks. Check on your system that frames from one trigger pulse have close stamps across PCs before relying on them.
- Because the stamp is in the name, names are longer and the move step needs the exact same string on both PCs (the code reuses one string for both).

---

## 한국어 요약
- 이 단계는 MdigProcess 훅에서 프레임별 타임스탬프(`M_TIME_STAMP`)를 읽어 파일 이름에 넣는 방법을 다룹니다.
- 기본 구조는 07/08장과 같고, 달라진 부분은 세 가지입니다: `SaveItem`에 `TimeStamp` 추가, 훅 안에서 `MdigGetHookInfo(HookId, M_TIME_STAMP, ...)` 호출, 파일 이름을 `프레임_카메라_타임스탬프.tiff`로 만드는 것.
- 원본은 타임스탬프를 출력하거나 코드에서 비교하지 않고, 파일 이름에만 넣습니다. 카메라/PC 간 매칭은 저장된 파일 이름으로 나중에 확인합니다.
- 로컬 전용 원본과 원격 PC 포함 원본은 카메라 목록만 다르므로 하나의 예제로 합쳤습니다(CAMERAS 표가 로컬/원격을 결정).
- 원격 카메라는 `remote:///` 접두사로 저장한 뒤 `MappFileOperation`으로 호스트로 이동합니다.
- 종료 순서: MdigProcess 정지, 큐 Close, 스레드 join.
- 주의: PC마다 시계가 다를 수 있으므로 PC 간 타임스탬프를 신뢰하기 전에 직접 확인하세요.
