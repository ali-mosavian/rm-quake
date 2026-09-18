;; t44turb -- liquid spans keep Quake's 128-coordinate phase before
;; wrapping the final sample to a raw 64x64 texture.
;;
;; The old renderer displaced only polygon corners and sampled the shaded
;; atlas. This checks the observable span equation at a covered pixel in
;; both projective modes, including the high phase bit and all depth paths.

                .model  medium, pascal
                .386

                include qgl.inc
                include tfw.inc

qglDrFill       proto   far :dword, :word, :word, :word, :word, :word
qglClRect       proto   far :word, :word, :word, :word
qglRsPoly       proto   far :dword, :dword, :word, :word, :dword
qglRsTurbInit   proto   far :dword
qglRsTurbPhase  proto   far :word
qglSfZMode      proto   far :dword, :word
qglSfZNew       proto   far :dword, :word
qglSfZClear     proto   far :dword, :word
qglZScale       proto   far :dword

DIM             equ     64
BG              equ     9
PX              equ     16
PY              equ     20

.data
n_init          db      'turb table accepted     $'
n_tex           db      'texture allocated       $'
n_dst           db      'destination allocated   $'
n_ptex          db      'control ptex accepted   $'
n_draw          db      'turb mode accepted      $'
n_phase0        db      'phase zero exact sample $'
n_phase64       db      'phase keeps high bit    $'
n_affine        db      'affine exact sample     $'
n_zset          db      'set rejects farther     $'
n_znear         db      'test accepts nearer     $'
n_taildraw      db      'short span accepted     $'
n_tail_a        db      'short span pixel 10     $'
n_tail_b        db      'short span pixel 16     $'
n_tail_c        db      'short span pixel 21     $'
n_full          db      'full span signed shift  $'
n_small         db      'non-64 texture refused  $'

;; Constant texel (8,12), expressed as u/z and v/z over a z=1 polygon.
;; Keeping the input constant makes the expected sample independent of
;; edge rounding: this test is the turbulence equation, not the scanner.
quad            QVert   <8.0,  8.0, 1.0, 0.125, 0.1875>
                QVert   <40.0, 8.0, 1.0, 0.125, 0.1875>
                QVert   <40.0,40.0, 1.0, 0.125, 0.1875>
                QVert   <8.0, 40.0, 1.0, 0.125, 0.1875>
quadp           dd      0

;; A 15-pixel row with changing depth and texture coordinate. Projective
;; turbulence has only its final short span here, so it must agree with
;; the affine mode that interpolates the same two recovered endpoints.
tailquad        QVert   <8.0,  44.0, 1.0,  0.0625,   0.1875>
                QVert   <23.0, 44.0, 0.25, 0.140625, 0.046875>
                QVert   <23.0, 52.0, 0.25, 0.140625, 0.046875>
                QVert   <8.0,  52.0, 1.0,  0.0625,   0.1875>
tailquadp       dd      0

farquad         QVert   <8.0,  8.0, 0.5, 0.0625, 0.09375>
                QVert   <40.0, 8.0, 0.5, 0.0625, 0.09375>
                QVert   <40.0,40.0, 0.5, 0.0625, 0.09375>
                QVert   <8.0, 40.0, 0.5, 0.0625, 0.09375>
farquadp        dd      0

nearquad        QVert   <8.0,  8.0, 2.0, 0.25, 0.375>
                QVert   <40.0, 8.0, 2.0, 0.25, 0.375>
                QVert   <40.0,40.0, 2.0, 0.25, 0.375>
                QVert   <8.0, 40.0, 2.0, 0.25, 0.375>
nearquadp       dd      0

;; The first full 16-pixel span has a -17 fixed-point endpoint delta.
;; Id's arithmetic >>4 steps -2; signed division would incorrectly step
;; -1. The start is one fixed unit above texel 20, so pixel 9 exposes it.
fullquad        QVert   <8.0,  54.0, 1.0, 0.3125004917383194, 0.1875>
                QVert   <41.0, 54.0, 1.0, 0.31249213218688965, 0.1875>
                QVert   <41.0, 62.0, 1.0, 0.31249213218688965, 0.1875>
                QVert   <8.0,  62.0, 1.0, 0.3125004917383194, 0.1875>
fullquadp       dd      0

dst             dd      0
zdc             dd      0
pad             dd      0
tex             dd      0
smalltex        dd      0
tabp            dd      0
zs              real4   1000.0
turbtab         dd      128 dup (?)
row_y           dw      0

.code

filltab         proc    near private uses ax bx dx si
                xor     bx, bx
                mov     si, offset turbtab
@@loop:         mov     ax, bx
                imul    ax, 3
                test    bx, 64
                jz      @F
                add     ax, 7
@@:             and     ax, 63
                movzx   eax, ax
                shl     eax, 16
                mov     D [si], eax
                add     si, 4
                inc     bx
                cmp     bx, 128
                jb      @@loop
                ret
filltab         endp

filltex         proc    near private uses ax bx cx dx di es
                mov     row_y, 0
@@row:          invoke  qglSfWrRow, tex, row_y
                mov     es, dx
                mov     di, ax
                mov     bx, row_y
                shl     bx, 6
                mov     cx, 64
@@pixel:        mov     al, bl
                stosb
                inc     bx
                loop    @@pixel
                inc     row_y
                cmp     row_y, 64
                jb      @@row
                ret
filltex         endp

tmain           proc    far public uses ax bx cx dx si di es
                invoke  qglSfInit

                invoke  qglSfNew, DIM, DIM, SURF_EMS
                SAVEP   dst
                invoke  qglSfZNew, dst, SURF_EMS
                SAVEP   zdc
                ;; The leading allocation leaves the texture at a nonzero
                ;; EMS offset while keeping all 4096 bytes in one page.
                invoke  qglSfNew, 8, 8, SURF_EMS
                SAVEP   pad
                invoke  qglSfNew, 64, 64, SURF_EMS
                SAVEP   tex
                invoke  qglSfNew, 32, 32, SURF_EMS
                SAVEP   smalltex

                mov     ax, W tex
                or      ax, W tex+2
                NZ      ax
                CHK     n_tex, ax, 1
                mov     ax, W dst
                or      ax, W dst+2
                NZ      ax
                CHK     n_dst, ax, 1

                mov     W quadp, offset quad
                mov     W quadp+2, ds
                mov     W tailquadp, offset tailquad
                mov     W tailquadp+2, ds
                mov     W farquadp, offset farquad
                mov     W farquadp+2, ds
                mov     W nearquadp, offset nearquad
                mov     W nearquadp+2, ds
                mov     W fullquadp, offset fullquad
                mov     W fullquadp+2, ds
                mov     W tabp, offset turbtab
                mov     W tabp+2, ds
                call    filltab
                invoke  qglRsTurbInit, tabp
                CHK     n_init, ax, 1
                call    filltex
                invoke  qglClRect, 0, 0, DIM-1, DIM-1
                invoke  qglZScale, dword ptr zs

                invoke  qglDrFill, dst, 0, 0, DIM-1, DIM-1, BG
                invoke  qglSfZMode, dst, QGL_Z_OFF
                invoke  qglRsTurbPhase, 0
                invoke  qglRsPoly, dst, quadp, 4, QGL_M_PTEX, tex
                NNEG    ax
                CHK     n_ptex, ax, 1
                invoke  qglRsPoly, dst, quadp, 4, QGL_M_TURB, tex
                NNEG    ax
                CHK     n_draw, ax, 1
                invoke  qglSfPget, dst, PX, PY
                CHK     n_phase0, ax, 44

                invoke  qglRsTurbPhase, 64
                invoke  qglRsPoly, dst, quadp, 4, QGL_M_TURB, tex
                invoke  qglSfPget, dst, PX, PY
                CHK     n_phase64, ax, 243

                invoke  qglDrFill, dst, 0, 0, DIM-1, DIM-1, BG
                invoke  qglRsPoly, dst, quadp, 4, QGL_M_ATURB, tex
                invoke  qglSfPget, dst, PX, PY
                CHK     n_affine, ax, 243

                invoke  qglSfZClear, dst, 0
                invoke  qglSfZMode, dst, QGL_Z_SET
                invoke  qglRsPoly, dst, quadp, 4, QGL_M_TURB, tex
                invoke  qglDrFill, dst, 0, 0, DIM-1, DIM-1, BG
                invoke  qglSfZMode, dst, QGL_Z_TEST
                invoke  qglRsPoly, dst, farquadp, 4, QGL_M_ATURB, tex
                invoke  qglSfPget, dst, PX, PY
                CHK     n_zset, ax, BG
                invoke  qglRsPoly, dst, nearquadp, 4, QGL_M_ATURB, tex
                invoke  qglSfPget, dst, PX, PY
                CHK     n_znear, ax, 243

                invoke  qglSfZMode, dst, QGL_Z_OFF
                invoke  qglRsTurbPhase, 17
                invoke  qglDrFill, dst, 0, 0, DIM-1, DIM-1, BG
                invoke  qglRsPoly, dst, tailquadp, 4, QGL_M_TURB, tex
                NNEG    ax
                CHK     n_taildraw, ax, 1
                ;; The scanner advances its integer left edge by pfrac=1.
                ;; At x10/16/21 the recovered s values are 9.0526,
                ;; 22.5262 and 33.7543; applying filltab's independent
                ;; cross-warp equation gives these three bytes.
                invoke  qglSfPget, dst, 10, 48
                CHK     n_tail_a, ax, 160
                invoke  qglSfPget, dst, 16, 48
                CHK     n_tail_b, ax, 109
                invoke  qglSfPget, dst, 21, 48
                CHK     n_tail_c, ax, 184

                invoke  qglDrFill, dst, 0, 0, DIM-1, DIM-1, BG
                invoke  qglRsPoly, dst, fullquadp, 4, QGL_M_TURB, tex
                invoke  qglSfPget, dst, 9, 58
                CHK     n_full, ax, 42

                invoke  qglRsPoly, dst, quadp, 4, QGL_M_TURB, smalltex
                CHK     n_small, ax, -1
                ret
tmain           endp
                end
