# Multi-camera capture application

Ten CoaXPress cameras on five PCs (two Rapixo CXP boards per PC), exposed by one hardware trigger, saved on each PC by a thread pool, and gathered on the host at the end. This is the finished program that tutorials 05 to 08 build up to.

## What you learn
- How the pieces of the tutorials fit together: trigger, `MdigProcess`, save queue, remote save, move.
- How to split a program into shared parts (`capture_common.h`) and two small front ends.
- The shutdown order that makes `WorkQueue` workers finish cleanly.

## Topology

```
 Host PC (runs the program)           Remote PCs (Distributed MIL servers, port 57010)
 +-----------------------+   TCP    +-------------------+  +-------------------+
 | board DEV0 -> cam  1  |<-------->| PC A  (192.168.0.2)|  | PC B  (192.168.0.3)|
 | board DEV1 -> cam  2  |          |  DEV0 -> cam  3    |  |  DEV1 -> cam  5    |
 +-----------------------+          |  DEV1 -> cam  4    |  |  DEV0 -> cam  6    |
                                    +-------------------+  +-------------------+
                                    | PC C  (192.168.0.4)|  | PC D  (192.168.0.5)|
                                    |  DEV0 -> cam  7    |  |  DEV0 -> cam  9    |
                                    |  DEV1 -> cam  8    |  |  DEV1 -> cam 10    |
                                    +-------------------+  +-------------------+
 Every board has one camera on digitizer DEV0. The same trigger pulse goes to all ten boards.
```

The original source masks the four remote addresses, so which remote entries share a PC is an assumption: consecutive pairs (cams 3+4, 5+6, 7+8, 9+10), one DEV0 and one DEV1 per PC. Edit `CAMERAS` in `capture_common.h` to match your wiring. Addresses are placeholders.

## Key MIL functions
| Function | Why it is used here |
|---|---|
| `MdigProcess` | Continuous multi-buffer grab with a hook (continuous variant) |
| `MdigGrab` | One frame per call (single-shot variant) |
| `MbufClone` | Copies a frame into host memory on the camera's own PC |
| `MbufSave` | Writes the TIFF; `remote:///` prefix saves on the remote PC |
| `MappFileOperation` (`M_FILE_MOVE`) | Moves remote files to the host |
| `MdigInquire` (`M_PROCESS_FRAME_*`) | Per-camera frame count, rate, missed frames |

## How it works
1. `capture_common.h` holds the camera table, per-camera state, setup and release steps, `SaveJob`/`MoveJob` and the two workers.
2. Frames are cloned and pushed to a save queue. `SaveWorker` writes local frames directly. For remote frames it saves on the remote PC and queues a `MoveJob`.
3. Shutdown: stop grabbing, `SaveQueue.Close()`, join save workers, `MoveQueue.Close()`, run and join move workers (time is printed), print statistics, free everything in reverse order.

| | continuous | single shot |
|---|---|---|
| Target | `multi_camera_continuous_capture` | `multi_camera_single_shot_capture` |
| Grab | `MdigProcess`, 60 buffers per camera | `MdigGrab` in one thread per camera, 10-buffer ring |
| Exposure / rate | 60000 us / 15 fps | 999500 us / 1 fps |
| Threads | 8 save, 8 move | 4 save, 12 move |

## Run
- Start the Distributed MIL server on the four remote PCs first.
- Continuous: any key starts, any key stops. Single shot: Enter opens a one-second grab window, Q quits.
- Files are `C:/output_image/FFFF_CC.tiff` (frame, camera 01 to 10, e.g. `0001_01.tiff`) on the host after the move step.

## Notes
- Single shot: a camera that receives two trigger pulses within the one-second window grabs two frames, so its frame numbers drift from the other cameras. Send one pulse per Enter.
- The originals allocated a display and a displayable image per camera but never showed them; they are dropped here.

---

## 한국어 요약

PC 5대(PC당 Rapixo CXP 보드 2장)에 연결된 CoaXPress 카메라 10대를 하드웨어 트리거로 동시에 촬영하고, 각 PC에서 스레드 풀로 저장한 뒤 호스트로 파일을 모으는 완성 애플리케이션입니다.

- `continuous_capture.cpp`: `MdigProcess`로 연속 촬영. 아무 키나 누르면 시작, 다시 누르면 종료.
- `single_shot_capture.cpp`: Enter를 누를 때마다 카메라 10대가 `MdigGrab`으로 한 장씩 촬영, Q로 종료.
- 공통 코드(카메라 표, 저장/이동 작업, 워커)는 `capture_common.h`에 있습니다.
- 원본에서 원격 PC 주소가 가려져 있어 연속된 두 항목이 한 PC라고 가정했습니다. 실제 배선에 맞게 `CAMERAS`를 수정하세요.
- 종료 순서: 촬영 중지, 저장 큐 닫기, 저장 워커 join, 이동 큐 닫기, 이동 워커 실행, 통계 출력, 역순 해제.
- 파일 이름은 `프레임4자리_카메라2자리.tiff` (예: `0001_01.tiff`), 카메라 번호는 1부터 시작합니다.
- 단일 촬영: 1초 창 안에 트리거가 두 번 들어오면 한 카메라가 두 장을 찍어 프레임 번호가 어긋날 수 있습니다.
- 사용하지 않던 디스플레이와 표시용 이미지는 제거했습니다.
