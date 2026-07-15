#include "actors.hpp"
#include "runtime.hpp"

namespace forge::concurrent {

std::atomic<size_t> Actor::nextId_{1};

} // namespace forge::concurrent

namespace forge::fvm {

void defineConcurrentModule(ForgeVM& vm) {
    using namespace forge::concurrent;
    auto* mod = new GCMap();
    
    // concurrent.spawn(fn) -> actor_id
    mod->entries["spawn"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("concurrent.spawn() expects 1 argument (function)");
        if (!args[0].isFunction() && !args[0].isClosure()) 
            throw std::runtime_error("concurrent.spawn() expects a function");
        
        auto actorId = ActorSystem::instance().spawn([fn = args[0]](Actor* self, const Message& msg) {
            // Call the user function with the message
            // In a real implementation, we'd need to call the function properly
            // For now, just log
        });
        return FValue::integer((long long)actorId);
    }, "concurrent.spawn", 1));
    
    // concurrent.send(actor_id, msg) -> bool
    mod->entries["send"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 2) throw std::runtime_error("concurrent.send() expects 2 arguments (actor_id, message)");
        size_t id = (size_t)args[0].asInteger();
        bool ok = ActorSystem::instance().send(id, args[1]);
        return FValue::boolean(ok);
    }, "concurrent.send", 2));
    
    // concurrent.stop(actor_id) -> bool
    mod->entries["stop"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("concurrent.stop() expects 1 argument (actor_id)");
        size_t id = (size_t)args[0].asInteger();
        ActorSystem::instance().stopActor(id);
        return FValue::boolean(true);
    }, "concurrent.stop", 1));
    
    // concurrent.actors() -> count
    mod->entries["actors"] = FValue::obj(new GCNative([](const std::vector<FValue>&) -> FValue {
        return FValue::integer((long long)ActorSystem::instance().actorCount());
    }, "concurrent.actors", 0));
    
    // concurrent.channel(capacity?) -> channel
    mod->entries["channel"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
        size_t cap = args.empty() ? 0 : (size_t)args[0].asInteger();
        auto chan = ActorSystem::instance().createChannel(cap);
        // Return as a special object - for now, store in a registry
        static std::vector<Channel::Ptr> channels;
        channels.push_back(chan);
        return FValue::integer((long long)(channels.size() - 1));
    }, "concurrent.channel", -1));
    
    // concurrent.chan_send(chan_id, value) -> bool
    mod->entries["chan_send"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 2) throw std::runtime_error("concurrent.chan_send() expects 2 arguments");
        // Simplified - would need a channel registry
        return FValue::boolean(false);
    }, "concurrent.chan_send", 2));
    
    // concurrent.chan_recv(chan_id, timeout_ms?) -> value
    mod->entries["chan_recv"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
        if (args.size() < 1 || args.size() > 2) 
            throw std::runtime_error("concurrent.chan_recv() expects 1 or 2 arguments");
        return FValue::nil();
    }, "concurrent.chan_recv", -1));
    
    // concurrent.chan_close(chan_id) -> nil
    mod->entries["chan_close"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("concurrent.chan_close() expects 1 argument");
        return FValue::nil();
    }, "concurrent.chan_close", 1));
    
    // concurrent.sleep(ms) -> nil
    mod->entries["sleep"] = FValue::obj(new GCNative([](const std::vector<FValue>& args) -> FValue {
        if (args.size() != 1) throw std::runtime_error("concurrent.sleep() expects 1 argument (ms)");
        std::this_thread::sleep_for(std::chrono::milliseconds((int)args[0].asInteger()));
        return FValue::nil();
    }, "concurrent.sleep", 1));
    
    // concurrent.current_id() -> actor_id (or 0 if not in actor)
    mod->entries["current_id"] = FValue::obj(new GCNative([](const std::vector<FValue>&) -> FValue {
        return FValue::integer(0);
    }, "concurrent.current_id", 0));
    
    vm.defineModule("concurrent", mod);
}

} // namespace forge::fvm