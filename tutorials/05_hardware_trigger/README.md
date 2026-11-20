# 05. Hardware trigger

This step makes the cameras act on an electrical pulse instead of free-running, so that cameras on several grabbers and PCs capture at the same instant. Four small programs show the four ways the originals did it.

## What you learn
- Two places a trigger can act: on the frame grabber (it releases `MdigGrab`) or on the camera (it starts an exposure).
- How a pulse on a grabber AUX input reaches the camera over the CoaXPress link.
- How a grabber timer delays or reshapes the pulse.
- Why `MdigGrab` calls on several cameras should be armed together (one thread per camera).

Images are not saved here. OpenCV `imwrite` and every other save call were removed on purpose: saving belongs to chapter 04, the only chapter that depends on OpenCV. Each program grabs into the buffers, shows them on the displays and prints one line per camera.

## Signal path

```
 pulse source        grabber            CoaXPress           camera
 (generator)         (Rapixo CXP)       link
 -------------       --------------     -------------       ---------------
 pulse ------------> AUX IO 4 input --> M_TL_TRIGGER ---->  LinkTrigger0
                         |                (link trigger)         |
                         |  [example 3: via M_TIMER1]            v
                         |                                  exposure start
                         +--> grab trigger (examples 1, 3, 4) releases MdigGrab
```

Wire the same pulse to AUX IO 4 of every grabber.

## Key MIL functions
| Function | Why it is used here |
|---|---|
| `MdigControl(M_IO_MODE + M_AUX_IO4, M_INPUT)` | turn the AUX pin into an input |
| `MdigControl(M_GRAB_TRIGGER_SOURCE/_ACTIVATION/_STATE)` | grabber holds `MdigGrab` until the pulse |
| `MdigControlFeature("TriggerMode"/"TriggerSource"/...)` | camera exposes on `LinkTrigger0` |
| `MdigControl(M_IO_SOURCE + M_TL_TRIGGER, ...)` | forward a pin or timer to the link trigger |
| `MdigControl(M_TIMER_* + M_TIMER1, ...)` | delay and shape the pulse in the grabber |
| `MdigControl(M_GRAB_MODE, M_SYNCHRONOUS)` | `MdigGrab` waits for the triggered frame |
| `MdispSelect` | show the grabbed buffer |

## Configurations compared
| Program | Who reacts to the pulse | Pulse path | Edge | Grabs |
|---|---|---|---|---|
| `05_grab_trigger` | grabber (camera free-runs) | AUX IO 4 -> grab trigger | falling | sequential |
| `05_grab_trigger`, `ROUTE_VIA_LINK_TRIGGER = true` | grabber | AUX IO 4 -> `M_TL_TRIGGER` -> grab trigger | rising | sequential |
| `05_camera_trigger` | camera | AUX IO 4 -> `M_TL_TRIGGER` -> camera | rising | sequential |
| `05_trigger_timer` | camera + grabber | AUX IO 4 -> `M_TIMER1` -> `M_TL_TRIGGER` -> camera | camera: falling, timer: rising | sequential |
| `05_trigger_threads` | camera + grabber | AUX IO 4 -> `M_TL_TRIGGER` (any edge) -> camera, plus grab trigger | rising | one thread per camera |

## How it works
The camera-side calls below are what `milcam::EnableExternalTrigger` wraps. They are written out in `05_camera_trigger.cpp` because this is where they are taught; later chapters call the helper.

```cpp
MdigControlFeature(d, M_FEATURE_VALUE, MIL_TEXT("TriggerSelector"), M_TYPE_STRING, MIL_TEXT("ExposureStart"));
MdigControlFeature(d, M_FEATURE_VALUE, MIL_TEXT("TriggerMode"), M_TYPE_STRING, MIL_TEXT("On"));
MdigControlFeature(d, M_FEATURE_VALUE, MIL_TEXT("TriggerSource"), M_TYPE_STRING, MIL_TEXT("LinkTrigger0"));
MdigControlFeature(d, M_FEATURE_VALUE, MIL_TEXT("TriggerActivation"), M_TYPE_STRING, MIL_TEXT("RisingEdge"));
MdigControl(d, M_IO_MODE + M_AUX_IO4, M_INPUT);
MdigControl(d, M_TL_TRIGGER_ACTIVATION, M_DEFAULT);
MdigControl(d, M_IO_SOURCE + M_TL_TRIGGER, M_AUX_IO4);
```

Each program has a `CAMERAS` table (two local grabbers, two on a Distributed MIL PC; replace `192.168.0.2`). Remove rows to test with fewer cameras.

## Run
Start the program, then send pulses on AUX IO 4. Example 4 first asks for a key press to arm the threads. When every camera has returned, the images appear on the displays and one "Camera N grabbed a W x H frame." line is printed per camera. Press a key to exit.

## Notes
- Sequential `MdigGrab` calls arm camera N+1 only after camera N returned. With the grab trigger (example 1) camera N+1 may wait for the next pulse; with the camera trigger its frame may come from a later pulse. Example 4 avoids this.
- The original thread experiment called `detach()` after `join()`, which throws. Dropped.
- Originals differ in edge: rising (most files), falling (grab-trigger "trigger & grab" and the timer file's camera setting). Pick the one your pulse source needs.
- The "burst" experiment contains no burst-specific call; it only routes the pulse through `M_TL_TRIGGER` and leaves the camera features untouched, so it is a flag in example 1.
- Two unfinished multi-thread copies (allocating inside each thread, `if (stop)` checked once at start) were dropped; they would not grab.

---

## 한국어 요약

하드웨어 트리거로 여러 카메라를 같은 순간에 촬영하는 방법을 네 가지 프로그램으로 보여 줍니다. 이미지 저장(OpenCV `imwrite` 등)은 04장 내용이므로 여기서는 제거했고, 버퍼에 grab한 뒤 디스플레이에 표시하고 카메라별로 출력만 합니다.

- 신호 경로: 펄스 발생기 -> 그래버 AUX IO 4 -> CoaXPress 링크 트리거(`M_TL_TRIGGER`) -> 카메라 노출 시작.
- `05_grab_trigger`: 카메라는 자유 실행, 그래버가 `MdigGrab`을 펄스까지 대기시킴 (`M_GRAB_TRIGGER_*`). 플래그로 "burst" 실험의 방식(`M_TL_TRIGGER` 경유)도 선택 가능.
- `05_camera_trigger`: 카메라 기능(`TriggerMode/LinkTrigger0/TriggerActivation`)과 AUX 입력 -> `M_TL_TRIGGER` 라우팅. 이 호출들이 `milcam::EnableExternalTrigger`가 감싸는 내용이며 이 장에서는 직접 풀어 썼습니다.
- `05_trigger_timer`: 그래버 타이머(`M_TIMER1`)로 지연/폭을 조절한 펄스를 링크 트리거로 보냅니다.
- `05_trigger_threads`: 카메라마다 스레드 하나로 `MdigGrab`을 동시에 대기시켜 펄스를 놓치지 않게 합니다.
- 주의: 순차 `MdigGrab`은 뒤쪽 카메라가 다음 펄스를 기다리거나 다른 펄스의 프레임을 받을 수 있습니다. 엣지(Rising/Falling)는 원본마다 달랐으므로 펄스 소스에 맞게 선택하세요.
