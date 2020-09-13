//
// Created by lpcvoid on 2020-05-07.
//

#ifndef BESTVMTEST_BMWBESTVIRTUALMACHINERELOADED_H
#define BESTVMTEST_BMWBESTVIRTUALMACHINERELOADED_H


#include <vector>
#include <functional>
#include "../common/bmwTypes.h"
#include "bmwBESTObject.h"
#include "bmwBESTVMTrace.hpp"
#include "../coms/bmwAdapterBase.hpp"
#include "bmwBESTVMErrorCodes.h"


const int BMW_BESTVM_STRING_MAXSIZE = 1024;

typedef std::function<std::optional<bmwBESTObject*>(std::string)> bmwBESTVMLoadBOCallback;

struct bmwBESTVMStackedRegister {
    union {
        uint32 rl;
        struct {
            uint16 rw0;
            uint16 rw1;
        };
        struct {
            uint8 rb0;
            uint8 rb1;
            uint8 rb2;
            uint8 rb3;
        };
    };
};

typedef uint8 BMWBESTVMStringRegister[BMW_BESTVM_STRING_MAXSIZE];

struct bmwBESTVMStackedRegisters {
    bmwBESTVMStackedRegister rl[8];
    BMWBESTVMStringRegister rs[16];
    f32 rf[8];
};

enum bmwBESTOperandCategory {bmwBESTOperandCategory_FixedDataSize,
    bmwBESTOperandCategory_VariableDataSize };

enum bmwBESTOperandAddrMode {
    bmwBESTOperandAddrMode_None = 0,
    bmwBESTOperandAddrMode_RegS = 1,
    bmwBESTOperandAddrMode_RegAB = 2,
    bmwBESTOperandAddrMode_RegI = 3,
    bmwBESTOperandAddrMode_RegL = 4,
    bmwBESTOperandAddrMode_Imm8 = 5,
    bmwBESTOperandAddrMode_Imm16 = 6,
    bmwBESTOperandAddrMode_Imm32 = 7,
    bmwBESTOperandAddrMode_ImmStr = 8,
    bmwBESTOperandAddrMode_IdxImm = 9,
    bmwBESTOperandAddrMode_IdxReg = 10,
    bmwBESTOperandAddrMode_IdxRegImm = 11,
    bmwBESTOperandAddrMode_IdxImmLenImm = 12,
    bmwBESTOperandAddrMode_IdxImmLenReg = 13,
    bmwBESTOperandAddrMode_IdxRegLenImm = 14,
    bmwBESTOperandAddrMode_IdxRegLenReg = 15 };

bmwBESTOperandCategory  get_operand_type_category(bmwBESTOperandAddrMode opmode);

struct bmwBESTDataMappingReference {
    void* first;
};


//addressmode, <memory location, length>
//can point to a register, or to a buffer for immidiate values
//typedef std::pair<bmwBESTOperandAddrMode, std::pair<void*, uint32>> bmwBESTOperand;
typedef std::pair<std::string, std::pair<void*, int32>> bmwBESTDataMapping;
void ensure_mapping_size(bmwBESTDataMapping* mapping, int32 mapping_size);
inline int32 get_mapping_value(bmwBESTDataMapping mapping);

struct bmwBESTOperand {
    bmwBESTOperandAddrMode omode;
    //only if operand has an immediate value
    struct {
        uint8 data[BMW_BESTVM_STRING_MAXSIZE];
        int32 len = 0;
    } immediate_data;
    //contains offset data matched to the specific addressing mode
    //example : RS3[2:5] (IdxImmLenImm)
    //absolute_register_data = {*rs3[0], current_rs3_size, 1024}
    //mapped_operand_data = {2, 5}
    //example : RS3[2] (IdxImm)
    //absolute_register_data = {*rs3[0], current_rs3_size, 1024}
    //mapped_operand_data = {1, 1024-1=1023, 1024}
    struct {
        int32 offset;
        int32 len;
    } mapped_operand_data;

    //contains base address of register, and pointer to the real length
    struct {
        void* data;
        int32* len;
        int32 max_len;
    } absolute_register_data;

    std::string dissasembly;

    void set_string_value(char* v);
    void set_raw_data(void* buf, uint32 len);

    template <typename T>
    T cast_data(bool reinforce_size = false);
    template <typename T>
    void set_value(T value);
    void set_int_value(int32 value);
    int32 get_value(int read_len = 0);
    char* get_c_string();
    bool is_register();
    void clear();
    std::vector<uint8> get_raw_data();
};
struct bmwBESTVMFlags {
    bool zero; // true if compare is equal
    bool carry;
    bool sign;
    bool overflow;
};

typedef std::pair<std::string, std::string> bmwBESTVMJobResult;

struct bmwBESTVMJobResultGroup : public std::vector<bmwBESTVMJobResult>{
    bool TryGetResult(std::string resultname, std::string* out_result_value);
    bool TryCompareResult(std::string resultname, std::string expected_value);
    std::vector<std::string> TryGetResultValues(std::vector<std::string> wantedresultnames);
};

struct bmwBESTVMContextReloaded {
private:
    std::vector<std::pair<std::string, std::pair<void*, int32>>> _register_map;
public:
    bmwBESTVMStackedRegisters registers;
    std::vector<uint8> stack;
    BMWBESTVMStringRegister shared_memory;
    std::string shared_memory_name;
    bmwBESTObject* best_obj = nullptr;
    bmwBESTJob* best_job = nullptr;
    uint32 eip = 0;
    bmwBESTVMFlags flags;
    bmwBESTOperand op_lhs;
    bmwBESTOperand op_rhs;
    uint32 eoj_counter = 0;
    std::vector<bmwBESTVMJobResultGroup> results;
    uint32 trap_mask = 0;
    int32 trap_index = -1;
    std::string token_seperator;
    int32 token_index = 0;
    std::vector<std::string> job_params;
    bmwBESTTable* tabset_table = nullptr;
    std::string tabseek_colname;
    std::string tabseek_valname;
    int32 tabseek_row_index = 0; // just row index
    bmwBESTObject* current_external_table_object = nullptr;
    bmwBESTVMTrace* current_trace = nullptr;
    //last xsend request payload that we needed to cache responses from, see xsend for explain
    bmwMessageRaw cached_xsend_request_payload;
    //cached response messages used for xsend, check there for documentation
    std::queue<bmwMessageRaw> cached_xsend_responses;
    //this is needed for etag instruction
    std::vector<std::string> wanted_results;
    bool get_register_memory(uint8 register_code, bmwBESTDataMapping** out_mapping);
    bmwBESTVMContextReloaded();
};

class bmwBESTVirtualMachineReloaded {
private:
    std::map<uint8, std::string> _opcode_name_map;
    std::map<bmwBESTVMErrorCodes, uint32> _trapbit_map;
    bmwBESTVMContextReloaded* _current_context = nullptr;
    std::map<bmwBESTObject*, bmwBESTVMContextReloaded*> _avaliable_contexts;
    bmwAdapterBase* _adapter = nullptr;
    //methods
    inline bool read_code(void *dest, uint32 len);
    inline bool read_operand(bmwBESTOperandAddrMode otype, bmwBESTOperand* operand, bool lhs);
    void update_flags(uint32 flag, uint32 len);
    void set_overflow(uint32 val1, uint32 val2, uint32 res, uint32 len);
    void set_carry(uint64 Value, uint32 len);
    void set_error(bmwBESTVMErrorCodes error);
    std::map<std::string, bmwBESTObject*> _external_table_objects;
    bmwBESTVMLoadBOCallback _cb_load_object = nullptr;
    std::string dissasemble_instruction(uint8 opcode, bmwBESTOperand* lhs, bmwBESTOperand* rhs);
public:
    bmwBESTVirtualMachineReloaded();
    bool set_job(bmwBESTObject* obj, std::string job);
    inline void execute_single_instruction();
    bool execute();
    void set_trace(bmwBESTVMTrace* trace);
    std::vector<bmwBESTVMJobResultGroup> get_results();
    std::vector<std::string>* get_parameter_list();
    void set_adapter(bmwAdapterBase* adapter);
    void set_load_bestobj_cb(bmwBESTVMLoadBOCallback cb_load_object);
};


#endif //BESTVMTEST_BMWBESTVIRTUALMACHINERELOADED_H
