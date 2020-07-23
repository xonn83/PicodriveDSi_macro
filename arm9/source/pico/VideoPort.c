// This is part of Pico Library

// Most code (c) Copyright 2004 Dave, All rights reserved.
// Some code (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "PicoInt.h"

static __inline void AutoIncrement()
{
  Pico.video.addr=(unsigned short)(Pico.video.addr+Pico.video.reg[0xf]);
}

static void VideoWrite(unsigned int d)
{
  unsigned int a=0;
  unsigned short sd=(unsigned short)d;

  a=Pico.video.addr;
  if (a&1) d=((d<<8)&0xff00)|(d>>8); // If address is odd, bytes are swapped
  a>>=1;

  switch (Pico.video.type)
  {
    case 1: Pico.vram [a&0x7fff]=sd; break;
    case 3: Pico.cram [a&0x003f]=sd; break;
    case 5: Pico.vsram[a&0x003f]=sd; break;
  }
  
  AutoIncrement();
}

static unsigned int VideoRead()
{
  unsigned int a=0,d=0;

  a=Pico.video.addr; a>>=1;

  switch (Pico.video.type)
  {
    case 0: d=Pico.vram [a&0x7fff]; break;
    case 8: d=Pico.cram [a&0x003f]; break;
    case 4: d=Pico.vsram[a&0x003f]; break;
  }
  
  AutoIncrement();
  return d;
}

static int GetDmaSource()
{
  struct PicoVideo *pvid=&Pico.video;
  int source=0;
  source =pvid->reg[0x15]<<1;
  source|=pvid->reg[0x16]<<9;
  source|=pvid->reg[0x17]<<17;
  return source;
}

static int GetDmaLength()
{
  struct PicoVideo *pvid=&Pico.video;
  int len=0;
  // 16-bit words to transfer:
  len =pvid->reg[0x13];
  len|=pvid->reg[0x14]<<8;
  return len;
}

static void DmaSlow(int source,int len)
{
  int i=0,max=0;

  if (source>=0x800000 && source<0xe00000) return; // Invalid source address

  /// Clip Cram DMA size (Todds Adventures in Slime World):
  if (Pico.video.type==3) { max=0x80-Pico.video.addr; if (len>max) len=max; }

  for (i=0;i<len;i++)
  {
    VideoWrite(PicoRead16(source));
    source+=2;
  }
}

static void DmaCopy(int source,int len)
{
  int i=0;

  len>>=1; // Length specifies number of bytes

  for (i=0;i<len;i++)
  {
    VideoWrite(Pico.vram[source&0x7fff]);
    source+=2;
  }
}

static void DmaFill(int data)
{
  int len,i=0;
  unsigned short *a=&Pico.video.addr;
  unsigned char *vram=(unsigned char *) Pico.vram;
  unsigned char high = (unsigned char) ((data >> 8) & 0xFF);
  
  len=GetDmaLength();

  //for (i=0;i<=len;i++) VideoWrite(data);

  // from Charles MacDonald's genvdp.txt:
  // Write lower byte to address specified
  vram[*a ^ 1] = (unsigned char) (data & 0xFF);

  for(i=0;i<len;i++) {
    // Write upper byte to adjacent address
	// (here we are byteswapped, so address is already 'adjacent')
    vram[*a] = high;

    // Increment address register
    AutoIncrement();
  }
}

static void CommandDma()
{
  struct PicoVideo *pvid=&Pico.video;
  int len=0,method=0,source=0;

  if ((pvid->reg[1]&0x10)==0) return; // DMA not enabled

  len=GetDmaLength();

  method=pvid->reg[0x17]>>6;
  source=GetDmaSource();
  if (method< 2) DmaSlow(source,len); // 68000 to VDP
  if (method==3) DmaCopy(source,len); // VRAM Copy
}

static void CommandChange()
{
  struct PicoVideo *pvid=&Pico.video;
  unsigned int cmd=0,addr=0;

  cmd=pvid->command;

  // Get type of transfer 0xc0000030 (v/c/vsram read/write)
  pvid->type=(unsigned char)(((cmd>>2)&0xc)|(cmd>>30));

  // Get address 0x3fff0003
  addr =(cmd>>16)&0x3fff;
  addr|=(cmd<<14)&0xc000;
  pvid->addr=(unsigned short)addr;

  // Check for dma:
  if (cmd&0x80) CommandDma();
}

void PicoVideoWrite(unsigned int a,unsigned int d)
{
  struct PicoVideo *pvid=&Pico.video;

  a&=0x1c;
  d=(unsigned short)d;

  if (a==0x00) // Data port 0 or 2
  {    
    if (pvid->pending) CommandChange();
    pvid->pending=0;

    // If a DMA fill has been set up, do it
    if ((pvid->command&0x80) && (pvid->reg[1]&0x10) && (pvid->reg[0x17]>>6)==2)
    {
      DmaFill(d);
    }
    else
    {
      VideoWrite(d);
    }
    return;
  }

  if (a==0x04) // Command port 4 or 6
  {
    if (pvid->pending)
    {
      // Low word of command:
      pvid->command&=0xffff0000;
      pvid->command|=d;
      pvid->pending=0;
      CommandChange();
      return;
    }

    if ((d&0xc000)==0x8000)
    {
      // Register write:
      int num=(d>>8)&0x1f;
      pvid->reg[num]=(unsigned char)d;
      return;
    }

    // High word of command:
    pvid->command&=0x0000ffff;
    pvid->command|=d<<16;
    pvid->pending=1;
  }
}

unsigned int PicoVideoRead(unsigned int a)
{
  unsigned int d=0;
  
  a&=0x1c;

  if (a==0x00) { d=VideoRead(); goto end; }

  if (a==0x04)
  {
    d=Pico.video.status;
    d|=0x3400; // always set bits
	d|=0x0020; // sprite collision

    // Toggle fifo full empty (who uses that stuff?):
    if (Pico.m.rotate&4) d|=0x0100; else d|=0x0200;
    if (Pico.m.rotate&2) d|=0x0004; // Toggle in/out of H-Blank
    Pico.m.rotate++;

    goto end;
  }

  if ((a&0x1c)==0x08)
  {
    if (Pico.m.scanline>-100) d=Pico.m.scanline; // V-Counter
    else                      d=Pico.m.rotate++; // Fudge

    d&=0xff; d<<=8;
	d|=((500-SekCycles)>>1)&0xff; // very preliminary H-counter. Seems to be enough for Comix Zone/Sonic 3D Blast
    goto end;
  }

end:

  return d;
}
