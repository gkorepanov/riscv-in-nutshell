#include "infra/config/config.hpp"
#include "infra/common/common.h"

class LRUInfo {
private:
    std::vector<std::list<uint>> lru;

public:
    LRUInfo(size_t ways, size_t sets);
    void touch(uint set, uint way);
    uint get_LRU_way(uint set);
};

class Cache {
public:
    struct RequestResult {
        bool is_ready = false;
        uint32 data = NO_VAL32;
    };

private:
    struct Request {
        bool complete = true;
        bool is_read = false;
        bool awaiting_memory_request = false;
        Addr addr = NO_VAL32;
        uint32 data = NO_VAL32;
        size_t num_bytes = NO_VAL32;
    };

    struct Line {
        std::vector<uint8> data;
        Addr tag = NO_VAL32;
        bool is_valid = false;
        bool is_dirty = false;
    }

    std::vector<  // way
    std::vector<  // set
        Line
    >> array;

    std::pair<bool, Line&> lookup(Addr addr) {
        const auto set = get_set(addr);
        const auto tag = get_tag(Addr);

        for (uint way = 0; way < array->size(); ++way) {
            if (array[way][set].tag == tag) {
                lru_info.touch(set, way);
                return {true, array[way][set]};
            }
        }
        return {false, Line()};
    }

    LRUInfo lru_info;
    Request request;
    
    void process();

    uint get_set(Addr addr) const {
        return (addr / line_size) & (num_sets - 1);
    }

    Addr get_tag(Addr addr) const {
        return addr / line_size;
    }

public:
    Cache(Memory& memory, size_t num_ways, size_t num_sets, uint line_size_in_bytes = 32);
    void clock();
    bool is_busy() { return !request.complete; }
    void send_read_request(Addr addr, size_t num_bytes);
    void send_write_request(uint32 value, Addr addr, size_t num_bytes);
    RequestResult get_request_status();
};
