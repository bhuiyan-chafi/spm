/**
 * @author ASM CHAFIULLAH BHUIYAN
 * @brief before I was using stdd::vector<> to store my payload. Vector was nice, clean and
 * convenient but the biggest shock was the overhead. A clean 24 bytes overhead almost bloated
 * my memory, forcing the OS to kill the process. Maybe it was not a common issue for everyone
 * but for me, I didn't know this issue about std::vector<>. Well, we all learn something at
 * some points.
 */
#pragma once

#include <cstdint>
#include <cstring>

class CompactPayload
{
private:
    // Points to: [uint32_t size][actual payload data...]
    uint8_t *data_;

public:
    // Default constructor
    CompactPayload() : data_(nullptr) {}

    // Destructor
    ~CompactPayload()
    {
        if (data_)
            delete[] data_;
    }

    // Copy constructor
    CompactPayload(const CompactPayload &other)
    {
        if (other.data_)
        {
            uint32_t sz = other.size();
            data_ = new uint8_t[4 + sz];
            std::memcpy(data_, other.data_, 4 + sz);
        }
        else
        {
            data_ = nullptr;
        }
    }

    // Move constructor (efficient)
    CompactPayload(CompactPayload &&other) noexcept : data_(other.data_)
    {
        other.data_ = nullptr;
    }

    // Copy assignment
    CompactPayload &operator=(const CompactPayload &other)
    {
        if (this != &other)
        {
            if (data_)
                delete[] data_;
            if (other.data_)
            {
                uint32_t sz = other.size();
                data_ = new uint8_t[4 + sz];
                std::memcpy(data_, other.data_, 4 + sz);
            }
            else
            {
                data_ = nullptr;
            }
        }
        return *this;
    }

    // Move assignment (efficient)
    CompactPayload &operator=(CompactPayload &&other) noexcept
    {
        if (this != &other)
        {
            if (data_)
                delete[] data_;
            data_ = other.data_;
            other.data_ = nullptr;
        }
        return *this;
    }

    // Resize and allocate memory
    void resize(size_t new_size)
    {
        if (data_)
            delete[] data_;
        data_ = new uint8_t[4 + new_size];
        *reinterpret_cast<uint32_t *>(data_) = static_cast<uint32_t>(new_size);
    }

    // Get the payload size
    uint32_t size() const
    {
        return data_ ? *reinterpret_cast<const uint32_t *>(data_) : 0;
    }

    // Get mutable pointer to payload data
    uint8_t *data()
    {
        return data_ ? data_ + 4 : nullptr;
    }

    // Get const pointer to payload data
    const uint8_t *data() const
    {
        return data_ ? data_ + 4 : nullptr;
    }

    // Array access operator
    uint8_t &operator[](size_t idx)
    {
        return data()[idx];
    }

    // Const array access operator
    const uint8_t &operator[](size_t idx) const
    {
        return data()[idx];
    }

    // Check if empty
    bool empty() const
    {
        return data_ == nullptr || size() == 0;
    }
};
