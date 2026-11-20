# 01. Single camera, live view

The first complete MIL program: one Rapixo CXP board, one CoaXPress camera, camera settings written and read back, live video until a key is pressed.

## What you learn
- The MIL object hierarchy: application, system, display, digitizer, image buffer.
- Writing camera (GenICam) features with `MdigControlFeature` and reading them with `MdigInquireFeature`.
- Reading grabber-side properties with `MdigInquire`.
- Starting and stopping live grab, and freeing in reverse order.

## Key MIL functions
| Function | Why it is used here |
|---|---|
| `MappAlloc`, `MsysAlloc` | Application, then the Rapixo CXP system (`M_SYSTEM_RAPIXOCXP`, board `M_DEV0`). |
| `MdispAlloc`, `MdigAlloc` | Windowed display, and the digitizer (digitizer `M_DEV3`, default DCF). |
| `MdigControlFeature` | Writes `Height`, `Width`, `PixelFormat`, `ExposureTime`, `AcquisitionFrameRate`. |
| `MdigControl` | `M_SOURCE_SIZE_X/Y`: tells the grabber the image size the camera now sends. |
| `MdigInquireFeature` | Reads `DeviceVendorName`, `DeviceModelName`, `PixelFormat` back from the camera. |
| `MdigInquire` | Image size (to size the buffer) and properties such as `M_SELECTED_FRAME_RATE`. |
| `MbufAlloc2d`, `MbufClear`, `MdispSelect` | 8-bit buffer with `M_IMAGE + M_DISP + M_GRAB`, shown in the display. |
| `MdigGrabContinuous`, `MdigHalt` | Start and stop live grab into the buffer. |

## How it works
Camera features go through the digitizer, and the type constant must match the feature:
```cpp
MIL_DOUBLE ExposureTime = 60000;
MdigControlFeature(MilDigitizer, M_FEATURE_VALUE, MIL_TEXT("ExposureTime"), M_TYPE_DOUBLE, &ExposureTime);
```
`Height` and `Width` are written to the camera and then also to the grabber with `M_SOURCE_SIZE_Y/X`. The frame rate is set to `1e6 / ExposureTime`, rounded down to one decimal (16.6 fps for 60 ms), so the camera is not asked for more than the exposure allows. The buffer is sized from `MdigInquire(M_SIZE_X / M_SIZE_Y)` rather than hard-coded.

## Run
Edit `SYSTEM_NUM` and `DIGITIZER_NUM` at the top if your camera is on another board or connection. The camera description is printed and a live window opens. Press any key to stop; the grabber properties are printed. Press a key again to exit. No files are written.

## Notes
- The original printed values with `MosPrintf(MIL_TEXT("...%d\n", x))`, with the parenthesis in the wrong place, so the value was never printed. Fixed here.
- The digitizer is `M_DEV3` because that is where the camera was connected; change it for your setup.

---

## 한국어 요약
Rapixo CXP 보드 1개와 CoaXPress 카메라 1대로 라이브 영상을 보는 가장 기본적인 예제입니다.

- 객체 순서: Application, System, Display, Digitizer, Image buffer. 해제는 반대 순서입니다.
- `MdigControlFeature`로 Height, Width, PixelFormat, ExposureTime, AcquisitionFrameRate를 쓰고, `MdigInquireFeature`로 제조사/모델명/픽셀 포맷을 읽습니다.
- Height/Width는 `M_SOURCE_SIZE_Y/X`로 프레임 그래버에도 알려 줍니다.
- 프레임 레이트는 `1e6 / ExposureTime`을 소수 첫째 자리까지 내림한 값입니다.
- 버퍼 크기는 `MdigInquire(M_SIZE_X/M_SIZE_Y)`로 구합니다.
- 실행: 라이브 창이 뜨고, 아무 키나 누르면 정지 후 그래버 속성을 출력합니다. 파일은 저장하지 않습니다.
- 원본의 `MosPrintf(MIL_TEXT("...%d\n", x))` 괄호 오류는 수정했습니다.
