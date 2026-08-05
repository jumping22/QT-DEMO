/**
 * 无 Qt 依赖的协议逻辑校验（验证测试用例反序列化结果）
 * 编译: g++ -o verify_protocol tests/verify_protocol.cpp && ./verify_protocol
 */
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

static std::string stripEndMark(std::string s)
{
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\n' || s.back() == '\r')) {
        s.pop_back();
    }
    while (!s.empty() && s.back() == '/') {
        s.pop_back();
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
            s.pop_back();
        }
    }
    return s;
}

static bool deserialize(const unsigned char *data, int length,
                        std::vector<std::string> &keys,
                        std::map<std::string, std::string> &fields)
{
    keys.clear();
    fields.clear();

    std::string raw(reinterpret_cast<const char *>(data), length);
    raw = stripEndMark(raw);
    if (raw.empty()) {
        return true;
    }

    size_t start = 0;
    while (start <= raw.size()) {
        size_t end = raw.find(';', start);
        if (end == std::string::npos) {
            end = raw.size();
        }
        std::string part = raw.substr(start, end - start);
        // trim left/right spaces
        while (!part.empty() && part[0] == ' ') {
            part.erase(part.begin());
        }
        while (!part.empty() && part.back() == ' ') {
            part.pop_back();
        }

        if (!part.empty()) {
            size_t colon = part.find(':');
            if (colon == std::string::npos || colon == 0) {
                return false;
            }
            std::string key = part.substr(0, colon);
            std::string value = part.substr(colon + 1);
            while (!key.empty() && key.back() == ' ') {
                key.pop_back();
            }
            if (fields.find(key) == fields.end()) {
                keys.push_back(key);
            }
            fields[key] = value;
        }

        if (end == raw.size()) {
            break;
        }
        start = end + 1;
    }
    return true;
}

int main()
{
    static const unsigned char kTestPayload[] = {
        0x64, 0x61, 0x74, 0x61, 0x54, 0x79, 0x70, 0x65, 0x3A, 0x30, 0x3B,
        0x6E, 0x61, 0x6D, 0x65, 0x3A, 0xE5, 0xBC, 0xA0, 0xE6, 0x98, 0x8E,
        0xE5, 0x8D, 0x8E, 0x3B, 0x49, 0x64, 0x3A, 0x50, 0x32, 0x30, 0x32,
        0x36, 0x30, 0x37, 0x30, 0x30, 0x31, 0x3B, 0x65, 0x78, 0x61, 0x6D,
        0x3A, 0xE8, 0x83, 0xB8, 0xE9, 0x83, 0xA8, 0x43, 0x54, 0x41, 0x3B,
        0x77, 0x65, 0x69, 0x67, 0x68, 0x74, 0x3A, 0x37, 0x30
    };

    std::vector<std::string> keys;
    std::map<std::string, std::string> fields;
    if (!deserialize(kTestPayload, static_cast<int>(sizeof(kTestPayload)), keys, fields)) {
        std::printf("FAIL: deserialize\n");
        return 1;
    }

    const char *expectKeys[] = {"dataType", "name", "Id", "exam", "weight"};
    const char *expectVals[] = {"0", "\xE5\xBC\xA0\xE6\x98\x8E\xE5\x8D\x8E",
                                "P202607001",
                                "\xE8\x83\xB8\xE9\x83\xA8" "CTA", "70"};

    if (keys.size() != 5) {
        std::printf("FAIL: key count %zu\n", keys.size());
        return 1;
    }

    for (size_t i = 0; i < 5; ++i) {
        if (keys[i] != expectKeys[i] || fields[keys[i]] != expectVals[i]) {
            std::printf("FAIL at %zu: %s=%s\n", i, keys[i].c_str(), fields[keys[i]].c_str());
            return 1;
        }
        std::printf("%s = %s\n", keys[i].c_str(), fields[keys[i]].c_str());
    }

    std::printf("PASS\n");
    return 0;
}
