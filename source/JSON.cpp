#include "common.h"
#include "JSON.h"

struct JSON_Token
{
    bool inQuotes = false;
    std::string value;
};


JSON_Token GrabToken(u8* mem, int size, int& idx)
{
    JSON_Token result;

    // skip whitespace
    while (idx < size && mem[idx] == ' ' || mem[idx] == '\t' || mem[idx] == '\n' || mem[idx] == '\r')
        idx++;

    if (mem[idx] == '{' || mem[idx] == '}' || mem[idx] == '[' || mem[idx] == ']' || mem[idx] == ':' || mem[idx] == ',')
    {
        result.value.push_back(mem[idx++]);
    }
    else
    {
        if (mem[idx] == '\"')
        {
            result.inQuotes = true;
            idx++;
            while (idx < size && mem[idx] != '\"')
            {
                result.value.push_back(mem[idx++]);
            }
            idx++;
        }
        else
        {
            while (idx < size && ((mem[idx] >= '0' && mem[idx] <= '0') || (mem[idx] >= 'a' && mem[idx] <= 'z')))
                result.value.push_back(mem[idx++]);
        }
    }
    return result;
}

JSON_Token PeekToken(u8* mem, int size, int idx)
{
    return GrabToken(mem, size, idx);
}

bool PopToken(u8* mem, int size, int &idx, const std::string& expect)
{
    JSON_Token token = GrabToken(mem, size, idx);
    Assert((token.inQuotes == false) && (token.value == expect), "Unexpected {}, expected {}", token.value, expect);
    return (token.inQuotes == false) && (token.value == expect);
}


JSON_Value::~JSON_Value()
{
    if (type == JSON_ValueType::String)
    {
        delete str;
    }
    else if (type == JSON_ValueType::Array)
    {
        delete array;
    }
    else if (type == JSON_ValueType::Set)
    {
        delete set;
    }
}


JSON_Array::~JSON_Array()
{
    for (auto value : m_values)
        delete value;
}

JSON_Set::~JSON_Set()
{
    for (auto it : m_items)
    {
        delete it.second;
    }
}

JSON_Value* JSON::ParseValue(u8* mem, int size, int& idx)
{
    JSON_Value* value = new JSON_Value;
    JSON_Token token = PeekToken(mem, size, idx);
    if (token.value == "{" && token.inQuotes == false)
    {
        value->type = JSON_ValueType::Set;
        value->set = ParseSet(mem, size, idx);
        if (value->set == nullptr)
        {
            delete value;
            return nullptr;
        }
    }
    else if (token.value == "[" && token.inQuotes == false)
    {
        value->type = JSON_ValueType::Array;
        value->array = ParseArray(mem, size, idx);
        if (value->array == nullptr)
        {
            delete value;
            return nullptr;
        }
    }
    else
    {
        token = GrabToken(mem, size, idx);
        if (token.inQuotes)
        {
            value->type = JSON_ValueType::String;
            value->str = new std::string(token.value);
        }
        else
        {
            value->type = JSON_ValueType::Integer;
            if (token.value == "true")
                value->integer = 1;
            else if (token.value == "false")
                value->integer = 0;
            else
                value->integer = std::stoi(token.value);
        }
    }
    return value;
}

JSON_Array* JSON::ParseArray(u8* mem, int size, int& idx)
{
    if (!PopToken(mem, size, idx, "["))
        return nullptr;

    JSON_Array* array = new JSON_Array;
    while (true)
    {
        JSON_Token token = PeekToken(mem, size, idx);
        if (token.value == "]")
        {
            PopToken(mem, size, idx, "]");
            return array;
        }
        else
        {
            JSON_Value* value = ParseValue(mem, size, idx);
            if (value == nullptr)
            {
                delete array;
                return nullptr;
            }
            array->m_values.push_back(value);
        }
    }
}

JSON_Set* JSON::ParseSet(u8* mem, int size, int& idx)
{
    if (!PopToken(mem, size, idx, "{"))
        return nullptr;

    JSON_Set* set = new JSON_Set;
    while (true)
    {
        JSON_Token field = GrabToken(mem, size, idx);
        if (!PopToken(mem, size, idx, ":"))
        {
            delete set;
            return nullptr;
        }

        JSON_Value* value = ParseValue(mem, size, idx);
        if (value == nullptr)
        {
            delete set;
            return nullptr;
        }

        set->m_items[field.value] = value;

        JSON_Token next = GrabToken(mem, size, idx);
        if (next.value == "}")
            break;
        Assert(next.value == ",", "Expected ',', but got {}", next.value);
    }
    return set;
}

void JSON::Parse(u8* mem, int size)
{
    int idx = 0;
    m_data = ParseSet(mem, size, idx);
}

std::string JSON::FindString(const std::string& field)
{
    if (m_data)
    {
        for (auto item : m_data->m_items)
        {
            if (item.first == field)
            {
                if (item.second->type == JSON_ValueType::String)
                {
                    return *item.second->str;
                }
                return "";
            }
        }
    }
    return "";
}


