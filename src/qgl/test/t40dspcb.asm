;; t40dspcb -- IRQ 7 gives each completed DMA half to the registered callback.
;;
;; Foreground code does no painting after registration.  Within ten BIOS
;; ticks both halves must have been handed to the callback, with the 2048-byte
;; half size MGL's Sound Blaster callback contract uses.  The callback must
;; run on the driver's stack: using the interrupted renderer stack corrupted
;; a partially presented frame, then returned through VGA ROM after long play.

                .model  medium, pascal
                .386

                include qgl.inc
                include tfw.inc

qglDspInit      proto   far :word
qglDspBuf       proto   far
qglDspCallback  proto   far :dword
qglDspStackFree proto   far
qglDspShutdown  proto   far

BIOS_TICK       equ     6Ch

.data
n_called        db      'IRQ callback called   $'
n_size          db      'callback half is 2048 $'
n_halves        db      'both halves refilled  $'
n_stack         db      'callback owns stack   $'
n_bound         db      'callback stack bounded$'
m_free          db      'callback stack free $'

cbptr           dd      0
ringptr         dd      0
calls           dw      0
badsize         dw      0
main_ss         dw      0
cb_ss           dw      0

.code

;; cdecl far callback(dst, bytes): caller removes the six argument bytes.
dsp_callback    proc    far private
                push    bp
                mov     bp, sp
                mov     ax, ss
                mov     cb_ss, ax
                inc     calls
                cmp     word ptr [bp+10], 2048
                je      @@size
                inc     badsize
@@size:         push    es
                push    di
                les     di, [bp+6]
                mov     byte ptr es:[di], 11h
                pop     di
                pop     es
                pop     bp
                ret
dsp_callback    endp

bios_wait       proc    near private uses ebx es,
                        n:word
                mov     ax, 40h
                mov     es, ax
                mov     ebx, es:[BIOS_TICK]
                movzx   eax, n
                add     ebx, eax
@@:             cmp     es:[BIOS_TICK], ebx
                jb      @B
                ret
bios_wait       endp

tmain           proc    far public uses bx dx es
                mov     ax, ss
                mov     main_ss, ax
                invoke  qglDspInit, 11025
                test    ax, ax
                jz      @@done

                invoke  qglDspBuf
                mov     word ptr ringptr, ax
                mov     word ptr ringptr+2, dx
                mov     word ptr cbptr, offset dsp_callback
                mov     word ptr cbptr+2, cs
                invoke  qglDspCallback, cbptr
                invoke  bios_wait, 10

                xor     ax, ax
                cmp     calls, 2
                jb      @@called
                mov     ax, 1
@@called:       CHK     n_called, ax, 1

                xor     ax, ax
                cmp     badsize, 0
                jne     @@size
                mov     ax, 1
@@size:         CHK     n_size, ax, 1

                les     bx, ringptr
                xor     ax, ax
                cmp     byte ptr es:[bx], 11h
                jne     @@halves
                cmp     byte ptr es:[bx+2048], 11h
                jne     @@halves
                mov     ax, 1
@@halves:       CHK     n_halves, ax, 1

                xor     ax, ax
                mov     dx, cb_ss
                cmp     dx, main_ss
                je      @@stack
                mov     ax, 1
@@stack:        CHK     n_stack, ax, 1

                invoke  qglDspStackFree
                mov     dx, ax
                movzx   eax, ax
                invoke  tshow, offset m_free, eax
                cmp     dx, 256
                jae     @@bound
                xor     ax, ax
                jmp     short @@check_bound
@@bound:        mov     ax, 1
@@check_bound:  CHK     n_bound, ax, 1

                invoke  qglDspCallback, 0
                invoke  qglDspShutdown
@@done:         ret
tmain           endp

                end
