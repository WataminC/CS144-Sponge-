#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // When writing stream length is bigger than the stream capacity
    if (eof == true) {
        end_flag = eof;
        total_write = index + data.size();
    }

    // Stream should ignore the empty string
    if (data.empty()) {
        if (end_flag && reassembler.size() == 0) {
            _output.end_input();
        }
        return ;
    }

    string copy_data = data;
    size_t copy_index = index;

    // Avoid overlapping
    if (index < willing && index + copy_data.size() - 1 >=index) {
        copy_data.erase(0, willing - index);
        copy_index = willing;
    }

    size_t remain_size = _capacity - (_output.buffer_size() + reassembler.size());

    if (copy_data.size() > remain_size) {   // check does the data fix the capacity
        copy_data = copy_data.substr(0, remain_size);
        if (copy_data.empty()) {
            return ;
        }
    }

    reassembler[copy_index] = std::move(copy_data);

    for (;;) {
        size_t temp_will = willing;
        
        for (auto element = reassembler.begin(); element != reassembler.end(); /* no increment here */) {
            if (element->first > willing) {
                ++element;  // Increment if not erasing
                continue;
            }

            size_t element_index = element->first;
            std::string element_str = element->second;

            // Erase the element and update the iterator
            element = reassembler.erase(element);

            if (element_index + element_str.size() - 1 < willing) {
                continue;
            }

            // Get the substring from the element
            std::string temp_str = element_str.substr(willing - element_index);
            
            // Update the willing value and write to output
            willing += temp_str.size();
            bytes_writed += _output.write(temp_str);
        }

        if (temp_will == willing) {
            break;
        }
    }


    if (end_flag && bytes_writed == total_write) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t size = 0;
    std::unordered_map<size_t, bool> pos;
    for (auto &item : reassembler) {
        for (size_t index = 0; index < item.second.size(); ++index) {
            if (!pos[index + item.first]) {
                size += 1;
                pos[index + item.first] = true;
            }
        }
    }
    return size;
}

bool StreamReassembler::empty() const {
    // return _output.buffer_size() + reassembler.size() == 0;
    return reassembler.size() == 0;
}
