_pl_damage	proc	far
	?debug	B
	enter	10,0
	push	si
	push	di
	?debug	C E603646D67040A0E0000037264722E0A0C000005+
	?debug	C 6669676874720A0A000005776F726C64380A0800+
	?debug	C 0006706C617965721B0A060000
	mov	di,word ptr [bp+6]
	mov	dx,word ptr [bp+8]
	mov	si,word ptr [bp+10]
	?debug	B
   ;	
   ;	{
   ;	    Rng   *rng = &world->rng;
   ;	
	?debug	L 236
	mov	word ptr [bp-2],dx
   ;	
   ;	    Sound *snd = &world->snd;
   ;	
	?debug	L 237
	mov	ax,dx
	add	ax,7358
	mov	word ptr [bp-4],ax
   ;	
   ;	    short save;
   ;	    float prod;
   ;	
   ;	    /* T_Damage: the armor takes ceil(type * damage), and its last point
   ;	       takes the type with it. QuakeC stores the product as a float32 --
   ;	       0.8 * 10 is 8.00000012 at 80 bits, 8.0 stored -- and a cast chops
   ;	       (F_FTOL@), so ceil() it is */
   ;	    prod = fight->armor_type * dmg;
   ;	
	?debug	L 245
	mov	ax,word ptr [bp+14]
	mov	word ptr [bp-10],ax
	fild	word ptr [bp-10]
	fmul	dword ptr [si+65]
	fstp	dword ptr [bp-8]
   ;	
   ;	    save = (short) ceil( prod );
   ;	
	?debug	L 246
	fld	dword ptr [bp-8]
	sub	sp,8
	fstp	qword ptr [bp-22]
	fwait	
	call	far ptr _ceil
	add	sp,8
	call	far ptr F_FTOL@
	mov	cx,ax
   ;	
   ;	    if ( save >= fight->armor ) { save = fight->armor; fight->armor_type = 0.0f; }
   ;	
	?debug	L 247
	cmp	word ptr [si+63],cx
	jg	short @11@86
	mov	cx,word ptr [si+63]
	mov	dword ptr [si+65],large 0
@11@86:
   ;	
   ;	    fight->armor = (short) ( fight->armor - save );
   ;	
	?debug	L 248
	mov	ax,word ptr [si+63]
	sub	ax,cx
	mov	word ptr [si+63],ax
   ;	
   ;	
   ;	    /* then the pentagram stops the rest -- the armor above still wore --
   ;	       and protect3 says so no more than two seconds apart */
   ;	    if ( rdr->anim_time < fight->pent_until ) {
   ;	
	?debug	L 252
	mov	bx,word ptr [bp+12]
	fld	dword ptr [bx+14]
	fcomp	dword ptr [si+91]
	fstsw	word ptr [bp-10]
	fwait	
	mov	ax,word ptr [bp-10]
	sahf	
	jae	short @11@170
   ;	
   ;	        if ( rdr->anim_time >= fight->pent_at ) {
   ;	
	?debug	L 253
	mov	bx,word ptr [bp+12]
	fld	dword ptr [bx+14]
	fcomp	dword ptr [si+103]
	fstsw	word ptr [bp-10]
	fwait	
	mov	ax,word ptr [bp-10]
	sahf	
	jae short	@@2
	jmp	@11@562
@@2:
   ;	
   ;	            fight->pent_at = rdr->anim_time + 2.0f;
   ;	
	?debug	L 254
	mov	bx,word ptr [bp+12]
	fld	dword ptr [bx+14]
	fadd	dword ptr DGROUP:s@+56
	fstp	dword ptr [si+103]
   ;	
   ;	            snd_play( snd, player, SND_PENT_HIT, &player->pos );
   ;	
	?debug	L 255
	push	di
	push	99
	push	di
	fwait	
	push	word ptr [bp-4]
	call	far ptr _snd_play
	jmp	@11@534
@11@170:
   ;	
   ;	        }
   ;	        return;
   ;	    }
   ;	    fight->health = (short) ( fight->health - ( dmg - save ) );
   ;	
	?debug	L 259
	mov	ax,word ptr [si+61]
	mov	dx,word ptr [bp+14]
	sub	dx,cx
	sub	ax,dx
	mov	word ptr [si+61],ax
   ;	
   ;	
   ;	    /* PainSound: drowning under water, a burn in slime or lava -- both
   ;	       every hit -- else one of six grunts a half second apart at most,
   ;	       rint(random() * 5 + 1) so the first and last half as often */
   ;	    if ( fight->health <= 0 ) return;
   ;	
	?debug	L 264
	cmp	word ptr [si+61],0
	jg short	@@3
	jmp	@11@562
@@3:
   ;	
   ;	    if ( rdr->anim_time < fight->invis_until ) return;   /* player_pain: the eyes have no pain frames */
   ;	
	?debug	L 265
	mov	bx,word ptr [bp+12]
	fld	dword ptr [bx+14]
	fcomp	dword ptr [si+135]
	fstsw	word ptr [bp-10]
	fwait	
	mov	ax,word ptr [bp-10]
	sahf	
	jae short	@@4
	jmp	@11@562
@@4:
   ;	
   ;	    if ( player->water_level == 3 && player->water_type == CONTENTS_WATER ) {
   ;	
	?debug	L 266
	cmp	word ptr [di+32],3
	jne	short @11@310
	cmp	word ptr [di+34],-3
	jne	short @11@310
   ;	
   ;	        snd_self( snd, player, CHAN_VOICE, (short) ( SND_DROWN1 + ( rng_int( rng ) & 1 ) ) );
   ;	
	?debug	L 267
	push	word ptr [bp-2]
	call	far ptr _rng_int
	pop	cx
	and	ax,1
	add	ax,124
	jmp	short @11@506
@11@310:
   ;	
   ;	        return;
   ;	    }
   ;	    if ( player->water_level > 0 && player->water_type != CONTENTS_WATER ) {
   ;	
	?debug	L 270
	cmp	word ptr [di+32],0
	jle	short @11@394
	cmp	word ptr [di+34],-3
	je	short @11@394
   ;	
   ;	        snd_self( snd, player, CHAN_VOICE, (short) ( SND_BURN1 + ( rng_int( rng ) & 1 ) ) );
   ;	
	?debug	L 271
	push	word ptr [bp-2]
	call	far ptr _rng_int
	pop	cx
	and	ax,1
	add	ax,22
	jmp	short @11@506
@11@394:
   ;	
   ;	        return;
   ;	    }
   ;	    if ( rdr->anim_time < fight->pain_at ) return;
   ;	
	?debug	L 274
	mov	bx,word ptr [bp+12]
	fld	dword ptr [bx+14]
	fcomp	dword ptr [si+99]
	fstsw	word ptr [bp-10]
	fwait	
	mov	ax,word ptr [bp-10]
	sahf	
	jb	short @11@562
   ;	
   ;	    fight->pain_at = rdr->anim_time + PL_PAIN_GAP;
   ;	
	?debug	L 275
	mov	bx,word ptr [bp+12]
	fld	dword ptr [bx+14]
	fadd	dword ptr DGROUP:s@+68
	fstp	dword ptr [si+99]
   ;	
   ;	    save = (short) ( rng_float( rng ) * 5.0f + 0.5f );
   ;	
	?debug	L 276
	fwait	
	push	word ptr [bp-2]
	call	far ptr _rng_float
	pop	cx
	fmul	dword ptr DGROUP:s@+60
	fadd	dword ptr DGROUP:s@+68
	call	far ptr F_FTOL@
	mov	cx,ax
   ;	
   ;	    snd_self( snd, player, CHAN_VOICE, (short) ( save < 3 ? SND_PAIN1 + save : SND_PAIN4 + save - 3 ) );
   ;	
	?debug	L 277
	cmp	cx,3
	jge	short @11@478
	add	ax,14
	jmp	short @11@506
@11@478:
	mov	ax,cx
	add	ax,113
@11@506:
	push	ax
	push	2
	push	di
	push	word ptr [bp-4]
	call	far ptr _snd_self
@11@534:
	add	sp,8
@11@562:
   ;	
   ;	}
   ;	
	?debug	L 278
	pop	di
	pop	si
	leave	
	ret	
	?debug	C E60470726F640E02F8FF00047361766504080149+
	?debug	C 008C0104010003736E641D02FCFF0003726E672C+
	?debug	C 02FEFF0006706C617965721B08010A0091010C07+
	?debug	C 0005776F726C643808010D0018000C0200056669+
	?debug	C 67687472080110009C010C0600037264722E0A0C+
	?debug	C 000003646D67040A0E0000
	?debug	E
	?debug	E
_pl_damage	endp
