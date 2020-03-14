#include "riscv_inst.h"
#include <assert.h>

typedef enum {
  INST_OP_UNSPECIFIED,  /* an instruction we don't care about for this module */
  INST_OP_BEQ,
  INST_OP_BNE,
  INST_OP_BLT,
  INST_OP_BLTU,
  INST_OP_BGE,
  INST_OP_BGEU,
  INST_OP_JAL,
  INST_OP_JALR,
  INST_OP_C_J,
  INST_OP_C_JAL,
  INST_OP_C_JR,
  INST_OP_C_JALR,
  INST_OP_C_BEQZ,
  INST_OP_C_BNEZ,
} INST_OP;

typedef struct {
  INST_OP op;
  uint32_t length;
  uint32_t rs1;
  uint32_t rs2;
  uint32_t rd; 
  uint32_t imm;
} INST_DECODE_INFO;

#define BITS(val, startbit, numbits)  (((val) >> (startbit)) & ((1 << (numbits)) - 1))

uint32_t sign_extend_32(uint32_t val, int signbit)
{
  uint32_t mask = ~((1 << signbit) - 1);
  if (BITS(val, signbit, 1)) {
    return val | mask;
  } else {
    return val &~ mask;
  }
}

void riscv_inst_decode(uint32_t inst, INST_DECODE_INFO *info)
{
  if ((inst & MASK_BEQ) == MATCH_BEQ) {
    info->op = INST_OP_BEQ;
    info->length = 4;
    info->rs1 = BITS(inst, 15, 5);
    info->rs2 = BITS(inst, 20, 5);
    info->imm = sign_extend_32((BITS(inst, 31, 1) << 12) | (BITS(inst, 7, 1) << 11) | (BITS(inst, 25, 6) << 5) | (BITS(inst, 8, 4) << 1), 12);
  } else if ((inst & MASK_BNE) == MATCH_BNE) {
    info->op = INST_OP_BNE;
    info->length = 4;    
    info->rs1 = BITS(inst, 15, 5);
    info->rs2 = BITS(inst, 20, 5);
    info->imm = sign_extend_32((BITS(inst, 31, 1) << 12) | (BITS(inst, 7, 1) << 11) | (BITS(inst, 25, 6) << 5) | (BITS(inst, 8, 4) << 1), 12);
  } else if ((inst & MASK_BLT) == MATCH_BLT) {
    info->op = INST_OP_BLT;
    info->length = 4;    
    info->rs1 = BITS(inst, 15, 5);
    info->rs2 = BITS(inst, 20, 5);
    info->imm = sign_extend_32((BITS(inst, 31, 1) << 12) | (BITS(inst, 7, 1) << 11) | (BITS(inst, 25, 6) << 5) | (BITS(inst, 8, 4) << 1), 12);
  } else if ((inst & MASK_BLTU) == MATCH_BLTU) {
    info->op = INST_OP_BLTU;
    info->length = 4;    
    info->rs1 = BITS(inst, 15, 5);
    info->rs2 = BITS(inst, 20, 5);
    info->imm = sign_extend_32((BITS(inst, 31, 1) << 12) | (BITS(inst, 7, 1) << 11) | (BITS(inst, 25, 6) << 5) | (BITS(inst, 8, 4) << 1), 12);
  } else if ((inst & MASK_BGE) == MATCH_BGE) {
    info->op = INST_OP_BGE;
    info->length = 4;    
    info->rs1 = BITS(inst, 15, 5);
    info->rs2 = BITS(inst, 20, 5);
    info->imm = sign_extend_32((BITS(inst, 31, 1) << 12) | (BITS(inst, 7, 1) << 11) | (BITS(inst, 25, 6) << 5) | (BITS(inst, 8, 4) << 1), 12);
  } else if ((inst & MASK_BGEU) == MATCH_BGEU) {
    info->op = INST_OP_BGEU;
    info->length = 4;    
    info->rs1 = BITS(inst, 15, 5);
    info->rs2 = BITS(inst, 20, 5);
    info->imm = sign_extend_32((BITS(inst, 31, 1) << 12) | (BITS(inst, 7, 1) << 11) | (BITS(inst, 25, 6) << 5) | (BITS(inst, 8, 4) << 1), 12);
  } else if ((inst & MASK_JAL) == MATCH_JAL) {
    info->op = INST_OP_JAL;
    info->length = 4;    
    info->rd = BITS(inst, 7, 5);
    info->imm = sign_extend_32((BITS(inst, 31, 1) << 20) | (BITS(inst, 12, 8) << 12) | (BITS(inst, 20, 1) << 11) | (BITS(inst, 21, 10) << 1), 20);
  } else if ((inst & MASK_JALR) == MATCH_JALR) {
    info->op = INST_OP_JALR;
    info->length = 4;    
    info->rd = BITS(inst, 7, 5);
    info->rs1 = BITS(inst, 15, 5);    
    info->imm = sign_extend_32(BITS(inst, 20, 12), 11);
  } else if ((inst & MASK_C_J) == MATCH_C_J) {
    info->op = INST_OP_C_J;
    info->length = 2;
    info->imm = sign_extend_32(
			       (BITS(inst, 2, 1) << 5) |
			       (BITS(inst, 3, 3) << 1) |
			       (BITS(inst, 6, 1) << 7) |
			       (BITS(inst, 7, 1) << 6) |
			       (BITS(inst, 8, 1) << 10) |
			       (BITS(inst, 9, 2) << 8) |
			       (BITS(inst, 11, 1) << 4) |
			       (BITS(inst, 12, 1) << 11), 11);
  } else if ((inst & MASK_C_JAL) == MATCH_C_JAL) {
    info->op = INST_OP_C_JAL;
    info->length = 2;
    info->imm = sign_extend_32(
			       (BITS(inst, 2, 1) << 5) |
			       (BITS(inst, 3, 3) << 1) |
			       (BITS(inst, 6, 1) << 7) |
			       (BITS(inst, 7, 1) << 6) |
			       (BITS(inst, 8, 1) << 10) |
			       (BITS(inst, 9, 2) << 8) |
			       (BITS(inst, 11, 1) << 4) |
			       (BITS(inst, 12, 1) << 11), 11);
  } else if ((inst & MASK_C_JR) == MATCH_C_JR) {
    info->op = INST_OP_C_JR;
    info->length = 2;
    info->rs1 = BITS(inst, 7, 5);
  } else if ((inst & MASK_C_JALR) == MATCH_C_JALR) {
    info->op = INST_OP_C_JALR;
    info->length = 2;
    info->rs1 = BITS(inst, 7, 5);
  } else if ((inst & MASK_C_BEQZ) == MATCH_C_BEQZ) {
    info->op = INST_OP_C_BEQZ;
    info->length = 2;
    info->rs1 = BITS(inst, 7, 3) + 8;
    info->imm = sign_extend_32(
      ((BITS(inst, 2, 1)) << 5) |
      ((BITS(inst, 3, 2)) << 1) |
      ((BITS(inst, 5, 2)) << 6) |
      ((BITS(inst, 10, 2)) << 3) |
      ((BITS(inst, 12, 1)) << 8), 8);
  } else if ((inst & MASK_C_BNEZ) == MATCH_C_BNEZ) {
    info->op = INST_OP_C_BNEZ;
    info->length = 2;
    info->rs1 = BITS(inst, 7, 3) + 8;
    info->imm = sign_extend_32(
      ((BITS(inst, 2, 1)) << 5) |
      ((BITS(inst, 3, 2)) << 1) |
      ((BITS(inst, 5, 2)) << 6) |
      ((BITS(inst, 10, 2)) << 3) |
      ((BITS(inst, 12, 1)) << 8), 8);
  } else {
    info->op = INST_OP_UNSPECIFIED;
    info->length = ((inst & 0x3) == 0x3) ? 32 : 16;
  }
}

int riscv_inst_length(uint32_t inst)
{
  return ((inst & 0x3) == 0x3) ? 32 : 16;
}

uint32_t riscv_next_pc(uint32_t pc_cur, uint32_t inst, RISCV_X_UNSIGNED *gprs)
{
  INST_DECODE_INFO info;

  riscv_inst_decode(inst, &info);
  switch (info.op) {
  case INST_OP_BEQ:
    return gprs[info.rs1] == gprs[info.rs2] ? pc_cur + info.imm : pc_cur + info.length;
  case INST_OP_BNE:
    return gprs[info.rs1] != gprs[info.rs2] ? pc_cur + info.imm : pc_cur + info.length;
  case INST_OP_BLT:
    return (RISCV_X_SIGNED)gprs[info.rs1] < (RISCV_X_SIGNED)gprs[info.rs2] ? pc_cur + info.imm : pc_cur + info.length;
  case INST_OP_BLTU:
    return gprs[info.rs1] < gprs[info.rs2] ? pc_cur + info.imm : pc_cur + info.length;
  case INST_OP_BGE:
    return (RISCV_X_SIGNED)gprs[info.rs1] >= (RISCV_X_SIGNED)gprs[info.rs2] ? pc_cur + info.imm : pc_cur + info.length;
  case INST_OP_BGEU:
    return gprs[info.rs1] >= gprs[info.rs2] ? pc_cur + info.imm : pc_cur + info.length;
  case INST_OP_JAL:
    return pc_cur + info.imm;
  case INST_OP_JALR:
    return (gprs[info.rs1] + info.imm) & ~(RISCV_X_UNSIGNED)1;
  case INST_OP_C_J:
  case INST_OP_C_JAL:
    return pc_cur + info.imm;
  case INST_OP_C_JR:
  case INST_OP_C_JALR:    
    return gprs[info.rs1];
  case INST_OP_C_BEQZ:
    return gprs[info.rs1] == 0 ? pc_cur + info.imm : pc_cur + info.length;
  case INST_OP_C_BNEZ:
    return gprs[info.rs1] != 0 ? pc_cur + info.imm : pc_cur + info.length;
  default:
    return pc_cur + info.length;
  }
}


// #define RISCV_INST_TEST 1
#ifdef RISCV_INST_TEST

/*
00000000800000a4 <myroutine>:
    800000a4:	0001                	nop
    800000a6:	0001                	nop
    800000a8:	0001                	nop
    800000aa:	0001                	nop
    800000ac:	ff9ff06f          	j	800000a4 <myroutine>
    800000b0:	0500006f          	j	80000100 <myroutine+0x5c>
    800000b4:	00000297          	auipc	t0,0x0
    800000b8:	ff028293          	addi	t0,t0,-16 # 800000a4 <myroutine>
    800000bc:	00328067          	jr	3(t0)
    800000c0:	00000297          	auipc	t0,0x0
    800000c4:	04028293          	addi	t0,t0,64 # 80000100 <myroutine+0x5c>
    800000c8:	00328067          	jr	3(t0)
    800000cc:	4285                	li	t0,1
    800000ce:	4309                	li	t1,2
    800000d0:	fc628ae3          	beq	t0,t1,800000a4 <myroutine>
    800000d4:	02628663          	beq	t0,t1,80000100 <myroutine+0x5c>
    800000d8:	4285                	li	t0,1
    800000da:	4305                	li	t1,1
    800000dc:	fc6284e3          	beq	t0,t1,800000a4 <myroutine>
    800000e0:	02628063          	beq	t0,t1,80000100 <myroutine+0x5c>
    800000e4:	4285                	li	t0,1
    800000e6:	4309                	li	t1,2
    800000e8:	fa629ee3          	bne	t0,t1,800000a4 <myroutine>
    800000ec:	00629a63          	bne	t0,t1,80000100 <myroutine+0x5c>
    800000f0:	4289                	li	t0,2
    800000f2:	4309                	li	t1,2
    800000f4:	fa6298e3          	bne	t0,t1,800000a4 <myroutine>
    800000f8:	00629463          	bne	t0,t1,80000100 <myroutine+0x5c>
    800000fc:	0001                	nop
    800000fe:	0001                	nop
    80000100:	0001                	nop
    80000102:	0001                	nop
    80000104:	0001                	nop
    80000106:	0001                	nop
    80000108:	bf71                	j	800000a4 <myroutine>
    8000010a:	a81d                	j	80000140 <main>
    8000010c:	00000297          	auipc	t0,0x0
    80000110:	f9828293          	addi	t0,t0,-104 # 800000a4 <myroutine>
    80000114:	8282                	jr	t0
    80000116:	9282                	jalr	t0
    80000118:	00000297          	auipc	t0,0x0
    8000011c:	02828293          	addi	t0,t0,40 # 80000140 <main>
    80000120:	8282                	jr	t0
    80000122:	9282                	jalr	t0
    80000124:	4285                	li	s0,1
    80000126:	dc3d                	beqz	s0,800000a4 <myroutine>
    80000128:	cc01                	beqz	s0,80000140 <main>
    8000012a:	fc2d                	bnez	s0,800000a4 <myroutine>
    8000012c:	e811                	bnez	s0,80000140 <main>
    8000012e:	4281                	li	s0,0
    80000130:	d835                	beqz	s0,800000a4 <myroutine>
    80000132:	c419                	beqz	s0,80000140 <main>
    80000134:	f825                	bnez	s0,800000a4 <myroutine>
    80000136:	e409                	bnez	s0,80000140 <main>
    80000138:	0001                	nop
    8000013a:	0001                	nop
    8000013c:	0001                	nop
    8000013e:	0001                	nop

*/

int riscv_inst_test() {
  RISCV_X_UNSIGNED gprs[31] = {0};
  uint32_t nextpc;
  // t0 is x5, t1 = x6

  nextpc = riscv_next_pc(0x800000ac, 0xff9ff06f, gprs);
  assert(nextpc == 0x800000a4);

  nextpc = riscv_next_pc(0x800000b0, 0x0500006f, gprs);
  assert(nextpc == 0x80000100);

  gprs[5] = 0x800000a4;
  nextpc = riscv_next_pc(0x800000bc, 0x00328067, gprs);
  assert(nextpc == 0x800000a6);

  gprs[5] = 0x80000100;  
  nextpc = riscv_next_pc(0x800000c8, 0x00328067, gprs);
  assert(nextpc == 0x80000102);

  gprs[5] = 1;
  gprs[6] = 2;  
  nextpc = riscv_next_pc(0x800000d0, 0xfc628ae3, gprs);  
  assert(nextpc == 0x800000d4);
  nextpc = riscv_next_pc(0x800000d4, 0x02628663, gprs);
  assert(nextpc == 0x800000d8);

  //   nextpc = riscv_next_pc(0x, 0x, gprs);
  gprs[5] = 1;
  gprs[6] = 1;  
  nextpc = riscv_next_pc(0x800000dc, 0xfc6284e3, gprs);
  assert(nextpc == 0x800000a4);
  nextpc = riscv_next_pc(0x800000e0, 0x02628063, gprs);
  assert(nextpc == 0x80000100);

  gprs[5] = 1;
  gprs[6] = 2;
  nextpc = riscv_next_pc(0x800000e8, 0xfa629ee3, gprs);
  assert(nextpc == 0x800000a4);
  nextpc = riscv_next_pc(0x800000ec, 0x00629a63, gprs);
  assert(nextpc == 0x80000100);

  gprs[5] = 2;
  gprs[6] = 2;
  nextpc = riscv_next_pc(0x800000f4, 0xfa6298e3, gprs);
  assert(nextpc == 0x800000f8);
  nextpc = riscv_next_pc(0x800000f8, 0x00629463, gprs);
  assert(nextpc == 0x800000fc);

  nextpc = riscv_next_pc(0x80000108, 0xbf71, gprs);
  assert(nextpc == 0x800000a4);
  nextpc = riscv_next_pc(0x8000010a, 0xa81d, gprs);
  assert(nextpc == 0x80000140);

  gprs[5] = 0x800000a4;
  nextpc = riscv_next_pc(0x80000114, 0x8282, gprs);
  assert(nextpc == 0x800000a4);
  nextpc = riscv_next_pc(0x80000116, 0x9282, gprs);
  assert(nextpc == 0x800000a4);

  gprs[5] = 0x80000140;  
  nextpc = riscv_next_pc(0x80000120, 0x8282, gprs);
  assert(nextpc == 0x80000140);  
  nextpc = riscv_next_pc(0x80000122, 0x9282, gprs);
  assert(nextpc == 0x80000140);


  // 80000124:	4285                	li	t0,1
  gprs[8] = 1;
  nextpc = riscv_next_pc(0x80000126, 0xdc3d, gprs);
  assert(nextpc == 0x80000128);  
  nextpc = riscv_next_pc(0x80000128, 0xcc01, gprs);
  assert(nextpc == 0x8000012a);  
  nextpc = riscv_next_pc(0x8000012a, 0xfc2d, gprs);
  assert(nextpc == 0x800000a4);  
  nextpc = riscv_next_pc(0x8000012c, 0xe811, gprs);
  assert(nextpc == 0x80000140);

  /*
    8000012e:	4281                	li	s0,0
    80000130:	d835                	beqz	s0,800000a4 <myroutine>
    80000132:	c419                	beqz	s0,80000140 <main>
    80000134:	f825                	bnez	s0,800000a4 <myroutine>
    80000136:	e409                	bnez	s0,80000140 <main>
  */

  gprs[8] = 0;  
  nextpc = riscv_next_pc(0x80000130, 0xd835, gprs);
  assert(nextpc == 0x800000a4);  
  nextpc = riscv_next_pc(0x80000132, 0xc419, gprs);
  assert(nextpc == 0x80000140);  
  nextpc = riscv_next_pc(0x80000134, 0xf825, gprs);
  assert(nextpc == 0x80000136);    
  nextpc = riscv_next_pc(0x80000136, 0xe409, gprs);
  assert(nextpc == 0x80000138);  
}


int main()
{
  riscv_inst_test();
  return 0;
}


#endif
