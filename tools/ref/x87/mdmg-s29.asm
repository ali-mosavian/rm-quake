_mdl_damage	proc	far
	?debug	B
	enter	6,0
	push	si
	push	di
	?debug	C E3260022211E4E
	?debug	C E3280020001E8096
	?debug	C E327000400162800
	?debug	C E32A0010001E809D
	?debug	C E32B0006001E80A4
	?debug	C E32C0006001A08
	?debug	C E329000400162A00
	?debug	C E32E0010001E80A6
	?debug	C E32D000400162E00
	?debug	C E3300012001E80A9
	?debug	C E32F000400163000
	?debug	C E3320006001E80AE
	?debug	C E331000400163200
	?debug	C E3340010001E80B2
	?debug	C E333000400163400
	?debug	C E335000400160400
	?debug	C E336000400160800
	?debug	C E338000A001E80B9
	?debug	C E337000400163800
	?debug	C E33A0022001E80BF
	?debug	C E33B0010001A0E
	?debug	C E33C0010001A0E
	?debug	C E339000400163A00
	?debug	C E33E0012001E80C3
	?debug	C E33D000400163E00
	?debug	C E3400030001E80CB
	?debug	C E33F000400164000
	?debug	C E3410060001A1A
	?debug	C E3420020001A0E
	?debug	C E3440034001E80D3
	?debug	C E343000400164400
	?debug	C E3460064001E80E1
	?debug	C E345000400164600
	?debug	C E348004C001E80F6
	?debug	C E347000400164800
	?debug	C E34A0012001E8109
	?debug	C E349000400164A00
	?debug	C E34C000A001E810D
	?debug	C E34B000400164C00
	?debug	C E34E0016001E8113
	?debug	C E34D000400164E00
	?debug	C E350005C001E811A
	?debug	C E3520010001E811D
	?debug	C E353000C001A02
	?debug	C E3510050001A52
	?debug	C E34F000400165000
	?debug	C E3550066001E8121
	?debug	C E354000400165500
	?debug	C E357003C001E8139
	?debug	C E35600E0011A57
	?debug	C E3580068011A57
	?debug	C E359000400160200
	?debug	C E35A003C001A06
	?debug	C E35B003C001A06
	?debug	C E35C001E001A04
	?debug	C E35D001E001A04
	?debug	C E35E0000101A06
	?debug	C E35F0084071E814A
	?debug	C E3610018001E814E
	?debug	C E3620010001A02
	?debug	C E3600080071A61
	?debug	C E3630064041E8152
	?debug	C E3640016001A08
	?debug	C E3650042041E815A
	?debug	C E366000400160400
	?debug	C E368000A001E816F
	?debug	C E367000400166800
	?debug	C E36A0024001E8173
	?debug	C E369000400166A00
	?debug	C E36C0008001E8180
	?debug	C E36B000400166C00
	?debug	C E36D0010001A04
	?debug	C E36E0000021A02
	?debug	C E36F0000021A02
	?debug	C E325000200152604
	?debug	C E3710028001E8186
	?debug	C E370000200157104
	?debug	C E3730052041E818F
	?debug	C E375001E001E81C4
	?debug	C E37400C0031A75
	?debug	C E377002C001E81CB
	?debug	C E3780012001A04
	?debug	C E376000400167700
	?debug	C E372000200157304
	?debug	C E603646D67040A12000003656E74540A0E000003+
	?debug	C 726472720A0C00000566696768741E0A0A000006+
	?debug	C 706C61796572700A08000005776F726C64250A06+
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
	?debug	L 136
	les	bx,dword ptr [bp+14]
	cmp	word ptr es:[bx+100],0
	je	short @8@86
	jmp	@8@1234
@8@86:
   ;	
   ;	    ent->health = (short) ( ent->health - dmg );
   ;	
	?debug	L 137
	les	bx,dword ptr [bp+14]
	mov	ax,word ptr es:[bx+48]
	sub	ax,word ptr [bp+18]
	mov	word ptr es:[bx+48],ax
   ;	
   ;	    if ( ent->health <= 0 ) {
   ;	
	?debug	L 138
	cmp	word ptr es:[bx+48],0
	jle short	@@3
	jmp	@8@282
@@3:
   ;	
   ;	        ent->state = MDL_ST_DEAD;
   ;	
	?debug	L 139
	mov	word ptr es:[bx+22],2
   ;	
   ;	        ent->anim_frame = 0;
   ;	
	?debug	L 140
	mov	word ptr es:[bx+24],0
   ;	
   ;	        if ( ent->kind != MDL_KIND_ZOMBIE && ent->health < mdl_gib_below[ ent->kind ] )
   ;	
	?debug	L 141
	cmp	word ptr es:[bx+20],5
	je	short @8@198
	mov	ax,word ptr es:[bx+48]
	mov	bx,word ptr es:[bx+20]
	shl	bx,1
	cmp	ax,word ptr DGROUP:mdl_gib_below[bx]
	jge	short @8@198
   ;	
   ;	            mdl_voice( player, &world->snd, ent, CHAN_VOICE, SND_UDEATH, ATTN_NORM );
   ;	
	?debug	L 142
	push	large 000010084h
	push	2
	push	dword ptr [bp+14]
	mov	ax,word ptr [bp+6]
	add	ax,7358
	push	ax
	push	word ptr [bp+8]
	call	far ptr _mdl_voice
	add	sp,14
	jmp	short @8@226
@8@198:
   ;	
   ;	        else
   ;	            mdl_say( player, &world->snd, ent, 3 );
   ;	
	?debug	L 144
	push	3
	push	dword ptr [bp+14]
	mov	ax,word ptr [bp+6]
	add	ax,7358
	push	ax
	push	word ptr [bp+8]
	call	far ptr _mdl_say
	add	sp,10
@8@226:
   ;	
   ;	        fight->kills++;
   ;	
	?debug	L 145
	inc	word ptr [si+157]
   ;	
   ;	        if ( ent->kind == MDL_KIND_ARMY )
   ;	
	?debug	L 146
	les	bx,dword ptr [bp+14]
	cmp	word ptr es:[bx+20],0
	je short	@@4
	jmp	@8@1234
@@4:
   ;	
   ;	            pl_item_add( world, ENT_ITEM_SHELLS, ENT_BACKPACK, &ent->pos );
   ;	
	?debug	L 147
	push	word ptr [bp+16]
	push	bx
	push	large 000050001h
	push	word ptr [bp+6]
	call	far ptr _pl_item_add
	add	sp,10
	jmp	@8@1234
@8@282:
   ;	
   ;	        return;
   ;	    }
   ;	    ent->hunting = -1;
   ;	
	?debug	L 150
	les	bx,dword ptr [bp+14]
	mov	word ptr es:[bx+50],-1
   ;	
   ;	    ent->ideal_yaw = mdl_vectoyaw( player->pos.x - ent->pos.x,
   ;	
	?debug	L 151
   ;	
   ;	                                    player->pos.y - ent->pos.y );
   ;	
	?debug	L 152
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
	?debug	L 154
	fwait	
	cmp	word ptr es:[bx+20],5
	je short	@@5
	jmp	@8@646
@@5:
   ;	
   ;	        /* zombie_pain: z_pain at every hit, the health always back, so
   ;	           only one hit of 60 kills; under 9 ignored, 25 or more drops it
   ;	           for three seconds. Half the flinches are painb and painc, whose
   ;	           frames say z_pain1. */
   ;	        ent->health = ZOMBIE_HEALTH;
   ;	
	?debug	L 159
	mov	word ptr es:[bx+48],60
   ;	
   ;	        mdl_say( player, &world->snd, ent, 2 );
   ;	
	?debug	L 160
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
	?debug	L 161
	cmp	word ptr [bp+18],9
	jge	short @8@366
	jmp	@8@1234
@8@366:
   ;	
   ;	        if ( rdr->anim_time < ent->pain_finished ) return;
   ;	
	?debug	L 162
	les	bx,dword ptr [bp+14]
	fld	dword ptr es:[bx+60]
	fcomp	dword ptr [di+14]
	fstsw	word ptr [bp-6]
	fwait	
	mov	ax,word ptr [bp-6]
	sahf	
	jbe	short @8@422
	jmp	@8@1234
@8@422:
   ;	
   ;	        ent->pain_finished = rdr->anim_time +
   ;	
	?debug	L 163
   ;	
   ;	            ( dmg >= ZOMBIE_FALL_DMG ? ZOMBIE_FALL_TIME : ZOMBIE_FLINCH );
   ;	
	?debug	L 164
	cmp	word ptr [bp+18],25
	jl	short @8@478
	fld	dword ptr DGROUP:s@+44
	jmp	short @8@506
@8@478:
	fld1	
@8@506:
	fadd	dword ptr [di+14]
	les	bx,dword ptr [bp+14]
	fstp	dword ptr es:[bx+60]
   ;	
   ;	        r = rng_float( &world->rng );
   ;	
	?debug	L 165
	fwait	
	push	word ptr [bp+6]
	call	far ptr _rng_float
	pop	cx
	fstp	dword ptr [bp-4]
   ;	
   ;	        if ( dmg < ZOMBIE_FALL_DMG && r >= 0.25f && r < 0.75f )
   ;	
	?debug	L 166
	fwait	
	cmp	word ptr [bp+18],25
	jl short	@@6
	jmp	@8@1206
@@6:
	fld	dword ptr [bp-4]
	fcomp	dword ptr DGROUP:s@+48
	fstsw	word ptr [bp-6]
	fwait	
	mov	ax,word ptr [bp-6]
	sahf	
	jae short	@@7
	jmp	@8@1206
@@7:
	fld	dword ptr [bp-4]
	fcomp	dword ptr DGROUP:s@+52
	fstsw	word ptr [bp-6]
	fwait	
	mov	ax,word ptr [bp-6]
	sahf	
	jb short	@@8
	jmp	@8@1206
@@8:
   ;	
   ;	            mdl_voice( player, &world->snd, ent, CHAN_VOICE, SND_Z_PAIN1, ATTN_NORM );
   ;	
	?debug	L 167
	push	large 00001009Dh
	push	2
	push	dword ptr [bp+14]
	mov	ax,word ptr [bp+6]
	add	ax,7358
	push	ax
	push	word ptr [bp+8]
	call	far ptr _mdl_voice
	add	sp,14
	jmp	@8@1206
	jmp	@8@1206
@8@646:
   ;	
   ;	        ent->state = MDL_ST_PAIN;
   ;	        ent->anim_frame = 0;
   ;	        return;
   ;	    }
   ;	
   ;	    if ( rdr->anim_time < ent->pain_finished ) return;
   ;	
	?debug	L 173
	les	bx,dword ptr [bp+14]
	fld	dword ptr es:[bx+60]
	fcomp	dword ptr [di+14]
	fstsw	word ptr [bp-6]
	fwait	
	mov	ax,word ptr [bp-6]
	sahf	
	jbe	short @8@702
	jmp	@8@1234
@8@702:
   ;	
   ;	    switch ( ent->kind ) {
   ;	
	?debug	L 174
	les	bx,dword ptr [bp+14]
	mov	bx,word ptr es:[bx+20]
	dec	bx
	cmp	bx,6
	ja	short @8@870
	shl	bx,1
	jmp	word ptr cs:@8@C786[bx]
@8@786:
   ;	
   ;	    case MDL_KIND_KNIGHT:
   ;	    case MDL_KIND_OGRE:
   ;	    case MDL_KIND_DEMON:
   ;	        ent->pain_finished = rdr->anim_time + KNIGHT_PAIN;
   ;	
	?debug	L 178
	les	bx,dword ptr [bp+14]
	fld	dword ptr [di+14]
	fld1	
	fadd	
@8@814:
	fstp	dword ptr es:[bx+60]
   ;	
   ;	        break;
   ;	
	?debug	L 179
	fwait	
	jmp	@8@1038
@8@842:
   ;	
   ;	    case MDL_KIND_SHAMBLER:
   ;	        ent->pain_finished = rdr->anim_time + SHAMBLER_PAIN;
   ;	
	?debug	L 181
	les	bx,dword ptr [bp+14]
	fld	dword ptr [di+14]
	fadd	dword ptr DGROUP:s@+32
	jmp	short @8@814
@8@870:
   ;	
   ;	        break;
   ;	    default:
   ;	        r = rng_float( &world->rng );
   ;	
	?debug	L 184
	push	word ptr [bp+6]
	call	far ptr _rng_float
	pop	cx
	fstp	dword ptr [bp-4]
   ;	
   ;	        ent->pain_finished = rdr->anim_time +
   ;	
	?debug	L 185
   ;	
   ;	            ( r < MDL_PAIN_SHORT_P ? MDL_PAIN_SHORT : MDL_PAIN_LONG );
   ;	
	?debug	L 186
	fld	dword ptr [bp-4]
	fcomp	dword ptr DGROUP:s@+56
	fstsw	word ptr [bp-6]
	fwait	
	mov	ax,word ptr [bp-6]
	sahf	
	jae	short @8@926
	fld	dword ptr DGROUP:s@+60
	jmp	short @8@954
@8@926:
	fld	dword ptr DGROUP:s@+64
@8@954:
	fadd	dword ptr [di+14]
	les	bx,dword ptr [bp+14]
	fstp	dword ptr es:[bx+60]
   ;	
   ;	        /* army_pain: pain1 with the short flinch, pain2 with the others */
   ;	        if ( ent->kind == MDL_KIND_ARMY && r >= MDL_PAIN_SHORT_P ) {
   ;	
	?debug	L 188
	fwait	
	cmp	word ptr es:[bx+20],0
	jne	short @8@1038
	fld	dword ptr [bp-4]
	fcomp	dword ptr DGROUP:s@+56
	fstsw	word ptr [bp-6]
	fwait	
	mov	ax,word ptr [bp-6]
	sahf	
	jb	short @8@1038
   ;	
   ;	            mdl_voice( player, &world->snd, ent, CHAN_VOICE, SND_ARMY_PAIN2, ATTN_NORM );
   ;	
	?debug	L 189
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
	jmp	@8@1206
@8@1038:
   ;	
   ;	            ent->state = MDL_ST_PAIN;
   ;	            ent->anim_frame = 0;
   ;	            return;
   ;	        }
   ;	        break;
   ;	    }
   ;	    mdl_say( player, &world->snd, ent, 2 );
   ;	
	?debug	L 196
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
	?debug	L 198
	les	bx,dword ptr [bp+14]
	cmp	word ptr es:[bx+20],4
	jne	short @8@1122
	push	word ptr [bp+6]
	call	far ptr _rng_float
	pop	cx
	fmul	dword ptr DGROUP:s@+68
	mov	ax,word ptr [bp+18]
	mov	word ptr [bp-6],ax
	fild	word ptr [bp-6]
	fcompp
	fstsw	word ptr [bp-6]
	fwait	
	mov	ax,word ptr [bp-6]
	sahf	
	jae	short @8@1122
	jmp	short @8@1234
@8@1122:
   ;	
   ;	    if ( ent->kind == MDL_KIND_SHAMBLER && rng_float( &world->rng ) * SHAMBLER_PAIN_ROLL > dmg ) return;
   ;	
	?debug	L 199
	les	bx,dword ptr [bp+14]
	cmp	word ptr es:[bx+20],7
	jne	short @8@1206
	push	word ptr [bp+6]
	call	far ptr _rng_float
	pop	cx
	fmul	dword ptr DGROUP:s@+72
	mov	ax,word ptr [bp+18]
	mov	word ptr [bp-6],ax
	fild	word ptr [bp-6]
	fcompp
	fstsw	word ptr [bp-6]
	fwait	
	mov	ax,word ptr [bp-6]
	sahf	
	jae	short @8@1206
	jmp	short @8@1234
@8@1206:
	les	bx,dword ptr [bp+14]
	mov	word ptr es:[bx+22],3
   ;	
   ;	
   ;	    ent->state = MDL_ST_PAIN;
   ;	    ent->anim_frame = 0;
   ;	
	?debug	L 202
	mov	word ptr es:[bx+24],0
@8@1234:
   ;	
   ;	}
   ;	
	?debug	L 203
	pop	di
	pop	si
	leave	
	ret	
	?debug	C E601720E02FCFF0005776F726C64250A06000006+
	?debug	C 706C61796572700A0800000566696768741E0801+
	?debug	C 0A0068030C0600037264727208010D0000000C07+
	?debug	C 0003656E74540A0E000003646D67040A120000
	?debug	E
	?debug	E
_mdl_damage	endp

@8@C786	label	word
	dw	@8@786
	dw	@8@870
	dw	@8@786
	dw	@8@786
	dw	@8@870
	dw	@8@870
	dw	@8@842

s@	label	byte
	db	'-C'
	db	28
	db	-21
	db	-30
	db	'6'
	db	26
	db	'?'
	db	0
	db	0
	db	-128
	db	-65
	db	0
	db	0
	db	-128
	db	'A'
	db	0
	db	0
	db	-64
	db	-63
	db	0
	db	0
	db	0
	db	'B'
	db	0
	db	0
	db	' B'
	db	0
	db	0
	db	'pA'
	db	0
	db	0
	db	0
	db	'@'
	db	0
	db	0
	db	'xBo'
	db	18
	db	-125
	db	':'
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
	db	0
	db	0
	db	-8
	db	'A'
	db	0
	db	0
	db	-128
	db	'@'
	db	0
	db	0
	db	0
	db	'?'
	db	0
	db	0
	db	' C'
	db	0
	db	0
	db	0
	db	'A'
	db	-51
	db	-52
	db	-52
	db	'='
	db	0
	db	0
	db	'zD'
	db	0
	db	0
	db	-64
	db	'@'
	db	0
	db	0
	db	22
	db	'D'
	db	0
	db	0
	db	' @'
	db	0
	db	0
	db	-16
	db	'C'
	db	0
	db	0
	db	-16
	db	'B'
	db	-51
	db	-52
	db	'L?'
	db	0
	db	0
	db	-96
	db	'@'
	db	0
	db	0
	db	-96
	db	'A'
	db	0
	db	0
	db	-80
	db	'A'
	db	0
	db	0
	db	0
	db	'E'
	db	0
	db	0
	db	-64
	db	'?333?'
	db	0
	db	0
	db	'pB'
	db	0
	db	0
	db	-56
	db	'B'
	db	0
	db	0
	db	'HB'
	db	0
	db	0
	db	-16
	db	'A'
