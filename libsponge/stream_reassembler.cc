#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

// void StreamReassembler::push_stream(const string &data) {
//     willing += data.size();
//     _output.write(data);
//     size +=

//     while(_) {

//     }
// }

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    string copy_data = data;
    size_t copy_index = index;

    if (index < willing && index + copy_data.size() - 1 >=index) {
        // copy_data = copy_data.substr(willing - index);
        copy_data.erase(0, willing - index);
        copy_index = willing;
    }

    if (_output.buffer_size() + reassembler.size() + copy_data.size() > _capacity) {   // check does the data fix the capacity
        return ;
    }

    // size += data.size();
    reassembler[copy_index] = std::move(copy_data);

    end_flag = eof;

    // Writing assembled block to the stream
    // for (;;) {
    //     // auto it = reassembler.find(willing);
    //     // if (it == reassembler.end()) {
    //     //     break;
    //     // }
    //     // std::string temp_str = std::move(it->second);
    //     // reassembler.erase(it);

    //     // willing += temp_str.size();
    //     // _output.write(temp_str);
    //     size_t temp_will = willing;
    //     for (auto element = reassembler.begin(); element != reassembler.end(); element++) {
    //         if (element->first > willing || element->first + element->second.size() - 1 < willing) {
    //             continue;
    //         }

    //         std::string temp_str = std::move(element->second.substr(willing - element->first));
    //         reassembler.erase(element);

    //         willing += temp_str.size();
    //         _output.write(temp_str);
    //     }

    //     if (temp_will == willing) {
    //         break;
    //     }
    // }

    for (;;) {
        size_t temp_will = willing;
        
        for (auto element = reassembler.begin(); element != reassembler.end(); /* no increment here */) {
            if (element->first > willing || element->first + element->second.size() - 1 < willing) {
                ++element;  // Increment if not erasing
                continue;
            }

            // Get the substring from the element
            std::string temp_str = element->second.substr(willing - element->first);
            
            // Erase the element and update the iterator
            element = reassembler.erase(element);

            // Update the willing value and write to output
            willing += temp_str.size();
            _output.write(temp_str);
        }

        if (temp_will == willing) {
            break;
        }
    }


    if (end_flag && reassembler.size() == 0) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    return reassembler.size();
}

bool StreamReassembler::empty() const {
    return reassembler.size() == 0;
}
