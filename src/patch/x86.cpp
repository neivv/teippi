#include "x86.h"

#include <stdint.h>
#include <string.h>
#include <windows.h>

#ifdef __GNUC__
static void Panic(const char *str) __attribute__((noreturn));
static void Panic(const char *str)
#else // Msvc
static void __declspec(noreturn) Panic(const char *str)
#endif
{
    MessageBoxA(0, str, "Whoops", 0);
    abort();
}

void CopyInstructions(void *dest_, void *src_, int len)
{
    uint8_t *dest = (uint8_t *)dest_, *src = (uint8_t *)src_;
    while (len > 0)
    {
        int next_ins_len = CountInstructionLength(src, 1);

        switch (src[0])
        {
            case 0xe8: // Relative call
            case 0xe9: // Relative jump
            {
                dest[0] = src[0];
                uint32_t *srcc = (uint32_t *)(src + 1);
                uint32_t *destt = (uint32_t *)(dest + 1);
                destt[0] = srcc[0];
                destt[0] += (uint32_t)(src - dest);
            }
            break;
            default:
                memcpy(dest, src, next_ins_len);
            break;
        }

        dest += next_ins_len;
        src += next_ins_len;
        len -= next_ins_len;
    }
}

static int CountExtendedInstructionLength(uint8_t *address, bool op16)
{
    switch (address[1])
    {
        default:
            Panic("Extended instruction");
    }
    return 0;
}

static int CountSingleInstructionLength(uint8_t *address, bool op16)
{
    char inc = 0, sib = 0;
    char op_size;
    if (op16)
        op_size = 2;
    else
        op_size = 4;

    switch (address[0])
    {
        case 0x81: // ..., rm16/32, imm16/32
        case 0xf7: // ..., rm16/32, imm16/32
            inc += op_size - 1;
        case 0x80: // ..., rm8, imm8
        case 0x82: // ..., rm8, imm8
        case 0x83: // ..., rm16/32, imm16/32 (yep)
        case 0xc0: // ..., rm8, imm8
        case 0xc1: // ..., rm8, imm8
        case 0xf6: // ..., rm8, imm8
        case 0xc6: // Mov rm8, imm8
            inc++;
        case 0x00: // Add rm8, r8
        case 0x01: // Add rm16/32, r16/32
        case 0x02: // Add r8, rm8
        case 0x03: // Add r16/32, rm16/32
        case 0x08: // Or rm8, r8
        case 0x09: // Or rm16/32, r16/32
        case 0x0a: // Or r8, rm8
        case 0x0b: // Or r16/32, rm16/32
        case 0x10: // Adc rm8, r8
        case 0x11: // Adc rm16/32, r16/32
        case 0x12: // Adc r8, rm8
        case 0x13: // Adc r16/32, rm16/32
        case 0x18: // Sbb rm8, r8
        case 0x19: // Sbb rm16/32, r16/32
        case 0x1a: // Sbb r8, rm8
        case 0x1b: // Sbb r16/32, rm16/32
        case 0x20: // And rm8, r8
        case 0x21: // And rm16/32, r16/32
        case 0x22: // And r8, rm8
        case 0x23: // And r16/32, rm16/32
        case 0x28: // Sub rm8, r8
        case 0x29: // Sub rm16/32, r16/32
        case 0x2a: // Sub r8, rm8
        case 0x2b: // Sub r16/32, rm16/32
        case 0x30: // Xor rm8, r8
        case 0x31: // Xor rm16/32, r16/32
        case 0x32: // Xor r8, rm8
        case 0x33: // Xor r16/32, rm16/32
        case 0x38: // Cmp rm8, r8
        case 0x39: // Cmp rm16/32, r16/32
        case 0x3a: // Cmp r8, rm8
        case 0x3b: // Cmp r16/32, rm16/32
        case 0x62: // Bound
        case 0x63: // Arpl
        case 0x84: // Test rm8, r8
        case 0x85: // Test rm16/32, r16/32
        case 0x86: // Xchg rm8, r8
        case 0x87: // Xchg rm16/32, r16/32
        case 0x88: // Mov rm8, r8
        case 0x89: // Mov rm16/32, r16/32
        case 0x8a: // Mov r8, rm8
        case 0x8b: // Mov r16/32, rm16/32
        case 0x8c: // Mov r16/32, sreg
        case 0x8d: // Lea
        case 0x8e: // Mov sreg, rm16
        case 0x8f: // Pop rm16/32
        case 0xc4: // Les
        case 0xc5: // Lds
        case 0xd0: // ..., 8
        case 0xd1: // ..., 16/32
        case 0xd2: // ..., 8
        case 0xd3: // ..., 16/32
        case 0xd8: // Float
        case 0xd9: // Float
        case 0xda: // Float
        case 0xdb: // Float
        case 0xdc: // Float
        case 0xdd: // Float
        case 0xde: // Float
        case 0xdf: // Float
        case 0xfe: // ...
        case 0xff: // ...
            if ((address[1] & 0x7) == 0x4) // sib
                sib = 1;
            switch ((address[1] & 0xc0) >> 6)
            {
                case 0:
                    if ((address[1 + inc] & 0x7) == 0x5) // disp32
                        return 6 + inc + sib;
                    else
                        return 2 + inc + sib;
                case 1:
                    return 3 + inc + sib;
                case 2:
                    return 6 + inc + sib;
                case 3:
                    return 2 + inc; // no sib
            }

        case 0x04: // Add al, imm8
        case 0x0c: // Or al, imm8
        case 0x14: // Adc al, imm8
        case 0x1c: // Sbb al, imm8
        case 0x24: // And al, imm8
        case 0x2c: // Sub al, imm8
        case 0x34: // Xor al, imm8
        case 0x3c: // Cmp al, imm8
        case 0x6a: // Push imm8
        case 0x70: // Jo short
        case 0x71: // Jno short
        case 0x72: // Jb short
        case 0x73: // Jnb short
        case 0x74: // Je short
        case 0x75: // Jne short
        case 0x76: // Jbe short
        case 0x77: // Ja short
        case 0x78: // Js short
        case 0x79: // Jns short
        case 0x7a: // Jpe short
        case 0x7b: // Jpo short
        case 0x7c: // Jl short
        case 0x7d: // Jge short
        case 0x7e: // Jle short
        case 0x7f: // Jg short
        case 0xa8: // Test al, imm8
        case 0xb0: // Mov al, imm8
        case 0xb1: // Mov cl, imm8
        case 0xb2: // Mov dl, imm8
        case 0xb3: // Mov bl, imm8
        case 0xb4: // Mov ah, imm8
        case 0xb5: // Mov ch, imm8
        case 0xb6: // Mov dh, imm8
        case 0xb7: // Mov bh, imm8
        case 0xcd: // Int
        case 0xd4: // Aam
        case 0xd5: // Aad
        case 0xe0: // Loopnz
        case 0xe1: // Loopz
        case 0xe2: // Loop
        case 0xe3: // Jecxz
        case 0xe4: // In al
        case 0xe5: // In eax
        case 0xe6: // Out al
        case 0xe7: // Out eax
            return 2;

        case 0x05: // Add (e)ax, imm16/32
        case 0x0d: // Or (e)ax, imm16/32
        case 0x15: // Adc (e)ax, imm16/32
        case 0x1d: // Sbb (e)ax, imm16/32
        case 0x25: // And (e)ax, imm16/32
        case 0x2d: // And (e)ax, imm16/32
        case 0x35: // Xor (e)ax, imm16/32
        case 0x3d: // Cmp (e)ax, imm16/32
        case 0x68: // Push imm16/32
        case 0xa9: // Test (e)ax, imm16/32
        case 0xb8: // Mov (e)ax, imm8
        case 0xb9: // Mov (e)cx, imm8
        case 0xba: // Mov (e)dx, imm8
        case 0xbb: // Mov (e)bx, imm8
        case 0xbc: // Mov (e)sp, imm8
        case 0xbd: // Mov (e)bp, imm8
        case 0xbe: // Mov (e)si, imm8
        case 0xbf: // Mov (e)di, imm8
        case 0xc3: // Retn
            return 1 + op_size;

        case 0x06: // Push es
        case 0x07: // Pop es
        case 0x0e: // Push cs
        case 0x16: // Push ss
        case 0x17: // Pop ss
        case 0x1e: // Push ds
        case 0x1f: // Pop ds
        case 0x27: // Daa
        case 0x2f: // Das
        case 0x37: // Aaa
        case 0x3f: // Aas
        case 0x40: // Inc (e)ax
        case 0x41: // Inc (e)cx
        case 0x42: // Inc (e)dx
        case 0x43: // Inc (e)bx
        case 0x44: // Inc (e)sp
        case 0x45: // Inc (e)bp
        case 0x46: // Inc (e)si
        case 0x47: // Inc (e)di
        case 0x48: // Dec (e)ax
        case 0x49: // Dec (e)cx
        case 0x4a: // Dec (e)dx
        case 0x4b: // Dec (e)bx
        case 0x4c: // Dec (e)sp
        case 0x4d: // Dec (e)bp
        case 0x4e: // Dec (e)si
        case 0x4f: // Dec (e)di
        case 0x50: // Push (e)ax
        case 0x51: // Push (e)cx
        case 0x52: // Push (e)dx
        case 0x53: // Push (e)bx
        case 0x54: // Push (e)sp
        case 0x55: // Push (e)bp
        case 0x56: // Push (e)si
        case 0x57: // Push (e)di
        case 0x58: // Pop (e)ax
        case 0x59: // Pop (e)cx
        case 0x5a: // Pop (e)dx
        case 0x5b: // Pop (e)bx
        case 0x5c: // Pop (e)sp
        case 0x5d: // Pop (e)bp
        case 0x5e: // Pop (e)si
        case 0x5f: // Pop (e)di
        case 0x60: // Pushad
        case 0x61: // Popad
        case 0x6c: // Ins 8
        case 0x6d: // Ins 16/32
        case 0x6e: // Outs 8
        case 0x6f: // Outs 16/32
        case 0x90: // Nop
        case 0x91: // Xchg eax, ecx
        case 0x92: // Xchg eax, edx
        case 0x93: // Xchg eax, ebx
        case 0x94: // Xchg eax, esp
        case 0x95: // Xchg eax, ebp
        case 0x96: // Xchg eax, esi
        case 0x97: // Xchg eax, edi
        case 0x98: // Cwde
        case 0x99: // Cdq
        case 0x9b: // Wait
        case 0x9c: // Pushfd
        case 0x9d: // Popfd
        case 0x9e: // Sahf
        case 0x9f: // Lahf
        case 0xa4: // Movs 8
        case 0xa5: // Movs 16/32
        case 0xa6: // Cmps 8
        case 0xa7: // Cmps 16/32
        case 0xaa: // Stos 8
        case 0xab: // Stos 16/32
        case 0xac: // Lods 8
        case 0xad: // Lods 16/32
        case 0xae: // Scas 8
        case 0xaf: // Scas 16/32
        case 0xc9: // Leave
        case 0xcb: // Retf
        case 0xcc: // Int 3
        case 0xce: // Into
        case 0xcf: // Iretd
        case 0xd7: // Xlat
        case 0xf4: // Hlt
        case 0xf5: // Cmc
        case 0xf8: // Clc
        case 0xf9: // Stc
        case 0xfa: // Cli
        case 0xfb: // Sti
        case 0xfc: // Cld
        case 0xfd: // Cli
            return 1;

        case 0x6b: // Imul r16/32, rm16/32, imm8
            op_size = 1;
        case 0x69: // Imul r16/32, rm16/32, imm16/32
            if ((address[1] & 0x7) == 0x4) // sib
                inc = 1;
            else
                inc = 0;
            switch ((address[1] & 0xc0) >> 6)
            {
                case 0:
                    if ((address[1 + inc] & 0x7) == 0x5) // disp32
                        return 6 + inc + op_size;
                    else
                        return 2 + inc + op_size;
                case 1:
                    return 3 + inc + op_size;
                case 2:
                    return 6 + inc + op_size;
                case 3:
                    return 2 + op_size; // no inc
            }

        case 0x9a: // Far call
            return 7;

        case 0xa0: // Mov al, m8
        case 0xa1: // Mov eax, m32
        case 0xa2: // Mov m8, al
        case 0xa3: // Mov m32, eax
        case 0xe8: // Relative call
        case 0xe9: // Relative jump
            return 5;

        case 0xc8: // Enter
            return 4;

        case 0xc2: // Retn imm16
        case 0xca: // Retf
            return 3;

        case 0x26: // Es segment prefix
        case 0x2e: // Cs segment prefix
        case 0x36: // Ss segment prefix
        case 0x3e: // Ds segment prefix
        case 0x64: // Fs segment prefix
        case 0x65: // Gs segment prefix
        case 0xf0: // Lock prefix
        case 0xf2: // Repne prefix
        case 0xf3: // Rep prefix
            return 1 + CountSingleInstructionLength(address + 1, op16);

        case 0x66: // Operand size prefix
            return 1 + CountSingleInstructionLength(address + 1, true);

        case 0x67: // Address size prefix
            Panic("Address size prefix instruction, fuck this"); // ewww

        case 0x0f:
            return CountExtendedInstructionLength(address, op16);

        case 0xc7:
        case 0xd6: // Salc, undocumented
        case 0xf1: // Int 1, undocumented
        default:
            Panic("Unknown instruction opcode");
    }
}

int CountInstructionLength(void *address, int min_length)
{
    int length = 0;
    while (length < min_length)
    {
        length += CountSingleInstructionLength((uint8_t *)address + length, false);
    }
    return length;
}
