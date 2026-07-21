#pragma once

#include <string>
#include <map>
#include <vector>

enum class JSON_ValueType
{
    Integer,
    String,
    Array,
    Set
};

struct JSON_Value
{
    ~JSON_Value();

    JSON_ValueType type;
    union
    {
        int integer;
        std::string* str;
        struct JSON_Array* array;
        struct JSON_Set* set;
    };
};

struct JSON_Array
{
    ~JSON_Array();

    std::vector<JSON_Value*> m_values;
};

struct JSON_Set
{
    ~JSON_Set();

    std::map<std::string, JSON_Value*> m_items;
};

class JSON
{
public:
    void Parse(u8* mem, int size);
    std::string FindString(const std::string& field);

    JSON_Set* m_data;

protected:
    JSON_Value* ParseValue(u8* mem, int size, int& idx);
    JSON_Array* ParseArray(u8* mem, int size, int& idx);
    JSON_Set* ParseSet(u8* mem, int size, int& idx);
};
