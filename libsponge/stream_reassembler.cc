#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

std::string merge_substrings(const std::string &str1, size_t index1, const std::string &str2, size_t index2) {
    // Determine start and end of each substring in the final combined string
    size_t start = std::min(index1, index2);
    size_t end1 = index1 + str1.size();
    size_t end2 = index2 + str2.size();
    size_t combined_length = std::max(end1, end2) - start;
    
    // Prepare combined string with the correct size
    std::string result(combined_length, '\0');

    // Copy `str1` into `result` at its relative position
    std::copy(str1.begin(), str1.end(), result.begin() + (index1 - start));

    // Copy `str2` into `result` at its relative position
    std::copy(str2.begin(), str2.end(), result.begin() + (index2 - start));

    return result;
}

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

    // Create a container to store the keys
    std::vector<size_t> key_set;

    // Iterate over the map and collect the keys
    for (const auto &pair : reassembler) {
        key_set.push_back(pair.first);
    }
    
    for (auto &key : key_set) {
        size_t first_start = key;
        size_t first_end = key + reassembler[key].size() - 1;

        size_t second_start = copy_index;
        size_t second_end = copy_index + copy_data.size() - 1;

        if (second_end < first_start || first_end < second_start) {
            continue;
        }
        
        copy_data = std::move(merge_substrings(copy_data, copy_index, reassembler[key], key));
        copy_index = std::min(first_start, second_start);

        reassembler.erase(key);
    }
    reassembler[copy_index] = std::move(copy_data); 

    size_t total_reassembler_size = 0;
    for (const auto &pair : reassembler) {
        total_reassembler_size += pair.second.size();
    }

    if ((_output.buffer_size() + total_reassembler_size) > _capacity)  {
        size_t remain_size = (_output.buffer_size() + total_reassembler_size) - _capacity; 
        // Sort in decreasing order
        std::sort(key_set.begin(), key_set.end(), std::greater<int>());

        for (auto &key : key_set) {
            if (reassembler[key].size() <= remain_size) {
                remain_size -= reassembler[key].size();
                reassembler.erase(key);
            }

            reassembler[key] = reassembler[key].substr(0, reassembler[key].size() - remain_size);
            break;
        }
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