# 07. Threaded save queue

Keeps the `MdigProcess` hook fast: the hook only clones the grabbed buffer and queues it, and a pool of worker threads does the slow `MbufSave`.

## What you learn
- Why saving inside the hook loses frames.
- Clone in the hook, save in a worker, free after saving.
- Saving on the PC that owns the buffer (`remote:///` prefix for Distributed MIL).
- A safe shutdown order with a closable queue.

## Key MIL functions
| function | why it is used here |
|---|---|
| `MdigProcess` | grabs into a ring of `N_FRAME` buffers per camera and calls the hook |
| `MdigGetHookInfo` | gets the buffer the hook was called for |
| `MbufClone` + `M_COPY_SOURCE_DATA` | private copy so the grab buffer can be reused immediately |
| `MbufSave` | writes the TIFF, in a worker thread |
| `MbufFree` | frees the clone after saving |
| `MsysControl` + `M_MODIFIED_BUFFER_HOOK_MODE` | lets hooks of one system run on several threads |

## How it works
The program grew in four steps:
1. Save in the hook. Simple, but `MbufSave` of a 5120 x 5120 image takes longer than a frame period, so the hook falls behind and frames are missed.
2. One worker thread with a queue. The hook pushed the grab buffer itself. But that buffer is handed back to `MdigProcess` and overwritten while it waits in the queue, so saved images could be wrong.
3. Clone first. The hook clones the buffer and pushes the clone, which is safe. One worker still cannot write four cameras at 90 fps.
4. A pool of workers (`N_THREAD = 12`) sharing one queue. This is the version here.

```cpp
// hook
MbufClone(GrabbedBuffer, ..., M_COPY_SOURCE_DATA, &Clone);
Data->Queue->Push(SaveJob{ Clone, Data->Remote, FrameIndex, Data->CameraIndex });

// worker
while (Queue.Pop(Job)) { MbufSave(FileName, Job.Image); MbufFree(Job.Image); }
```

Shutdown order: (1) `MdigProcess(... M_STOP ...)` for every camera, so no hook can push any more; (2) `SaveQueue.Close()`; (3) join the workers, which first drain what is left. Workers are started before `M_START`; they just block in `Pop()` until the first clone arrives.

## Run
Press a key to start, a key again to stop. Each PC writes `C:/output_image/FFFF_CC.tiff` (frame, camera) to its own disk: the two local cameras on the host, the two remote cameras on the remote PC. At the end the frame count, rate and missed frames per camera are printed. Copying remote files to the host is chapter 08.

## Notes
- The originals checked `save_queue.empty()` without holding the mutex while several workers popped. Two workers could both see one item, and `front()`/`pop()` on an empty queue is undefined behaviour. They also waited with `Sleep(500)` polling. `milcam::WorkQueue` locks around check and pop and blocks on a condition variable.
- The originals joined the workers before stopping `MdigProcess`, so late hooks could push clones nobody saved (leaked buffers). Stop first, then close, then join.
- Clones are 25 MB each. If saving is slower than grabbing, the queue and memory grow without bound.

---

## 한국어 요약
`MdigProcess` 훅 안에서 `MbufSave`를 하면 5120x5120 이미지 저장이 프레임 주기보다 느려 프레임을 놓칩니다. 그래서 훅에서는 `MbufClone`(`M_COPY_SOURCE_DATA`)으로 복사본만 만들어 큐에 넣고, 워커 스레드 풀(12개)이 꺼내서 `MbufSave` 후 `MbufFree` 합니다.
- 진행 과정: 훅에서 직접 저장 -> 스레드 1개 + 큐(그랩 버퍼를 그대로 넣어 덮어쓰기 문제) -> 복제 후 큐 -> 스레드 풀.
- 원격(Distributed MIL) 카메라는 파일명 앞에 `remote:///`를 붙여 원격 PC 디스크에 저장합니다. 호스트로 옮기는 것은 08장입니다.
- 종료 순서: 모든 카메라 `MdigProcess` M_STOP -> 큐 `Close()` -> 워커 join.
- 원본은 락 없이 `queue.empty()`를 검사해 여러 워커가 동시에 pop 하는 경쟁 상태가 있었고, `Sleep(500)` 폴링을 썼습니다. `milcam::WorkQueue`가 이를 해결합니다.
- 실행: 아무 키로 시작/중지, 각 PC의 `C:/output_image/FFFF_CC.tiff`에 저장됩니다.
