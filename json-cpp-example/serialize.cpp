#include <json/json.h>
#include <iostream>
#include <string>
#include <fstream>
using namespace std;

// [
//     {
//         "address": "浙江省杭州市",
//         "school": "家里蹲大学",
//         "personInfo" : [{
//             "gender": "男",
//             "age": 23,
//             "name": "张三"
//         }]
//     }
// ]

int main()
{
	Json::Value root, subObj, subsubArr, subsubObj;

    // 对象类型
    subsubObj["gender"] = "男";
    subsubObj["age"] = 23;
    subsubObj["name"] = "张三";

    // 数组添加对象
    subsubArr.append(subsubObj);

    // 对象类型
    subObj["address"] = "浙江省杭州市";
    subObj["school"] = "家里蹲大学";
    subObj["personInfo"] = Json::Value(subsubArr);

    // 数组添加对象
    root.append(subObj);
    
    // 带换行
    string str = root.toStyledString();
	
    // 一行显示
    // Json::FastWriter writer;
    // cout << writer.write(root) << endl;
    
    // 将序列化后的字符串保存到文件
    ofstream ostream("data.json");
    ostream << str;
    return 0;
}
