;; t06layout -- the complete tableless Surface descriptor, written out.
;;
;; The offsets below are a TRIPWIRE. Every accessor reaches its fields
;; through the struct, so reordering them breaks nothing on its own --
;; but the layout is also what qgl.bi mirrors and what a debugger session
;; reads by hand, so a change to it should be a decision and not a
;; side effect. A failure here means: say so out loud, then update these.

                .model  medium, pascal
                .386

                include qgl.inc
                include tfw.inc

qglSfFree     proto   far :dword
qglMemAvail   proto   far :word

SF_FMT          equ     0
SF_TYP          equ     2
SF_BPP          equ     4
SF_P2B          equ     5
SF_XRES         equ     6
SF_YRES         equ     8
SF_BPS          equ     10
SF_PAGES        equ     12
SF_STARTSL      equ     14
SF_SIZE         equ     16
SF_VIEWOFS      equ     20
SF_XMIN         equ     24
SF_YMIN         equ     26
SF_XMAX         equ     28
SF_YMAX         equ     30
SF_FPTR         equ     32              ;; MEM_SF.fptr / EMS_SF.hnd, the union
SF_ZSF          equ     36              ;; the depth Surface made for this one
SF_ZMODE        equ     40
SF_BYTES        equ     42              ;; T Surface, header only

SFW             equ     32

.data
n_fmt           db      'fmt     at 0          $'
n_typ           db      'typ     at 2          $'
n_bpp           db      'bpp     at 4          $'
n_p2b           db      'p2b     at 5          $'
n_xres          db      'xRes    at 6          $'
n_yres          db      'yRes    at 8          $'
n_bps           db      'bps     at 10         $'
n_pages         db      'pages   at 12         $'
n_startsl       db      'startSL at 14         $'
n_size          db      '_size   at 16         $'
n_viewofs       db      'baseOfs at 20         $'
n_xmin          db      'xMin    at 24         $'
n_ymin          db      'yMin    at 26         $'
n_xmax          db      'xMax    at 28         $'
n_ymax          db      'yMax    at 30         $'
n_fptr          db      'fptr    at 32         $'
n_hnd           db      'hnd     at 32         $'
n_zsf           db      'zsf     at 36         $'
n_zmode         db      'zmode   at 40         $'
n_size_of       db      'T Surface is 42       $'
n_new           db      'ems surface made      $'
n_foot          db      'ems descriptor <=128 B$'

sf              dd      0
before          dd      0
after           dd      0

.code
tmain           proc    far public uses bx es

                CHK     n_fmt,     Surface.fmt,     SF_FMT
                CHK     n_typ,     Surface.typ,     SF_TYP
                CHK     n_bpp,     Surface.bpp,     SF_BPP
                CHK     n_p2b,     Surface.p2b,     SF_P2B
                CHK     n_xres,    Surface.xRes,    SF_XRES
                CHK     n_yres,    Surface.yRes,    SF_YRES
                CHK     n_bps,     Surface.bps,     SF_BPS
                CHK     n_pages,   Surface.pages,   SF_PAGES
                CHK     n_startsl, Surface.startSL, SF_STARTSL
                CHK     n_size,    Surface._size,   SF_SIZE
                CHK     n_viewofs, Surface.baseOfs, SF_VIEWOFS
                CHK     n_xmin,    Surface.xMin,    SF_XMIN
                CHK     n_ymin,    Surface.yMin,    SF_YMIN
                CHK     n_xmax,    Surface.xMax,    SF_XMAX
                CHK     n_ymax,    Surface.yMax,    SF_YMAX
                CHK     n_fptr,    Surface.fptr,    SF_FPTR
                CHK     n_hnd,     Surface.hnd,     SF_FPTR
                CHK     n_zsf,     Surface.zsf,     SF_ZSF
                CHK     n_zmode,   Surface.zmode,   SF_ZMODE
                CHK     n_size_of, T Surface,       SF_BYTES

                invoke  qglSfInit
                invoke  qglMemAvail, MEM_TOTAL
                SAVEP   before
                invoke  qglSfNew, SFW, 512, SURF_EMS
                SAVEP   sf
                mov     bx, dx
                or      bx, ax
                NZ      bx
                CHK     n_new, ax, 1
                invoke  qglMemAvail, MEM_TOTAL
                SAVEP   after

                mov     eax, before
                sub     eax, after
                cmp     eax, 128
                mov     ax, 0
                seta    al
                xor     ax, 1
                CHK     n_foot, ax, 1

                invoke  qglSfFree, sf
                ret
tmain           endp
                end
