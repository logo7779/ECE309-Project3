// src/tools/tool_registry.cpp — hand-written separate-chaining hash table.
#include "tools/tool_registry.h"

#include <utility>

namespace {
constexpr std::size_t kInitialBuckets = 16;
constexpr std::size_t kMaxLoadNumerator = 3;
constexpr std::size_t kMaxLoadDenominator = 4; // resize before load exceeds 0.75
}

ToolRegistry::ToolRegistry() : buckets_(kInitialBuckets) {} //constrcutor initializes to know size
ToolRegistry::~ToolRegistry() = default;    //normal destructor invokes

std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    // 64-bit FNV-1a: every byte participates and byte order matters.
    std::size_t value = static_cast<std::size_t>(14695981039346656037ULL);
    for (unsigned char ch : key) {
        value ^= static_cast<std::size_t>(ch);
        value *= static_cast<std::size_t>(1099511628211ULL);
    }
    return value;   //return number for modulo assignment to a bucket
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (!tool) return false; //null ownership check

    const std::string key = tool->name(); //save the name
    const std::size_t old_index = hash(key) % buckets_.size(); //find its current bucket saved to
    for (const Entry& entry : buckets_[old_index]) {
        if (entry.name == key) return false; //compare names to not duplicate tools
    }

    // Check before inserting so the public load factor never exceeds 1.0.
    if ((size_ + 1) * kMaxLoadDenominator > buckets_.size() * kMaxLoadNumerator) {
        rehash(buckets_.size() * 2); //increase by factor of 2 when needed
    }

    const std::size_t index = hash(key) % buckets_.size(); //recheck indexing because tool may have moved to different bucket after resizing
    buckets_[index].push_back(Entry{key, std::move(tool)});// put tool into table
    ++size_; //increase tool count
    return true;
}

Tool* ToolRegistry::find(std::string_view name) const {
    if (buckets_.empty()) return nullptr; //is the bucket empty? prevent mem leak
    const std::size_t index = hash(name) % buckets_.size(); //get has of requested name
    for (const Entry& entry : buckets_[index]) { //search for the hash number
        if (entry.name == name) return entry.tool.get(); //return pointer of found tool
    }
    return nullptr; //otherwise tool not in the table
}

std::size_t ToolRegistry::size() const noexcept { return size_; } // how many tools are in our table?

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const auto& bucket : buckets_) { //look in every bucket
        for (const Entry& entry : bucket) fn(*entry.tool); //lok at every tool in bucket
    }
}

std::size_t ToolRegistry::bucket_count() const noexcept { return buckets_.size(); }

double ToolRegistry::load_factor() const noexcept { //check sizing vs. items held
    return buckets_.empty() ? 0.0 : static_cast<double>(size_) / buckets_.size();
} //this function sees if we need to resize or not

std::size_t ToolRegistry::max_probe_length() const {
    std::size_t longest = 0;
    for (const auto& bucket : buckets_) { //look through all buckets
        if (bucket.size() > longest) longest = bucket.size(); //find deepest bucket or bucket with most tools
    }
    return longest;
}

void ToolRegistry::rehash(std::size_t new_bucket_count) { //resize the table
    std::vector<std::vector<Entry>> new_buckets(new_bucket_count);
    for (auto& bucket : buckets_) { // look at all existing buckeets
        for (auto& entry : bucket) { //look at every entry in buckets
            const std::size_t index = hash(entry.name) % new_bucket_count; //rehash and find new bucket home
            // for each tool, this could change when resizing because of % remainder difference even though the hash number doesnt change
            new_buckets[index].push_back(std::move(entry)); //place tool into its (new) bucket
        }
    }
    buckets_ = std::move(new_buckets); //preserve pointers while moving into new table
}
