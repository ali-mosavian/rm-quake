_pl_damage	proc	far
	?debug	B
	enter	10,0
	push	si
	push	di
	?debug	C E3190028001E01
	?debug	C E31A000C001E0A
	?debug	C E318000200151904
	?debug	C E31C0022211E0E
	?debug	C E31D0004001E56
	?debug	C E31F0020001E58
	?debug	C E31E000400161F00
	?debug	C E3210010001E5F
	?debug	C E3220006001E66
	?debug	C E3230006001A08
	?debug	C E320000400162100
	?debug	C E3250010001E68
	?debug	C E324000400162500
	?debug	C E3270012001E6B
	?debug	C E326000400162700
	?debug	C E3290006001E70
	?debug	C E328000400162900
	?debug	C E32B0010001E74
	?debug	C E32A000400162B00
	?debug	C E32C000400160400
	?debug	C E32D000400160800
	?debug	C E32F000A001E7B
	?debug	C E32E000400162F00
	?debug	C E3310022001E8081
	?debug	C E3320010001A0E
	?debug	C E3330010001A0E
	?debug	C E330000400163100
	?debug	C E3350012001E8085
	?debug	C E334000400163500
	?debug	C E3370030001E808D
	?debug	C E336000400163700
	?debug	C E3380060001A1A
	?debug	C E3390020001A0E
	?debug	C E33B0034001E8095
	?debug	C E33A000400163B00
	?debug	C E33D0064001E80A3
	?debug	C E33C000400163D00
	?debug	C E33F004C001E80B8
	?debug	C E33E000400163F00
	?debug	C E3410012001E80CB
	?debug	C E340000400164100
	?debug	C E343000A001E80CF
	?debug	C E342000400164300
	?debug	C E3450016001E80D5
	?debug	C E344000400164500
	?debug	C E347005C001E80DC
	?debug	C E3490010001E80DF
	?debug	C E34A000C001A02
	?debug	C E3480050001A49
	?debug	C E346000400164700
	?debug	C E34C0066001E80E3
	?debug	C E34B000400164C00
	?debug	C E34E003C001E80FB
	?debug	C E34D00E0011A4E
	?debug	C E34F0068011A4E
	?debug	C E350000400160200
	?debug	C E351003C001A06
	?debug	C E352003C001A06
	?debug	C E353001E001A04
	?debug	C E354001E001A04
	?debug	C E3550000101A06
	?debug	C E3560084071E810C
	?debug	C E3580018001E8110
	?debug	C E3590010001A02
	?debug	C E3570080071A58
	?debug	C E35A0064041E8114
	?debug	C E35B0016001A08
	?debug	C E35C0042041E811C
	?debug	C E35D000400160400
	?debug	C E35F000A001E8131
	?debug	C E35E000400165F00
	?debug	C E3610024001E8135
	?debug	C E360000400166100
	?debug	C E3630008001E8142
	?debug	C E362000400166300
	?debug	C E3640010001A04
	?debug	C E3650000021A02
	?debug	C E3660000021A02
	?debug	C E31B000200151C04
	?debug	C E368008C051E8148
	?debug	C E3690029001A02
	?debug	C E36A0009001A02
	?debug	C E36B0050001A02
	?debug	C E36D002E001E8182
	?debug	C E36C0050041A6D
	?debug	C E367000200156804
	?debug	C E36F0052041E818F
	?debug	C E371001E001E81C4
	?debug	C E37000C0031A71
	?debug	C E373002C001E81CB
	?debug	C E3740012001A04
	?debug	C E372000400167300
	?debug	C E36E000200156F04
	?debug	C E603646D67040A0E0000037264726E0A0C000005+
	?debug	C 6669676874670A0A000005776F726C641B0A0800+
	?debug	C 0006706C61796572180A060000
	mov	di,word ptr [bp+6]
	mov	dx,word ptr [bp+8]
	mov	si,word ptr [bp+10]
	?debug	B
   ;	
   ;	{
   ;	    Rng   *rng = &world->rng;
   ;	
	?debug	L 15
	mov	word ptr [bp-2],dx
   ;	
   ;	    Sound *snd = &world->snd;
   ;	
	?debug	L 16
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
	?debug	L 24
	mov	ax,word ptr [bp+14]
	mov	word ptr [bp-10],ax
	fild	word ptr [bp-10]
	fmul	dword ptr [si+65]
	fstp	dword ptr [bp-8]
   ;	
   ;	    save = (short) ceil( prod );
   ;	
	?debug	L 25
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
	?debug	L 26
	cmp	word ptr [si+63],cx
	jg	short @1@86
	mov	cx,word ptr [si+63]
	mov	dword ptr [si+65],large 0
@1@86:
   ;	
   ;	    fight->armor = (short) ( fight->armor - save );
   ;	
	?debug	L 27
	mov	ax,word ptr [si+63]
	sub	ax,cx
	mov	word ptr [si+63],ax
   ;	
   ;	
   ;	    /* then the pentagram stops the rest -- the armor above still wore --
   ;	       and protect3 says so no more than two seconds apart */
   ;	    if ( rdr->anim_time < fight->pent_until ) {
   ;	
	?debug	L 31
	mov	bx,word ptr [bp+12]
	fld	dword ptr [bx+14]
	fcomp	dword ptr [si+91]
	fstsw	word ptr [bp-10]
	fwait	
	mov	ax,word ptr [bp-10]
	sahf	
	jae	short @1@170
   ;	
   ;	        if ( rdr->anim_time >= fight->pent_at ) {
   ;	
	?debug	L 32
	mov	bx,word ptr [bp+12]
	fld	dword ptr [bx+14]
	fcomp	dword ptr [si+103]
	fstsw	word ptr [bp-10]
	fwait	
	mov	ax,word ptr [bp-10]
	sahf	
	jae short	@@0
	jmp	@1@562
@@0:
   ;	
   ;	            fight->pent_at = rdr->anim_time + 2.0f;
   ;	
	?debug	L 33
	mov	bx,word ptr [bp+12]
	fld	dword ptr [bx+14]
	fadd	dword ptr DGROUP:s@
	fstp	dword ptr [si+103]
   ;	
   ;	            snd_play( snd, player, SND_PENT_HIT, &player->pos );
   ;	
	?debug	L 34
	push	di
	push	99
	push	di
	fwait	
	push	word ptr [bp-4]
	call	far ptr _snd_play
	jmp	@1@534
@1@170:
   ;	
   ;	        }
   ;	        return;
   ;	    }
   ;	    fight->health = (short) ( fight->health - ( dmg - save ) );
   ;	
	?debug	L 38
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
	?debug	L 43
	cmp	word ptr [si+61],0
	jg short	@@1
	jmp	@1@562
@@1:
   ;	
   ;	    if ( rdr->anim_time < fight->invis_until ) return;   /* player_pain: the eyes have no pain frames */
   ;	
	?debug	L 44
	mov	bx,word ptr [bp+12]
	fld	dword ptr [bx+14]
	fcomp	dword ptr [si+135]
	fstsw	word ptr [bp-10]
	fwait	
	mov	ax,word ptr [bp-10]
	sahf	
	jae short	@@2
	jmp	@1@562
@@2:
   ;	
   ;	    if ( player->water_level == 3 && player->water_type == CONTENTS_WATER ) {
   ;	
	?debug	L 45
	cmp	word ptr [di+32],3
	jne	short @1@310
	cmp	word ptr [di+34],-3
	jne	short @1@310
   ;	
   ;	        snd_self( snd, player, CHAN_VOICE, (short) ( SND_DROWN1 + ( rng_int( rng ) & 1 ) ) );
   ;	
	?debug	L 46
	push	word ptr [bp-2]
	call	far ptr _rng_int
	pop	cx
	and	ax,1
	add	ax,124
	jmp	short @1@506
@1@310:
   ;	
   ;	        return;
   ;	    }
   ;	    if ( player->water_level > 0 && player->water_type != CONTENTS_WATER ) {
   ;	
	?debug	L 49
	cmp	word ptr [di+32],0
	jle	short @1@394
	cmp	word ptr [di+34],-3
	je	short @1@394
   ;	
   ;	        snd_self( snd, player, CHAN_VOICE, (short) ( SND_BURN1 + ( rng_int( rng ) & 1 ) ) );
   ;	
	?debug	L 50
	push	word ptr [bp-2]
	call	far ptr _rng_int
	pop	cx
	and	ax,1
	add	ax,22
	jmp	short @1@506
@1@394:
   ;	
   ;	        return;
   ;	    }
   ;	    if ( rdr->anim_time < fight->pain_at ) return;
   ;	
	?debug	L 53
	mov	bx,word ptr [bp+12]
	fld	dword ptr [bx+14]
	fcomp	dword ptr [si+99]
	fstsw	word ptr [bp-10]
	fwait	
	mov	ax,word ptr [bp-10]
	sahf	
	jb	short @1@562
   ;	
   ;	    fight->pain_at = rdr->anim_time + PL_PAIN_GAP;
   ;	
	?debug	L 54
	mov	bx,word ptr [bp+12]
	fld	dword ptr [bx+14]
	fadd	dword ptr DGROUP:s@+4
	fstp	dword ptr [si+99]
   ;	
   ;	    save = (short) ( rng_float( rng ) * 5.0f + 0.5f );
   ;	
	?debug	L 55
	fwait	
	push	word ptr [bp-2]
	call	far ptr _rng_float
	pop	cx
	fmul	dword ptr DGROUP:s@+8
	fadd	dword ptr DGROUP:s@+4
	call	far ptr F_FTOL@
	mov	cx,ax
   ;	
   ;	    snd_self( snd, player, CHAN_VOICE, (short) ( save < 3 ? SND_PAIN1 + save : SND_PAIN4 + save - 3 ) );
   ;	
	?debug	L 56
	cmp	cx,3
	jge	short @1@478
	add	ax,14
	jmp	short @1@506
@1@478:
	mov	ax,cx
	add	ax,113
@1@506:
	push	ax
	push	2
	push	di
	push	word ptr [bp-4]
	call	far ptr _snd_self
@1@534:
	add	sp,8
@1@562:
   ;	
   ;	}
   ;	
	?debug	L 57
	pop	di
	pop	si
	leave	
	ret	
	?debug	C E375000200155A04
	?debug	C E376000200151D04
	?debug	C E60470726F640E02F8FF00047361766504080149+
	?debug	C 008C0104010003736E647502FCFF0003726E6776+
	?debug	C 02FEFF0006706C617965721808010A0091010C07+
	?debug	C 0005776F726C641B08010D0018000C0200056669+
	?debug	C 67687467080110009C010C0600037264726E0A0C+
	?debug	C 000003646D67040A0E0000
	?debug	E
	?debug	E
_pl_damage	endp

s@	label	byte
	db	1
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
