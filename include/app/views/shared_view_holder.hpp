#ifndef AKIRA_SHARED_VIEW_HOLDER_HPP
#define AKIRA_SHARED_VIEW_HOLDER_HPP

#include <memory>
#include <mutex>
#include <unordered_map>

/**
 * Borealis uses raw View pointers
 * https://github.com/xfangfang/borealis/blob/35734ab7e40e6a790582f6d00721bbfbf8543429/library/lib/core/application.cpp#L969C1-L969C82
 *  Activity will tkae a raw View pointer, and the destructor will call delete on the raw View *.
 * The problem for this is that we use plenty of async chiaki threads that borealis knows nothing about.
 * So, in the callback for chiaki, we pass callbacks like onQuit, onRumble, etc. 
 * these are then passed back to borealis to queue work to the borealis thread.
 * However, when exiting, the view may no longer exist, so if we pass the this reference, we either get a NPE or complete garbage.
 * Instead of that, we use shared_ptrs with a no-op delete (so that it wont clean itself up and end up with multiple deletes
 * we let Activity manage the lifecycle of the view, and guard the operations on the weak pointer to make sure that we no-op if it 
 * has already been released.
 * To use this template  do the following:
 *  - inherit from shared_from_this
 *      class MyView : public brls::Box, public std::enable_shared_from_this<MyView>
 *  - create with new, wrap with holdNew,
 *      auto view = SharedViewHolder::holdNew<MyView>(args...);
 *  - dont use this in callbacks, use weak
 *    auto weak = weak_from_this();
 *    host->setOnSomething([weak]() {
 *        if (auto self = weak.lock()) {
 *            self->doSomething();
 *        }
 *    });
 * - destroy it at the end
 *     MyView::~MyView() {
 *        SharedViewHolder::release(this);
 *    }
 */
class SharedViewHolder {
public:
    /**
     * Create a View with new and wrap in shared_ptr with NO-OP deleter.
     * This enables weak_from_this() without shared_ptr taking ownership.
     * Activity will still delete the raw pointer normally.
     */
    template<typename T, typename... Args>
    static std::shared_ptr<T> holdNew(Args&&... args) {
        T* rawPtr = new T(std::forward<Args>(args)...);
        // No-op deleter - Activity handles deletion via raw pointer
        auto ptr = std::shared_ptr<T>(rawPtr, [](T*) { /* no-op */ });
        std::lock_guard<std::mutex> lock(mutex_);
        held_views_[static_cast<void*>(rawPtr)] = ptr;
        return ptr;
    }

    template<typename T>
    static void hold(std::shared_ptr<T> view) {
        std::lock_guard<std::mutex> lock(mutex_);
        held_views_[static_cast<void*>(view.get())] = view;
    }

    /**
     * Release the shared_ptr when View is being destroyed
     * Call this in the View's destructor
     */
    static void release(void* view) {
        std::lock_guard<std::mutex> lock(mutex_);
        held_views_.erase(view);
    }

    static bool isHeld(void* view) {
        std::lock_guard<std::mutex> lock(mutex_);
        return held_views_.find(view) != held_views_.end();
    }

private:
    inline static std::mutex mutex_;
    inline static std::unordered_map<void*, std::shared_ptr<void>> held_views_;
};

#endif // AKIRA_SHARED_VIEW_HOLDER_HPP
