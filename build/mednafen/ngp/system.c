#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "flash.h"
#include "system.h"
#include "rom.h"

#include "../general.h"
#include "../state.h"

#include <streams/file_stream.h>

/* ── 링크 케이블 ────────────────────────────────────────────────────
   네오지오 포켓의 2인 대전은 **기기 두 대를 케이블로 잇는** 방식이다.
   BIOS·인터럽트는 이미 이 셋을 부르고 있었고 몸통만 비어 있었다 — 채우면 2인 대전이 열린다.

   전송로는 이름있는 파이프 두 개다. 환경변수로 준다:
     NGP_LINK_OUT  내가 쓰는 쪽
     NGP_LINK_IN   내가 읽는 쪽
   두 인스턴스가 서로 엇갈리게 지정하면 케이블이 된다.
   환경변수가 없으면 예전 그대로 아무 일도 안 한다 — 1인용에는 영향이 없다.

   파이프는 O_RDWR 로 연다. 쓰기 전용으로 열면 상대가 읽기로 열 때까지 **막힌다** —
   두 인스턴스가 서로를 기다리다 둘 다 안 뜬다. O_RDWR 는 리눅스에서 안 막힌다.

   호출 규약(biosHLE.c·interrupt.c 확인):
     write(d)      한 바이트 보낸다. 늘 성공한 것으로 친다.
     read(NULL)    **받은 바이트 수**를 돌려준다 (VECT_COMRECIVESTATUS).
     read(&b)      한 바이트 꺼낸다. 없으면 0.
     poll(&b)      수신 인터럽트용. 매 스캔라인 불리므로 싸야 한다. */
#include <fcntl.h>
#include <unistd.h>

#define NGPLINK_RING 1024
static int   ngplink_in = -1, ngplink_out = -1, ngplink_tried = 0;
static uint8_t ngplink_buf[NGPLINK_RING];
static int   ngplink_head, ngplink_tail;
static long  ngplink_nr, ngplink_nw, ngplink_np, ngplink_got;   /* 진단용 계수기 */

/* 초기화 때 **한 번** 붙잡는다. 한 프로세스에서 코어 두 벌을 돌릴 때
   (링크 하네스가 그렇게 한다) 나중에 열면 두 벌이 같은 환경변수를 보게 되므로,
   각자 retro_init 하는 그 순간의 값을 잡아 둬야 서로 다른 파이프를 쥔다. */
void ngplink_init(void)
{
   const char *i, *o;
   if (ngplink_tried) return;
   ngplink_tried = 1;
   i = getenv("NGP_LINK_IN");
   o = getenv("NGP_LINK_OUT");
   if (i && *i) ngplink_in  = open(i, O_RDWR | O_NONBLOCK);
   if (o && *o) ngplink_out = open(o, O_RDWR | O_NONBLOCK);
}
static void ngplink_open(void){ ngplink_init(); }

/* 들어온 것을 한 번에 긁어 온다 — 스캔라인마다 read() 를 때리지 않으려고 */
static void ngplink_drain(void)
{
   uint8_t tmp[256];
   int n, k;
   ngplink_open();
   if (ngplink_in < 0) return;
   while ((n = (int)read(ngplink_in, tmp, sizeof tmp)) > 0)
   {
      for (k = 0; k < n; k++)
      {
         int nx = (ngplink_head + 1) % NGPLINK_RING;
         if (nx == ngplink_tail) return;      /* 넘치면 버린다 — 밀린 통신은 어차피 늦었다 */
         ngplink_buf[ngplink_head] = tmp[k];
         ngplink_head = nx; ngplink_got++;
      }
      if (n < (int)sizeof tmp) break;
   }
}

static int ngplink_count(void)
{
   return (ngplink_head - ngplink_tail + NGPLINK_RING) % NGPLINK_RING;
}

static int ngplink_pop(uint8_t *b)
{
   if (ngplink_head == ngplink_tail) return 0;
   *b = ngplink_buf[ngplink_tail];
   ngplink_tail = (ngplink_tail + 1) % NGPLINK_RING;
   return 1;
}

static void ngplink_dbg(void)
{
   static int on = -1;
   if (on < 0){ const char *e = getenv("NGP_LINK_DBG"); on = (e && *e=='1'); }
   if (on) fprintf(stderr, "[link] read=%ld poll=%ld write=%ld 받은바이트=%ld\n",
                   ngplink_nr, ngplink_np, ngplink_nw, ngplink_got);
}

int system_comms_read(uint8_t* buffer)
{
   ngplink_nr++; if((ngplink_nr % 2000) == 1) ngplink_dbg();
   ngplink_drain();
   if (!buffer) return ngplink_count();      /* 개수를 묻는 호출 — 진짜 개수를 준다 */
   return ngplink_pop(buffer);
}

int system_comms_poll(uint8_t* buffer)
{
   ngplink_np++;
   if (ngplink_head == ngplink_tail) ngplink_drain();
   if (!buffer) return ngplink_count();
   return ngplink_pop(buffer);
}

void system_comms_write(uint8_t data)
{
   ngplink_nw++; if((ngplink_nw % 200) == 1) ngplink_dbg();
   ngplink_open();
   if (ngplink_out < 0) return;
   if (write(ngplink_out, &data, 1) < 0) { /* 상대가 안 읽어도 게임은 계속 돈다 */ }
}

bool system_io_flash_read(uint8_t *s, uint32_t len)
{
   char path_str[1024];
   RFILE *flash_fp = NULL;

   MDFN_MakeFName(MDFNMKF_SAV, path_str, sizeof(path_str), 0, "flash");
   flash_fp = filestream_open(path_str,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!flash_fp)
      return 0;

   filestream_read(flash_fp, s, len);
   filestream_close(flash_fp);

   return 1;
}

bool system_io_flash_write(uint8_t *s, uint32_t len)
{
   char path_str[1024];
   RFILE *flash_fp = NULL;

   MDFN_MakeFName(MDFNMKF_SAV, path_str, sizeof(path_str), 0, "flash");
   flash_fp = filestream_open(path_str,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!flash_fp)
      return 0;

   filestream_write(flash_fp, s, len);
   filestream_close(flash_fp);

   return 1;
}

int FLASH_StateAction(void *data, int load, int data_only)
{
   int32_t FlashLength = 0;
   uint8_t *flashdata = NULL;
   SFORMAT FINF_StateRegs[] =
   {
      { &FlashLength, sizeof(FlashLength), 0x80000000, "FlashLength" },
      { 0, 0, 0, 0 }
   };
   SFORMAT FLSH_StateRegs[] =
   {
      { flashdata, (uint32_t)FlashLength, 0, "flashdata" },
      { 0, 0, 0, 0 }
   };

   if(!load)
      flashdata = make_flash_commit(&FlashLength);

   if(!MDFNSS_StateAction(data, load, data_only, FINF_StateRegs, "FINF", false))
   {
      if(flashdata)
         free(flashdata);
      return 0;
   }

   if(!FlashLength) // No flash data to save, OR no flash data to load.
   {
      if(flashdata)
         free(flashdata);
      return 1;
   }

   if(load)
      flashdata = (uint8_t *)malloc(FlashLength);

   (*FLSH_StateRegs).v = flashdata;
   (*FLSH_StateRegs).size = FlashLength;

   if(!MDFNSS_StateAction(data, load, data_only, FLSH_StateRegs, "FLSH", false))
   {
      free(flashdata);
      return 0;
   }

   if(load)
   {
      memcpy(ngpc_rom.data, ngpc_rom.orig_data, ngpc_rom.length);
      do_flash_read(flashdata);
   }

   free(flashdata);
   return 1;
}
