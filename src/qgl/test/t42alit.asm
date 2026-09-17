;; t42alit -- an alias triangle samples the copied colormap row.
;;
;; Alias geometry keeps its triangles and the map colormap in the same
;; EMS window. qglRsLitMap must copy the selected row before that window
;; is remapped, and the z-tested Atex filler must apply the copy after
;; every texture fetch rather than merely remembering a row number.

                .model  medium, pascal
                .386

                include qgl.inc
                include tfw.inc

qglDrFill      proto   far :dword, :word, :word, :word, :word, :word
qglClRect      proto   far :word, :word, :word, :word
qglRsPoly      proto   far :dword, :dword, :word, :word, :dword
qglRsLitMap    proto   far :dword

DIM             equ     64
TEXEL           equ     11
SHADED          equ     73

.data
n_copy          db      'lit row copied            $'
n_pixel         db      'lit alias texel translated$'

quad            QVert   <8.0,  8.0, 1.0, 0.0, 0.0>
                QVert   <40.0, 8.0, 1.0, 1.0, 0.0>
                QVert   <40.0,40.0, 1.0, 1.0, 1.0>
                QVert   <8.0, 40.0, 1.0, 0.0, 1.0>
quadp           dd      0
dst             dd      0
tex             dd      0
lut             db      256 dup (?)
lutp            dd      0

.code
tmain           proc    far public uses bx cx dx si di es
                invoke  qglSfInit
                invoke  qglSfNew, DIM, DIM, SURF_CMEM
                SAVEP   dst
                invoke  qglSfNew, 8, 8, SURF_CMEM
                SAVEP   tex
                invoke  qglClRect, 0, 0, DIM-1, DIM-1
                invoke  qglDrFill, tex, 0, 0, 7, 7, TEXEL
                invoke  qglDrFill, dst, 0, 0, DIM-1, DIM-1, 0

                xor     bx, bx
                mov     cx, 256
@@lut:          mov     lut[bx], bl
                inc     bx
                loop    @@lut
                mov     lut[TEXEL], SHADED
                mov     word ptr quadp, offset quad
                mov     word ptr quadp+2, ds
                mov     word ptr lutp, offset lut
                mov     word ptr lutp+2, ds

                invoke  qglRsLitMap, lutp
                CHK     n_copy, ax, 1
                invoke  qglRsPoly, dst, quadp, 4, QGL_M_ALIT, tex
                invoke  qglSfPget, dst, 24, 24
                CHK     n_pixel, ax, SHADED
                ret
tmain           endp
                end
