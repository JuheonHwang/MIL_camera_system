# 03. Distributed MIL basics

Allocate MIL systems on another PC with a `dmiltcp://` descriptor, check that it works, then view local and remote cameras together.

## What you learn
- A remote system is allocated with the same `MsysAlloc`, using a descriptor string that names the remote PC.
- After that, display, digitizer, buffer and grab code is the same as for local cameras.
- Checking allocation first, and telling local from remote systems.

## Key MIL functions
| Function | Why it is used here |
|---|---|
| `MsysAlloc` | Descriptor `dmiltcp://<ip>:57010/M_SYSTEM_RAPIXOCXP` selects a board on the other PC. |
| `MsysControl` | `M_MODIFIED_BUFFER_HOOK_MODE` set to `M_MULTI_THREAD`, kept from the original allocation check. |
| `MsysInquire` | `M_DISTRIBUTED_MIL_TYPE` equals `M_DMIL_REMOTE` for remote systems. |
| `MdispAlloc`, `MdigAlloc`, `MbufAlloc2d`, `MdigGrabContinuous` | As in chapter 02, called on the remote system for remote cameras. |

## How it works
The descriptor is built from an address and a port:
```cpp
dmiltcp://192.168.0.2:57010/M_SYSTEM_RAPIXOCXP
```
`dmil_allocation_check` allocates four systems (boards 0 and 1 on this PC, boards 0 and 1 on the remote PC), reports each result and stops with a message if one returns `M_NULL`. `dmil_live_view` uses the same table with a digitizer number added and runs the chapter 02 loops over all four cameras.

The remote PC must be running the Distributed MIL server and be reachable on that address and port (57010 is the port used in the original). For the setup itself see the MIL documentation for Distributed MIL setup.

## Run
Set `REMOTE_PC` at the top of both files. Run `03_dmil_allocation_check` first; it prints one line per system and waits for a key. Then run `03_dmil_live_view`: four windows open, press any key to stop. No files are written.

## Notes
- The original also had a variant that allocated and ran each camera in its own `std::thread`. It is not included: the system, display, digitizer and image ids were passed by value so `main` never saw them, the free calls ran only if a stop flag was already set (it never was), and the threads were detached. It does not teach anything the plain loop does not.
- The original live-view program never freed the digitizers; this version does.

---

## 한국어 요약
다른 PC의 카메라를 Distributed MIL로 사용하는 방법을 다룹니다.

- 원격 시스템은 `MsysAlloc`에 `dmiltcp://<ip>:57010/M_SYSTEM_RAPIXOCXP` 형태의 문자열을 주어 할당합니다.
- 할당 이후의 Display/Digitizer/Buffer/Grab 코드는 로컬 카메라와 같고, 원격 카메라는 원격 System에서 할당합니다.
- `03_dmil_allocation_check`: 로컬 2개와 원격 2개 System의 할당 성공 여부만 확인합니다. 먼저 실행하세요.
- `03_dmil_live_view`: 로컬 카메라 2대와 원격 카메라 2대를 동시에 라이브로 봅니다. 아무 키나 누르면 종료합니다.
- 원격 PC에서 Distributed MIL 서버가 실행 중이어야 하고 해당 주소/포트로 접속 가능해야 합니다. 설정 방법은 MIL 문서의 Distributed MIL 항목을 참고하세요.
- 스레드 버전은 id를 값으로 전달하는 문제 등으로 동작이 올바르지 않아 포함하지 않았습니다.
