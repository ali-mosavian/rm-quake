_mdl_damage	proc	far
	?debug	B
	enter	6,0
	push	si
	push	di
	?debug	C E603646D67040A12000003656E744B0A0E000003+
	?debug	C 7264726E0A0C0000056669676874670A0A000006+
	?debug	C 706C61796572180A08000005776F726C641B0A06+
	?debug	C 0000
	mov	si,word ptr [bp+10]
	mov	di,word ptr [bp+12]
	?debug	B
   ;	
   ;	                  MdlEnt far *ent, short dmg )
   ;	{
   ;	    float r;
   ;	
   ;	    if ( ent->cruc ) return;    /* never walkmonster_start: takedamage stays DAMAGE_NO */
   ;	
	?debug	L 68
	les	bx,dword ptr [bp+14]
	cmp	word ptr es:[bx+100],0
	je	short @2@86
	jmp	@2@1234
@2@86:
   ;	
   ;	    ent->health = (short) ( ent->health - dmg );
   ;	
	?debug	L 69
	les	bx,dword ptr [bp+14]
	mov	ax,word ptr es:[bx+48]
	sub	ax,word ptr [bp+18]
	mov	word ptr es:[bx+48],ax
   ;	
   ;	    if ( ent->health <= 0 ) {
   ;	
	?debug	L 70
	cmp	word ptr es:[bx+48],0
	jle short	@@3
	jmp	@2@282
@@3:
   ;	
   ;	        ent->state = MDL_ST_DEAD;
   ;	
	?debug	L 71
	mov	word ptr es:[bx+22],2
   ;	
   ;	        ent->anim_frame = 0;
   ;	
	?debug	L 72
	mov	word ptr es:[bx+24],0
   ;	
   ;	        if ( ent->kind != MDL_KIND_ZOMBIE && ent->health < mdl_gib_below[ ent->kind ] )
   ;	
	?debug	L 73
	cmp	word ptr es:[bx+20],5
	je	short @2@198
	mov	bx,word ptr es:[bx+20]
	shl	bx,1
	mov	ax,COMBAT5_DATA
	mov	es,ax
	mov	ax,word ptr es:mdl_gib_below[bx]
	les	bx,dword ptr [bp+14]
	cmp	ax,word ptr es:[bx+48]
	jle	short @2@198
   ;	
   ;	            mdl_voice( player, &world->snd, ent, CHAN_VOICE, SND_UDEATH, ATTN_NORM );
   ;	
	?debug	L 74
	push	large 000010084h
	push	2
	push	word ptr [bp+16]
	push	bx
	mov	ax,word ptr [bp+6]
	add	ax,7358
	push	ax
	push	word ptr [bp+8]
	call	far ptr _mdl_voicz
	add	sp,14
	jmp	short @2@226
@2@198:
   ;	
   ;	        else
   ;	            mdl_say( player, &world->snd, ent, 3 );
   ;	
	?debug	L 76
	push	3
	push	dword ptr [bp+14]
	mov	ax,word ptr [bp+6]
	add	ax,7358
	push	ax
	push	word ptr [bp+8]
	call	far ptr _mdl_say
	add	sp,10
@2@226:
   ;	
   ;	        fight->kills++;
   ;	
	?debug	L 77
	inc	word ptr [si+157]
   ;	
   ;	        if ( ent->kind == MDL_KIND_ARMY )
   ;	
	?debug	L 78
	les	bx,dword ptr [bp+14]
	cmp	word ptr es:[bx+20],0
	je short	@@4
	jmp	@2@1234
@@4:
   ;	
   ;	            pl_item_add( world, ENT_ITEM_SHELLS, ENT_BACKPACK, &ent->pos );
   ;	
	?debug	L 79
	push	word ptr [bp+16]
	push	bx
	push	large 000050001h
	push	word ptr [bp+6]
	call	far ptr _pl_item_add
	add	sp,10
	jmp	@2@1234
@2@282:
   ;	
   ;	        return;
   ;	    }
   ;	    ent->hunting = -1;
   ;	
	?debug	L 82
	les	bx,dword ptr [bp+14]
	mov	word ptr es:[bx+50],-1
   ;	
   ;	    ent->ideal_yaw = mdl_vectoyaw( player->pos.x - ent->pos.x,
   ;	
	?debug	L 83
   ;	
   ;	                                    player->pos.y - ent->pos.y );
   ;	
	?debug	L 84
	fld	dword ptr es:[bx+4]
	mov	bx,word ptr [bp+8]
	fsubr	dword ptr [bx+4]
	sub	sp,4
	fstp	dword ptr [bp-14]
	fld	dword ptr [bx]
	les	bx,dword ptr [bp+14]
	fsub	dword ptr es:[bx]
	sub	sp,4
	fstp	dword ptr [bp-18]
	fwait	
	call	far ptr _mdl_vectoyaw
	add	sp,8
	les	bx,dword ptr [bp+14]
	fstp	dword ptr es:[bx+16]
   ;	
   ;	
   ;	    if ( ent->kind == MDL_KIND_ZOMBIE ) {
   ;	
	?debug	L 86
	fwait	
	cmp	word ptr es:[bx+20],5
	je short	@@5
	jmp	@2@646
@@5:
   ;	
   ;	        /* zombie_pain: z_pain at every hit, the health always back, so
   ;	           only one hit of 60 kills; under 9 ignored, 25 or more drops it
   ;	           for three seconds. Half the flinches are painb and painc, whose
   ;	           frames say z_pain1. */
   ;	        ent->health = ZOMBIE_HEALTH;
   ;	
	?debug	L 91
	mov	word ptr es:[bx+48],60
   ;	
   ;	        mdl_say( player, &world->snd, ent, 2 );
   ;	
	?debug	L 92
	push	2
	push	word ptr [bp+16]
	push	bx
	mov	ax,word ptr [bp+6]
	add	ax,7358
	push	ax
	push	word ptr [bp+8]
	call	far ptr _mdl_say
	add	sp,10
   ;	
   ;	        if ( dmg < ZOMBIE_PAIN_MIN ) return;
   ;	
	?debug	L 93
	cmp	word ptr [bp+18],9
	jge	short @2@366
	jmp	@2@1234
@2@366:
   ;	
   ;	        if ( rdr->anim_time < ent->pain_finished ) return;
   ;	
	?debug	L 94
	les	bx,dword ptr [bp+14]
	fld	dword ptr es:[bx+60]
	fcomp	dword ptr [di+14]
	fstsw	word ptr [bp-6]
	fwait	
	mov	ax,word ptr [bp-6]
	sahf	
	jbe	short @2@422
	jmp	@2@1234
@2@422:
   ;	
   ;	        ent->pain_finished = rdr->anim_time +
   ;	
	?debug	L 95
   ;	
   ;	            ( dmg >= ZOMBIE_FALL_DMG ? ZOMBIE_FALL_TIME : ZOMBIE_FLINCH );
   ;	
	?debug	L 96
	cmp	word ptr [bp+18],25
	jl	short @2@478
	fld	dword ptr DGROUP:s@+12
	jmp	short @2@506
@2@478:
	fld1	
@2@506:
	fadd	dword ptr [di+14]
	les	bx,dword ptr [bp+14]
	fstp	dword ptr es:[bx+60]
   ;	
   ;	        r = rng_float( &world->rng );
   ;	
	?debug	L 97
	fwait	
	push	word ptr [bp+6]
	call	far ptr _rng_float
	pop	cx
	fstp	dword ptr [bp-4]
   ;	
   ;	        if ( dmg < ZOMBIE_FALL_DMG && r >= 0.25f && r < 0.75f )
   ;	
	?debug	L 98
	fwait	
	cmp	word ptr [bp+18],25
	jl short	@@6
	jmp	@2@1206
@@6:
	fld	dword ptr [bp-4]
	fcomp	dword ptr DGROUP:s@+16
	fstsw	word ptr [bp-6]
	fwait	
	mov	ax,word ptr [bp-6]
	sahf	
	jae short	@@7
	jmp	@2@1206
@@7:
	fld	dword ptr [bp-4]
	fcomp	dword ptr DGROUP:s@+20
	fstsw	word ptr [bp-6]
	fwait	
	mov	ax,word ptr [bp-6]
	sahf	
	jb short	@@8
	jmp	@2@1206
@@8:
   ;	
   ;	            mdl_voice( player, &world->snd, ent, CHAN_VOICE, SND_Z_PAIN1, ATTN_NORM );
   ;	
	?debug	L 99
	push	large 00001009Dh
	push	2
	push	dword ptr [bp+14]
	mov	ax,word ptr [bp+6]
	add	ax,7358
	push	ax
	push	word ptr [bp+8]
	call	far ptr _mdl_voice
	add	sp,14
	jmp	@2@1206
	jmp	@2@1206
@2@646:
   ;	
   ;	        ent->state = MDL_ST_PAIN;
   ;	        ent->anim_frame = 0;
   ;	        return;
   ;	    }
   ;	
   ;	    if ( rdr->anim_time < ent->pain_finished ) return;
   ;	
	?debug	L 105
	les	bx,dword ptr [bp+14]
	fld	dword ptr es:[bx+60]
	fcomp	dword ptr [di+14]
	fstsw	word ptr [bp-6]
	fwait	
	mov	ax,word ptr [bp-6]
	sahf	
	jbe	short @2@702
	jmp	@2@1234
@2@702:
   ;	
   ;	    switch ( ent->kind ) {
   ;	
	?debug	L 106
	les	bx,dword ptr [bp+14]
	mov	bx,word ptr es:[bx+20]
	dec	bx
	cmp	bx,6
	ja	short @2@870
	shl	bx,1
	jmp	word ptr cs:@2@C786[bx]
@2@786:
   ;	
   ;	    case MDL_KIND_KNIGHT:
   ;	    case MDL_KIND_OGRE:
   ;	    case MDL_KIND_DEMON:
   ;	        ent->pain_finished = rdr->anim_time + KNIGHT_PAIN;
   ;	
	?debug	L 110
	les	bx,dword ptr [bp+14]
	fld	dword ptr [di+14]
	fld1	
	fadd	
@2@814:
	fstp	dword ptr es:[bx+60]
   ;	
   ;	        break;
   ;	
	?debug	L 111
	fwait	
	jmp	@2@1038
@2@842:
   ;	
   ;	    case MDL_KIND_SHAMBLER:
   ;	        ent->pain_finished = rdr->anim_time + SHAMBLER_PAIN;
   ;	
	?debug	L 113
	les	bx,dword ptr [bp+14]
	fld	dword ptr [di+14]
	fadd	dword ptr DGROUP:s@
	jmp	short @2@814
@2@870:
   ;	
   ;	        break;
   ;	    default:
   ;	        r = rng_float( &world->rng );
   ;	
	?debug	L 116
	push	word ptr [bp+6]
	call	far ptr _rng_float
	pop	cx
	fstp	dword ptr [bp-4]
   ;	
   ;	        ent->pain_finished = rdr->anim_time +
   ;	
	?debug	L 117
   ;	
   ;	            ( r < MDL_PAIN_SHORT_P ? MDL_PAIN_SHORT : MDL_PAIN_LONG );
   ;	
	?debug	L 118
	fld	dword ptr [bp-4]
	fcomp	dword ptr DGROUP:s@+24
	fstsw	word ptr [bp-6]
	fwait	
	mov	ax,word ptr [bp-6]
	sahf	
	jae	short @2@926
	fld	dword ptr DGROUP:s@+28
	jmp	short @2@954
@2@926:
	fld	dword ptr DGROUP:s@+32
@2@954:
	fadd	dword ptr [di+14]
	les	bx,dword ptr [bp+14]
	fstp	dword ptr es:[bx+60]
   ;	
   ;	        /* army_pain: pain1 with the short flinch, pain2 with the others */
   ;	        if ( ent->kind == MDL_KIND_ARMY && r >= MDL_PAIN_SHORT_P ) {
   ;	
	?debug	L 120
	fwait	
	cmp	word ptr es:[bx+20],0
	jne	short @2@1038
	fld	dword ptr [bp-4]
	fcomp	dword ptr DGROUP:s@+24
	fstsw	word ptr [bp-6]
	fwait	
	mov	ax,word ptr [bp-6]
	sahf	
	jb	short @2@1038
   ;	
   ;	            mdl_voice( player, &world->snd, ent, CHAN_VOICE, SND_ARMY_PAIN2, ATTN_NORM );
   ;	
	?debug	L 121
	push	large 000010091h
	push	2
	push	word ptr [bp+16]
	push	bx
	mov	ax,word ptr [bp+6]
	add	ax,7358
	push	ax
	push	word ptr [bp+8]
	call	far ptr _mdl_voice
	add	sp,14
	jmp	@2@1206
@2@1038:
   ;	
   ;	            ent->state = MDL_ST_PAIN;
   ;	            ent->anim_frame = 0;
   ;	            return;
   ;	        }
   ;	        break;
   ;	    }
   ;	    mdl_say( player, &world->snd, ent, 2 );
   ;	
	?debug	L 128
	push	2
	push	dword ptr [bp+14]
	mov	ax,word ptr [bp+6]
	add	ax,7358
	push	ax
	push	word ptr [bp+8]
	call	far ptr _mdl_say
	add	sp,10
   ;	
   ;	    /* demon1_pain and sham_pain: a hit under the roll does not flinch */
   ;	    if ( ent->kind == MDL_KIND_DEMON   && rng_float( &world->rng ) * 200.0f > dmg ) return;
   ;	
	?debug	L 130
	les	bx,dword ptr [bp+14]
	cmp	word ptr es:[bx+20],4
	jne	short @2@1122
	push	word ptr [bp+6]
	call	far ptr _rng_float
	pop	cx
	fmul	dword ptr DGROUP:s@+36
	mov	ax,word ptr [bp+18]
	mov	word ptr [bp-6],ax
	fild	word ptr [bp-6]
	fcompp
	fstsw	word ptr [bp-6]
	fwait	
	mov	ax,word ptr [bp-6]
	sahf	
	jae	short @2@1122
	jmp	short @2@1234
@2@1122:
   ;	
   ;	    if ( ent->kind == MDL_KIND_SHAMBLER && rng_float( &world->rng ) * SHAMBLER_PAIN_ROLL > dmg ) return;
   ;	
	?debug	L 131
	les	bx,dword ptr [bp+14]
	cmp	word ptr es:[bx+20],7
	jne	short @2@1206
	push	word ptr [bp+6]
	call	far ptr _rng_float
	pop	cx
	fmul	dword ptr DGROUP:s@+40
	mov	ax,word ptr [bp+18]
	mov	word ptr [bp-6],ax
	fild	word ptr [bp-6]
	fcompp
	fstsw	word ptr [bp-6]
	fwait	
	mov	ax,word ptr [bp-6]
	sahf	
	jae	short @2@1206
	jmp	short @2@1234
@2@1206:
	les	bx,dword ptr [bp+14]
	mov	word ptr es:[bx+22],3
   ;	
   ;	
   ;	    ent->state = MDL_ST_PAIN;
   ;	    ent->anim_frame = 0;
   ;	
	?debug	L 134
	mov	word ptr es:[bx+24],0
@2@1234:
   ;	
   ;	}
   ;	
	?debug	L 135
	pop	di
	pop	si
	leave	
	ret	
	?debug	C E601720E02FCFF0005776F726C641B0A06000006+
	?debug	C 706C61796572180A080000056669676874670801+
	?debug	C 0A0072030C0600037264726E08010D0000000C07+
	?debug	C 0003656E744B0A0E000003646D67040A120000
	?debug	E
	?debug	E
_mdl_damage	endp

@2@C786	label	word
	dw	@2@786
	dw	@2@870
	dw	@2@786
	dw	@2@786
	dw	@2@870
	dw	@2@870
	dw	@2@842

s@	label	byte
	db	0
	db	0
	db	0
	db	'@'
	db	0
	db	0
	db	0
	db	'?'
	db	0
	db	0
	db	-96
	db	'@'
	db	0
	db	0
	db	'@@'
	db	0
	db	0
	db	-128
	db	'>'
	db	0
	db	0
	db	'@?'
	db	-51
	db	-52
	db	'L>'
	db	-102
	db	-103
	db	25
	db	'?'
	db	-51
	db	-52
	db	-116
	db	'?'
	db	0
	db	0
	db	'HC'
	db	0
	db	0
	db	-56
	db	'C'
