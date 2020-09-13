//
// Created by lpcvoid on 2020-02-06.
//

#ifndef BESTVMTEST_BMWBESTTABLE_H
#define BESTVMTEST_BMWBESTTABLE_H

#include <string>
#include <vector>
#include "../common/bmwTypes.h"

#define BMW_TABLE_NOT_FOUND -1

class bmwBESTTable {
public:

    std::string table_name;
    std::vector<std::string> table_head;
    std::vector<std::vector<std::string>> table_data; //row[col]

    //Get row index by looking into column with name colname, and searching for the row within that column with content rowcontent
    bool GetColumnRow_ColName_RowStr(std::string colname, std::string rowcontent, int32* row_index);
    //Get row index by looking into column with index, and searching for the row within that column with content rowcontent
    bool GetColumnRow_ColIndx_RowStr(int32 colname, std::string rowcontent, int32* row_index);
    //Get row index by looking into column with name colname, and searching for the row within that column with content rowcontent
    bool GetColumnRow_ColName_ColValue(std::string colname, std::string colvalue, int32* row_index);

    bool TryGetDataCell(int32 row, int32 col, std::string* out_contents);
    //returns index of column, or (BMW_TABLE_NOT_FOUND) if not found
    int32 GetColumnIndex(std::string name);
    uint32 GetRowCount();
};

#endif //BESTVMTEST_BMWBESTTABLE_H
