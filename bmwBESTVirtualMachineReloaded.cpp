//
// Created by lpcvoid on 2020-05-07.
//

#include <algorithm>
#include <cmath>
#include <iostream>
#include "bmwBESTVirtualMachineReloaded.h"
#include "../common/bmwLogger.h"
#include "../common/bmwStringHelper.hpp"
#include "bmwBESTVMJobConstants.h"
#include "../coms/bmwAdapterBMWFAST.hpp"

bool bmwBESTVirtualMachineReloaded::read_operand(bmwBESTOperandAddrMode otype, bmwBESTOperand *operand, bool lhs) {
    operand->omode = otype;
    switch (otype) {

        case bmwBESTOperandAddrMode_None:
            return false;
        case bmwBESTOperandAddrMode_RegS:
        case bmwBESTOperandAddrMode_RegAB:
        case bmwBESTOperandAddrMode_RegI:
        case bmwBESTOperandAddrMode_RegL: {
            uint8 regcode;
            read_code(&regcode, 1);
            bmwBESTDataMapping *mapping;
            if (_current_context->get_register_memory(regcode, &mapping)) {
                operand->dissasembly = mapping->first;
                operand->absolute_register_data.data = mapping->second.first;
                operand->absolute_register_data.len = &mapping->second.second;
                //only string registers size can ever be changed
                if (otype == bmwBESTOperandAddrMode_RegS)
                    operand->absolute_register_data.max_len = BMW_BESTVM_STRING_MAXSIZE;
                else
                    operand->absolute_register_data.max_len = *operand->absolute_register_data.len;

                operand->mapped_operand_data.offset = 0;
                operand->mapped_operand_data.len = *operand->absolute_register_data.len;
                return true;
            } else {
                BMWLOG().error("Regx : Register not found");
                return false;
            }
        }
            break;
        case bmwBESTOperandAddrMode_Imm8: {
            read_code(&operand->immediate_data.data[0], 1);
            operand->immediate_data.len = 1;
            operand->absolute_register_data.data = &operand->immediate_data.data[0];
            operand->absolute_register_data.len = &operand->immediate_data.len;
            operand->absolute_register_data.max_len = operand->immediate_data.len;
            operand->mapped_operand_data.offset = 0;
            operand->mapped_operand_data.len = *operand->absolute_register_data.len;
            operand->dissasembly = std::to_string(operand->cast_data<uint8>());
            return true;
        }
        case bmwBESTOperandAddrMode_Imm16: {
            read_code(&operand->immediate_data.data[0], 2);
            operand->immediate_data.len = 2;
            operand->absolute_register_data.data = &operand->immediate_data.data[0];
            operand->absolute_register_data.len = &operand->immediate_data.len;
            operand->absolute_register_data.max_len = operand->immediate_data.len;
            operand->mapped_operand_data.offset = 0;
            operand->mapped_operand_data.len = *operand->absolute_register_data.len;
            operand->dissasembly = std::to_string(operand->cast_data<uint16>());
            return true;
        }
        case bmwBESTOperandAddrMode_Imm32: {
            read_code(&operand->immediate_data.data[0], 4);
            operand->immediate_data.len = 4;
            operand->absolute_register_data.data = &operand->immediate_data.data[0];
            operand->absolute_register_data.len = &operand->immediate_data.len;
            operand->absolute_register_data.max_len = operand->immediate_data.len;
            operand->mapped_operand_data.offset = 0;
            operand->mapped_operand_data.len = *operand->absolute_register_data.len;
            operand->dissasembly = std::to_string(operand->cast_data<int32>());
            return true;
        }
        case bmwBESTOperandAddrMode_ImmStr: {
            uint16 str_len = 0;
            read_code(&str_len, 2);
            read_code(&operand->immediate_data.data[0], str_len);
            operand->immediate_data.len = str_len;
            operand->absolute_register_data.data = &operand->immediate_data.data[0];
            operand->absolute_register_data.len = &operand->immediate_data.len;
            operand->absolute_register_data.max_len = operand->immediate_data.len;
            operand->mapped_operand_data.offset = 0;
            operand->mapped_operand_data.len = *operand->absolute_register_data.len;
            std::string immstr(reinterpret_cast<const char *>(&operand->immediate_data.data[0]),
                               operand->immediate_data.len);
            operand->dissasembly = "\"" + immstr + "\"";
            return true;
        }

        case bmwBESTOperandAddrMode_IdxImm: {
            //reg[2]
            uint8 buf[3];
            read_code(&buf[0], 3);
            int16 index = *(int16 *) &buf[1];
            bmwBESTDataMapping *mapping;
            if (_current_context->get_register_memory(buf[0], &mapping)) {
                //make sure that the size of register is at least at index
                //otherwise this will fail on an empty RS0:
                //move RS0[x], xxx
                ensure_mapping_size(mapping, index);
                operand->absolute_register_data.data = mapping->second.first;
                operand->absolute_register_data.len = &mapping->second.second;
                operand->absolute_register_data.max_len = BMW_BESTVM_STRING_MAXSIZE;
                operand->mapped_operand_data.offset = index;
                operand->mapped_operand_data.len = (lhs ? mapping->second.second - index : 1);
                operand->dissasembly = mapping->first + "[" + std::to_string(index) + "]";
                return true;
            } else {
                BMWLOG().error("IdImm : Register not found");
                return false;
            }
        }

        case bmwBESTOperandAddrMode_IdxReg: {
            //reg[reg]
            uint8 buf[2];
            read_code(&buf[0], 2);
            bmwBESTDataMapping *mapping_reg;
            bmwBESTDataMapping *mapping_reg_indx;
            if (_current_context->get_register_memory(buf[0], &mapping_reg)) {
                if (_current_context->get_register_memory(buf[1], &mapping_reg_indx)) {
                    int32 index = get_mapping_value(*mapping_reg_indx);

                    ensure_mapping_size(mapping_reg, index);

                    int32 data_size = (lhs ? mapping_reg->second.second - index : 1);

                    operand->absolute_register_data.data = mapping_reg->second.first;
                    operand->absolute_register_data.len = &mapping_reg->second.second;
                    operand->absolute_register_data.max_len = BMW_BESTVM_STRING_MAXSIZE;
                    operand->mapped_operand_data.offset = index;
                    operand->mapped_operand_data.len = data_size;
                    operand->dissasembly = mapping_reg->first + "[" + mapping_reg_indx->first + "]";
                    return true;
                }
            }
            BMWLOG().error("IdxReg : Register not found");
            return false;
        }
        case bmwBESTOperandAddrMode_IdxRegImm: {
            //reg[reg2+3]
            //this operand was probably meant as an optimization
            uint8 buf[4];
            read_code(&buf[0], 4);
            bmwBESTDataMapping *mapping_reg;
            bmwBESTDataMapping *mapping_reg_indx;
            if (_current_context->get_register_memory(buf[0], &mapping_reg)) {
                if (_current_context->get_register_memory(buf[1], &mapping_reg_indx)) {
                    int32 index_reg = get_mapping_value(*mapping_reg_indx);

                    ensure_mapping_size(mapping_reg, index_reg);

                    int16 index_imm = *(int16 *) &buf[2];
                    operand->absolute_register_data.data = mapping_reg->second.first;
                    operand->absolute_register_data.len = &mapping_reg->second.second;
                    operand->absolute_register_data.max_len = BMW_BESTVM_STRING_MAXSIZE;
                    operand->mapped_operand_data.offset = (index_reg + index_imm);
                    operand->mapped_operand_data.len = (lhs ? mapping_reg->second.second - (index_reg + index_imm) : 1);
                    operand->dissasembly = mapping_reg->first + "[" + mapping_reg_indx->first +
                                           "+" + std::to_string(index_imm) + "]";
                    return true;
                }
            }
            BMWLOG().error("IdxRegImm : Register not found");
            return false;
        }
        case bmwBESTOperandAddrMode_IdxImmLenImm: {
            //[3:5] --> len:2, addr:&reg[3]
            uint8 buf[5];
            read_code(&buf[0], 5);
            bmwBESTDataMapping *mapping_reg;

            int16 index_imm0 = *(int16 *) &buf[1];
            int16 index_imm1 = *(int16 *) &buf[3];
            if (_current_context->get_register_memory(buf[0], &mapping_reg)) {
                ensure_mapping_size(mapping_reg, index_imm0 + index_imm1);
                operand->absolute_register_data.data = mapping_reg->second.first;
                operand->absolute_register_data.len = &mapping_reg->second.second;
                operand->absolute_register_data.max_len = BMW_BESTVM_STRING_MAXSIZE;
                operand->mapped_operand_data.offset = index_imm0;
                operand->mapped_operand_data.len = index_imm1;
                operand->dissasembly =
                        mapping_reg->first + "[" + std::to_string(index_imm0) + ":" + std::to_string(index_imm1) + "]";
                return true;
            }
            BMWLOG().error("IdxImmLenImm : Register not found");
            return false;
        }
        case bmwBESTOperandAddrMode_IdxImmLenReg: {
            //[3:reg] --> len:reg, addr:&reg[3]
            uint8 buf[4];
            read_code(&buf[0], 4);
            int16 index_imm = *(int16 *) &buf[1];
            bmwBESTDataMapping *mapping_reg;
            bmwBESTDataMapping *mapping_reg_len;
            if (_current_context->get_register_memory(buf[0], &mapping_reg)) {
                if (_current_context->get_register_memory(buf[3], &mapping_reg_len)) {
                    uint32 index_len = get_mapping_value(*mapping_reg_len);

                    ensure_mapping_size(mapping_reg, index_imm + index_len);
                    operand->absolute_register_data.data = mapping_reg->second.first;
                    operand->absolute_register_data.len = &mapping_reg->second.second;
                    operand->absolute_register_data.max_len = BMW_BESTVM_STRING_MAXSIZE;
                    operand->mapped_operand_data.offset = index_imm;
                    operand->mapped_operand_data.len = index_len;
                    operand->dissasembly =
                            mapping_reg->first + "[" + std::to_string(index_imm) + ":" + mapping_reg_len->first + "]";
                    return true;
                }
            }
            BMWLOG().error("IdxImmLenImm : Register not found");
            return false;
        }
        case bmwBESTOperandAddrMode_IdxRegLenImm: {
            //reg1[reg2:3] --> len:reg, addr:&reg[3]
            uint8 buf[4];
            read_code(&buf[0], 4);
            int16 index_len_imm = *(int16 *) &buf[2];
            bmwBESTDataMapping *mapping_reg;
            bmwBESTDataMapping *mapping_reg_len;
            if (_current_context->get_register_memory(buf[0], &mapping_reg)) {
                if (_current_context->get_register_memory(buf[1], &mapping_reg_len)) {
                    uint32 index_reg = get_mapping_value(*mapping_reg_len);

                    ensure_mapping_size(mapping_reg, index_reg + index_len_imm);
                    operand->absolute_register_data.data = mapping_reg->second.first;
                    operand->absolute_register_data.len = &mapping_reg->second.second;
                    operand->absolute_register_data.max_len = BMW_BESTVM_STRING_MAXSIZE;
                    operand->mapped_operand_data.offset = index_reg;
                    operand->mapped_operand_data.len = index_len_imm;
                    operand->dissasembly =
                            mapping_reg->first + "[" + mapping_reg_len->first + ":" + std::to_string(index_reg) + "]";
                    return true;
                }
            }
            BMWLOG().error("IdxRegLenImm : Register not found");
        }

        case bmwBESTOperandAddrMode_IdxRegLenReg: {
            //reg2[reg2:reg3]
            uint8 buf[3];
            read_code(&buf[0], 3);
            bmwBESTDataMapping *mapping_reg;
            bmwBESTDataMapping *mapping_reg_index;
            bmwBESTDataMapping *mapping_reg_len;
            if (_current_context->get_register_memory(buf[0], &mapping_reg)) {
                if (_current_context->get_register_memory(buf[1], &mapping_reg_index)) {
                    if (_current_context->get_register_memory(buf[2], &mapping_reg_len)) {
                        int32 index_reg = get_mapping_value(*mapping_reg_index);
                        int32 index_len = get_mapping_value(*mapping_reg_len);

                        ensure_mapping_size(mapping_reg, index_reg + index_len);

                        operand->absolute_register_data.data = mapping_reg->second.first;
                        operand->absolute_register_data.len = &mapping_reg->second.second;
                        operand->absolute_register_data.max_len = BMW_BESTVM_STRING_MAXSIZE;
                        operand->mapped_operand_data.offset = index_reg;
                        operand->mapped_operand_data.len = index_len;
                        operand->dissasembly =
                                mapping_reg->first + "[" + mapping_reg_index->first + ":" + mapping_reg_len->first +
                                "]";
                        return true;
                    }
                }
            }
            BMWLOG().error("IdxRegLenReg : Register not found");
        }
    }

    return false;
}

bool bmwBESTVirtualMachineReloaded::read_code(void *dest, uint32_t len) {
    memcpy(dest, &_current_context->best_job->job_code[_current_context->eip], len);
    _current_context->eip += len;
    return true;
}

bool bmwBESTVMContextReloaded::get_register_memory(uint8 register_code, bmwBESTDataMapping **out_mapping) {
    if (_register_map.size() > (register_code)) {
        *out_mapping = &_register_map[register_code];
        return true;
    }
    *out_mapping = nullptr;
    return false;
}

bmwBESTVirtualMachineReloaded::bmwBESTVirtualMachineReloaded() {
    //_current_context = new bmwBESTVMContextReloaded();

    _trapbit_map = {
            {bmwBESTVMErrorCodes::EDIABAS_BIP_0002, 2},
            {bmwBESTVMErrorCodes::EDIABAS_BIP_0006, 6},
            {bmwBESTVMErrorCodes::EDIABAS_BIP_0009, 9},
            {bmwBESTVMErrorCodes::EDIABAS_BIP_0010, 10},
            {bmwBESTVMErrorCodes::EDIABAS_IFH_0001, 11},
            {bmwBESTVMErrorCodes::EDIABAS_IFH_0002, 12},
            {bmwBESTVMErrorCodes::EDIABAS_IFH_0003, 13},
            {bmwBESTVMErrorCodes::EDIABAS_IFH_0004, 14},
            {bmwBESTVMErrorCodes::EDIABAS_IFH_0005, 15},
            {bmwBESTVMErrorCodes::EDIABAS_IFH_0006, 16},
            {bmwBESTVMErrorCodes::EDIABAS_IFH_0007, 17},
            {bmwBESTVMErrorCodes::EDIABAS_IFH_0008, 18},
            {bmwBESTVMErrorCodes::EDIABAS_IFH_0009, 19},
            {bmwBESTVMErrorCodes::EDIABAS_IFH_0010, 20},
            {bmwBESTVMErrorCodes::EDIABAS_IFH_0011, 21},
            {bmwBESTVMErrorCodes::EDIABAS_IFH_0012, 22},
            {bmwBESTVMErrorCodes::EDIABAS_IFH_0013, 23},
            {bmwBESTVMErrorCodes::EDIABAS_IFH_0014, 24},
            {bmwBESTVMErrorCodes::EDIABAS_IFH_0015, 25},
            {bmwBESTVMErrorCodes::EDIABAS_IFH_0016, 26}
    };

    {
        _opcode_name_map[0x00] = "move";
        _opcode_name_map[0x01] = "clear";
        _opcode_name_map[0x02] = "comp";
        _opcode_name_map[0x03] = "subb";
        _opcode_name_map[0x04] = "adds";
        _opcode_name_map[0x05] = "mult";
        _opcode_name_map[0x06] = "divs";
        _opcode_name_map[0x07] = "and";
        _opcode_name_map[0x08] = "or";
        _opcode_name_map[0x09] = "xor";
        _opcode_name_map[0x0A] = "not";
        _opcode_name_map[0x0B] = "jump";
        _opcode_name_map[0x0C] = "jtsr";
        _opcode_name_map[0x0D] = "ret";
        _opcode_name_map[0x0E] = "jc";
        _opcode_name_map[0x0F] = "jae";
        _opcode_name_map[0x10] = "jz";
        _opcode_name_map[0x11] = "jnz";
        _opcode_name_map[0x12] = "jv";
        _opcode_name_map[0x13] = "jnv";
        _opcode_name_map[0x14] = "jmi";
        _opcode_name_map[0x15] = "jpl";
        _opcode_name_map[0x16] = "clrc";
        _opcode_name_map[0x17] = "setc";
        _opcode_name_map[0x18] = "asr";
        _opcode_name_map[0x19] = "lsl";
        _opcode_name_map[0x1A] = "lsr";
        _opcode_name_map[0x1B] = "asl";
        _opcode_name_map[0x1C] = "nop";
        _opcode_name_map[0x1D] = "eoj";
        _opcode_name_map[0x1E] = "push";
        _opcode_name_map[0x1F] = "pop";
        _opcode_name_map[0x20] = "scmp";
        _opcode_name_map[0x21] = "scat";
        _opcode_name_map[0x22] = "scut";
        _opcode_name_map[0x23] = "slen";
        _opcode_name_map[0x24] = "spaste";
        _opcode_name_map[0x25] = "serase";
        _opcode_name_map[0x26] = "xconnect";
        _opcode_name_map[0x27] = "xhangup";
        _opcode_name_map[0x28] = "xsetpar";
        _opcode_name_map[0x29] = "xawlen";
        _opcode_name_map[0x2A] = "xsend";
        _opcode_name_map[0x2B] = "xsendf";
        _opcode_name_map[0x2C] = "xrequf";
        _opcode_name_map[0x2D] = "xstopf";
        _opcode_name_map[0x2E] = "xkeyb";
        _opcode_name_map[0x2F] = "xstate";
        _opcode_name_map[0x30] = "xboot";
        _opcode_name_map[0x31] = "xreset";
        _opcode_name_map[0x32] = "xtype";
        _opcode_name_map[0x33] = "xvers";
        _opcode_name_map[0x34] = "ergb";
        _opcode_name_map[0x35] = "ergw";
        _opcode_name_map[0x36] = "ergd";
        _opcode_name_map[0x37] = "ergi";
        _opcode_name_map[0x38] = "ergr";
        _opcode_name_map[0x39] = "ergs";
        _opcode_name_map[0x3A] = "a2flt";
        _opcode_name_map[0x3B] = "fadd";
        _opcode_name_map[0x3C] = "fsub";
        _opcode_name_map[0x3D] = "fmul";
        _opcode_name_map[0x3E] = "fdiv";
        _opcode_name_map[0x3F] = "ergy";
        _opcode_name_map[0x40] = "enewset";
        _opcode_name_map[0x41] = "etag";
        _opcode_name_map[0x42] = "xreps";
        _opcode_name_map[0x43] = "gettmr";
        _opcode_name_map[0x44] = "settmr";
        _opcode_name_map[0x45] = "sett";
        _opcode_name_map[0x46] = "clrt";
        _opcode_name_map[0x47] = "jt";
        _opcode_name_map[0x48] = "jnt";
        _opcode_name_map[0x49] = "addc";
        _opcode_name_map[0x4A] = "subc";
        _opcode_name_map[0x4B] = "break";
        _opcode_name_map[0x4C] = "clrv";
        _opcode_name_map[0x4D] = "eerr";
        _opcode_name_map[0x4E] = "popf";
        _opcode_name_map[0x4F] = "pushf";
        _opcode_name_map[0x50] = "atsp";
        _opcode_name_map[0x51] = "swap";
        _opcode_name_map[0x52] = "setspc";
        _opcode_name_map[0x53] = "srevrs";
        _opcode_name_map[0x54] = "stoken";
        _opcode_name_map[0x55] = "parb";
        _opcode_name_map[0x56] = "parw";
        _opcode_name_map[0x57] = "parl";
        _opcode_name_map[0x58] = "pars";
        _opcode_name_map[0x59] = "fclose";
        _opcode_name_map[0x5A] = "jg";
        _opcode_name_map[0x5B] = "jge";
        _opcode_name_map[0x5C] = "jl";
        _opcode_name_map[0x5D] = "jle";
        _opcode_name_map[0x5E] = "ja";
        _opcode_name_map[0x5F] = "jbe";
        _opcode_name_map[0x60] = "fopen";
        _opcode_name_map[0x61] = "fread";
        _opcode_name_map[0x62] = "freadln";
        _opcode_name_map[0x63] = "fseek";
        _opcode_name_map[0x64] = "fseekln";
        _opcode_name_map[0x65] = "ftell";
        _opcode_name_map[0x66] = "ftellln";
        _opcode_name_map[0x67] = "a2fix";
        _opcode_name_map[0x68] = "fix2flt";
        _opcode_name_map[0x69] = "parr";
        _opcode_name_map[0x6A] = "test";
        _opcode_name_map[0x6B] = "wait";
        _opcode_name_map[0x6C] = "date";
        _opcode_name_map[0x6D] = "time";
        _opcode_name_map[0x6E] = "xbatt";
        _opcode_name_map[0x6F] = "tosp";
        _opcode_name_map[0x70] = "xdownl";
        _opcode_name_map[0x71] = "xgetport";
        _opcode_name_map[0x72] = "xignit";
        _opcode_name_map[0x73] = "xloopt";
        _opcode_name_map[0x74] = "xprog";
        _opcode_name_map[0x75] = "xraw";
        _opcode_name_map[0x76] = "xsetport";
        _opcode_name_map[0x77] = "xsireset";
        _opcode_name_map[0x78] = "xstoptr";
        _opcode_name_map[0x79] = "fix2hex";
        _opcode_name_map[0x7A] = "fix2dez";
        _opcode_name_map[0x7B] = "tabset";
        _opcode_name_map[0x7C] = "tabseek";
        _opcode_name_map[0x7D] = "tabget";
        _opcode_name_map[0x7E] = "strcat";
        _opcode_name_map[0x7F] = "pary";
        _opcode_name_map[0x80] = "parn";
        _opcode_name_map[0x81] = "ergc";
        _opcode_name_map[0x82] = "ergl";
        _opcode_name_map[0x83] = "tabline";
        _opcode_name_map[0x84] = "xsendr";
        _opcode_name_map[0x85] = "xrecv";
        _opcode_name_map[0x86] = "xinfo";
        _opcode_name_map[0x87] = "flt2a";
        _opcode_name_map[0x88] = "setflt";
        _opcode_name_map[0x89] = "cfgig";
        _opcode_name_map[0x8A] = "cfgsg";
        _opcode_name_map[0x8B] = "cfgis";
        _opcode_name_map[0x8C] = "a2y";
        _opcode_name_map[0x8D] = "xparraw";
        _opcode_name_map[0x8E] = "hex2y";
        _opcode_name_map[0x8F] = "strcmp";
        _opcode_name_map[0x90] = "strlen";
        _opcode_name_map[0x91] = "y2bcd";
        _opcode_name_map[0x92] = "y2hex";
        _opcode_name_map[0x93] = "shmset";
        _opcode_name_map[0x94] = "shmget";
        _opcode_name_map[0x95] = "ergsysi";
        _opcode_name_map[0x96] = "flt2fix";
        _opcode_name_map[0x97] = "iupdate";
        _opcode_name_map[0x98] = "irange";
        _opcode_name_map[0x99] = "iincpos";
        _opcode_name_map[0x9A] = "tabseeku";
        _opcode_name_map[0x9B] = "flt2y4";
        _opcode_name_map[0x9C] = "flt2y8";
        _opcode_name_map[0x9D] = "y42flt";
        _opcode_name_map[0x9E] = "y82flt";
        _opcode_name_map[0x9F] = "plink";
        _opcode_name_map[0xA0] = "pcall";
        _opcode_name_map[0xA1] = "fcomp";
        _opcode_name_map[0xA2] = "plinkv";
        _opcode_name_map[0xA3] = "ppush";
        _opcode_name_map[0xA4] = "ppop";
        _opcode_name_map[0xA5] = "ppushflt";
        _opcode_name_map[0xA6] = "ppopflt";
        _opcode_name_map[0xA7] = "ppushy";
        _opcode_name_map[0xA8] = "ppopy";
        _opcode_name_map[0xA9] = "pjtsr";
        _opcode_name_map[0xAA] = "tabsetex";
        _opcode_name_map[0xAB] = "ufix2dez";
        _opcode_name_map[0xAC] = "generr";
        _opcode_name_map[0xAD] = "ticks";
        _opcode_name_map[0xAE] = "waitex";
        _opcode_name_map[0xAF] = "xopen";
        _opcode_name_map[0xB0] = "xclose";
        _opcode_name_map[0xB1] = "xcloseex";
        _opcode_name_map[0xB2] = "xswitch";
        _opcode_name_map[0xB3] = "xsendex";
        _opcode_name_map[0xB4] = "xrecvex";
        _opcode_name_map[0xB5] = "ssize";
        _opcode_name_map[0xB6] = "tabcols";
        _opcode_name_map[0xB7] = "tabrows";
    }
}

void bmwBESTVirtualMachineReloaded::execute_single_instruction() {

    uint32 eip_before_op = _current_context->eip;
    uint8 inst_buf[2];
    read_code(&inst_buf, 2);
    bmwBESTOperandAddrMode opmode0 = bmwBESTOperandAddrMode(((inst_buf[1] & 0xF0) >> 4));
    bmwBESTOperandAddrMode opmode1 = bmwBESTOperandAddrMode(((inst_buf[1] & 0x0F) >> 0));

    read_operand(opmode0, &_current_context->op_lhs, true);
    read_operand(opmode1, &_current_context->op_rhs, false);
    bmwBESTOperand *lhs = &_current_context->op_lhs;
    bmwBESTOperand *rhs = &_current_context->op_rhs;
    bmwBESTVMFlags *flags = &_current_context->flags;

    if (_current_context->current_trace) {
        bmwBESTVMTraceEntry trc;
        for (int32 i = 0; i < 8; i++)
            trc.int_regs[i] = _current_context->registers.rl[i].rl;

        trc.trace_addr = eip_before_op;
        trc.trace_flag_c = flags->carry;
        trc.trace_flag_o = flags->overflow;
        trc.trace_flag_z = flags->zero;
        trc.trace_flag_s = flags->sign;
        bmwBESTVMTraceResult trc_res = _current_context->current_trace->CheckTraceStep(&trc);
        switch (trc_res) {
            case bmwBESTVMTraceResult_trace_okay:
                //all okay, continue
                break;
            default:
                //return with trace error - do not execute next instruction
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                std::cout << "trace mismatch" << std::endl;
                _current_context->eoj_counter = 9000;
                return;
        }
    }




    std::string eip = bmwStringHelper::int_to_hex<uint32>(_current_context->eip);
    std::cout << eip << "," << flags->zero << "," << flags->carry << ","
    << flags->sign << "," << flags->overflow << ","
            << std::to_string(_current_context->registers.rl[0].rl) <<","
            << std::to_string(_current_context->registers.rl[1].rl) <<","
            << std::to_string(_current_context->registers.rl[2].rl) <<","
            << std::to_string(_current_context->registers.rl[3].rl) <<","
            << std::to_string(_current_context->registers.rl[4].rl) <<","
            << std::to_string(_current_context->registers.rl[5].rl) <<","
            << std::to_string(_current_context->registers.rl[6].rl) <<","
            << std::to_string(_current_context->registers.rl[7].rl) << std::endl;




    uint8 opcode = inst_buf[0];
    BMWLOG().log(dissasemble_instruction(opcode, lhs, rhs));

    //return;

    switch (opcode) {
        case 0x00: {
            //move
            // encountered:
            // move RS2[RL1], RAB0
            // move RL0, 100
            // move RS2, RS1
            // move RS1, "82FFF11A87"
            // RAB0, RS3[0]
            // move RS1[RAB0], 0x44
            // move RI2, RS3[RAB2]
            // move RI2, RS3[RAB2:RI3]
            //move RS4, RS3[RAB2:RI2]
            //move RS4, RS3[RB2:RI2]


            std::vector<uint8> src = rhs->get_raw_data();
            //std::cout << "src : " << bmwStringHelper::byte_array_to_string(src) << std::endl;
            std::vector<uint8> dest = lhs->get_raw_data();
            //std::cout << "before cpy dst : " << bmwStringHelper::byte_array_to_string(dest) << std::endl;


            //problem:
            //move RS1[0], 166
            //move RS1[1], RL2

            //if (rhs->omode == bmwBESTOperandAddrMode_IdxRegLenReg)
            //    std::cout << "idxreglenreg" << std::endl;


            //if (rhs->data.second.second > *lhs->absolute_register_size)
            //    *lhs->absolute_register_size = rhs->data.second.second;
            dest = lhs->get_raw_data();
            //std::cout << "after copy dst : " << bmwStringHelper::byte_array_to_string(dest) << std::endl;
            bmwBESTOperandCategory op0type = get_operand_type_category(lhs->omode);
            bmwBESTOperandCategory op1type = get_operand_type_category(rhs->omode);
            if (op0type == bmwBESTOperandCategory_FixedDataSize) {
                flags->carry = false;
                flags->overflow = false;
                int32 value = rhs->get_value(lhs->mapped_operand_data.len);
                lhs->set_raw_data(&value, lhs->mapped_operand_data.len);
                update_flags(rhs->get_value(), lhs->mapped_operand_data.len);
            } else {
                if (op1type == bmwBESTOperandCategory_FixedDataSize) {
                    flags->carry = false;
                    flags->overflow = false;
                    int32 value = rhs->get_value(rhs->mapped_operand_data.len);
                    lhs->set_raw_data(&value, rhs->mapped_operand_data.len);
                    update_flags(rhs->get_value(), 1);
                    //lhs->set_raw_data(rhs->get_raw_data().data(),1);
                } else {
                    //move RS4, RS3[RB2:RB2] - seems to work
                    flags->overflow = false;
                    flags->carry = false;
                    flags->zero = false;
                    flags->sign = false;
                    lhs->set_raw_data(rhs->get_raw_data().data(), rhs->get_raw_data().size());
                }
            }
        }
            break;

        case 0x01: {
            //clear
            lhs->clear();
            flags->carry = false;
            flags->zero = true;
            flags->sign = false;
            flags->overflow = false;
        }
            break;

        case 0x02: {
            //comp
            uint32 comp_lhs = lhs->get_value(rhs->mapped_operand_data.len);
            uint32 comp_rhs = rhs->get_value();
            uint32 len = rhs->mapped_operand_data.len;
            uint64 diff = (uint64) comp_lhs - (uint64) comp_rhs;
            update_flags((uint32) diff, len);
            set_overflow(comp_lhs, ((uint32) -comp_rhs), ((uint32) diff), len);
            set_carry(diff, len);
        }
            break;

        case 0x03: {
            //subb
            uint32 val_lhs = lhs->get_value(rhs->mapped_operand_data.len);
            uint32 val_rhs = rhs->get_value();
            uint32 len = lhs->mapped_operand_data.len;
            uint64 diff = (uint64) val_lhs - (uint64) val_rhs;
            lhs->set_raw_data(&diff, len);
            update_flags((uint32) diff, len);
            set_overflow(val_lhs, ((uint32) -val_rhs), ((uint32) diff), len);
            set_carry(diff, len);
        }
            break;

        case 0x04: {
            //adds
            //stupid: adds RS2[0], 3
            uint32 value_lhs = lhs->get_value(rhs->mapped_operand_data.len);
            uint32 value_rhs = rhs->get_value();
            uint32 len = rhs->mapped_operand_data.len;
            uint32 sum = (uint64) value_lhs + (uint64) value_rhs;
            lhs->set_raw_data(&sum, len);
            //lhs->set_int_value(sum);
            update_flags((uint32) sum, len);
            set_overflow(value_lhs, value_rhs, (uint32) sum, len);
            set_carry(sum, len);
        }
            break;

        case 0x05: {
            //mult
            uint32 value_lhs = lhs->get_value();
            uint32 value_rhs = rhs->get_value();
            uint32 len = rhs->mapped_operand_data.len;
            uint32 multip = value_lhs * value_rhs;
            lhs->set_raw_data(&multip, len);
            flags->overflow = false;
            update_flags(multip, len);
            if (rhs->is_register()) {
                uint32 resultHigh = (uint32) ((uint64) multip >> (int32) (len << 3));
                rhs->set_raw_data(&resultHigh, len);
            }
        }
            break;

        case 0x06: {
            //divs
            uint32 value_lhs = lhs->get_value();
            uint32 value_rhs = rhs->get_value();
            uint32 len = rhs->mapped_operand_data.len;
            uint64 divide = 0, remain = 0;
            bool error = false;
            if (value_rhs != 0) {
                divide = value_lhs / value_rhs;
                remain = value_lhs % value_rhs;
                lhs->set_raw_data(&divide, len);
                if (rhs->is_register()) {
                    rhs->set_raw_data(&remain, len);
                }
            } else {
                lhs->set_raw_data(&value_lhs, len);
            }
            flags->overflow = false;
            update_flags(divide, len);
        }
            break;

        case 0x07: {
            //and
            uint32 value_lhs = lhs->get_value(rhs->mapped_operand_data.len);
            uint32 value_rhs = rhs->get_value();
            uint32 len = rhs->mapped_operand_data.len;
            uint32 anded = value_rhs & value_lhs;
            lhs->set_raw_data(&anded, len);
            flags->overflow = false;
            update_flags(anded, len);
        }
            break;

        case 0x08: {
            //or
            uint32 value_lhs = lhs->get_value(rhs->mapped_operand_data.len);
            uint32 value_rhs = rhs->get_value();
            uint32 len = rhs->mapped_operand_data.len;
            uint32 ored = value_rhs | value_lhs;
            lhs->set_raw_data(&ored, len);
            flags->overflow = false;
            update_flags(ored, len);
        }
            break;

        case 0x09: {
            //xor
            uint32 value_lhs = lhs->get_value(rhs->mapped_operand_data.len);
            uint32 value_rhs = rhs->get_value();
            uint32 len = rhs->mapped_operand_data.len;
            uint32 xored = value_rhs ^value_lhs;
            lhs->set_raw_data(&xored, len);
            flags->overflow = false;
            update_flags(xored, len);
        }
            break;

        case 0x0a: {
            //not
            uint32 value_lhs = ~(lhs->get_value(rhs->mapped_operand_data.len));
            uint32 len = rhs->mapped_operand_data.len;
            lhs->set_raw_data(&value_lhs, len);
            flags->overflow = false;
            update_flags(value_lhs, len);
        }
            break;

        case 0x0b: {
            //jump
            _current_context->eip += lhs->get_value();
        }
            break;

        case 0x0e: {
            //jc
            if (flags->carry)
                _current_context->eip += lhs->get_value();
        }
            break;

        case 0x0f: {
            //jae
            if (!flags->carry)
                _current_context->eip += lhs->get_value();
        }
            break;

        case 0x10: {
            //jz
            if (flags->zero)
                _current_context->eip += lhs->get_value();
        }
            break;

        case 0x11: {
            //jnz
            if (!flags->zero)
                _current_context->eip += lhs->get_value();
        }
            break;

        case 0x12: {
            //jv
            if (flags->overflow)
                _current_context->eip += lhs->get_value();
        }
            break;

        case 0x13: {
            //jnv
            if (!flags->overflow)
                _current_context->eip += lhs->get_value();
        }
            break;

        case 0x14: {
            //jmi
            if (flags->sign)
                _current_context->eip += lhs->get_value();
        }
            break;

        case 0x15: {
            //jpl
            if (!flags->sign)
                _current_context->eip += lhs->get_value();
        }
            break;

        case 0x16: {
            //clrc
            flags->carry = false;
        }
            break;

        case 0x17: {
            //setc
            flags->carry = true;
        }
            break;

        case 0x18: {
            //asr
            uint32 value_lhs = lhs->get_value();
            uint32 value_rhs = rhs->get_value();
            uint32 len = lhs->mapped_operand_data.len;
            if (value_rhs < 0) {
            } else if (value_rhs == 0) {
                flags->carry = false;
            } else {
                if (value_rhs > len * 8) {
                    uint64 carrymask = (1 << ((len * 8) - 1));
                    flags->carry = ((value_lhs & carrymask)) != 0;
                } else {
                    uint64 carryshift = value_rhs - 1;
                    uint64 carrymask = 1 << carryshift;
                    flags->carry = ((value_lhs & carrymask)) != 0;
                }
                if (value_rhs >= len * 8) {
                    uint64 carrymask = (1 << ((len * 8) - 1));
                    if (((value_lhs & carrymask)) != 0)
                        value_lhs = 0xFFFFFFFF;
                    else
                        value_lhs = 0;
                } else {
                    value_lhs = value_lhs >> value_rhs;
                }
            }
            lhs->set_int_value(value_lhs);
            flags->overflow = false;
            update_flags(value_lhs, len);
        }
            break;

        case 0x19: {
            //lsl
            uint32 value_lhs = lhs->get_value();
            uint32 value_rhs = rhs->get_value();
            uint32 len = lhs->mapped_operand_data.len;
            if (value_rhs < 0) {
            } else if (value_rhs == 0) {
                flags->carry = false;
            } else {
                if (value_rhs > len * 8)
                    flags->carry = false;
                else {
                    int64 carryshift = (len << 3) - value_rhs;
                    uint32 carrymask = ((uint32) (1 << carryshift));
                    flags->carry = ((value_lhs & carrymask)) != 0;
                }
                if (value_rhs >= len * 8)
                    value_lhs = 0;
                else
                    value_lhs = value_lhs << value_rhs;
            }
            lhs->set_int_value(value_lhs);
            flags->overflow = false;
            update_flags(value_lhs, len);
        }
            break;

        case 0x1a: {
            //lsr
            uint32 value = lhs->get_value();
            uint32 shift = rhs->get_value();
            uint32 len = lhs->mapped_operand_data.len;
            if (shift < 0) {
            } else if (shift == 0) {
                _current_context->flags.carry = false;
            } else {
                if (shift > len * 8)
                    _current_context->flags.carry = false;
                else {
                    int64 carryshift = (len << 3) - shift;
                    uint32 carrymask = ((uint32) (1 << carryshift));
                    _current_context->flags.carry = ((value & carrymask)) != 0;
                }
                if (shift >= len * 8)
                    value = 0;
                else
                    value = value >> shift;
            }
            lhs->set_int_value(value);
            flags->overflow = false;
            update_flags(value, len);
        }
            break;

        case 0x1b: {
            //asl
            uint32 value = lhs->get_value();
            uint32 shift = rhs->get_value();
            uint32 len = lhs->mapped_operand_data.len;
            if (shift < 0) {
                // NO CARRY
            } else if (shift == 0) {
                _current_context->flags.carry = false;
            } else {
                if (shift > len * 8)
                    _current_context->flags.carry = false;
                else {
                    uint64 carryshift = (len << 3) - shift;
                    uint64 carrymask = 1 << carryshift;
                    _current_context->flags.carry = ((value & carrymask)) != 0;
                }
                if (shift >= len * 8)
                    value = 0;
                else
                    value = value << shift;
            }
            lhs->set_int_value(value);
            flags->overflow = false;
            update_flags(value, len);
        }
            break;

        case 0x1c: {
            //nop
        }
            break;

        case 0x1d: {
            //eoj
            _current_context->eoj_counter++;
        }
            break;

        case 0x1e: {
            //push
            int32 push_val = lhs->get_value();
            for (int32 i = 0; i < lhs->mapped_operand_data.len; i++) {
                _current_context->stack.push_back(((uint8) push_val));
                push_val = push_val >> 8; // move to next byte
            }
        }
            break;

        case 0x1f: {
            //pop
            int32 value = 0;
            for (int32 i = 0; i < lhs->mapped_operand_data.len; i++) {
                value = value << 8;
                uint8 b = _current_context->stack.back();
                _current_context->stack.pop_back();
                value = value | b;
            }
            lhs->set_raw_data(&value, lhs->mapped_operand_data.len);
            flags->overflow = false;
            update_flags(value, lhs->mapped_operand_data.len);

        }
            break;

        case 0x20: {
            //scmp
            flags->zero = (lhs->get_raw_data() == rhs->get_raw_data());
        }
            break;

        case 0x21: {
            //scat
            std::vector<uint8> data_lhs = lhs->get_raw_data();
            std::vector<uint8> data_rhs = rhs->get_raw_data();
            std::vector<uint8> catarray;
            catarray.insert(catarray.end(), data_lhs.begin(), data_lhs.end());
            catarray.insert(catarray.end(), data_rhs.begin(), data_rhs.end());
            if (catarray.size() > BMW_BESTVM_STRING_MAXSIZE) {
                BMWLOG().error("BMWVM : scat too long");
            }
            lhs->set_raw_data(catarray.data(), catarray.size());
        }
            break;

        case 0x22: {
            //scut
            //strcut shortens string s by count characters. The string cannot be less than 0 characters long.
            std::vector<uint8> data_lhs = lhs->get_raw_data();
            int32 cutlen = rhs->get_value();
            if (cutlen <= (data_lhs.size())) {
                std::vector<uint8> resarray(data_lhs);
                resarray.resize(data_lhs.size() - cutlen);
                lhs->set_raw_data(resarray.data(), resarray.size());
            }
        }
            break;

        case 0x23: {
            //slen
            uint32 len = rhs->mapped_operand_data.len;
            lhs->set_int_value(len);
        }
            break;

        case 0x24: {
            //spaste
            //spaste RS2[3], RS1
            //insert RS1 into RS2 at position 3


            std::vector<uint8> data_lhs = lhs->get_raw_data();
            std::vector<uint8> data_rhs = rhs->get_raw_data();

            std::vector<uint8> data_concat;

            data_concat.insert(data_concat.end(), data_rhs.begin(), data_rhs.end());
            data_concat.insert(data_concat.end(), data_lhs.begin(), data_lhs.end());
            lhs->set_raw_data(data_concat.data(), data_concat.size());

            //BMWLOG().error("spaste (datainstert) not impl");
        }
            break;

        case 0x25: {
            //serase
            std::vector<uint8> data_lhs = lhs->get_raw_data();
            int32 len = rhs->get_value();
            std::vector<uint8> data_erase;
            data_erase.insert(data_erase.end(), data_lhs.begin() + len, data_lhs.end());

            lhs->set_raw_data(data_erase.data(), data_erase.size());
            *lhs->absolute_register_data.len = *lhs->absolute_register_data.len - len;
        }
            break;

        case 0x26: //xconnect
        case 0x27: //xhangup
        case 0x28: //xsetpar
        case 0x29: //xawlen
            break;

        case 0x2a: {
            //xsend
            std::vector<uint8> send_payload = rhs->get_raw_data();

            unsigned char cas_cbs_daten_lesen[72] = {
                    0x80, 0xF1, 0x60, 0x43, 0x62, 0x10, 0x01, 0x04, 0x00, 0x00, 0x01, 0x01, 0xFF, 0x00, 0x00, 0x01,
                    0x01, 0x2C, 0x01, 0x01, 0xFF, 0x00, 0x00, 0x02, 0x05, 0xC7, 0x01, 0x01, 0xFF, 0x00, 0x00, 0x06,
                    0x06, 0xA5, 0x01, 0x01, 0xFF, 0x00, 0x00, 0x11, 0x0F, 0xC4, 0x01, 0x01, 0xFF, 0x00, 0x00, 0x20,
                    0xFF, 0xFF, 0x02, 0x01, 0xFF, 0x00, 0x00, 0x21, 0xFF, 0xFF, 0x02, 0x01, 0xFF, 0x00, 0x00, 0x03,
                    0xFF, 0xFF, 0x02, 0x01, 0xFF, 0x00, 0x00, 0x65
            };


            unsigned char CAS_IDENT[35] = {
                    0x9F, 0xF1, 0x40, 0x5A, 0x80, 0x00, 0x00, 0x06, 0x94, 0x38,
                    0x30, 0xC3, 0x06, 0x06, 0xA0, 0x53, 0x41, 0x20, 0x07, 0x01, 0x18, 0x04, 0x00, 0x00, 0x00, 0x01,
                    0x07, 0x01, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x02
            };


            unsigned char KOMB60_SERIENNUMMER_LESEN[15] = {
                    0x8B, 0xF1, 0x60, 0x5A, 0x89, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, 0x39, 0x38, 0x81
            };


            unsigned char KOMB60_STATUS_GWSZ_OFFSET[7] = {
                    0x83, 0xF1, 0x60, 0x61, 0x17, 0x46, 0x92
            };


            //KOMB60_STATUS_A_TEMP_LESEN
            unsigned char KOMB60_STATUS_A_TEMP_LESEN[8] = {
                    0x84, 0xF1, 0x60, 0x61, 0x22, 0xB4, 0x00, 0x0C
            };


            unsigned char ihka87_fs_lesen_detail[15] = {
                    0x8B, 0xF1, 0x78, 0x57, 0x02, 0x9C, 0x6C, 0x31, 0x05, 0x58, 0x43, 0x85, 0x6E, 0x5A, 0x73
            };


            unsigned char KOMB60_LESEN_HW_NUMMER[24] = {
                    0x94, 0xF1, 0x60, 0x5A, 0x87, 0x00, 0x00, 0x06, 0x92, 0x43, 0x54, 0x00, 0x00, 0x06, 0x92, 0x43,
                    0x54, 0x00, 0x00, 0x06, 0x92, 0x43, 0x54, 0x53
            };


            unsigned char cas_status_fzg_typ[18] = {
                    0x8E, 0xF1, 0x40, 0x62, 0x10, 0x11, 0x16, 0x64, 0x01, 0x0C, 0x14, 0x02, 0xC0, 0x01, 0x08, 0x50,
                    0x64, 0x5C
            };


            bmwMessageRaw data_snd = rhs->get_raw_data();
            //BMWLOG().debug("Requesting message send : " + bmwStringHelper::byte_array_to_string(data_snd));

            std::vector<uint8> response;


            if (_current_context->current_trace) {

                response = _current_context->current_trace->GetResponseStep();
                if (response.empty()) {
                    BMWLOG().error("Simulated xsend response was empty!");
                    //arg0->SetArrayData((void *) &e60_ident[0], 35);
                    lhs->clear();
                    set_error(bmwBESTVMErrorCodes::EDIABAS_IFH_0009);
                } else {


                    //convert
                    std::vector<uint8> framepayload(response.begin() + 2, response.end());
                    bmwAdapterBMWFAST fast("", "", nullptr);
                    response = fast.CreateBMWFASTMessage(bmwAddressingMode_physical,response[1],response[0],framepayload, true);

                    lhs->set_raw_data(&response[0], response.size());
                    set_error(bmwBESTVMErrorCodes::EDIABAS_ERR_NONE);
                    BMWLOG().debug(
                            "Simulated xsend response : " +
                            bmwStringHelper::byte_array_to_string(response, -1, true, ' '));
                }
            } else {
                if (_adapter) {
                    if ((!_current_context->cached_xsend_request_payload.empty()) || (data_snd.empty())) {
                        //we have in the past recieved more than one message from a single send
                        //if we have more queued, return one of those (or if send msg is empty)
                        if ((_current_context->cached_xsend_request_payload == data_snd) || (data_snd.empty())) {
                            //we need to check if we still have cached messages
                            if (!_current_context->cached_xsend_responses.empty()) {
                                set_error(bmwBESTVMErrorCodes::EDIABAS_ERR_NONE);
                                response = _current_context->cached_xsend_responses.front();
                                _current_context->cached_xsend_responses.pop();
                                lhs->set_raw_data(&response[0], response.size());
                                BMWLOG().debug(
                                        "Used cached message from previous recv : " +
                                        bmwStringHelper::byte_array_to_string(response));
                            } else {
                                //we used up all of our cached messages
                                lhs->clear();
                                _current_context->cached_xsend_request_payload.clear();
                                BMWLOG().debug(
                                        "Used last cached message last cycle, returning 0 len once");
                                set_error(bmwBESTVMErrorCodes::EDIABAS_IFH_0009);
                            }

                        } else {
                            //we are requested to send a new message even though we still have queued messages. Dangerous?
                            BMWLOG().error(
                                    "Still got cached responses, but VM requests to send new message. Error!");
                            set_error(bmwBESTVMErrorCodes::EDIABAS_IFH_0009);
                        }
                    } else {
                        //We never sent a message before, send it
                        set_error(bmwBESTVMErrorCodes::EDIABAS_ERR_NONE);
                        BMWLOG().debug("Sending message : " + bmwStringHelper::byte_array_to_string(data_snd));
                        bmwAdapterIOTransactionResult res = _adapter->SendMessage(data_snd);
                        switch (res.first) {
                            case bmwAdapterIOResultCode_success:

                                if (!res.second.empty()) {

                                    /*
                                     * RESEARCH:
                                     *
                                     * It seems like xsend always only returns **1** message, no matter how many we actually recieved.
                                     * It also seems like that the xsend function shall cache if we got more than one result message
                                     * Then, on each following xsend call with same send data, it shall not send, but instead dequeue the last message
                                     * Until the queue is empty - then it actually sends again
                                     * Who invented this piece of shit specification
                                     *
                                     */
                                    if (res.second.size() > 1) {
                                        BMWLOG().debug(
                                                "Recv more than one message, returning first and caching rest");

                                        std::reverse(res.second.begin(), res.second.end());
                                        //save last, return it now, and cache the rest
                                        response = res.second.back();
                                        res.second.pop_back();

                                        for (bmwMessageRaw rawmsg : res.second)
                                            _current_context->cached_xsend_responses.emplace(rawmsg);

                                    } else if (res.second.size() == 1) {
                                        response = res.second.front();
                                    }

                                    BMWLOG().debug(
                                            "Recv message : " + bmwStringHelper::byte_array_to_string(response));
                                    lhs->set_raw_data(response.data(), response.size());


                                } else {
                                    BMWLOG().debug("Got no messages!");
                                    lhs->set_raw_data(nullptr, 0);
                                    set_error(bmwBESTVMErrorCodes::EDIABAS_IFH_0009);
                                }
                                break;
                            case bmwAdapterIOResultCode_error_send:
                                set_error(bmwBESTVMErrorCodes::EDIABAS_IFH_0003);
                                BMWLOG().error("BESTVM :: Failed to send data");
                                break;
                            case bmwAdapterIOResultCode_error_recv:
                                set_error(bmwBESTVMErrorCodes::EDIABAS_IFH_0003);
                                BMWLOG().error("BESTVM :: Recv error");
                                break;
                            case bmwAdapterIOResultCode_error_no_answer:
                                set_error(bmwBESTVMErrorCodes::EDIABAS_IFH_0009);
                                BMWLOG().error("BESTVM :: No answer from ECU");
                                break;
                            case bmwAdapterIOResultCode_error_not_connected:
                                set_error(bmwBESTVMErrorCodes::EDIABAS_IFH_0002);
                                BMWLOG().error("BESTVM :: No response from interface");
                                break;
                        }
                    }


                } else {
                    //we actually only want to test in this case, so we return some predetermined data
                    BMWLOG().debug("Returned hardcoded xsend response!");
                    lhs->set_raw_data((void *) &CAS_IDENT[0], sizeof(CAS_IDENT));
                }


            }

            BMWLOG().debug("xsend end trap register state: mask " +
                           std::to_string(_current_context->trap_mask) + ", index " +
                           std::to_string(_current_context->trap_index));


            /*
            const uint8 e60_ident[35] = {
                    0x9F, 0xF1, 0x60, 0x5A, 0x80, 0x00, 0x00, 0x06, 0x93, 0x76, 0x08, 0x06,
                    0x04, 0x06, 0xE0, 0x48, 0x55, 0x20, 0x02, 0x11, 0x28, 0x10, 0x00, 0x0A,
                    0x98, 0x04, 0x06, 0x10, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x9A};
                    */

            //lhs->set_raw_data((void *) &e60_ident[0], 35);
            //lhs->set_raw_data(&CAS_IDENT[0], 35);
        }
            break;

        case 0x2b: //xsendf
        case 0x2c: //xrequf
        case 0x2d: //xstopf
        case 0x2e: //xkeyb
        case 0x2f: //xstate
        case 0x30: //xboot
        case 0x31: //xreset
            break;

        case 0x32: {
            char const *interfacetype = "OBD";
            lhs->set_string_value(const_cast<char *>(interfacetype));
        }
            break;

        case 0x33: {
            //xvers
            lhs->set_int_value(209);
        }
            break;

        case 0x81: //ergc
        case 0x34: {
            //ergb
            std::string res_name = lhs->get_c_string();
            uint8 res_val = rhs->get_value();
            _current_context->results.back().push_back({res_name, std::to_string(res_val)});
        }
            break;

        case 0x35: {
            //ergw
            std::string res_name = lhs->get_c_string();
            uint16 res_val = rhs->get_value();
            _current_context->results.back().push_back({res_name, std::to_string(res_val)});
        }
            break;

        case 0x36: {
            //ergd
            std::string res_name = lhs->get_c_string();
            uint32 res_val = rhs->get_value();
            _current_context->results.back().push_back({res_name, std::to_string(res_val)});
        }
            break;

        case 0x82: //ergl
        case 0x37: {
            //ergi
            std::string res_name = lhs->get_c_string();
            int32 res_val = rhs->get_value();
            _current_context->results.back().push_back({res_name, std::to_string(res_val)});
        }
            break;

        case 0x38: {
            //ergr
            std::string res_name = lhs->get_c_string();
            f32 res_val = rhs->cast_data<f32>(true);
            _current_context->results.back().push_back({res_name, std::to_string(res_val)});
        }
            break;

        case 0x39: {
            //ergs
            std::string res_name = lhs->get_c_string();
            std::string res_value = rhs->get_c_string();
            _current_context->results.back().push_back({res_name, res_value});
        }
            break;

        case 0x3a: {
            //a2flt
            std::string strfloat = rhs->get_c_string();
            bmwStringHelper::replace(strfloat, ",", ".");
            float f = 0.0;
            if (!strfloat.empty()) {
                try {
                    f = std::stof(strfloat);
                } catch (...) {
                    f = 0.0;
                }
            }
            lhs->set_value<f32>(f);
        }
            break;

        case 0x3b: {
            //fadd
            f32 flhs = lhs->cast_data<f32>(true);
            f32 frhs = rhs->cast_data<f32>(true);
            lhs->set_value<f32>(flhs + frhs);
        }
            break;

        case 0x3c: {
            //fsub
            f32 flhs = lhs->cast_data<f32>(true);
            f32 frhs = rhs->cast_data<f32>(true);
            lhs->set_value<f32>(flhs - frhs);
        }
            break;

        case 0x3d: {
            //fmul
            f32 flhs = lhs->cast_data<f32>(true);
            f32 frhs = rhs->cast_data<f32>(true);
            lhs->set_value<f32>(flhs * frhs);
        }
            break;

        case 0x3e: {
            //fdiv
            f32 flhs = lhs->cast_data<f32>(true);
            f32 frhs = rhs->cast_data<f32>(true);
            f32 res = 0.0;
            if (frhs != 0) {
                res = flhs / frhs;
            }
            lhs->set_value<f32>(res);
        }
            break;

        case 0x3f: {
            //ergy
            std::string res_name = lhs->get_c_string();
            std::vector<uint8> bytes = rhs->get_raw_data();
            _current_context->results.back().push_back({res_name, bmwStringHelper::byte_array_to_string(bytes)});
        }
            break;

        case 0x40: {
            //Enewset
            _current_context->results.resize(_current_context->results.size() + 1);
        }
            break;

        case 0x41: {
            // check if we have result
            //do nothing; we want every result
        }
            break;

        case 0x42: {
            // set number of repeat connections
        }
            break;

        case 0x43: {
            //gettmr
            lhs->set_raw_data(&_current_context->trap_mask, lhs->mapped_operand_data.len);
            update_flags(_current_context->trap_mask, lhs->mapped_operand_data.len);
        }
            break;

        case 0x44: {
            //settmr
            _current_context->trap_mask = (uint32) lhs->get_value();
        }
            break;

        case 0x45: {
            //sett
            uint32 error = lhs->get_value();
            if (error == 0) {
                error = 0x40000000;
            }
            _current_context->trap_index = (int32) error;
        }
            break;

        case 0x46: {
            //clrt
            _current_context->trap_index = -1;
        }
            break;

        case 0x47: {
            //jt
            bool error_detected = false;
            if (rhs->omode != bmwBESTOperandAddrMode_None) {
                uint8 testBit = rhs->get_value(1);
                if (testBit > 0) {
                    if (_current_context->trap_index == (int) testBit) {
                        error_detected = true;
                    }
                    if (_current_context->trap_index == 0 && testBit == 32) {
                        error_detected = true;
                    }
                } else {
                    if (_current_context->trap_index >= 0x40000000) {
                        error_detected = true;
                    }
                }
            } else {
                if (_current_context->trap_index >= 0) {
                    error_detected = true;
                }
            }
            if (error_detected) {
                _current_context->eip += lhs->get_value();
                //BMWLOG().log("jt jump");
            } else {
                //BMWLOG().log("jt NOT jump");
            }
        }
            break;

        case 0x48: {
            //jnt
            bool error_detected = false;
            if (rhs->omode != bmwBESTOperandAddrMode_None) {
                uint32 test_bit = rhs->get_value(1);
                if (test_bit > 0) {
                    if (_current_context->trap_index == (int32) test_bit) {
                        error_detected = true;
                    }
                    if ((_current_context->trap_index == 0) && (test_bit == 32)) {
                        error_detected = true;
                    }
                } else {
                    if (_current_context->trap_index >= 0x40000000) {
                        error_detected = true;
                    }
                }
            } else {
                // Ediabas failure, should be identical to OpJt
                // incorrect behaviour if no argument is specified
                if (_current_context->trap_index >= 0x40000000) {
                    error_detected = true;
                }
            }
            if (!error_detected) {
                _current_context->eip += lhs->get_value();
                //BMWLOG().log("jnt jump");
            } else {
                //BMWLOG().log("jnt NOT jump");
            }

        }
            break;

        case 0x49: {
            //addc
            uint32 value_lhs = lhs->get_value();
            uint32 value_rhs = rhs->get_value();
            uint32 len = lhs->mapped_operand_data.len;
            if (flags->carry)
                value_rhs = value_rhs + 1;
            uint64 sum = value_lhs + value_rhs;
            lhs->set_int_value(sum);
            update_flags(sum, len);
            set_overflow(value_lhs, value_rhs, sum, len);
            set_carry(sum, len);
        }
            break;

        case 0x4a: {
            //subc
            uint32 value_lhs = lhs->get_value();
            uint32 value_rhs = rhs->get_value();
            uint32 len = lhs->mapped_operand_data.len;
            if (flags->carry)
                value_rhs = value_rhs + 1;
            uint64 sum = value_lhs - value_rhs;
            lhs->set_int_value(sum);
            update_flags(sum, len);
            set_overflow(value_lhs, -value_rhs, sum, len);
            set_carry(sum, len);
        }
            break;

        case 0x4b: {
            //break
            // just error stuff, userbreak, not gonna implement
            BMWLOG().error("not impl userbreak");
        }
            break;

        case 0x4c: {
            //clrv
            flags->overflow = false;
        }
            break;

        case 0x4d: //eerr
        case 0x4e: //popf
        case 0x4f: { //pushf
            BMWLOG().error("not impl");
        }
            break;

        case 0x50: {
            //atsp
            uint32 len = lhs->mapped_operand_data.len;
            uint32 pos = rhs->get_value();
            std::vector<uint8> localstack = _current_context->stack;
            std::reverse(localstack.begin(), localstack.end());
            uint32 indx = pos - len;
            uint32 value = 0;
            for (int i = 0; i < len; i++) {
                value = value << 8;
                value = value | localstack[indx];
                indx++;
            }
            lhs->set_int_value(value);
            update_flags(value, len);
        }
            break;

        case 0x52: {
            //setspc
            std::string seperator = bmwStringHelper::remove_zero_terminator(lhs->get_c_string());
            _current_context->token_seperator = seperator;
            _current_context->token_index = rhs->get_value();
        }
            break;

        case 0x53: {
            //srevers
            std::string rvrs = lhs->get_c_string();
            std::reverse(rvrs.begin(), rvrs.end());
            lhs->set_string_value(const_cast<char *>(rvrs.c_str()));
        }
            break;

        case 0x54: {
            //stoken
            std::string rvrs = lhs->get_c_string();
            char *dbg = const_cast<char *>(_current_context->token_seperator.c_str());
            if (_current_context->token_seperator.length() == 0) {
                flags->zero = true;
            } else {
                std::string splitstring = rhs->get_c_string();
                std::vector<std::string> words = bmwStringHelper::split(splitstring, _current_context->token_seperator);
                if ((_current_context->token_index < 1) || (_current_context->token_index > words.size())) {
                    flags->zero = true;
                } else {
                    lhs->set_string_value(const_cast<char *>(words[_current_context->token_index - 1].c_str()));
                    flags->zero = false;
                }
            }
        }
            break;

        case 0x55: //parb
        case 0x56: //parw
        case 0x57: //parl
        {
            uint32 res = 0;
            flags->carry = false;
            flags->sign = false;
            flags->overflow = false;
            flags->zero = true;
            uint32 pos = rhs->get_value();
            pos--;
            if (pos < _current_context->job_params.size()) {
                if (_current_context->job_params[pos] != "") {

                    res = bmwStringHelper::to_integer<uint32>(_current_context->job_params[pos]);
                    flags->zero = false;
                }
            }
            lhs->set_int_value(res);
        }
            break;

        case 0x58: //pars
        {
            flags->zero = true;
            uint32 pos = rhs->get_value();
            std::vector<uint8> res;
            pos--; // 0 indexed please you twats
            if (pos < _current_context->job_params.size()) {
                if (_current_context->job_params[pos] != "") {
                    std::string param = _current_context->job_params[pos];
                    std::copy(param.begin(), param.end(), std::back_inserter(res));
                    res.push_back(0); //zero term
                    flags->zero = false;
                }
            }
            lhs->set_raw_data(res.data(), res.size());
            //lhs->set_string_value(const_cast<char *>(res.c_str()));
        }
            break;

        case 0x5a: {
            //jg
            if ((!flags->zero) & (flags->sign == flags->overflow))
                this->_current_context->eip += lhs->get_value();
        }
            break;

        case 0x5b: {
            //jge
            if (flags->zero | (flags->overflow == flags->sign))
                this->_current_context->eip += lhs->get_value();
        }
            break;

        case 0x5c: {
            //jl
            if (!flags->zero && (flags->sign != flags->overflow))
                this->_current_context->eip += lhs->get_value();
        }
            break;

        case 0x5d: {
            //jle
            if ((flags->sign != flags->overflow) || flags->zero)
                this->_current_context->eip += lhs->get_value();
        }
            break;

        case 0x5e: {
            //ja
            if ((!flags->carry) && (!flags->zero))
                this->_current_context->eip += lhs->get_value();
        }
            break;

        case 0x5f: {
            //jbe
            if (!flags->carry)
                this->_current_context->eip += lhs->get_value();
        }
            break;

        case 0x67: {
            //a2fix
            std::string s = rhs->get_c_string();
            uint32 v = 0;
            if (!s.empty()) {
                try {
                    v = bmwStringHelper::to_integer<uint32>(s);
                } catch (...) {
                    v = 0;
                }
            }
            lhs->set_int_value(v);
            flags->zero = false;
            flags->sign = false;
            flags->overflow = false;
        }
            break;

        case 0x68: {
            //fix2flt
            int32 val = rhs->get_value();
            lhs->set_value<f32>((f32) val);
        }
            break;

        case 0x69: {
            //parr
            int32 pos = rhs->get_value();
            f32 res = 0;
            flags->zero = true;
            flags->carry = false;
            flags->sign = false;
            flags->overflow = false;
            pos--;
            if (pos < _current_context->job_params.size()) {
                if (_current_context->job_params[pos] != "") {
                    res = std::atof(_current_context->job_params[pos].c_str());
                    flags->zero = false;
                }
            }
            lhs->set_value<f32>(res);
        }
            break;

        case 0x6b: {
            //wait
            std::this_thread::sleep_for(std::chrono::seconds(lhs->get_value()));
        }
            break;

        case 0x6c: {
            //date
            uint8 datearr[5];
            std::time_t t = std::time(0);   // get time now
            std::tm *now = std::localtime(&t);
            datearr[0] = now->tm_mday;
            datearr[1] = now->tm_mon;
            datearr[2] = now->tm_year + 1900;
            datearr[3] = (uint8) now->tm_yday / 7;
            datearr[4] = now->tm_wday;
            lhs->set_raw_data(&datearr[0], 5);
        }
            break;

        case 0x6d: {
            //time
            uint8 datearr[3];
            std::time_t t = std::time(0);   // get time now
            std::tm *now = std::localtime(&t);
            datearr[0] = now->tm_hour;
            datearr[1] = now->tm_min;
            datearr[2] = now->tm_sec;
            lhs->set_raw_data(&datearr[0], 3);
        }
            break;

        case 0x72: //xignit
        case 0x6e: {
            //xbatt
            lhs->set_int_value(13200); //13v
            BMWLOG().debug("VM reported 13.2v battery voltage to job");
        }
            break;

        case 0x79: {
            //fix2hex
            //this can actually also be called like this:
            //fix2hex RS1, RS3[RB0]
            //This means we need to handle the case where a single byte is supposed to be converted
            uint32 len = (get_operand_type_category(rhs->omode) != bmwBESTOperandCategory_FixedDataSize) ? 1
                                                                                                         : rhs->mapped_operand_data.len;

            uint32 val = rhs->get_value();
            std::string str;
            switch (len) {
                case 1:
                    str = bmwStringHelper::int_to_hex<uint8>(val);
                    break;
                case 2:
                    str = bmwStringHelper::int_to_hex<uint16>(val);
                    break;
                case 4:
                    str = bmwStringHelper::int_to_hex<int32>(val);
                    break;
                default:
                    BMWLOG().error("fix2hex : Not handled case!");
            }
            lhs->set_string_value(const_cast<char *>((std::string("0x") + str).c_str()));

        }
            break;

        case 0x7a: {
            //fix2dez
            uint32 val = rhs->get_value();
            lhs->set_string_value(const_cast<char *>(std::to_string(val).c_str()));
        }
            break;

        case 0x7b: {
            //tabset
            std::string table_name = bmwStringHelper::trim(lhs->get_c_string());
            _current_context->tabset_table = _current_context->best_obj->tables[bmwStringHelper::uppercase(table_name)];
        }
            break;

        case 0x7c: {
            //tabseek
            if ((_current_context->tabset_table != nullptr)) {
                _current_context->tabseek_colname = bmwStringHelper::trim(lhs->get_c_string());
                _current_context->tabseek_valname = bmwStringHelper::trim(rhs->get_c_string());
                flags->zero = !_current_context->tabset_table->GetColumnRow_ColName_RowStr(
                        _current_context->tabseek_colname,
                        _current_context->tabseek_valname,
                        &_current_context->tabseek_row_index);

                //std::cout << "tabseek " << _current_context->tabset_table->table_name << std::endl;
                //std::cout << "colname: " << _current_context->tabseek_colname << std::endl;
                //std::cout << "valname: " << _current_context->tabseek_valname << std::endl;
                //std::cout << "res row: " << _current_context->tabseek_row_index << std::endl;
            }
        }
            break;

        case 0x7d: {
            //tabget
            std::string out_Str;
            flags->zero = true;
            //std::cout << "tabget" << std::endl;
            if ((_current_context->tabset_table != nullptr)) {
                // std::cout << "cur table : " << _current_context->tabset_table->table_name  << std::endl;
                //std::cout << "wanted col : " << rhs->get_c_string()  << std::endl;
                int32 column_index = _current_context->tabset_table->GetColumnIndex(rhs->get_c_string());

                if (column_index == BMW_TABLE_NOT_FOUND)
                    column_index = _current_context->tabset_table->GetColumnIndex(rhs->get_c_string());

                if (column_index != BMW_TABLE_NOT_FOUND) {
                    //   std::cout << "col index : " << column_index  << std::endl;
                    //   std::cout << "cur row index : " << _current_context->tabseek_row_index  << std::endl;
                    if (_current_context->tabset_table->TryGetDataCell(_current_context->tabseek_row_index,
                                                                       column_index,
                                                                       &out_Str)) {
                        //     std::cout << "wanted value : " << out_Str  << std::endl;
                        flags->zero = false;
                        lhs->set_string_value(const_cast<char *>(out_Str.c_str()));
                    }
                }
            }
        }
            break;

        case 0x7e: {
            //strcat
            char *strlhs = lhs->get_c_string();
            char *strrhs = rhs->get_c_string();
            std::string catted(std::string(strlhs) + strrhs);
            lhs->set_string_value(const_cast<char *>(catted.c_str()));
        }
            break;

        case 0x7f: {
            //pary
            std::string res;
            flags->zero = true;
            //assumes one argument, which we want as hex
            if (_current_context->job_params.size()) {
                flags->zero = false;
                std::vector<uint8> hex = bmwStringHelper::to_byte_array(_current_context->job_params.front());
                lhs->set_raw_data(hex.data(), hex.size());
            } else {
                lhs->clear();
            }
        }
            break;

        case 0x83: {
            //tabline
            if ((_current_context->tabset_table != nullptr)) {
                uint32 wanted_row = lhs->get_value();
                if (_current_context->tabset_table->GetRowCount() < wanted_row) {
                    flags->zero = false;
                    _current_context->tabseek_row_index = wanted_row;
                } else {
                    flags->zero = true;
                    // set row rto last line according to BEST2 doc
                    _current_context->tabseek_row_index = _current_context->tabset_table->GetRowCount() - 1;
                }
            }
        }
            break;

        case 0x87: {
            //flt2a
            std::string res = std::to_string(rhs->cast_data<f32>(true));
            lhs->set_string_value(const_cast<char *>(res.c_str()));
        }
            break;

        case 0x88: {
            //setflt
            uint32 float_precision = lhs->get_value();
        }
            break;

        case 0x89: {
            //cfgig
            std::string requested_config;
            requested_config = bmwStringHelper::uppercase(rhs->get_c_string());
            if (requested_config == "SIMULATION") {
                lhs->set_int_value(0);// never  a simulation
            }
        }
            break;

        case 0x8a: {
            //cfgsg
            std::string requested_configrequested_config = bmwStringHelper::uppercase(rhs->get_c_string());
            if (requested_configrequested_config == bmwStringHelper::uppercase("BipEcuFile")) {
                lhs->set_string_value(
                        const_cast<char *>(_current_context->best_obj->filename.c_str())); // current ecu file name
            }
        }
            break;

        case 0x8b: {
            //setting change, don't care
        }
            break;

        case 0x8c: {
            //a2y
            //string to byte array?
            std::string input_str = rhs->get_c_string();
            std::vector<uint8> result_array;
            if (!input_str.empty()) {
                bmwStringHelper::lowercase(input_str);
                bmwStringHelper::replace(input_str, ";", "");
                bmwStringHelper::replace(input_str, ",", "");
                bmwStringHelper::replace(input_str, " ", "");
                result_array = bmwStringHelper::to_byte_array(input_str);
            }
            lhs->set_raw_data(result_array.data(), result_array.size());

        }
            break;

        case 0x8e: {
            //hex2y
            std::string s = rhs->get_c_string();
            std::vector<uint8> ba = bmwStringHelper::to_byte_array(s);
            flags->carry = false;
            lhs->set_raw_data(ba.data(), ba.size());
        }
            break;

        case 0x8f: {
            //strcmp
            char *lhsstr = lhs->get_c_string();
            char *rhsstr = rhs->get_c_string();
            flags->zero = (strcmp(lhsstr, rhsstr));
        }
            break;

        case 0x90: {
            //strlen
            char *rhsstr = rhs->get_c_string();
            uint32 sl = strlen(rhsstr);
            lhs->set_int_value(sl);
            flags->overflow = false;
            update_flags(sl, lhs->mapped_operand_data.len);
        }
            break;

        case 0x91: {
            //y2bcd
            std::string res;
            std::vector<uint8> bytes = rhs->get_raw_data();
            static const char outputs[] = "0123456789ABCDEF";
            for (uint8 b : bytes) {
                res += outputs[b >> 4];
                res += outputs[b & 0xf];
            }
            lhs->set_string_value(const_cast<char *>(res.c_str()));
        }
            break;

        case 0x92: {
            //y2hex
            std::string res;
            std::vector<uint8> bytes = rhs->get_raw_data();
            for (uint8 byte : bytes) {
                res += bmwStringHelper::int_to_hex<uint8>(byte);
            }
            lhs->set_string_value(const_cast<char *>(res.c_str()));
        }
            break;

        case 0x93: {
            //shmset
            _current_context->shared_memory_name = lhs->get_c_string();
            auto shm = rhs->get_raw_data();
            //std::cout << "shmset : " << bmwStringHelper::byte_array_to_string(shm);
            memcpy(&_current_context->shared_memory[0], shm.data(), shm.size());
        }
            break;

        case 0x94: {
            //shmget
            std::string shm_name = rhs->get_c_string();
            if (shm_name == _current_context->shared_memory_name) {
                if (lhs->omode != bmwBESTOperandAddrMode_RegS) {
                    BMWLOG().error("shmget : target register not a string register");
                } else {
                    lhs->set_raw_data(&_current_context->shared_memory[0], BMW_BESTVM_STRING_MAXSIZE);
                    flags->carry = false;
                }
            } else {
                flags->carry = true;
                lhs->clear();
            }
        }
            break;

        case 0x96: {
            //flt2fix
            uint32 res = round(rhs->cast_data<f32>(true));
            lhs->set_int_value(res);
            flags->overflow = false;
            update_flags(res, sizeof(uint32));
        }
            break;

        case 0x97: {
            BMWLOG().log("Job reports : " + std::string(lhs->get_c_string()));
        }
            break;

        case 0x9a: {
            //tabseeku
            if ((_current_context->tabset_table != nullptr)) {
                _current_context->tabseek_colname = bmwStringHelper::trim(lhs->get_c_string());
                int32 int_data = rhs->get_value();
                _current_context->tabseek_valname = std::to_string(
                        int_data);//"0x" + bmwStringHelper::int_to_hex<uint8>(int_data);
                flags->zero = !_current_context->tabset_table->GetColumnRow_ColName_RowStr(
                        _current_context->tabseek_colname,
                        _current_context->tabseek_valname,
                        &_current_context->tabseek_row_index);

            }
            //std::cout << "tabseeku " << _current_context->tabset_table->table_name << std::endl;
            //std::cout << "colname: " << _current_context->tabseek_colname << std::endl;
            //std::cout << "valname: " << _current_context->tabseek_valname << std::endl;
            //std::cout << "res row: " << _current_context->tabseek_row_index << std::endl;
        }
            break;

        case 0x9d:  //y42flt
        case 0x9e: {//y82flt
            lhs->set_value<f32>(rhs->cast_data<f32>(true));
        }
            break;

        case 0xa1: {
            //fcomp
            float val1 = 0.0, val2 = 0.0;
            float diff = 0.0;
            val1 = lhs->cast_data<f32>(true);
            val2 = rhs->cast_data<f32>(true);
            diff = (val1) - (val2);
            if ((isinff(diff)) | (isnanf(diff)))
                flags->carry = true;

            flags->zero = val1 == val2;
            flags->sign = val1 < val2;
            flags->overflow = false; // not quite sure yet
        }
            break;


        case 0xaa: {
            //tabsetex
            std::string prg_name = bmwStringHelper::lowercase(bmwStringHelper::trim(rhs->get_c_string()));
            std::string table_name = bmwStringHelper::uppercase(bmwStringHelper::trim(lhs->get_c_string()));

            if (_external_table_objects.count(prg_name)) {
                _current_context->current_external_table_object = _external_table_objects[prg_name];
                _current_context->tabset_table = _current_context->current_external_table_object->tables[table_name];
                return;
            } else {
                if (_cb_load_object) {
                    auto bestobj = _cb_load_object(prg_name);
                    if (bestobj.has_value()) {
                        _current_context->current_external_table_object = bestobj.value();
                        _external_table_objects[prg_name] = _current_context->current_external_table_object;
                        _current_context->tabset_table = _current_context->current_external_table_object->tables[table_name];
                        BMWLOG().debug("Tabsexex : Loaded external table " + table_name + " from " + prg_name);

                    } else {
                        BMWLOG().error(
                                "Tabsexex : Failed loading external table " + table_name + " from " + prg_name);
                    }
                } else {
                    BMWLOG().error("Tabsexex : No prg callback");
                }

            }
        }
            break;

        case 0xab: {
            //ufix2dez
            int len = rhs->mapped_operand_data.len;
            uint32 value = rhs->get_value();
            std::string s;
            switch (len) {
                case 1:
                    s = std::to_string(((uint8) value));
                    break;
                case 2:
                    s = std::to_string(((uint16) value));
                    break;
                case 4:
                    s = std::to_string(value);
                    break;
                default:
                    BMWLOG().error("modOp_ufix2dez() incorrect len");
            }
            lhs->set_string_value(const_cast<char *>(s.c_str()));
        }
            break;

        case 0xad: {
            //ticks
            auto tick = 0; //TODO : Ticks
            lhs->set_int_value(tick);
        }
            break;

        case 0xae: {
            //waitex
            std::this_thread::sleep_for(std::chrono::milliseconds(lhs->get_value()));
        }
            break;

        case 0xb5: {
            //ssize
            lhs->set_int_value(BMW_BESTVM_STRING_MAXSIZE);
        }
            break;

        case 0xb6: {
            //tabcols
            if (this->_current_context->tabset_table != nullptr)
                lhs->set_int_value(0);
            else {
                lhs->set_int_value(_current_context->tabset_table->table_head.size());
            }
        }
            break;

        case 0xb7: {
            //tabrows
            if (this->_current_context->tabset_table != nullptr)
                lhs->set_int_value(0);
            else {
                lhs->set_int_value(_current_context->tabset_table->table_data.size());
            }
        }
            break;

        default: {
            BMWLOG().error("BESTVM : op not impl:" + std::to_string(opcode));
        }

    }

}

void bmwBESTVirtualMachineReloaded::set_overflow(uint32 val1, uint32 val2, uint32 res, uint32 len) {
    uint64 sign_mask = 0;
    switch (len) {
        case 1:
            sign_mask = 0x00000080;
            break;
        case 2:
            sign_mask = 0x00008000;
            break;
        case 4:
            sign_mask = 0x80000000;
            break;
        default:
            BMWLOG().error("overflow len invalid!", __FUNCTION__);
    }
    if (((val1 & sign_mask)) != (val2 & sign_mask))
        _current_context->flags.overflow = false;
    else
        _current_context->flags.overflow = (val1 & sign_mask) != (res & sign_mask);
}

void bmwBESTVirtualMachineReloaded::set_carry(uint64 Value, uint32 len) {
    uint64 carry_mask = 0;
    switch (len) {
        case 1:
            carry_mask = 0x000000100;
            break;
        case 2:
            carry_mask = 0x000010000;
            break;
        case 4:
            carry_mask = 0x100000000;
            break;
        default:
            BMWLOG().error("Carry len invalid!", __FUNCTION__);
    }
    _current_context->flags.carry = (bool) (((uint64) (Value & carry_mask) != 0));
}

void bmwBESTVirtualMachineReloaded::update_flags(uint32 flag, uint32 len) {
    uint32 valueMask = 0, signMask = 0;
    switch (len) {
        case 1: {
            valueMask = 0x000000FF;
            signMask = 0x00000080;
        }
            break;
        case 2: {
            valueMask = 0x0000FFFF;
            signMask = 0x00008000;
        }
            break;
        case 4: {
            valueMask = 0xFFFFFFFF;
            signMask = 0x80000000;
        }
            break;
        default: {
            BMWLOG().error("bmwBESTVMContextReloaded.update_flags() : T is not 1,2,4 len!", __FUNCTION__);
        }
    }
    _current_context->flags.zero = ((flag & valueMask)) == 0;
    _current_context->flags.sign = ((flag & signMask)) != 0;
}

bool bmwBESTVirtualMachineReloaded::execute() {

    if (!_current_context)
        return false;

    std::cout << "Executing job : " << _current_context->best_job->name << std::endl;
    _current_context->eoj_counter = 0;
    _current_context->eip = 0;
    _current_context->stack.clear();
    _current_context->flags = {0};
    _current_context->trap_index = -1;
    _current_context->trap_mask = 0;
    _current_context->results.clear();
    _current_context->results.resize(1);
    auto start = std::chrono::system_clock::now();
    while (!_current_context->eoj_counter) {
        execute_single_instruction();
    }
    auto end = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    BMWLOG().debug("BESTVM :: Total execution time was " + std::to_string(duration) + " microseconds");
    return true;
}

bool bmwBESTVirtualMachineReloaded::set_job(bmwBESTObject *obj, std::string job) {

    if (_avaliable_contexts.count(obj)) {
        _current_context = _avaliable_contexts[obj];
        BMWLOG().debug("BESTVM :: Switching context to " + obj->filename);
    } else {
        BMWLOG().debug("BESTVM :: Creating new context for " + obj->filename);
        _current_context = new bmwBESTVMContextReloaded();
        _current_context->best_obj = obj;
        _avaliable_contexts[obj] = _current_context;

        //run init job
        _current_context->best_job = _current_context->best_obj->jobs[BMW_BEST_JOB_NAME_INITIALISIERUNG];
        if (_current_context->best_job) {
            execute();
            auto results = get_results();
            if (!results.empty()) {
                if (results.front().TryCompareResult("DONE", "1")) {
                    BMWLOG().debug("Initialized context for " + obj->filename);
                } else {
                    BMWLOG().error("Failed initializing context for" + obj->filename);
                }
            }
        }

    }
    _current_context->best_job = _current_context->best_obj->jobs[job];
    //set wanted results to all avaliable
    _current_context->wanted_results.clear();
    bmwBESTDescriptionEntry *descs = obj->desc_jobs[job];
    for (auto &results : descs->desc_lines) {
        if (results.first == bmwBESTDescriptionEntryType_result) {
            _current_context->wanted_results.push_back(results.second);
        }
    }


    return (_current_context->best_job != nullptr);
}

std::vector<bmwBESTVMJobResultGroup> bmwBESTVirtualMachineReloaded::get_results() {
    return _current_context->results;
}

std::string
bmwBESTVirtualMachineReloaded::dissasemble_instruction(uint8_t opcode, bmwBESTOperand *lhs, bmwBESTOperand *rhs) {
    std::string res = _opcode_name_map[opcode];
    if (lhs->omode != bmwBESTOperandAddrMode_None)
        res += " " + lhs->dissasembly;
    if (rhs->omode != bmwBESTOperandAddrMode_None)
        res += ", " + rhs->dissasembly;
    return res;
}

void bmwBESTVirtualMachineReloaded::set_trace(bmwBESTVMTrace *trace) {
    _current_context->current_trace = trace;
}

std::vector<std::string> *bmwBESTVirtualMachineReloaded::get_parameter_list() {
    return &_current_context->job_params;
}

void bmwBESTVirtualMachineReloaded::set_adapter(bmwAdapterBase *adapter) {
    _adapter = adapter;
}

void bmwBESTVirtualMachineReloaded::set_error(bmwBESTVMErrorCodes error) {
    BMWLOG().log(std::to_string(error), __FUNCTION__);
    if (error != bmwBESTVMErrorCodes::EDIABAS_ERR_NONE) {
        uint32 bitNumber;
        if (_trapbit_map.count(error)) {
            _current_context->trap_index = (int) _trapbit_map[error];
        } else {
            _current_context->trap_index = 0;
        }
    } else {
        _current_context->trap_index = -1;
    }
}

void bmwBESTVirtualMachineReloaded::set_load_bestobj_cb(bmwBESTVMLoadBOCallback cb_load_object) {
    _cb_load_object = cb_load_object;
}


bmwBESTVMContextReloaded::bmwBESTVMContextReloaded() {
    memset(&shared_memory, 0, sizeof(BMWBESTVMStringRegister));
    memset(&registers.rs, 0, sizeof(BMWBESTVMStringRegister) * 16);
    memset(&registers.rl, 0, sizeof(bmwBESTVMStackedRegister) * 8);
    memset(&registers.rf, 0, sizeof(f32) * 8);
    memset(&flags, 0, sizeof(bmwBESTVMFlags));


    results.resize(1); //initial

    //init register map
    _register_map.resize(0x9c);
    _register_map[0] = {"RB0", {&registers.rl[0].rb0, 1}};
    _register_map[1] = {"RB1", {&registers.rl[0].rb1, 1}};
    _register_map[2] = {"RB2", {&registers.rl[0].rb2, 1}};
    _register_map[3] = {"RB3", {&registers.rl[0].rb3, 1}};

    _register_map[4] = {"RB4", {&registers.rl[1].rb0, 1}};
    _register_map[5] = {"RB5", {&registers.rl[1].rb1, 1}};
    _register_map[6] = {"RB6", {&registers.rl[1].rb2, 1}};
    _register_map[7] = {"RB7", {&registers.rl[1].rb3, 1}};

    _register_map[8] = {"RB8", {&registers.rl[2].rb0, 1}};
    _register_map[9] = {"RB9", {&registers.rl[2].rb1, 1}};
    _register_map[0xa] = {"RB10", {&registers.rl[2].rb2, 1}};
    _register_map[0xb] = {"RB11", {&registers.rl[2].rb3, 1}};

    _register_map[0xc] = {"RB12", {&registers.rl[3].rb0, 1}};
    _register_map[0xd] = {"RB13", {&registers.rl[3].rb1, 1}};
    _register_map[0xe] = {"RB14", {&registers.rl[3].rb2, 1}};
    _register_map[0xf] = {"RB15", {&registers.rl[3].rb3, 1}};

    _register_map[0x10] = {"RW0", {&registers.rl[0].rw0, 2}};
    _register_map[0x11] = {"RW1", {&registers.rl[0].rw1, 2}};
    _register_map[0x12] = {"RW2", {&registers.rl[1].rw0, 2}};
    _register_map[0x13] = {"RW3", {&registers.rl[1].rw1, 2}};
    _register_map[0x14] = {"RW4", {&registers.rl[2].rw0, 2}};
    _register_map[0x15] = {"RW5", {&registers.rl[2].rw1, 2}};
    _register_map[0x16] = {"RW6", {&registers.rl[3].rw0, 2}};
    _register_map[0x17] = {"RW7", {&registers.rl[3].rw1, 2}};

    _register_map[0x18] = {"RL0", {&registers.rl[0], 4}};
    _register_map[0x19] = {"RL1", {&registers.rl[1], 4}};
    _register_map[0x1A] = {"RL2", {&registers.rl[2], 4}};
    _register_map[0x1B] = {"RL3", {&registers.rl[3], 4}};

    _register_map[0x1C] = {"RS0", {&registers.rs[0], 0}};
    _register_map[0x1D] = {"RS1", {&registers.rs[1], 0}};
    _register_map[0x1E] = {"RS2", {&registers.rs[2], 0}};
    _register_map[0x1F] = {"RS3", {&registers.rs[3], 0}};
    _register_map[0x20] = {"RS4", {&registers.rs[4], 0}};
    _register_map[0x21] = {"RS5", {&registers.rs[5], 0}};
    _register_map[0x22] = {"RS6", {&registers.rs[6], 0}};
    _register_map[0x23] = {"RS7", {&registers.rs[7], 0}};

    _register_map[0x24] = {"RF0", {&registers.rf[0], 4}};
    _register_map[0x25] = {"RF1", {&registers.rf[1], 4}};
    _register_map[0x26] = {"RF2", {&registers.rf[2], 4}};
    _register_map[0x27] = {"RF3", {&registers.rf[3], 4}};
    _register_map[0x28] = {"RF4", {&registers.rf[4], 4}};
    _register_map[0x29] = {"RF5", {&registers.rf[5], 4}};
    _register_map[0x2A] = {"RF6", {&registers.rf[6], 4}};
    _register_map[0x2B] = {"RF7", {&registers.rf[7], 4}};

    _register_map[0x2C] = {"RS8", {&registers.rs[8], 0}};
    _register_map[0x2D] = {"RS9", {&registers.rs[9], 0}};
    _register_map[0x2E] = {"RS10", {&registers.rs[10], 0}};
    _register_map[0x2F] = {"RS11", {&registers.rs[11], 0}};
    _register_map[0x30] = {"RS12", {&registers.rs[12], 0}};
    _register_map[0x31] = {"RS13", {&registers.rs[13], 0}};
    _register_map[0x32] = {"RS14", {&registers.rs[14], 0}};
    _register_map[0x33] = {"RS15", {&registers.rs[15], 0}};

    _register_map[0x80] = {"RB16", {&registers.rl[4].rb0, 1}};
    _register_map[0x81] = {"RB17", {&registers.rl[4].rb1, 1}};
    _register_map[0x82] = {"RB18", {&registers.rl[4].rb2, 1}};
    _register_map[0x83] = {"RB19", {&registers.rl[4].rb3, 1}};
    _register_map[0x84] = {"RB20", {&registers.rl[5].rb0, 1}};
    _register_map[0x85] = {"RB21", {&registers.rl[5].rb1, 1}};
    _register_map[0x86] = {"RB22", {&registers.rl[5].rb2, 1}};
    _register_map[0x87] = {"RB23", {&registers.rl[5].rb3, 1}};
    _register_map[0x88] = {"RB24", {&registers.rl[6].rb0, 1}};
    _register_map[0x89] = {"RB25", {&registers.rl[6].rb1, 1}};
    _register_map[0x8A] = {"RB26", {&registers.rl[6].rb2, 1}};
    _register_map[0x8B] = {"RB27", {&registers.rl[6].rb3, 1}};
    _register_map[0x8C] = {"RB28", {&registers.rl[7].rb0, 1}};
    _register_map[0x8D] = {"RB29", {&registers.rl[7].rb1, 1}};
    _register_map[0x8E] = {"RB30", {&registers.rl[7].rb2, 1}};
    _register_map[0x8F] = {"RB31", {&registers.rl[7].rb3, 1}};

    _register_map[0x90] = {"RW8", {&registers.rl[4].rw0, 2}};
    _register_map[0x91] = {"RW9", {&registers.rl[4].rw1, 2}};
    _register_map[0x92] = {"RW10", {&registers.rl[5].rw0, 2}};
    _register_map[0x93] = {"RW11", {&registers.rl[5].rw1, 2}};
    _register_map[0x94] = {"RW12", {&registers.rl[6].rw0, 2}};
    _register_map[0x95] = {"RW13", {&registers.rl[6].rw1, 2}};
    _register_map[0x96] = {"RW14", {&registers.rl[7].rw0, 2}};
    _register_map[0x97] = {"RW15", {&registers.rl[7].rw1, 2}};

    _register_map[0x98] = {"RL4", {&registers.rl[4].rl, 4}};
    _register_map[0x99] = {"RL5", {&registers.rl[5].rl, 4}};
    _register_map[0x9A] = {"RL6", {&registers.rl[6].rl, 4}};
    _register_map[0x9B] = {"RL7", {&registers.rl[7].rl, 4}};


}


bmwBESTOperandCategory get_operand_type_category(bmwBESTOperandAddrMode opmode) {
    {
        switch (opmode) {
            case bmwBESTOperandAddrMode_RegS:
            case bmwBESTOperandAddrMode_ImmStr:
            case bmwBESTOperandAddrMode_IdxImm:
            case bmwBESTOperandAddrMode_IdxReg:
            case bmwBESTOperandAddrMode_IdxRegImm:
            case bmwBESTOperandAddrMode_IdxImmLenImm:
            case bmwBESTOperandAddrMode_IdxImmLenReg:
            case bmwBESTOperandAddrMode_IdxRegLenImm:
            case bmwBESTOperandAddrMode_IdxRegLenReg:
                return bmwBESTOperandCategory_VariableDataSize;
            default:
                return bmwBESTOperandCategory_FixedDataSize;
        }
    }
}


bool bmwBESTVMJobResultGroup::TryGetResult(std::string resultname, std::string *out_result_value) {
    for (int32 i = 0; i < size(); i++) {
        if (at(i).first == resultname) {
            *out_result_value = at(i).second;
            return true;
        }
    }
    return false;
}

bool bmwBESTVMJobResultGroup::TryCompareResult(std::string resultname, std::string expected_value) {
    std::string resultvalue;
    if (TryGetResult(resultname, &resultvalue)) {
        return (resultvalue == expected_value);
    }
    return false;
}

std::vector<std::string> bmwBESTVMJobResultGroup::TryGetResultValues(std::vector<std::string> wantedresultnames) {
    std::vector<std::string> res;
    for (std::string s : wantedresultnames) {
        std::string resval;
        if (TryGetResult(s, &resval)) {
            res.push_back(resval);
        }
    }
    return res;
}


bool bmwBESTOperand::is_register() {
    return (this->omode != bmwBESTOperandAddrMode_None) &&
           (this->omode != bmwBESTOperandAddrMode_Imm8) &&
           (this->omode != bmwBESTOperandAddrMode_Imm16) &&
           (this->omode != bmwBESTOperandAddrMode_Imm32) &&
           (this->omode != bmwBESTOperandAddrMode_ImmStr);
}

std::vector<uint8> bmwBESTOperand::get_raw_data() {
    std::vector<uint8> res(mapped_operand_data.len);
    memcpy(res.data(), (uint8 *) absolute_register_data.data + mapped_operand_data.offset, mapped_operand_data.len);
    return res;
}

int32 get_mapping_value(bmwBESTDataMapping mapping) {
    switch (mapping.second.second) {
        case 1:
            return *(int8 *) mapping.second.first;
        case 2:
            return *(int16 *) mapping.second.first;
        case 4:
            return *(int32 *) mapping.second.first;
        default:
            BMWLOG().error("get_mapping_value() : Invalid length");
            return 0;
    }
}

void ensure_mapping_size(bmwBESTDataMapping *mapping, int32 mapping_size) {
    if (mapping->second.second < mapping_size)
        mapping->second.second = mapping_size;
}


void bmwBESTOperand::set_string_value(char *v) {
    //include zero terminator
    set_raw_data(v, strlen(v) + 1);
}

void bmwBESTOperand::set_raw_data(void *buf, uint32_t len) {
    assert(len + mapped_operand_data.offset <= absolute_register_data.max_len);
    memcpy((uint8 *) absolute_register_data.data + mapped_operand_data.offset, buf, len);
    if (*absolute_register_data.len < (mapped_operand_data.offset + len))
        *absolute_register_data.len = (mapped_operand_data.offset + len);
    if (mapped_operand_data.len < len)
        mapped_operand_data.len = len; //only the mapped length, not absolute
    //if this operand is a string register without index, we need to set length in any case
    //otherwise, this would leave old register length active:
    //move rs1, rs2[2:4] (assuming rs1 length is larger than 4)
    if (omode == bmwBESTOperandAddrMode_RegS)
        *absolute_register_data.len = (mapped_operand_data.offset + len);
}

template<typename T>
T bmwBESTOperand::cast_data(bool reinforce_size) {
    assert(sizeof(T) <= absolute_register_data.max_len);
    if (reinforce_size)
        assert(sizeof(T) == mapped_operand_data.len);
    return *(T *) ((uint8 *) absolute_register_data.data + mapped_operand_data.offset);
}

template<typename T>
void bmwBESTOperand::set_value(T value) {
    assert(sizeof(T) <= absolute_register_data.max_len);
    memcpy((uint8 *) absolute_register_data.data + mapped_operand_data.offset, &value, sizeof(value));
}

void bmwBESTOperand::clear() {
    if (is_register()) {
        switch (omode) {
            case bmwBESTOperandAddrMode_RegS: {
                memset(absolute_register_data.data, 0, absolute_register_data.max_len);
                *absolute_register_data.len = 0;
            }
                break;
            case bmwBESTOperandAddrMode_RegAB:
                set_value<uint8>(0);
                break;
            case bmwBESTOperandAddrMode_RegI:
                set_value<uint16>(0);
                break;
            case bmwBESTOperandAddrMode_RegL:
                set_value<int32>(0);
                break;
            default: {
                BMWLOG().error("bmwBESTOperand::clear() :: Unsuppored omode");
            }
        }
    } else {
        BMWLOG().error("bmwBESTOperand::clear() :: clear called on non register");
    }
}

int32 bmwBESTOperand::get_value(int read_len) {
    if (!read_len) {
        read_len = mapped_operand_data.len;
    }
    switch (read_len) {
        case 1:
            return cast_data<uint8>();
        case 2:
            return cast_data<uint16>();
        case 4:
            return cast_data<int32>();
        default:
            BMWLOG().error("bmwBESTOperand::get_value() : Invalid length");
            return 0;
    }
}

void bmwBESTOperand::set_int_value(int32_t value) {
    switch (mapped_operand_data.len) {
        case 1:
            set_value<uint8>(value);
            break;
        case 2:
            set_value<uint16>(value);
            break;
        case 4:
            set_value<int32>(value);
            break;
        default:
            BMWLOG().error("bmwBESTOperand::set_int_value() : Invalid length");
    }
}

char *bmwBESTOperand::get_c_string() {
    if ((omode == bmwBESTOperandAddrMode_ImmStr) || (omode == bmwBESTOperandAddrMode_RegS))
        return (char *) ((uint8 *) absolute_register_data.data + mapped_operand_data.offset);
    else
        BMWLOG().error("bmwBESTOperand::get_c_string() :: Not Str register");
    return nullptr;
}
