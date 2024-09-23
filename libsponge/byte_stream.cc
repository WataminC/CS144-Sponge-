#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

// template <typename... Targs>
// void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) {
    this->capacity = capacity;
}

size_t ByteStream::write(const string &data) {
    if (end_flag) {
        return 0;
    }

    size_t write_bytes = data.size();
    if (this->remaining_capacity() <= write_bytes)
        write_bytes = this->remaining_capacity();

    string copy_data = data.substr(0, write_bytes);
    Buffer temp_buffer = Buffer(std::move(copy_data));
    stream.append(temp_buffer);

    size += write_bytes;
    bytes_w += write_bytes;

    return write_bytes;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string str = stream.concatenate();
    return str.substr(0, len);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    stream.remove_prefix(len);
    return ;
}

//! Read (i.e., copy and then pop) the next  e"len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    size_t remain = this->buffer_size();
    size_t read_bytes = len;
    if (remain <= len)
        read_bytes = remain;
    string str = this->peek_output(read_bytes);
    this->pop_output(read_bytes);
    bytes_r += read_bytes;

    size -= read_bytes;

    return str;
}

void ByteStream::end_input() {
    end_flag = true;
    return ;
}

bool ByteStream::input_ended() const {
    return end_flag == true;
}

size_t ByteStream::buffer_size() const {
    return size;
}

bool ByteStream::buffer_empty() const {
    return size == 0;
}

bool ByteStream::eof() const {
    return size == 0;
}

size_t ByteStream::bytes_written() const {
    return bytes_w;
}

size_t ByteStream::bytes_read() const {
    return bytes_r;
}

size_t ByteStream::remaining_capacity() const {
    return capacity-size;
}
