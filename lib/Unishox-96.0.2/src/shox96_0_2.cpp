/*
 * Copyright (C) 2019 Siara Logics (cc)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * @author Arundale R.
 *
 */
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>

#include "shox96_0_2.h"

typedef unsigned char byte;

uint16_t c_95[95] PROGMEM = {16384, 16256, 15744, 16192, 15328, 15344, 15360, 16064, 15264, 15296, 15712, 15200, 14976, 15040, 14848, 15104, 14528, 14592, 14656, 14688, 14720, 14752, 14784, 14816, 14832, 14464, 15552, 15488, 15616, 15168, 15680, 16000, 15872, 10752,  8576,  8192,  8320,  9728,  8672,  8608,  8384, 11264,  9024,  8992, 12160,  8544, 11520, 11008,  8512,  9008, 12032, 11776, 10240,  8448,  8960,  8640,  9040,  8688,  9048, 15840, 16288, 15856, 16128, 16224, 16368, 40960,  6144,     0,  2048, 24576,  7680,  6656,  3072, 49152, 13312, 12800, 63488,  5632, 53248, 45056,  5120, 13056, 61440, 57344, 32768,  4096, 12288,  7168, 13568,  7936, 13696, 15776, 16320, 15808, 16352};
uint8_t  l_95[95] PROGMEM = {    3,    11,    11,    11,    12,    12,     9,    10,    11,    11,    11,    11,    10,    10,     9,    10,    10,    10,    11,    11,    11,    11,    11,    12,    12,    10,    10,    10,    10,    11,    11,    10,     9,     8,    11,     9,    10,     7,    12,    11,    10,     8,    12,    12,     9,    11,     8,     8,    11,    12,     9,     8,     7,    10,    11,    11,    13,    12,    13,    12,    11,    12,    10,    11,    12,     4,     7,     5,     6,     3,     8,     7,     6,     4,     8,     8,     5,     7,     4,     4,     7,     8,     5,     4,     3,     6,     7,     7,     9,     8,     9,    11,    11,    11,    12};
//unsigned char c[]    = {  ' ',   '!',   '"',   '#',   '$',   '%',   '&',  '\'',   '(',   ')',   '*',   '+',   ',',   '-',   '.',   '/',   '0',   '1',   '2',   '3',   '4',   '5',   '6',   '7',   '8',   '9',   ':',   ';',   '<',   '=',   '>',   '?',   '@',   'A',   'B',   'C',   'D',   'E',   'F',   'G',   'H',   'I',   'J',   'K',   'L',   'M',   'N',   'O',   'P',   'Q',   'R',   'S',   'T',   'U',   'V',   'W',   'X',   'Y',   'Z',   '[',  '\\',   ']',   '^',   '_',   '`',   'a',   'b',   'c',   'd',   'e',   'f',   'g',   'h',   'i',   'j',   'k',   'l',   'm',   'n',   'o',   'p',   'q',   'r',   's',   't',   'u',   'v',   'w',   'x',   'y',   'z',   '{',   '|',   '}',   '~'};
char SET2_STR[] PROGMEM = {'9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '.', ',', '-', '/', '=', '+', ' ', '(', ')', '$', '%', '&', ';', ':', '<', '>', '*', '"', '{', '}', '[', ']', '@', '?', '\'', '^', '#', '_', '!', '\\', '|', '~', '`', '\0'};

enum {SHX_STATE_1 = 1, SHX_STATE_2};

const uint16_t TERM_CODE = 0b0011011111000000; // 0x37C0;
const uint8_t  TERM_CODE_LEN = 10;

// byte to_match_repeats_within = 1;

#define NICE_LEN_FOR_PRIOR 7
#define NICE_LEN_FOR_OTHER 12

uint16_t mask[] PROGMEM = {0x8000, 0xC000, 0xE000, 0xF000, 0xF800, 0xFC00, 0xFE00, 0xFF00};

int append_bits(char *out, int ol, unsigned int code, int clen, byte state) {

   byte cur_bit;
   byte blen;
   unsigned char a_byte;

   if (state == SHX_STATE_2) {
      // remove change state prefix
      if ((code >> 9) == 0x1C) {
         code <<= 7;
         clen -= 7;
      }
      //if (code == 14272 && clen == 10) {
      //   code = 9084;
      //   clen = 14;
      //}
   }
   while (clen > 0) {
     cur_bit = ol % 8;
     blen = (clen > 8 ? 8 : clen);
     a_byte = (code & pgm_read_word(&mask[blen - 1])) >> 8;
     a_byte >>= cur_bit;
     if (blen + cur_bit > 8)
        blen = (8 - cur_bit);
     if (cur_bit == 0)
        out[ol / 8] = a_byte;
     else
        out[ol / 8] |= a_byte;
     code <<= blen;
     ol += blen;
     clen -= blen;
   }
   return ol;
}

byte codes[7] PROGMEM = {0x01, 0x82, 0xC3, 0xE5, 0xED, 0xF5, 0xFD};
byte bit_len[7] PROGMEM =  {2, 5,  7,   9,  12,   16,  17};
uint16_t adder[7] PROGMEM = {0, 4, 36, 164, 676, 4772,  0};

int encodeCount(char *out, int ol, int count) {
//   const byte codes[7] = {0x01, 0x82, 0xC3, 0xE5, 0xED, 0xF5, 0xFD};
//   const byte bit_len[7] =  {2, 5,  7,   9,  12,   16,  17};
//   const uint16_t adder[7] = {0, 4, 36, 164, 676, 4772,  0};
  int till = 0;
  for (int i = 0; i < 6; i++) {
    uint32_t bit_len_i = pgm_read_byte(&bit_len[i]);
    till += (1 << bit_len_i);
    if (count < till) {
      byte codes_i = pgm_read_byte(&codes[i]);
      ol = append_bits(out, ol, (codes_i & 0xF8) << 8, codes_i & 0x07, 1);
      ol = append_bits(out, ol, (count - pgm_read_word(&adder[i])) << (16 - bit_len_i), bit_len_i, 1);
      return ol;
    }
  }
  return ol;
}

int matchOccurance(const char *in, int len, int l, char *out, int *ol) {
  int j, k;
  for (j = 0; j < l; j++) {
    for (k = j; k < l && (l + k - j) < len; k++) {
      if (in[k] != in[l + k - j])
        break;
    }
    if ((k - j) > (NICE_LEN_FOR_PRIOR - 1)) {
      *ol = append_bits(out, *ol, 14144, 10, 1);
      *ol = encodeCount(out, *ol, k - j - NICE_LEN_FOR_PRIOR); // len
      *ol = encodeCount(out, *ol, l - j - NICE_LEN_FOR_PRIOR + 1); // dist
      l += (k - j);
      l--;
      return l;
    }
  }
  return -l;
}

int shox96_0_2_compress(const char *in, int len, char *out) {

  char *ptr;
  byte bits;
  byte state;

  int l, ll, ol;
  char c_in, c_next, c_prev;
  byte is_upper, is_all_upper;

  ol = 0;
  c_prev = 0;
  state = SHX_STATE_1;
  is_all_upper = 0;
  for (l=0; l<len; l++) {

    c_in = in[l];

    if (l < len - 4) {
      if (c_in == c_prev && c_in == in[l + 1] && c_in == in[l + 2] && c_in == in[l + 3]) {
        int rpt_count = l + 4;
        while (rpt_count < len && in[rpt_count] == c_in)
          rpt_count++;
        rpt_count -= l;
        ol = append_bits(out, ol, 14208, 10, 1);
        ol = encodeCount(out, ol, rpt_count - 4);
        l += rpt_count;
        l--;
        continue;
      }
    }

    // if (l < (len - NICE_LEN_FOR_PRIOR) && to_match_repeats_within) {
    if (l < (len - NICE_LEN_FOR_PRIOR)) {
          l = matchOccurance(in, len, l, out, &ol);
          if (l > 0) {
            c_prev = in[l - 1];
            continue;
          }
          l = -l;
    }
    if (state == SHX_STATE_2) {
      if (c_in == ' ' && len - 1 > l)
        ptr = (char *) memchr_P(SET2_STR, in[l+1], 42);
      else
        ptr = (char *) memchr_P(SET2_STR, c_in, 42);
      if (ptr == NULL) {
        state = SHX_STATE_1;
        ol = append_bits(out, ol, 8192, 4, 1);
      }
    }
    is_upper = 0;
    if (c_in >= 'A' && c_in <= 'Z')
      is_upper = 1;
    else {
      if (is_all_upper) {
        is_all_upper = 0;
        ol = append_bits(out, ol, 8192, 4, state);
      }
    }
    if (is_upper && !is_all_upper) {
      for (ll=l+5; ll>=l && ll<len; ll--) {
        if (in[ll] >= 'a' && in[ll] <= 'z')
          break;
      }
      if (ll == l-1) {
        ol = append_bits(out, ol, 8704, 8, state);
        is_all_upper = 1;
      }
    }
    if (state == SHX_STATE_1 && c_in >= '0' && c_in <= '9') {
      ol = append_bits(out, ol, 14336, 7, state);
      state = SHX_STATE_2;
    }
    c_next = 0;
    if (l+1 < len)
      c_next = in[l+1];

    c_prev = c_in;
    if (c_in >= 32 && c_in <= 126) {    // printable char
      c_in -= 32;
      if (is_all_upper && is_upper)
        c_in += 32;
      if (c_in == 0 && state == SHX_STATE_2)
        ol = append_bits(out, ol, 0x3B80 /*15232*/, 11, state); // 0011 1011 100.0 = Set3 + 1100 = space
      else
        ol = append_bits(out, ol, pgm_read_word(&c_95[c_in]), pgm_read_byte(&l_95[c_in]), state);
    } else
    // TOOD : remove CRLF
    // if (c_in == 13 && c_next == 10) {   // CR/LF
    //   ol = append_bits(out, ol, 0x3600 /*13824*/, 9, state);
    //   l++;  // skip next char
    //   c_prev = 10;
    // } else
    if (c_in == 10) {                   // LF
      ol = append_bits(out, ol, 0x3680 /*13952*/, 9, state);
    } else
    if (c_in == 13) {                   // CR
      ol = append_bits(out, ol, 0x2368 /*9064*/, 13, state);
    } else
    if (c_in == '\t') {                 // TAB
      ol = append_bits(out, ol, 9216, 7, state);
    } else {    // TODO adding binary encoding
      // printf("Bin:%d:%x\n", (unsigned char) c_in, (unsigned char) c_in);
      // ol = append_bits(out, ol, 0x2000, 9, state);    // TODO check if ok.  0010 000 00 = Upper + Set1a + bin
      ol = append_bits(out, ol, 0x3600, 9, state);    // TODO check if ok.  00 110 1100 = Set1B 1100
      ol = encodeCount(out, ol, (unsigned char) c_in);
    }
  }
  bits = ol % 8;
  if (bits) {
    ol = append_bits(out, ol, TERM_CODE, 8 - bits, 1);   // 0011 0111 1100 0000 TERM = 0011 0111 11
    // TODO write complete TERM?
  }
  //printf("\n%ld\n", ol);
  return ol/8+(ol%8?1:0);

}

// Decoder is designed for using less memory, not speed
// Decode lookup table for code index and length
// First 2 bits 00, Next 3 bits indicate index of code from 0,
// last 3 bits indicate code length in bits
//                0,            1,            2,            3,            4,
char vcode[32] PROGMEM = 
                 {2 + (0 << 3), 3 + (3 << 3), 3 + (1 << 3), 4 + (6 << 3), 0,
//                5,            6,            7,            8, 9, 10
                  4 + (4 << 3), 3 + (2 << 3), 4 + (8 << 3), 0, 0,  0,
//                11,          12, 13,            14, 15
                  4 + (7 << 3), 0,  4 + (5 << 3),  0,  5 + (9 << 3),
//                16, 17, 18, 19, 20, 21, 22, 23
                   0,  0,  0,  0,  0,  0,  0,  0,
//                24, 25, 26, 27, 28, 29, 30, 31
                   0, 0,  0,  0,  0,  0,  0,  5 + (10 << 3)};
//                0,            1,            2, 3,            4, 5, 6, 7,
char hcode[32] PROGMEM =
                 {1 + (1 << 3), 2 + (0 << 3), 0, 3 + (2 << 3), 0, 0, 0, 5 + (3 << 3),
//                8, 9, 10, 11, 12, 13, 14, 15,
                  0, 0,  0,  0,  0,  0,  0,  5 + (5 << 3),
//                16, 17, 18, 19, 20, 21, 22, 23
                   0, 0,  0,  0,  0,  0,  0,  5 + (4 << 3),
//                24, 25, 26, 27, 28, 29, 30, 31
                   0, 0,  0,  0,  0,  0,  0,  5 + (6 << 3)};

enum {SHX_SET1 = 0, SHX_SET1A, SHX_SET1B, SHX_SET2, SHX_SET3, SHX_SET4, SHX_SET4A};
char sets[][11] PROGMEM = 
                  {{' ', ' ', 'e', 't', 'a', 'o', 'i', 'n', 's', 'r', 'l'},
                   {'c', 'd', 'h', 'u', 'p', 'm', 'b', 'g', 'w', 'f', 'y'},
                   {'v', 'k', 'q', 'j', 'x', 'z', ' ', ' ', ' ', ' ', ' '},
                   {' ', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8'},
                   {'.', ',', '-', '/', '=', '+', ' ', '(', ')', '$', '%'},
                   {'&', ';', ':', '<', '>', '*', '"', '{', '}', '[', ']'},
                   {'@', '?', '\'', '^', '#', '_', '!', '\\', '|', '~', '`'}};

int getBitVal(const char *in, int bit_no, int count) {
   return (in[bit_no >> 3] & (0x80 >> (bit_no % 8)) ? 1 << count : 0);
}

int getCodeIdx(char *code_type, const char *in, int len, int *bit_no_p) {
  int code = 0;
  int count = 0;
  do {
    if (*bit_no_p >= len)
      return 199;
    code += getBitVal(in, *bit_no_p, count);
    (*bit_no_p)++;
    count++;
    char code_type_code = pgm_read_byte(&code_type[code]);
    if (code_type_code && (code_type_code & 0x07) == count) {
      return code_type_code >> 3;
    }
  } while (count < 5);
  return 1; // skip if code not found
}

int getNumFromBits(const char *in, int bit_no, int count) {
   int ret = 0;
   while (count--) {
     ret += getBitVal(in, bit_no++, count);
   }
   return ret;
}


byte bit_len_read[7] PROGMEM = {5, 2,  7,   9,  12,   16, 17};
uint16_t adder_read[7] PROGMEM = {4, 0, 36, 164, 676, 4772,  0};

int readCount(const char *in, int *bit_no_p, int len) {
  // const byte bit_len[7]   = {5, 2,  7,   9,  12,   16, 17};
  // const uint16_t adder[7] = {4, 0, 36, 164, 676, 4772,  0};
  int idx = getCodeIdx(hcode, in, len, bit_no_p);
  if (idx > 6)
    return 0;
  byte bit_len_idx = pgm_read_byte(&bit_len_read[idx]);
  int count = getNumFromBits(in, *bit_no_p, bit_len_idx) + pgm_read_word(&adder_read[idx]);
  (*bit_no_p) += bit_len_idx;
  return count;
}

int shox96_0_2_decompress(const char *in, int len, char *out) {

  int dstate;
  int bit_no;
  byte is_all_upper;

  int ol = 0;
  bit_no = 0;
  dstate = SHX_SET1;
  is_all_upper = 0;

  len <<= 3;    // *8, len in bits
  out[ol] = 0;
  while (bit_no < len) {
    int h, v;
    char c;
    byte is_upper = is_all_upper;
    int orig_bit_no = bit_no;
    v = getCodeIdx(vcode, in, len, &bit_no);    // read vCode
    if (v == 199) {     // end of stream
      bit_no = orig_bit_no;
      break;
    }
    h = dstate;     // Set1 or Set2
    if (v == 0) {   // Switch which is common to Set1 and Set2, first entry
      h = getCodeIdx(hcode, in, len, &bit_no);    // read hCode
      if (h == 199) {   // end of stream
        bit_no = orig_bit_no;
        break;
      }
      if (h == SHX_SET1) {          // target is Set1
         if (dstate == SHX_SET1) {  // Switch from Set1 to Set1 us UpperCase
           if (is_all_upper) {      // if CapsLock, then back to LowerCase
             is_upper = is_all_upper = 0;
             continue;
           }
           v = getCodeIdx(vcode, in, len, &bit_no);   // read again vCode
           if (v == 199) {          // end of stream
             bit_no = orig_bit_no;
             break;
           }
           if (v == 0) {
              h = getCodeIdx(hcode, in, len, &bit_no);  // read second hCode
              if (h == 199) {     // end of stream
                bit_no = orig_bit_no;
                break;
              }
              if (h == SHX_SET1) {  // If double Switch Set1, the CapsLock
                 is_all_upper = 1;
                 continue;
              }
           }
           is_upper = 1;      // anyways, still uppercase
         } else {
            dstate = SHX_SET1;  // if Set was not Set1, switch to Set1
            continue;
         }
      } else
      if (h == SHX_SET2) {    // If Set2, switch dstate to Set2
         if (dstate == SHX_SET1)    // TODO: is this test useful, there are only 2 states possible
           dstate = SHX_SET2;
         continue;
      }
      if (h != SHX_SET1) {    // all other Sets (why not else)
        v = getCodeIdx(vcode, in, len, &bit_no);    // we changed set, now read vCode for char
        if (v == 199) {
          bit_no = orig_bit_no;
          break;
        }
      }
    }
    // TODO insert special cases here
    if (h < 64 && v < 32)     // TODO: are these the actual limits? Not 11x7 ?
      c = pgm_read_byte(&sets[h][v]);
      // c = sets[h][v];
    if (c >= 'a' && c <= 'z') {
      if (is_upper)
        c -= 32;      // go to UpperCase for letters
    } else {          // handle all other cases
      if (is_upper && dstate == SHX_SET1 && v == 1)
        c = '\t';     // If UpperCase Space, change to TAB
      if (h == SHX_SET1B) {
         switch (v) {
           case 6:        // Set1B v=6, CRLF
             // TODO we remove CRLF and put Bin instead
             //  out[ol++] = '\r';
             //  c = '\n';
             c = readCount(in, &bit_no, len);
             break;
           case 7:        // Set1B v=7, CR or LF
             c = is_upper ? '\r' : '\n';
             break;
           case 8:        // Set1B v=8, Dict
             if (getBitVal(in, bit_no++, 0)) {
               int dict_len = readCount(in, &bit_no, len) + NICE_LEN_FOR_PRIOR;
               int dist = readCount(in, &bit_no, len) + NICE_LEN_FOR_PRIOR - 1;
               memcpy(out + ol, out + ol - dist, dict_len);
               ol += dict_len;
             } else {
               int dict_len = readCount(in, &bit_no, len) + NICE_LEN_FOR_OTHER;
               int dist = readCount(in, &bit_no, len);
               int ctx = readCount(in, &bit_no, len);
               ol += dict_len;
             }
             continue;
           case 9: {        // Set1B v=9, Rpt
             int count = readCount(in, &bit_no, len);
             count += 4;
             char rpt_c = out[ol - 1];
             while (count--)
               out[ol++] = rpt_c;
             continue;
           }
           case 10:         // TERM
             continue;

    // if (v == 0 && h == SHX_SET1A) {
    //   if (is_upper) {
    //     out[ol++] = readCount(in, &bit_no, len);
    //   } else {
    //     ol = decodeRepeat(in, len, out, ol, &bit_no, prev_lines);
    //   }
    //   continue;
    // }
         }
      }
    }
    out[ol++] = c;
  }

  // TODO remove `bit_no = orig_bit_no;` when exist. It is not used anyways
  return ol;

}
