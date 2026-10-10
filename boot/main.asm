
; boot/main.asm
;
; Our first x86-64 UEFI application.
; No GRUB, no Limine, no C runtime.

BITS 64
DEFAULT REL

SECTION .text

global efi_main

efi_main:
    ; UEFI x86-64 passes arguments using the
    ; Microsoft x64 calling convention:
    ;
    ; RCX = ImageHandle
    ; RDX = SystemTable
    ;
    ; Save the SystemTable pointer.
    mov r12, rdx

    ; At function entry, RSP is 8 mod 16.
    ; Reserve 40 bytes:
    ;   32 bytes of shadow space
    ;    8 bytes to align the stack for CALL
    sub rsp, 40

    ; EFI_SYSTEM_TABLE.ConOut is at offset 0x40.
    mov rax, [r12 + 0x40]

    ; SIMPLE_TEXT_OUTPUT_PROTOCOL.OutputString
    ; is at offset 0x08 within that protocol.
    mov rax, [rax + 0x08]

    ; RCX = ConOut protocol pointer
    ; RDX = pointer to UTF-16 message
    mov rcx, [r12 + 0x40]
    lea rdx, [rel message]

    ; Call the UEFI firmware's OutputString.
    call rax

    ; Restore the stack.
    add rsp, 40

.hang:
    hlt
    jmp .hang

    ; Return EFI_SUCCESS (0).
    xor eax, eax
    ret

SECTION .rdata

message:
    ; UEFI text uses 16-bit CHAR16 characters.
    dw 'H', 'e', 'l', 'l', 'o', ' '
    dw 'f', 'r', 'o', 'm', ' '
    dw 'm', 'y', ' ', 'O', 'S', '!'
    dw 13, 10, 0
