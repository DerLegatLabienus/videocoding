#include "common.h"
#include "input.h"
#include "cavlc.h"
#include "cavlc_tables.h"
#include "residual.h"
#include "rc4.h"

code_table *CoeffTokenCodeTable[4];
code_table *CoeffTokenCodeTable_ChromaDC;
code_table *TotalZerosCodeTable_4x4[15];
code_table *TotalZerosCodeTable_ChromaDC[3];
code_table *RunBeforeCodeTable[6];


int residual_block(int *coeffLevel, int maxNumCoeff, int nC, rc4_state* rc4) {
  int coeff_token, TotalCoeff, TrailingOnes;
  int i, suffixLength, zerosLeft, coeffNum;
  int level[16],run[16];
  static int times = 0;

  switch(nC) {
    case -1:
      coeff_token=get_code(CoeffTokenCodeTable_ChromaDC);  break;
    case 0: case 1:
      coeff_token=get_code(CoeffTokenCodeTable[0]);        break;
    case 2: case 3:
      coeff_token=get_code(CoeffTokenCodeTable[1]);        break;
    case 4: case 5: case 6: case 7:
      coeff_token=get_code(CoeffTokenCodeTable[2]);        break;
    default:
      coeff_token=get_code(CoeffTokenCodeTable[3]);
  }
  TotalCoeff=coeff_token>>2;
  TrailingOnes=coeff_token&3;

  if(TotalCoeff>10 && TrailingOnes<3) suffixLength=1; else suffixLength=0;
  if(!TotalCoeff) return 0;

  for(i=0; i<TotalCoeff; ++i)
    if(i<TrailingOnes)
      level[i]=1-2*input_get_one_bit();
    else {
      int level_prefix;
      int levelSuffixSize=suffixLength;
      int levelCode;
      for(level_prefix=0; !input_get_one_bit(); ++level_prefix); // level_prefix counts the number of prefix zeros
      levelCode=level_prefix<<suffixLength;
      if(level_prefix==14 && suffixLength==0) levelSuffixSize=4;
      else if(level_prefix==15) levelSuffixSize=12;
      if(levelSuffixSize)
        levelCode+=input_get_bits(levelSuffixSize);
      if(level_prefix==15 && suffixLength==0)
        levelCode+=15;
      if(i==TrailingOnes && TrailingOnes<3)
        levelCode+=2;
      if(levelCode&1) level[i]=(-levelCode-1)>>1;
                 else level[i]=( levelCode+2)>>1;
      if(suffixLength==0) suffixLength=1;
      if(abs(level[i])>(3<<(suffixLength-1)) && suffixLength<6)
        ++suffixLength;
    }
  
#define APPLY_RC4
//#undef APPLY_RC4
#ifdef APPLY_RC4

  /* unsigned char a, b; */
  /* for (i=0; i<TotalCoeff; ++i) { */
  /*     a = level[i] > 0; */
  /*     rc4_crypt(rc4, &a, &b, 1); */
  /*     if (a ^ b) { */
  /*       level[i] *= -1; */
  /*     } */
  /* } */

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define GET_HALF(i) ((i) <= 8 ? 0 : 1)
/* #define PRINT_CH(C) printf("%3d [%d %d %d %d %d %d %d %d]", (C), (C>>7)&1, (C>>6)&1, (C>>5)&1, (C>>4)&1, (C>>3)&1, (C>>2)&1, (C>>1)&1, (C)&1) */  

  unsigned char p[2]={0}, c[2]={0};
  /* static unsigned long it=0; */
  for (i=0; i < MIN(TotalCoeff, 16); ++i) {
      c[GET_HALF(i)] |= (level[i] > 0) << i;
  }
  rc4_crypt(rc4, c, p, GET_HALF(TotalCoeff)+1);
  /* printf("xxx %6lu %3d ", ++it, TotalCoeff); */
  /* PRINT_CH(c); */
  /* printf(" "); */
  /* PRINT_CH(p); */
  /* printf(" "); */

  for (i=0; i < MIN(TotalCoeff, 16); ++i) {
      if ((p[GET_HALF(i)] ^ c[GET_HALF(i)]) & (1 << i)) {
        level[i] *= -1;
      }
  }
  /* for (i=0; i < MIN(TotalCoeff, 8); ++i) { */
  /*     b |= (level[i] > 0) << i; */
  /* } */
  /* PRINT_CH(b); */
  /* printf("\n"); */
#endif

  if(TotalCoeff<maxNumCoeff) {
    if(nC<0) zerosLeft=get_code(TotalZerosCodeTable_ChromaDC[TotalCoeff-1]);
        else zerosLeft=get_code(TotalZerosCodeTable_4x4[TotalCoeff-1]);
  } else
    zerosLeft=0;
  for(i=0; i<TotalCoeff-1; ++i) {
    if(zerosLeft>6) {
      int run_before=7-input_get_bits(3);
      if(run_before==7)
        while(!input_get_one_bit()) ++run_before;
      run[i]=run_before;
    } else if(zerosLeft>0)
      run[i]=get_code(RunBeforeCodeTable[zerosLeft-1]);
    else
      run[i]=0;
    zerosLeft-=run[i];
  }
  run[TotalCoeff-1]=zerosLeft;

  coeffNum=-1;
  for(i=TotalCoeff-1; i>=0; --i) {
    coeffNum+=run[i]+1;
    coeffLevel[coeffNum]=level[i];
  }

  /* fprintf(stderr, " - %d:%d\n", TotalCoeff, TrailingOnes); */

  return TotalCoeff;
}

///////////////////////////////////////////////////////////////////////////////

void init_code_tables() {
  int i;
  for(i=0; i<4; ++i)
    CoeffTokenCodeTable[i]=init_code_table(CoeffTokenCodes[i]);
  CoeffTokenCodeTable_ChromaDC=init_code_table(CoeffTokenCodes_ChromaDC);
  for(i=0; i<15; ++i)
    TotalZerosCodeTable_4x4[i]=init_code_table(TotalZerosCodes_4x4[i]);
  for(i=0; i<3; ++i)
    TotalZerosCodeTable_ChromaDC[i]=init_code_table(TotalZerosCodes_ChromaDC[i]);
  for(i=0; i<6; ++i)
    RunBeforeCodeTable[i]=init_code_table(RunBeforeCodes[i]);
}
