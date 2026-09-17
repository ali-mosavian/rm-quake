;; t39gemrestore -- a borrower restores the EMS window it changed.
;;
;; The sound mixer borrows PAGE_SLOT while decoding. Leaving sample data
;; mapped there makes a caller's live mapping read unrelated bytes.

                .model  medium, pascal
                .386

                include qgl.inc
                include tfw.inc

SLOT            equ     2
MARK            equ     55h
OTHER           equ     0A5h

.data
n_a             db      'first handle made       $'
n_b             db      'borrower handle made    $'
n_restore       db      'borrower restores slot   $'

hA              dw      0
hB              dw      0
saved           dd      0

.code

tmain           proc    far public uses bx cx dx si di es

                invoke  qglGemInit
                invoke  qglGemAlloc, 32768
                mov     hA, ax
                NZ      ax
                CHK     n_a, ax, 1

                invoke  qglGemAlloc, 16384
                mov     hB, ax
                NZ      ax
                CHK     n_b, ax, 1

                invoke  qglGemMap, hA, 1, SLOT
                mov     es, ax
                mov     byte ptr es:[0], MARK

                invoke  qglGemSaveMap, SLOT
                mov     W saved+0, ax
                mov     W saved+2, dx

                invoke  qglGemMap, hB, 0, SLOT
                mov     es, ax
                mov     byte ptr es:[0], OTHER

                invoke  qglGemRestoreMap, saved, SLOT

                ;; Observe the physical window directly. Asking GemMap for
                ;; hA here would repair a failed restore and test nothing.
                invoke  qglGemFrame
                add     ax, SLOT*400h
                mov     es, ax
                mov     al, es:[0]
                xor     ah, ah
                CHK     n_restore, ax, MARK

                invoke  qglGemFree, hB
                invoke  qglGemFree, hA
                ret
tmain           endp
                end
