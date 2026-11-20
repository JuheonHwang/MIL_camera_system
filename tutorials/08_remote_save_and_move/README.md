# 08. Remote save and move to host

Cameras on remote PCs save their frames to the remote PC's own disk during capture. After capture, a pool of worker threads moves those files to the host with `MappFileOperation`.

## What you learn
- Why remote cameras save locally first: a 5120x5120 frame is 25 MB, and sending it over the network at capture rate loses frames.
- How to find the application that owns a remote system, and use it as the source of a file move.
- How to chain two work queues (save, then move) and shut them down in the right order.

## Key MIL functions
| function | why it is used here |
|---|---|
| `MsysInquire(system, M_OWNER_APPLICATION, M_NULL)` | id of the application that owns the system; for a remote system, the application on that PC |
| `MappFileOperation(srcApp, src, dstApp, dst, M_FILE_MOVE, M_DEFAULT, M_NULL)` | moves a file between two applications' computers |
| `MbufSave("remote:///...", buffer)` | (from chapter 07) writes the file on the PC that owns the buffer |
| `MbufClone`, `MdigProcess` | (from chapters 06-07) copy the frame in the hook, and grab |

## How it works
```
hook --clone--> save queue --12 save workers--> MbufSave
                                   |  (remote cameras only)
                                   +--> move queue --8 move workers--> MappFileOperation
```
- Each camera stores `OwnerApp` and a `Remote` flag in its hook data.
- A save worker saves with the `remote:///` prefix, frees the clone, and pushes `{OwnerApp, FileName}` on the move queue. The file name has no prefix: it is the path as seen on the remote PC.
- Move workers run only after capture, so the network is not shared with the grab:
  ```cpp
  MappFileOperation(Item.RemoteApp, Item.FileName, HostApp, Item.FileName, M_FILE_MOVE, M_DEFAULT, M_NULL);
  ```
- Shutdown order: stop `MdigProcess` on all cameras, `Close()` the save queue, join the save workers, then `Close()` the move queue and start and join the move workers. The save workers are the producers of the move queue, so it is complete only after they are joined. Closing a queue is what lets blocked `Pop()` calls return.
- The time spent in the move phase is measured with `std::chrono::steady_clock` and printed.

## Run
1. Start the Distributed MIL server on the remote PC and set `REMOTE_PC` in the file.
2. Make sure `C:/output_image/` exists on every PC.
3. Press a key to start, send trigger pulses, press a key to stop.
4. The program prints `Gathering Images...`, then `Move took N.NN seconds`.
5. All frames end up in `C:/output_image/` on the host, named `<frame>_<camera>.tiff`.

## Notes
- File names now come from `milcam::FrameFileName` (`0012_03.tiff`). The originals wrote `12_3.tiff` because they formatted a zero-padded string and then ran `stoi` on it, which removed the padding.
- One original variant (`thread로 remote image move.txt`) passes `M_NULL` as the destination file name of `MappFileOperation`. The other variants pass the same name as the source. This chapter uses the explicit name.
- Earlier attempts, kept here for the lesson:
  - Move once by hand after capture, for two files only (`MappFileOperation` example).
  - Push a move item from inside the `MdigProcess` hook and move with one thread while grabbing. The thread polled `std::queue` without a proper wait and slept 500 ms, and the move ran while grabbing.
  - Save and move with polling threads. The move threads exited as soon as `move_queue.empty()`, which only works because they start after the save threads end.
- `MfuncAlloc` was tried as a way to run the save on the remote PC. It is not included because it was never finished:
  ```cpp
  MfuncAlloc(MIL_TEXT("SaveGrabbedImage"), 2, SlaveSaveGrabbedImage,
             M_NULL, M_NULL, M_USER_FUNCTION + 1, M_DEFAULT, &Func);
  MfuncParamMilId(Func, 1, GrabbedImage, M_IMAGE, M_IN);
  MfuncParamFilename(Func, 2, file_name.c_str(), file_name.length()+1, M_IN);
  MfuncCall(Func);  MfuncFree(Func);
  ```
  The slave function reads the parameters with `MfuncParamValue`, wraps `MbufSave` in `MbufControl(..., M_LOCK / M_UNLOCK, ...)`. The function is a plain local function pointer, with no library for the remote PC to load. The `remote:///` prefix is still used in the file name, so it is unclear which PC actually saved the file. The file also has an unused `opencv` include, a commented-out alternative (`MfuncParamMilText`), and nothing that shows it beat `MbufSave` with `remote:///`. The final design dropped it.

---

## 한국어 요약

원격 PC에 연결된 카메라의 프레임은 캡처 중에 해당 PC의 디스크에 저장하고(`remote:///` 접두어), 캡처가 끝난 뒤 `MappFileOperation(..., M_FILE_MOVE, ...)`로 호스트에 가져옵니다.

- `MsysInquire(system, M_OWNER_APPLICATION, M_NULL)`로 원격 시스템의 애플리케이션 ID를 얻어 파일 이동의 원본으로 사용합니다.
- 구조: 훅에서 `MbufClone` -> 저장 큐 -> 저장 워커 12개 -> (원격 카메라만) 이동 큐 -> 이동 워커 8개.
- 종료 순서: 모든 카메라 `MdigProcess` 정지 -> 저장 큐 `Close()` -> 저장 워커 join -> 이동 큐 `Close()` -> 이동 워커 실행/join. 저장 워커가 이동 큐의 생산자이므로 이 순서를 지켜야 합니다.
- 이동에 걸린 시간을 `steady_clock`으로 측정해 출력합니다.
- 파일 이름은 `milcam::FrameFileName`으로 만들어 `0012_03.tiff` 형식이 됩니다(원본은 `12_3.tiff`).
- `MfuncAlloc`로 원격에서 저장을 실행하려던 시도는 미완성이라 코드는 싣지 않고 Notes에 호출 형태만 남겼습니다.
