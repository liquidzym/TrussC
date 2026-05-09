#pragma once
// Simple ring buffer (float, single-writer single-reader safe with barriers)
#include <vector>
#include <atomic>
namespace tcx::pdsp {
class RingBuffer {
public:
    // Call resize() before audio starts (allocates memory)
    void resize(int n){buf_.resize(n);w_.store(0,std::memory_order_relaxed);r_.store(0,std::memory_order_relaxed);}
    bool write(float v){int w=w_.load(std::memory_order_relaxed),nxt=(w+1)%(int)buf_.size();if(nxt==r_.load(std::memory_order_acquire))return false;buf_[w]=v;w_.store(nxt,std::memory_order_release);return true;}
    bool read(float&v){int r=r_.load(std::memory_order_relaxed);if(r==w_.load(std::memory_order_acquire))return false;v=buf_[r];r_.store((r+1)%(int)buf_.size(),std::memory_order_release);return true;}
private:std::vector<float> buf_;std::atomic<int> w_,r_;
};
}
