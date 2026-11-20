# MIL multi-camera capture: a step-by-step tutorial

A synchronized multi-camera capture system built with the Matrox Imaging Library (MIL), written up as a tutorial. It starts with one camera showing live video and ends with ten CoaXPress cameras on five PCs, hardware-triggered, saving 5120 x 5120 frames on each PC and gathering them on the host.

Each chapter adds one idea to the previous one and is a small program you can run on its own.

[한국어 설명은 아래에 있습니다.](#한국어)

## Hardware and software

| | |
|---|---|
| Frame grabber | Matrox Rapixo CXP (CoaXPress 2.0, CXP-12), two boards per PC |
| Cameras | CoaXPress area scan, 5120 x 5120, Mono8 (one Bayer camera in chapter 10) |
| PCs | One host plus up to four remote PCs, linked with Distributed MIL |
| Trigger | External pulse generator wired to an auxiliary input of every grabber |
| Software | MIL with the Rapixo CXP driver and Distributed MIL, Windows, Visual Studio (C++17) |

You need the hardware and a MIL licence to run anything here. The code is still readable as a reference without them.

## Chapters

| # | Chapter | What it adds |
|---|---|---|
| 01 | [Single camera, live view](tutorials/01_single_camera_live) | Application, system, digitizer, display, buffer; camera features; `MdigGrabContinuous` |
| 02 | [Two cameras on one PC](tutorials/02_multi_camera_one_pc) | Several boards and digitizers in one program |
| 03 | [Distributed MIL basics](tutorials/03_distributed_mil_basics) | Cameras on another PC through a `dmiltcp://` system |
| 04 | [Grab and save](tutorials/04_grab_and_save) | `MdigGrab`, copying pixels out with `MbufGet`, saving with OpenCV; introduces `mil_common.h` |
| 05 | [Hardware trigger](tutorials/05_hardware_trigger) | Grab trigger, camera trigger over the CoaXPress link, grabber timer, one grab thread per camera |
| 06 | [MdigProcess with several buffers](tutorials/06_mdigprocess_multibuffer) | Buffer list, hook function, frame statistics, `MbufExport` |
| 07 | [Threaded save queue](tutorials/07_threaded_save_queue) | Clone in the hook, save on a thread pool, `remote:///` saves |
| 08 | [Remote save and move](tutorials/08_remote_save_and_move) | `MappFileOperation` to bring remote files to the host |
| 09 | [Time stamps](tutorials/09_timestamp) | `M_TIME_STAMP` in the hook, written into the file name |
| 10 | [Colour and region of interest](tutorials/10_color_and_roi) | Bayer pixel format with `MbufAllocColor`; `OffsetX`/`OffsetY` |
| 11 | [Serial trigger generator](tutorials/11_serial_trigger_generator) | Switching the pulse generator on and off from the program |
| App | [Multi-camera capture](apps/multi_camera_capture) | The finished ten-camera program: continuous and single-shot variants |

Chapters 01 to 03 use only raw MIL calls. From chapter 04 on, what has already been taught is wrapped in [`common/mil_common.h`](common/mil_common.h) so that each file shows mostly what is new.

## Build

Requirements: Windows, Visual Studio 2019 or later, CMake 3.16 or later, MIL installed.

```
cmake -S . -B build -A x64 -DMIL_ROOT="C:/Program Files/Matrox Imaging/MIL"
cmake --build build --config Release
```

- `MIL_ROOT` is the folder that contains `Include/`, `LIB/` and `DLL/`. The value above is the default.
- Chapter 04 needs OpenCV and is skipped unless you add `-DBUILD_OPENCV_EXAMPLES=ON` (and `-DOpenCV_DIR=...` if CMake cannot find it).
- Each example is its own target, named after its chapter, for instance `07_threaded_save_queue`.
- Visual Studio can also open the folder directly as a CMake project.

## Before you run

- Edit the camera table at the top of the example: remote PC addresses (`192.168.0.x` are placeholders), board numbers and digitizer numbers.
- Remote PCs must be running the Distributed MIL server and be reachable on port 57010. See the MIL documentation for the setup.
- Examples that save write to `C:/output_image/` on each PC. Create that folder first.
- Triggered examples wait for pulses on `M_AUX_IO4` of every grabber.

## Status

This code was written in 2023 for a specific rig and reorganized into tutorial form in 2026. The reorganization was done without access to the hardware: the MIL calls were carried over unchanged from the working originals, but the restructured programs have not been rebuilt against MIL or rerun on the rig. Treat them as a well-commented reference and expect to fix small things when you first build.

---

## 한국어

Matrox Imaging Library(MIL)로 만든 다중 카메라 동기 촬영 시스템을 튜토리얼 형태로 정리한 저장소입니다. 카메라 1대의 라이브 뷰에서 시작해, PC 5대에 연결된 CoaXPress 카메라 10대를 하드웨어 트리거로 동시에 촬영하고 5120 x 5120 프레임을 각 PC에 저장한 뒤 호스트로 모으는 프로그램까지 단계별로 진행합니다.

각 챕터는 앞 챕터에 개념 하나를 더한 독립 실행 예제입니다.

### 사용한 장비

- 프레임 그래버: Matrox Rapixo CXP (CoaXPress 2.0, CXP-12), PC당 2장
- 카메라: CoaXPress 에어리어 스캔, 5120 x 5120, Mono8 (10장에는 Bayer 컬러 카메라 1대)
- PC: 호스트 1대와 원격 PC 최대 4대, Distributed MIL로 연결
- 트리거: 외부 펄스 발생기를 모든 그래버의 보조 입력에 연결
- 소프트웨어: MIL(Rapixo CXP 드라이버, Distributed MIL), Windows, Visual Studio (C++17)

### 챕터 구성

| # | 내용 |
|---|---|
| 01 | 카메라 1대 라이브 뷰: 할당 순서, 카메라 기능 설정, `MdigGrabContinuous` |
| 02 | 한 PC에서 카메라 2대 |
| 03 | Distributed MIL: `dmiltcp://`로 다른 PC의 카메라 사용 |
| 04 | `MdigGrab`으로 촬영하고 `MbufGet` + OpenCV로 저장, 공통 헤더 `mil_common.h` 소개 |
| 05 | 하드웨어 트리거: 그랩 트리거, CoaXPress 링크를 통한 카메라 트리거, 타이머, 카메라별 스레드 |
| 06 | `MdigProcess`와 다중 버퍼, 훅 함수, 프레임 통계 |
| 07 | 훅에서는 복제만 하고 스레드 풀에서 저장, 원격 PC에 직접 저장(`remote:///`) |
| 08 | `MappFileOperation`으로 원격 PC의 파일을 호스트로 이동 |
| 09 | 훅에서 타임스탬프를 읽어 파일 이름에 기록 |
| 10 | 컬러(Bayer) 카메라, 관심 영역(ROI) 설정 |
| 11 | 시리얼 포트로 트리거 발생기 제어 |
| App | 카메라 10대 완성 프로그램 (연속 촬영 / Enter 단일 촬영) |

01~03장은 MIL 호출을 그대로 보여 주고, 04장부터는 이미 배운 부분을 [`common/mil_common.h`](common/mil_common.h)로 감싸서 각 파일에 새로 나오는 내용만 남겼습니다. 각 챕터 폴더의 README에 한국어 요약이 있습니다.

### 빌드

```
cmake -S . -B build -A x64 -DMIL_ROOT="C:/Program Files/Matrox Imaging/MIL"
cmake --build build --config Release
```

04장은 OpenCV가 필요하며 `-DBUILD_OPENCV_EXAMPLES=ON`을 줄 때만 빌드됩니다.

### 실행 전 확인

- 각 예제 상단의 카메라 표에서 원격 PC 주소(`192.168.0.x`는 예시 값), 보드 번호, 디지타이저 번호를 실제 구성에 맞게 고치세요.
- 원격 PC에서 Distributed MIL 서버가 실행 중이어야 합니다 (포트 57010).
- 저장 예제는 각 PC의 `C:/output_image/`에 파일을 씁니다. 폴더를 미리 만들어 두세요.

### 상태

2023년에 특정 장비 구성에 맞춰 작성한 코드를 2026년에 튜토리얼 형태로 재구성했습니다. 재구성은 장비 없이 진행했습니다. MIL 호출은 동작하던 원본에서 그대로 옮겼지만, 재구성한 프로그램을 MIL로 다시 빌드하거나 장비에서 다시 실행해 보지는 않았습니다. 처음 빌드할 때 사소한 수정이 필요할 수 있습니다.
