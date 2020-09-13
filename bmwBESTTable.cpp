//
// Created by lpcvoid on 2020-02-06.
//

#include "bmwBESTTable.h"
#include "../common/bmwStringHelper.hpp"

bool bmwBESTTable::TryGetDataCell(int32 row, int32 col, std::string *out_contents) {
    if ((row >= 0) && (col >= 0)){
        if (row >= table_data.size())
            return false;
        if (col >= table_data[row].size())
            return false;

        *out_contents = table_data[row][col];
        return true;
    }
    return false;
}

uint32 bmwBESTTable::GetRowCount() {
    return table_data.size();
}

int32 bmwBESTTable::GetColumnIndex(std::string name) {
    //trim string
    name = bmwStringHelper::trim(name);
    for (int i = 0; i < table_head.size(); i++){
        if (bmwStringHelper::uppercase(table_head[i]) == bmwStringHelper::uppercase(name))
            return i;
    }
    return BMW_TABLE_NOT_FOUND;
}

bool bmwBESTTable::GetColumnRow_ColName_RowStr(std::string colname, std::string rowcontent, int32 *row_index) {
    *row_index = BMW_TABLE_NOT_FOUND;
    colname = bmwStringHelper::uppercase(colname);
    int32 col_index = GetColumnIndex(colname);
    if (col_index != BMW_TABLE_NOT_FOUND){
        return GetColumnRow_ColIndx_RowStr(col_index, rowcontent, row_index);
    }

    return false;
}

bool bmwBESTTable::GetColumnRow_ColIndx_RowStr(int32 colname, std::string rowcontent, int32 *row_index) {
    *row_index = BMW_TABLE_NOT_FOUND;

    for (int i = 0; i < GetRowCount(); i++){
        std::string entry;
        if (TryGetDataCell(i,colname,&entry)){
            entry = bmwStringHelper::uppercase(entry);
            if (bmwStringHelper::is_numeric_string(rowcontent) && bmwStringHelper::is_numeric_string(entry)){
                int32 i_rc = bmwStringHelper::to_integer<int32>(rowcontent);
                int32 i_entry = bmwStringHelper::to_integer<int32>(entry);
                if (i_rc == i_entry)
                {
                    *row_index = i;
                    return true;
                }
            } else {
                //not a hex number or number
                rowcontent = bmwStringHelper::uppercase(rowcontent);
                if (rowcontent == entry)
                {
                    *row_index = i;
                    return true;
                }
            }

        } else {
            *row_index = (int32)(GetRowCount() -1); //return last row, for whatever reason
            return false;
        }

    }
    *row_index = (int32)(GetRowCount() -1);//return last row, for whatever reason
    return false;
}

bool bmwBESTTable::GetColumnRow_ColName_ColValue(std::string colname, std::string colvalue, int32 *row_index) {
    return false;
}
