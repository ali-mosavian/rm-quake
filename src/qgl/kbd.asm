;; kbd.asm -- every key's state, from the scancode line.
;;
;; name: qglKbdInit / qglKbdShutdown
;; desc: Hooks INT 9 and keeps a word per scancode in the caller's table:
;;       -1 while the key is down, 0 once it is up, and word 0 the code
;;       of the last key pressed. The BIOS never sees a key while the
;;       hook is up -- INKEY$ and SLEEP would wait forever -- and its
;;       buffer is flushed at shutdown so nothing typed leaks to the
;;       prompt.
;;
;; obs.: The table is 128 words and lives in the caller's segment; the
;;       renderer's is Keys in in.bi, a field of Game, which is dim'd in
;;       DGROUP and does not move.

                .model  medium, pascal
                .386

                include qgl.inc

KBD_DATA        equ     60h
PIC_CMD         equ     20h
PIC_EOI         equ     20h
PIC_MASK        equ     21h
BIOS_KB_HEAD    equ     1Ah
BIOS_KB_TAIL    equ     1Ch

.code

qgl$kbd_on      dw      0
qgl$kbd_old     dd      0
qgl$kbd_tab     dd      0

;;::::::::::::::
;; the INT 9 handler: table[code] = down ? -1 : 0, table[0] = code or 0
;;::::::::::::::
qgl$KbdIsr      proc    far private
                push    ax
                push    bx
                push    cx
                push    dx
                push    si
                push    ds
                lds     si, cs:qgl$kbd_tab
                in      al, KBD_DATA
                mov     ah, al
                and     al, 7Fh
                movzx   bx, al
                shl     bx, 1
                mov     dx, [si+bx]             ;; typematic repeats see -1
                test    ah, 80h
                jnz     @@release
                mov     W [si+bx], -1
                xor     ah, ah
                mov     [si], ax
                or      dx, dx
                jnz     @@eoi                    ;; queue one make per press
                mov     dl, 1
                jmp     @@modifier

@@release:      mov     W [si+bx], 0
                mov     W [si], 0
                or      dx, dx
                jz      @@eoi                    ;; queue one break per held key
                xor     dl, dl

@@modifier:
                xor     ah, ah
                or      dl, dl
                jz      @@queue                  ;; shift only matters to a make
                mov     ah, 1                    ;; bit 8 = down

                cmp     W [si+84], 0             ;; left Shift, k[42]
                jne     @@shifted
                cmp     W [si+108], 0            ;; right Shift, k[54]
                je      @@queue
@@shifted:      or      al, 80h                  ;; modifier at make time
@@queue:

                ;; A held-state table cannot preserve transitions between
                ;; two slow frames. Queue each make and break; the ISR owns
                ;; head and the foreground owns tail, so neither side
                ;; updates the other's byte.
                mov     cl, [si+320]
                mov     ch, cl
                inc     ch
                and     ch, 31
                cmp     ch, [si+321]
                jne     @@store
                mov     B [si+322], 1            ;; foreground releases every owner
                jmp     @@eoi
@@store:
                movzx   bx, cl
                shl     bx, 1
                mov     [si+256+bx], ax
                mov     [si+320], ch
@@eoi:
                mov     al, PIC_EOI
                out     PIC_CMD, al
                pop     ds
                pop     si
                pop     dx
                pop     cx
                pop     bx
                pop     ax
                iret
qgl$KbdIsr      endp


;;::::::::::::::
;; qglKbdInit ( keys:far ptr ) -- a second call changes nothing
;;::::::::::::::
qglKbdInit    proc    public uses bx cx dx di es ds,\
                        keys:far ptr

                cmp     cs:qgl$kbd_on, 0
                jne     @@done

                les     di, keys
                mov     W cs:qgl$kbd_tab, di
                mov     W cs:qgl$kbd_tab+2, es
                xor     ax, ax
                mov     cx, 162                   ;; state, word queue, indices/overflow
                cld
                rep     stosw

                mov     ax, 3509h
                int     21h
                mov     W cs:qgl$kbd_old, bx
                mov     W cs:qgl$kbd_old+2, es

                mov     ax, cs
                mov     ds, ax
                mov     dx, offset qgl$KbdIsr
                mov     ax, 2509h
                int     21h

                in      al, PIC_MASK
                and     al, not 2               ;; IRQ 1 on
                out     PIC_MASK, al
                mov     cs:qgl$kbd_on, 1
@@done:         ret
qglKbdInit    endp


;;::::::::::::::
;; qglKbdShutdown () -- the BIOS handler back, its buffer emptied
;;::::::::::::::
qglKbdShutdown proc   public uses ax dx ds

                cmp     cs:qgl$kbd_on, 0
                je      @@done
                mov     cs:qgl$kbd_on, 0

                lds     dx, cs:qgl$kbd_old
                mov     ax, 2509h
                int     21h

                mov     ax, 40h
                mov     ds, ax
                mov     ax, ds:[BIOS_KB_TAIL]
                mov     ds:[BIOS_KB_HEAD], ax
@@done:         ret
qglKbdShutdown endp

                end
