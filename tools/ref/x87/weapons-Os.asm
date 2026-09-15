mdl_slab	proc	far
	enter	14,0
	push	si
	push	di
	fldz	
	fldz	
	fld	dword ptr [bp+14]
	fstp	st(1)
	fld	dword ptr [bp+18]
	fstp	st(2)
	mov	si,word ptr [bp+22]
	mov	di,word ptr [bp+24]
   ;	
   ;	                        float *tn, float *tf )
   ;	{
   ;	    float t0, t1, sw;
   ;	
   ;	    if ( fabs( d ) < 0.0001 ) return ( o >= lo && o <= hi ) ? -1 : 0;
   ;	
	fld	st(1)
	fabs	
	fcomp	qword ptr DGROUP:s@
	fstsw	word ptr [bp-14]
	fwait	
	mov	ax,word ptr [bp-14]
	sahf	
	jae	short @1@198
	fld	st(0)
	fcomp	dword ptr [bp+6]
	fstsw	word ptr [bp-14]
	fwait	
	mov	ax,word ptr [bp-14]
	sahf	
	jae short	@@0
	jmp	@1@422
@@0:
	fld	st(0)
	fcomp	dword ptr [bp+10]
	fstsw	word ptr [bp-14]
	fwait	
	mov	ax,word ptr [bp-14]
	sahf	
	jbe short	@@1
	jmp	@1@422
@@1:
	mov	ax,-1
@1@142:
	jmp	@1@450
	jmp	@1@422
@1@198:
   ;	
   ;	    t0 = ( lo - o ) / d;
   ;	
	fld	dword ptr [bp+6]
	fsub	st(0),st(1)
	fdiv	st(0),st(2)
	fstp	dword ptr [bp-4]
   ;	
   ;	    t1 = ( hi - o ) / d;
   ;	
	fld	dword ptr [bp+10]
	fsub	st(0),st(1)
	fdiv	st(0),st(2)
	fstp	dword ptr [bp-8]
   ;	
   ;	    if ( t0 > t1 ) { sw = t0; t0 = t1; t1 = sw; }
   ;	
	fld	dword ptr [bp-4]
	fcomp	dword ptr [bp-8]
	fstsw	word ptr [bp-14]
	fwait	
	mov	ax,word ptr [bp-14]
	sahf	
	jbe	short @1@254
	mov	eax,dword ptr [bp-4]
	mov	dword ptr [bp-12],eax
	mov	eax,dword ptr [bp-8]
	mov	dword ptr [bp-4],eax
	mov	eax,dword ptr [bp-12]
	mov	dword ptr [bp-8],eax
@1@254:
   ;	
   ;	    if ( t0 > *tn ) *tn = t0;
   ;	
	fld	dword ptr [si]
	fcomp	dword ptr [bp-4]
	fstsw	word ptr [bp-14]
	fwait	
	mov	ax,word ptr [bp-14]
	sahf	
	jae	short @1@310
	mov	eax,dword ptr [bp-4]
	mov	dword ptr [si],eax
@1@310:
   ;	
   ;	    if ( t1 < *tf ) *tf = t1;
   ;	
	fld	dword ptr [di]
	fcomp	dword ptr [bp-8]
	fstsw	word ptr [bp-14]
	fwait	
	mov	ax,word ptr [bp-14]
	sahf	
	jbe	short @1@366
	mov	eax,dword ptr [bp-8]
	mov	dword ptr [di],eax
@1@366:
   ;	
   ;	    return (short) ( *tn <= *tf );
   ;	
	fld	dword ptr [si]
	fcomp	dword ptr [di]
	fstsw	word ptr [bp-14]
	fwait	
	mov	ax,word ptr [bp-14]
	sahf	
	ja	short @1@422
	mov	ax,1
	jmp	@1@142
@1@422:
	xor	ax,ax
@1@450:
   ;	
   ;	}
   ;	
	pop	di
	pop	si
	fstp    st(0)	
	fstp    st(0)	
	leave	
	ret	
mdl_slab	endp
