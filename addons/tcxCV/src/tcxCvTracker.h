#pragma once

// ======================================================================
// tcxCvTracker.h - Generic object tracker
// ======================================================================
//
// Tracks identities of objects over time across frames.
// Supports cv::Rect and cv::Point2f tracking out of the box.
//
// Key settings:
//   setPersistence(n)    - frames before forgetting unseen objects (default 15)
//   setMaximumDistance(d) - max movement distance for matching (default 64)
//
// Usage:
//   RectTracker tracker;
//   tracker.track(boundingRects);
//   unsigned int label = tracker.getLabelFromIndex(i);
//

#include <opencv2/opencv.hpp>
#include <utility>
#include <map>
#include <vector>
#include <algorithm>

namespace tcx {

// Distance functions for different tracked types
inline float trackingDistance(const cv::Rect& a, const cv::Rect& b) {
    float dx = (a.x + a.width / 2.0f) - (b.x + b.width / 2.0f);
    float dy = (a.y + a.height / 2.0f) - (b.y + b.height / 2.0f);
    float dw = (a.width - b.width) * 0.5f;
    float dh = (a.height - b.height) * 0.5f;
    return sqrtf(dx * dx + dy * dy + dw * dw + dh * dh);
}

inline float trackingDistance(const cv::Point2f& a, const cv::Point2f& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

// ======================================================================
// TrackedObject
// ======================================================================

template <class T>
class TrackedObject {
protected:
    unsigned int lastSeen, label_, age_;
    int index_;
public:
    T object;

    TrackedObject(const T& object, unsigned int label__, int index)
        : lastSeen(0), label_(label__), age_(0), index_(index), object(object) {}

    TrackedObject(const T& object, const TrackedObject<T>& previous, int index)
        : lastSeen(0), label_(previous.label_), age_(previous.age_),
          index_(index), object(object) {}

    TrackedObject(const TrackedObject<T>& old)
        : lastSeen(old.lastSeen), label_(old.label_), age_(old.age_),
          index_(old.index_), object(old.object) {}

    void timeStep(bool visible) {
        age_++;
        if (!visible) {
            lastSeen++;
        }
    }

    unsigned int getLastSeen() const { return lastSeen; }
    unsigned long getAge() const { return age_; }
    unsigned int getLabel() const { return label_; }
    int getIndex() const { return index_; }
};

// ======================================================================
// Sort helper
// ======================================================================

struct bySecond {
    template <class First, class Second>
    bool operator()(std::pair<First, Second> const& a,
                    std::pair<First, Second> const& b) {
        return a.second < b.second;
    }
};

// ======================================================================
// Tracker
// ======================================================================

template <class T>
class Tracker {
protected:
    std::vector<TrackedObject<T>> previous, current;
    std::vector<unsigned int> currentLabels, previousLabels, newLabels, deadLabels;
    std::map<unsigned int, TrackedObject<T>*> previousLabelMap, currentLabelMap;

    unsigned int persistence;
    unsigned long long curLabel;
    float maximumDistance;

    unsigned long long getNewLabel() {
        curLabel++;
        return curLabel;
    }

public:
    Tracker<T>()
        : persistence(15), curLabel(0), maximumDistance(64) {}
    virtual ~Tracker() {}

    void setPersistence(unsigned int persistence_) {
        this->persistence = persistence_;
    }

    void setMaximumDistance(float maximumDistance_) {
        this->maximumDistance = maximumDistance_;
    }

    virtual const std::vector<unsigned int>& track(const std::vector<T>& objects) {
        previous = current;
        int n = (int)objects.size();
        int m = (int)previous.size();

        // Build NxM distance matrix
        typedef std::pair<int, int> MatchPair;
        typedef std::pair<MatchPair, float> MatchDistancePair;
        std::vector<MatchDistancePair> all;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < m; j++) {
                float curDistance = trackingDistance(objects[i], previous[j].object);
                if (curDistance < maximumDistance) {
                    all.push_back(MatchDistancePair(MatchPair(i, j), curDistance));
                }
            }
        }

        // Sort matches by distance
        std::sort(all.begin(), all.end(), bySecond());

        previousLabels = currentLabels;
        currentLabels.clear();
        currentLabels.resize(n);
        current.clear();
        std::vector<bool> matchedObjects(n, false);
        std::vector<bool> matchedPrevious(m, false);

        // Walk through matches in order
        for (int k = 0; k < (int)all.size(); k++) {
            MatchPair& match = all[k].first;
            int i = match.first;
            int j = match.second;
            if (!matchedObjects[i] && !matchedPrevious[j]) {
                matchedObjects[i] = true;
                matchedPrevious[j] = true;
                int index = (int)current.size();
                current.push_back(TrackedObject<T>(objects[i], previous[j], index));
                current.back().timeStep(true);
                currentLabels[i] = current.back().getLabel();
            }
        }

        // Create new labels for unmatched objects
        newLabels.clear();
        for (int i = 0; i < n; i++) {
            if (!matchedObjects[i]) {
                int curLabel_ = (int)getNewLabel();
                int index = (int)current.size();
                current.push_back(TrackedObject<T>(objects[i], curLabel_, index));
                current.back().timeStep(true);
                currentLabels[i] = curLabel_;
                newLabels.push_back(curLabel_);
            }
        }

        // Copy old unmatched objects if young enough
        deadLabels.clear();
        for (int j = 0; j < m; j++) {
            if (!matchedPrevious[j]) {
                if (previous[j].getLastSeen() < persistence) {
                    current.push_back(previous[j]);
                    current.back().timeStep(false);
                }
                deadLabels.push_back(previous[j].getLabel());
            }
        }

        // Build label maps
        currentLabelMap.clear();
        for (size_t i = 0; i < current.size(); i++) {
            unsigned int label_ = current[i].getLabel();
            currentLabelMap[label_] = &(current[i]);
        }
        previousLabelMap.clear();
        for (size_t i = 0; i < previous.size(); i++) {
            unsigned int label_ = previous[i].getLabel();
            previousLabelMap[label_] = &(previous[i]);
        }

        return currentLabels;
    }

    const std::vector<unsigned int>& getCurrentLabels() const { return currentLabels; }
    const std::vector<unsigned int>& getPreviousLabels() const { return previousLabels; }
    const std::vector<unsigned int>& getNewLabels() const { return newLabels; }
    const std::vector<unsigned int>& getDeadLabels() const { return deadLabels; }

    unsigned int getLabelFromIndex(unsigned int i) const {
        return currentLabels[i];
    }

    int getIndexFromLabel(unsigned int label_) const {
        auto it = currentLabelMap.find(label_);
        return (it != currentLabelMap.end()) ? it->second->getIndex() : -1;
    }

    const T& getPrevious(unsigned int label_) const {
        return previousLabelMap.find(label_)->second->object;
    }

    const T& getCurrent(unsigned int label_) const {
        return currentLabelMap.find(label_)->second->object;
    }

    bool existsCurrent(unsigned int label_) const {
        return currentLabelMap.count(label_) > 0;
    }

    bool existsPrevious(unsigned int label_) const {
        return previousLabelMap.count(label_) > 0;
    }

    int getAge(unsigned int label_) const {
        auto it = currentLabelMap.find(label_);
        return (it != currentLabelMap.end()) ? (int)it->second->getAge() : 0;
    }

    int getLastSeen(unsigned int label_) const {
        auto it = currentLabelMap.find(label_);
        return (it != currentLabelMap.end()) ? (int)it->second->getLastSeen() : 0;
    }
};

// ======================================================================
// RectTracker - smoothed rectangular tracker
// ======================================================================

class RectTracker : public Tracker<cv::Rect> {
protected:
    float smoothingRate;
    std::map<unsigned int, cv::Rect> smoothed;

public:
    RectTracker()
        : smoothingRate(0.5f) {}

    void setSmoothingRate(float smoothingRate_) {
        this->smoothingRate = smoothingRate_;
    }

    float getSmoothingRate() const {
        return smoothingRate;
    }

    const std::vector<unsigned int>& track(const std::vector<cv::Rect>& objects) {
        const std::vector<unsigned int>& labels = Tracker<cv::Rect>::track(objects);
        // Add new, update existing
        for (size_t i = 0; i < labels.size(); i++) {
            unsigned int label_ = labels[i];
            const cv::Rect& cur = getCurrent(label_);
            if (smoothed.count(label_) > 0) {
                cv::Rect& smooth = smoothed[label_];
                smooth.x = static_cast<int>(smooth.x + (cur.x - smooth.x) * smoothingRate);
                smooth.y = static_cast<int>(smooth.y + (cur.y - smooth.y) * smoothingRate);
                smooth.width = static_cast<int>(smooth.width + (cur.width - smooth.width) * smoothingRate);
                smooth.height = static_cast<int>(smooth.height + (cur.height - smooth.height) * smoothingRate);
            } else {
                smoothed[label_] = cur;
            }
        }
        // Remove dead
        auto it = smoothed.begin();
        while (it != smoothed.end()) {
            if (!existsCurrent(it->first)) {
                it = smoothed.erase(it);
            } else {
                ++it;
            }
        }
        return labels;
    }

    const cv::Rect& getSmoothed(unsigned int label_) const {
        return smoothed.find(label_)->second;
    }

    cv::Vec2f getVelocity(unsigned int i) const {
        unsigned int label_ = getLabelFromIndex(i);
        if (existsPrevious(label_)) {
            const cv::Rect& prev = getPrevious(label_);
            const cv::Rect& cur = getCurrent(label_);
            return cv::Vec2f(
                (cur.x + cur.width / 2.0f) - (prev.x + prev.width / 2.0f),
                (cur.y + cur.height / 2.0f) - (prev.y + prev.height / 2.0f)
            );
        }
        return cv::Vec2f(0, 0);
    }
};

typedef Tracker<cv::Point2f> PointTracker;

// ======================================================================
// Follower / TrackerFollower - maintain paired objects
// ======================================================================

template <class T>
class Follower {
protected:
    bool dead;
    unsigned int label_;

public:
    Follower()
        : dead(false), label_(0) {}

    virtual ~Follower() {}
    virtual void setup(const T& /*track*/) {}
    virtual void update(const T& /*track*/) {}
    virtual void kill() { dead = true; }

    void setLabel(unsigned int label__) { this->label_ = label__; }
    unsigned int getLabel() const { return label_; }
    bool getDead() const { return dead; }
};

typedef Follower<cv::Rect> RectFollower;
typedef Follower<cv::Point2f> PointFollower;

template <class T, class F>
class TrackerFollower : public Tracker<T> {
protected:
    std::vector<unsigned int> labels;
    std::vector<F> followers;

public:
    const std::vector<unsigned int>& track(const std::vector<T>& objects) {
        Tracker<T>::track(objects);
        // Kill missing, update existing
        for (size_t i = 0; i < labels.size(); i++) {
            unsigned int curLabel_ = labels[i];
            F& curFollower = followers[i];
            if (!Tracker<T>::existsCurrent(curLabel_)) {
                curFollower.kill();
            } else {
                curFollower.update(Tracker<T>::getCurrent(curLabel_));
            }
        }
        // Add new
        for (size_t i = 0; i < Tracker<T>::newLabels.size(); i++) {
            unsigned int curLabel_ = Tracker<T>::newLabels[i];
            labels.push_back(curLabel_);
            followers.push_back(F());
            followers.back().setup(Tracker<T>::getCurrent(curLabel_));
            followers.back().setLabel(curLabel_);
        }
        // Remove dead
        for (int i = (int)labels.size() - 1; i >= 0; i--) {
            if (followers[i].getDead()) {
                followers.erase(followers.begin() + i);
                labels.erase(labels.begin() + i);
            }
        }
        return labels;
    }

    std::vector<F>& getFollowers() {
        return followers;
    }
};

template <class F> using RectTrackerFollower = TrackerFollower<cv::Rect, F>;
template <class F> using PointTrackerFollower = TrackerFollower<cv::Point2f, F>;

} // namespace tcx
