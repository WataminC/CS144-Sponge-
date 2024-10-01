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
    string copy_data = data;
    size_t copy_index = index;

    // When writing snippet length is bigger than the stream capacity
    if (eof == true) {
        end_flag = eof;
        total_write = index + data.size();
    }

    // Stream should ignore the empty string
    if (data.empty()) {
        // If it's the last segment
        if (end_flag && bytes_writed == total_write) {
            _output.end_input();
        }
        return ;
    }
    
    // Avoid overlapping: remove the segment and the stream that has been reassembled's overlapping part
    if (copy_index < willing && copy_index + copy_data.size() - 1 >= willing) {
        copy_data.erase(0, willing - index);
        copy_index = willing;
    }

    size_t total_reassembler_size = 0;
    for (const auto &pair : reassembler) {
        total_reassembler_size += pair.second.size();
    }
    size_t remain_size = _capacity - (_output.buffer_size() + total_reassembler_size);

    if (copy_data.size() > remain_size) {   // check does the data fix the capacity
        size_t size_diff = copy_data.size() - remain_size;
        size_t copy_diff = size_diff;

        // the segment size is bigger than the remaining capacity
        if (size_diff > 0) {
            // Create a container to store the keys
            std::vector<size_t> key_set;

            // Iterate over the map and collect the keys
            for (const auto &pair : reassembler) {
                key_set.push_back(pair.first);
            }
            // Sort the key decreasingly
            std::sort(key_set.begin(), key_set.end(), std::greater<int>());

            // Traverse all keys in the reassembler in decreasing order to remove the overlapping parts and the end of the stream that exceeds the capacity.
            for (auto &key : key_set) {
                size_t copy_key = key;
                // No overlapping
                if (key + reassembler[key].size() < copy_index) {
                    continue;
                }

                if (size_diff > reassembler[key].size()) {
                    size_diff -= reassembler[key].size();
                    reassembler.erase(key);
                    continue;
                }

                bool overflow_flag = false;

                if (key < copy_index + copy_data.size()) {
                    reassembler[key] = reassembler[key].substr(copy_index + copy_data.size() - key, std::string::npos);
                    if (size_diff < (copy_index + copy_data.size() - key)) {
                        overflow_flag = true;
                    }
                    size_diff -= (copy_index + copy_data.size() - key);
                    copy_key += (copy_index + copy_data.size() - key);
                }
                
                if (!overflow_flag) {
                    reassembler[copy_key] = reassembler[key].substr(0, reassembler[key].size() - size_diff);
                }

                if (copy_key != key) {
                    reassembler.erase(key);
                }

                size_diff = 0;
                break;
            }
        }
        
        copy_data = copy_data.substr(0, remain_size + copy_diff - size_diff);

        if (copy_data.empty()) {
            return ;
        }
    }

    auto [it, inserted] = reassembler.emplace(std::make_pair(copy_index, (copy_data)));
    if (!inserted && (it->second.size() < copy_data.size())) {
        it->second = std::move(copy_data);
    }

    // Reassemble the segment in the reassembler to the _output stream
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
            std::string temp_str = element_str.substr(willing - element_index, std::string::npos);
            
            // Update the willing value and write to output
            size_t actual_write = _output.write(temp_str);

            willing += actual_write;
            bytes_writed += actual_write;
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
    // Add stream size
    return _output.buffer_size() + reassembler.size() == 0;
}
