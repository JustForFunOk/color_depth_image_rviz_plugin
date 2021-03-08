#pragma once 

#include <vector>
#include <map>
#include <cstdint>  // uint8_t
#include <cstring>  // memset

namespace smart_car
{

// Enhancement for debug mode that incurs performance penalty with STL
// std::clamp to be introduced with c++17
template< typename T>
inline T clamp_val(T val, const T& min, const T& max)
{
    static_assert((std::is_arithmetic<T>::value), "clamping supports arithmetic built-in types only");
#ifdef _DEBUG
    const T t = val < min ? min : val;
    return t > max ? max : t;
#else
    return std::min(std::max(val, min), max);
#endif
}

#pragma pack(push, 1)
struct float3 { float x, y, z; float & operator [] (int i) { return (&x)[i]; } };
#pragma pack(pop)
inline bool operator == (const float3 & a, const float3 & b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
inline float3 operator + (const float3 & a, const float3 & b) { return{ a.x + b.x, a.y + b.y, a.z + b.z }; }
inline float3 operator - (const float3 & a, const float3 & b) { return{ a.x - b.x, a.y - b.y, a.z - b.z }; }
inline float3 operator * (const float3 & a, float b) { return{ a.x*b, a.y*b, a.z*b }; }


class ColorMap
{
public:
    ColorMap(const std::vector<float3>& values, int steps = 4000)
    {
        for(size_t i = 0; i < values.size(); ++i)
        {
            map_[static_cast<float>(i)/(values.size()-1)] = values[i];
        }
        initialize(steps);
    }

    inline float3 get(float value) const
    {
        if (max_ == min_) return *data_;
        auto t = (value - min_) / (max_ - min_);
        t = clamp_val(t, 0.f, 1.f);
        return data_[(int)(t * (size_ - 1))];
    }

    float min_key() const { return min_; }
    float max_key() const { return max_; }

    const std::vector<float3>& get_cache() const { return cache_; }

private:
    inline float3 lerp(const float3& a, const float3& b, float t) const
    {
        return b * t + a * (1 - t);
    }

    float3 calc(float value) const
    {
        if (map_.size() == 0) return{ value, value, value };
        // if we have exactly this value in the map, just return it
        if (map_.find(value) != map_.end()) return map_.at(value);
        // if we are beyond the limits, return the first/last element
        if (value < map_.begin()->first)   return map_.begin()->second;
        if (value > map_.rbegin()->first)  return map_.rbegin()->second;

        auto lower = map_.lower_bound(value) == map_.begin() ? map_.begin() : --(map_.lower_bound(value));
        auto upper = map_.upper_bound(value);

        auto t = (value - lower->first) / (upper->first - lower->first);
        auto c1 = lower->second;
        auto c2 = upper->second;
        return lerp(c1, c2, t);
    }

    void initialize(int steps)
    {
        if(map_.empty()) return;

        min_ = map_.begin()->first;  // 0.0
        max_ = map_.rbegin()->first;  // 1.0

        // divide color map into <steps> level
        cache_.resize(steps+1);
        for(int i = 0; i <= steps; ++i)
        {
            auto t = (float)i / steps;
            auto x = min_ + t*(max_ - min_);
            cache_[i] = calc(x); 
        }

        // Save size and data to avoid STL checks penalties in DEBUG
        size_ = cache_.size();
        data_ = cache_.data();
    }

private:
    std::map<float, float3> map_;  // float key?

    float min_, max_;
    std::vector<float3> cache_;
    size_t size_;
    float3* data_;
};

class Colorizer
{
public:
    Colorizer(/* args */);
    ~Colorizer();

    /**
     * Convert raw depth data to colorful RGB depth data
     * 
     * \param[in&out] pixel_data [in] the depth data to be processed.
     *                           [out] RGB depth data
     * \param[in] pixel_step     the Byte of one pixel. Such as, 16bit(2)
     */
    void process_frame(::std::vector<uint8_t>& pixel_data, uint8_t pixel_step);

public:
    static const int MAX_DEPTH = 65536;

private:
    template <typename T, typename F>
    void process_depth_data(::std::vector<uint8_t>& pixel_data, size_t pixel_cnt, F coloring_func)
    {
        auto raw_depth_data = reinterpret_cast<T*>(pixel_data.data());

        update_histogram(hist_data_, raw_depth_data, pixel_cnt);

        make_rgb_data(raw_depth_data, pixel_cnt, rgb_pixel_data_.data(), coloring_func);

        pixel_data = rgb_pixel_data_;
    }

    template <typename T>
    static void update_histogram(int* hist, const T* depth_data, size_t pixel_cnt)
    {
        memset(hist, 0, MAX_DEPTH * sizeof(int));

        for(size_t i = 0; i < pixel_cnt; ++i)
        {
            T depth_val = depth_data[i];
            int index = static_cast<int>(depth_val);
            hist[index] += 1;
        }

        for(size_t i = 2; i < MAX_DEPTH; ++i)
        {
            hist[i] += hist[i-1];  // Build a cumulative histogram for the indices in [1,0xFFFF]
        }
    }


    template<typename T, typename F>
    void make_rgb_data(T* depth_data, size_t pixel_cnt, uint8_t* rgb_data, F coloring_func)
    {
        auto color_map = color_maps_[maps_index_];  // choose convert style
        for(size_t i = 0; i < pixel_cnt; ++i)
        {
            auto depth = depth_data[i];
            colorize_pixel(depth, rgb_data+3*i, color_map, coloring_func);
        }
    }

    template<typename T, typename F>
    void colorize_pixel(T depth, uint8_t* rgb_pixel, ColorMap* color_map, F coloring_func)
    {
        if(depth)
        {
            auto f = coloring_func(depth);
            auto c = color_map->get(f);
            rgb_pixel[0] = static_cast<uint8_t>(c.x);
            rgb_pixel[1] = static_cast<uint8_t>(c.y);
            rgb_pixel[2] = static_cast<uint8_t>(c.z);
        }
        else
        {
            rgb_pixel[0] = 0;
            rgb_pixel[1] = 0;
            rgb_pixel[2] = 0;
        }
        
    }

private:

    std::vector<int> histogram_;
    int* hist_data_;

    std::vector<ColorMap*> color_maps_;
    uint8_t maps_index_ = 0;

    std::vector<uint8_t> rgb_pixel_data_;
    size_t pixel_cnt_;
};

} // namespace smart_car
