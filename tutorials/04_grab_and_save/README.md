# 04. Grab and save

Grabs frames from four cameras (two boards on this PC, two on a remote PC through Distributed MIL) with plain `MdigGrab` and writes them to disk with OpenCV. It is also the first chapter that uses `common/mil_common.h`.

## What you learn
- Grabbing without a trigger: `MdigGrab` blocks until one frame is in the buffer.
- Copying pixel data out of a MIL buffer into your own memory with `MbufGet`.
- Saving a frame as PNG through `cv::imwrite`.
- The `milcam::` helpers in `mil_common.h`.

## The helper header
`common/mil_common.h` wraps what chapters 01-03 showed as raw calls, and nothing else:
`milcam::AllocSystem` / `AllocDigitizer` (MsysAlloc, MdigAlloc), `AllocMono8Buffer` (MbufAlloc2d sized from the digitizer, then MbufClear), `ApplyCameraSettings` (the MdigControlFeature writes for size, pixel format, exposure, frame rate, gain), `RemoteRapixoCxp` (the `dmiltcp://` descriptor) and `FrameFileName`. Calls that are new in a chapter stay written out in that chapter's source.

## Key MIL functions
| Function | Why it is used here |
|---|---|
| `MdigGrab` | Grab one frame into a buffer, no trigger |
| `MbufGet` | Copy the buffer's pixels into a plain memory block (the `cv::Mat` data) |
| `MdigControlFeature` | Set `TriggerMode` to `Off` so the camera free-runs |

## How it works
1. Allocate the four systems and digitizers, apply the camera settings, allocate one grab buffer per camera on that camera's own system.
2. For each frame: `MdigGrab` on every camera, `MbufGet` into the camera's `cv::Mat`, `cv::imwrite`.

```cpp
CvImage[i].create(cv::Size((int)Settings.Width, (int)Settings.Height), CV_8U);
...
MbufGet(MilImage[i], CvImage[i].data);
```

The originals do not wrap the MIL host address in a `cv::Mat`. They create a `cv::Mat` (`CV_8U`, 5120 x 5120) and let `MbufGet` copy the pixels into its data block. That is one full copy per frame, but it needs no host address and works for the remote buffers.

## Run
Needs OpenCV: configure with `-DBUILD_OPENCV_EXAMPLES=ON`. Nothing to press. The program grabs `NUM_FRAMES` (5) frames per camera and exits. Files go to `C:/output_image/` as `<frame>_<camera>.png`, e.g. `0003_02.png` is frame 3 of the third camera. Camera numbers follow the order of the `CAMERAS` table.

## Notes
- This is the only example that depends on OpenCV. From chapters 06/07 on the repo saves with MIL's own functions. The three original files for this step give no reason for the switch, so none is claimed here.
- The three originals grab each camera once. The loop over frames and the file names are added for this tutorial. They name files `1_1.png`/`01_1.png` (picking the next number by scanning the folder) or `155_DEV0.bmp`; this version overwrites files of an earlier run with the same names.
- The earliest original has no `TriggerMode Off` lines; the later ones add them. They are kept because a camera left in trigger mode would make `MdigGrab` wait for a pulse.
- The originals allocate a display per camera but never select an image on it; it is left out here.

---

## 한국어 요약
4대의 카메라(이 PC의 보드 2개, Distributed MIL로 연결된 원격 PC의 보드 2개)에서 트리거 없이 `MdigGrab`으로 프레임을 받아 OpenCV로 저장하는 단계입니다. `common/mil_common.h`를 처음 사용하는 챕터이기도 합니다.

- `mil_common.h`의 `milcam::` 함수는 01-03장에서 직접 호출로 보여준 할당, 카메라 설정, 버퍼 생성, 원격 시스템 주소 생성, 파일 이름 생성만 감쌉니다. 새로 배우는 호출은 각 챕터 소스에 그대로 적습니다.
- `MdigGrab`은 프레임 한 장이 버퍼에 들어올 때까지 대기합니다. `TriggerMode`를 `Off`로 두어 카메라가 자유 실행하게 합니다.
- 원본은 MIL 버퍼의 호스트 주소를 `cv::Mat`으로 감싸지 않고, `cv::Mat`을 만든 뒤 `MbufGet`으로 픽셀을 복사해 넣습니다. 호스트 주소가 필요 없어 원격 PC의 버퍼에도 동작합니다.
- 실행: `-DBUILD_OPENCV_EXAMPLES=ON`으로 설정하고 실행하면 카메라당 5프레임을 받아 `C:/output_image/<프레임>_<카메라>.png`로 저장하고 종료합니다.
- 이 예제가 저장소에서 OpenCV에 의존하는 유일한 예제이며, 06/07장부터는 MIL 자체 함수로 저장합니다. 원본 3개에는 그 이유를 보여주는 근거가 없어 추측하지 않습니다.
- 원본은 카메라당 한 번만 grab하며, 프레임 반복과 파일 이름 규칙은 튜토리얼용으로 추가했습니다.
