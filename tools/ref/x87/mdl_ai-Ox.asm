mdl_leap_height	proc	far
	push	bp
	mov	bp,sp
	sub	sp,6
   ;	
   ;	{
   ;	    return (short) ( MDL_ZLO < dz + PL_ZLO + 0.75f * ( PL_ZHI - PL_ZLO ) &&
   ;	
   ;	
   ;	                     MDL_ZHI > dz + PL_ZLO + 0.25f * ( PL_ZHI - PL_ZLO ) );
   ;	
	fld	dword ptr [bp+6]
	fadd	dword ptr DGROUP:s@+188
	fst	dword ptr [bp-6]
	fadd	dword ptr DGROUP:s@+208
	fcomp	dword ptr DGROUP:s@+188
	fstsw	word ptr [bp-2]
	fwait	
	mov	ax,word ptr [bp-2]
	sahf	
	jbe	short @21@114
	fld	dword ptr [bp-6]
	fadd	dword ptr DGROUP:s@+212
	fcomp	dword ptr DGROUP:s@+88
	fstsw	word ptr [bp-2]
	fwait	
	mov	ax,word ptr [bp-2]
	sahf	
	jae	short @21@114
	mov	ax,1
	jmp	short @21@142
@21@114:
	xor	ax,ax
@21@142:
   ;	
   ;	}
   ;	
	leave	
	ret	
mdl_leap_height	endp
