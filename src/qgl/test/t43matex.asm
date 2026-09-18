;; t43matex -- masked sprites preserve the pixel and depth behind index 255.
;;
;; A sprite is not a colour-keyed 2D blit: its opaque texels participate in
;; world depth, while transparent texels must write neither colour nor depth.
;; Exercising OFF, SET and TEST here catches both halves of that contract.

                .model  medium, pascal
                .386

                include qgl.inc
                include tfw.inc

qglDrFill     proto   far :dword, :word, :word, :word, :word, :word
qglClRect     proto   far :word, :word, :word, :word
qglRsPoly     proto   far :dword, :dword, :word, :word, :dword
qglSfZMode    proto   far :dword, :word
qglSfZNew     proto   far :dword, :word
qglSfZClear   proto   far :dword, :word
qglZScale     proto   far :dword

DIM             equ     48
BG              equ     12
FG              equ     77
OPAQUE_X        equ     16
TRANS_X         equ     32
SAMPLE_Y        equ     24

.data
n_off_o         db      'masked off opaque colour $'
n_off_t         db      'masked off keeps colour  $'
n_draw          db      'masked mode accepted     $'
n_set_o         db      'masked set writes depth  $'
n_set_t         db      'masked set keeps depth   $'
n_test_o        db      'masked test opaque colour$'
n_test_t        db      'masked test keeps colour $'
n_test_z        db      'masked test keeps depth  $'

;; 1/z and u/z,v/z: the left half samples the opaque texture column,
;; the right half the transparent one.
quad            QVert   <8.0,  8.0, 1.0, 0.0, 0.0>
                QVert   <40.0, 8.0, 1.0, 1.0, 0.0>
                QVert   <40.0,40.0, 1.0, 1.0, 1.0>
                QVert   <8.0, 40.0, 1.0, 0.0, 1.0>
quadp           dd      0
farquad         QVert   <0.0,  0.0, 0.25, 0.0, 0.0>
                QVert   <47.0, 0.0, 0.25, 0.0, 0.0>
                QVert   <47.0,47.0, 0.25, 0.0, 0.0>
                QVert   <0.0, 47.0, 0.25, 0.0, 0.0>
farp            dd      0

dst             dd      0
zdc             dd      0
tex             dd      0
zs              real4   1000.0
before_z        dw      0

.code

zget            proc    near private uses bx dx es, x:word, y:word
                invoke  qglSfRow, zdc, y
                mov     es, dx
                mov     bx, x
                shl     bx, 1
                add     bx, ax
                mov     ax, es:[bx]
                ret
zget            endp

tmain           proc    far public uses bx cx dx si di es
                invoke  qglSfInit
                invoke  qglSfNew, DIM, DIM, SURF_CMEM
                SAVEP   dst
                invoke  qglSfZNew, dst, SURF_CMEM
                SAVEP   zdc
                invoke  qglSfNew, 8, 8, SURF_CMEM
                SAVEP   tex
                invoke  qglDrFill, tex, 0, 0, 3, 7, FG
                invoke  qglDrFill, tex, 4, 0, 7, 7, 255
                mov     word ptr quadp, offset quad
                mov     word ptr quadp+2, ds
                mov     word ptr farp, offset farquad
                mov     word ptr farp+2, ds
                invoke  qglClRect, 0, 0, DIM-1, DIM-1
                invoke  qglZScale, dword ptr zs

                invoke  qglDrFill, dst, 0, 0, DIM-1, DIM-1, BG
                invoke  qglSfZMode, dst, QGL_Z_OFF
                invoke  qglRsPoly, dst, quadp, 4, QGL_M_MATEX, tex
                NNEG    ax
                CHK     n_draw, ax, 1
                invoke  qglSfPget, dst, OPAQUE_X, SAMPLE_Y
                CHK     n_off_o, ax, FG
                invoke  qglSfPget, dst, TRANS_X, SAMPLE_Y
                CHK     n_off_t, ax, BG

                invoke  qglDrFill, dst, 0, 0, DIM-1, DIM-1, BG
                invoke  qglSfZClear, dst, 0
                invoke  qglSfZMode, dst, QGL_Z_SET
                invoke  qglRsPoly, dst, quadp, 4, QGL_M_MATEX, tex
                invoke  zget, OPAQUE_X, SAMPLE_Y
                NZ      ax
                CHK     n_set_o, ax, 1
                invoke  zget, TRANS_X, SAMPLE_Y
                CHK     n_set_t, ax, 0

                invoke  qglDrFill, dst, 0, 0, DIM-1, DIM-1, BG
                invoke  qglSfZClear, dst, 0
                invoke  qglSfZMode, dst, QGL_Z_SET
                invoke  qglRsPoly, dst, farp, 4, QGL_M_FLAT, BG
                invoke  zget, TRANS_X, SAMPLE_Y
                mov     before_z, ax
                invoke  qglSfZMode, dst, QGL_Z_TEST
                invoke  qglRsPoly, dst, quadp, 4, QGL_M_MATEX, tex
                invoke  qglSfPget, dst, OPAQUE_X, SAMPLE_Y
                CHK     n_test_o, ax, FG
                invoke  qglSfPget, dst, TRANS_X, SAMPLE_Y
                CHK     n_test_t, ax, BG
                invoke  zget, TRANS_X, SAMPLE_Y
                CHK     n_test_z, ax, before_z
                ret
tmain           endp
                end
