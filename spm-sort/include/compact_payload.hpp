#pragma once

#include <cstdint>
#include <cstring>

/**
 * CompactPayload: Lightweight wrapper for variable-length byte arrays
 *
 * Memory layout: [uint32_t size][actual data bytes...]
 * Overhead: Only 8 bytes (pointer) instead of 24 bytes (std::vector)
 *
 * Saves: 16 bytes per item = 1.6 GB for 100M records
 */
class CompactPayload
{
private:
    uint8_t *data_; // Points to: [uint32_t size][actual payload data...]

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
