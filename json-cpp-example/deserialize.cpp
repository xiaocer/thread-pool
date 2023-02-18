// 解铃还须系铃人

#include <iostream>
#include <json/json.h>
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
int main() {
    ifstream ifs("data.json");
    Json::Reader reader;
    Json::Value root;
    reader.parse(ifs, root);

    for (int i = 0; i < root.size(); ++i) {
        // 取出数组中的各个元素，类型是Value
        Json::Value item = root[i];
        // 得到所有的key
        Json::Value::Members keys = item.getMemberNames();
        for (int j = 0; j < keys.size(); ++j) {
            if (item[keys[j]].isArray()) {
                // 取出数组中的各个元素，类型是Value
                for (int k = 0; k < item[keys[j]].size(); ++k) {
                    Json::Value temp = item[keys[j]][k];
                    Json::Value::Members keys = temp.getMemberNames();
                    for (int z = 0; z < keys.size(); ++z) {
                        cout << keys.at(z) << ":" << temp[keys[z]] << ",";
                    }
                    cout << endl;
                }
            } else {
                cout << keys.at(j) << ":" << item[keys[j]] << ",";
            }
            
        }
        cout << endl;
    }
    return 0;
}