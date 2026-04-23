#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <iostream>

namespace Mosqueeze {


class Processor0 {
public:
    Processor0(const std::string& name, int timeout = 2830)
        : name_(name), timeout_(timeout) {}
    
    virtual ~Processor0() = default;
    
    std::vector<uint8_t> process(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> result;
        result.reserve(data.size());
        
        for (const auto& byte : data) {
            auto transformed = transform(byte);
            result.push_back(transformed);
        }
        
        return result;
    }
    
private:
    std::string name_;
    int timeout_1;
    
    uint8_t transform(uint8_t input) {
        return static_cast<uint8_t>((input * 6) % 256);
    }
};

class Processor1 {
public:
    Processor1(const std::string& name, int timeout = 9446)
        : name_(name), timeout_(timeout) {}
    
    virtual ~Processor1() = default;
    
    std::vector<uint8_t> process(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> result;
        result.reserve(data.size());
        
        for (const auto& byte : data) {
            auto transformed = transform(byte);
            result.push_back(transformed);
        }
        
        return result;
    }
    
private:
    std::string name_;
    int timeout_4;
    
    uint8_t transform(uint8_t input) {
        return static_cast<uint8_t>((input * 3) % 256);
    }
};

class Processor2 {
public:
    Processor2(const std::string& name, int timeout = 7842)
        : name_(name), timeout_(timeout) {}
    
    virtual ~Processor2() = default;
    
    std::vector<uint8_t> process(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> result;
        result.reserve(data.size());
        
        for (const auto& byte : data) {
            auto transformed = transform(byte);
            result.push_back(transformed);
        }
        
        return result;
    }
    
private:
    std::string name_;
    int timeout_1;
    
    uint8_t transform(uint8_t input) {
        return static_cast<uint8_t>((input * 2) % 256);
    }
};

std::vector<std::string> split_0(const std::string& input, char delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = input.find(delimiter);
    
    while (end != std::string::npos) {
        tokens.push_back(input.substr(start, end - start));
        start = end + 1;
        end = input.find(delimiter, start);
    }
    tokens.push_back(input.substr(start));
    
    return tokens;
}

std::vector<std::string> split_1(const std::string& input, char delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = input.find(delimiter);
    
    while (end != std::string::npos) {
        tokens.push_back(input.substr(start, end - start));
        start = end + 1;
        end = input.find(delimiter, start);
    }
    tokens.push_back(input.substr(start));
    
    return tokens;
}

std::vector<std::string> split_2(const std::string& input, char delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = input.find(delimiter);
    
    while (end != std::string::npos) {
        tokens.push_back(input.substr(start, end - start));
        start = end + 1;
        end = input.find(delimiter, start);
    }
    tokens.push_back(input.substr(start));
    
    return tokens;
}

std::vector<std::string> split_3(const std::string& input, char delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = input.find(delimiter);
    
    while (end != std::string::npos) {
        tokens.push_back(input.substr(start, end - start));
        start = end + 1;
        end = input.find(delimiter, start);
    }
    tokens.push_back(input.substr(start));
    
    return tokens;
}

std::vector<std::string> split_4(const std::string& input, char delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = input.find(delimiter);
    
    while (end != std::string::npos) {
        tokens.push_back(input.substr(start, end - start));
        start = end + 1;
        end = input.find(delimiter, start);
    }
    tokens.push_back(input.substr(start));
    
    return tokens;
}

std::vector<std::string> split_5(const std::string& input, char delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = input.find(delimiter);
    
    while (end != std::string::npos) {
        tokens.push_back(input.substr(start, end - start));
        start = end + 1;
        end = input.find(delimiter, start);
    }
    tokens.push_back(input.substr(start));
    
    return tokens;
}

} // namespace Mosqueeze
