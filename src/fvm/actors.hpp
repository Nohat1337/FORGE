#pragma once

#include "runtime.hpp"
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <unordered_map>
#include <memory>
#include <chrono>

namespace forge::concurrent {

using forge::fvm::FValue;

struct Message {
    FValue payload;
    std::string type;
    int64_t timestamp;
    size_t senderId;
    
    Message() : timestamp(0), senderId(0) {}
    Message(const FValue& p, const std::string& t = "msg", size_t s = 0)
        : payload(p), type(t), timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()), senderId(s) {}
};

class Mailbox {
public:
    void send(const Message& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(msg);
        cv_.notify_one();
    }
    
    bool tryReceive(Message& msg, std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (cv_.wait_for(lock, timeout, [this] { return !queue_.empty() || closed_; })) {
            if (!queue_.empty()) {
                msg = queue_.front();
                queue_.pop();
                return true;
            }
        }
        return false;
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        cv_.notify_all();
    }
    
    bool closed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }
    
private:
    std::queue<Message> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool closed_ = false;
};

class Channel {
public:
    using Ptr = std::shared_ptr<Channel>;
    
    Channel(size_t capacity = 0) : capacity_(capacity), closed_(false) {}
    
    void send(const FValue& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (capacity_ > 0) {
            cvSend_.wait(lock, [this] { return buffer_.size() < capacity_ || closed_; });
        }
        if (closed_) throw std::runtime_error("Channel closed");
        buffer_.push(value);
        cvRecv_.notify_one();
    }
    
    bool trySend(const FValue& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_ || (capacity_ > 0 && buffer_.size() >= capacity_)) return false;
        buffer_.push(value);
        cvRecv_.notify_one();
        return true;
    }
    
    bool receive(FValue& value, std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (cvRecv_.wait_for(lock, timeout, [this] { return !buffer_.empty() || closed_; })) {
            if (!buffer_.empty()) {
                value = buffer_.front();
                buffer_.pop();
                cvSend_.notify_one();
                return true;
            }
        }
        return false;
    }
    
    bool tryReceive(FValue& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (buffer_.empty()) return false;
        value = buffer_.front();
        buffer_.pop();
        cvSend_.notify_one();
        return true;
    }
    
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        cvSend_.notify_all();
        cvRecv_.notify_all();
    }
    
    bool closed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffer_.size();
    }
    
    size_t capacity() const { return capacity_; }
    
private:
    std::queue<FValue> buffer_;
    size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable cvSend_;
    std::condition_variable cvRecv_;
    bool closed_;
};

struct Actor {
    using Behavior = std::function<void(Actor*, const Message&)>;
    
    Actor() : id_(nextId_++), mailbox_(std::make_shared<Mailbox>()), running_(false) {}
    
    size_t id() const { return id_; }
    std::shared_ptr<Mailbox> mailbox() const { return mailbox_; }
    bool running() const { return running_.load(); }
    
    void start(Behavior behavior) {
        if (running_.exchange(true)) return;
        behavior_ = std::move(behavior);
        thread_ = std::thread(&Actor::run, this);
    }
    
    void stop() {
        mailbox_->close();
        if (thread_.joinable()) thread_.join();
        running_.store(false);
    }
    
    void send(const Message& msg) {
        mailbox_->send(msg);
    }
    
    void send(const FValue& payload, const std::string& type = "msg") {
        send(Message(payload, type, id_));
    }
    
private:
    void run() {
        Message msg;
        while (mailbox_->tryReceive(msg)) {
            if (behavior_) behavior_(this, msg);
        }
    }
    
    static std::atomic<size_t> nextId_;
    size_t id_;
    std::shared_ptr<Mailbox> mailbox_;
    Behavior behavior_;
    std::thread thread_;
    std::atomic<bool> running_;
};

class ActorSystem {
public:
    static ActorSystem& instance() {
        static ActorSystem sys;
        return sys;
    }
    
    size_t spawn(Actor::Behavior behavior) {
        auto actor = std::make_shared<Actor>();
        actor->start(std::move(behavior));
        std::lock_guard<std::mutex> lock(mutex_);
        size_t id = actor->id();
        actors_[id] = actor;
        return id;
    }
    
    bool send(size_t actorId, const FValue& payload, const std::string& type = "msg") {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = actors_.find(actorId);
        if (it == actors_.end()) return false;
        it->second->send(payload, type);
        return true;
    }
    
    bool sendMsg(size_t actorId, const Message& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = actors_.find(actorId);
        if (it == actors_.end()) return false;
        it->second->send(msg);
        return true;
    }
    
    std::shared_ptr<Actor> getActor(size_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = actors_.find(id);
        return it != actors_.end() ? it->second : nullptr;
    }
    
    void stopActor(size_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = actors_.find(id);
        if (it != actors_.end()) {
            it->second->stop();
            actors_.erase(it);
        }
    }
    
    void stopAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [id, actor] : actors_) {
            actor->stop();
        }
        actors_.clear();
    }
    
    size_t actorCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return actors_.size();
    }
    
    Channel::Ptr createChannel(size_t capacity = 0) {
        return std::make_shared<Channel>(capacity);
    }
    
private:
    ActorSystem() = default;
    std::unordered_map<size_t, std::shared_ptr<Actor>> actors_;
    mutable std::mutex mutex_;
};

} // namespace forge::concurrent