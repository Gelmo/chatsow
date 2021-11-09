/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "import.h"

#if !defined ( ENDIAN_LITTLE ) && !defined ( ENDIAN_BIG )
short ( *BigShort )(short l);
short ( *LittleShort )(short l);
int ( *BigLong )(int l);
int ( *LittleLong )(int l);
float ( *BigFloat )(float l);
float ( *LittleFloat )(float l);
#endif

short   ShortSwap( short l )
{
	qbyte b1, b2;

	b1 = l&255;
	b2 = ( l>>8 )&255;

	return ( b1<<8 ) + b2;
}

#if !defined ( ENDIAN_LITTLE ) && !defined ( ENDIAN_BIG )
static short   ShortNoSwap( short l )
{
	return l;
}
#endif

int    LongSwap( int l )
{
	qbyte b1, b2, b3, b4;

	b1 = l&255;
	b2 = ( l>>8 )&255;
	b3 = ( l>>16 )&255;
	b4 = ( l>>24 )&255;

	return ( (int)b1<<24 ) + ( (int)b2<<16 ) + ( (int)b3<<8 ) + b4;
}

#if !defined ( ENDIAN_LITTLE ) && !defined ( ENDIAN_BIG )
static int     LongNoSwap( int l )
{
	return l;
}
#endif

float FloatSwap( float f )
{
	union
	{
		float f;
		qbyte b[4];
	} dat1, dat2;


	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

#if !defined ( ENDIAN_LITTLE ) && !defined ( ENDIAN_BIG )
static float FloatNoSwap( float f )
{
	return f;
}
#endif


/*
* Swap_Init
*/
void Swap_Init( void )
{
#if !defined ( ENDIAN_LITTLE ) && !defined ( ENDIAN_BIG )
	qbyte swaptest[2] = { 1, 0 };

	// set the byte swapping variables in a portable manner
	if( *(short *)swaptest == 1 )
	{
		BigShort = ShortSwap;
		LittleShort = ShortNoSwap;
		BigLong = LongSwap;
		LittleLong = LongNoSwap;
		BigFloat = FloatSwap;
		LittleFloat = FloatNoSwap;
	}
	else
	{
		BigShort = ShortNoSwap;
		LittleShort = ShortSwap;
		BigLong = LongNoSwap;
		LittleLong = LongSwap;
		BigFloat = FloatNoSwap;
		LittleFloat = FloatSwap;
	}
#endif
}

int read_char(msg_t *msg) {
	int i = (signed char)msg->data[msg->readcount++];
	if(msg->readcount > msg->cursize)
		i = -1;
	return i;
}

int read_byte(msg_t *msg) {
    int i = (qbyte)msg->data[msg->readcount++];
    if (msg->readcount > msg->cursize)
        return -1;
    return i;
}

int read_short(msg_t *msg)
{
	int i;
	short *sp = (short *)&msg->data[msg->readcount];
	i = LittleShort(*sp);
	msg->readcount += 2;
	if (msg->readcount > msg->cursize )
		i = -1;
	return i;
}

int read_long(msg_t *msg) {
	int i;
	unsigned int *ip = (unsigned int *)&msg->data[msg->readcount];
	i = LittleLong(*ip);
	msg->readcount += 4;
	if (msg->readcount > msg->cursize)
		i = -1;
	return i;
}

int read_int3(msg_t *msg)
{
	int i = msg->data[msg->readcount]
	| ( msg->data[msg->readcount+1]<<8 )
		| ( msg->data[msg->readcount+2]<<16 )
		| ( ( msg->data[msg->readcount+2] & 0x80 ) ? ~0xFFFFFF : 0 );
	msg->readcount += 3;
	if( msg->readcount > msg->cursize )
		i = -1;
	return i;
}

char *read_string(msg_t *msg) {
	int l, c;
	static char string[MAX_MSG_STRING_CHARS];

	l = 0;
	do {
		c = read_byte(msg);
		if (c == -1 || c == '\0')
            break;

		string[l] = c;
		l++;
	} while ((unsigned int)l < sizeof(string) - 1);
	string[l] = '\0';

	return string;
}

void read_data(msg_t *msg, void *data, size_t length) {
	unsigned int i;
	for (i = 0; i < length; i++)
        ((qbyte *)data)[i] = read_byte(msg);
}

unsigned read_entity_bits(msg_t *msg) {
	unsigned b, total;

	total = (qbyte)read_byte(msg);
	if (total & U_MOREBITS1) {
		b = (qbyte)read_byte(msg);
		total |= (b<<8)&0x0000FF00;
	}
	if (total & U_MOREBITS2) {
		b = (qbyte)read_byte(msg);
		total |= (b<<16)&0x00FF0000;
	}
	if (total & U_MOREBITS3) {
		b = (qbyte)read_byte(msg);
		total |= (b<<24)&0xFF000000;
	}

	if (total & U_NUMBER16)
		read_short(msg);
	else
		read_byte(msg);

    return total;
}

void read_delta_entity(msg_t *msg, unsigned bits) {
    qboolean linear_projectile = qfalse;
    short solid = 0;

	if( bits & U_TYPE )
        linear_projectile = ( read_byte( msg ) & ET_INVERSE ) ? qtrue : qfalse;

	if( bits & U_SOLID )
		solid = read_short( msg );

	if( bits & U_MODEL )
		read_short( msg );
	if( bits & U_MODEL2 )
		read_short( msg );

	if( bits & U_FRAME8 )
		read_byte( msg );
	if( bits & U_FRAME16 )
		read_short( msg );

	if( ( bits & U_SKIN8 ) && ( bits & U_SKIN16 ) )
		read_long( msg );
	else if( bits & U_SKIN8 )
		read_byte( msg );
	else if( bits & U_SKIN16 )
		read_short( msg );

	if( ( bits & ( U_EFFECTS8|U_EFFECTS16 ) ) == ( U_EFFECTS8|U_EFFECTS16 ) )
		read_long( msg );
	else if( bits & U_EFFECTS8 )
		read_byte( msg );
	else if( bits & U_EFFECTS16 )
		read_short( msg );

	if( linear_projectile ) {
		if( bits & U_ORIGIN1 )
			read_coord_dummy( msg );
		if( bits & U_ORIGIN2 )
			read_coord_dummy( msg );
		if( bits & U_ORIGIN3 )
			read_coord_dummy( msg );
	} else {
		if( bits & U_ORIGIN1 )
			read_coord_dummy( msg );
		if( bits & U_ORIGIN2 )
			read_coord_dummy( msg );
		if( bits & U_ORIGIN3 )
			read_coord_dummy( msg );
	}

	if( ( bits & U_ANGLE1 ) && ( solid == SOLID_BMODEL ) )
		read_angle16_dummy( msg );
	else if( bits & U_ANGLE1 )
		read_angle_dummy( msg );

	if( ( bits & U_ANGLE2 ) && ( solid == SOLID_BMODEL ) )
		read_angle16_dummy( msg );
	else if( bits & U_ANGLE2 )
		read_angle_dummy( msg );

	if( ( bits & U_ANGLE3 ) && ( solid == SOLID_BMODEL ) )
		read_angle16_dummy( msg );
	else if( bits & U_ANGLE3 )
		read_angle_dummy( msg );

	if( bits & U_OTHERORIGIN )
        read_pos_dummy(msg, NULL);

	if( bits & U_SOUND )
		read_short( msg );

	if( bits & U_EVENT ) {
		int event = (qbyte)read_byte( msg );
		if( event & EV_INVERSE )
			read_byte( msg );
	}

	if( bits & U_EVENT2 ) {
		int event = (qbyte)read_byte( msg );
		if( event & EV_INVERSE )
			read_byte( msg );
	}

	if( bits & U_ATTENUATION )
		read_byte( msg );

	if( bits & U_WEAPON )
		read_byte( msg );

	if( bits & U_SVFLAGS )
		read_short( msg );

	if( bits & U_LIGHT )
        read_long( msg );

	if( bits & U_TEAM )
		read_byte( msg );
}

int skip_data(msg_t *msg, size_t length) {
    if (msg->readcount + length <= msg->cursize) {
        msg->readcount += length;
        return 1;
    }
    return 0;
}

void write_data( msg_t *msg, const void *data, size_t length ) {
    memcpy(msg->data + msg->cursize, data, length);
    msg->cursize += length;
}

void write_char( msg_t *msg, int c ) {
    msg->data[msg->cursize++] = (char)c;
}

void write_byte( msg_t *msg, int c ) {
    msg->data[msg->cursize++] = (qbyte)(c & 0xff);
}

void write_short( msg_t *msg, int c ) {
    msg->data[msg->cursize++] = (qbyte)(c & 0xff);
    msg->data[msg->cursize++] = (qbyte)((c >> 8) & 0xff);
}

void write_int3( msg_t *msg, int c ) {
    msg->data[msg->cursize++] = (qbyte)(c & 0xff);
    msg->data[msg->cursize++] = (qbyte)((c >> 8) & 0xff);
    msg->data[msg->cursize++] = (qbyte)((c >> 16) & 0xff);
}

void write_long( msg_t *msg, int c ) {
	unsigned int *ip = (unsigned int *)(msg->data + msg->cursize);
	*ip = LittleLong( c );
    msg->cursize += 4;
}

void write_float( msg_t *msg, float f ) {
	union {
		float f;
		int l;
	} dat;

	dat.f = f;
	write_long( msg, dat.l );
}
