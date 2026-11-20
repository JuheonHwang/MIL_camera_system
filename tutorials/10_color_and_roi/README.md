# 10. Colour camera and region of interest

Two small, independent variations of the capture program: a colour (Bayer) camera, and a cropped sensor window (ROI). Each is its own executable.

## What you learn
- What changes in the pipeline for a colour camera (pixel format and buffer allocation).
- How to select a sensor window with `OffsetX`/`OffsetY` and a smaller `Width`/`Height`, and in which order.

## Key MIL functions
| Function | Why it is used here |
|---|---|
| `MdigControlFeature(..., "PixelFormat", ..., "BayerGB8")` | Colour camera output format (10a). |
| `MdigInquire(M_SIZE_BAND)` + `MbufAllocColor` | Allocate buffers with the right number of bands (10a). |
| `MdigControlFeature(..., "OffsetX"/"OffsetY", ...)` | Position of the window on the sensor (10b). |
| `MdigControl(M_SOURCE_OFFSET_X / _Y)` | Tell the grabber the same offset (10b). |

## How it works
### 10a: `color_capture.cpp` (original: `color single camera.txt`)
Baseline: one-camera version of chapter 07. Only two places differ:
- `PixelFormat` is `"BayerGB8"` instead of `"Mono8"`.
- Every buffer (grab buffers) is allocated with `MbufAllocColor(MilSystem, M_SIZE_BAND, M_SIZE_X, M_SIZE_Y, 8 + M_UNSIGNED, M_IMAGE + M_GRAB, ...)` instead of `MbufAlloc2d(...)`. The band count comes from the digitizer.
The original has no Bayer-conversion call of its own, and this chapter adds none. The saved TIFF has the band layout the digitizer reports; whether that is 1 band (raw mosaic) or 3 depends on your grabber and MIL configuration, so check a saved file. The camera used is local, system `M_DEV2`, digitizer `M_DEV0`.

### 10b: `roi_capture.cpp` (original: `main_offset_있는 버전.cpp`)
Baseline: the four-camera MdigProcess capture. Differences:
- `ROI_WIDTH/ROI_HEIGHT` = 2560 instead of 5120, and `ROI_OFFSET_X/Y` = 1280.
- Order of writes per camera, as in the original: `Height`, `M_SOURCE_SIZE_Y`, `Width`, `M_SOURCE_SIZE_X`, `OffsetX`, `M_SOURCE_OFFSET_X`, `OffsetY`, `M_SOURCE_OFFSET_Y`, `PixelFormat`, `ExposureTime`, `AcquisitionFrameRate`. Size first, offset second, so the offset is never written while the window would exceed the sensor. Keep that order on GenICam cameras.
- Buffers are allocated with the ROI size (2560 x 2560).
Offset plus size must stay within the sensor (1280 + 2560 = 3840 <= 5120). Shutdown order: stop MdigProcess on all cameras, `Close()` the queue, join workers.

## Run
- `10_color_capture`: key to start, wait for the trigger, key to stop. Files `C:/output_image/<frame>_01.tiff`.
- `10_roi_capture`: set `REMOTE_PC`. Same keys. Files `<frame>_<camera>.tiff`; remote frames stay on the remote PC (the original does not move them, see chapter 08).

## Notes
- Original 10b stopped after a busy-wait `delay(5)` instead of a key press, and stopped the save threads before `MdigProcess` stop; here it waits for a key and uses the shutdown order above.
- Original 10b used `M_DEV3` as digitizer on all four systems; kept.
- Original colour file names were unpadded (`7_1.tiff`); here `FrameFileName` gives `0007_01.tiff`.

---

## 한국어 요약
- 컬러 카메라(10a)와 ROI(10b) 두 가지 독립 예제입니다.
- 컬러: PixelFormat을 `BayerGB8`로 바꾸고, 버퍼를 `MbufAllocColor`(밴드 수는 `M_SIZE_BAND`로 조회)로 할당하는 것이 전부입니다. 원본에는 Bayer 변환 호출이 없으며, 저장된 TIFF의 밴드 구성은 그래버/MIL 설정에 따르므로 직접 확인하세요.
- ROI: Width/Height를 2560으로 줄이고 OffsetX/OffsetY 1280을 설정합니다. 순서는 원본 그대로 Height, Width, OffsetX, OffsetY, PixelFormat 순이며, 각 카메라 기능 뒤에 `M_SOURCE_SIZE_*`/`M_SOURCE_OFFSET_*`를 호출합니다.
- 오프셋+크기는 센서(5120) 안에 있어야 합니다. 버퍼도 ROI 크기로 할당합니다.
- 종료 순서: MdigProcess 정지, 큐 Close, 스레드 join.
- 원본의 busy-wait 종료(`delay(5)`)는 키 입력 대기로 바꿨습니다.
