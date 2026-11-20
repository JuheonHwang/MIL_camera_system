# 06. MdigProcess with multiple grab buffers

Replaces the `MdigGrab` loop with `MdigProcess`: MIL grabs into a list of buffers and calls a hook function for every frame.

## What you learn
- Giving a digitizer a list of buffers and letting MIL keep grabbing.
- Writing a hook function and passing it a per-camera user data struct.
- Starting and stopping with `M_START` / `M_STOP`, and reading the statistics afterwards.
- Why saving inside the hook is a problem at 5120x5120.

## Key MIL functions
| Function | Why it is used here |
|---|---|
| `MdigProcess(..., M_START, M_TRIGGER_FOR_FIRST_GRAB, hook, &data)` | start continuous grabbing into the buffer list; first frame waits for the trigger |
| `MdigProcess(..., M_STOP, ...)` | stop it; same buffers and hook are passed again |
| `MdigGetHookInfo(HookId, M_MODIFIED_BUFFER + M_BUFFER_ID, &id)` | in the hook: the buffer that holds the new frame |
| `MsysControl(sys, M_MODIFIED_BUFFER_HOOK_MODE, M_MULTI_THREAD)` | hooks of this system run on their own thread |
| `MdigControl(dig, M_GRAB_FRAME_MISSED_RESET, M_DEFAULT)` | zero the missed-frame counter after start |
| `MdigInquire(dig, M_PROCESS_FRAME_COUNT / _MISSED / _RATE, ...)` | frames grabbed, frames missed, measured frame rate |
| `MbufExport(name, M_PNG, buffer)` | save a frame to PNG (replaces OpenCV `imwrite`) |

## How it works
`NUM_GRAB_BUFFERS` buffers are allocated per camera (`M_IMAGE + M_GRAB`) and handed to `MdigProcess`. Each camera has its own `HookData` (frame counter, camera number, output folder); MIL passes it back to the hook as `HookDataPtr`.

The hook is short: count, ask for the filled buffer, optionally save it.

```cpp
MdigGetHookInfo(HookId, M_MODIFIED_BUFFER + M_BUFFER_ID, &ModifiedBuffer);
MbufExport(FileName, M_PNG, ModifiedBuffer);
```

Single vs multi buffer: the original single-buffer and multi-buffer programs differ only in the buffer count (1 vs 15). Change `NUM_GRAB_BUFFERS` to compare. With one buffer, MIL has no free buffer to grab the next frame into until the hook returns.

Trigger: `milcam::EnableExternalTrigger(dig)` is used as in chapter 05. The originals use `RisingEdge` on `M_AUX_IO4`, which are the helper's defaults, so no argument is passed. None of the four originals uses `FallingEdge`.

## Run
1. Start the program, press a key to start `MdigProcess`.
2. Feed trigger pulses to the grabbers. With `SAVE_IN_HOOK = true`, files `0001_01.png`, `0001_02.png`, ... (frame, camera) appear in `C:/output_image/`.
3. Press a key to stop. One line per camera is printed: frames grabbed, frame rate, frames missed.

## Notes
- Saving in the hook is the weak point. A 5120x5120 Mono8 frame is about 26 MB, and PNG encoding runs inside the hook. While the hook is busy that buffer is not available to the grabber; if the hook is slower than the trigger rate, the buffers fill up and `M_PROCESS_FRAME_MISSED` goes up. More buffers (15) only delay this, they do not remove it. The originals contain no recorded timings: one variant (`cap imgs multi buf with print`) turns the saving off, prints `clock()` in the hook and prints the three statistics, which is how the author checked it. Set `SAVE_IN_HOOK = false` and compare the missed count with and without saving on your own machine. Chapter 07 moves the saving out of the hook into worker threads.
- The original multi buffer program with OpenCV copied the frame with `MbufGet` into a `cv::Mat` and used `imwrite`; the `MbufExport` variant did the same job without OpenCV. Both save inside the hook and have the same problem.
- The original file index continued after the highest file already in the folder. This chapter always starts at 1, so a second run overwrites the first.
- Of the four originals only the "print" one sets `M_MODIFIED_BUFFER_HOOK_MODE`; the other three leave it at the MIL default. This chapter sets it for all cameras.

---

## 한국어 요약

`MdigGrab` 반복 대신 `MdigProcess`를 사용합니다. 카메라마다 그랩 버퍼 목록을 주면 MIL이 연속으로 그랩하고, 프레임마다 훅 함수를 호출합니다.

- 훅에서는 `MdigGetHookInfo(M_MODIFIED_BUFFER + M_BUFFER_ID)`로 방금 채워진 버퍼를 얻습니다. 사용자 데이터 구조체는 카메라별로 하나씩 `MdigProcess`에 넘깁니다.
- `M_START` / `M_STOP`으로 시작/중지하고, 첫 프레임은 `M_TRIGGER_FOR_FIRST_GRAB`로 하드웨어 트리거를 기다립니다.
- `M_MODIFIED_BUFFER_HOOK_MODE = M_MULTI_THREAD`를 시스템에 설정해 훅이 별도 스레드에서 실행됩니다.
- 중지 후 `M_PROCESS_FRAME_COUNT`, `M_PROCESS_FRAME_MISSED`, `M_PROCESS_FRAME_RATE`를 읽어 출력합니다.
- 단일 버퍼 / 다중 버퍼 원본은 버퍼 개수(1 대 15)만 다릅니다. `NUM_GRAB_BUFFERS` 상수로 비교하세요.
- 저장은 OpenCV 대신 `MbufExport`를 사용합니다.
- 5120x5120 프레임을 훅 안에서 저장하면 훅이 오래 걸려 버퍼가 반환되지 않고, 트리거 속도를 따라가지 못하면 놓친 프레임이 늘어납니다. 원본에는 측정값 기록이 없으므로 `SAVE_IN_HOOK`을 바꿔가며 직접 확인하세요. 07장에서 저장을 별도 스레드로 옮깁니다.
