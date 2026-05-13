.code64
.global _entry
.extern _start

_entry:
    and $0xFFFFFFFFFFFFFFF0, %rsp  # 16-byte alignment
    sub $32, %rsp                  # 32-byte Shadow Space
    call _start
    1: hlt
    jmp 1b
